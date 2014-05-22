//********************************************************************
//	created:	23:7:2008   21:05
//	file:		pci.pdo.cpp
//	author:		tiamo
//	purpose:	pdo
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciPdoCreate)
#pragma alloc_text("PAGE",PciPdoDestroy)
#pragma alloc_text("PAGE",PciPdoIrpStartDevice)
#pragma alloc_text("PAGE",PciPdoIrpQueryRemoveDevice)
#pragma alloc_text("PAGE",PciPdoIrpRemoveDevice)
#pragma alloc_text("PAGE",PciPdoIrpCancelRemoveDevice)
#pragma alloc_text("PAGE",PciPdoIrpStopDevice)
#pragma alloc_text("PAGE",PciPdoIrpQueryStopDevice)
#pragma alloc_text("PAGE",PciPdoIrpCancelStopDevice)
#pragma alloc_text("PAGE",PciPdoIrpQueryDeviceRelations)
#pragma alloc_text("PAGE",PciPdoIrpQueryInterface)
#pragma alloc_text("PAGE",PciPdoIrpQueryCapabilities)
#pragma alloc_text("PAGE",PciPdoIrpQueryResourceRequirements)
#pragma alloc_text("PAGE",PciPdoIrpQueryDeviceText)
#pragma alloc_text("PAGE",PciPdoIrpReadConfig)
#pragma alloc_text("PAGE",PciPdoIrpWriteConfig)
#pragma alloc_text("PAGE",PciPdoIrpQueryId)
#pragma alloc_text("PAGE",PciPdoIrpQueryDeviceState)
#pragma alloc_text("PAGE",PciPdoIrpQueryBusInformation)
#pragma alloc_text("PAGE",PciPdoIrpDeviceUsageNotification)
#pragma alloc_text("PAGE",PciPdoIrpSurpriseRemoval)
#pragma alloc_text("PAGE",PciPdoIrpQueryLegacyBusInformation)

//
// create pdo [checked]
//
NTSTATUS PciPdoCreate(__in PPCI_FDO_EXTENSION ParentFdoExt,__in PCI_SLOT_NUMBER Slot,__out PDEVICE_OBJECT* Pdo)
{
	PAGED_CODE();

	//
	// update seq number
	//
	LONG SeqNumber										= InterlockedIncrement(&PciPdoSequenceNumber);

	//
	// build name
	//
	WCHAR NameBuffer[0x20]								= {0};
	RtlStringCchPrintfW(NameBuffer,ARRAYSIZE(NameBuffer) - 1,L"\\Device\\NTPNP_PCI%04d",SeqNumber);

	//
	// create device
	//
	UNICODE_STRING Name;
	RtlInitUnicodeString(&Name,NameBuffer);
	NTSTATUS Status										= IoCreateDevice(PciDriverObject,sizeof(PCI_PDO_EXTENSION),&Name,FILE_DEVICE_UNKNOWN,0,0,Pdo);
	if(!NT_SUCCESS(Status))
		return Status;

	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>((*Pdo)->DeviceExtension);
	PciDebugPrintf(3,"PCI: New PDO (b=0x%x, d=0x%x, f=0x%x) @ %p, ext @ %p\n",ParentFdoExt->BaseBus,Slot.u.bits.DeviceNumber,
				   Slot.u.bits.FunctionNumber,*Pdo,PdoExt);

	//
	// setup
	//
	RtlZeroMemory(PdoExt,sizeof(PCI_PDO_EXTENSION));
	PdoExt->Common.ExtensionType						= PciPdoExtensionType;
	PdoExt->Common.IrpDispatchTable						= &PciPdoDispatchTable;
	PdoExt->ParentFdoExtension							= ParentFdoExt;
	PdoExt->PowerState.CurrentDeviceState				= PowerDeviceD0;
	PdoExt->PowerState.CurrentSystemState				= PowerSystemWorking;
	PdoExt->PhysicalDeviceObject						= *Pdo;
	PdoExt->Slot										= Slot;
	PdoExt->Slot.u.bits.Reserved						= 0;
	KeInitializeEvent(&PdoExt->Common.SecondaryExtLock,SynchronizationEvent,TRUE);

	//
	// initialize state
	//
	PciInitializeState(&PdoExt->Common);

	//
	// link it
	//
	PciInsertEntryAtTail(&ParentFdoExt->ChildPdoList,&PdoExt->Common.ListEntry,&ParentFdoExt->ChildListLock);

	return STATUS_SUCCESS;
}

