//********************************************************************
//	created:	27:7:2008   1:53
//	file:		pci.ppbridge.cpp
//	author:		tiamo
//	purpose:	pci-to-pci bridge
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PPBridge_GetAdditionalResourceDescriptors)
#pragma alloc_text("PAGE",PPBridge_MassageHeaderForLimitsDetermination)
#pragma alloc_text("PAGE",PPBridge_RestoreCurrent)
#pragma alloc_text("PAGE",PPBridge_SaveLimits)
#pragma alloc_text("PAGE",PPBridge_SaveCurrentSettings)
#pragma alloc_text("PAGE",PciBridgeIsPositiveDecode)
#pragma alloc_text("PAGE",PciBridgeIsSubtractiveDecode)
#pragma alloc_text("PAGE",PciBridgeMemoryBase)
#pragma alloc_text("PAGE",PciBridgeMemoryLimit)
#pragma alloc_text("PAGE",PciBridgeIoBase)
#pragma alloc_text("PAGE",PciBridgeIoLimit)
#pragma alloc_text("PAGE",PciBridgePrefetchMemoryBase)
#pragma	alloc_text("PAGE",PciBridgePrefetchMemoryLimit)
#pragma alloc_text("PAGE",PciBridgeMemoryWorstCaseAlignment)

//
// reset device [checked]
//
NTSTATUS PPBridge_ResetDevice(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config)
{
	if(Config->Command)
		return STATUS_SUCCESS;

	if(!FlagOn(PdoExt->HackFlags.HighPart,PCI_HACK_FLAGS_HIGH_RESET_BRIDGE_OK))
		return STATUS_SUCCESS;

	ASSERT(!PdoExt->OnDebugPath);

	USHORT BridgeControl								= 0;
	PciReadDeviceConfig(PdoExt,&BridgeControl,FIELD_OFFSET(PCI_COMMON_HEADER,u.type1.BridgeControl),sizeof(BridgeControl));

	SetFlag(BridgeControl,0x40);
	PciWriteDeviceConfig(PdoExt,&BridgeControl,FIELD_OFFSET(PCI_COMMON_HEADER,u.type1.BridgeControl),sizeof(BridgeControl));

	KeStallExecutionProcessor(100);

	ClearFlag(BridgeControl,0x40);
	PciWriteDeviceConfig(PdoExt,&BridgeControl,FIELD_OFFSET(PCI_COMMON_HEADER,u.type1.BridgeControl),sizeof(BridgeControl));

	return STATUS_SUCCESS;
}

//
// get additional resource descriptors [checked]
//
VOID PPBridge_GetAdditionalResourceDescriptors(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in PIO_RESOURCE_DESCRIPTOR IoRes)
{
	PAGED_CODE();

	//
	// vga enabled
	//
	if(!FlagOn(Config->u.type1.BridgeControl,8))
		return;

	IoRes->Type											= CmResourceTypeDevicePrivate;
	IoRes->u.DevicePrivate.Data[0]						= 3;
	IoRes->u.DevicePrivate.Data[1]						= 3;
	IoRes												+= 1;

	IoRes->Type											= CmResourceTypeMemory;
	IoRes->Flags										= 0;
	IoRes->u.Memory.Length								= 0x20000;
	IoRes->u.Memory.MinimumAddress.QuadPart				= 0xa0000;
	IoRes->u.Memory.MaximumAddress.QuadPart				= 0xbffff;
	IoRes->u.Memory.Alignment							= 1;
	IoRes												+= 1;

	IoRes->Type											= CmResourceTypePort;
	IoRes->Flags										= CM_RESOURCE_PORT_10_BIT_DECODE | CM_RESOURCE_PORT_POSITIVE_DECODE;
	IoRes->u.Port.MinimumAddress.QuadPart				= 0x3b0;
	IoRes->u.Port.MaximumAddress.QuadPart				= 0x3bb;
	IoRes->u.Port.Length								= 0x0c;
	IoRes->u.Port.Alignment								= 1;
	IoRes												+= 1;

	IoRes->Type											= CmResourceTypePort;
	IoRes->Flags										= CM_RESOURCE_PORT_10_BIT_DECODE | CM_RESOURCE_PORT_POSITIVE_DECODE;
	IoRes->u.Port.MinimumAddress.QuadPart				= 0x3c0;
	IoRes->u.Port.MaximumAddress.QuadPart				= 0x3df;
	IoRes->u.Port.Length								= 0x20;
	IoRes->u.Port.Alignment								= 1;
}

