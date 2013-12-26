#include "DMON.h"
//----------------------------------------------------------------------
//                         GLOBALS
//----------------------------------------------------------------------
PDEVICE_ENTRY			g_pDevObjList;
PDRIVER_ENTRY			g_pDrvObjList;
BOOLEAN					g_bStartMon;

PDRIVER_OBJECT			g_pDriverObject;
PDEVICE_OBJECT			g_pDeviceObject;

KSPIN_LOCK				HashLock;
NPAGED_LOOKASIDE_LIST	ContextLookaside;
ULONG					g_uDispatchCount;

//----------------------------------------------------------------------
//                         R O U T I N E S
//----------------------------------------------------------------------
VOID
DriverUnload(PDRIVER_OBJECT driver)
{
	KIRQL			OldIrql;
	PDRIVER_ENTRY	DrvEntry, prevDrvEntry;
	PDEVICE_ENTRY	DevEntry, prevDevEntry;
	UNICODE_STRING	SymbolicLinkName;
	DbgPrint("DMon: unloading %u ...\n", g_uDispatchCount);

	g_bStartMon = FALSE;

	RtlInitUnicodeString(&SymbolicLinkName, L"\\DosDevices\\Dmon");
	IoDeleteSymbolicLink(&SymbolicLinkName);

	OldIrql = KfAcquireSpinLock(&HashLock);
	DrvEntry = g_pDrvObjList;
	while ( DrvEntry )
	{
		// Restore Dispatch Functions
		RtlMoveMemory ( DrvEntry->DriverObject->MajorFunction,
						DrvEntry->DriverDispatch,
						IRP_MJ_MAXIMUM_FUNCTION * sizeof(void*) );
		prevDrvEntry = DrvEntry;
		DrvEntry = DrvEntry->Next;
		ExFreePool(prevDrvEntry);
	}
	KfReleaseSpinLock(&HashLock, OldIrql);

	// Wait for All Dispatch(es) Finished
	while (g_uDispatchCount != 0);

	OldIrql = KfAcquireSpinLock(&HashLock);
	DevEntry = g_pDevObjList;
	while ( DevEntry )
	{
		prevDevEntry = DevEntry;
		DevEntry = DevEntry->Next;
		DestroyCachePool(&prevDevEntry->CachePool);
		ExFreePool(prevDevEntry);
	}
	KfReleaseSpinLock(&HashLock, OldIrql);

	IoDeleteDevice(g_pDeviceObject);
	ExDeleteNPagedLookasideList(&ContextLookaside);
	DbgPrint("DMon: unloaded\n");
}

NTSTATUS
DriverEntry(
	PDRIVER_OBJECT	DriverObject,
	PUNICODE_STRING	RegistryPath
	)
{
	NTSTATUS		status;
	UNICODE_STRING	DeviceName;
	UNICODE_STRING	SymbolicLinkName;

	g_bStartMon = FALSE;
	g_pDriverObject = DriverObject;

	RtlInitUnicodeString(&DeviceName, L"\\Device\\Dmon");
	status = IoCreateDevice(DriverObject, 0,
							&DeviceName,
							FILE_DEVICE_UNKNOWN,
							0, // characteristic
							TRUE, &g_pDeviceObject);

	if ( NT_SUCCESS(status) )
	{
		RtlInitUnicodeString(&SymbolicLinkName, L"\\DosDevices\\Dmon");
		if ( !NT_SUCCESS(IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName)) )
		{
			DbgPrint("Diskmon.SYS: IoCreateSymbolicLink failed\n");
		}

		DriverObject->MajorFunction[IRP_MJ_CREATE]			=
		DriverObject->MajorFunction[IRP_MJ_CLOSE]			= (PDRIVER_DISPATCH)DMCreateClose;
		DriverObject->MajorFunction[IRP_MJ_READ]			=
		DriverObject->MajorFunction[IRP_MJ_WRITE]			= (PDRIVER_DISPATCH)DMReadWrite;
		DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]		=
		DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]	= (PDRIVER_DISPATCH)DMShutDownFlushBuffer;
		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]	=
		DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = (PDRIVER_DISPATCH)DMDeviceControl;
		DriverObject->DriverUnload = DriverUnload;

		g_uDispatchCount = 0;
		KeInitializeSpinLock(&HashLock);
		ExInitializeNPagedLookasideList(&ContextLookaside, NULL, NULL, 0,
										sizeof( MYCONTEXT ), 'nmkD', 0);
		// Hook Disk's partition(s)
		HookDiskPartition(DriverObject, 0);
	}

	return status;
}