//
// delete pdo [checked]
//
VOID PciPdoDestroy(__in PDEVICE_OBJECT Pdo)
{
	PAGED_CODE();

	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);

	ASSERT(PdoExt->Common.ExtensionType	== PciPdoExtensionType);
	ASSERT(!PdoExt->LegacyDriver);
	ASSERT(PdoExt->ParentFdoExtension->Common.ExtensionType == PciFdoExtensionType);

	PPCI_FDO_EXTENSION ParentFdoExt						= PdoExt->ParentFdoExtension;

	PciDebugPrintf(3,"PCI: destroy PDO (b=0x%x, d=0x%x, f=0x%x)\n",ParentFdoExt->BaseBus,PdoExt->Slot.u.bits.DeviceNumber,PdoExt->Slot.u.bits.FunctionNumber);

	//
	// remove from parent list
	//
	PciRemoveEntryFromList(&ParentFdoExt->ChildPdoList,&PdoExt->Common.ListEntry,0);

	//
	// remove from bridge list
	//
	if(ParentFdoExt->ChildBridgePdoList == PdoExt)
	{
		ParentFdoExt->ChildBridgePdoList				= PdoExt->NextBridge;
	}
	else
	{
		PPCI_PDO_EXTENSION Prev							= ParentFdoExt->ChildBridgePdoList;
		while(Prev && Prev->NextBridge != PdoExt)
			Prev										= Prev->NextBridge;

		if(Prev)
			Prev->NextBridge							= PdoExt->NextBridge;
	}

	//
	// destroy secondary extensions
	//
	PPCI_SECONDARY_EXTENSION SecondaryExtension			= CONTAINING_RECORD(PdoExt->SecondaryExtension.Next,PCI_SECONDARY_EXTENSION,ListEntry);
	while(SecondaryExtension)
	{
		PcipDestroySecondaryExtension(&PdoExt->SecondaryExtension,0,SecondaryExtension);

		SecondaryExtension								= CONTAINING_RECORD(PdoExt->SecondaryExtension.Next,PCI_SECONDARY_EXTENSION,ListEntry);
	}

	PciInvalidateResourceInfoCache(PdoExt);

	//
	// setup a error type
	//
	PdoExt->Common.ExtensionType						= static_cast<PCI_SIGNATURE>(0xdead);

	//
	// free resource buffer
	//
	if(PdoExt->Resources)
		ExFreePool(PdoExt->Resources);

	//
	// free fake parent's ext
	//
	if(!ParentFdoExt->ChildBridgePdoList && ParentFdoExt->ChildDelete)
		ExFreePool(ParentFdoExt);

	IoDeleteDevice(Pdo);
}

//
// start pdo [checked]
//
NTSTATUS PciPdoIrpStartDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	NTSTATUS Status										= PciBeginStateTransition(&PdoExt->Common,PciStarted);
	if(!NT_SUCCESS(Status))
		return Status;

	__try
	{
		if( (PdoExt->BaseClass != PCI_CLASS_PRE_20 || PdoExt->SubClass != PCI_SUBCLASS_PRE_20_VGA) &&
			(PdoExt->BaseClass != PCI_CLASS_DISPLAY_CTLR || PdoExt->SubClass != PCI_SUBCLASS_VID_VGA_CTLR))
		{
			SetFlag(PdoExt->CommandEnables,PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
		}

		if(PdoExt->IoSpaceUnderNativeIdeControl)
			ClearFlag(PdoExt->CommandEnables,PCI_ENABLE_MEMORY_SPACE);

		SetFlag(PdoExt->CommandEnables,PCI_ENABLE_BUS_MASTER);

		PCM_RESOURCE_LIST CmResList						= IrpSp->Parameters.StartDevice.AllocatedResources;

		//
		// compute new resource settings
		//
		if(PciComputeNewCurrentSettings(PdoExt,CmResList))
			PdoExt->MovedDevice							= TRUE;
		else
			PciDebugPrintf(0x7fffffff,"PCI - START not changing resource settings.\n");

		//
		// power up device if we are not in d0 state
		//
		BOOLEAN ResetDevice								= FALSE;
		if(PdoExt->PowerState.CurrentDeviceState != PowerDeviceD0)
		{
			if(!NT_SUCCESS(PciSetPowerManagedDevicePowerState(PdoExt,PowerDeviceD0,FALSE)))
				try_leave(Status = STATUS_DEVICE_POWER_FAILURE);

			POWER_STATE PowerState;
			PowerState.DeviceState						= PowerDeviceD0;
			PoSetPowerState(PdoExt->PhysicalDeviceObject,DevicePowerState,PowerState);

			PdoExt->PowerState.CurrentDeviceState		= PowerDeviceD0;

			ResetDevice									= TRUE;
		}

		//
		// set resource
		//
		Status											= PciSetResources(PdoExt,ResetDevice,TRUE);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);
	}
	__finally
	{
		if(!NT_SUCCESS(Status))
			PciCancelStateTransition(&PdoExt->Common,PciStarted);
		else
			PciCommitStateTransition(&PdoExt->Common,PciStarted);
	}

	return Status;
}

