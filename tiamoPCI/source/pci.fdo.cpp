//********************************************************************
//	created:	22:7:2008   23:38
//	file:		pci.fdo.cpp
//	author:		tiamo
//	purpose:	fdo
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciAddDevice)
#pragma alloc_text("PAGE",PciInitializeFdoExtensionCommonFields)
#pragma alloc_text("PAGE",PciGetHotPlugParameters)
#pragma alloc_text("PAGE",PciFdoIrpStartDevice)
#pragma alloc_text("PAGE",PciFdoIrpQueryRemoveDevice)
#pragma alloc_text("PAGE",PciFdoIrpRemoveDevice)
#pragma alloc_text("PAGE",PciFdoIrpCancelRemoveDevice)
#pragma alloc_text("PAGE",PciFdoIrpStopDevice)
#pragma alloc_text("PAGE",PciFdoIrpQueryStopDevice)
#pragma alloc_text("PAGE",PciFdoIrpCancelStopDevice)
#pragma alloc_text("PAGE",PciFdoIrpQueryDeviceRelations)
#pragma alloc_text("PAGE",PciFdoIrpQueryInterface)
#pragma alloc_text("PAGE",PciFdoIrpQueryCapabilities)
#pragma alloc_text("PAGE",PciFdoIrpDeviceUsageNotification)
#pragma alloc_text("PAGE",PciFdoIrpSurpriseRemoval)
#pragma alloc_text("PAGE",PciFdoIrpQueryLegacyBusInformation)

