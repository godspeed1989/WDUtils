//********************************************************************
//	created:	23:7:2008   15:19
//	file:		pci.enum.cpp
//	author:		tiamo
//	purpose:	enum
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciQueryDeviceRelations)
#pragma alloc_text("PAGE",PciQueryTargetDeviceRelations)
#pragma alloc_text("PAGE",PciQueryEjectionRelations)
#pragma alloc_text("PAGE",PciScanBus)
#pragma alloc_text("PAGE",PciProcessBus)
#pragma alloc_text("PAGE",PciGetEnhancedCapabilities)
#pragma alloc_text("PAGE",PciGetFunctionLimits)
#pragma alloc_text("PAGE",PcipGetFunctionLimits)
#pragma alloc_text("PAGE",PciWriteLimitsAndRestoreCurrent)
#pragma alloc_text("PAGE",PciComputeNewCurrentSettings)
#pragma alloc_text("PAGE",PciQueryRequirements)
#pragma alloc_text("PAGE",PciQueryResources)
#pragma alloc_text("PAGE",PciAllocateCmResourceList)
#pragma alloc_text("PAGE",PciGetInUseRanges)
#pragma alloc_text("PAGE",PciBuildGraduatedWindow)
#pragma alloc_text("PAGE",PciPrivateResourceInitialize)

//
// query bus relations [checked]
//
NTSTATUS PciQueryDeviceRelations(__in PPCI_FDO_EXTENSION FdoExt,__inout PDEVICE_RELATIONS* Relations)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		//
		// must be in start state
		//
		if(FdoExt->Common.DeviceState != PciStarted)
			try_leave(Status = STATUS_INVALID_DEVICE_REQUEST;ASSERT(FdoExt->Common.DeviceState == PciStarted));

		Status											= PciBeginStateTransition(&FdoExt->Common,PciSynchronizedOperation);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// mark all children as not present
		//
		PPCI_PDO_EXTENSION PdoExt						= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);

		while(PdoExt)
		{
			PdoExt->NotPresent							= TRUE;

			PdoExt										= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		}

		//
		// rescan bus
		//
		Status											= PciScanBus(FdoExt);
		ASSERT(NT_SUCCESS(Status));

		//
		// count children
		//
		ULONG Count										= 0;
		PdoExt											= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);

		while(PdoExt)
		{
			if(PdoExt->NotPresent)
			{
				PciDebugPrintf(0x7fffffff,"PCI - Old device (pdox) %08x not found on rescan.\n",PdoExt);
				PdoExt->ReportedMissing					= TRUE;
			}
			else
			{
				Count									+= 1;
			}

			PdoExt										= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		}

		//
		// calc buffer length
		//
		ULONG Length									= Count * sizeof(PDEVICE_OBJECT) + sizeof(DEVICE_RELATIONS);
		if(*Relations)
			Length										+= (*Relations)->Count * sizeof(PDEVICE_OBJECT);

		//
		// allocate buffer
		//
		PDEVICE_RELATIONS Buffer						= static_cast<PDEVICE_RELATIONS>(ExAllocatePoolWithTag(NonPagedPool,Length,'BicP'));
		if(!Buffer)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		ULONG Index										= 0;

		//
		// copy old buffer,and free it
		//
		if(*Relations && (*Relations)->Count)
		{
			RtlCopyMemory(Buffer->Objects,(*Relations)->Objects,(*Relations)->Count * sizeof(PDEVICE_OBJECT));

			Index										= (*Relations)->Count;

			ExFreePool(*(Relations));

			*Relations									= 0;
		}

		Buffer->Count									= Index;

		//
		// store our children
		//
		PciDebugPrintf(0x7fffffff,"PCI QueryDeviceRelations/BusRelations FDOx %08x (bus 0x%02x)\n",FdoExt,FdoExt->BaseBus);

		PdoExt											= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);

		while(PdoExt)
		{
			if(PdoExt->NotPresent)
			{
				PciDebugPrintf(0x7fffffff,"  QDR PDO %08x (x %08x) <Omitted, device flaged not present>\n",PdoExt->PhysicalDeviceObject,PdoExt);
			}
			else
			{
				PciDebugPrintf(0x7fffffff,"  QDR PDO %08x (x %08x) \n",PdoExt->PhysicalDeviceObject,PdoExt);
				ObReferenceObject(PdoExt->PhysicalDeviceObject);

				Buffer->Objects[Buffer->Count ++]		= PdoExt->PhysicalDeviceObject;
			}

			PdoExt										= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		}

		PciDebugPrintf(0x7fffffff,"  QDR Total PDO count = %d (%d already in list)\n",Buffer->Count,Index);

		*Relations										= Buffer;
	}
	__finally
	{
		PciCancelStateTransition(&FdoExt->Common,PciSynchronizedOperation);
	}

	return Status;
}

//
// query target relations [checked]
//
NTSTATUS PciQueryTargetDeviceRelations(__in PPCI_PDO_EXTENSION PdoExt,__inout PDEVICE_RELATIONS* Relations)
{
	PAGED_CODE();

	if(*Relations)
		ExFreePool(*Relations);

	*Relations											= static_cast<PDEVICE_RELATIONS>(ExAllocatePoolWithTag(NonPagedPool,sizeof(DEVICE_RELATIONS),'BicP'));
	if(!*Relations)
		return STATUS_INSUFFICIENT_RESOURCES;

	(*Relations)->Count									= 1;
	(*Relations)->Objects[0]							= PdoExt->PhysicalDeviceObject;
	ObReferenceObject(PdoExt->PhysicalDeviceObject);

	return STATUS_SUCCESS;
}