//
// massage header [checked]
//
VOID PPBridge_MassageHeaderForLimitsDetermination(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	for(ULONG i = 0; i < ARRAYSIZE(Param->Working->u.type1.BaseAddresses); i ++)
		Param->Working->u.type1.BaseAddresses[i]		= 0xffffffff;

	Param->Working->u.type1.PrimaryBus					= Param->OriginalConfig->u.type1.PrimaryBus;
	Param->Working->u.type1.SecondaryBus				= Param->OriginalConfig->u.type1.SecondaryBus;
	Param->Working->u.type1.SubordinateBus				= Param->OriginalConfig->u.type1.SubordinateBus;
	Param->Working->u.type1.SecondaryLatency			= Param->OriginalConfig->u.type1.SecondaryLatency;
	Param->Working->u.type1.IOBase						= 0xff;
	Param->Working->u.type1.IOLimit						= 0xff;
	Param->Working->u.type1.SecondaryStatus				= 0xffff;
	Param->Working->u.type1.MemoryBase					= 0xffff;
	Param->Working->u.type1.MemoryLimit					= 0xffff;
	Param->Working->u.type1.PrefetchBase				= 0xffff;
	Param->Working->u.type1.PrefetchLimit				= 0xffff;
	Param->Working->u.type1.PrefetchBaseUpper32			= 0xffffffff;
	Param->Working->u.type1.PrefetchLimitUpper32		= 0xffffffff;
	Param->Working->u.type1.IOBaseUpper16				= 0xfffe;
	Param->Working->u.type1.IOLimitUpper16				= 0xffff;
	Param->Working->u.type1.SecondaryStatus				= 0;

	Param->SavedSecondaryStatus							= Param->OriginalConfig->u.type1.SecondaryStatus;
	Param->OriginalConfig->u.type1.SecondaryStatus		= 0;
}

//
// restore current [checked]
//
VOID PPBridge_RestoreCurrent(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	Param->OriginalConfig->u.type1.SecondaryStatus		= Param->SavedSecondaryStatus;
}