//
// add device [checked]
//
NTSTATUS PciAddDevice(__in PDRIVER_OBJECT DriverObject,__in PDEVICE_OBJECT PhysicalDeviceObject)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;
	PDEVICE_OBJECT FunctionDeviceObject					= 0;
	PCM_RESOURCE_LIST CmRes								= 0;

	__try
	{
		PciDebugPrintf(0x1000,"PCI - AddDevice (a new bus).\n");

		//
		// add a child bus?
		//
		PPCI_FDO_EXTENSION ParentFdoExt					= PciFindParentPciFdoExtension(PhysicalDeviceObject,&PciGlobalLock);
		PPCI_PDO_EXTENSION PdoExt						= 0;

		if(ParentFdoExt)
		{
			PdoExt										= static_cast<PPCI_PDO_EXTENSION>(PhysicalDeviceObject->DeviceExtension);

			ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

			if(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && PdoExt->SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI)
			{
				PciDebugPrintf(0x1000,"PCI - AddDevice (new bus is child of bus 0x%x).\n",ParentFdoExt->BaseBus);

				if(!PciAreBusNumbersConfigured(PdoExt))
				{
					PciDebugPrintf(0x1001,"PCI - Bus numbers not configured for bridge (0x%x.0x%x.0x%x)\n",
								   ParentFdoExt->BaseBus,PdoExt->Slot.u.bits.DeviceNumber,PdoExt->Slot.u.bits.FunctionNumber);

					try_leave(Status = STATUS_UNSUCCESSFUL);
				}
			}
			else
			{
				PciDebugPrintf(0,"PCI - PciAddDevice for Non-Root/Non-PCI-PCI bridge,\n    Class %02x, SubClass %02x, will not add.\n",
							   PdoExt->BaseClass,PdoExt->SubClass);

				ASSERT(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && PdoExt->SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI);

				try_leave(Status = STATUS_UNSUCCESSFUL);
			}
		}

		//
		// create device
		//
		Status											= IoCreateDevice(DriverObject,sizeof(PCI_FDO_EXTENSION),0,FILE_DEVICE_BUS_EXTENDER,0,0,&FunctionDeviceObject);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		PPCI_FDO_EXTENSION FdoExt						= static_cast<PPCI_FDO_EXTENSION>(FunctionDeviceObject->DeviceExtension);

		//
		// initialize common field
		//
		PciInitializeFdoExtensionCommonFields(FdoExt,FunctionDeviceObject,PhysicalDeviceObject);

		//
		// attach to stack
		//
		FdoExt->AttachedDeviceObject					= IoAttachDeviceToDeviceStack(FunctionDeviceObject,PhysicalDeviceObject);
		if(!FdoExt->AttachedDeviceObject)
			try_leave(Status = STATUS_NO_SUCH_DEVICE);

		if(ParentFdoExt)
		{
			//
			// our base bus is the secondary bus of the pdo
			//
			FdoExt->BaseBus								= PdoExt->Dependent.type1.SecondaryBus;
			FdoExt->BusRootFdoExtension					= ParentFdoExt->BusRootFdoExtension;
			PdoExt->BridgeFdoExtension					= FdoExt;
			FdoExt->ParentFdoExtension					= ParentFdoExt;

			PciInvalidateResourceInfoCache(PdoExt);
		}
		else
		{
			//
			// this is the root bridge,read boot configuration from registry and find the bus number resource descriptor
			//
			PCM_PARTIAL_RESOURCE_DESCRIPTOR BusNumDesc	= 0;
			Status										= PciGetDeviceProperty(PhysicalDeviceObject,DevicePropertyBootConfiguration,reinterpret_cast<PVOID*>(&CmRes));
			if(NT_SUCCESS(Status))
			{
				PciDebugPrintf(0x2000,"PCI - CM RESOURCE LIST FROM ROOT PDO\n");
				PciDebugPrintCmResList(0x2000,CmRes);

				BusNumDesc								= PciFindDescriptorInCmResourceList(CmResourceTypeBusNumber,CmRes,0);
			}

			//
			// unable to find bus number resource descriptor,try to use a default one
			//
			if(!BusNumDesc)
			{
				if(PciDefaultConfigAlreadyUsed)
					KeBugCheckEx(0xA1,0xDEAD0010,reinterpret_cast<ULONG_PTR>(PhysicalDeviceObject),0,0);

				PciDebugPrintf(0,"PCI   Will use default configuration.\n");

				PciDefaultConfigAlreadyUsed				= TRUE;

				FdoExt->BaseBus							= 0;
			}
			else
			{
				ASSERT(BusNumDesc->u.BusNumber.Start <= PCI_MAX_BRIDGE_NUMBER);
				ASSERT(BusNumDesc->u.BusNumber.Start + BusNumDesc->u.BusNumber.Length - 1 <= PCI_MAX_BRIDGE_NUMBER);

				FdoExt->BaseBus							= static_cast<UCHAR>(BusNumDesc->u.BusNumber.Start);
				FdoExt->MaxSubordinateBus				= static_cast<UCHAR>(BusNumDesc->u.BusNumber.Start + BusNumDesc->u.BusNumber.Length - 1);

				PciDebugPrintf(0x1000,"PCI - Root Bus # 0x%x->0x%x.\n",FdoExt->BaseBus,FdoExt->MaxSubordinateBus);
			}

			//
			// save root fdo
			//
			FdoExt->BusRootFdoExtension					= FdoExt;
		}

		//
		// get config handler
		//
		Status											= PciGetConfigHandlers(FdoExt);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// intialize arbiters
		//
		Status											= PciInitializeArbiters(FdoExt);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// link it
		//
		PciInsertEntryAtTail(&PciFdoExtensionListHead,&FdoExt->Common.ListEntry,&PciGlobalLock);

		//
		// open hardware key
		//
		HANDLE HardwareKeyHandle						= 0;
		Status											= IoOpenDeviceRegistryKey(PhysicalDeviceObject,PLUGPLAY_REGKEY_DEVICE,KEY_ALL_ACCESS,&HardwareKeyHandle);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// query bus hack flags
		//
		UNICODE_STRING ValueName;
		RtlInitUnicodeString(&ValueName,L"HackFlags");
		UCHAR Buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
		ULONG Length									= 0;
		Status											= ZwQueryValueKey(HardwareKeyHandle,&ValueName,KeyValuePartialInformation,Buffer,sizeof(Buffer),&Length);
		ZwClose(HardwareKeyHandle);

		PKEY_VALUE_PARTIAL_INFORMATION ValueInfo		= reinterpret_cast<PKEY_VALUE_PARTIAL_INFORMATION>(Buffer);
		if(NT_SUCCESS(Status) && ValueInfo->Type == REG_DWORD && ValueInfo->DataLength == sizeof(ULONG))
			FdoExt->BusHackFlags						= *reinterpret_cast<PULONG>(ValueInfo->Data);

		ClearFlag(FunctionDeviceObject->Flags,DO_DEVICE_INITIALIZING);

		//
		// get hotplug parameters
		//
		PciGetHotPlugParameters(FdoExt);

		Status											= STATUS_SUCCESS;
	}
	__finally
	{
		if((AbnormalTermination() || !NT_SUCCESS(Status)) && FunctionDeviceObject)
		{
			PPCI_FDO_EXTENSION FdoExt					= static_cast<PPCI_FDO_EXTENSION>(FunctionDeviceObject->DeviceExtension);

			//
			// destroy secondary extension
			//
			while(FdoExt->SecondaryExtension.Next)
				PcipDestroySecondaryExtension(&FdoExt->SecondaryExtension,0,CONTAINING_RECORD(FdoExt->SecondaryExtension.Next,PCI_SECONDARY_EXTENSION,ListEntry));

			if(FdoExt->AttachedDeviceObject)
				IoDetachDevice(FdoExt->AttachedDeviceObject);

			IoDeleteDevice(FunctionDeviceObject);
		}

		if(CmRes)
			ExFreePool(CmRes);
	}

	return Status;
}