//
// query ejection relations [checked]
//
NTSTATUS PciQueryEjectionRelations(__in PPCI_PDO_EXTENSION PdoExt,__inout PDEVICE_RELATIONS* Relations)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&PdoExt->ParentFdoExtension->ChildListLock,Executive,KernelMode,FALSE,0);

		//
		// count whose devices in parent fdo's children list whose device numbe equals to ours
		//
		ULONG Count										= 0;
		PPCI_PDO_EXTENSION ChildPdoExt					= CONTAINING_RECORD(PdoExt->ParentFdoExtension->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		while(ChildPdoExt)
		{
			if(ChildPdoExt != PdoExt && !ChildPdoExt->NotPresent && ChildPdoExt->Slot.u.bits.DeviceNumber == PdoExt->Slot.u.bits.DeviceNumber)
				Count									+= 1;

			ChildPdoExt									= CONTAINING_RECORD(ChildPdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		}

		if(!Count)
			try_leave(Status = STATUS_NOT_SUPPORTED);

		//
		// allocate buffer
		//
		ULONG Index										= *Relations ? (*Relations)->Count : 0;
		ULONG Length									= sizeof(DEVICE_RELATIONS) + (Index + Count) * sizeof(PDEVICE_OBJECT);
		PDEVICE_RELATIONS Buffer						= static_cast<PDEVICE_RELATIONS>(ExAllocatePoolWithTag(NonPagedPool,Length,'BicP'));
		if(!Buffer)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// copy old buffer,and free it
		//
		if(*Relations)
		{
			RtlCopyMemory(Buffer->Objects,(*Relations)->Objects,(*Relations)->Count * sizeof(PDEVICE_OBJECT));

			ExFreePool(*(Relations));

			*Relations									= 0;
		}

		Buffer->Count									= Index;

		//
		// fill ours
		//
		ChildPdoExt					= CONTAINING_RECORD(PdoExt->ParentFdoExtension->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		while(ChildPdoExt)
		{
			if(ChildPdoExt != PdoExt && !ChildPdoExt->NotPresent && ChildPdoExt->Slot.u.bits.DeviceNumber == PdoExt->Slot.u.bits.DeviceNumber)
			{
				Buffer->Objects[Buffer->Count ++]		= ChildPdoExt->PhysicalDeviceObject;
				ObReferenceObject(ChildPdoExt->PhysicalDeviceObject);
			}

			ChildPdoExt									= CONTAINING_RECORD(ChildPdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		}

		*Relations										= Buffer;

		Status											= STATUS_SUCCESS;
	}
	__finally
	{
		KeSetEvent(&PdoExt->ParentFdoExtension->ChildListLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}

	return Status;
}

//
// scan bus [checked]
//
NTSTATUS PciScanBus(__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	PciDebugPrintf(7,"PCI Scan Bus: FDO Extension @ 0x%x, Base Bus = 0x%x\n",FdoExt,FdoExt->BaseBus);

	NTSTATUS Status										= STATUS_SUCCESS;
	BOOLEAN NewPdoCreated								= FALSE;

	__try
	{
		//
		// check max device override
		//
		ULONG MaxDevice									= PCI_MAX_DEVICES;
		if(FdoExt->BusRootFdoExtension != FdoExt)
		{
			PPCI_PDO_EXTENSION PdoExt					= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);
			ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

			if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_ONLY_ONE_DEVICE_ON_BUS))
				MaxDevice								= 1;

			//
			// check bus number
			//
			UCHAR BusNumber								= 0;
			PciReadDeviceConfig(PdoExt,&BusNumber,FIELD_OFFSET(PCI_COMMON_HEADER,u.type1.SecondaryBus),sizeof(BusNumber));

			if(BusNumber != PdoExt->Dependent.type1.SecondaryBus)
			{
				DbgPrint("PCI: Bus numbers have been changed!  Restoring originals.\n");

				PciSetBusNumbers(PdoExt,PdoExt->Dependent.type1.PrimaryBus,PdoExt->Dependent.type1.SecondaryBus,PdoExt->Dependent.type1.SubordinateBus);
			}
		}

		//
		// scan bus
		//
		PCI_COMMON_HEADER DeviceConfig;
		for(ULONG CurrentDevice = 0; CurrentDevice < MaxDevice; CurrentDevice ++)
		{
			for(ULONG CurrentFunction = 0; CurrentFunction < PCI_MAX_FUNCTION; CurrentFunction ++)
			{
				PCI_SLOT_NUMBER Slot;
				Slot.u.AsULONG							= 0;
				Slot.u.bits.DeviceNumber				= CurrentDevice;
				Slot.u.bits.FunctionNumber				= CurrentFunction;

				//
				// read vendor id
				//
				PciReadSlotConfig(FdoExt,Slot,&DeviceConfig,FIELD_OFFSET(PCI_COMMON_HEADER,VendorID),sizeof(DeviceConfig.VendorID));

				if(DeviceConfig.VendorID != PCI_INVALID_VENDORID && DeviceConfig.VendorID != 0)
				{
					//
					// found a device,then read the whole config header
					//
					PciReadSlotConfig(FdoExt,Slot,&DeviceConfig.DeviceID,FIELD_OFFSET(PCI_COMMON_HEADER,DeviceID),sizeof(DeviceConfig) - sizeof(DeviceConfig.VendorID));

					//
					// apply hacks to the device config buffer
					//
					PciApplyHacks(FdoExt,&DeviceConfig,Slot,0,0);

					PciDebugPrintf(7,"Scan Found Device (b=0x%x, d=0x%x, f=0x%x)\n",FdoExt->BaseBus,CurrentDevice,CurrentFunction);

					//
					// dump common config
					//
					PciDebugDumpCommonConfig(&DeviceConfig);

					//
					// get description message
					//
					PWCHAR Description					= PciGetDeviceDescriptionMessage(DeviceConfig.BaseClass,DeviceConfig.SubClass);

					PciDebugPrintf(7,"Device Description \"%S\".\n",Description ? Description : L"(null)");

					//
					// free it
					//
					if(Description)
						ExFreePool(Description);

					USHORT SubSystemId					= 0;
					USHORT SubVendorId					= 0;

					if(PCI_CONFIGURATION_TYPE(&DeviceConfig) == PCI_DEVICE_TYPE && DeviceConfig.BaseClass != PCI_CLASS_BRIDGE_DEV)
					{
						SubSystemId						= DeviceConfig.u.type0.SubSystemID;
						SubVendorId						= DeviceConfig.u.type0.SubVendorID;
					}

					//
					// read hack flags
					//
					ULARGE_INTEGER HackFlags;
					HackFlags.QuadPart					= PciGetHackFlags(DeviceConfig.VendorID,DeviceConfig.DeviceID,SubVendorId,SubSystemId,DeviceConfig.RevisionID);

					//
					// should we skip this function?
					//
					if(!PciSkipThisFunction(&DeviceConfig,Slot,1,HackFlags))
					{
						//
						// this device already exists?
						//
						PPCI_PDO_EXTENSION PdoExt		= PciFindPdoByFunction(FdoExt,Slot,&DeviceConfig);
						if(PdoExt)
						{
							//
							// set as it present on the bus
							//
							PdoExt->NotPresent			= FALSE;
							ASSERT(PdoExt->Common.DeviceState != PciDeleted);
						}
						else
						{
							//
							// create it
							//
							NewPdoCreated				= TRUE;
							PDEVICE_OBJECT Pdo			= 0;
							Status						= PciPdoCreate(FdoExt,Slot,&Pdo);
							if(!NT_SUCCESS(Status))
								try_leave(ASSERT(NT_SUCCESS(Status)));

							PdoExt						= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);

							//
							// writeback failure
							//
							if(FlagOn(HackFlags.HighPart,PCI_HACK_FLAGS_HIGH_WRITEBACK_FAILURE))
							{
								PdoExt->ExpectedWritebackFailure	= TRUE;
								DeviceConfig.BaseClass	= PCI_CLASS_BASE_SYSTEM_DEV;
								DeviceConfig.SubClass	= PCI_SUBCLASS_SYS_OTHER;
							}

							//
							// save ids into extension
							//
							PdoExt->VendorId			= DeviceConfig.VendorID;
							PdoExt->DeviceId			= DeviceConfig.DeviceID;
							PdoExt->RevisionId			= DeviceConfig.RevisionID;
							PdoExt->BaseClass			= DeviceConfig.BaseClass;
							PdoExt->SubClass			= DeviceConfig.SubClass;
							PdoExt->ProgIf				= DeviceConfig.ProgIf;
							PdoExt->HeaderType			= PCI_CONFIGURATION_TYPE(&DeviceConfig);

							//
							// if this is a bridge pdo,link it
							//
							if( (PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV) &&
								(PdoExt->SubClass == PCI_SUBCLASS_BR_CARDBUS || PdoExt->SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI))
							{
								KeEnterCriticalRegion();
								KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

								if(!FdoExt->ChildBridgePdoList)
								{
									FdoExt->ChildBridgePdoList	= PdoExt;
								}
								else
								{
									PPCI_PDO_EXTENSION Prev		= FdoExt->ChildBridgePdoList;
									while(Prev->NextBridge)
										Prev					= Prev->NextBridge;

									Prev->NextBridge			= PdoExt;
								}

								ASSERT(!PdoExt->NextBridge);

								KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
								KeLeaveCriticalRegion();
							}

							//
							// read bios config from registry
							//
							PCI_COMMON_HEADER BiosConfig;
							BOOLEAN WriteConfig					= FALSE;
							if(NT_SUCCESS(PciGetBiosConfig(PdoExt,&BiosConfig)))
							{
								//
								// is the same device?
								//
								if(PcipIsSameDevice(PdoExt,&BiosConfig))
								{
									//
									// interrupt line changed?
									//
									if(DeviceConfig.u.type0.InterruptLine != BiosConfig.u.type0.InterruptLine)
										PciWriteDeviceConfig(PdoExt,&BiosConfig.u.type0.InterruptLine,FIELD_OFFSET(PCI_COMMON_HEADER,u.type0.InterruptLine),1);
								
									PdoExt->RawInterruptLine	= BiosConfig.u.type0.InterruptLine;
									PdoExt->InitialCommand		= BiosConfig.Command;
								}
								else
								{
									//
									// device changed,overwrite registry
									//
									WriteConfig			= TRUE;
								}
							}
							else
							{
								//
								// we should write device config to registry
								//
								WriteConfig				= TRUE;
							}

							//
							// write DeviceConfig to registry
							//
							if(WriteConfig)
							{
								Status					= PciSaveBiosConfig(PdoExt,&DeviceConfig);
								ASSERT(NT_SUCCESS(Status));

								//
								// save interrupt line and initial command
								//
								PdoExt->RawInterruptLine= DeviceConfig.u.type0.InterruptLine;
								PdoExt->InitialCommand	= DeviceConfig.Command;
							}

							PdoExt->CommandEnables		= DeviceConfig.Command;
							PdoExt->HackFlags.QuadPart	= HackFlags.QuadPart;

							//
							// read enhanced capabilities
							//
							PciGetEnhancedCapabilities(PdoExt,&DeviceConfig);

							//
							// power up device
							//
							PciSetPowerManagedDevicePowerState(PdoExt,PowerDeviceD0,FALSE);

							//
							// apply hacks to device hardware
							//
							PciApplyHacks(FdoExt,&DeviceConfig,Slot,1,PdoExt);

							//
							// save interrupt pin
							//
							PdoExt->InterruptPin		= DeviceConfig.u.type0.InterruptPin;

							//
							// get interrupt line
							//
							PdoExt->AdjustedInterruptLine	= PciGetAdjustedInterruptLine(PdoExt);

							//
							// check on debug path
							//
							PdoExt->OnDebugPath			= PciIsDeviceOnDebugPath(PdoExt);

							//
							// get function limit
							//
							Status						= PciGetFunctionLimits(PdoExt,&DeviceConfig,HackFlags);

							//
							// set as current device state
							//
							PciSetPowerManagedDevicePowerState(PdoExt,PdoExt->PowerState.CurrentDeviceState,FALSE);

							//
							// get function limit failed
							//
							if(!NT_SUCCESS(Status))
								try_leave(ASSERT(NT_SUCCESS(Status));PciPdoDestroy(Pdo));

							//
							// sub id hack
							//
							if(FlagOn(HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_SUB_IDS))
							{
								PdoExt->SubSystemId		= 0;
								PdoExt->SubVendorId		= 0;
							}

							//
							// check capabilites
							//
							UCHAR CurrentOffset			= PdoExt->CapabilitiesPtr;
							while(CurrentOffset)
							{
								//
								// read capablities head
								//
								PCI_CAPABILITIES_HEADER Head;
								UCHAR TempOffset		= PciReadDeviceCapability(PdoExt,CurrentOffset,0,&Head,sizeof(Head));
								if(TempOffset != CurrentOffset)
								{
									PciDebugPrintf(0,"PCI - Failed to read PCI capability at offset 0x%02x\n",CurrentOffset);
									ASSERT(TempOffset == CurrentOffset);
									break;
								}

								//
								// capabilities buffer
								//
								UCHAR Buffer[sizeof(PCI_AGP_CAPABILITY) > sizeof(PCI_PM_CAPABILITY) ? sizeof(PCI_AGP_CAPABILITY) : sizeof(PCI_PM_CAPABILITY)];
								ULONG Length			= 0;
								PCHAR CapsName			= "UNKNOWN CAPABILITY";
								if(Head.CapabilityID == PCI_CAPABILITY_ID_AGP)
									Length				= sizeof(PCI_AGP_CAPABILITY),CapsName = "AGP";
								else if(Head.CapabilityID == PCI_CAPABILITY_ID_POWER_MANAGEMENT)
									Length				= sizeof(PCI_PM_CAPABILITY),CapsName = "POWER";

								//
								// read the whole buffer
								//
								if(Length)
									TempOffset			= PciReadDeviceCapability(PdoExt,CurrentOffset,Head.CapabilityID,Buffer,Length);

								PciDebugPrintf(7,"CAP @%02x ID %02x (%s)",CurrentOffset,Head.CapabilityID,CapsName);

								if(TempOffset != CurrentOffset)
								{
									PciDebugPrintf(0,"- Failed to read capability data. ***\n");
									ASSERT(TempOffset == CurrentOffset);
									break;
								}

								for(ULONG i = 0; i < Length; i ++)
									PciDebugPrintf(7," %02x",Buffer[i]);

								PciDebugPrintf(7,"\n");

								CurrentOffset			= Head.Next;
							}

							//
							// check we can power down this function
							//
							if(PdoExt->BaseClass == PCI_CLASS_MASS_STORAGE_CTLR && PdoExt->SubClass == PCI_SUBCLASS_MSC_IDE_CTLR)
								PdoExt->DisablePowerDown	= TRUE;

							if(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && PdoExt->SubClass > PCI_SUBCLASS_BR_HOST && PdoExt->SubClass < PCI_SUBCLASS_BR_PCI_TO_PCI)
								PdoExt->DisablePowerDown	= TRUE;

							//
							// 82375EB/SB PCI to EISA Bridge
							//
							if(PdoExt->VendorId == 0x8086 && PdoExt->DeviceId == 0x482)
								PdoExt->DisablePowerDown	= TRUE;

							//
							// unconfigured device?
							//
							if( !FlagOn(PdoExt->CommandEnables,PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER) &&
								!DeviceConfig.LatencyTimer && !DeviceConfig.CacheLineSize)
							{
								PciDebugPrintf(0x80000,"PCI - ScanBus, PDOx %x found unconfigured\n",PdoExt);
								PdoExt->NeedsHotPlugConfiguration	= TRUE;
							}

							PdoExt->SavedCacheLineSize	= DeviceConfig.LatencyTimer;
							PdoExt->SavedCacheLineSize	= DeviceConfig.CacheLineSize;

							//
							// finished
							//
							ClearFlag(Pdo->Flags,DO_DEVICE_INITIALIZING);
						}
					}

					//
					// this is not a multi-function device,then try the next device instead of the next function
					//
					if(!CurrentFunction && !PCI_MULTIFUNCTION_DEVICE(&DeviceConfig))
						break;
				}
				else
				{
					//
					// read vendor id failed,if this is the first function,then skip the whole device
					//
					if(!CurrentFunction)
						break;
				}
			}
		}

		if(NewPdoCreated)
			PciProcessBus(FdoExt);

		Status											= STATUS_SUCCESS;
	}
	__finally
	{

	}

	return Status;
}

