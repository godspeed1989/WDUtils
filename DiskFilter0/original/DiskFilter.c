#include "DiskFilter.h"
/*
此驱动默认还原C盘，缓冲文件放D盘（根据inf文件配置）。
此代码为课程设计而写，只实现了最基本的功能，很多细节问题并未考虑。另外，
对读写请求未进行过滤，还原系统盘时，Win7下测试，系统启动后会显示
“正常进入Windows”等选项，系统在关机时应该做了记录日志，被拦截后出
现此提示。在驱动安装后，对注册表的配置也会被还原，也就是说，安装后如果
想卸载。。。。。
*/

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject,
					 PUNICODE_STRING RegistryPath
					 )
{
	ULONG i;
	NTSTATUS Status;
	PDISKFILTER_DRIVER_EXTENSION	DrvExt = NULL;
	ULONG ClientId = DISKFILTER_DRIVER_EXTENSION_ID;
	PUNICODE_STRING ServiceKey;

	KdBreakPoint();

	for (i = 0; i!=IRP_MJ_MAXIMUM_FUNCTION; ++i)
	{
		DriverObject->MajorFunction[i] = DiskFilter_DispatchDefault;
	}

	DriverObject->MajorFunction[IRP_MJ_READ] = DiskFilter_DispatchReadWrite;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = DiskFilter_DispatchReadWrite;
	DriverObject->MajorFunction[IRP_MJ_POWER] = DiskFilter_DispatchPower;
	DriverObject->MajorFunction[IRP_MJ_PNP] = DiskFilter_DispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DiskFilter_DispatchControl;
	
	DriverObject->DriverExtension->AddDevice = DiskFilter_AddDevice;

	//
	//	Copy serivce key to driver extension.
	//

	Status = IoAllocateDriverObjectExtension(DriverObject, (PVOID)ClientId,
		sizeof(DISKFILTER_DRIVER_EXTENSION), (PVOID*)&DrvExt);
	ASSERT(NT_SUCCESS(Status));
	ClientId = DISKFILTER_DRIVER_EXTENSION_ID_UNICODE_BUFFER;
	Status = IoAllocateDriverObjectExtension(DriverObject, (PVOID)ClientId, 
		RegistryPath->Length + 1, (PVOID*)&DrvExt->RegistryUnicodeBuffer);
	ASSERT(NT_SUCCESS(Status));
	ServiceKey = &DrvExt->ServiceKeyName;
	ServiceKey->Buffer = DrvExt->RegistryUnicodeBuffer;
	ServiceKey->MaximumLength = RegistryPath->Length + 1;
	RtlCopyUnicodeString(ServiceKey, RegistryPath);

	DiskFilter_QueryConfig(DrvExt->ProtectedVolume, DrvExt->CacheVolume, MAX_PROTECTED_VOLUME, RegistryPath);

	IoRegisterBootDriverReinitialization(DriverObject, DiskFilter_DriverReinitializeRoutine, NULL);
	
	KdPrint(("Service key :\n%wZ\n", ServiceKey));
	return STATUS_SUCCESS;
}