//
// initialize fdo extension common fields [checked]
//
VOID PciInitializeFdoExtensionCommonFields(__in PPCI_FDO_EXTENSION FdoExt,__in PDEVICE_OBJECT FunctionDeviceObject,__in PDEVICE_OBJECT PhyscialDeviceObject)
{
	PAGED_CODE();

	//
	// zero it out
	//
	RtlZeroMemory(FdoExt,sizeof(PCI_FDO_EXTENSION));

	FdoExt->FunctionalDeviceObject						= FunctionDeviceObject;
	FdoExt->PhysicalDeviceObject						= PhyscialDeviceObject;
	FdoExt->Common.ExtensionType						= PciFdoExtensionType;
	FdoExt->Common.IrpDispatchTable						= &PciFdoDispatchTable;
	FdoExt->PowerState.CurrentDeviceState				= PowerDeviceD0;
	FdoExt->PowerState.CurrentSystemState				= PowerSystemWorking;

	KeInitializeEvent(&FdoExt->ChildListLock,SynchronizationEvent,TRUE);
	KeInitializeEvent(&FdoExt->Common.SecondaryExtLock,SynchronizationEvent,TRUE);

	PciInitializeState(&FdoExt->Common);
}

//
// get hotplug parameters [checked]
//
NTSTATUS PciGetHotPlugParameters(__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;
	PVOID Output										= 0;

	__try
	{
		//
		// allocate output buffer
		//
		ULONG Length									= sizeof(ACPI_EVAL_OUTPUT_BUFFER) + sizeof(ACPI_METHOD_ARGUMENT) * 4;
		Output											= PciAllocateColdPoolWithTag(PagedPool,Length,'BicP');
		if(!Output)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		RtlZeroMemory(Output,Length);

		//
		// eval acpi method _HPP
		//
		ACPI_EVAL_INPUT_BUFFER Input;
		Input.MethodNameAsUlong							= 'PPH_';
		Input.Signature									= ACPI_EVAL_INPUT_BUFFER_SIGNATURE;
		Status											= PciSendIoctl(FdoExt->PhysicalDeviceObject,IOCTL_ACPI_EVAL_METHOD,&Input,sizeof(Input),Output,Length);

		//
		// acpi failed,use the parent's hotplug info
		//
		if(!NT_SUCCESS(Status))
		{
			if(FdoExt != FdoExt->BusRootFdoExtension)
			{
				FdoExt->HotPlugParameters				= FdoExt->ParentFdoExtension->HotPlugParameters;
				Status									= STATUS_SUCCESS;
			}
		}
		else
		{
			//
			// check acpi return value
			//
			PACPI_EVAL_OUTPUT_BUFFER Result				= static_cast<PACPI_EVAL_OUTPUT_BUFFER>(Output);
			if(Result->Count != 4)
				try_leave(Status = STATUS_UNSUCCESSFUL);

			if(Result->Argument[0].Type != ACPI_METHOD_ARGUMENT_INTEGER || Result->Argument[0].Argument > 0xff)
				try_leave(Status = STATUS_UNSUCCESSFUL);

			if(Result->Argument[1].Type != ACPI_METHOD_ARGUMENT_INTEGER || Result->Argument[1].Argument > 0xff)
				try_leave(Status = STATUS_UNSUCCESSFUL);

			if(Result->Argument[2].Type != ACPI_METHOD_ARGUMENT_INTEGER || Result->Argument[2].Argument > 1)
				try_leave(Status = STATUS_UNSUCCESSFUL);

			if(Result->Argument[3].Type != ACPI_METHOD_ARGUMENT_INTEGER || Result->Argument[3].Argument > 1)
				try_leave(Status = STATUS_UNSUCCESSFUL);

			FdoExt->HotPlugParameters.Acquired			= TRUE;
			FdoExt->HotPlugParameters.CacheLineSize		= static_cast<UCHAR>(Result->Argument[0].Argument);
			FdoExt->HotPlugParameters.LatencyTimer		= static_cast<UCHAR>(Result->Argument[1].Argument);
			FdoExt->HotPlugParameters.EnablePERR		= Result->Argument[2].Argument == TRUE;
			FdoExt->HotPlugParameters.EnableSERR		= Result->Argument[3].Argument == TRUE;
		}
	}
	__finally
	{
		if(Output)
			ExFreePool(Output);
	}

	return Status;
}

