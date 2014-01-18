#include "Utils.h"
#include "DiskFilter.h"

IO_COMPLETION_ROUTINE QueryCompletion;
NTSTATUS QueryCompletion (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	PMDL mdl, nextMdl;
	UNREFERENCED_PARAMETER(DeviceObject);

	KeSetEvent((PKEVENT)Context, (KPRIORITY)0, FALSE);
	if(Irp->AssociatedIrp.SystemBuffer && (Irp->Flags & IRP_DEALLOCATE_BUFFER) )
	{
            ExFreePool(Irp->AssociatedIrp.SystemBuffer);
    }
	else if (Irp->MdlAddress != NULL)
	{
        for (mdl = Irp->MdlAddress; mdl != NULL; mdl = nextMdl)
		{
            nextMdl = mdl->Next;
            MmUnlockPages( mdl ); IoFreeMdl( mdl );
        }
        Irp->MdlAddress = NULL;
    }
	IoFreeIrp(Irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS IoDoRWRequestAsync (
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
		KdPrint(("%s: Build IRP failed!\n", __FUNCTION__));
		return STATUS_UNSUCCESSFUL;
	}
	IoSetCompletionRoutine(Irp, QueryCompletion, &Event, TRUE, TRUE, TRUE);

	IoCallDriver(DeviceObject, Irp);
	return STATUS_SUCCESS;
}

NTSTATUS IoDoRWRequestSync (
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
		KdPrint(("%s: Build IRP failed!\n", __FUNCTION__));
		return STATUS_UNSUCCESSFUL;
	}
	if (IoCallDriver(DeviceObject, Irp) == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
	}
	if (!NT_SUCCESS(Irp->IoStatus.Status))
	{
		KdPrint(("%s: %x Forward IRP failed!\n", __FUNCTION__, Irp->IoStatus.Status));
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
		KdPrint(("%s: Build IRP failed!\n", __FUNCTION__));
		return STATUS_UNSUCCESSFUL;
	}
	if (IoCallDriver(DeviceObject, Irp) == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
		if (!NT_SUCCESS(Irp->IoStatus.Status))
		{
			KdPrint(("%s: %x Forward IRP failed!\n", __FUNCTION__, Irp->IoStatus.Status));
			return STATUS_UNSUCCESSFUL;
		}
	}
	return STATUS_SUCCESS;
}

static NTSTATUS
DF_CreateRWThread(PDF_DEVICE_EXTENSION DevExt)
{
	NTSTATUS			Status;
	HANDLE				hThread;
	DBG_PRINT(DBG_TRACE_ROUTINES, (": %s: Enter\n", __FUNCTION__));

	Status = PsCreateSystemThread (
		&hThread,
		(ULONG)0, NULL, NULL, NULL,
		DF_ReadWriteThread,
		(PVOID)DevExt
	);

	if (NT_SUCCESS(Status))
	{
		// Reference thread object.
		Status = ObReferenceObjectByHandle(
			hThread,
			THREAD_ALL_ACCESS,
			NULL,
			KernelMode,
			&DevExt->RwThreadObject,
			NULL
		);
		if (NT_SUCCESS(Status))
		{
			ZwClose(hThread);
			return STATUS_SUCCESS;
		}
	}
	// Terminate thread
	KdPrint(("%s Failed\n", __FUNCTION__));
	DevExt->bTerminalThread = TRUE;
	KeSetEvent(&DevExt->RwThreadEvent, (KPRIORITY)0, FALSE);
	ZwClose(hThread);
	return STATUS_UNSUCCESSFUL;
}

VOID StartDevice(PDEVICE_OBJECT DeviceObject)
{
	PDF_DEVICE_EXTENSION	DevExt;
	DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	if (DeviceObject != g_pDeviceObject)
	{
		DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
		DF_QueryDeviceInfo(DeviceObject);
		if (NT_SUCCESS(DF_CreateRWThread(DevExt)))
		{
			InitCachePool(&DevExt->CachePool);
			KdPrint((": %p Start\n", DeviceObject));
		}
		KdPrint(("\n"));
	}
	DeviceObject = DeviceObject->NextDevice;
}

VOID StopDevice(PDEVICE_OBJECT DeviceObject)
{
	PDF_DEVICE_EXTENSION	DevExt;
	DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	DevExt->bIsProtected = FALSE;
	//TODO: Flush back cache ...
	if (DevExt->LowerDeviceObject)
	{
		IoDetachDevice(DevExt->LowerDeviceObject);
	}
	if (DevExt->RwThreadObject)
	{
		DevExt->bTerminalThread = TRUE;
		KeSetEvent(&DevExt->RwThreadEvent, (KPRIORITY)0, FALSE);
		KeWaitForSingleObject(DevExt->RwThreadObject, Executive, KernelMode, FALSE, NULL);
		ObDereferenceObject(DevExt->RwThreadObject);
	}
	DestroyCachePool(&DevExt->CachePool);
	IoDeleteDevice(DeviceObject);
}