NTSTATUS DiskFilter_QueryConfig( PWCHAR ProtectedVolume, 
							 PWCHAR CacheVolume,
							 USHORT MaxProtectedVolume,
							 PUNICODE_STRING RegistryPath
							)
{
	NTSTATUS Status;
	ULONG i;
	RTL_QUERY_REGISTRY_TABLE QueryTable[3 + 1] = {0};

	UNICODE_STRING ustrProtectedVolume;
	UNICODE_STRING ustrCacheVolume;

	ustrProtectedVolume.Buffer = ProtectedVolume;
	ustrProtectedVolume.MaximumLength = MaxProtectedVolume;
	
	ustrCacheVolume.Buffer = CacheVolume;
	ustrCacheVolume.MaximumLength = sizeof(WCHAR) * MAX_CACHE_VOLUME;


	QueryTable[0].Flags         = RTL_QUERY_REGISTRY_SUBKEY;
	QueryTable[0].Name          = L"Parameters";
	QueryTable[0].EntryContext  = NULL;
	QueryTable[0].DefaultType   = (ULONG_PTR)NULL;
	QueryTable[0].DefaultData   = NULL;
	QueryTable[0].DefaultLength = (ULONG_PTR)NULL;

	QueryTable[1].Flags         = RTL_QUERY_REGISTRY_DIRECT;
	QueryTable[1].Name          = L"ProtectedVolume";
	QueryTable[1].EntryContext  = &ustrProtectedVolume;
	QueryTable[1].DefaultType   = REG_SZ;
	QueryTable[1].DefaultData   = L"D";
	QueryTable[1].DefaultLength = 0;
	
	QueryTable[2].Flags         = RTL_QUERY_REGISTRY_DIRECT;
	QueryTable[2].Name          = L"CacheVolume";
	QueryTable[2].EntryContext  = &ustrCacheVolume;
	QueryTable[2].DefaultType   = REG_SZ;
	QueryTable[2].DefaultData   = L"C";
	QueryTable[2].DefaultLength = 0;

	Status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
		RegistryPath->Buffer,
		QueryTable,
		NULL,
		NULL
		);

	if (NT_SUCCESS(Status))
	{
		ProtectedVolume[ustrProtectedVolume.Length] = 0;
		//	Check Configuration.
		if (CacheVolume[0] < L'A' || CacheVolume[0] > L'Z')
		{
			return STATUS_UNSUCCESSFUL;
		}
		for (i=0; i!=ustrProtectedVolume.Length; ++i)
		{
			if (ProtectedVolume[i] < L'A' || ProtectedVolume[i] > L'Z' ||
				(ProtectedVolume[i+1] && ProtectedVolume[i] >= ProtectedVolume[i+1]) ||
				ProtectedVolume[i] == CacheVolume[0]
				)
			{
				KdPrint(("Check configuration failed!\n"));
				ProtectedVolume[ustrProtectedVolume.Length] = 0;
				return STATUS_UNSUCCESSFUL;
			}
		}
	}
	else
	{
		KdPrint(("Check configuration failed!\n"));
		ProtectedVolume[ustrProtectedVolume.Length] = 0;
		return STATUS_UNSUCCESSFUL;
	}
	return STATUS_SUCCESS;
}


VOID DiskFilter_DriverReinitializeRoutine (
	 PDRIVER_OBJECT DriverObject,
	 PVOID Context,
	 ULONG Count
	)
{
	NTSTATUS Status = STATUS_SUCCESS;
	OBJECT_ATTRIBUTES ObjAttr = {0};
	IO_STATUS_BLOCK ios = {0};
	FILE_END_OF_FILE_INFORMATION FileEndInfo = {0};
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	PDISKFILTER_DEVICE_EXTENSION DevExt;

	UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(Count);
	KdPrint(("Enter Driver Reinitialize...\n"));

	//
	//	Enumerate device.
	//
	for(; DeviceObject; DeviceObject = DeviceObject->NextDevice)
	{
		DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
		if (DevExt->bIsProtectedVolume)
		{
			KdPrint(("Creating Sparse file : %wZ\n", &DevExt->SparseFileName));
			InitializeObjectAttributes(
				&ObjAttr,
				&DevExt->SparseFileName, 
				OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
				NULL,
				NULL);

			Status = ZwCreateFile(
				&DevExt->hSparseFile,
				GENERIC_READ | GENERIC_WRITE,
				&ObjAttr,
				&ios,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				0,
				FILE_OVERWRITE_IF,
				FILE_NON_DIRECTORY_FILE |
				FILE_RANDOM_ACCESS |
				FILE_SYNCHRONOUS_IO_NONALERT |
				FILE_NO_INTERMEDIATE_BUFFERING,
				NULL,
				0);
			if (!NT_SUCCESS(Status))
			{
				KdPrint(("Create File Failed! Error Code = 0x%X\n", Status));
				DevExt->bIsProtectedVolume = FALSE;
				goto l_error;
			}
			Status = ZwFsControlFile(
				DevExt->hSparseFile,
				NULL,
				NULL,
				NULL,
				&ios,
				FSCTL_SET_SPARSE,
				NULL,
				0,
				NULL,
				0);
			if (!NT_SUCCESS(Status))
			{
				KdPrint(("Set Sparse file failed ! Error Code = 0x%X, File Handle = 0x%X\n", Status, DevExt->hSparseFile));
				DevExt->bIsProtectedVolume = FALSE;
				goto l_error;
			}
			//	Set the size of this sparse file as the volume we protected, and reserve size of 10MB.
			FileEndInfo.EndOfFile.QuadPart = DevExt->TotalSize.QuadPart + 10 * 1024 * 1024;
			Status = ZwSetInformationFile(
				DevExt->hSparseFile,
				&ios,
				&FileEndInfo,
				sizeof(FILE_END_OF_FILE_INFORMATION),
				FileEndOfFileInformation
				);
			if (!NT_SUCCESS(Status))
			{
				KdPrint(("Set file size failed ! Error Code = %d\n", Status));
				DevExt->bIsProtectedVolume = FALSE;
				goto l_error;
			}
		}
	}
	KdPrint(("Set Sparse file success ! \n"));
	return;
l_error:

	return;
}