//
// query remove pdo [checked]
//
NTSTATUS PciPdoIrpQueryRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	//
	// hibernate,crashdump,paging,debug device can not be removed
	//
	if(PdoExt->PowerState.Hibernate || PdoExt->PowerState.CrashDump || PdoExt->PowerState.Paging || PdoExt->OnDebugPath)
		return STATUS_DEVICE_BUSY;

	if(FlagOn(PdoExt->HackFlags.HighPart,PCI_HACK_FLAGS_HIGH_NO_REMOVE))
		return STATUS_DEVICE_BUSY;

	//
	// bridge on vga path
	//
	if(PdoExt->HeaderType == PCI_BRIDGE_TYPE && PciIsOnVGAPath(PdoExt))
		return STATUS_DEVICE_BUSY;

	//
	// legacy driver
	//
	if(PdoExt->LegacyDriver)
		return STATUS_DEVICE_BUSY;

	//
	// did not receive a start request?
	//
	if(PdoExt->Common.DeviceState == PciNotStarted)
		return STATUS_SUCCESS;

	return PciBeginStateTransition(&PdoExt->Common,PciNotStarted);
}

//
// remove pdo [checked]
//
NTSTATUS PciPdoIrpRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	PPCI_FDO_EXTENSION ParentFdoExt						= PdoExt->ParentFdoExtension;
	PdoExt->BridgeFdoExtension							= 0;

	if(!PdoExt->NotPresent)
	{
		//
		// disable decode and power down
		//
		PciDecodeEnable(PdoExt,FALSE,0);

		if(PdoExt->PowerState.CurrentDeviceState != PowerDeviceD3 && PciCanDisableDecodes(PdoExt,0,0,0,FALSE))
		{
			PciSetPowerManagedDevicePowerState(PdoExt,PowerDeviceD3,FALSE);

			POWER_STATE State;
			State.DeviceState							= PowerDeviceD3;
			PoSetPowerState(PdoExt->PhysicalDeviceObject,DevicePowerState,State);

			PdoExt->PowerState.CurrentDeviceState		= PowerDeviceD3;
		}
	}

	//
	// transition to not start state
	//
	if(!PciIsInTransitionToState(&PdoExt->Common,PciNotStarted) && PdoExt->Common.DeviceState == PciStarted)
		PciBeginStateTransition(&PdoExt->Common,PciNotStarted);

	if(PciIsInTransitionToState(&PdoExt->Common,PciNotStarted))
		PciCommitStateTransition(&PdoExt->Common,PciNotStarted);

	//
	// we can delete the device if and only if we reported this device as missing in the query bus relationship request
	//
	if(PdoExt->ReportedMissing)
	{
		NTSTATUS Status									= PciBeginStateTransition(&PdoExt->Common,PciDeleted);
		ASSERT(NT_SUCCESS(Status));

		//
		// acquire parent's lock and destroy this pdo
		//
		if(ParentFdoExt)
		{
			KeEnterCriticalRegion();
			KeWaitForSingleObject(&ParentFdoExt->ChildListLock,Executive,KernelMode,FALSE,0);
		}

		PciPdoDestroy(PdoExt->PhysicalDeviceObject);

		if(ParentFdoExt)
		{
			KeSetEvent(&ParentFdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
			KeLeaveCriticalRegion();
		}
	}

	return STATUS_SUCCESS;
}

//
// cancel remove pdo [checked]
//
NTSTATUS PciPdoIrpCancelRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	PciCancelStateTransition(&PdoExt->Common,PciNotStarted);

	return STATUS_SUCCESS;
}

//
// stop pdo [checked]
//
NTSTATUS PciPdoIrpStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	PciDecodeEnable(PdoExt,FALSE,0);

	PciCommitStateTransition(&PdoExt->Common,PciStopped);

	return STATUS_SUCCESS;
}