//
// apply hacks [checked]
//
NTSTATUS PciApplyHacks(__in PPCI_FDO_EXTENSION FdoExt,__in PPCI_COMMON_HEADER PciConfig,__in PCI_SLOT_NUMBER Slot,__in ULONG Phase,__in PPCI_PDO_EXTENSION PdoExt)
{
	if(Phase == 0)
	{
		ASSERT(!PdoExt);

		if(PciConfig->VendorID == 0x8086)
		{
			if(PciConfig->DeviceID == 0x482)
				PciConfig->SubClass						= PCI_SUBCLASS_BR_EISA;
			else if(PciConfig->DeviceID == 0x484)
				PciConfig->SubClass						= PCI_SUBCLASS_BR_ISA;
			else
				return STATUS_SUCCESS;

			PciConfig->BaseClass						= PCI_CLASS_BRIDGE_DEV;
		}
	}
	else if(Phase == 1)
	{
		ASSERT(PdoExt);

		if(PciConfig->VendorID == 0x1045 && PciConfig->DeviceID == 0xc621)
		{
			SetLegacyModeIde(PciConfig->ProgIf);
			PdoExt->ExpectedWritebackFailure			= TRUE;
		}
		else if(PciConfig->BaseClass == PCI_CLASS_MASS_STORAGE_CTLR && PciConfig->SubClass == PCI_SUBCLASS_MSC_IDE_CTLR)
		{
			if(PciEnableNativeModeATA && *InitSafeBootMode == FALSE && PciIsSlotPresentInParentMethod(PdoExt,'ATAN'))
				PdoExt->BIOSAllowsIDESwitchToNativeMode	= TRUE;
			else
				PdoExt->BIOSAllowsIDESwitchToNativeMode	= FALSE;

			PdoExt->SwitchedIDEToNativeMode				= PciConfigureIdeController(PdoExt,PciConfig,TRUE);
		}

		if(PciConfig->BaseClass == PCI_CLASS_MASS_STORAGE_CTLR && PciConfig->SubClass == PCI_SUBCLASS_MSC_IDE_CTLR && !NativeModeIde(PciConfig->ProgIf))
			PciConfig->u.type0.InterruptPin				= 0;
	
		if(FlagOn(PdoExt->HackFlags.HighPart,PCI_HACK_FLAGS_HIGH_BROKEN_VIDEO) && FdoExt == FdoExt->BusRootFdoExtension && !FdoExt->BrokenVideoHackApplied)
			ario_ApplyBrokenVideoHack(FdoExt);

		if(PciConfig->VendorID == 0x0e11 && PciConfig->DeviceID == 0xa0f7 && PciConfig->RevisionID == 0x11 && ExIsProcessorFeaturePresent(PF_PAE_ENABLED))
		{
			ClearFlag(PciConfig->Command,PCI_ENABLE_BUS_MASTER | PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
			PciWriteDeviceConfig(PdoExt,&PciConfig->Command,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(PciConfig->Command));
			ClearFlag(PdoExt->CommandEnables,PCI_ENABLE_BUS_MASTER | PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
			SetFlag(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_DONOT_TOUCH_COMMAND);
		}

		if(PCI_CONFIGURATION_TYPE(PciConfig) == PCI_CARDBUS_BRIDGE_TYPE)
		{
			ULONG Temp									= 0;
			PciWriteDeviceConfig(PdoExt,&Temp,0x44,sizeof(ULONG));
		}
	}
	else if(Phase == 3)
	{
		ASSERT(PdoExt);

		if(PdoExt->VendorId == 0x1014 && PdoExt->DeviceId == 0x95)
		{
			USHORT Command;
			PciReadDeviceConfig(PdoExt,&Command,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(Command));

			PciDecodeEnable(PdoExt,FALSE,&Command);

			UCHAR OffsetE0;
			PciReadDeviceConfig(PdoExt,&OffsetE0,0xe0,sizeof(OffsetE0));

			ClearFlag(OffsetE0,2);
			SetFlag(OffsetE0,1);
			PciWriteDeviceConfig(PdoExt,&OffsetE0,0xe0,sizeof(OffsetE0));

			PciWriteDeviceConfig(PdoExt,&Command,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(Command));
		}

		if( PdoExt->HeaderType == PCI_BRIDGE_TYPE && PdoExt->Dependent.type1.SubtractiveDecode &&
			(PdoExt->VendorId != 0x8086 || (PdoExt->DeviceId != 0x2418 && PdoExt->DeviceId != 0x2428 && PdoExt->DeviceId != 0x244e && PdoExt->DeviceId != 0x2448)) &&
			!FlagOn(PdoExt->HackFlags.HighPart,PCI_HACK_FLAGS_HIGH_PRESERVE_BRIDGE_CONFIG))
		{
			PciConfig->u.type1.MemoryBase				= 0xffff;
			PciConfig->u.type1.PrefetchBase				= 0xffff;
			PciConfig->u.type1.IOBase					= 0xff;
			PciConfig->u.type1.IOLimit					= 0;
			PciConfig->u.type1.MemoryLimit				= 0;
			PciConfig->u.type1.PrefetchLimit			= 0;
			PciConfig->u.type1.PrefetchBaseUpper32		= 0;
			PciConfig->u.type1.PrefetchLimitUpper32		= 0;
			PciConfig->u.type1.IOBaseUpper16			= 0;
			PciConfig->u.type1.IOLimitUpper16			= 0;
		}

		if(PdoExt->HeaderType == PCI_CARDBUS_BRIDGE_TYPE)
		{
			ULONG Temp									= 0;
			PciWriteDeviceConfig(PdoExt,&Temp,0x44,sizeof(ULONG));
		}
	}

	return STATUS_SUCCESS;
}

//
// should we skip this function [checked]
//
BOOLEAN PciSkipThisFunction(__in PPCI_COMMON_HEADER Config,__in PCI_SLOT_NUMBER Slot,__in ULONG Type,__in ULARGE_INTEGER HackFlags)
{
	if(Type == 1 || Type == 2)
	{
		if(FlagOn(HackFlags.LowPart,PCI_HACK_FLAGS_LOW_SKIP_DEVICE_TYPE1_8) && Type == 1)
		{
			PciDebugPrintf(7,"   Device skipped (not enumerated) <HackFlags & 8>.\n");
			return TRUE;
		}

		if(FlagOn(HackFlags.LowPart,PCI_HACK_FLAGS_LOW_SKIP_DEVICE_TYPE2_10) && Type == 2)
		{
			PciDebugPrintf(7,"   Device skipped (not enumerated) <HackFlags & 0x10>.\n");
			return TRUE;
		}

		if(FlagOn(HackFlags.LowPart,PCI_HACK_FLAGS_LOW_GHOST_DEVICE) && Slot.u.bits.DeviceNumber >= 0x10 && Type == 1)
		{
			PciDebugPrintf(7,"   Device skipped (not enumerated) <HackFlags & 0x1000 AND device >= 0x10> ghost device.\n");
			return TRUE;
		}

		if(Config->BaseClass == PCI_CLASS_BRIDGE_DEV)
		{
			if(Config->SubClass <= PCI_SUBCLASS_BR_MCA && Type == 2)
			{
				PciDebugPrintf(7,"   Device skipped (not enumerated) <PCI_CLASS_BRIDGE_DEV:PCI_SUBCLASS_BR_MCA 2>.\n");
				return TRUE;
			}
		}
		else if(Config->BaseClass == PCI_CLASS_NOT_DEFINED && Config->VendorID == 0x8086 && Config->DeviceID == 0x0008)
		{
			PciDebugPrintf(7,"   Device skipped (not enumerated) <PCI_CLASS_NOT_DEFINED 8086:0008>.\n");
			return TRUE;
		}

		if(PCI_CONFIGURATION_TYPE(Config) > PCI_CARDBUS_BRIDGE_TYPE)
		{
			PciDebugPrintf(7,"   Device skipped (not enumerated) <PCI_CONFIGURATION_TYPE > 2>.\n");
			return TRUE;
		}

		return FALSE;
	}

	ASSERTMSG("PCI Skip Function - Operation type unknown.",FALSE);

	return TRUE;
}

//
// is the same device [checked]
//
BOOLEAN PcipIsSameDevice(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config)
{
	if(PdoExt->VendorId != Config->VendorID)
		return FALSE;

	if(PdoExt->DeviceId != Config->DeviceID)
		return FALSE;

	if(PdoExt->RevisionId != Config->RevisionID)
		return FALSE;

	if(PCI_CONFIGURATION_TYPE(Config) != PCI_DEVICE_TYPE)
		return TRUE;

	if(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV)
		return TRUE;

	if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_SUB_IDS))
		return TRUE;

	if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_SUB_IDS2))
		return TRUE;

	if(PdoExt->SubSystemId != Config->u.type0.SubSystemID)
		return FALSE;

	if(PdoExt->SubVendorId != Config->u.type0.SubVendorID)
		return FALSE;

	return TRUE;
}