//
// save limits [checked]
//
VOID PPBridge_SaveLimits(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	//
	// build io descriptor from bars
	//
	for(ULONG i = 0; i < ARRAYSIZE(Param->Working->u.type1.BaseAddresses); i ++)
	{
		if(PciCreateIoDescriptorFromBarLimit(Param->PdoExt->Resources->Limit + i,Param->Working->u.type1.BaseAddresses + i,FALSE))
		{
			//
			// this is a 64 bits memory bar
			//
			ASSERT(i + 1 < ARRAYSIZE(Param->Working->u.type1.BaseAddresses));
			i											+= 1;
			Param->PdoExt->Resources->Limit[i].Type		= CmResourceTypeNull;
		}
	}

	PIO_RESOURCE_DESCRIPTOR NextDesc					= Param->PdoExt->Resources->Limit + ARRAYSIZE(Param->Working->u.type1.BaseAddresses);

	//
	// check subtractive decode
	//
	if(PciBridgeIsSubtractiveDecode(Param))
	{
		Param->PdoExt->Dependent.type1.SubtractiveDecode= TRUE;
		Param->PdoExt->Dependent.type1.IsaBitSet		= FALSE;
		Param->PdoExt->Dependent.type1.VgaBitSet		= FALSE;

		NextDesc										+= 3;
	}

	//
	// skip subtractive bridge
	//
	if(!Param->PdoExt->Dependent.type1.SubtractiveDecode)
	{
		for(ULONG i = 0; i < 3; i ++)
		{
			LARGE_INTEGER MaximumAddress;
			MaximumAddress.QuadPart						= 0;
			NextDesc->Type								= CmResourceTypeNull;
			NextDesc->u.Generic.MinimumAddress.QuadPart	= 0;
			NextDesc->u.Generic.Length					= 0;

			if(i == 0)
			{
				ASSERT(Param->Working->u.type1.IOLimit);
				MaximumAddress.LowPart					= PciBridgeIoLimit(Param->Working);

				ASSERT(!FlagOn(Param->Working->u.type1.IOLimit,0x0e));

				NextDesc->Type							= CmResourceTypePort;
				NextDesc->Flags							= CM_RESOURCE_PORT_POSITIVE_DECODE | CM_RESOURCE_PORT_WINDOW_DECODE | CM_RESOURCE_PORT_IO;
				NextDesc->u.Port.Alignment				= 0x1000;
			}
			else if(i == 1)
			{
				MaximumAddress.LowPart					= PciBridgeMemoryLimit(Param->Working);

				ASSERT(!FlagOn(Param->Working->u.type1.MemoryLimit,0x0f));

				NextDesc->Type							= CmResourceTypeMemory;
				NextDesc->Flags							= 0;
				NextDesc->u.Memory.Alignment			= 0x100000;
			}
			else
			{
				ASSERT(i == 2);

				 if(Param->Working->u.type1.PrefetchLimit)
				 {
					 MaximumAddress.QuadPart			= PciBridgePrefetchMemoryLimit(Param->Working);
					 NextDesc->Type						= CmResourceTypeMemory;
					 NextDesc->u.Memory.Alignment		= 0x100000;
					 NextDesc->Flags					= CM_RESOURCE_MEMORY_PREFETCHABLE;
				 }
			}

			NextDesc->u.Port.MaximumAddress.QuadPart	= MaximumAddress.QuadPart;
			NextDesc									+= 1;
		}
	}

	//
	// rom address
	//
	if(FlagOn(Param->OriginalConfig->u.type1.ROMBaseAddress,PCI_ROMADDRESS_ENABLED))
		PciCreateIoDescriptorFromBarLimit(NextDesc,&Param->Working->u.type1.ROMBaseAddress,TRUE);
}