NTSTATUS
GetDiskDeviceObjectPointer(
	ULONG			DiskIndex,
	ULONG			PartitionIndex,
	PFILE_OBJECT	*FileObject,
	PDEVICE_OBJECT	*DeviceObject
	)
{
	NTSTATUS					status;
	CHAR						SourceString[64] = "";
	STRING						astr;
	UNICODE_STRING				ustr;

	RtlStringCbPrintfA(SourceString, 64, "\\Device\\Harddisk%d\\Partition%d", DiskIndex, PartitionIndex);
	RtlInitAnsiString(&astr, SourceString);
	RtlAnsiStringToUnicodeString(&ustr, &astr, TRUE);

	status = IoGetDeviceObjectPointer(&ustr, FILE_READ_ATTRIBUTES, FileObject, DeviceObject);

	RtlFreeUnicodeString(&ustr);
	return status;
}

NTSTATUS
HookDiskPartition(
	PDRIVER_OBJECT	DriverObject,
	ULONG			DiskIndex
	)
{
	NTSTATUS					status;
	PIRP						Irp;
	PFILE_OBJECT				FileObject, FileObject1;
	PDEVICE_OBJECT				DeviceObject, DeviceObject1;
	PCONFIGURATION_INFORMATION	ConfigInfo;
	PDRIVE_LAYOUT_INFORMATION	LayoutInfo;
	KEVENT						Event;
	IO_STATUS_BLOCK				iosb;
	ULONG						i, j;

	ConfigInfo = IoGetConfigurationInformation();

	for(i = DiskIndex; i < ConfigInfo->DiskCount; i++)
	{
		status = GetDiskDeviceObjectPointer(i, 0, &FileObject, &DeviceObject);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("Error in hook disk(%d)-part(%d)\n", i, 0);
			continue;
		}
		// Add disk [i] partition [0] to hook entry
		//AddDeviceToHookEntry(DeviceObject, i, 0);
		ObfDereferenceObject(FileObject);

		LayoutInfo = (PDRIVE_LAYOUT_INFORMATION)ExAllocatePool(NonPagedPool, 0x2000);
		if ( LayoutInfo )
		{
			KeInitializeEvent(&Event, NotificationEvent, FALSE);
			Irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_LAYOUT,
												DeviceObject, NULL, 0,
												LayoutInfo, 0x2000, FALSE, &Event, &iosb);
			if ( Irp )
			{
				status = IoCallDriver(DeviceObject, Irp);
				if ( status == STATUS_PENDING )
				{
					KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
					status = iosb.Status;
				}
				if ( NT_SUCCESS(status) )
				{
					for(j = 1; j < LayoutInfo->PartitionCount; j++)
					{
						status = GetDiskDeviceObjectPointer(i, j, &FileObject1, &DeviceObject1);
						if (!NT_SUCCESS(status))
						{
							DbgPrint("Error in hook disk(%d)-part(%d)\n", i, j);
							continue;
						}
						// Add disk [i] partition [j] to hook entry
						AddDeviceToHookEntry(DeviceObject1, i, j);
						ObfDereferenceObject(FileObject1);
					}
				}
				else
				{
					DbgPrint("Error in get driver layout\n");
				}
			}
			ExFreePool(LayoutInfo);
		}
	}

	return status;
}

