#pragma once

#include "Structs.h"

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
KSTART_ROUTINE				DF_WriteBackThread;

NTSTATUS
	DF_QueryConfig (
		PWCHAR ProtectedVolume,
		PWCHAR CacheVolume,
		PUNICODE_STRING RegistryPath
	);

VOID
	DF_ReadWriteThread (PVOID Context);

VOID
	DF_WriteBackThread (PVOID Context);

#pragma alloc_text("INIT",  DriverEntry)
#pragma alloc_text("PAGED", DF_AddDevice)
#pragma alloc_text("PAGED", DF_DispatchDefault)
#pragma alloc_text("PAGED", DF_DispatchPnp)
#pragma alloc_text("PAGED", DF_DispatchPower)
#pragma alloc_text("PAGED", DF_DispatchIoctl)
#pragma alloc_text("PAGED", DF_CtlDevDispatch)

#define DBG_TRACE_ROUTINES				0x00000001
#define DBG_TRACE_OPS					0x00000002
#define DBG_TRACE_RW					0x00000004
#define DBG_TRACE_CACHE					0x00000008
#define DBG_PRINT( _dbgLevel, _string ) \
	( FlagOn(g_TraceFlags,(_dbgLevel) ) ? DbgPrint _string : ((void)0) )

#define DF_POOL_TAG					'dftD'
#define DF_FREE(_p)					ExFreePoolWithTag(_p,DF_POOL_TAG)
#define DF_MALLOC(_n)				ExAllocatePoolWithTag(			\
										NonPagedPool,				\
										(SIZE_T)(_n),				\
										DF_POOL_TAG )

#define COMPLETE_IRP(_Irp,_Status)	_Irp->IoStatus.Status=_Status;			\
									_Irp->IoStatus.Information=0;			\
									IoCompleteRequest(_Irp,IO_NO_INCREMENT);
