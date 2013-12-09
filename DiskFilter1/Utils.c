#include "Utils.h"
#include <Ntdddisk.h>

NTSTATUS DiskFilter_QueryVolumeCompletion (PDEVICE_OBJECT DeviceObject,
										   PIRP Irp,
										   PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);
	KeSetEvent((PKEVENT)Context, (KPRIORITY)0, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
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

	// Build IRP to get DBR to get SectorSize
	KeInitializeEvent(&Event, NotificationEvent, FALSE);
	Irp = IoBuildAsynchronousFsdRequest(
		IRP_MJ_READ,
		DevExt->PhysicalDeviceObject,
		DBR,
		DBR_LENGTH,
		&readOffset,
		&ios
	);
	if (NULL == Irp)
	{
		KdPrint(("Build IRP failed!\n"));
		Status = STATUS_UNSUCCESSFUL;
		goto ERROUT;
	}
	IoSetCompletionRoutine(Irp, DiskFilter_QueryVolumeCompletion, &Event, TRUE, TRUE, TRUE);
	if (IoCallDriver(DevExt->PhysicalDeviceObject, Irp) == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
		if (!NT_SUCCESS(Irp->IoStatus.Status))
		{
			KdPrint(("Forward IRP failed!\n"));
			Status = STATUS_UNSUCCESSFUL;
			goto ERROUT;
		}
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
	if ((DevExt->LowerDeviceObject->Flags & DO_DIRECT_IO) &&
		(Irp->MdlAddress != NULL))
	{
		MmUnlockPages(Irp->MdlAddress);
	}

	IoFreeMdl(Irp->MdlAddress);
	IoFreeIrp(Irp);
	return Status;
}
