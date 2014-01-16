#include "Utils.h"
#include "DiskFilter.h"

ULONG				g_TraceFlags;
PDEVICE_OBJECT		g_pDeviceObject;

NTSTATUS CreateControlDevice(PDRIVER_OBJECT pDriverObject);
//{4D36E967-E325-11CE-BFC1-08002BE10318}
NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject,
			PUNICODE_STRING RegistryPath)
{
	ULONG					i, ClientId;
	NTSTATUS				Status;
	PDF_DRIVER_EXTENSION	DrvExt;

	g_TraceFlags = DBG_TRACE_ROUTINES | DBG_TRACE_OPS | DBG_TRACE_RW;
	for (i = 0; i<=IRP_MJ_MAXIMUM_FUNCTION; ++i)
	{
		DriverObject->MajorFunction[i] = DF_DispatchDefault;
	}

	DriverObject->MajorFunction[IRP_MJ_READ] = DF_DispatchReadWrite;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = DF_DispatchReadWrite;
	DriverObject->MajorFunction[IRP_MJ_POWER] = DF_DispatchPower;
	DriverObject->MajorFunction[IRP_MJ_PNP] = DF_DispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DF_DispatchIoctl;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DF_DispatchDevCtl;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DF_DispatchDevCtl;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = DF_DispatchDevCtl;

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
	return CreateControlDevice(DriverObject);
}

NTSTATUS
DF_QueryConfig(PWCHAR ProtectedVolume, PWCHAR CacheVolume, PUNICODE_STRING RegistryPath)
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

VOID
DF_DriverReinitializeRoutine(PDRIVER_OBJECT DriverObject, PVOID Context, ULONG Count)
{
	PDEVICE_OBJECT 			DeviceObject;
	PDF_DEVICE_EXTENSION	DevExt;

	UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(Count);
	DBG_PRINT(DBG_TRACE_ROUTINES, ("Start Reinitialize...\n"));

	DeviceObject = DriverObject->DeviceObject;
	while (DeviceObject != NULL)
	{
		DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
		if (DeviceObject != g_pDeviceObject)
			DF_QueryVolumeInfo(DeviceObject);
		/*
		KdPrint((" -> [%wZ]", &DevExt->VolumeDosName));
		if (DevExt->bIsProtectedVolume)
		{
			KdPrint((" Protected\n"));
		}
		else
		{
			KdPrint(("\n"));
		}*/
		DeviceObject = DeviceObject->NextDevice;
	}
}

NTSTATUS
DF_AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
	NTSTATUS				Status;
	PDF_DEVICE_EXTENSION	DevExt;
	PDEVICE_OBJECT			DeviceObject;
	PDEVICE_OBJECT			LowerDeviceObject;
	PAGED_CODE();

	DBG_PRINT(DBG_TRACE_ROUTINES, ("DF_AddDevice: Enter\n"));

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
		KdPrint(("Create device failed"));
		goto l_error;
	}

	DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	DevExt->bIsProtectedVolume = FALSE;
	DevExt->PhysicalDeviceObject = PhysicalDeviceObject;
	KeInitializeEvent(&DevExt->PagingCountEvent, NotificationEvent, TRUE);

	LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	if (LowerDeviceObject == NULL)
	{
		KdPrint(("Attach device failed...\n"));
		goto l_error;
	}
	DevExt->LowerDeviceObject = LowerDeviceObject;

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
CreateControlDevice(PDRIVER_OBJECT pDriverObject)
{
	NTSTATUS				status;
	PDF_DEVICE_EXTENSION	pDevExt;
	UNICODE_STRING			DevName;
	UNICODE_STRING			SymLinkName;

	RtlInitUnicodeString(&DevName, L"\\Device\\Diskfilter");
	status = IoCreateDevice(pDriverObject,
							sizeof(DF_DEVICE_EXTENSION),//0?
							&DevName,
							FILE_DEVICE_UNKNOWN,
							0, TRUE,
							&g_pDeviceObject);
	if(NT_SUCCESS(status))
	{
		DbgPrint("Message Device Create success\n");
	}
	else
	{
		DbgPrint("Message Device Create fail\n");
		return status;
	}

	g_pDeviceObject->Flags |= DO_BUFFERED_IO;

	RtlInitUnicodeString(&SymLinkName, L"\\DosDevices\\Diskfilter");
	status = IoCreateSymbolicLink(&SymLinkName, &DevName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(g_pDeviceObject);
		return status;
	}
	return STATUS_SUCCESS;
}