//
// process bus [checked]
//
VOID PciProcessBus(__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);
	PPCI_PDO_EXTENSION BridgePdoExt						= 0;
	PPCI_PDO_EXTENSION VgaPdoExt						= 0;
	BOOLEAN CheckDevice									= FALSE;

	if(	FdoExt == FdoExt->BusRootFdoExtension ||
		!PdoExt ||
		PCI_DEVICE_TYPE_PCI_TO_PCI != PciClassifyDeviceType(PdoExt) ||
		(!PdoExt->Dependent.type1.IsaBitSet && !PdoExt->Dependent.type1.IsaBitRequired))
	{
		CheckDevice										= TRUE;

		PPCI_PDO_EXTENSION ChildBridgePdoExt			= FdoExt->ChildBridgePdoList;
		while(ChildBridgePdoExt)
		{
			if(ChildBridgePdoExt->Dependent.type1.VgaBitSet)
			{
				BridgePdoExt							= FdoExt->ChildBridgePdoList;
				VgaPdoExt								= ChildBridgePdoExt;
				break;
			}

			ChildBridgePdoExt							= ChildBridgePdoExt->NextBridge;
		}
	}
	else
	{
		BridgePdoExt									= FdoExt->ChildBridgePdoList;
	}

	while(BridgePdoExt)
	{
		if(BridgePdoExt != VgaPdoExt && PCI_DEVICE_TYPE_PCI_TO_PCI == PciClassifyDeviceType(BridgePdoExt))
		{
			if(CheckDevice && BridgePdoExt->Common.DeviceState == PciStarted)
				ASSERT(BridgePdoExt->Dependent.type1.IsaBitRequired || BridgePdoExt->Dependent.type1.IsaBitSet);

			if(BridgePdoExt->Dependent.type1.SubtractiveDecode)
			{
				BridgePdoExt->Dependent.type1.IsaBitRequired	= TRUE;
			}
			else
			{
				BridgePdoExt->Dependent.type1.IsaBitSet	= TRUE;
				BridgePdoExt->UpdateHardware			= TRUE;
			}
		}
		BridgePdoExt									= BridgePdoExt->NextBridge;
	}

	if(PciAssignBusNumbers)
		PciConfigureBusNumbers(FdoExt);
}

//
// scan hibernated bus [checked]
//
VOID PciScanHibernatedBus(__in PPCI_FDO_EXTENSION FdoExt)
{
	PciDebugPrintf(7,"PCI Scan Bus: FDO Extension @ 0x%x, Base Bus = 0x%x\n",FdoExt,FdoExt->BaseBus);

	//
	// check max device override
	//
	ULONG MaxDevice										= PCI_MAX_DEVICES;
	if(FdoExt->BusRootFdoExtension != FdoExt)
	{
		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);
		ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

		if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_ONLY_ONE_DEVICE_ON_BUS))
			MaxDevice									= 1;
	}

	//
	// scan bus
	//
	PCI_COMMON_HEADER DeviceConfig;
	BOOLEAN FoundDevice									= FALSE;
	for(ULONG CurrentDevice = 0; CurrentDevice < MaxDevice; CurrentDevice ++)
	{
		for(ULONG CurrentFunction = 0; CurrentFunction < PCI_MAX_FUNCTION; CurrentFunction ++)
		{
			PCI_SLOT_NUMBER Slot;
			Slot.u.AsULONG								= 0;
			Slot.u.bits.DeviceNumber					= CurrentDevice;
			Slot.u.bits.FunctionNumber					= CurrentFunction;

			//
			// read vendor id
			//
			PciReadSlotConfig(FdoExt,Slot,&DeviceConfig,FIELD_OFFSET(PCI_COMMON_HEADER,VendorID),sizeof(DeviceConfig.VendorID));

			if(DeviceConfig.VendorID != PCI_INVALID_VENDORID && DeviceConfig.VendorID != 0)
			{
				//
				// found a device,then read the whole config header
				//
				PciReadSlotConfig(FdoExt,Slot,&DeviceConfig.DeviceID,FIELD_OFFSET(PCI_COMMON_HEADER,DeviceID),sizeof(DeviceConfig)-sizeof(DeviceConfig.VendorID));

				//
				// apply hacks to the device config buffer
				//
				PciApplyHacks(FdoExt,&DeviceConfig,Slot,0,0);

				USHORT SubSystemId						= 0;
				USHORT SubVendorId						= 0;

				if(PCI_CONFIGURATION_TYPE(&DeviceConfig) == PCI_DEVICE_TYPE && DeviceConfig.BaseClass != PCI_CLASS_BRIDGE_DEV)
				{
					SubSystemId							= DeviceConfig.u.type0.SubSystemID;
					SubVendorId							= DeviceConfig.u.type0.SubVendorID;
				}

				//
				// read hack flags
				//
				ULARGE_INTEGER HackFlags;
				HackFlags.QuadPart						= PciGetHackFlags(DeviceConfig.VendorID,DeviceConfig.DeviceID,SubVendorId,SubSystemId,DeviceConfig.RevisionID);

				//
				// should we skip this function?
				//
				if(!PciSkipThisFunction(&DeviceConfig,Slot,1,HackFlags))
				{
					//
					// this device already exists?
					//
					PPCI_PDO_EXTENSION PdoExt			= PciFindPdoByFunction(FdoExt,Slot,&DeviceConfig);
					if(!PdoExt)
					{
						FoundDevice						= TRUE;

						//
						// disable it
						//
						if(PciCanDisableDecodes(0,&DeviceConfig,HackFlags.LowPart,HackFlags.HighPart,FALSE))
						{
							ClearFlag(DeviceConfig.Command,PCI_ENABLE_BUS_MASTER | PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
							PciWriteSlotConfig(FdoExt,Slot,&DeviceConfig.Command,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(DeviceConfig.Command));
						}
					}
				}

				if(!CurrentFunction && !PCI_MULTIFUNCTION_DEVICE(&DeviceConfig))
					break;
			}
			else
			{
				if(!CurrentFunction)
					break;
			}
		}
	}

	if(FoundDevice)
		IoInvalidateDeviceRelations(FdoExt->PhysicalDeviceObject,BusRelations);
}

//
// query requirements [checked]
//
NTSTATUS PciQueryRequirements(__in PPCI_PDO_EXTENSION PdoExt,__out PIO_RESOURCE_REQUIREMENTS_LIST* IoResRequirementsList)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;
	*IoResRequirementsList								= 0;

	__try
	{
		//
		// no need resource
		//
		if(!PdoExt->Resources && !PdoExt->InterruptPin)
			try_leave(PciDebugPrintf(7,"PciQueryRequirements returning NULL requirements list\n"));

		//
		// read config header
		//
		PCI_COMMON_HEADER Config;
		PciReadDeviceConfig(PdoExt,&Config,0,sizeof(Config));

		//
		// build requirements from header
		//
		PIO_RESOURCE_REQUIREMENTS_LIST IoReqList		= 0;
		Status											= PciBuildRequirementsList(PdoExt,&Config,&IoReqList);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// special check for 0e11:a0f7:11
		//
		if(Config.VendorID == 0x0e11 && Config.DeviceID == 0xa0f7 && Config.RevisionID == 0x11 && ExIsProcessorFeaturePresent(PF_PAE_ENABLED))
		{
			PIO_RESOURCE_DESCRIPTOR Desc				= IoReqList->List->Descriptors;

			for(ULONG i = 0; i < IoReqList->List->Count; i ++,Desc ++)
			{
				if(Desc->Type == CmResourceTypeMemory)
				{
					Desc->Type							= CmResourceTypeNull;
					i									+= 1;
					Desc								+= 1;

					if(i < IoReqList->List->Count && Desc->Type == CmResourceTypeDevicePrivate)
					{
						Desc->Type						= CmResourceTypeNull;
						i								+= 1;
						Desc							+= 1;
					}
				}
			}
		}

		if(IoReqList == PciZeroIoResourceRequirements)
			try_leave(PciDebugPrintf(7,"Returning NULL requirements list\n"));

		*IoResRequirementsList							= IoReqList;

		PciDebugPrintIoResReqList(IoReqList);
	}
	__finally
	{

	}

	return Status;
}

