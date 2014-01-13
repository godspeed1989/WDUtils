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
	DBG_PRINT(DBG_TRACE_ROUTINES, ("DiskFilter_DispatchPower: "));
	if (IrpSp->Parameters.Power.Type == SystemPowerState)
	{
		DBG_PRINT(DBG_TRACE_OPS, ("SystemPowerState...\n"));
		if (PowerSystemShutdown == IrpSp->Parameters.Power.State.SystemState)
		{
			KdPrint(("System is shutting down...\n"));
			// ... Flush back Cache
		}
	}
	else if (IrpSp->Parameters.Power.Type == DevicePowerState)
		DBG_PRINT(DBG_TRACE_OPS, ("DevicePowerState...\n"));
	else
		DBG_PRINT(DBG_TRACE_OPS, ("\n"));

#if WINVER < _WIN32_WINNT_VISTA
	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return PoCallDriver(DevExt->LowerDeviceObject, Irp);
#else
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
#endif
}

static BOOLEAN IsProtectedVolume(PDEVICE_OBJECT DeviceObject)
{
	ULONG i;
	BOOLEAN bIsProtected = FALSE;
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PDISKFILTER_DRIVER_EXTENSION DrvExt = (PDISKFILTER_DRIVER_EXTENSION)
		IoGetDriverObjectExtension(DeviceObject->DriverObject, DISKFILTER_DRIVER_EXTENSION_ID);

#if WINVER > _WIN32_WINNT_WINXP
	if (!KeAreAllApcsDisabled())
#else
	while (!KeAreApcsDisabled());
#endif
	{
		if (NT_SUCCESS(IoVolumeDeviceToDosName(DevExt->PhysicalDeviceObject, &DevExt->VolumeDosName)))
		{
			KdPrint((": [%wZ] Online\n", &DevExt->VolumeDosName));
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
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
	PAGED_CODE();

	switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_VOLUME_ONLINE:
		DBG_PRINT(DBG_TRACE_OPS, ("DiskFilter_DispatchControl: IOCTL_VOLUME_ONLINE\n"));
		if (IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp) &&
			NT_SUCCESS(Irp->IoStatus.Status) &&
			IsProtectedVolume(DeviceObject) &&
			NT_SUCCESS(DiskFilter_QueryVolumeInfo(DeviceObject))
			)
		{
			KdPrint((": Protected\n"));
			InitCachePool(&DevExt->CachePool);
			DevExt->bIsProtectedVolume = TRUE;
		}
		Status = Irp->IoStatus.Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
	case IOCTL_VOLUME_OFFLINE:
		DBG_PRINT(DBG_TRACE_OPS, ("DiskFilter_DispatchControl: IOCTL_VOLUME_OFFLINE\n"));
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
	// For handling paging requests.
	BOOLEAN setPageable;
	BOOLEAN bAddPageFile;

	PAGED_CODE();
	switch(IrpSp->MinorFunction)
	{
	case IRP_MN_START_DEVICE:
		DBG_PRINT(DBG_TRACE_OPS, ("Pnp: Start Device...\n"));
		status = Irp->IoStatus.Status;
		DevExt->CurrentPnpState = IRP_MN_START_DEVICE;
		break;
	case IRP_MN_DEVICE_USAGE_NOTIFICATION :
		setPageable = FALSE;
		bAddPageFile = IrpSp->Parameters.UsageNotification.InPath;
		DBG_PRINT(DBG_TRACE_OPS, ("Pnp: Paging file request...\n"));
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
				IoAdjustPagingPathCount(&DevExt->PagingCount, bAddPageFile);
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
			KeSetEvent(&DevExt->PagingCountEvent, IO_NO_INCREMENT, FALSE);
			status = Irp->IoStatus.Status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return status;
		}
		break;
	case IRP_MN_REMOVE_DEVICE:
		DBG_PRINT(DBG_TRACE_OPS, ("Removing device ...\n"));
		IoForwardIrpSynchronously(DeviceObject, Irp);
		status = Irp->IoStatus.Status;
		if (NT_SUCCESS(status))
		{
			DevExt->bIsProtectedVolume = FALSE;
			//TODO: Flush back cache ...
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
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

static NTSTATUS
CompletionRoutine_2(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
{
	if (Irp->PendingReturned == TRUE)
	{
		KeSetEvent ((PKEVENT) Context, IO_NO_INCREMENT, FALSE);
	}
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
ForwardIrpSynchronously (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	KEVENT   event;
	NTSTATUS status;

	KeInitializeEvent(&event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine (Irp,
							CompletionRoutine_2,
							&event,
							TRUE, TRUE, TRUE);
    status = IoCallDriver(DeviceObject, Irp);

    if (status == STATUS_PENDING)
	{
		KeWaitForSingleObject (&event,
								Executive,// WaitReason
								KernelMode,// must be Kernelmode to prevent the stack getting paged out
								FALSE,
								NULL// indefinite wait
								);
		status = Irp->IoStatus.Status;
	}
	return status;
}

NTSTATUS DiskFilter_DispatchReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	LONGLONG						Offset;
	ULONG							Length;
	PUCHAR							SysBuf;
	PIO_STACK_LOCATION				IrpSp;
	PDISKFILTER_DEVICE_EXTENSION	DevExt;
	PAGED_CODE();

	DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	if (DevExt->bIsProtectedVolume)
	{
		IrpSp = IoGetCurrentIrpStackLocation(Irp);
		// Get system buffer address
		if (Irp->MdlAddress != NULL)
			SysBuf = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
		else
			SysBuf = (PUCHAR)Irp->UserBuffer;
		if (SysBuf == NULL)
			SysBuf = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;

		// Get offset and length
		if (IRP_MJ_READ == IrpSp->MajorFunction)
		{
			Offset = IrpSp->Parameters.Read.ByteOffset.QuadPart;
			Length = IrpSp->Parameters.Read.Length;
		}
		else if (IRP_MJ_WRITE == IrpSp->MajorFunction)
		{
			Offset = IrpSp->Parameters.Write.ByteOffset.QuadPart;
			Length = IrpSp->Parameters.Write.Length;
		}
		else
		{
			Offset = 0;
			Length = 0;
		}

		if (!SysBuf || !Length) // Ignore this IRP.
		{
			IoSkipCurrentIrpStackLocation(Irp);
			return IoCallDriver(DevExt->LowerDeviceObject, Irp);
		}

		if (IrpSp->MajorFunction == IRP_MJ_READ)
			DBG_PRINT(DBG_TRACE_RW, ("%u-%u: R off(%I64d) len(%x)\n",
				DevExt->DiskNumber, DevExt->PartitionNumber, Offset, Length));
		else
			DBG_PRINT(DBG_TRACE_RW, ("%u-%u: W off(%I64d) len(%x)\n",
				DevExt->DiskNumber, DevExt->PartitionNumber, Offset, Length));
		// Read Request
		if (IrpSp->MajorFunction == IRP_MJ_READ)
		{
			// Cache Full Matched
			if (QueryAndCopyFromCachePool(
					&DevExt->CachePool,
					SysBuf,
					Offset,
					Length) == TRUE)
			{
				KdPrint(("^^^^cache hit^^^^\n"));
				Irp->IoStatus.Status = STATUS_SUCCESS;
				Irp->IoStatus.Information = Length;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);
				return STATUS_SUCCESS;
			}
			else
			{
				Irp->IoStatus.Information = 0;
				Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				ForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
				if (NT_SUCCESS(Irp->IoStatus.Status))
				{
					UpdataCachePool(&DevExt->CachePool,
									SysBuf,
									Offset,
									Length,
									_READ_);
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					return Irp->IoStatus.Status;
				}
				else
				{
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					return Irp->IoStatus.Status;
				}
			}
		}
		// Write Request
		else
		{
			Irp->IoStatus.Information = 0;
			Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			ForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
			if (NT_SUCCESS(Irp->IoStatus.Status))
			{
				UpdataCachePool(&DevExt->CachePool,
								SysBuf,
								Offset,
								Length,
								_WRITE_);
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);
				return Irp->IoStatus.Status;
			}
			else
			{
				Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);
				return Irp->IoStatus.Status;
			}
		}
	}
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}
