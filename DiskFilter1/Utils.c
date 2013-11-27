#include "Utils.h"

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
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	DbgPrint(": DiskFilter_QueryVolumeInfo: Enter\n");
	// Build IRP to get DBR
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
		DbgPrint("Build IRP failed!\n");
		Status = STATUS_UNSUCCESSFUL;
		goto ERROUT;
	}

	IoSetCompletionRoutine(Irp, DiskFilter_QueryVolumeCompletion, &Event, TRUE, TRUE, TRUE);
	if (IoCallDriver(DevExt->PhysicalDeviceObject, Irp) == STATUS_PENDING)
	{
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
		if (!NT_SUCCESS(Irp->IoStatus.Status))
		{
			DbgPrint("Forward IRP failed!\n");
			Status = STATUS_UNSUCCESSFUL;
			goto ERROUT;
		}
	}

	// Distinguish the file system.
	if (*(ULONG32*)NTFSFLG == *(ULONG32*)&DBR[NTFS_SIG_OFFSET])
	{
		DbgPrint(": Current file system is NTFS\n");
		DevExt->SectorSize = pNtfsBootSector->BytesPerSector;
		DevExt->ClusterSize = DevExt->SectorSize * pNtfsBootSector->SectorsPerCluster;
		DevExt->TotalSize.QuadPart = DevExt->SectorSize * pNtfsBootSector->TotalSectors;
	}
	else if (*(ULONG32*)FAT32FLG == *(ULONG32*)&DBR[FAT32_SIG_OFFSET])
	{
		DbgPrint(": Current file system is FAT32\n");
		DevExt->SectorSize = pFat32BootSector->BytesPerSector;
		DevExt->ClusterSize = DevExt->SectorSize * pFat32BootSector->SectorsPerCluster;
		DevExt->TotalSize.QuadPart = (DevExt->SectorSize*
			pFat32BootSector->LargeSectors + pFat32BootSector->Sectors);
	}
	else if (*(ULONG32*)FAT16FLG == *(ULONG32*)&DBR[FAT16_SIG_OFFSET])
	{
		DbgPrint(": Current file system is FAT16\n");
		DevExt->SectorSize = pFat16BootSector->BytesPerSector;
		DevExt->ClusterSize = DevExt->SectorSize * pFat16BootSector->SectorsPerCluster;
		DevExt->TotalSize.QuadPart = DevExt->SectorSize *
			pFat16BootSector->LargeSectors + pFat16BootSector->Sectors;
	}
	else
	{
		//	Unknown file system.
		DbgPrint("file system can't be recongnized\n");
		Status = STATUS_UNSUCCESSFUL;
	}
ERROUT:

	DbgPrint(": Sector Size = %d, Volume Total size = Hi(%ld)Lo(%ld)\n",
		DevExt->SectorSize, DevExt->TotalSize.HighPart, DevExt->TotalSize.LowPart);

	if ((DevExt->LowerDeviceObject->Flags & DO_DIRECT_IO) &&
		(Irp->MdlAddress != NULL))
	{
		MmUnlockPages(Irp->MdlAddress);
	}

	IoFreeMdl(Irp->MdlAddress);
	IoFreeIrp(Irp);
	return Status;
}