//
// build requirements list [checked]
//
NTSTATUS PciBuildRequirementsList(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__out PIO_RESOURCE_REQUIREMENTS_LIST* IoReqList)
{
	NTSTATUS Status										= STATUS_SUCCESS;
	*IoReqList											= 0;

	__try
	{
		PciDebugPrintf(1,"PciBuildRequirementsList: Bus 0x%x, Dev 0x%x, Func 0x%x.\n",
					   PdoExt->ParentFdoExtension->BaseBus,PdoExt->Slot.u.bits.DeviceNumber,PdoExt->Slot.u.bits.FunctionNumber);

		//
		// total io res descriptor count
		//
		ULONG Count										= 0;

		//
		// pdo's resource count
		//
		ULONG PdoResCount								= PdoExt->Resources ? ARRAYSIZE(PdoExt->Resources->Limit) : 0;

		//
		// get inused resouce list
		//
		CM_PARTIAL_RESOURCE_DESCRIPTOR InusedCmResList[ARRAYSIZE(PdoExt->Resources->Current)];
		if(PdoResCount)
			PciGetInUseRanges(PdoExt,Config,InusedCmResList);

		//
		// count pdo's resource
		//
		for(ULONG i = 0; i < PdoResCount; i++)
		{
			PIO_RESOURCE_DESCRIPTOR IoRes				= PdoExt->Resources->Limit + i;
			if(IoRes->Type == CmResourceTypeNull)
				continue;

			//
			// if this resource is currently in-used,then it has a preferred desc
			//
			if(InusedCmResList[i].Type != CmResourceTypeNull)
			{
				PciDebugPrintf(0x7fffffff,"    Index %d, Preferred = TRUE\n",i);
				Count									+= 1;
			}
			else
			{
				if(IoRes->u.Generic.Length)
				{
					//
					// skip rom
					//
					if(IoRes->Type == CmResourceTypeMemory && IoRes->Flags == CM_RESOURCE_MEMORY_READ_ONLY)
						continue;
				}
				else
				{
					//
					// skip vga port
					//
					if(IoRes->Type == CmResourceTypePort && PdoExt->Dependent.type1.VgaBitSet)
						continue;

					if(IoRes->Type == CmResourceTypeMemory)
					{
						Count							+= 8;
						continue;
					}
				}
			}

			Count										+= 2;
			PciDebugPrintf(0x7fffffff,"    Index %d, Base Resource = TRUE\n",i);
		}

		//
		// get interrupt resource
		//
		UCHAR MinVector,MaxVector;
		BOOLEAN HasInterrupt							= FALSE;
		if(NT_SUCCESS(PciGetInterruptAssignment(PdoExt,&MinVector,&MaxVector)))
		{
			Count										+= 1;
			HasInterrupt								= TRUE;
		}

		//
		// also count additional resource
		//
		Count											+= PdoExt->AdditionalResourceCount;

		PciDebugPrintf(7,"PCI - build resource reqs - baseResourceCount = %d\n",Count);

		//
		// no request requirement
		//
		if(!Count)
		{
			if(!PciZeroIoResourceRequirements)
				PciZeroIoResourceRequirements			= PciAllocateIoRequirementsList(0,0,0);

			if(!PciZeroIoResourceRequirements)
				Status									= STATUS_INSUFFICIENT_RESOURCES;
			else
				*IoReqList								= PciZeroIoResourceRequirements;

			PciDebugPrintf(7,"PCI - build resource reqs - early out, 0 resources\n");

			try_leave(NOTHING);
		}

		//
		// allocate list
		//
		PIO_RESOURCE_REQUIREMENTS_LIST ReqList			= PciAllocateIoRequirementsList(Count,PdoExt->ParentFdoExtension->BaseBus,PdoExt->Slot.u.AsULONG);
		if(!ReqList)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		PIO_RESOURCE_DESCRIPTOR IoRes					= ReqList->List->Descriptors;

		//
		// set pdo resource
		//
		for(ULONG i = 0; i < PdoResCount; i ++)
		{
			PIO_RESOURCE_DESCRIPTOR PdoRes				= PdoExt->Resources->Limit + i;
			PCM_PARTIAL_RESOURCE_DESCRIPTOR CmRes		= InusedCmResList + i;

			if(PdoRes->Type == CmResourceTypeNull)
				continue;

			ULONG Length								= PdoRes->u.Generic.Length;
			ULONG Alignment								= PdoRes->u.Generic.Alignment;
			BOOLEAN AppendPreferred						= FALSE;
			PCHAR PreferredString						= "";

			if(CmRes->Type != CmResourceTypeNull)
			{
				//
				// append a preferred desc
				//
				Length									= CmRes->u.Generic.Length;
				AppendPreferred							= TRUE;
			}
			else
			{
				AppendPreferred							= FALSE;
				PreferredString							= "not ";

				if(PdoRes->u.Generic.Length)
				{
					//
					// skip rom
					//
					if(PdoRes->Type == CmResourceTypeMemory && PdoRes->Flags == CM_RESOURCE_MEMORY_READ_ONLY)
						continue;
				}
				else
				{
					if(PdoRes->Type == CmResourceTypeMemory || (PdoRes->Type == CmResourceTypePort && !PdoExt->Dependent.type1.VgaBitSet))
					{
						ULONG DeviceType				= PciClassifyDeviceType(PdoExt);

						if(DeviceType == PCI_DEVICE_TYPE_PCI_TO_PCI || DeviceType == PCI_DEVICE_TYPE_CARDBUS)
						{
							if(PdoRes->Type != CmResourceTypeMemory)
							{
								Length					= DeviceType == PCI_DEVICE_TYPE_CARDBUS ? 0x100 : 0x1000;
								Alignment				= Length;
							}
							else
							{
								PciBuildGraduatedWindow(PdoRes,0x4000000,7,IoRes);
								IoRes					+= 7;
								PciPrivateResourceInitialize(IoRes,1,i);
								IoRes					+= 1;

								continue;
							}
						}
						else
						{
							continue;
						}
					}
					else
					{
						continue;
					}
				}
			}

			PciDebugPrintf(0x7fffffff,"    Index %d, Setting Base Resource,%ssetting preferred.\n",i,PreferredString);

			*IoRes										= *PdoRes;
			IoRes->u.Generic.Length						= Length;
			IoRes->u.Generic.Alignment					= Alignment;
			IoRes->ShareDisposition						= CmResourceShareDeviceExclusive;
			SetFlag(IoRes->Flags,PdoRes->Type == CmResourceTypePort ? CM_RESOURCE_PORT_16_BIT_DECODE | CM_RESOURCE_PORT_POSITIVE_DECODE : 0);

			if(AppendPreferred)
			{
				PciDebugPrintf(3,"  Duplicating for preferred locn.\n");

				*(IoRes + 1)							= *IoRes;

				IoRes->Option							= IO_RESOURCE_PREFERRED;
				IoRes->u.Generic.MinimumAddress			= CmRes->u.Generic.Start;
				IoRes->u.Generic.MaximumAddress.QuadPart= CmRes->u.Generic.Start.QuadPart + CmRes->u.Generic.Length - 1;
				IoRes->u.Generic.Alignment				= 1;

				if( PciLockDeviceResources || PdoExt->LegacyDriver || PdoExt->OnDebugPath || FlagOn(PdoExt->ParentFdoExtension->BusHackFlags,PCI_BUS_HACK_LOCK_RES) ||
					(PdoExt->VendorId == 0x11c1 && PdoExt->DeviceId == 0x441 && PdoExt->SubVendorId == 0x1179 && (PdoExt->SubSystemId == 1 || PdoExt->SubSystemId == 2)))
				{
					*(IoRes + 1)						= *IoRes;
				}

				IoRes									+= 1;
				IoRes->Option							= IO_RESOURCE_ALTERNATIVE;
			}

			//
			// add a device private desc
			//
			PciPrivateResourceInitialize(IoRes + 1,1,i);

			//
			// 2 means base and device private
			//
			IoRes										+= 2;
		}

		//
		// interrupt resource
		//
		if(HasInterrupt)
		{
			PciDebugPrintf(3,"  Assigning INT descriptor\n");

			IoRes->Type									= CmResourceTypeInterrupt;
			IoRes->ShareDisposition						= CmResourceShareShared;
			IoRes->u.Interrupt.MaximumVector			= MaxVector;
			IoRes->u.Interrupt.MinimumVector			= MinVector;
			IoRes->Flags								= 0;
			IoRes->Option								= 0;

			IoRes										+= 1;
		}

		//
		// additional resource
		//
		if(PdoExt->AdditionalResourceCount)
		{
			PciConfigurators[PdoExt->HeaderType].GetAdditionalResourceDescriptors(PdoExt,Config,IoRes);
			IoRes										+= PdoExt->AdditionalResourceCount;
		}

		ULONG_PTR FinalCount							= IoRes - ReqList->List->Descriptors;
		ASSERT(ReqList->ListSize == reinterpret_cast<PUCHAR>(IoRes) - reinterpret_cast<PUCHAR>(ReqList));

		PciDebugPrintf(7,"PCI build resource req - final resource count == %d\n",FinalCount);
		ASSERT(FinalCount);

		*IoReqList										= ReqList;
	}
	__finally
	{

	}

	return Status;
}

