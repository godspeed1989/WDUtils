//********************************************************************
//	created:	26:7:2008   1:59
//	file:		pci.romimage.cpp
//	author:		tiamo
//	purpose:	rom image
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciReadRomImage)
#pragma alloc_text("PAGE",PciRomTestWriteAccessToBuffer)
#pragma alloc_text("PAGE",PciTransferRomData)

//
// read rom image [checked]
//
NTSTATUS PciReadRomImage(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG WhichSpace,__in PVOID Buffer,__in ULONG Offset,__inout PULONG Length)
{
	PAGED_CODE();

	ASSERT(Offset == 0);
	ASSERT(WhichSpace == PCI_WHICHSPACE_ROM);

	PciDebugPrintf(0x40000,"PCI ROM entered for pdox %08x (buffer @ %08x %08x bytes)\n",PdoExt,Buffer,*Length);

	NTSTATUS Status										= STATUS_SUCCESS;
	BOOLEAN NeedProgramHardware							= FALSE;
	ULONG OldRomAddress									= 0;
	ULONG NewRomAddress									= 0;
	ULONG OldCommandStatus								= 0;
	ULONG NewCommandStatus								= 0;
	PVOID MappedRomAddress								= 0;
	ULONG ChangedBarIndex								= -1;
	ULONG NewBar										= 0;
	PPCI_ARBITER_INSTANCE ArbiterInstance				= 0;
	CM_PARTIAL_RESOURCE_DESCRIPTOR SaveDesc;

	__try
	{
		//
		// only support type0's rom
		//
		if(PdoExt->HeaderType != PCI_DEVICE_TYPE)
			try_leave(Status = STATUS_INVALID_DEVICE_REQUEST);

		//
		// rom resource's index == PCI_TYPE0_ADDRESSES
		// has a rom resource?
		//
		if(!PdoExt->Resources || PdoExt->Resources->Limit[PCI_TYPE0_ADDRESSES].Type == CmResourceTypeNull)
			try_leave(Status = STATUS_SUCCESS);

		PIO_RESOURCE_DESCRIPTOR Requirement				= PdoExt->Resources->Limit + PCI_TYPE0_ADDRESSES;
		ASSERT(!FlagOn(Requirement->u.Memory.Length,0x1ff));
	
		//
		// buffer to small?
		//
		if(*Length == 0)
			try_leave(Status = STATUS_BUFFER_TOO_SMALL;*Length = Requirement->u.Memory.Length);

		ULONG ReadLength								= *Length > Requirement->u.Memory.Length ? Requirement->u.Memory.Length : *Length;
		*Length											= 0;

		//
		// probe for write
		//
		Status											= PciRomTestWriteAccessToBuffer(Buffer,ReadLength);
		ASSERT(NT_SUCCESS(Status));

		ASSERT(Requirement->Type == CmResourceTypeMemory);
		ASSERT(Requirement->Flags == CM_RESOURCE_MEMORY_READ_ONLY);

		//
		// read command status
		//
		PciReadDeviceConfig(PdoExt,&OldCommandStatus,FIELD_OFFSET(PCI_COMMON_CONFIG,Command),sizeof(OldCommandStatus));
		OldCommandStatus								&= 0x0000ffff;
		NewCommandStatus								= OldCommandStatus;

		//
		// read current rom address
		//
		PciReadDeviceConfig(PdoExt,&OldRomAddress,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.ROMBaseAddress),sizeof(OldRomAddress));
		NewRomAddress									= OldRomAddress;

		//
		// if current->type != null,it means that the rom address bar has been enabled
		//
		PHYSICAL_ADDRESS BusAddress						= {0};
		NeedProgramHardware								= TRUE;
		PCM_PARTIAL_RESOURCE_DESCRIPTOR	 PartialDesc	= PdoExt->Resources->Current + PCI_TYPE0_ADDRESSES;
		if(PartialDesc->Type == CmResourceTypeMemory)
		{
			ASSERT(FlagOn(OldRomAddress,PCI_ROMADDRESS_ENABLED));

			if(FlagOn(OldCommandStatus,PCI_ENABLE_MEMORY_SPACE))
			{
				//
				// memory decode is enabled,no need to program hardware
				//
				NeedProgramHardware						= FALSE;
				BusAddress.LowPart						= FlagOn(NewRomAddress,PCI_ADDRESS_ROM_ADDRESS_MASK);
			}
		}
		else
		{
			ASSERT(PartialDesc->Type == CmResourceTypeNull);
		}

		//
		// we should find a unused memory region,and map rom to it
		//
		if(NeedProgramHardware)
		{
			//
			// try to find a memory arbiter
			//
			PPCI_PDO_EXTENSION CurrentPdoExt			= PdoExt;
			PPCI_PDO_EXTENSION ParentPdoExt				= 0;

			while(1)
			{
				PPCI_FDO_EXTENSION ParentFdoExt			= CurrentPdoExt->ParentFdoExtension;

				if(ParentFdoExt != ParentFdoExt->BusRootFdoExtension)
					ParentPdoExt						= static_cast<PPCI_PDO_EXTENSION>(ParentFdoExt->PhysicalDeviceObject->DeviceExtension);
				else
					ParentPdoExt						= 0;

				PPCI_SECONDARY_EXTENSION SecondaryExt	= PciFindNextSecondaryExtension(ParentFdoExt->SecondaryExtension.Next,PciArb_Memory);
				if(!SecondaryExt)
				{
					if(!ParentPdoExt)
						try_leave(Status = STATUS_UNSUCCESSFUL;ASSERT(SecondaryExt));

					if(!ParentPdoExt->Dependent.type1.SubtractiveDecode)
						try_leave(Status = STATUS_UNSUCCESSFUL;ASSERT(ParentPdoExt->Dependent.type1.SubtractiveDecode));

					CurrentPdoExt						= ParentPdoExt;
				}
				else
				{
					ArbiterInstance						= CONTAINING_RECORD(SecondaryExt,PCI_ARBITER_INSTANCE,SecondaryExtension);
					break;
				}
			}

			ASSERT(ArbiterInstance);
			ArbAcquireArbiterLock(&ArbiterInstance->CommonInstance);

			ULONG Alignment								= Requirement->u.Memory.Alignment;
			ULONG RangeLength							= Requirement->u.Memory.Length;
			ULONGLONG Min								= Requirement->u.Memory.MinimumAddress.QuadPart;
			ULONGLONG Max								= Requirement->u.Memory.MaximumAddress.QuadPart;

			if(ParentPdoExt && ParentPdoExt->HeaderType == PCI_BRIDGE_TYPE)
			{
				if(ParentPdoExt->Resources->Current[3].Type == CmResourceTypeNull)
					try_leave(Status = STATUS_UNSUCCESSFUL;PciDebugPrintf(0x40000,"PCI ROM pdo %p parent %p has no memory aperture.\n",PdoExt,ParentPdoExt));

				ASSERT(ParentPdoExt->Resources->Current[3].Type == CmResourceTypeMemory);

				Min										= ParentPdoExt->Resources->Current[3].u.Memory.Start.QuadPart;
				Max										= Min + ParentPdoExt->Resources->Current[3].u.Memory.Length - 1;
			}

			ULONGLONG Start;
			Status										= RtlFindRange(ArbiterInstance->CommonInstance.Allocation,Min,Max,RangeLength,Alignment,0,0,0,0,&Start);
			if(!NT_SUCCESS(Status))
			{
				if(ParentPdoExt && ParentPdoExt->HeaderType == PCI_CARDBUS_BRIDGE_TYPE)
					try_leave(Status = STATUS_UNSUCCESSFUL);

				for(ULONG i = 0; i < PCI_TYPE0_ADDRESSES; i ++)
				{
					PIO_RESOURCE_DESCRIPTOR Cur			= PdoExt->Resources->Limit + i;
					if(Cur->Type == CmResourceTypeMemory && Cur->u.Memory.Length >= RangeLength && !PdoExt->Resources->Current[i].u.Memory.Start.HighPart)
					{
						if(ChangedBarIndex == -1 || PdoExt->Resources->Limit[ChangedBarIndex].u.Memory.Length > Cur->u.Memory.Length)
							ChangedBarIndex				= i;
					}
				}

				if(ChangedBarIndex == -1)
					try_leave(Status = STATUS_UNSUCCESSFUL;PciDebugPrintf(0x40000,"PCI ROM pdo %p could not get MEM resource len 0x%x.\n",PdoExt,RangeLength));

				Max										= 0xffffffff;
				Min										= 0;
				ULONG Flags								= RTL_RANGE_LIST_NULL_CONFLICT_OK;
				ULONGLONG NewBarAddr					= 0;
				RangeLength								= PdoExt->Resources->Limit[ChangedBarIndex].u.Memory.Length;
				Alignment								= PdoExt->Resources->Limit[ChangedBarIndex].u.Memory.Alignment;
				Status									= RtlFindRange(ArbiterInstance->CommonInstance.Allocation,Min,Max,RangeLength,Alignment,Flags,0,0,0,&NewBarAddr);
				if(!NT_SUCCESS(Status))
					try_leave(Status = STATUS_UNSUCCESSFUL;PciDebugPrintf(0x40000,"PCI ROM could find range to disable %x memory window.\n",RangeLength));

				SaveDesc								= PdoExt->Resources->Current[ChangedBarIndex];
				Start									= PdoExt->Resources->Current[ChangedBarIndex].u.Memory.Start.QuadPart;
				NewBar									= static_cast<ULONG>(NewBarAddr);
				PciDebugPrintf(0x40000,"PCI ROM Moving existing memory resource from %p to %p\n",SaveDesc.u.Memory.Start.LowPart,NewBar);
			}

			BusAddress.LowPart							= static_cast<ULONG>(Start);
		}

		ULONG AddressSpace								= 0;
		PHYSICAL_ADDRESS TranslatedAddress;

		//
		// translate the address
		//
		ASSERT(PcipSavedTranslateBusAddress);

		if(!PcipSavedTranslateBusAddress(PCIBus,PdoExt->ParentFdoExtension->BaseBus,BusAddress,&AddressSpace,&TranslatedAddress))
		{
			if(!PcipSavedTranslateBusAddress(PCIBus,PdoExt->ParentFdoExtension->BusRootFdoExtension->BaseBus,BusAddress,&AddressSpace,&TranslatedAddress))
			{
				PciDebugPrintf(0x40000,"PCI ROM range at %p FAILED to translate\n");
				try_leave(Status = STATUS_UNSUCCESSFUL;ASSERT(FALSE));
			}
		}

		PciDebugPrintf(0x40000,"PCI ROM range at %p translated to %p\n",BusAddress.LowPart,TranslatedAddress.LowPart);

		PUCHAR BaseAddress								= 0;

		if(!AddressSpace)
		{
			//
			// memory space,we need map it
			//
			BaseAddress									= static_cast<PUCHAR>(MmMapIoSpace(TranslatedAddress,Requirement->u.Memory.Length,MmNonCached));
			MappedRomAddress							= BaseAddress;

			PciDebugPrintf(0x40000,"PCI ROM mapped b %08x t %08x to %p length %x bytes\n",BusAddress,TranslatedAddress.LowPart,BaseAddress,Requirement->u.Memory.Length);
		}
		else
		{
			//
			// io space
			//
			BaseAddress									= reinterpret_cast<PUCHAR>(TranslatedAddress.LowPart);

			PciDebugPrintf(0x40000,"PCI ROM b %08x t %08x IO length %x bytes\n",BusAddress,TranslatedAddress.LowPart,Requirement->u.Memory.Length);
		}

		if(!BaseAddress)
			try_leave(Status = STATUS_UNSUCCESSFUL;ASSERT(BaseAddress));

		//
		// program hardware
		//
		if(NeedProgramHardware)
		{
			ClearFlag(NewCommandStatus,PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER);
			PciWriteDeviceConfig(PdoExt,&NewCommandStatus,FIELD_OFFSET(PCI_COMMON_CONFIG,Command),sizeof(NewCommandStatus));

			if(ChangedBarIndex != -1)
				PciWriteDeviceConfig(PdoExt,&NewBar,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.BaseAddresses[ChangedBarIndex]),sizeof(NewBar));

			NewRomAddress								= BusAddress.LowPart;
			SetFlag(NewRomAddress,PCI_ROMADDRESS_ENABLED);
			PciWriteDeviceConfig(PdoExt,&NewRomAddress,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.ROMBaseAddress),sizeof(NewRomAddress));

			SetFlag(NewCommandStatus,PCI_ENABLE_MEMORY_SPACE);
			PciWriteDeviceConfig(PdoExt,&NewCommandStatus,FIELD_OFFSET(PCI_COMMON_CONFIG,Command),sizeof(NewCommandStatus));
		}

		//
		// transfer rom
		//
		for(ULONG Offset = 0; ReadLength; )
		{
			//
			// read rom header and check it
			//
			PCI_ROM_HEADER LocalHeader;
			PciTransferRomData(BaseAddress,&LocalHeader,sizeof(LocalHeader));
			if(LocalHeader.Signature != 0xaa55)
				try_leave(PciDebugPrintf(0x40000,"PCI ROM invalid signature, offset %x, expected %04x, got %04x\n",Offset,0xaa55,LocalHeader.Signature));

			//
			// read pci data structure
			//
			PCI_ROM_DATA_HEADER LocalDataHeader;
			PciTransferRomData(BaseAddress + LocalHeader.PciDataPtr,&LocalDataHeader,sizeof(LocalDataHeader));
			if(LocalDataHeader.Signature != 'RICP')
				try_leave(PciDebugPrintf(0x40000,"PCI ROM invalid signature, offset %x, expected %08x, got %08x\n",Offset,'RICP',LocalDataHeader.Signature));

			ULONG ThisLength							= static_cast<ULONG>(LocalDataHeader.ImageLength) << 9;
			if(ThisLength > ReadLength)
				ThisLength								= ReadLength;

			//
			// read it
			//
			PciTransferRomData(BaseAddress,Buffer,ThisLength);

			BaseAddress									+= ThisLength;
			Buffer										= Add2Ptr(Buffer,ThisLength,PVOID);
			ReadLength									-= ThisLength;
			Offset										+= ThisLength;
			*Length										+= ThisLength;

			if(FlagOn(LocalDataHeader.Indicator,0x80))
				break;
		}
	}
	__finally
	{
		if(NeedProgramHardware)
		{
			if(OldRomAddress != NewRomAddress)
			{
				ClearFlag(NewCommandStatus,PCI_ENABLE_MEMORY_SPACE);
				PciWriteDeviceConfig(PdoExt,&NewCommandStatus,FIELD_OFFSET(PCI_COMMON_CONFIG,Command),sizeof(NewCommandStatus));

				PciWriteDeviceConfig(PdoExt,&OldRomAddress,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.ROMBaseAddress),sizeof(OldRomAddress));
			}

			if(ChangedBarIndex != -1)
			{
				ULONG OldValue							= PdoExt->Resources->Current[ChangedBarIndex].u.Generic.Start.LowPart;
				ULONG Offset							= FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.BaseAddresses[ChangedBarIndex]);
				ULONG Length							= sizeof(ULONG);
				PciWriteDeviceConfig(PdoExt,&OldValue,Offset,Length);
			}

			if(OldCommandStatus != NewCommandStatus)
				PciWriteDeviceConfig(PdoExt,&OldCommandStatus,FIELD_OFFSET(PCI_COMMON_CONFIG,Command),sizeof(OldCommandStatus));

			if(ArbiterInstance)
				ArbReleaseArbiterLock(&ArbiterInstance->CommonInstance);
		}

		if(MappedRomAddress)
			MmUnmapIoSpace(MappedRomAddress,PdoExt->Resources->Limit[PCI_TYPE0_ADDRESSES].u.Generic.Length);
	}

	PciDebugPrintf(0x40000,"PCI ROM leaving pdox %08x (buffer @ %08x %08x bytes, status %08x)\n",PdoExt,Buffer,*Length,Status);

	return Status;
}