NTSTATUS DF_QueryDeviceInfo(PDEVICE_OBJECT DeviceObject)
{
	NTSTATUS				Status;
	PDF_DEVICE_EXTENSION	DevExt;
	PARTITION_INFORMATION	PartitionInfo;
	VOLUME_DISK_EXTENTS		VolumeDiskExt;
#define FAT16_SIG_OFFSET	54
#define FAT32_SIG_OFFSET	82
#define NTFS_SIG_OFFSET		3
#define DBR_LENGTH			512
	// File system signature
	const UCHAR FAT16FLG[4] = {'F','A','T','1'};
	const UCHAR FAT32FLG[4] = {'F','A','T','3'};
	const UCHAR NTFSFLG[4] = {'N','T','F','S'};
	UCHAR DBR[DBR_LENGTH] = {0};
	LARGE_INTEGER readOffset = {0};
	PDP_NTFS_BOOT_SECTOR pNtfsBootSector = (PDP_NTFS_BOOT_SECTOR)DBR;
	PDP_FAT32_BOOT_SECTOR pFat32BootSector = (PDP_FAT32_BOOT_SECTOR)DBR;
	PDP_FAT16_BOOT_SECTOR pFat16BootSector = (PDP_FAT16_BOOT_SECTOR)DBR;

	DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	KdPrint((": %s: %p Enter\n", __FUNCTION__, DeviceObject));

	// Get Partition Length
	Status = IoDoIoctl (
		IOCTL_DISK_GET_PARTITION_INFO,
		DevExt->PhysicalDeviceObject,
		NULL,
		0,
		&PartitionInfo,
		sizeof(PARTITION_INFORMATION)
	);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Get Partition Length failed\n"));
		return Status;
	}
	DevExt->TotalSize = PartitionInfo.PartitionLength;
	DevExt->PartitionNumber = PartitionInfo.PartitionNumber;

	// Get Disk Number
	Status = IoDoIoctl (
		IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
		DevExt->PhysicalDeviceObject,
		NULL,
		0,
		&VolumeDiskExt,
		sizeof(VOLUME_DISK_EXTENTS)
	);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Get Disk Number failed\n"));
		return Status;
	}
	DevExt->DiskNumber = VolumeDiskExt.Extents[0].DiskNumber;

	// Read DBR
	Status = IoDoRWRequestSync (
		IRP_MJ_READ,
		DevExt->PhysicalDeviceObject,
		DBR,
		DBR_LENGTH,
		&readOffset
	);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Read DBR failed\n"));
		return Status;
	}

	// Distinguish the file system.
	if (*(ULONG32*)NTFSFLG == *(ULONG32*)&DBR[NTFS_SIG_OFFSET])
	{
		KdPrint((": Current file system is NTFS\n"));
		DevExt->SectorSize = pNtfsBootSector->BytesPerSector;
		DevExt->ClusterSize = DevExt->SectorSize * pNtfsBootSector->SectorsPerCluster;
	}
	else if (*(ULONG32*)FAT32FLG == *(ULONG32*)&DBR[FAT32_SIG_OFFSET])
	{
		KdPrint((": Current file system is FAT32\n"));
		DevExt->SectorSize = pFat32BootSector->BytesPerSector;
		DevExt->ClusterSize = DevExt->SectorSize * pFat32BootSector->SectorsPerCluster;
	}
	else if (*(ULONG32*)FAT16FLG == *(ULONG32*)&DBR[FAT16_SIG_OFFSET])
	{
		KdPrint((": Current file system is FAT16\n"));
		DevExt->SectorSize = pFat16BootSector->BytesPerSector;
		DevExt->ClusterSize = DevExt->SectorSize * pFat16BootSector->SectorsPerCluster;
	}
	else
	{
		KdPrint((": File system can't be recongnized\n"));
		DevExt->SectorSize = 512;
		DevExt->ClusterSize = 512 * 63;
	}
	KdPrint((": %u-%u Sector = %u, Cluster = %u, Total = %I64d\n",
		DevExt->DiskNumber, DevExt->PartitionNumber,
		DevExt->SectorSize, DevExt->ClusterSize, DevExt->TotalSize.QuadPart));

	return STATUS_SUCCESS;
}
