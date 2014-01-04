#include "DiskFilter.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject,
					 PUNICODE_STRING RegistryPath
					 )
{
	ULONG i;
	NTSTATUS Status;
	PDISKFILTER_DRIVER_EXTENSION DrvExt = NULL;
	ULONG ClientId;

	for (i = 0; i<=IRP_MJ_MAXIMUM_FUNCTION; ++i)
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
	ClientId = DISKFILTER_DRIVER_EXTENSION_ID;
	Status = IoAllocateDriverObjectExtension(DriverObject, (PVOID)ClientId,
		sizeof(DISKFILTER_DRIVER_EXTENSION), (PVOID*)&DrvExt);
	ASSERT(NT_SUCCESS(Status));

	ClientId = DISKFILTER_DRIVER_EXTENSION_ID_UNICODE_BUFFER;
	Status = IoAllocateDriverObjectExtension(DriverObject, (PVOID)ClientId,
		RegistryPath->Length + 1, (PVOID*)&DrvExt->RegistryUnicodeBuffer);
	ASSERT(NT_SUCCESS(Status));

	DrvExt->ServiceKeyName.Buffer = DrvExt->RegistryUnicodeBuffer;
	DrvExt->ServiceKeyName.MaximumLength = RegistryPath->Length + 1;
	RtlCopyUnicodeString(&DrvExt->ServiceKeyName, RegistryPath);

	DiskFilter_QueryConfig(DrvExt->ProtectedVolumes, DrvExt->CacheVolumes, RegistryPath);

	IoRegisterBootDriverReinitialization(DriverObject, DiskFilter_DriverReinitializeRoutine, NULL);

	KdPrint(("Service key :\n%wZ\n", &DrvExt->ServiceKeyName));
	return STATUS_SUCCESS;
}

