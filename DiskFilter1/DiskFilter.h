#pragma once

#include "Structs.h"

#ifdef __cplusplus
extern "C"
{
#endif

DRIVER_INITIALIZE		DriverEntry;

DRIVER_ADD_DEVICE		DiskFilter_AddDevice;

DRIVER_UNLOAD			DiskFilter_DriverUnload;

DRIVER_REINITIALIZE		DiskFilter_DriverReinitializeRoutine;

KSTART_ROUTINE 			DiskFilter_ReadWriteThread;

DRIVER_DISPATCH			DiskFilter_DispatchDefault;

DRIVER_DISPATCH			DiskFilter_DispatchReadWrite;

DRIVER_DISPATCH			DiskFilter_DispatchPnp;

DRIVER_DISPATCH			DiskFilter_DispatchPower;

DRIVER_DISPATCH			DiskFilter_DispatchControl;

#ifdef __cplusplus
}
#endif

NTSTATUS
	DiskFilter_QueryConfig (
		PWCHAR ProtectedVolume,
		PWCHAR CacheVolume,
		PUNICODE_STRING RegistryPath
	);

NTSTATUS
	DiskFilter_InitBitMapAndCreateThread(
		PDISKFILTER_DEVICE_EXTENSION DevExt
	);

VOID
	DiskFilter_ReadWriteThread(PVOID Context);

#pragma alloc_text("INIT",  DriverEntry)
#pragma alloc_text("PAGED", DiskFilter_AddDevice)
#pragma alloc_text("PAGED", DiskFilter_DriverUnload)
#pragma alloc_text("PAGED", DiskFilter_DispatchDefault)
#pragma alloc_text("PAGED", DiskFilter_DispatchReadWrite)
#pragma alloc_text("PAGED", DiskFilter_DispatchPnp)
#pragma alloc_text("PAGED", DiskFilter_DispatchPower)
#pragma alloc_text("PAGED", DiskFilter_DispatchControl)
