//********************************************************************
//	created:	25:7:2008   18:23
//	file:		pci.usage.cpp
//	author:		tiamo
//	purpose:	usage
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciLocalDeviceUsage)
#pragma alloc_text("PAGE",PciPdoDeviceUsage)

//
// usage notification [checked]
//
NTSTATUS PciLocalDeviceUsage(__in PPCI_POWER_STATE PowerState,__in PIRP Irp)
{
	PAGED_CODE();

	PIO_STACK_LOCATION IrpSp							= IoGetCurrentIrpStackLocation(Irp);
	LONG volatile* Value								= 0;

	switch(IrpSp->Parameters.UsageNotification.Type)
	{
	case DeviceUsageTypePaging:
		Value											= &PowerState->Paging;
		break;

	case DeviceUsageTypeDumpFile:
		Value											= &PowerState->CrashDump;
		break;

	case DeviceUsageTypeHibernation:
		Value											= &PowerState->Hibernate;
		break;

	default:
		return STATUS_NOT_SUPPORTED;
		break;
	}

	if(IrpSp->Parameters.UsageNotification.InPath)
	{
		LONG New										= InterlockedIncrement(Value);
		ASSERT(New > 0);
	}
	else
	{
		InterlockedDecrement(Value);
	}

	return STATUS_SUCCESS;
}

//
// pdo usage [checked]
//
NTSTATUS PciPdoDeviceUsage(__in PPCI_PDO_EXTENSION PdoExt,__in PIRP Irp)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;
	PDEVICE_OBJECT DeviceObject							= 0;

	__try
	{
		if(!PdoExt->ParentFdoExtension || !PdoExt->ParentFdoExtension->PhysicalDeviceObject)
			try_leave(Status = STATUS_SUCCESS);

		//
		// get parent's device stack
		//
		DeviceObject									= IoGetAttachedDeviceReference(PdoExt->ParentFdoExtension->PhysicalDeviceObject);
		if(!DeviceObject)
			try_leave(Status = STATUS_NO_SUCH_DEVICE);

		KEVENT Event;
		KeInitializeEvent(&Event,SynchronizationEvent,FALSE);

		//
		// build a pnp irp to notificate parent usage
		//
		IO_STATUS_BLOCK IoStatus;
		PIRP IrpNew										= IoBuildSynchronousFsdRequest(IRP_MJ_PNP,DeviceObject,0,0,0,&Event,&IoStatus);
		if(!IrpNew)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// pnp irp's default status value
		//
		IrpNew->IoStatus.Status							= STATUS_NOT_SUPPORTED;

		PIO_STACK_LOCATION NextIrpSp					= IoGetNextIrpStackLocation(IrpNew);
		PIO_STACK_LOCATION IrpSp						= IoGetCurrentIrpStackLocation(Irp);

		//
		// copy stack location,see IoCopyCurrentIrpStackLocationToNext
		//
		RtlCopyMemory(NextIrpSp,IrpSp,FIELD_OFFSET(IO_STACK_LOCATION,CompletionRoutine));
		NextIrpSp->Control								= 0;

		//
		// send to parent's device stack
		//
		Status											= IoCallDriver(DeviceObject,IrpNew);
		if(Status == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,0);
			Status										= IoStatus.Status;
		}
	}
	__finally
	{
		//
		// dereference parent's device stack
		//
		if(DeviceObject)
			ObDereferenceObject(DeviceObject);

		//
		// count pdo's usage
		//
		if(NT_SUCCESS(Status))
			Status										= PciLocalDeviceUsage(&PdoExt->PowerState,Irp);
	}

	return Status;
}