BOOLEAN IsProtectedVolume(PDEVICE_OBJECT DeviceObject)
{
	BOOLEAN bIsProtected = FALSE;
	UNICODE_STRING VolumeDosName;
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	ULONG i;

	PDISKFILTER_DRIVER_EXTENSION DrvExt = (PDISKFILTER_DRIVER_EXTENSION)
		IoGetDriverObjectExtension(DeviceObject->DriverObject, DISKFILTER_DRIVER_EXTENSION_ID);

	PAGED_CODE();
#if WINVER > _WIN32_WINNT_WINXP
	if (!KeAreAllApcsDisabled())
#else
	while (!KeAreApcsDisabled());
#endif
	{
		if (NT_SUCCESS(IoVolumeDeviceToDosName(DevExt->PhysicalDeviceObject, &VolumeDosName)))
		{
			KdPrint(("Current Volume : %wZ Online\n", &VolumeDosName));

			for (i = 0; DrvExt->ProtectedVolume[i]; ++i)
			{
				if (DrvExt->ProtectedVolume[i] == VolumeDosName.Buffer[0])
				{
					DevExt->SparseFileName.Buffer[4] = DrvExt->CacheVolume[0];
					KdPrint(("Sparse file name : %wZ\n", &DevExt->SparseFileName));
					bIsProtected = TRUE;
				}
			}
			RtlFreeUnicodeString(&VolumeDosName);
		}	//	End of acquire volume letter.
	}
	return bIsProtected;
}

NTSTATUS DiskFilter_AddDevice(PDRIVER_OBJECT DriverObject,
							  PDEVICE_OBJECT PhysicalDeviceObject
							  )
{
	NTSTATUS Status;

	PDISKFILTER_DEVICE_EXTENSION DevExt;

	PDEVICE_OBJECT DeviceObject = NULL;
	PDEVICE_OBJECT LowerDeviceObject = NULL;
	UNICODE_STRING ModelSparseFileName;

	PAGED_CODE();
	KdPrint(("OK, Enter add device...\n"));

	//	Create device.
	Status = IoCreateDevice(DriverObject,
							sizeof(DISKFILTER_DEVICE_EXTENSION),
							NULL,
							PhysicalDeviceObject->DeviceType,
							FILE_DEVICE_SECURE_OPEN,
							FALSE,
							&DeviceObject
							);

	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Create device failed"));
	}
	
	DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	DevExt->bIsProtectedVolume = FALSE;
	
	//	Set sprase file.
	DevExt->SparseFileName.Buffer = DevExt->SparseFileBuffer;
	DevExt->SparseFileName.MaximumLength = MAX_SPARSE_FILE_NAME * 2;

	RtlInitUnicodeString(&ModelSparseFileName, MODEL_SPARSE_FILE_NAME);
	RtlCopyUnicodeString(&DevExt->SparseFileName, &ModelSparseFileName);

	DevExt->PhysicalDeviceObject = PhysicalDeviceObject;

	LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	if (LowerDeviceObject == NULL)
	{
		KdPrint(("Attach device failed...\n"));
		goto l_error;
	}

	//	Set device extension
	DevExt->LowerDeviceObject = LowerDeviceObject;
	KeInitializeEvent(&DevExt->PagingCountEvent, NotificationEvent, TRUE);
	
	InitializeListHead(&DevExt->RwList);
	KeInitializeSpinLock(&DevExt->RwSpinLock);
	KeInitializeEvent(&DevExt->RwThreadEvent, SynchronizationEvent, FALSE);
	DevExt->bTerminalThread = FALSE;
	DevExt->RwThreadObject = NULL;

	DeviceObject->Flags = (LowerDeviceObject->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO))| DO_POWER_PAGABLE;
	DeviceObject->Characteristics = LowerDeviceObject->Characteristics;
	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	return STATUS_SUCCESS;