//
// start fdo [checked]
//
NTSTATUS PciFdoIrpStartDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	//
	// lower driver failed,we return not_supported,DispatchIrp will complete this irp with the status in the irp's io status block
	//
	if(!NT_SUCCESS(Irp->IoStatus.Status))
		return STATUS_NOT_SUPPORTED;

	//
	// check we can goto started state
	//
	NTSTATUS Status											= PciBeginStateTransition(&FdoExt->Common,PciStarted);
	if(!NT_SUCCESS(Status))
		return Status;

	__try
	{
		//
		// check resource list
		//
		PCM_RESOURCE_LIST CmResList						= IrpSp->Parameters.StartDevice.AllocatedResourcesTranslated;
		UCHAR SavedResType[PCI_TYPE1_ADDRESSES]			= {CmResourceTypeNull,CmResourceTypeNull};
		PPCI_PDO_EXTENSION PdoExt						= 0;

		if(CmResList && FdoExt->BusRootFdoExtension != FdoExt)
		{
			ASSERT(CmResList->Count == 1);
			PdoExt										= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);

			//
			// resource list should match those resouces we stored in pdo ext
			//
			if(PdoExt->Resources && PdoExt->HeaderType == PCI_BRIDGE_TYPE)
			{
				PIO_RESOURCE_DESCRIPTOR Requirement		= PdoExt->Resources->Limit;
				PCM_PARTIAL_RESOURCE_DESCRIPTOR Current = CmResList->List->PartialResourceList.PartialDescriptors;

				//
				// we will build a range list from the start resources,but first
				// mark those resources for BAR as null to keep it from being used by the arbiter
				//
				for(ULONG i = 0; i < PCI_TYPE1_ADDRESSES; i ++)
				{
					//
					// the format should be a regular resource followed by a device private data
					//
					if(Requirement->Type != CmResourceTypeNull)
					{
						ASSERT(Requirement->Type == Current->Type);
						ASSERT((Current + 1)->Type == CmResourceTypeDevicePrivate);

						SavedResType[i]					= Current->Type;
						Current->Type					= CmResourceTypeNull;

						Current							+= 2;
					}

					Requirement							+= 1;
				}
			}
		}

		//
		// build arbiter range from resource list
		//
		Status											= PciInitializeArbiterRanges(FdoExt,CmResList);
		if(!CmResList || FdoExt->BusRootFdoExtension == FdoExt || !PdoExt->Resources)
			try_leave(NOTHING);

		//
		// restore resource type for BAR
		//
		PCM_PARTIAL_RESOURCE_DESCRIPTOR Current = CmResList->List->PartialResourceList.PartialDescriptors;

		for(ULONG i = 0; i < PCI_TYPE1_ADDRESSES; i ++)
		{
			if(SavedResType[i] != CmResourceTypeNull)
			{
				ASSERT((Current + 1)->Type == CmResourceTypeDevicePrivate);

				Current->Type							= SavedResType[i];

				Current									+= 2;
			}
		}
	}
	__finally
	{
		if(NT_SUCCESS(Status))
			PciCommitStateTransition(&FdoExt->Common,PciStarted);
		else
			PciCancelStateTransition(&FdoExt->Common,PciStarted);
	}

	return Status;
}

