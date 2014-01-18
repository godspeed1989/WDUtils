#include "Utils.h"
#include "DiskFilter.h"

ULONG				g_TraceFlags;
PDEVICE_OBJECT		g_pDeviceObject;
PDRIVER_OBJECT		g_pDriverObject;

NTSTATUS DF_CreateControlDevice(PDRIVER_OBJECT pDriverObject);

NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject,
			PUNICODE_STRING RegistryPath)
{
	ULONG					i, ClientId;
	NTSTATUS				Status;
	PDF_DRIVER_EXTENSION	DrvExt;

	g_TraceFlags = DBG_TRACE_ROUTINES | DBG_TRACE_OPS | DBG_TRACE_RW;
	g_pDriverObject = DriverObject;
	for (i = 0; i<=IRP_MJ_MAXIMUM_FUNCTION; ++i)
	{
		DriverObject->MajorFunction[i] = DF_DispatchDefault;
	}

	DriverObject->MajorFunction[IRP_MJ_READ] = DF_DispatchReadWrite;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = DF_DispatchReadWrite;
	DriverObject->MajorFunction[IRP_MJ_POWER] = DF_DispatchPower;
	DriverObject->MajorFunction[IRP_MJ_PNP] = DF_DispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DF_DispatchIoctl;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DF_CtlDevDispatch;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DF_CtlDevDispatch;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = DF_CtlDevDispatch;

	DriverObject->DriverExtension->AddDevice = DF_AddDevice;

	//
	//	Copy serivce key to driver extension.
	//
	ClientId = DF_DRIVER_EXTENSION_ID;
	Status = IoAllocateDriverObjectExtension(DriverObject, (PVOID)ClientId,
		sizeof(DF_DRIVER_EXTENSION), (PVOID*)&DrvExt);
	ASSERT(NT_SUCCESS(Status));

	ClientId = DF_DRIVER_EXTENSION_ID_UNICODE_BUFFER;
	Status = IoAllocateDriverObjectExtension(DriverObject, (PVOID)ClientId,
		RegistryPath->Length + 1, (PVOID*)&DrvExt->RegistryUnicodeBuffer);
	ASSERT(NT_SUCCESS(Status));

	DrvExt->ServiceKeyName.Buffer = DrvExt->RegistryUnicodeBuffer;
	DrvExt->ServiceKeyName.MaximumLength = RegistryPath->Length + 1;
	RtlCopyUnicodeString(&DrvExt->ServiceKeyName, RegistryPath);

	DF_QueryConfig(DrvExt->ProtectedVolumes, DrvExt->CacheVolumes, RegistryPath);

	IoRegisterBootDriverReinitialization(DriverObject, DF_DriverReinitializeRoutine, NULL);

	KdPrint(("Service key :\n%wZ\n", &DrvExt->ServiceKeyName));
	return DF_CreateControlDevice(DriverObject);
}

NTSTATUS
DF_QueryConfig(PWCHAR ProtectedVolume, PWCHAR CacheVolume, PUNICODE_STRING RegistryPath)
{
	ULONG		i;
	NTSTATUS	Status;
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

VOID
DF_DriverReinitializeRoutine(PDRIVER_OBJECT DriverObject, PVOID Context, ULONG Count)
{
	PDEVICE_OBJECT 			DeviceObject;
	PDF_DEVICE_EXTENSION	DevExt;

	UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(Count);
	DBG_PRINT(DBG_TRACE_ROUTINES, ("%s...\n", __FUNCTION__));
}

NTSTATUS
DF_AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
	NTSTATUS				Status;
	PDF_DEVICE_EXTENSION	DevExt;
	PDEVICE_OBJECT			DeviceObject;
	PDEVICE_OBJECT			LowerDeviceObject;
	PAGED_CODE();

	DBG_PRINT(DBG_TRACE_ROUTINES, ("%s: Enter\n", __FUNCTION__));
	// Create Device Object
	Status = IoCreateDevice(DriverObject,
							sizeof(DF_DEVICE_EXTENSION),
							NULL,
							PhysicalDeviceObject->DeviceType,
							FILE_DEVICE_SECURE_OPEN,
							FALSE,
							&DeviceObject
							);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("%s: Create device failed\n", __FUNCTION__));
		goto l_error;
	}
	// Attach to Lower Device
	LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	if (LowerDeviceObject == NULL)
	{
		KdPrint(("%s: Attach device failed\n", __FUNCTION__));
		goto l_error;
	}

	DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	// Initialize Device Extention
	DevExt->DiskNumber = -1;
	DevExt->PartitionNumber = -1;
	DevExt->bIsProtected = FALSE;
	DevExt->PhysicalDeviceObject = PhysicalDeviceObject;
	DevExt->LowerDeviceObject = LowerDeviceObject;

	KeInitializeEvent(&DevExt->PagingCountEvent, NotificationEvent, TRUE);
	DevExt->RwThreadObject = NULL;
	DevExt->bTerminalThread = FALSE;
	InitializeListHead(&DevExt->RwList);
	KeInitializeSpinLock(&DevExt->RwSpinLock);
	KeInitializeEvent(&DevExt->RwThreadEvent, SynchronizationEvent, FALSE);

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

static NTSTATUS
DF_CreateControlDevice(PDRIVER_OBJECT pDriverObject)
{
	NTSTATUS				status;
	PDF_DEVICE_EXTENSION	DevExt;
	UNICODE_STRING			DevName;
	UNICODE_STRING			SymLinkName;

	RtlInitUnicodeString(&DevName, L"\\Device\\Diskfilter");
	status = IoCreateDevice(pDriverObject,
							sizeof(DF_DEVICE_EXTENSION),
							&DevName,
							FILE_DEVICE_UNKNOWN,
							0, TRUE,
							&g_pDeviceObject);
	if(NT_SUCCESS(status))
	{
		KdPrint(("%s: success\n", __FUNCTION__));
	}
	else
	{
		KdPrint(("%s: failed\n", __FUNCTION__));
		return status;
	}

	g_pDeviceObject->Flags |= DO_BUFFERED_IO;
	DevExt = (PDF_DEVICE_EXTENSION)g_pDeviceObject->DeviceExtension;
	DevExt->bIsProtected = FALSE;

	RtlInitUnicodeString(&SymLinkName, L"\\DosDevices\\Diskfilter");
	status = IoCreateSymbolicLink(&SymLinkName, &DevName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(g_pDeviceObject);
		return status;
	}
	return STATUS_SUCCESS;
}