NTSTATUS DiskFilter_QueryConfig( PWCHAR ProtectedVolume,
								 PWCHAR CacheVolume,
								 PUNICODE_STRING RegistryPath
								)
{
	NTSTATUS Status;
	ULONG i;
	RTL_QUERY_REGISTRY_TABLE QueryTable[3 + 1] = {0};

	UNICODE_STRING ustrProtectedVolume;
	UNICODE_STRING ustrCacheVolume;

	ustrProtectedVolume.Buffer = ProtectedVolume;
	ustrProtectedVolume.MaximumLength = sizeof(WCHAR) * MAX_PROTECTED_VOLUME;

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
		CacheVolume[ustrCacheVolume.Length] = 0;
		//	Check Configuration.
		for (i = 0; i != ustrCacheVolume.Length; ++i)
		{
			if (CacheVolume[i] < L'A' || CacheVolume[i] > L'Z')
			{
				return STATUS_UNSUCCESSFUL;
			}
		}
		//TODO: fix support ONLY one cache volume now
		for (i = 0; i != ustrProtectedVolume.Length; ++i)
		{
			if ( ProtectedVolume[i] < L'A' || ProtectedVolume[i] > L'Z' ||
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
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	PDISKFILTER_DEVICE_EXTENSION DevExt;

	UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(Count);
	KdPrint(("Start Reinitialize...\n"));

	//	Enumerate device.
	for(; DeviceObject; DeviceObject = DeviceObject->NextDevice)
	{
		DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
		KdPrint((" -> [%wZ]", &DevExt->VolumeDosName));
		if (DevExt->bIsProtectedVolume)
		{
			KdPrint((" Protected\n"));
		}
		else
		{
			KdPrint(("\n"));
		}
	}
}

NTSTATUS DiskFilter_AddDevice(PDRIVER_OBJECT DriverObject,
							  PDEVICE_OBJECT PhysicalDeviceObject
							  )
{
	NTSTATUS Status;

	PDISKFILTER_DEVICE_EXTENSION DevExt;

	PDEVICE_OBJECT DeviceObject = NULL;
	PDEVICE_OBJECT LowerDeviceObject = NULL;

	PAGED_CODE();
	KdPrint(("DiskFilter_AddDevice: Enter\n"));

	// Create a device
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
		goto l_error;
	}

	DevExt = (PDISKFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	DevExt->bIsProtectedVolume = FALSE;
	DevExt->PhysicalDeviceObject = PhysicalDeviceObject;

	LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	if (LowerDeviceObject == NULL)
	{
		KdPrint(("Attach device failed...\n"));
		goto l_error;
	}

	// Set device extension
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

NTSTATUS DiskFilter_InitCacheAndCreateThread(PDISKFILTER_DEVICE_EXTENSION DevExt)
{
	NTSTATUS Status;
	HANDLE hThread;
	KdPrint((": DiskFilter_InitBitMapAndCreateThread: Enter\n"));

	if (InitCachePool(&DevExt->CachePool) == FALSE)
	{
		KdPrint(("Initial Cache Pool Failed!\n"));
		return STATUS_UNSUCCESSFUL;
	}
	Status = PsCreateSystemThread (
				&hThread,
				(ULONG)0, NULL, NULL, NULL,
				DiskFilter_ReadWriteThread, (PVOID)DevExt
			);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("Create R/W Failed!\n"));
		goto l_error;
	}
	// Reference thread object.
	Status = ObReferenceObjectByHandle(
		hThread,
		THREAD_ALL_ACCESS,
		NULL,
		KernelMode,
		&DevExt->RwThreadObject,
		NULL
	);
	ZwClose(hThread);
	if (NT_SUCCESS(Status))
	{
		return STATUS_SUCCESS;
	}
l_error:
	// Terminate thread.
	DevExt->RwThreadObject = 0;
	DevExt->bTerminalThread = TRUE;
	KeSetEvent(&DevExt->RwThreadEvent, (KPRIORITY)0, FALSE);
	return STATUS_UNSUCCESSFUL;
}

VOID DiskFilter_ReadWriteThread(PVOID Context)
{
	PIRP Irp = NULL;
	PLIST_ENTRY ReqEntry = NULL;
	PIO_STACK_LOCATION IrpSp;
	PUCHAR SysBuf;
	LARGE_INTEGER Offset;
	ULONG Length;
	PDISKFILTER_DEVICE_EXTENSION DevExt = (PDISKFILTER_DEVICE_EXTENSION)Context;

	KdPrint((": Start Read Write Thread\n"));

	// set thread priority.
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
	for (;;)
	{
		KeWaitForSingleObject(&DevExt->RwThreadEvent,
			Executive, KernelMode, FALSE, NULL);
		if (DevExt->bTerminalThread)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
			return;
		}

		while(NULL != (ReqEntry = ExInterlockedRemoveHeadList(
						&DevExt->RwList, &DevExt->RwSpinLock)))
		{
			Irp = CONTAINING_RECORD(ReqEntry, IRP, Tail.Overlay.ListEntry);
			IrpSp = IoGetCurrentIrpStackLocation(Irp);
			// Get system buffer address
			if (Irp->MdlAddress)
			{
				SysBuf = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
			}
			else
			{
				SysBuf = (PUCHAR)Irp->UserBuffer;
			}
			// Get offset and length
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

			if (!SysBuf || !Length) // Ignore this IRP.
			{
				IoSkipCurrentIrpStackLocation(Irp);
				IoCallDriver(DevExt->LowerDeviceObject, Irp);
				continue;
			}
		#if 0
			if (IrpSp->MajorFunction == IRP_MJ_READ)
				KdPrint(("[R off(%I64d) len(%x)]\n", Offset.QuadPart, Length));
			else
				KdPrint(("[W off(%I64d) len(%x)]\n", Offset.QuadPart, Length));
		#endif
			// Read Request
			if (IrpSp->MajorFunction == IRP_MJ_READ)
			{
				// Cache Full Matched
				if (QueryAndCopyFromCachePool(
						&DevExt->CachePool,
						SysBuf,
						Offset.QuadPart,
						Length) == TRUE)
				{
					KdPrint(("^^^^cache hit^^^^\n"));
					KdPrint(("[R off(%I64d) len(%x)]\n", Offset.QuadPart, Length));
					Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = Length;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				}
				else
				{
					Irp->IoStatus.Information = 0;
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

					IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
					if (NT_SUCCESS(Irp->IoStatus.Status))
					{
						UpdataCachePool(&DevExt->CachePool,
										SysBuf,
										Offset.QuadPart,
										Length,
										_READ_);
						Irp->IoStatus.Status = STATUS_SUCCESS;
						Irp->IoStatus.Information = Length;
					}
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				}
			}
			// Write Request
			else
			{
				Irp->IoStatus.Information = 0;  
				Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;  
				IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
				if (NT_SUCCESS(Irp->IoStatus.Status)) 
				{
					UpdataCachePool(&DevExt->CachePool,
									SysBuf,
									Offset.QuadPart,
									Length,
									_WRITE_);
					Irp->IoStatus.Status = STATUS_SUCCESS;  
					Irp->IoStatus.Information = Length;  
					IoCompleteRequest(Irp, IO_DISK_INCREMENT); 
					continue;
				}
				else
				{
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				}
			}
		} // End of travel list.
	}
	return;
}