l_error:
	if (LowerDeviceObject)
	{
		IoDetachDevice(LowerDeviceObject);
	}
	if (DeviceObject)
	{
		IoDeleteDevice(DeviceObject);
	}
	return STATUS_UNSUCCESSFUL;
}

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
	KdPrint(("Enter Power Dispatch Routine...\n"));

#if WINVER<_WIN32_WINNT_VISTA
	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return PoCallDriver(DevExt->LowerDeviceObject, Irp);
#else
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
#endif // WINVER>_WIN32_WINNT_VISTA_
}

NTSTATUS DiskFilter_DispatchReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)
		DeviceObject->DeviceExtension;
	//PEPROCESS Process;

	PAGED_CODE();

	if (DevExt->bIsProtectedVolume
		//&& !PsIsSystemThread(Irp->Tail.Overlay.Thread)
		)
	{
		IoMarkIrpPending(Irp);
		//	Queue this IRP.
		ExInterlockedInsertTailList(&DevExt->RwList, 
			&Irp->Tail.Overlay.ListEntry, &DevExt->RwSpinLock);
		//	Set Event.
		KeSetEvent(&DevExt->RwThreadEvent, (KPRIORITY)0, FALSE);
		return STATUS_PENDING;
	}
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
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
		if (IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp) &&
			NT_SUCCESS(Irp->IoStatus.Status) && 
			IsProtectedVolume(DeviceObject) &&
			NT_SUCCESS(DiskFilter_QueryVolumeInfo(DeviceObject)) &&
			NT_SUCCESS(DiskFilter_InitBitMapAndCreateThread(DevExt))
			)
		{
			DevExt->bIsProtectedVolume = TRUE;
			Status = Irp->IoStatus.Status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return Status;
		}
		Status = Irp->IoStatus.Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
	}

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