//
// query current resource [checked]
//
NTSTATUS PciQueryResources(__in PPCI_PDO_EXTENSION PdoExt,__out PCM_RESOURCE_LIST* CmResList)
{
	PAGED_CODE();

	*CmResList											= 0;

	if(!PdoExt->Resources)
		return STATUS_SUCCESS;

	//
	// check which spaces is enabled
	//
	USHORT Command										= 0;
	PciReadDeviceConfig(PdoExt,&Command,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(Command));

	BOOLEAN IoEnabled									= BooleanFlagOn(Command,PCI_ENABLE_IO_SPACE);
	BOOLEAN MemoryEnabled								= BooleanFlagOn(Command,PCI_ENABLE_MEMORY_SPACE);
	BOOLEAN HasInterrupt								= PdoExt->InterruptPin && PdoExt->AdjustedInterruptLine && PdoExt->AdjustedInterruptLine != 0xff;

	//
	// compute total count
	//
	ULONG Count											= 0;
	for(ULONG i = 0; i < ARRAYSIZE(PdoExt->Resources->Current); i ++)
	{
		if(IoEnabled && PdoExt->Resources->Current[i].Type == CmResourceTypePort)
			Count										+= 1;

		if(MemoryEnabled && PdoExt->Resources->Current[i].Type == CmResourceTypeMemory)
			Count										+= 1;
	}

	//
	// check interrupt
	//
	if((IoEnabled || MemoryEnabled) && HasInterrupt)
		Count											+= 1;

	//
	// check vga bit in pci-to-pci bridge
	//
	BOOLEAN VgaEnabled									= FALSE;
	if(PdoExt->HeaderType == PCI_BRIDGE_TYPE)
	{
		USHORT BridgeControl							= 0;
		PciReadDeviceConfig(PdoExt,&BridgeControl,FIELD_OFFSET(PCI_COMMON_HEADER,u.type1.BridgeControl),sizeof(BridgeControl));

		if(FlagOn(BridgeControl,8))
		{
			if(MemoryEnabled)
				Count									+= 1;

			if(IoEnabled)
				Count									+= 2;

			VgaEnabled									= TRUE;
		}
	}

	//
	// no resource is in-use
	//
	if(!Count)
		return STATUS_SUCCESS;

	//
	// allocate
	//
	PCM_RESOURCE_LIST Res								= PciAllocateCmResourceList(Count,PdoExt->ParentFdoExtension->BaseBus);
	if(!Res)
		return STATUS_INSUFFICIENT_RESOURCES;

	//
	// setup it
	//
	PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDesc			= Res->List[0].PartialResourceList.PartialDescriptors;
	for(ULONG i = 0; i < ARRAYSIZE(PdoExt->Resources->Current); i ++)
	{
		if(IoEnabled && PdoExt->Resources->Current[i].Type == CmResourceTypePort)
			*PartialDesc++								= PdoExt->Resources->Current[i];

		if(MemoryEnabled && PdoExt->Resources->Current[i].Type == CmResourceTypeMemory)
			*PartialDesc++								= PdoExt->Resources->Current[i];
	}

	//
	// vga resource
	//
	if(VgaEnabled)
	{
		if(MemoryEnabled)
		{
			PartialDesc->u.Memory.Length				= 0x20000;
			PartialDesc->u.Memory.Start.QuadPart		= 0xa0000;
			PartialDesc->Type							= CmResourceTypeMemory;
			PartialDesc->Flags							= 0;

			PartialDesc									+= 1;
		}

		if(IoEnabled)
		{
			PartialDesc->u.Port.Length					= 0x0c;
			PartialDesc->u.Port.Start.QuadPart			= 0x3b0;
			PartialDesc->Type							= CmResourceTypePort;
			PartialDesc->Flags							= CM_RESOURCE_PORT_POSITIVE_DECODE | CM_RESOURCE_PORT_10_BIT_DECODE;

			PartialDesc									+= 1;

			PartialDesc->u.Port.Length					= 0x20;
			PartialDesc->u.Port.Start.QuadPart			= 0x3c0;
			PartialDesc->Type							= CmResourceTypePort;
			PartialDesc->Flags							= CM_RESOURCE_PORT_POSITIVE_DECODE | CM_RESOURCE_PORT_10_BIT_DECODE;

			PartialDesc									+= 1;
		}
	}

	//
	// interrupt resource
	//
	if((IoEnabled || MemoryEnabled) && HasInterrupt)
	{
		PartialDesc->u.Interrupt.Affinity				= 0xffffffff;
		PartialDesc->u.Interrupt.Level					= PdoExt->AdjustedInterruptLine;
		PartialDesc->u.Interrupt.Vector					= PdoExt->AdjustedInterruptLine;
		PartialDesc->Type								= CmResourceTypeInterrupt;
		PartialDesc->ShareDisposition					= CmResourceShareShared;
	}

	*CmResList											= Res;

	return STATUS_SUCCESS;
}

//
// get enhanced capabilites [checked]
//
VOID PciGetEnhancedCapabilities(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	BOOLEAN NoPmeCaps									= TRUE;
	__try
	{
		PdoExt->PowerState.DeviceWakeLevel				= PowerDeviceUnspecified;

		//
		// device does not have caps list
		//
		if(!FlagOn(Config->Status,PCI_STATUS_CAPABILITIES_LIST))
			try_leave(PdoExt->CapabilitiesPtr = 0);

		UCHAR CapsPtr									= 0;
		switch(PCI_CONFIGURATION_TYPE(Config))
		{
		case PCI_DEVICE_TYPE:
			CapsPtr										= Config->u.type0.CapabilitiesPtr;
			break;

		case PCI_BRIDGE_TYPE:
			CapsPtr										= Config->u.type1.CapabilitiesPtr;
			break;

		case PCI_CARDBUS_BRIDGE_TYPE:
			CapsPtr										= Config->u.type2.CapabilitiesPtr;
			break;

		default:
			break;
		}

		//
		// save it in pdo ext
		//
		if(CapsPtr)
		{
			ASSERT(!FlagOn(CapsPtr,3) && CapsPtr >= sizeof(PCI_COMMON_HEADER));

			if(!FlagOn(CapsPtr,3) && CapsPtr >= sizeof(PCI_COMMON_HEADER))
				PdoExt->CapabilitiesPtr						= CapsPtr;
		}

		//
		// find vga caps
		//
		if(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && (PdoExt->SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI || PdoExt->SubClass == PCI_SUBCLASS_BR_HOST))
		{
			UCHAR CapsId								= PdoExt->SubClass == PCI_SUBCLASS_BR_HOST ? PCI_CAPABILITY_ID_AGP : PCI_CAPABILITY_ID_AGP_TARGET;
		
			PCI_CAPABILITIES_HEADER Caps;
			if(PciReadDeviceCapability(PdoExt,PdoExt->CapabilitiesPtr,CapsId,&Caps,sizeof(Caps)))
				PdoExt->TargetAgpCapabilityId			= CapsId;
		}

		//
		// find power caps
		//
		if(FlagOn(PdoExt->HackFlags.u.LowPart,PCI_HACK_FLAGS_LOW_NO_PME_CAPS))
			try_leave(NOTHING);

		PCI_PM_CAPABILITY Caps;
		UCHAR Offset									= PciReadDeviceCapability(PdoExt,PdoExt->CapabilitiesPtr,PCI_CAPABILITY_ID_POWER_MANAGEMENT,&Caps,sizeof(Caps));
		if(!Offset)
			try_leave(NOTHING);

		if(Caps.PMC.Capabilities.Support.PMED0)
			PdoExt->PowerState.DeviceWakeLevel			= PowerDeviceD0;

		if(Caps.PMC.Capabilities.Support.PMED1)
			PdoExt->PowerState.DeviceWakeLevel			= PowerDeviceD1;

		if(Caps.PMC.Capabilities.Support.PMED2)
			PdoExt->PowerState.DeviceWakeLevel			= PowerDeviceD2;

		if(Caps.PMC.Capabilities.Support.PMED3Hot)
			PdoExt->PowerState.DeviceWakeLevel			= PowerDeviceD3;

		if(Caps.PMC.Capabilities.Support.PMED3Cold)
			PdoExt->PowerState.DeviceWakeLevel			= PowerDeviceD3;

		PdoExt->PowerState.CurrentDeviceState			= static_cast<DEVICE_POWER_STATE>(Caps.PMCSR.ControlStatus.PowerState + 1);
		PdoExt->PowerCapabilities						= Caps.PMC.Capabilities;
		NoPmeCaps										= FALSE;
	}
	__finally
	{
		if(NoPmeCaps)
			SetFlag(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_PME_CAPS);

		if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_PME_CAPS))
		{
			if(FlagOn(Config->Command,PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER))
				PdoExt->PowerState.CurrentDeviceState	=  PowerDeviceD0;
			else
				PdoExt->PowerState.CurrentDeviceState	=  PowerDeviceD3;
		}
	}
}

//
// get function limit [checked]
//
NTSTATUS PciGetFunctionLimits(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in ULARGE_INTEGER HackFlags)
{
	PAGED_CODE();

	if(PciSkipThisFunction(Config,PdoExt->Slot,2,HackFlags))
		return STATUS_SUCCESS;

	PCI_CONFIGURATOR_PARAM Param;
	Param.PdoExt										= PdoExt;
	Param.OriginalConfig								= Config;
	Param.Configurator									= PciConfigurators + PdoExt->HeaderType;
	Param.Working										= static_cast<PPCI_COMMON_HEADER>(ExAllocatePoolWithTag(NonPagedPool,sizeof(PCI_COMMON_HEADER) * 2,'BicP'));
	if(!Param.Working)
		return STATUS_INSUFFICIENT_RESOURCES;

	NTSTATUS Status										= PcipGetFunctionLimits(&Param);
	ExFreePool(Param.Working);

	return Status;
}

//
// get function limit [checked]
//
NTSTATUS PcipGetFunctionLimits(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	Param->SavedCommand									= Param->OriginalConfig->Command;
	Param->SavedStatus									= Param->OriginalConfig->Status;
	Param->OriginalConfig->Status						= 0;
	ClearFlag(Param->OriginalConfig->Command,PCI_ENABLE_BUS_MASTER | PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);

	RtlCopyMemory(Param->Working,Param->OriginalConfig,sizeof(PCI_COMMON_HEADER));

	Param->Configurator->MassageHeaderForLimitsDetermination(Param);

	PciWriteLimitsAndRestoreCurrent(Param);

	ASSERT(!Param->PdoExt->Resources);

	ULONG Length										= sizeof(PCI_FUNCTION_RESOURCES);
	Param->PdoExt->Resources							= static_cast<PPCI_FUNCTION_RESOURCES>(ExAllocatePoolWithTag(NonPagedPool,Length,'BicP'));
	if(!Param->PdoExt->Resources)
		return STATUS_INSUFFICIENT_RESOURCES;

	RtlZeroMemory(Param->PdoExt->Resources,Length);

	Param->Configurator->SaveLimits(Param);

	Param->Configurator->SaveCurrentSettings(Param);

	for(ULONG i = 0; i < ARRAYSIZE(Param->PdoExt->Resources->Limit); i ++)
	{
		if(Param->PdoExt->Resources->Limit[i].Type != CmResourceTypeNull)
			return STATUS_SUCCESS;
	}

	ExFreePool(Param->PdoExt->Resources);
	Param->PdoExt->Resources							= 0;

	return STATUS_SUCCESS;
}