//
// save current [checked]
//
VOID PPBridge_SaveCurrentSettings(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	PCI_COMMON_HEADER BiosConfig;
	PPCI_COMMON_HEADER Current;

	if(FlagOn(Param->SavedCommand,PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE) || !NT_SUCCESS(PciGetBiosConfig(Param->PdoExt,&BiosConfig)))
		Current											= Param->OriginalConfig;
	else
		Current											= &BiosConfig;

	for(ULONG i = 0; i < 6; i ++)
	{
		PIO_RESOURCE_DESCRIPTOR IoDesc					= Param->PdoExt->Resources->Limit + i;
		PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc			= Param->PdoExt->Resources->Current + i;

		CmDesc->Type									= IoDesc->Type;

		if(IoDesc->Type != CmResourceTypeNull)
		{
			CmDesc->Flags								= IoDesc->Flags;
			CmDesc->ShareDisposition					= IoDesc->ShareDisposition;

			LARGE_INTEGER Start;
			Start.QuadPart								= 0;

			if(i < ARRAYSIZE(Current->u.type1.BaseAddresses))
			{
				ULONG Mask								= PCI_ADDRESS_MEMORY_ADDRESS_MASK;
				if(FlagOn(Current->u.type1.BaseAddresses[i],PCI_ADDRESS_IO_SPACE))
					Mask								= PCI_ADDRESS_IO_ADDRESS_MASK;
				else if(FlagOn(Current->u.type1.BaseAddresses[i],PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_64BIT)
					Start.HighPart						= Current->u.type1.BaseAddresses[i + 1];

				Start.LowPart							= Current->u.type1.BaseAddresses[i] & Mask;
				CmDesc->u.Generic.Length				= IoDesc->u.Generic.Length;
			}
			else if(i == 5)
			{
				Start.LowPart							= Current->u.type1.ROMBaseAddress & PCI_ADDRESS_ROM_ADDRESS_MASK;
				CmDesc->u.Generic.Length				= IoDesc->u.Generic.Length;
			}
			else
			{
				LARGE_INTEGER Limit;
				Limit.QuadPart							= 0;

				BOOLEAN MemoryResource					= FALSE;
				BOOLEAN HardwareSupported				= FALSE;

				if(i == ARRAYSIZE(Current->u.type1.BaseAddresses))
				{
					Start.LowPart						= PciBridgeIoBase(Current);
					Limit.LowPart						= PciBridgeIoLimit(Current);

					if(!Start.LowPart && Param->Working->u.type1.IOLimit)
						HardwareSupported				= TRUE;
				}
				else if(i == ARRAYSIZE(Current->u.type1.BaseAddresses) + 1)
				{
					Start.LowPart						= PciBridgeMemoryBase(Current);
					Limit.LowPart						= PciBridgeMemoryLimit(Current);
					MemoryResource						= TRUE;
				}
				else
				{
					ASSERT(i == ARRAYSIZE(Current->u.type1.BaseAddresses) + 2);
					ASSERT(FlagOn(IoDesc->Flags,CM_RESOURCE_MEMORY_PREFETCHABLE));
				
					Start.QuadPart						= PciBridgePrefetchMemoryBase(Current);
					Limit.QuadPart						= PciBridgePrefetchMemoryLimit(Current);
					MemoryResource						= TRUE;
				}

				if(Start.QuadPart > Limit.QuadPart)
				{
					IoDesc->Type						= CmResourceTypeNull;
					CmDesc->Type						= CmResourceTypeNull;
					continue;
				}

				if(!Start.QuadPart && !HardwareSupported)
				{
					CmDesc->Type						= CmResourceTypeNull;
					continue;
				}

				LARGE_INTEGER Length;
				Length.QuadPart							= Limit.QuadPart - Start.QuadPart + 1;

				ASSERT(!Length.HighPart);

				CmDesc->u.Generic.Length				= Length.LowPart;

				if(MemoryResource)
				{
					ASSERT(Length.LowPart > 0);
					IoDesc->u.Memory.Alignment			= PciBridgeMemoryWorstCaseAlignment(Length.LowPart);
				}
			}
		
			CmDesc->u.Generic.Start.QuadPart			= Start.QuadPart;
		}
	}

	Param->PdoExt->Dependent.type1.PrimaryBus			= Param->OriginalConfig->u.type1.PrimaryBus;
	Param->PdoExt->Dependent.type1.SecondaryBus			= Param->OriginalConfig->u.type1.SecondaryBus;
	Param->PdoExt->Dependent.type1.SubordinateBus		= Param->OriginalConfig->u.type1.SubordinateBus;

	if(Param->PdoExt->Dependent.type1.SubtractiveDecode)
	{
		ASSERT(!Param->PdoExt->Dependent.type1.VgaBitSet);
		ASSERT(!Param->PdoExt->Dependent.type1.IsaBitSet);
	}
	else
	{
		if(FlagOn(Param->OriginalConfig->u.type1.BridgeControl,0x08))
		{
			Param->PdoExt->Dependent.type1.VgaBitSet	= TRUE;
			Param->PdoExt->AdditionalResourceCount		= 4;
		}

		if(FlagOn(Param->OriginalConfig->u.type1.BridgeControl,0x04))
			Param->PdoExt->Dependent.type1.IsaBitSet	= TRUE;
	}

	USHORT DeviceId										= Param->OriginalConfig->DeviceID;
	USHORT VendorId										= Param->OriginalConfig->VendorID;
	if( (VendorId != 0x8086 || (DeviceId != 0x2418 && DeviceId != 0x2428 && DeviceId != 0x244E &&DeviceId != 0x2448)) && 
		!FlagOn(Param->PdoExt->HackFlags.u.HighPart,PCI_HACK_FLAGS_HIGH_PRESERVE_BRIDGE_CONFIG))
		return;

	if(!Param->PdoExt->Dependent.type1.SubtractiveDecode)
		return;

	Param->PdoExt->ParentFdoExtension->PreservedConfig	= static_cast<PPCI_COMMON_CONFIG>(ExAllocatePoolWithTag(NonPagedPool,sizeof(PCI_COMMON_HEADER),'BicP'));
	if(Param->PdoExt->ParentFdoExtension->PreservedConfig)
		RtlCopyMemory(Param->PdoExt->ParentFdoExtension->PreservedConfig,Param->OriginalConfig,sizeof(PCI_COMMON_HEADER));
}

//
// change resource settings [checked]
//
VOID PPBridge_ChangeResourceSettings(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config)
{
	USHORT DeviceId										= PdoExt->DeviceId;
	USHORT VendorId										= PdoExt->VendorId;

	BOOLEAN RestoreConfig								= FALSE;

	if(VendorId == 0x8086 && (DeviceId == 0x2418 || DeviceId == 0x2428 || DeviceId == 0x244E || DeviceId == 0x2448))
		RestoreConfig									= PdoExt->Dependent.type1.SubtractiveDecode;
	else if(FlagOn(PdoExt->HackFlags.HighPart,PCI_HACK_FLAGS_HIGH_PRESERVE_BRIDGE_CONFIG))
		RestoreConfig									= PdoExt->Dependent.type1.SubtractiveDecode;

	if(RestoreConfig)
	{
		ASSERT(!PdoExt->Resources);
		PPCI_COMMON_CONFIG PreservedConfig				= PdoExt->ParentFdoExtension->PreservedConfig;

		Config->u.type1.IOLimit							= PreservedConfig->u.type1.IOLimit;
		Config->u.type1.IOBase							= PreservedConfig->u.type1.IOBase;
		Config->u.type1.IOBaseUpper16					= PreservedConfig->u.type1.IOBaseUpper16;
		Config->u.type1.MemoryLimit						= PreservedConfig->u.type1.MemoryLimit;
		Config->u.type1.MemoryBase						= PreservedConfig->u.type1.MemoryBase;
		Config->u.type1.PrefetchLimit					= PreservedConfig->u.type1.PrefetchLimit;
		Config->u.type1.PrefetchLimitUpper32			= PreservedConfig->u.type1.PrefetchLimitUpper32;
		Config->u.type1.PrefetchBase					= PreservedConfig->u.type1.PrefetchBase;
		Config->u.type1.PrefetchBaseUpper32				= PreservedConfig->u.type1.PrefetchBaseUpper32;
	}
	else
	{
		Config->u.type1.IOLimit							= 0;
		Config->u.type1.IOBase							= 0xff;
		Config->u.type1.IOBaseUpper16					= 0;
		Config->u.type1.MemoryLimit						= 0;
		Config->u.type1.MemoryBase						= 0xffff;
		Config->u.type1.PrefetchLimit					= 0;
		Config->u.type1.PrefetchLimitUpper32			= 0;
		Config->u.type1.PrefetchBase					= 0xffff;
		Config->u.type1.PrefetchBaseUpper32				= 0;
	}

	if(PdoExt->Resources)
	{
		for(ULONG i = 0; i < 6; i ++)
		{
			PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc		= PdoExt->Resources->Current + i;

			if(CmDesc->Type != CmResourceTypeNull)
			{
				ULONG StartLow							= CmDesc->u.Generic.Start.LowPart;
				ULONG LimitLow							= StartLow + CmDesc->u.Generic.Length - 1;

				if(i < ARRAYSIZE(Config->u.type1.BaseAddresses))
				{
					if(CmDesc->Type == CmResourceTypeMemory)
					{
						ULONG Type						= FlagOn(Config->u.type1.BaseAddresses[i],PCI_ADDRESS_MEMORY_TYPE_MASK);

						if(Type == PCI_TYPE_64BIT)
						{
							ASSERT(i == 0);
							ASSERT(CmDesc[1].Type == CmResourceTypeNull);

							Config->u.type1.BaseAddresses[i + 1]	= CmDesc->u.Memory.Start.HighPart;
						}
						else if(Type == PCI_TYPE_20BIT)
						{
							ASSERT(!FlagOn(CmDesc->u.Memory.Start.LowPart,0xfff00000));
						}
					}

					Config->u.type1.BaseAddresses[i]	= StartLow;
				}
				else if(i == 5)
				{
					ASSERT(CmDesc->Type == CmResourceTypeMemory);
					ClearFlag(Config->u.type1.ROMBaseAddress,PCI_ADDRESS_ROM_ADDRESS_MASK);
					SetFlag(Config->u.type1.ROMBaseAddress,FlagOn(StartLow,PCI_ADDRESS_ROM_ADDRESS_MASK));
				}
				else
				{
					if(i == ARRAYSIZE(Config->u.type1.BaseAddresses))
					{
						ASSERT(!FlagOn(StartLow,0xfff) && FlagOn(LimitLow,0xfff) == 0xfff);

						if(FlagOn(Config->u.type1.IOBase,0x0f) != 1)
							ASSERT(((StartLow | LimitLow) & 0xffff0000) == 0);

						Config->u.type1.IOBase			= static_cast<UCHAR>((StartLow >> 8) & 0xf0);
						Config->u.type1.IOLimit			= static_cast<UCHAR>((LimitLow >> 8) & 0xf0);
						Config->u.type1.IOBaseUpper16	= static_cast<USHORT>(StartLow >> 0x10);
						Config->u.type1.IOLimitUpper16	= static_cast<USHORT>(LimitLow >> 0x10);
					}
					else if(i == ARRAYSIZE(Config->u.type1.BaseAddresses) + 1)
					{
						ASSERT((StartLow & 0xfffff) == 0 && (LimitLow & 0xfffff) == 0xfffff);

						Config->u.type1.MemoryBase		= static_cast<USHORT>(StartLow >> 0x10);
						Config->u.type1.MemoryLimit		= static_cast<USHORT>(LimitLow >> 0x10) & 0xfff0;
					}
					else
					{
						ASSERT(i == ARRAYSIZE(Config->u.type1.BaseAddresses) + 2);

						LARGE_INTEGER BigLimit;
						BigLimit.QuadPart				= CmDesc->u.Memory.Start.QuadPart + CmDesc->u.Memory.Length - 1;

						ASSERT((StartLow & 0xfffff) == 0 && (BigLimit.LowPart & 0xfffff) == 0xfffff);

						Config->u.type1.PrefetchBase		= static_cast<USHORT>(StartLow >> 0x10);
						Config->u.type1.PrefetchBaseUpper32	= CmDesc->u.Memory.Start.HighPart;
						Config->u.type1.PrefetchLimit		= static_cast<USHORT>(BigLimit.LowPart >> 0x10) & 0xfff0;
						Config->u.type1.PrefetchLimitUpper32= BigLimit.HighPart;
					}
				}
			}
		}
	}

	Config->u.type1.PrimaryBus							= PdoExt->Dependent.type1.PrimaryBus;
	Config->u.type1.SecondaryBus						= PdoExt->Dependent.type1.SecondaryBus;
	Config->u.type1.SubordinateBus						= PdoExt->Dependent.type1.SubordinateBus;

	if(PdoExt->Dependent.type1.VgaBitSet)
		SetFlag(Config->u.type1.BridgeControl,0x08);

	if(PdoExt->Dependent.type1.IsaBitSet)
		SetFlag(Config->u.type1.BridgeControl,0x04);
}

//
// is positive decode [checked]
//
BOOLEAN PciBridgeIsPositiveDecode(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	return PciIsSlotPresentInParentMethod(PdoExt,'CEDP');
}

//
// is subtractive decode [checked]
//
BOOLEAN PciBridgeIsSubtractiveDecode(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	ASSERT(Param->OriginalConfig->BaseClass == PCI_CLASS_BRIDGE_DEV && Param->OriginalConfig->SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI);

	if(!FlagOn(Param->PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_SUBTRACTIVE_DECODE) && Param->OriginalConfig->ProgIf != 1 && FlagOn(Param->Working->u.type1.IOLimit,0xf0) == 0xf0)
		return FALSE;

	USHORT DeviceId										= Param->OriginalConfig->DeviceID;
	USHORT VendorId										= Param->OriginalConfig->VendorID;
	if( (VendorId == 0x8086 && (DeviceId == 0x2418 || DeviceId == 0x2428 || DeviceId == 0x244E || DeviceId == 0x2448)) || 
		FlagOn(Param->PdoExt->HackFlags.u.HighPart,PCI_HACK_FLAGS_HIGH_PRESERVE_BRIDGE_CONFIG))
	{
		if(PciBridgeIsPositiveDecode(Param->PdoExt))
		{
			PciDebugPrintf(1,"Putting bridge in positive decode because of PDEC\n");
			return FALSE;
		}
	}

	PciDebugPrintf(1,"PCI : Subtractive decode on Bus 0x%x\n",Param->OriginalConfig->u.type1.SecondaryBus);

	Param->PdoExt->UpdateHardware						= TRUE;

	return TRUE;
}

//
// memory base [checked]
//
ULONG PciBridgeMemoryBase(__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	ASSERT(PCI_CONFIGURATION_TYPE(Config) == PCI_BRIDGE_TYPE);

	return static_cast<ULONG>(Config->u.type1.MemoryBase) << 0x10;
}

//
// memory limit [checked]
//
ULONG PciBridgeMemoryLimit(__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	ASSERT(PCI_CONFIGURATION_TYPE(Config) == PCI_BRIDGE_TYPE);

	return (static_cast<ULONG>(Config->u.type1.MemoryLimit) << 0x10) | 0xfffff;
}

//
// io base [checked]
//
ULONG PciBridgeIoBase(__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	ASSERT(PCI_CONFIGURATION_TYPE(Config) == PCI_BRIDGE_TYPE);

	ULONG Base											= static_cast<ULONG>(Config->u.type1.IOBase & 0xf0) << 8;

	if(FlagOn(Config->u.type1.IOBase,0xf) == 1)
	{
		Base											|= (static_cast<ULONG>(Config->u.type1.IOBaseUpper16) << 0x10);
		ASSERT(FlagOn(Config->u.type1.IOLimit,1));
	}

	return Base;
}

//
// io limit [checked]
//
ULONG PciBridgeIoLimit(__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	ASSERT(PCI_CONFIGURATION_TYPE(Config) == PCI_BRIDGE_TYPE);

	ULONG Limit											= static_cast<ULONG>(Config->u.type1.IOLimit & 0xf0) << 8;

	if(FlagOn(Config->u.type1.IOLimit,0xf) == 1)
	{
		Limit											|= (static_cast<ULONG>(Config->u.type1.IOLimitUpper16) << 0x10);
		ASSERT(FlagOn(Config->u.type1.IOBase,1));
	}

	return Limit | 0xfff;
}

//
// prefetch memory base [checked]
//
LONGLONG PciBridgePrefetchMemoryBase(__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	ASSERT(PCI_CONFIGURATION_TYPE(Config) == PCI_BRIDGE_TYPE);

	LARGE_INTEGER Base;

	Base.LowPart										= (static_cast<ULONG>(Config->u.type1.PrefetchBase) & 0xfffffff0) << 0x10;

	if(FlagOn(Config->u.type1.PrefetchLimit,0xf) == 1)
		Base.HighPart									= static_cast<LONG>(Config->u.type1.PrefetchBaseUpper32);
	else
		Base.HighPart									= 0;

	return Base.QuadPart;
}

//
// prefetch memory limit [checked]
//
LONGLONG PciBridgePrefetchMemoryLimit(__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	ASSERT(PCI_CONFIGURATION_TYPE(Config) == PCI_BRIDGE_TYPE);

	LARGE_INTEGER Limit;

	Limit.LowPart										= (static_cast<ULONG>(Config->u.type1.PrefetchLimit) << 0x10) | 0xfffff;

	if(FlagOn(Config->u.type1.PrefetchLimit,0xf) == 1)
		Limit.HighPart									= static_cast<LONG>(Config->u.type1.PrefetchLimitUpper32);
	else
		Limit.HighPart									= 0;

	return Limit.QuadPart;
}

//
// calc alignment [checked]
//
ULONG PciBridgeMemoryWorstCaseAlignment(__in ULONG Length)
{
	PAGED_CODE();

	if(!Length)
	{
		ASSERT(Length);
		return 0;
	}

	ULONG Alignment										= 0x80000000;

	while(!(Length & Alignment))
		Alignment										>>= 1;

	return Alignment;
}