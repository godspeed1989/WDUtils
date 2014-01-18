#pragma once

#include "Structs.h"

extern ULONG				g_TraceFlags;
extern PDEVICE_OBJECT		g_pDeviceObject;
extern PDRIVER_OBJECT		g_pDriverObject;

DRIVER_INITIALIZE			DriverEntry;
DRIVER_ADD_DEVICE			DF_AddDevice;
DRIVER_REINITIALIZE			DF_DriverReinitializeRoutine;
DRIVER_DISPATCH				DF_DispatchDefault;
DRIVER_DISPATCH				DF_DispatchReadWrite;
DRIVER_DISPATCH				DF_DispatchPnp;
DRIVER_DISPATCH				DF_DispatchPower;
DRIVER_DISPATCH				DF_DispatchIoctl;
DRIVER_DISPATCH				DF_CtlDevDispatch;
KSTART_ROUTINE				DF_ReadWriteThread;

NTSTATUS
	DF_QueryConfig (
		PWCHAR ProtectedVolume,
		PWCHAR CacheVolume,
		PUNICODE_STRING RegistryPath
	);

VOID
	DF_ReadWriteThread (PVOID Context);

#pragma alloc_text("INIT",  DriverEntry)
#pragma alloc_text("PAGED", DF_AddDevice)
#pragma alloc_text("PAGED", DF_DispatchDefault)
#pragma alloc_text("PAGED", DF_DispatchReadWrite)
#pragma alloc_text("PAGED", DF_DispatchPnp)
#pragma alloc_text("PAGED", DF_DispatchPower)
#pragma alloc_text("PAGED", DF_DispatchIoctl)
#pragma alloc_text("PAGED", DF_CtlDevDispatch)

#define DBG_TRACE_ROUTINES				0x00000001
#define DBG_TRACE_OPS					0x00000002
#define DBG_TRACE_RW					0x00000004
#define DBG_PRINT( _dbgLevel, _string ) \
	( FlagOn(g_TraceFlags,(_dbgLevel) ) ? DbgPrint _string : ((void)0) )

#define DF_POOL_TAG					'dftD'
#define DF_FREE(p)					ExFreePoolWithTag(p,DF_POOL_TAG)
#define DF_MALLOC(n)				ExAllocatePoolWithTag(	\
										NonPagedPool,		\
										(SIZE_T)(n),		\
										DF_POOL_TAG )
