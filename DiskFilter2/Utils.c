#include "Utils.h"
#include "DiskFilter.h"

IO_COMPLETION_ROUTINE QueryVolumeCompletion;
NTSTATUS QueryVolumeCompletion (PDEVICE_OBJECT DeviceObject,
								PIRP Irp,
								PVOID Context)
{
	PMDL mdl, nextMdl;
	UNREFERENCED_PARAMETER(DeviceObject);

	KeSetEvent((PKEVENT)Context, (KPRIORITY)0, FALSE);
	if(Irp->AssociatedIrp.SystemBuffer && (Irp->Flags & IRP_DEALLOCATE_BUFFER) )
	{
            ExFreePool(Irp->AssociatedIrp.SystemBuffer);
    }
	else if (Irp->MdlAddress != NULL) {
        for (mdl = Irp->MdlAddress; mdl != NULL; mdl = nextMdl) {
            nextMdl = mdl->Next;
            MmUnlockPages( mdl ); IoFreeMdl( mdl );
        }
        Irp->MdlAddress = NULL;
    }
	IoFreeIrp(Irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS IoDoRequestAsync (
		ULONG			MajorFunction,
		PDEVICE_OBJECT  DeviceObject,
		PVOID 			Buffer,
		ULONG			Length,
		PLARGE_INTEGER	StartingOffset
	)
{
	PIRP   			Irp	= NULL;
	IO_STATUS_BLOCK	iosb;
	KEVENT			Event;

	KeInitializeEvent(&Event, NotificationEvent, FALSE);
	Irp = IoBuildAsynchronousFsdRequest (
		MajorFunction,
		DeviceObject,
		Buffer,
		Length,
		StartingOffset,
		&iosb
	);
	if (NULL == Irp)
	{
		DbgPrint("Build IRP failed!\n");
		return STATUS_UNSUCCESSFUL;
	}
	IoSetCompletionRoutine(Irp, QueryVolumeCompletion, &Event, TRUE, TRUE, TRUE);

	IoCallDriver(DeviceObject, Irp);
	return STATUS_SUCCESS;
}

NTSTATUS IoDoRequestSync (
		ULONG			MajorFunction,
		PDEVICE_OBJECT  DeviceObject,
		PVOID 			Buffer,
		ULONG			Length,
		PLARGE_INTEGER	StartingOffset
	)
{
	NTSTATUS		Status = STATUS_SUCCESS;
	PIRP   			Irp	= NULL;
	IO_STATUS_BLOCK	iosb;
	KEVENT			Event;

	KeInitializeEvent(&Event, NotificationEvent, FALSE);
	Irp = IoBuildSynchronousFsdRequest (
		MajorFunction,
		DeviceObject,
		Buffer,
		Length,
		StartingOffset,
		&Event,
		&iosb
	);
	if (NULL == Irp)
	{
		DbgPrint("Build IRP failed!\n");
		return STATUS_UNSUCCESSFUL;
	}

	if (IoCallDriver(DeviceObject, Irp) == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
	}
	if (!NT_SUCCESS(Irp->IoStatus.Status))
	{
		DbgPrint("Forward IRP failed!\n");
		Status = STATUS_UNSUCCESSFUL;
	}
	return Status;
}

NTSTATUS IoDoIoctl (
		ULONG			IoControlCode,
		PDEVICE_OBJECT	DeviceObject,
		PVOID			InputBuffer,
		ULONG			InputBufferLength,
		PVOID			OutputBuffer,
		ULONG			OutputBufferLength
	)
{
	PIRP				Irp;
	KEVENT				Event;
	IO_STATUS_BLOCK		ios;

	KeInitializeEvent(&Event, NotificationEvent, FALSE);
	Irp = IoBuildDeviceIoControlRequest (
		IoControlCode,
		DeviceObject,
		InputBuffer,
		InputBufferLength,
		OutputBuffer,
		OutputBufferLength,
		FALSE,
		&Event,
		&ios
	);
	if (NULL == Irp)
	{
		KdPrint(("Build IOCTL IRP failed!\n"));
		return STATUS_UNSUCCESSFUL;
	}
	if (IoCallDriver(DeviceObject, Irp) == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
		if (!NT_SUCCESS(Irp->IoStatus.Status))
		{
			KdPrint(("Forward IOCTL IRP failed!\n"));
			return STATUS_UNSUCCESSFUL;
		}
	}
	return STATUS_SUCCESS;
}

NTSTATUS DF_QueryVolumeInfo(PDEVICE_OBJECT DeviceObject)
{
	ULONG						i, nDiskBufferSize;
	NTSTATUS					Status = STATUS_SUCCESS;

	DISK_GEOMETRY				DiskGeo;
	GET_LENGTH_INFORMATION		LengthInfo;
	STORAGE_DEVICE_NUMBER		DeviceNumber;
	PDF_DEVICE_EXTENSION		DevExt;
	DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	KdPrint((": DF_QueryDiskInfo: %p Enter\n", DeviceObject));

	// Get Disk Layout Information
	nDiskBufferSize = sizeof(DRIVE_LAYOUT_INFORMATION) + sizeof(PARTITION_INFORMATION) * MAX_PARTITIONS_PER_DISK;
	DevExt->DiskLayout = (PDRIVE_LAYOUT_INFORMATION) DF_MALLOC (nDiskBufferSize);
	ASSERT(DevExt->DiskLayout != NULL);
	Status = IoDoIoctl (
		IOCTL_DISK_GET_DRIVE_LAYOUT,
		DevExt->PhysicalDeviceObject,
		NULL,
		0,
		DevExt->DiskLayout,
		nDiskBufferSize
	);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Get Disk Layout failed!\n"));
		goto ERROUT;
	}

	// Get Disk Geometry Information
	Status = IoDoIoctl (
		IOCTL_DISK_GET_DRIVE_GEOMETRY,
		DevExt->PhysicalDeviceObject,
		NULL,
		0,
		&DiskGeo,
		sizeof(DISK_GEOMETRY)
	);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Get Disk Length failed!\n"));
		goto ERROUT;
	}
	DevExt->SectorSize = DiskGeo.BytesPerSector;

	// Get Disk Length Information
	Status = IoDoIoctl (
		IOCTL_DISK_GET_LENGTH_INFO,
		DevExt->PhysicalDeviceObject,
		NULL,
		0,
		&LengthInfo,
		sizeof(GET_LENGTH_INFORMATION)
	);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Get Disk Length failed!\n"));
		goto ERROUT;
	}
	DevExt->TotalSize = LengthInfo.Length;

	// Get Disk Number
	Status = IoDoIoctl (
		IOCTL_STORAGE_GET_DEVICE_NUMBER,
		DevExt->PhysicalDeviceObject,
		NULL,
		0,
		&DeviceNumber,
		sizeof(STORAGE_DEVICE_NUMBER)
	);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Get Disk Number failed!\n"));
		goto ERROUT;
	}
	DevExt->DiskNumber = DeviceNumber.DeviceNumber;

	KdPrint((": disk%u Sector = %u, Total = %I64d\n",
			DevExt->DiskNumber, DevExt->SectorSize, DevExt->TotalSize));
	for (i = 0; i < DevExt->DiskLayout->PartitionCount; i++)
	{
		KdPrint((": disk%u\\partition%u off=%I64d len=%I64d\n",
				DevExt->DiskNumber, i,
				DevExt->DiskLayout->PartitionEntry[i].StartingOffset,
				DevExt->DiskLayout->PartitionEntry[i].PartitionLength));
	}
ERROUT:
	return Status;
}
