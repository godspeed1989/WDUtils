#include "DiskFilter.h"
#include "Utils.h"

NTSTATUS DiskFilter_DispatchDefault(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PAGED_CODE();
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

NTSTATUS DiskFilter_DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

	PAGED_CODE();
	DbgPrint("DiskFilter_DispatchPower: ");
	if (IrpSp->Parameters.Power.Type == SystemPowerState)
	{
		DbgPrint("SystemPowerState...\n");
		if (PowerSystemShutdown == IrpSp->Parameters.Power.State.SystemState)
		{
			DbgPrint("System is shutting down...\n");
			// ... Flush back Cache
		}
	}
	else if (IrpSp->Parameters.Power.Type == DevicePowerState)
		DbgPrint("DevicePowerState...\n");
	else
		DbgPrint("\n");

#if WINVER<_WIN32_WINNT_VISTA
	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return PoCallDriver(DevExt->LowerDeviceObject, Irp);
#else
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
#endif
}

NTSTATUS DiskFilter_DispatchReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PAGED_CODE();

	if (DevExt->bIsProtectedVolume)
	{
		IoMarkIrpPending(Irp);
		// Queue this IRP.
		ExInterlockedInsertTailList(&DevExt->RwList,
			&Irp->Tail.Overlay.ListEntry, &DevExt->RwSpinLock);
		// Set Event.
		KeSetEvent(&DevExt->RwThreadEvent, (KPRIORITY)0, FALSE);
		return STATUS_PENDING;
	}
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

static BOOLEAN IsProtectedVolume(PDEVICE_OBJECT DeviceObject)
{
	BOOLEAN bIsProtected = FALSE;
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	ULONG i;
	PDISKFILTER_DRIVER_EXTENSION DrvExt = (PDISKFILTER_DRIVER_EXTENSION)
		IoGetDriverObjectExtension(DeviceObject->DriverObject, DISKFILTER_DRIVER_EXTENSION_ID);

#if WINVER > _WIN32_WINNT_WINXP
	//if (!KeAreAllApcsDisabled())
#else
	//while (!KeAreApcsDisabled());
#endif
	{
		if (NT_SUCCESS(IoVolumeDeviceToDosName(DevExt->PhysicalDeviceObject, &DevExt->VolumeDosName)))
		{
			DbgPrint(": [%wZ] Online\n", &DevExt->VolumeDosName);
			for (i = 0; DrvExt->ProtectedVolumes[i]; ++i)
			{
				if (DrvExt->ProtectedVolumes[i] == DevExt->VolumeDosName.Buffer[0])
				{
					bIsProtected = TRUE;
				}
			}
		}
	}
	return bIsProtected;
}

NTSTATUS DiskFilter_DispatchControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS Status;
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)
		DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
	PAGED_CODE();

	switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_VOLUME_ONLINE:
		DbgPrint("DiskFilter_DispatchControl: IOCTL_VOLUME_ONLINE\n");
		if (IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp) &&
			NT_SUCCESS(Irp->IoStatus.Status) &&
			IsProtectedVolume(DeviceObject) &&
			NT_SUCCESS(DiskFilter_QueryVolumeInfo(DeviceObject)) &&
			NT_SUCCESS(DiskFilter_InitBitMapAndCreateThread(DevExt)) // create thread
			)
		{
			DbgPrint(": Protected\n");
			DevExt->bIsProtectedVolume = TRUE;
		}
		Status = Irp->IoStatus.Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
	case IOCTL_VOLUME_OFFLINE:
		DbgPrint("DiskFilter_DispatchControl: IOCTL_VOLUME_OFFLINE\n");
		// ... Flush back Cache
		break;
	}

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

NTSTATUS DiskFilter_DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS status;
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

	//	For handling paging requests.
	BOOLEAN setPageable;
	BOOLEAN bAddPageFile;

	PAGED_CODE();

	switch(IrpSp->MinorFunction)
	{
	case IRP_MN_START_DEVICE:
		DbgPrint("Pnp: Start Device...\n");
		status = Irp->IoStatus.Status;
		DevExt->CurrentPnpState = IRP_MN_START_DEVICE;
		IoSkipCurrentIrpStackLocation(Irp);
		break;
	case IRP_MN_DEVICE_USAGE_NOTIFICATION :
		setPageable = FALSE;
		bAddPageFile = IrpSp->Parameters.UsageNotification.InPath;
		DbgPrint("Pnp: Paging file request...\n");
		if (IrpSp->Parameters.UsageNotification.Type == DeviceUsageTypePaging)
		//	Indicated it will create or delete a paging file.
		{
			if(bAddPageFile && !DevExt->CurrentPnpState)
			{
				status = STATUS_DEVICE_NOT_READY;
				break;
			}

			//	Waiting other paging requests.
			KeWaitForSingleObject(&DevExt->PagingCountEvent,
				Executive, KernelMode,
				FALSE, NULL);

			//	Removing last paging device.
			if (!bAddPageFile && DevExt->PagingCount == 1 )
			{
				// The last paging file is no longer active.
				// Set the DO_POWER_PAGABLE bit before
				// forwarding the paging request down the
				// stack.
				if (!(DeviceObject->Flags & DO_POWER_INRUSH))
				{
					DeviceObject->Flags |= DO_POWER_PAGABLE;
					setPageable = TRUE;
				}
			}
			//	Waiting lower device complete.
			IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);

			if (NT_SUCCESS(Irp->IoStatus.Status))
			{
				IoAdjustPagingPathCount(&DevExt->PagingCount,
					bAddPageFile);
				if (bAddPageFile && DevExt->PagingCount == 1) {
					//	Once the lower device objects have succeeded the addition of the paging
					//	file, it is illegal to fail the request. It is also the time to clear
					//	the Filter DO's DO_POWER_PAGABLE flag.
					DeviceObject->Flags &= ~DO_POWER_PAGABLE;
				}
			}
			else
			{
				// F
				if (setPageable == TRUE) {
					DeviceObject->Flags &= ~DO_POWER_PAGABLE;
					setPageable = FALSE;
				}
			}
			// G
			KeSetEvent(&DevExt->PagingCountEvent,
				IO_NO_INCREMENT, FALSE);
			status = Irp->IoStatus.Status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return status;
		}
		break;
	case IRP_MN_REMOVE_DEVICE:
		DbgPrint("Removing device ...\n");
		IoForwardIrpSynchronously(DeviceObject, Irp);
		status = Irp->IoStatus.Status;
		if (NT_SUCCESS(status))
		{
			if (DevExt->RwThreadObject)
			{
				DevExt->bTerminalThread = TRUE;
				KeSetEvent(&DevExt->RwThreadEvent, (KPRIORITY)0, FALSE);
				KeWaitForSingleObject(DevExt->RwThreadObject, Executive, KernelMode, FALSE, NULL);
				ObDereferenceObject(DevExt->RwThreadObject);
			}
			if (DevExt->Bitmap.Buffer)
			{
				ExFreePoolWithTag(DevExt->Bitmap.Buffer, DISK_FILTER_TAG);
			}
			if (DevExt->LowerDeviceObject)
			{
				IoDetachDevice(DevExt->LowerDeviceObject);
			}
			IoDeleteDevice(DeviceObject);
			RtlFreeUnicodeString(&DevExt->VolumeDosName);
		}
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}