//
// query remove
//
NTSTATUS PciFdoIrpQueryRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

		//
		// if we have a legacy driver,fail this request
		//
		PPCI_PDO_EXTENSION PdoExt						= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);

		while(PdoExt)
		{
			if(PdoExt->LegacyDriver)
			{
				Status									= STATUS_DEVICE_BUSY;
				break;
			}

			PdoExt										= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		}
	}
	__finally
	{
		KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();

		//
		// if we did not fail this request,then begin transition to deleted state
		//
		if(NT_SUCCESS(Status))
			Status										= PciBeginStateTransition(&FdoExt->Common,PciDeleted);
	}

	return Status;
}

//
// remove [checked]
//
NTSTATUS PciFdoIrpRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	KeEnterCriticalRegion();
	KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

	//
	// delete all children
	//
	PPCI_PDO_EXTENSION PdoExt							= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);

	while(PdoExt)
	{
		PciDebugPrintf(3,"PCI Killing PDO %p PDOx %p (b=%d, d=%d, f=%d)\n",PdoExt->PhysicalDeviceObject,PdoExt,FdoExt->BaseBus,
					   PdoExt->Slot.u.bits.DeviceNumber,PdoExt->Slot.u.bits.FunctionNumber);

		PciPdoDestroy(PdoExt->PhysicalDeviceObject);

		PdoExt											= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);
	}

	KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
	KeLeaveCriticalRegion();

	//
	// destroy secondary extensions
	//
	PPCI_SECONDARY_EXTENSION SecondaryExtension			= CONTAINING_RECORD(FdoExt->SecondaryExtension.Next,PCI_SECONDARY_EXTENSION,ListEntry);
	while(SecondaryExtension)
	{
		PcipDestroySecondaryExtension(&FdoExt->SecondaryExtension,0,SecondaryExtension);

		SecondaryExtension								= CONTAINING_RECORD(FdoExt->SecondaryExtension.Next,PCI_SECONDARY_EXTENSION,ListEntry);
	}

	PciDebugPrintf(1,"PCI FDOx (%p) destroyed.",FdoExt);

	//
	// if we are not in transition to delete,begin transition to delete
	//
	if(!PciIsInTransitionToState(&FdoExt->Common,PciDeleted))
	{
		NTSTATUS Status									= PciBeginStateTransition(&FdoExt->Common,PciDeleted);
		ASSERT(NT_SUCCESS(Status));
	}

	//
	// commit delete state
	//
	PciCommitStateTransition(&FdoExt->Common,PciDeleted);

	//
	// remove from global list
	//
	PciRemoveEntryFromList(&PciFdoExtensionListHead,&FdoExt->Common.ListEntry,&PciGlobalLock);

	//
	// detach from stack and delete ourself
	//
	PDEVICE_OBJECT AttachedDeviceObject					= FdoExt->AttachedDeviceObject;

	IoDetachDevice(FdoExt->AttachedDeviceObject);
	IoDeleteDevice(FdoExt->FunctionalDeviceObject);

	//
	// call down stack
	//
	IoSkipCurrentIrpStackLocation(Irp);
	Irp->IoStatus.Status								= STATUS_SUCCESS;

	return IoCallDriver(AttachedDeviceObject,Irp);
}

//
// cancel remove [checked]
//
NTSTATUS PciFdoIrpCancelRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	PciCancelStateTransition(&FdoExt->Common,PciDeleted);

	return STATUS_SUCCESS;
}

//
// stop device [checked]
//
NTSTATUS PciFdoIrpStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	PciCommitStateTransition(&FdoExt->Common,PciStopped);

	return STATUS_SUCCESS;
}

//
// query stop [checked]
//
NTSTATUS PciFdoIrpQueryStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	PciBeginStateTransition(&FdoExt->Common,PciStopped);

	return STATUS_UNSUCCESSFUL;
}

//
// cancel stop [checked]
//
NTSTATUS PciFdoIrpCancelStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	PciCancelStateTransition(&FdoExt->Common,PciStopped);

	return STATUS_SUCCESS;
}

