//********************************************************************
//	created:	22:7:2008   23:32
//	file:		pci.dispatch.cpp
//	author:		tiamo
//	purpose:	dispatch
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciCallDownIrpStack)

//
// common dispatch [checked]
//
NTSTATUS PciDispatchIrp(__in PDEVICE_OBJECT DeviceObject,__in PIRP Irp)
{
	PPCI_COMMON_EXTENSION CommonExtension				= static_cast<PPCI_COMMON_EXTENSION>(DeviceObject->DeviceExtension);
	PIO_STACK_LOCATION IrpSp							= IoGetCurrentIrpStackLocation(Irp);

	ASSERT(CommonExtension->ExtensionType == PciPdoExtensionType || CommonExtension->ExtensionType == PciFdoExtensionType);

	//
	// if the device has gone,complete the irp with no_such_device
	//
	if(CommonExtension->DeviceState == PciDeleted)
	{
		Irp->IoStatus.Status							= STATUS_NO_SUCH_DEVICE;
	
		if(IrpSp->MajorFunction == IRP_MJ_POWER)
			PoStartNextPowerIrp(Irp);

		IoCompleteRequest(Irp,IO_NO_INCREMENT);
		return STATUS_NO_SUCH_DEVICE;
	}

	//
	// get style and dispatch function according to major minor code
	//
	PCI_DISPATCH_STYLE Style							= IRP_COMPLETE;
	PCI_DISPATCH_ROUTINE fnDispatch						= 0;
	ULONG MaxFunction									= MAXULONG;
	ULONG Index											= IrpSp->MinorFunction;

	if(IrpSp->MajorFunction == IRP_MJ_POWER)
	{
		//
		// bound check
		//
		if(IrpSp->MinorFunction > CommonExtension->IrpDispatchTable->PowerIrpMaximumMinorFunction)
			Index										= CommonExtension->IrpDispatchTable->PowerIrpMaximumMinorFunction + 1;

		Style											= CommonExtension->IrpDispatchTable->PowerIrpDispatchTable[Index].DispatchStyle;
		fnDispatch										= CommonExtension->IrpDispatchTable->PowerIrpDispatchTable[Index].DispatchFunction;
		MaxFunction										= CommonExtension->IrpDispatchTable->PowerIrpMaximumMinorFunction;
	}
	else if(IrpSp->MajorFunction == IRP_MJ_PNP)
	{
		//
		// bound check
		//
		if(IrpSp->MinorFunction > CommonExtension->IrpDispatchTable->PnpIrpMaximumMinorFunction)
			Index										= CommonExtension->IrpDispatchTable->PnpIrpMaximumMinorFunction + 1;

		Style											= CommonExtension->IrpDispatchTable->PnpIrpDispatchTable[Index].DispatchStyle;
		fnDispatch										= CommonExtension->IrpDispatchTable->PnpIrpDispatchTable[Index].DispatchFunction;
		MaxFunction										= CommonExtension->IrpDispatchTable->PnpIrpMaximumMinorFunction;
	}
	else if(IrpSp->MajorFunction == IRP_MJ_SYSTEM_CONTROL)
	{
		Style											= CommonExtension->IrpDispatchTable->SystemControlIrpDispatchStyle;
		fnDispatch										= CommonExtension->IrpDispatchTable->SystemControlIrpDispatchFunction;
	}
	else
	{
		Style											= CommonExtension->IrpDispatchTable->OtherIrpDispatchStyle;
		fnDispatch										= CommonExtension->IrpDispatchTable->OtherIrpDispatchFunction;
	}

	//
	// check debug break on irp
	//
	if(PciDebugIrpDispatchDisplay(IrpSp,CommonExtension,MaxFunction))
		DbgBreakPoint();

	//
	// pass it down first
	//
	if(Style == IRP_UPWARD)
		PciCallDownIrpStack(CommonExtension,Irp);

	//
	// call dispatch routine
	//
	NTSTATUS Status										= fnDispatch(Irp,IrpSp,CommonExtension);

	//
	// this irp is pending or the dispatch routine has already handle it
	//
	if(Status == STATUS_PENDING || Style == IRP_DISPATCH)
		return Status;

	//
	// dispatch routine return a status other than not_supported,
	// save it in the irp
	//
	if(Status != STATUS_NOT_SUPPORTED)
		Irp->IoStatus.Status							= Status;

	switch(Style)
	{
		//
		// pass it down if and only if dispatch routine return a success status
		//
	case IRP_DOWNWARD:
		{
			if(NT_SUCCESS(Status) || Status == STATUS_NOT_SUPPORTED)
			{
				Status									= PciPassIrpFromFdoToPdo(CommonExtension,Irp);
				break;
			}

			//
			// fall through
			//
		}

		//
		// complete the irp
		//
	case IRP_UPWARD:
	case IRP_COMPLETE:
		{
			if(IrpSp->MajorFunction == IRP_MJ_POWER)
				PoStartNextPowerIrp(Irp);

			Status										= Irp->IoStatus.Status;
			IoCompleteRequest(Irp,IO_NO_INCREMENT);
		}
		break;

	default:
		ASSERT(FALSE);
		break;
	}

	return Status;
}

//
// call down stack [checked]
//
NTSTATUS PciCallDownIrpStack(__in PPCI_COMMON_EXTENSION CommonExtension,__in PIRP Irp)
{
	PAGED_CODE();

	PciDebugPrintf(1,"PciCallDownIrpStack ...\n");

	ASSERT(CommonExtension->ExtensionType == PciFdoExtensionType);

	KEVENT Event;
	KeInitializeEvent(&Event,SynchronizationEvent,FALSE);

	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp,&PciSetEventCompletion,&Event,TRUE,TRUE,TRUE);

	NTSTATUS Status										= IoCallDriver(CONTAINING_RECORD(CommonExtension,PCI_FDO_EXTENSION,Common)->AttachedDeviceObject,Irp);
	if(Status == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,0);
		Status											= Irp->IoStatus.Status;
	}

	return Status;
}

//
// pass irp [checked]
//
NTSTATUS PciPassIrpFromFdoToPdo(__in PPCI_COMMON_EXTENSION CommonExtension,__in PIRP Irp)
{
	PciDebugPrintf(1,"Pci PassIrp ...\n");

	ASSERT(CommonExtension->ExtensionType == PciFdoExtensionType);

	if(IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_POWER)
	{
		IoCopyCurrentIrpStackLocationToNext(Irp);

		PoStartNextPowerIrp(Irp);

		return PoCallDriver(CONTAINING_RECORD(CommonExtension,PCI_FDO_EXTENSION,Common)->AttachedDeviceObject,Irp);
	}

	IoSkipCurrentIrpStackLocation(Irp);

	return IoCallDriver(CONTAINING_RECORD(CommonExtension,PCI_FDO_EXTENSION,Common)->AttachedDeviceObject,Irp);
}

//
// set event [checked]
//
NTSTATUS PciSetEventCompletion(__in PDEVICE_OBJECT DeviceObject,__in PIRP Irp,__in PVOID Context)
{
	ASSERT(Context);

	KeSetEvent(static_cast<PKEVENT>(Context),IO_NO_INCREMENT,FALSE);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

//
// unsupported [checked]
//
NTSTATUS PciIrpNotSupported(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PVOID CommonExtension)
{
	return STATUS_NOT_SUPPORTED;
}

//
// invalid request [checked]
//
NTSTATUS PciIrpInvalidDeviceRequest(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PVOID CommonExtension)
{
	return STATUS_INVALID_DEVICE_REQUEST;
}