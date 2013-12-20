#include "Utils.h"
#include <Ntdddisk.h>

IO_COMPLETION_ROUTINE QueryVolumeCompletion;
NTSTATUS QueryVolumeCompletion (PDEVICE_OBJECT DeviceObject,
								PIRP Irp,
								PVOID Context)
{
	PMDL mdl, nextMdl;
	UNREFERENCED_PARAMETER(DeviceObject);

	KeSetEvent((PKEVENT)Context, (KPRIORITY)0, FALSE);
	if(Irp->AssociatedIrp.SystemBuffer && (Irp->Flags & IRP_DEALLOCATE_BUFFER) ) {
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

NTSTATUS DiskFilter_QueryVolumeInfo(PDEVICE_OBJECT DeviceObject)
{
#define FAT16_SIG_OFFSET	54
#define FAT32_SIG_OFFSET	82
#define NTFS_SIG_OFFSET		3

#define DBR_LENGTH			512
	//	File system signature
	const UCHAR FAT16FLG[4] = {'F','A','T','1'};
	const UCHAR FAT32FLG[4] = {'F','A','T','3'};
	const UCHAR NTFSFLG[4] = {'N','T','F','S'};
	NTSTATUS Status = STATUS_SUCCESS;
	UCHAR DBR[DBR_LENGTH] = {0};

	PDP_NTFS_BOOT_SECTOR pNtfsBootSector = (PDP_NTFS_BOOT_SECTOR)DBR;
	PDP_FAT32_BOOT_SECTOR pFat32BootSector = (PDP_FAT32_BOOT_SECTOR)DBR;
	PDP_FAT16_BOOT_SECTOR pFat16BootSector = (PDP_FAT16_BOOT_SECTOR)DBR;

	LARGE_INTEGER readOffset = { 0 };	//	Read IRP offsets.
	IO_STATUS_BLOCK ios;
	PIRP   Irp	= NULL;
	KEVENT Event;
	PARTITION_INFORMATION PartitionInfo;
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	KdPrint((": DiskFilter_QueryVolumeInfo: Enter\n"));
	// Build IRP to get PartitionLength
	KeInitializeEvent(&Event, NotificationEvent, FALSE);
	Irp = IoBuildDeviceIoControlRequest(
		IOCTL_DISK_GET_PARTITION_INFO,
		DevExt->PhysicalDeviceObject,
		NULL,
		0,
		&PartitionInfo,
		sizeof(PARTITION_INFORMATION),
		FALSE,
		&Event,
		&ios
	);
	if (NULL == Irp)
	{
		KdPrint(("Build IOCTL IRP failed!\n"));
		Status = STATUS_UNSUCCESSFUL;
		return Status;
	}
	if (IoCallDriver(DevExt->PhysicalDeviceObject, Irp) == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
		if (!NT_SUCCESS(Irp->IoStatus.Status))
		{
			KdPrint(("Forward IOCTL IRP failed!\n"));
			Status = STATUS_UNSUCCESSFUL;
			goto ERROUT;
		}
	}
	DevExt->TotalSize = PartitionInfo.PartitionLength;

	Status = IoDoRequestSync (
		IRP_MJ_READ,
		DevExt->PhysicalDeviceObject,
		DBR,
		DBR_LENGTH,
		&readOffset,
		TRUE);
	if (!NT_SUCCESS(Status))
	{
		DbgPrint("Forward IRP failed!\n");
		goto ERROUT;
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
		//	Unknown file system.
		KdPrint(("file system can't be recongnized\n"));
		Status = STATUS_UNSUCCESSFUL;
	}

	KdPrint((": Sector = %d, Cluster = %d, Total = %I64d\n",
				DevExt->SectorSize, DevExt->ClusterSize, DevExt->TotalSize));

ERROUT:
	return Status;
}