//
// pdo query stop [checked]
//
NTSTATUS PciPdoIrpQueryStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	//
	// hibernate,crashdump,paging,debug device can not be stopped
	//
	if(PdoExt->PowerState.Hibernate || PdoExt->PowerState.CrashDump || PdoExt->PowerState.Paging || PdoExt->OnDebugPath)
		return STATUS_DEVICE_BUSY;

	//
	// pci-to-pci bridge and cardbus bridge
	//
	if(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && (PdoExt->SubClass == PCI_SUBCLASS_BR_CARDBUS || PdoExt->SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI))
		return STATUS_INVALID_DEVICE_REQUEST;

	//
	// legacy driver
	//
	if(PdoExt->LegacyDriver)
		return STATUS_INVALID_DEVICE_REQUEST;

	//
	// can not disable decode
	//
	if(!PciCanDisableDecodes(PdoExt,0,0,0,FALSE))
		return STATUS_INVALID_DEVICE_REQUEST;

	return PciBeginStateTransition(&PdoExt->Common,PciStopped);
}

//
// cancel stop pdo [checked]
//
NTSTATUS PciPdoIrpCancelStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	PciCancelStateTransition(&PdoExt->Common,PciStopped);

	return STATUS_SUCCESS;
}

//
// query relations [checked]
//
NTSTATUS PciPdoIrpQueryDeviceRelations(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	if(IrpSp->Parameters.QueryDeviceRelations.Type == EjectionRelations)
		return PciQueryEjectionRelations(PdoExt,static_cast<PDEVICE_RELATIONS*>(static_cast<PVOID>(&Irp->IoStatus.Information)));
	else if(IrpSp->Parameters.QueryDeviceRelations.Type == TargetDeviceRelation)
		return PciQueryTargetDeviceRelations(PdoExt,static_cast<PDEVICE_RELATIONS*>(static_cast<PVOID>(&Irp->IoStatus.Information)));

	return STATUS_NOT_SUPPORTED;
}

//
// query interface [checked]
//
NTSTATUS PciPdoIrpQueryInterface(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	GUID const* Type									= IrpSp->Parameters.QueryInterface.InterfaceType;
	USHORT Size											= IrpSp->Parameters.QueryInterface.Size;
	USHORT Version										= IrpSp->Parameters.QueryInterface.Version;
	PVOID Data											= IrpSp->Parameters.QueryInterface.InterfaceSpecificData;
	PINTERFACE Interface								= IrpSp->Parameters.QueryInterface.Interface;
	NTSTATUS Status										= PciQueryInterface(&PdoExt->Common,Type,Size,Version,Data,Interface,FALSE);
	if(NT_SUCCESS(Status))
		return Status;

	PPCI_FDO_EXTENSION BridgeFdoExt						= PdoExt->BridgeFdoExtension;
	if(!BridgeFdoExt || !BridgeFdoExt->Fake)
		return Status;

	ASSERT(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && PdoExt->SubClass == PCI_SUBCLASS_BR_CARDBUS);

	return PciQueryInterface(&PdoExt->BridgeFdoExtension->Common,Type,Size,Version,Data,Interface,FALSE);
}

//
// query capabilites [checked]
//
NTSTATUS PciPdoIrpQueryCapabilities(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	return PciQueryCapabilities(PdoExt,IrpSp->Parameters.DeviceCapabilities.Capabilities);
}

//
// query resource [checked]
//
NTSTATUS PciPdoIrpQueryResources(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	return PciQueryResources(PdoExt,reinterpret_cast<PCM_RESOURCE_LIST*>(&Irp->IoStatus.Information));
}

//
// query resource requirements [checked]
//
NTSTATUS PciPdoIrpQueryResourceRequirements(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	return PciQueryRequirements(PdoExt,reinterpret_cast<PIO_RESOURCE_REQUIREMENTS_LIST*>(&Irp->IoStatus.Information));
}

//
// query text [checked]
//
NTSTATUS PciPdoIrpQueryDeviceText(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	LCID LocalId										= IrpSp->Parameters.QueryDeviceText.LocaleId;
	DEVICE_TEXT_TYPE Type								= IrpSp->Parameters.QueryDeviceText.DeviceTextType;
	return PciQueryDeviceText(PdoExt,Type,LocalId,reinterpret_cast<PWCHAR*>(&Irp->IoStatus.Information));
}