NTSTATUS DiskFilter_InitBitMapAndCreateThread(PDISKFILTER_DEVICE_EXTENSION DevExt)
{
	NTSTATUS Status;
	HANDLE hThread;
	RtlInitializeBitMap(&DevExt->Bitmap, 
		(PULONG)ExAllocatePoolWithTag(NonPagedPool,
		(ULONG)((DevExt->TotalSize.QuadPart / DevExt->SectorSize / 8 + 1) / sizeof(ULONG) * sizeof(ULONG)), //	Size In Bytes.
		DISK_FILTER_TAG), 
		(ULONG)(DevExt->TotalSize.QuadPart / DevExt->SectorSize)		//	Number of bites.
		);
	if (DevExt->Bitmap.Buffer)
	{
		RtlClearAllBits(&DevExt->Bitmap);
		if (NT_SUCCESS(
			PsCreateSystemThread(&hThread, 
			(ULONG)0, NULL, NULL, NULL, DiskFilter_ReadWriteThread, (PVOID)DevExt))
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
			if (NT_SUCCESS(Status))
			{
				//	Everything is OK.
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
	KdPrint(("Allocate bitmap failed !\n"));

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
		KdPrint(("Pnp: Start Device...\n"));
		status = Irp->IoStatus.Status;
		DevExt->CurrentPnpState = IRP_MN_START_DEVICE;
		IoSkipCurrentIrpStackLocation(Irp);
		break;
	case IRP_MN_DEVICE_USAGE_NOTIFICATION :
		setPageable = FALSE;
		bAddPageFile = IrpSp->Parameters.UsageNotification.InPath;
		KdPrint(("Pnp: Paging file request...\n"));
		if (IrpSp->Parameters.UsageNotification.Type == DeviceUsageTypePaging)
			//	Indicated it will create or delete a paging file.
		{
			//	
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
		KdPrint(("Removing device ...\n"));
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
		}
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}
	return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}


#pragma code_seg()
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

NTSTATUS DiskFilter_QueryVolumeInfo(PDEVICE_OBJECT DeviceObject)
{
#define _FileSystemNameLength	64
	//	The offset of file system sign.
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

	//	Distinguish the file system.
	if (*(ULONG32*)NTFSFLG == *(ULONG32*)&DBR[NTFS_SIG_OFFSET])
	{
		KdPrint(("Current file system is NTFS\n"));
		DevExt->SectorSize = pNtfsBootSector->BytesPerSector;
		DevExt->ClusterSize = DevExt->SectorSize * pNtfsBootSector->SectorsPerCluster;    
		DevExt->TotalSize.QuadPart = DevExt->SectorSize * pNtfsBootSector->TotalSectors;
	}
	else if (*(ULONG32*)FAT32FLG == *(ULONG32*)&DBR[FAT32_SIG_OFFSET])
	{
		KdPrint(("Current file system is FAT32\n"));
		DevExt->SectorSize = pFat32BootSector->BytesPerSector;
		DevExt->SectorSize = DevExt->SectorSize * pFat32BootSector->SectorsPerCluster;    
		DevExt->TotalSize.QuadPart = (DevExt->SectorSize* 
			pFat32BootSector->LargeSectors + pFat32BootSector->Sectors);
	}
	else if (*(ULONG32*)FAT16FLG == *(ULONG32*)&DBR[FAT16_SIG_OFFSET])
	{
		KdPrint(("Current file system is FAT16\n"));
		DevExt->SectorSize = pFat16BootSector->BytesPerSector;
		DevExt->ClusterSize = DevExt->SectorSize * pFat16BootSector->SectorsPerCluster;    
		DevExt->TotalSize.QuadPart = DevExt->SectorSize * 
			pFat16BootSector->LargeSectors + pFat16BootSector->Sectors;
	}
	else
	{
		//	Unknown file system.
		KdPrint(("file system can't be recongnized\n"));
		Status = STATUS_UNSUCCESSFUL;
	}
ERROUT:

	KdPrint(("Sector Size = %d, Volume Total size = Hi(%ld)Lo(%ld)\n", 
		DevExt->SectorSize, DevExt->TotalSize.HighPart, DevExt->TotalSize.LowPart));

	if (DevExt->LowerDeviceObject->Flags & DO_DIRECT_IO && 
		(Irp->MdlAddress != NULL))
	{
		MmUnlockPages(Irp->MdlAddress);
	}

	IoFreeMdl(Irp->MdlAddress);
	IoFreeIrp(Irp);

	return Status;
}

VOID DiskFilter_ReadWriteThread(PVOID Context)
{
	NTSTATUS Status;
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)Context;
	PLIST_ENTRY ReqEntry = NULL;
	PIRP Irp = NULL;
	PIO_STACK_LOCATION IrpSp;
	PUCHAR buf;
	LARGE_INTEGER Offset;
	ULONG Length;
	PUCHAR fileBuf;

	IO_STATUS_BLOCK ios;

	KdPrint(("Enter Read Write Thread\n"));

	//	set thread priority.
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
	for (; ;)
	{
		KeWaitForSingleObject(&DevExt->RwThreadEvent, 
			Executive, KernelMode, FALSE, NULL);
		if (DevExt->bTerminalThread)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
			return ;
		}

		while(NULL != (ReqEntry = ExInterlockedRemoveHeadList(
			&DevExt->RwList, &DevExt->RwSpinLock)))
		{
			Irp = CONTAINING_RECORD(ReqEntry, IRP, Tail.Overlay.ListEntry);
			IrpSp = IoGetCurrentIrpStackLocation(Irp);
			if (Irp->MdlAddress)
			{
				buf = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
			}
			else
			{
				buf = (PUCHAR)Irp->UserBuffer;
			}

			if (IRP_MJ_READ == IrpSp->MajorFunction)
			{
				Offset = IrpSp->Parameters.Read.ByteOffset;
				Length = IrpSp->Parameters.Read.Length;
			}
			else if (IRP_MJ_WRITE == IrpSp->MajorFunction)
			{
				Offset = IrpSp->Parameters.Write.ByteOffset;
				Length = IrpSp->Parameters.Write.Length;
			}
			else
			{
				Offset.QuadPart = 0;
				Length = 0;
			}

			if (!buf || !Length)	//	Ignore this IRP.
			{
				IoSkipCurrentIrpStackLocation(Irp);
				IoCallDriver(DevExt->LowerDeviceObject, Irp);
				continue;
			}

			//	Allocate file buffer.
			if (IrpSp->MajorFunction == IRP_MJ_READ)	//	Read Request.
			{
				//	redirect IO.
				if (RtlAreBitsClear(&DevExt->Bitmap, 
					(ULONG)(Offset.QuadPart/DevExt->SectorSize), 
					Length / DevExt->SectorSize))
				{
					IoSkipCurrentIrpStackLocation(Irp);
					IoCallDriver(DevExt->LowerDeviceObject, Irp);
					continue;
				}
				else if (RtlAreBitsSet(&DevExt->Bitmap, 
					(ULONG)(Offset.QuadPart / DevExt->SectorSize), 
					Length / DevExt->SectorSize))
				{
					Irp->IoStatus.Information = 0;
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

					fileBuf = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool,
						Length * DevExt->SectorSize, DISK_FILTER_TAG);
					if (fileBuf)
					{
						Status = ZwReadFile(DevExt->hSparseFile, NULL,
							NULL,
							NULL,
							&ios,
							fileBuf,
							Length,
							&Offset,
							NULL);
						if (NT_SUCCESS(Status))
						{
							RtlCopyMemory(buf, fileBuf, Length);
							Irp->IoStatus.Information = Length;
							Irp->IoStatus.Status = STATUS_SUCCESS;
						}
						ExFreePoolWithTag(fileBuf, DISK_FILTER_TAG);
						fileBuf = 0;
					}
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
				}
				else	//	The data location if mixed.
				{
					Irp->IoStatus.Information = 0;
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

					fileBuf = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool,
						Length * DevExt->SectorSize, DISK_FILTER_TAG);
					if (fileBuf)
					{
						IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
						if (NT_SUCCESS(Irp->IoStatus.Status))
						{
							DiskFilter_MergeBuffer(&DevExt->Bitmap,
								DevExt->SectorSize, &Offset, Length, buf, fileBuf);
							Irp->IoStatus.Status = STATUS_SUCCESS;
							Irp->IoStatus.Information = Length;
						}
					}
					ExFreePoolWithTag(fileBuf, DISK_FILTER_TAG);
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
				}
			}
			else	//	Write Request.
			{
				Status = ZwWriteFile(
					DevExt->hSparseFile,
					NULL,
					NULL,
					NULL,
					&ios,
					buf,
					Length,
					&Offset,
					NULL);
				if (NT_SUCCESS(Status) && 
					(Offset.QuadPart + Length < DevExt->TotalSize.QuadPart))
				{
					RtlSetBits(&DevExt->Bitmap, 
						(ULONG)(Offset.QuadPart / DevExt->SectorSize), 
						Length / DevExt->SectorSize);
					Irp->IoStatus.Information = Length;
					Irp->IoStatus.Status = STATUS_SUCCESS;
				}
				else
				{
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					Irp->IoStatus.Information = 0;
				}
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);
			}
		}	//	End of travel list.

	}	//	End waiting request.
	return;
}


VOID DiskFilter_MergeBuffer(PRTL_BITMAP Bitmap, 
							ULONG SectorSize,
							PLARGE_INTEGER Offset, 
							ULONG LengthInBytes, 
							PUCHAR DeviceBuffer,
							PUCHAR FileBuffer
							)
{
	ULONG StartBit = (ULONG)(Offset->QuadPart / SectorSize);
	ULONG Index = StartBit;
	ULONG EndBit = StartBit + LengthInBytes / SectorSize;
	
	for (; Index != EndBit; ++Index)
	{
		if (RtlCheckBit(Bitmap, Index))
		{
			RtlCopyMemory(DeviceBuffer + (Index - StartBit) * SectorSize, 
				FileBuffer + (Index - StartBit) * SectorSize,
				SectorSize);
		}
	}

}
