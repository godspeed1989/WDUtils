#include "DiskFilter.h"

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
	PAGED_CODE();
	DbgPrint("Enter Power Dispatch Routine...\n");

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
		DbgPrint("DiskFilter_DispatchControl: Enter\n");
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
	}

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

NTSTATUS DiskFilter_QueryVolumeCompletion (PDEVICE_OBJECT DeviceObject,
										   PIRP Irp,
										   PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);
	KeSetEvent((PKEVENT)Context, (KPRIORITY)0, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}
#pragma code_seg("PAGED")

static NTSTATUS DiskFilter_QueryVolumeInfo(PDEVICE_OBJECT DeviceObject)
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

static NTSTATUS DiskFilter_InitBitMapAndCreateThread(PDISKFILTER_DEVICE_EXTENSION DevExt)
{
	NTSTATUS Status;
	HANDLE hThread;
	DbgPrint(": DiskFilter_InitBitMapAndCreateThread: Enter\n");

	RtlInitializeBitMap(
		&DevExt->Bitmap,
		(PULONG)ExAllocatePoolWithTag(NonPagedPool,
			(ULONG)((DevExt->TotalSize.QuadPart / DevExt->SectorSize / 8 + 1) / sizeof(ULONG) * sizeof(ULONG)),
			DISK_FILTER_TAG), // Buffer, Size In Bytes.
		(ULONG)(DevExt->TotalSize.QuadPart / DevExt->SectorSize) // Number of bites.
	);
	if (DevExt->Bitmap.Buffer)
	{
		RtlClearAllBits(&DevExt->Bitmap);
		if (NT_SUCCESS(
				PsCreateSystemThread(&hThread,
				(ULONG)0, NULL, NULL, NULL,
				DiskFilter_ReadWriteThread, (PVOID)DevExt))
			)
		{
			//	Reference thread object.
			Status = ObReferenceObjectByHandle(
				hThread,
				THREAD_ALL_ACCESS,
				NULL,
				KernelMode,
				&DevExt->RwThreadObject,
				NULL
				);
			if (NT_SUCCESS(Status)) // Everything is OK.
			{
				ZwClose(hThread);
				return STATUS_SUCCESS;
			}

			//	Terminate thread.
			DevExt->bTerminalThread = TRUE;
			KeSetEvent(&DevExt->RwThreadEvent, (KPRIORITY)0, FALSE);

			ZwClose(hThread);
			return STATUS_UNSUCCESSFUL;
		}
		//	Create Thread failed. free bitmap.
		ExFreePoolWithTag(DevExt->Bitmap.Buffer, DISK_FILTER_TAG);
		DevExt->RwThreadObject = 0;
		DevExt->Bitmap.Buffer = 0;
	}
	//	Allocate bitmap fiailed!
	DbgPrint("Allocate bitmap failed !\n");
	return STATUS_UNSUCCESSFUL;
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