//
// write limit and restore current [checked]
//
VOID PciWriteLimitsAndRestoreCurrent(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	if(Param->PdoExt->OnDebugPath)
	{
		if(FlagOn(Param->SavedCommand,PCI_ENABLE_BUS_MASTER))
		{
			SetFlag(Param->OriginalConfig->Command,PCI_ENABLE_BUS_MASTER);
			SetFlag(Param->Working[0].Command,PCI_ENABLE_BUS_MASTER);
		}

		KdDisableDebugger();
	}

	PciWriteDeviceConfig(Param->PdoExt,Param->Working,0,sizeof(PCI_COMMON_HEADER));

	PciReadDeviceConfig(Param->PdoExt,Param->Working,0,sizeof(PCI_COMMON_HEADER));

	PciWriteDeviceConfig(Param->PdoExt,Param->OriginalConfig,0,sizeof(PCI_COMMON_HEADER));

	Param->OriginalConfig->Command						= Param->SavedCommand;
	if(Param->SavedCommand)
		PciWriteDeviceConfig(Param->PdoExt,&Param->SavedCommand,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(Param->SavedCommand));

	Param->OriginalConfig->Status						= Param->SavedStatus;

	Param->Configurator->RestoreCurrent(Param);

	if(Param->PdoExt->OnDebugPath)
		KdEnableDebugger();

	if(!Param->PdoExt->ExpectedWritebackFailure)
	{
		PciReadDeviceConfig(Param->PdoExt,Param->Working + 1,0,sizeof(PCI_COMMON_HEADER));

		ULONG Offset									= RtlCompareMemory(Param->Working + 1,Param->OriginalConfig,sizeof(PCI_COMMON_HEADER));

		if(Offset != sizeof(PCI_COMMON_HEADER))
		{
			PciDebugPrintf(1,"PCI - CFG space write verify failed at offset 0x%x\n",Offset);
			PciDebugDumpCommonConfig(Param->Working + 1);
		}
	}
}

//
// compute new resource settings [checked]
//
BOOLEAN PciComputeNewCurrentSettings(__in PPCI_PDO_EXTENSION PdoExt,__in PCM_RESOURCE_LIST CmResList)
{
	PAGED_CODE();

	ASSERT(!CmResList || CmResList->Count == 1);

	BOOLEAN Ret											= FALSE;
	__try
	{
		if(!CmResList || !CmResList->Count)
			try_leave(Ret = PdoExt->UpdateHardware);

		PciDebugPrintCmResList(0x80000000,CmResList);

		//
		// build new resource descriptors
		//
		CM_PARTIAL_RESOURCE_DESCRIPTOR New[ARRAYSIZE(PdoExt->Resources->Current)];
		for(ULONG i = 0; i < ARRAYSIZE(New); i ++)
			New[i].Type									= CmResourceTypeNull;

		PCM_PARTIAL_RESOURCE_DESCRIPTOR InterruptRes	= 0;
		CM_PARTIAL_RESOURCE_DESCRIPTOR Temp				= {0};
		PCM_FULL_RESOURCE_DESCRIPTOR FullDesc			= CmResList->List;

		for(ULONG i = 0; i < CmResList->Count; i ++)
		{
			PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDesc	= FullDesc->PartialResourceList.PartialDescriptors;

			PCM_PARTIAL_RESOURCE_DESCRIPTOR BaseRes		= 0;
			ULONG Skip									= 0;
			for(ULONG j = 0; j < FullDesc->PartialResourceList.Count; j ++,PartialDesc = PciNextPartialDescriptor(PartialDesc))
			{
				if(Skip)
				{
					Skip								-= 1;
					continue;
				}

				switch(PartialDesc->Type)
				{
				case CmResourceTypePort:
				case CmResourceTypeMemory:
					{
						ASSERT(!BaseRes);
						BaseRes							= PartialDesc;
					}
					break;

				case CmResourceTypeInterrupt:
					{
						ASSERT(!InterruptRes);
						ASSERT(PartialDesc->u.Interrupt.Level == PartialDesc->u.Interrupt.Vector);
						ASSERT(!FlagOn(PartialDesc->u.Interrupt.Level,0xffffff00));
						PdoExt->AdjustedInterruptLine	= static_cast<UCHAR>(PartialDesc->u.Interrupt.Level & 0xff);
						InterruptRes					= PartialDesc;
					}
					break;

				case CmResourceTypeDevicePrivate:
					{
						switch(PartialDesc->u.DevicePrivate.Data[0])
						{
						case 2:
							{
								ASSERT(BaseRes);
								Temp					= *BaseRes;

								ASSERT(Temp.Type == CmResourceTypePort && Temp.u.Generic.Length == 0x100);
								ASSERT(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && PdoExt->Dependent.type1.IsaBitSet);

								ULONG Length			= PartialDesc->u.Generic.Length;
								Temp.u.Generic.Length	= Length;

								Skip					= (Length >> 10) - 1;

								PCM_PARTIAL_RESOURCE_DESCRIPTOR	LastOne = BaseRes + (Length >> 10);
							
								ASSERT(LastOne->Type == CmResourceTypePort);
								ASSERT(LastOne->u.Generic.Length == 0x100);
								ASSERT(LastOne->u.Generic.Start.QuadPart == Temp.u.Generic.Start.QuadPart + Temp.u.Generic.Length - 0x400);

								BaseRes					= &Temp;
							}
							//
							// fall through
							//

						case 1:
							{
								ASSERT(BaseRes);
								ULONG Index				= PartialDesc->u.DevicePrivate.Data[1];
								New[Index]				= *BaseRes;
								BaseRes					= 0;
							}
							break;

						case 3:
							{
								ASSERT(BaseRes);
								Skip					= PartialDesc->u.DevicePrivate.Data[1];
							}
							break;
						}
					}
					break;
				}
			}

			ASSERT(!BaseRes);
			FullDesc									= reinterpret_cast<PCM_FULL_RESOURCE_DESCRIPTOR>(PartialDesc);
		}

		//
		// hardware does not need resources
		//
		if(!PdoExt->Resources)
			try_leave(Ret = FALSE);

		//
		// compare old and new,and copy new to old if there is any diff between them
		//
		BOOLEAN Changed									= FALSE;
		for(ULONG i = 0; i < ARRAYSIZE(PdoExt->Resources->Current); i ++)
		{
			PCM_PARTIAL_RESOURCE_DESCRIPTOR Old			= PdoExt->Resources->Current + i;
			if( Old->Type != New[i].Type ||
				(Old->Type != CmResourceTypeNull && (Old->u.Generic.Length != New[i].u.Generic.Length || Old->u.Generic.Start.QuadPart != New[i].u.Generic.Start.QuadPart)))
			{
				Changed									= TRUE;

				if(Old->Type != CmResourceTypeNull)
				{
					PciDebugPrintf(0x4000000,"      Old range-\n");
					PciDebugPrintPartialResource(0x4000000,Old);
				}
				else
				{
					PciDebugPrintf(0x4000000,"      Previously unset range\n\n");
				}

				PciDebugPrintf(0x4000000,"      changed to\n");
				PciDebugPrintPartialResource(0x4000000,New + i);

				Old->Type								= New[i].Type;
				Old->u.Generic.Start.QuadPart			= New[i].u.Generic.Start.QuadPart;
				Old->u.Generic.Length					= New[i].u.Generic.Length;
			}
		}

		Ret												= Changed ? TRUE : PdoExt->UpdateHardware;
	}
	__finally
	{

	}

	return Ret;
}

//
// set resource [checked]
//
NTSTATUS PciSetResources(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN ResetDevice,__in BOOLEAN AssignResource)
{
	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		//
		// check changed device?
		//
		PCI_COMMON_HEADER Config;
		PciReadDeviceConfig(PdoExt,&Config,0,sizeof(Config));
		if(!PcipIsSameDevice(PdoExt,&Config))
			try_leave(Status = STATUS_DEVICE_DOES_NOT_EXIST;ASSERTMSG("PCI Set resources - not same device",0));

		//
		// skip host bridge
		//
		if(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && PdoExt->SubClass == PCI_SUBCLASS_BR_HOST)
			try_leave(Status = STATUS_SUCCESS);

		//
		// configure ide controller
		//
		if(ResetDevice && PdoExt->BaseClass == PCI_CLASS_MASS_STORAGE_CTLR && PdoExt->SubClass == PCI_SUBCLASS_MSC_IDE_CTLR)
		{
			BOOLEAN Switched							= PciConfigureIdeController(PdoExt,&Config,FALSE);
			ASSERT(Switched == PdoExt->SwitchedIDEToNativeMode);
		}

		//
		// update hotplug info
		//
		if(PdoExt->NeedsHotPlugConfiguration && PdoExt->ParentFdoExtension->HotPlugParameters.Acquired)
		{
			PPCI_HOTPLUG_PARAMETERS HotPlugParameters	= &PdoExt->ParentFdoExtension->HotPlugParameters;
			PciDebugPrintf(0x8000,"PCI - SetResources, PDOx %x current CacheLineSize is %x, Want %x\n",PdoExt,Config.CacheLineSize,HotPlugParameters->CacheLineSize);

			PdoExt->SavedLatencyTimer					= HotPlugParameters->LatencyTimer;

			//
			// set cache line size
			//
			PciWriteDeviceConfig(PdoExt,&HotPlugParameters->CacheLineSize,FIELD_OFFSET(PCI_COMMON_HEADER,CacheLineSize),sizeof(HotPlugParameters->CacheLineSize));

			//
			// read back
			//
			UCHAR ReadCacheLineSize;
			PciReadDeviceConfig(PdoExt,&ReadCacheLineSize,FIELD_OFFSET(PCI_COMMON_HEADER,CacheLineSize),sizeof(ReadCacheLineSize));

			USHORT Command								= 0;
			if(ReadCacheLineSize != HotPlugParameters->CacheLineSize || !ReadCacheLineSize)
			{
				PciDebugPrintf(0x8000,"PCI - SetResources, PDOx %x cache line size non-sticky\n",PdoExt);
			}
			else
			{
				PciDebugPrintf(0x8000,"PCI - SetResources, PDOx %x cache line size stuck, set MWI\n",PdoExt);
				PdoExt->SavedCacheLineSize				= HotPlugParameters->CacheLineSize;
				SetFlag(Command,PCI_ENABLE_WRITE_AND_INVALIDATE);
			}

			if(HotPlugParameters->EnableSERR)
				SetFlag(Command,PCI_ENABLE_SERR);

			if(HotPlugParameters->EnablePERR)
				SetFlag(Command,PCI_ENABLE_PARITY);

			SetFlag(PdoExt->CommandEnables,Command);
		}

		PciInvalidateResourceInfoCache(PdoExt);

		//
		// call configurators to change resource settings and reset device
		//
		ULONG HeadType									= PdoExt->HeaderType;
		PciConfigurators[HeadType].ChangeResourceSettings(PdoExt,&Config);
		PdoExt->UpdateHardware							= FALSE;

		if(ResetDevice)
		{
			PciConfigurators[HeadType].ResetDevice(PdoExt,&Config);
			Config.u.type0.InterruptLine				= PdoExt->RawInterruptLine;
		}

		if(PdoExt->SavedLatencyTimer != Config.LatencyTimer)
			PciDebugPrintf(0x80000,"PCI (pdox %08x) changing latency from %02x to %02x.\n",Config.LatencyTimer,PdoExt->SavedLatencyTimer);

		if(PdoExt->SavedCacheLineSize != Config.CacheLineSize)
			PciDebugPrintf(0x80000,"PCI (pdox %08x) changing cache line size from %02x to %02x.\n",Config.CacheLineSize,PdoExt->SavedCacheLineSize);

		Config.CacheLineSize							= PdoExt->SavedCacheLineSize;
		Config.LatencyTimer								= PdoExt->SavedLatencyTimer;
		Config.u.type0.InterruptLine					= PdoExt->RawInterruptLine;
		Config.Command									= PdoExt->CommandEnables;

		if(!FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_DONOT_TOUCH_COMMAND))
			ClearFlag(Config.Command,PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_IO_SPACE | PCI_ENABLE_BUS_MASTER | PCI_ENABLE_WRITE_AND_INVALIDATE);

		Config.Status									= 0;

		//
		// apply hack
		//
		PciApplyHacks(PdoExt->ParentFdoExtension,&Config,PdoExt->Slot,3,PdoExt);

		//
		// write config
		//
		PciWriteDeviceConfig(PdoExt,&Config,0,sizeof(Config));

		PdoExt->RawInterruptLine						= Config.u.type0.InterruptLine;

		//
		// enable decode
		//
		PciDecodeEnable(PdoExt,TRUE,&PdoExt->CommandEnables);

		PdoExt->NeedsHotPlugConfiguration				= FALSE;

		Status											= STATUS_SUCCESS;
	}
	__finally
	{

	}

	return Status;
}