//
// query relations [checked]
//
NTSTATUS PciFdoIrpQueryDeviceRelations(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	if(IrpSp->Parameters.QueryDeviceRelations.Type != BusRelations)
		return STATUS_NOT_SUPPORTED;

	return PciQueryDeviceRelations(FdoExt,static_cast<PDEVICE_RELATIONS*>(static_cast<PVOID>(&Irp->IoStatus.Information)));
}

//
// query interface [checked]
//
NTSTATUS PciFdoIrpQueryInterface(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	//
	// error device state?
	//
	if(FdoExt->Common.DeviceState == PciDeleted)
		return PciPassIrpFromFdoToPdo(&FdoExt->Common,Irp);

	//
	// query interfece,first chance
	//
	GUID const* Type									= IrpSp->Parameters.QueryInterface.InterfaceType;
	USHORT Size											= IrpSp->Parameters.QueryInterface.Size;
	USHORT Version										= IrpSp->Parameters.QueryInterface.Version;
	PVOID Data											= IrpSp->Parameters.QueryInterface.InterfaceSpecificData;
	PINTERFACE Interface								= IrpSp->Parameters.QueryInterface.Interface;
	NTSTATUS Status										= PciQueryInterface(&FdoExt->Common,Type,Size,Version,Data,Interface,FALSE);
	if(NT_SUCCESS(Status))
	{
		//
		// got interfece successfully,pass it down
		//
		Irp->IoStatus.Status							= Status;
		return PciPassIrpFromFdoToPdo(&FdoExt->Common,Irp);
	}

	if(Status == STATUS_NOT_SUPPORTED)
	{
		//
		// give lower driver a chance
		//
		Status											= PciCallDownIrpStack(&FdoExt->Common,Irp);

		//
		// query it,second chance
		//
		if(Status == STATUS_NOT_SUPPORTED)
			Status										= PciQueryInterface(&FdoExt->Common,Type,Size,Version,Data,Interface,TRUE);
	}

	if(Status != STATUS_NOT_SUPPORTED)
		Irp->IoStatus.Status							= Status;
	else
		Status											= Irp->IoStatus.Status;

	IoCompleteRequest(Irp,IO_NO_INCREMENT);

	return Status;
}

//
// query capabilites [checked]
//
NTSTATUS PciFdoIrpQueryCapabilities(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	PciDebugPrintf(0x10000,"PCI - FdoQueryCapabilitiesCompletion (fdox %08x) child status = %08x\n",FdoExt,Irp->IoStatus.Status);

	PDEVICE_CAPABILITIES DeviceCapabilities				= IrpSp->Parameters.DeviceCapabilities.Capabilities;

	FdoExt->PowerState.SystemWakeLevel					= DeviceCapabilities->SystemWake;
	FdoExt->PowerState.DeviceWakeLevel					= DeviceCapabilities->DeviceWake;
	RtlCopyMemory(FdoExt->PowerState.SystemStateMapping,DeviceCapabilities->DeviceState,sizeof(DeviceCapabilities->DeviceState));

	if(FlagOn(PciDebug,0x10000))
		PciDebugDumpQueryCapabilities(DeviceCapabilities);

	return STATUS_SUCCESS;
}

//
// usage notification [checked]
//
NTSTATUS PciFdoIrpDeviceUsageNotification(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	if(NT_SUCCESS(Irp->IoStatus.Status))
		return PciLocalDeviceUsage(&FdoExt->PowerState,Irp);

	return STATUS_NOT_SUPPORTED;
}

//
// surprise removal [checked]
//
NTSTATUS PciFdoIrpSurpriseRemoval(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	NTSTATUS Status										= PciBeginStateTransition(&FdoExt->Common,PciSurpriseRemoved);
	ASSERT(NT_SUCCESS(Status));

	PciCommitStateTransition(&FdoExt->Common,PciSurpriseRemoved);

	return PciBeginStateTransition(&FdoExt->Common,PciDeleted);
}

//
// query legacy bus info [checked]
//
NTSTATUS PciFdoIrpQueryLegacyBusInformation(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	return PciQueryLegacyBusInformation(FdoExt,static_cast<PLEGACY_BUS_INFORMATION*>(static_cast<PVOID>(&Irp->IoStatus.Information)));
}