//
// test buffer [checked]
//
NTSTATUS PciRomTestWriteAccessToBuffer(__in PVOID Buffer,__in ULONG Length)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		for(ULONG i = 0; i < Length; i += PAGE_SIZE)
			*Add2Ptr(Buffer,i,PUCHAR)					= 0;

		*Add2Ptr(Buffer,Length - 1,PUCHAR)				= 0;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		Status											= GetExceptionCode();
	}

	return Status;
}

//
// transfer rom data [checked]
//
VOID PciTransferRomData(__in PUCHAR Register,__in PVOID Buffer,__in ULONG Length)
{
	PAGED_CODE();

	if(Length > sizeof(ULONG) && (reinterpret_cast<ULONG>(Buffer) & (sizeof(ULONG) - 1)) == (reinterpret_cast<ULONG>(Register) & (sizeof(ULONG) - 1)))
	{
		if(reinterpret_cast<ULONG>(Register) & (sizeof(ULONG) - 1))
		{
			ULONG ReadLength							= sizeof(ULONG) - (reinterpret_cast<ULONG>(Register) & 3);
			READ_REGISTER_BUFFER_UCHAR(Register,static_cast<PUCHAR>(Buffer),ReadLength);
			Register									+= ReadLength;
			Buffer										= Add2Ptr(Buffer,ReadLength,PVOID);
			Length										-= ReadLength;
		}

		if(Length > sizeof(ULONG))
		{
			ULONG ReadLength							= Length / sizeof(ULONG);
			READ_REGISTER_BUFFER_ULONG(reinterpret_cast<PULONG>(Register),static_cast<PULONG>(Buffer),ReadLength);
			ReadLength									*= sizeof(ULONG);
			Register									+= ReadLength;
			Buffer										= Add2Ptr(Buffer,ReadLength,PVOID);
			Length										-= ReadLength;
		}
	}

	if(Length)
		READ_REGISTER_BUFFER_UCHAR(Register,static_cast<PUCHAR>(Buffer),Length);
}