VOID
AddDeviceToHookEntry(
	PDEVICE_OBJECT	DeviceObject,
	ULONG			DiskIndex,
	ULONG			PartitionIndex
	)
{
	PDEVICE_ENTRY	DevEntry, PreDevEntry = NULL;
	PDEVICE_ENTRY	NewDevEntry;
	PDRIVER_ENTRY	DrvEntry, NewDrvEntry;
	KIRQL			OldIrql;
	ULONG			i;

	OldIrql = KfAcquireSpinLock(&HashLock);
	DevEntry = g_pDevObjList;

	if ( g_pDevObjList )
	{
		while ( DevEntry->DeviceObject != DeviceObject )
		{
			PreDevEntry = DevEntry;
			DevEntry = DevEntry->Next;
			if ( !DevEntry )
				goto NewDev;
		}
		if ( PreDevEntry )
		{
			PreDevEntry->Next = DevEntry->Next;
		}
		else
		{
			g_pDevObjList = DevEntry->Next;
		}
		ExFreePool(DevEntry);
	}

NewDev:
	NewDevEntry = (PDEVICE_ENTRY)ExAllocatePool(NonPagedPool, sizeof(DEVICE_ENTRY));
	if ( NewDevEntry )
	{
		NewDevEntry->DeviceObject	= DeviceObject;
		NewDevEntry->DiskNumber		= DiskIndex;
		NewDevEntry->PartitionNumber= PartitionIndex;
		NewDevEntry->SectorSize		= DeviceObject->SectorSize ? DeviceObject->SectorSize : 512;
		NewDevEntry->ReadCount		= 0;
		NewDevEntry->WriteCount		= 0;
		InitCachePool(&NewDevEntry->CachePool);

		DrvEntry = g_pDrvObjList;
		if ( g_pDrvObjList )
		{
			while ( DrvEntry->DriverObject != DeviceObject->DriverObject)
			{
				DrvEntry = DrvEntry->Next;
				if ( !DrvEntry )
					goto NewDrv;
			}

			NewDevEntry->DrvEntry = DrvEntry;
			NewDevEntry->Next = g_pDevObjList;

			g_pDevObjList = NewDevEntry;
		}
		else
		{
NewDrv:
			NewDevEntry->Next = g_pDevObjList;
			g_pDevObjList = NewDevEntry;

			NewDrvEntry = (PDRIVER_ENTRY)ExAllocatePool(NonPagedPool, sizeof(DRIVER_ENTRY));
			if ( NewDrvEntry != NULL )
			{
				// Add to Head of Driver List
				NewDrvEntry->DriverObject	= DeviceObject->DriverObject;
				NewDrvEntry->Next			= g_pDrvObjList;
				g_pDrvObjList 				= NewDrvEntry;

				NewDevEntry->DrvEntry		= NewDrvEntry;

				RtlMoveMemory ( NewDrvEntry->DriverDispatch,
								DeviceObject->DriverObject->MajorFunction,
								(IRP_MJ_MAXIMUM_FUNCTION+1) * sizeof(PVOID) );
				// Replace Dispatch Functions
				for(i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
				{
					if(g_pDriverObject->MajorFunction[i] != NULL)
					{
						DeviceObject->DriverObject->MajorFunction[i] = g_pDriverObject->MajorFunction[i];
					}
				}
			}
		}
	}

	KfReleaseSpinLock(&HashLock, OldIrql);
	KdPrint(("Add disk(%d)part(%d) to Hook Entry\n", DiskIndex, PartitionIndex));
}

PDEVICE_ENTRY
LookupEntryByDevObj(
	PDEVICE_OBJECT	DeviceObject
	)
{
	KIRQL			OldIrql;
	PDEVICE_ENTRY	hashEntry, prevEntry;

	prevEntry = NULL;
	OldIrql = KfAcquireSpinLock(&HashLock);

	hashEntry = g_pDevObjList;
	while ( hashEntry )
	{
		if ( hashEntry->DeviceObject == DeviceObject )
		{
			prevEntry = hashEntry;
			break;
		}
		hashEntry = hashEntry->Next;
	}

	KfReleaseSpinLock(&HashLock, OldIrql);
	return prevEntry;
}

PDRIVER_ENTRY
LookupEntryByDrvObj(
	PDRIVER_OBJECT	DriverObject
	)
{
	KIRQL			OldIrql;
	PDRIVER_ENTRY	hashEntry, prevEntry = NULL;

	OldIrql = KfAcquireSpinLock(&HashLock);

	hashEntry = g_pDrvObjList;
	while ( hashEntry )
	{
		if ( hashEntry->DriverObject == DriverObject )
		{
			prevEntry = hashEntry;
			break;
		}
		hashEntry = hashEntry->Next;
	}

	KfReleaseSpinLock(&HashLock, OldIrql);
	return prevEntry;
}