//
// configure ide controller [checked]
//
BOOLEAN PciConfigureIdeController(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in BOOLEAN DisableIoDecode)
{
	PCI_NATIVE_IDE_CONTROLLER_PROGIF ProgIf;
	ProgIf.ProgIf										= Config->ProgIf;

	if(ProgIf.PrimaryState != ProgIf.SecondaryState || ProgIf.PrimarySwitchable != ProgIf.SecondarySwitchable)
	{
		DbgPrint("PCI: Warning unsupported IDE controller configuration for VEN_%04x&DEV_%04x!\n",Config->VendorID,Config->DeviceID);
		return FALSE;
	}

	if(ProgIf.PrimaryState && ProgIf.SecondaryState && (DisableIoDecode || PdoExt->IoSpaceUnderNativeIdeControl))
	{
		USHORT Command									= 0;
		PciReadDeviceConfig(PdoExt,&Command,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(Command));

		ClearFlag(Command,PCI_ENABLE_IO_SPACE);

		PciWriteDeviceConfig(PdoExt,&Command,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(Command));

		Config->Command									= Command;

		return FALSE;
	}

	if( ProgIf.PrimarySwitchable && 
		ProgIf.SecondarySwitchable && 
		PdoExt->BIOSAllowsIDESwitchToNativeMode && 
		!FlagOn(PdoExt->HackFlags.HighPart,PCI_HACK_FLAGS_HIGH_DISABLE_NATIVE_IDE))
	{
		PciDecodeEnable(PdoExt,FALSE,0);

		PciReadDeviceConfig(PdoExt,&Config->Command,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(Config->Command));

		SetNativeModeIde(ProgIf.ProgIf);

		PciWriteDeviceConfig(PdoExt,&ProgIf,FIELD_OFFSET(PCI_COMMON_HEADER,ProgIf),sizeof(ProgIf));

		UCHAR ReadBack;
		PciReadDeviceConfig(PdoExt,&ReadBack,FIELD_OFFSET(PCI_COMMON_HEADER,ProgIf),sizeof(ReadBack));

		if(ReadBack != ProgIf.ProgIf)
		{
			DbgPrint("PCI: Warning failed switch to native mode for IDE controller VEN_%04x&DEV_%04x!\n",Config->VendorID,Config->DeviceID);
			return FALSE;
		}

		Config->ProgIf									= ProgIf.ProgIf;
		PdoExt->ProgIf									= ProgIf.ProgIf;

		RtlZeroMemory(Config->u.type0.BaseAddresses,sizeof(Config->u.type0.BaseAddresses[0]) * 4);

		PciWriteDeviceConfig(PdoExt,Config->u.type0.BaseAddresses,FIELD_OFFSET(PCI_COMMON_HEADER,u.type0.BaseAddresses),sizeof(Config->u.type0.BaseAddresses[0]) * 4);

		PciReadDeviceConfig(PdoExt,Config->u.type0.BaseAddresses,FIELD_OFFSET(PCI_COMMON_HEADER,u.type0.BaseAddresses),sizeof(Config->u.type0.BaseAddresses[0]) * 4);

		PciReadDeviceConfig(PdoExt,&Config->u.type0.InterruptPin,FIELD_OFFSET(PCI_COMMON_HEADER,u.type0.InterruptPin),sizeof(Config->u.type0.InterruptPin));

		return TRUE;
	}

	return FALSE;
}

//
// allocate cm resource list [checked]
//
PCM_RESOURCE_LIST PciAllocateCmResourceList(__in ULONG Count,__in UCHAR BusNumber)
{
	PAGED_CODE();

	ULONG Length										= sizeof(CM_RESOURCE_LIST) + (Count ? sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * (Count - 1) : 0);
	PCM_RESOURCE_LIST CmResList							= static_cast<PCM_RESOURCE_LIST>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
	if(!CmResList)
		return 0;

	RtlZeroMemory(CmResList,Length);

	CmResList->Count									= 1;
	CmResList->List->BusNumber							= BusNumber;
	CmResList->List->InterfaceType						= PCIBus;
	CmResList->List->PartialResourceList.Count			= Count;
	CmResList->List->PartialResourceList.Revision		= 1;
	CmResList->List->PartialResourceList.Version		= 1;

	return CmResList;
}

//
// allocate io requirements resource list [checked]
//
PIO_RESOURCE_REQUIREMENTS_LIST PciAllocateIoRequirementsList(__in ULONG Count,__in UCHAR BusNumber,__in ULONG Slot)
{
	ULONG Length										= sizeof(IO_RESOURCE_REQUIREMENTS_LIST) + (Count ? sizeof(IO_RESOURCE_DESCRIPTOR) * (Count - 1) : 0);
	PIO_RESOURCE_REQUIREMENTS_LIST IoReqList			= static_cast<PIO_RESOURCE_REQUIREMENTS_LIST>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
	if(!IoReqList)
		return 0;

	RtlZeroMemory(IoReqList,Length);

	IoReqList->AlternativeLists							= 1;
	IoReqList->BusNumber								= BusNumber;
	IoReqList->InterfaceType							= PCIBus;
	IoReqList->SlotNumber								= Slot;
	IoReqList->ListSize									= Length;
	IoReqList->List->Count								= Count;
	IoReqList->List->Version							= 1;
	IoReqList->List->Revision							= 1;

	return IoReqList;
}

//
// in-use range [checked]
//
VOID PciGetInUseRanges(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc)
{
	PAGED_CODE();

	BOOLEAN IoEnabled									= FlagOn(Config->Command,PCI_ENABLE_IO_SPACE) || FlagOn(PdoExt->InitialCommand,PCI_ENABLE_IO_SPACE);
	BOOLEAN MemoryEnabled								= FlagOn(Config->Command,PCI_ENABLE_MEMORY_SPACE) || FlagOn(PdoExt->InitialCommand,PCI_ENABLE_MEMORY_SPACE);

	PIO_RESOURCE_DESCRIPTOR IoRes						= PdoExt->Resources->Limit;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR CmRes				= PdoExt->Resources->Current;

	for(ULONG i= 0; i < ARRAYSIZE(PdoExt->Resources->Current); i ++,Desc ++,IoRes ++,CmRes ++)
	{
		Desc->Type										= CmResourceTypeNull;

		if(IoRes->Type == CmResourceTypeNull)
			continue;

		if((IoEnabled && CmRes->Type == CmResourceTypePort) || (MemoryEnabled && CmRes->Type == CmResourceTypeMemory))
		{
			if(!CmRes->u.Generic.Length)
				continue;

			if(CmRes->u.Generic.Start.QuadPart || (PCI_CONFIGURATION_TYPE(Config) == PCI_BRIDGE_TYPE && CmRes->Type == CmResourceTypePort))
				*Desc									= *CmRes;
		}
	}
}

//
// build graduate window [checked]
//
VOID PciBuildGraduatedWindow(__in PIO_RESOURCE_DESCRIPTOR PrototypeDesc,__in ULONG Window,__in ULONG Count,__in PIO_RESOURCE_DESCRIPTOR OutputDesc)
{
	PAGED_CODE();

	ASSERT(PrototypeDesc->Type == CmResourceTypeMemory || PrototypeDesc->Type == CmResourceTypePort);

	for(ULONG i = 0; i < Count; i ++)
	{
		OutputDesc[i]									= *PrototypeDesc;
		OutputDesc[i].u.Generic.Length					= Window;

		if(i)
			OutputDesc[i].Option						= IO_RESOURCE_ALTERNATIVE;

		Window											>>= 1;
		ASSERT(Window > 1);
	}
}

//
// init private desc [checked]
//
VOID PciPrivateResourceInitialize(__in PIO_RESOURCE_DESCRIPTOR Desc,__in ULONG Type,__in ULONG Index)
{
	PAGED_CODE();

	Desc->Type											= CmResourceTypeDevicePrivate;
	Desc->Flags											= 0;
	Desc->ShareDisposition								= CmResourceShareDeviceExclusive;
	Desc->Option										= 0;
	Desc->u.DevicePrivate.Data[0]						= Type;
	Desc->u.DevicePrivate.Data[1]						= Index;
}