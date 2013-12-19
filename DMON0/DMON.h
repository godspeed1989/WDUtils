#pragma once
#include <ntifs.h>
#include <ntddk.h>
#include <ntdddisk.h>
#include <ntstrsafe.h>
#include "IOCTL.h"
//----------------------------------------------------------------------
//  Typedefs and defines
//----------------------------------------------------------------------
typedef struct _MYCONTEXT
{
	PVOID			CompletionRoutine;
	PVOID			Context;
	UCHAR			Control;
	ULONG			MajorFunction;
}MYCONTEXT, *PMYCONTEXT;

typedef struct _DRIVER_ENTRY
{
	// Original Driver Object
	PDRIVER_OBJECT			DriverObject;
	// Original Dispatch Functions
	PDRIVER_DISPATCH		DriverDispatch[IRP_MJ_MAXIMUM_FUNCTION + 1];
	struct _DRIVER_ENTRY *  Next;
} DRIVER_ENTRY, *PDRIVER_ENTRY;

typedef struct _DEVICE_ENTRY
{
	PDRIVER_ENTRY			DrvEntry;
	// Original Device Object
	PDEVICE_OBJECT			DeviceObject;
	ULONG					DiskNumber;
	ULONG					PartitionNumber;
	ULONG					ReadCount;
	ULONG					WriteCount;
	struct _DEVICE_ENTRY *  Next;
} DEVICE_ENTRY, *PDEVICE_ENTRY;

#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'noMD')
//----------------------------------------------------------------------
//                     F U N C T I O N S
//----------------------------------------------------------------------
DRIVER_INITIALIZE		DriverEntry;
DRIVER_UNLOAD			DriverUnload;
DRIVER_DISPATCH			DMReadWrite;
DRIVER_DISPATCH			DMCreateClose;
DRIVER_DISPATCH			DMDeviceControl;
DRIVER_DISPATCH			DMShutDownFlushBuffer;
IO_COMPLETION_ROUTINE	MyCompletionRoutine;

PDEVICE_ENTRY	LookupEntryByDevObj(PDEVICE_OBJECT DeviceObject);
PDRIVER_ENTRY	LookupEntryByDrvObj(PDRIVER_OBJECT DriverObject);

NTSTATUS		HookDispatch(PDRIVER_OBJECT DriverObject, ULONG DiskIndex);
VOID			AddDeviceToHookEntry(PDEVICE_OBJECT DeviceObject, ULONG DiskIndex, ULONG PartitionIndex);

NTSTATUS		DMReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS		DMCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS		DMDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS		DefaultDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS		DMShutDownFlushBuffer(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS		MyCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
//----------------------------------------------------------------------
//                         GLOBALS
//----------------------------------------------------------------------
extern	PDEVICE_ENTRY			g_pDevObjList;
extern	PDRIVER_ENTRY			g_pDrvObjList;
extern	BOOLEAN					g_bStartMon;

extern	PDRIVER_OBJECT			g_pDriverObject;
extern	PDEVICE_OBJECT			g_pDeviceObject;

extern	KSPIN_LOCK				HashLock;
extern	NPAGED_LOOKASIDE_LIST	ContextLookaside;
extern	ULONG					g_uDispatchCount;