//
// read config [checked]
//
NTSTATUS PciPdoIrpReadConfig(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	ULONG Offset										= IrpSp->Parameters.ReadWriteConfig.Offset;
	ULONG Length										= IrpSp->Parameters.ReadWriteConfig.Length;
	PVOID Buffer										= IrpSp->Parameters.ReadWriteConfig.Buffer;
	ULONG WhichSpace									= IrpSp->Parameters.ReadWriteConfig.WhichSpace;
	NTSTATUS Status										= PciReadDeviceSpace(PdoExt,WhichSpace,Buffer,Offset,Length,&Length);
	Irp->IoStatus.Information							= Length;

	return Status;
}

//
// write config [checked]
//
NTSTATUS PciPdoIrpWriteConfig(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	ULONG Offset										= IrpSp->Parameters.ReadWriteConfig.Offset;
	ULONG Length										= IrpSp->Parameters.ReadWriteConfig.Length;
	PVOID Buffer										= IrpSp->Parameters.ReadWriteConfig.Buffer;
	ULONG WhichSpace									= IrpSp->Parameters.ReadWriteConfig.WhichSpace;
	NTSTATUS Status										= PciWriteDeviceSpace(PdoExt,WhichSpace,Buffer,Offset,Length,&Length);
	Irp->IoStatus.Information							= Length;

	return Status;
}

//
// query id [checked]
//
NTSTATUS PciPdoIrpQueryId(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	return PciQueryId(PdoExt,IrpSp->Parameters.QueryId.IdType,reinterpret_cast<PWCHAR*>(&Irp->IoStatus.Information));
}

//
// query state [checked]
//
NTSTATUS PciPdoIrpQueryDeviceState(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	if(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && PdoExt->SubClass == PCI_SUBCLASS_BR_HOST)
		SetFlag(Irp->IoStatus.Information,PNP_DEVICE_NOT_DISABLEABLE);

	if(PdoExt->HeaderType == PCI_BRIDGE_TYPE && PciIsOnVGAPath(PdoExt))
		SetFlag(Irp->IoStatus.Information,PNP_DEVICE_NOT_DISABLEABLE);

	return STATUS_SUCCESS;
}

//
// query bus info [checked]
//
NTSTATUS PciPdoIrpQueryBusInformation(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	return PciQueryBusInformation(PdoExt,reinterpret_cast<PPNP_BUS_INFORMATION*>(&Irp->IoStatus.Information));
}

//
// usage notification [checked]
//
NTSTATUS PciPdoIrpDeviceUsageNotification(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	return PciPdoDeviceUsage(PdoExt,Irp);
}

//
// surprise removal [checked]
//
NTSTATUS PciPdoIrpSurpriseRemoval(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	if(!PdoExt->NotPresent)
	{
		//
		// disable decode and power down
		//
		PciDecodeEnable(PdoExt,FALSE,0);

		if(PdoExt->PowerState.CurrentDeviceState != PowerDeviceD3 && PciCanDisableDecodes(PdoExt,0,0,0,FALSE))
		{
			PciSetPowerManagedDevicePowerState(PdoExt,PowerDeviceD3,FALSE);

			POWER_STATE State;
			State.DeviceState							= PowerDeviceD3;
			PoSetPowerState(PdoExt->PhysicalDeviceObject,DevicePowerState,State);

			PdoExt->PowerState.CurrentDeviceState		= PowerDeviceD3;
		}
	}

	if(PdoExt->ReportedMissing)
	{
		PciBeginStateTransition(&PdoExt->Common,PciSurpriseRemoved);
		PciCommitStateTransition(&PdoExt->Common,PciSurpriseRemoved);
	}
	else
	{
		PciBeginStateTransition(&PdoExt->Common,PciNotStarted);
	}

	return STATUS_SUCCESS;
}

//
// query legacy bus info [checked]
//
NTSTATUS PciPdoIrpQueryLegacyBusInformation(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	if(PciClassifyDeviceType(PdoExt) != PCI_DEVICE_TYPE_CARDBUS)
		return STATUS_NOT_SUPPORTED;

	ULONG Length										= sizeof(LEGACY_BUS_INFORMATION);
	PLEGACY_BUS_INFORMATION Buffer						= static_cast<PLEGACY_BUS_INFORMATION>(ExAllocatePoolWithTag(PagedPool,Length,'Bicp'));
	if(!Buffer)
		return STATUS_INSUFFICIENT_RESOURCES;

	Buffer->BusNumber									= PdoExt->Dependent.type2.SecondaryBus;
	Buffer->LegacyBusType								= PCIBus;
	Buffer->BusTypeGuid									= GUID_BUS_TYPE_PCI;
	Irp->IoStatus.Information							= reinterpret_cast<ULONG_PTR>(Buffer);

	return STATUS_SUCCESS;
}