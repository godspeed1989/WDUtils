//======================================================================
// 
//  Reversed Code of DiskMon Driver.
//
//  Copyright (C) 2009 diyhack
// 
//======================================================================

#include <ntifs.h>
#include <ntdddisk.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "ioctlcmd.h"

#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'noMD')

//----------------------------------------------------------------------
//  Typedefs and defines
//----------------------------------------------------------------------

typedef struct _LOG_BUFFER {

	ULONG_PTR            Len;
	struct _LOG_BUFFER * Next;
	char                 Data[ MAX_STORE ];

} LOG_BUFFER, *PLOG_BUFFER;

typedef struct _MYCONTEXT {

	PVOID			CompletionRoutine;
	PVOID			Context;
	UCHAR			Control;
	ULONG			MajorFunction;
	ULONG			Seq;
	BOOLEAN			bUsePerfCounter;
	LARGE_INTEGER	PerfCount;

}MYCONTEXT, *PMYCONTEXT;

typedef struct _DRIVER_ENTRY {

	PDRIVER_OBJECT			DriverObject;
	PDRIVER_DISPATCH		DriverDispatch[IRP_MJ_MAXIMUM_FUNCTION + 1];
	struct _DRIVER_ENTRY *  Next;

} DRIVER_ENTRY, *PDRIVER_ENTRY;

typedef struct _DEVICE_ENTRY {

	PDRIVER_ENTRY			DrvEntry;
	PDEVICE_OBJECT			DeviceObject;
	ULONG					DiskNumber;
	struct _DEVICE_ENTRY *  Next;

} DEVICE_ENTRY, *PDEVICE_ENTRY;



//----------------------------------------------------------------------
//                     F O R E W A R D S
//----------------------------------------------------------------------
ULONGLONG		GetSectorOffset(ULONGLONG ByteOffset, ULONG SectorSize);
NTSTATUS		DMShutDownFlushBuffer(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS		DMDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
PCHAR			GetIoctlName(char *Buffer, ULONG IoctlCode);
NTSTATUS		DMReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
PDEVICE_ENTRY	LookupEntryByDevObj(PDEVICE_OBJECT DeviceObject);
PDRIVER_ENTRY	LookupEntryByDrvObj(PDRIVER_OBJECT DriverObject);
void			AddDeviceToHookEntry(PDEVICE_OBJECT DeviceObject, ULONG DiskIndex, ULONG PartitionIndex);
NTSTATUS		HookDispatch(PDRIVER_OBJECT DriverObject, ULONG DiskIndex);
NTSTATUS		DMCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS		MyCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
NTSTATUS		DefaultDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp, ULONG Seq, BOOLEAN bUsePerfCounter, PLARGE_INTEGER PerfCount);

//----------------------------------------------------------------------
//                         GLOBALS
//---------------------------------------------------------------------- 
PDEVICE_ENTRY			g_pDevObjList;
PDRIVER_ENTRY			g_pDrvObjList;
BOOLEAN					g_bGUIActive;
BOOLEAN					g_bStartMon;
BOOLEAN					g_bUsePerfCounter;
BOOLEAN					g_bRead;
volatile LONG			g_ReadCount;
BOOLEAN					g_bWrite;
volatile LONG			g_WriteCount;
PDRIVER_OBJECT			g_pDriverObject;
PDEVICE_OBJECT			g_pDeviceObject;
volatile LONG			Sequence;
PLOG_BUFFER				LogBuffer;
KSPIN_LOCK				HashLock;
KSPIN_LOCK				LogBufferLock;
NPAGED_LOOKASIDE_LIST	ContextLookaside;

ULONG NumLogBuffer		= 0;
ULONG MaxLogBuffer		= 1000000/MAX_STORE;

//----------------------------------------------------------------------
//                         R O U T I N E S 
//----------------------------------------------------------------------

//----- (0800068F) --------------------------------------------------------
PCHAR 
ErrorString( 
	NTSTATUS	RetStat, 
	PCHAR		Buffer
	) 
{
	switch( RetStat ) {

	case STATUS_SUCCESS:
		strcpy( Buffer, "SUCCESS" );
		break;

	case STATUS_CRC_ERROR:
		strcpy( Buffer, "CRC ERROR" );
		break;

	case STATUS_NOT_IMPLEMENTED:
		strcpy( Buffer, "NOT IMPLEMENTED" );
		break;

	case STATUS_EAS_NOT_SUPPORTED:
		strcpy( Buffer, "EAS NOT SUPPORTED" );
		break;

	case STATUS_EA_TOO_LARGE:
		strcpy( Buffer, "EA TOO LARGE");
		break;

	case STATUS_NONEXISTENT_EA_ENTRY:
		strcpy( Buffer, "NONEXISTENT EA ENTRY");
		break;

	case STATUS_BAD_NETWORK_NAME:
		strcpy( Buffer, "BAD NETWORK NAME" );
		break;

	case STATUS_NOTIFY_ENUM_DIR:
		strcpy( Buffer, "NOTIFY ENUM DIR" );
		break;

	case STATUS_FILE_CORRUPT_ERROR:
		strcpy( Buffer, "FILE CORRUPT" );
		break;

	case STATUS_DISK_CORRUPT_ERROR:
		strcpy( Buffer, "DISK CORRUPT" );
		break;

	case STATUS_RANGE_NOT_LOCKED:
		strcpy( Buffer, "RANGE NOT LOCKED" );
		break;

	case STATUS_FILE_CLOSED:
		strcpy( Buffer, "FILE CLOSED" );
		break;

	case STATUS_IN_PAGE_ERROR:
		strcpy( Buffer, "IN PAGE ERROR" );
		break;

	case STATUS_CANCELLED:
		strcpy( Buffer, "CANCELLED" );
		break;

	case STATUS_QUOTA_EXCEEDED:
		strcpy( Buffer, "QUOTA EXCEEDED" );
		break;

	case STATUS_NOT_SUPPORTED:
		strcpy( Buffer, "NOT SUPPORTED" );
		break;

	case STATUS_NO_MORE_FILES:
		strcpy( Buffer, "NO MORE FILES" );
		break;

	case STATUS_BUFFER_TOO_SMALL:
		strcpy( Buffer, "BUFFER TOO SMALL" );
		break;

	case STATUS_OBJECT_NAME_INVALID:
		strcpy( Buffer, "NAME INVALID" );
		break;

	case STATUS_OBJECT_NAME_NOT_FOUND:
		strcpy( Buffer, "FILE NOT FOUND" );
		break;

	case STATUS_NOT_A_DIRECTORY:
		strcpy( Buffer, "NOT A DIRECTORY" );
		break;

	case STATUS_NO_SUCH_FILE:
		strcpy( Buffer, "NO SUCH FILE" );
		break;

	case STATUS_OBJECT_NAME_COLLISION:
		strcpy( Buffer, "NAME COLLISION" );
		break;

	case STATUS_NONEXISTENT_SECTOR:
		strcpy( Buffer, "NONEXISTENT SECTOR" );
		break;

	case STATUS_BAD_NETWORK_PATH:
		strcpy( Buffer, "BAD NETWORK PATH" );
		break;

	case STATUS_OBJECT_PATH_NOT_FOUND:
		strcpy( Buffer, "PATH NOT FOUND" );
		break;

	case STATUS_NO_SUCH_DEVICE:
		strcpy( Buffer, "INVALID PARAMETER" );
		break;

	case STATUS_END_OF_FILE:
		strcpy( Buffer, "END OF FILE" );
		break;

	case STATUS_NOTIFY_CLEANUP:
		strcpy( Buffer, "NOTIFY CLEANUP" );
		break;

	case STATUS_BUFFER_OVERFLOW:
		strcpy( Buffer, "BUFFER OVERFLOW" );
		break;

	case STATUS_NO_MORE_ENTRIES:
		strcpy( Buffer, "NO MORE ENTRIES" );
		break;

	case STATUS_ACCESS_DENIED:
		strcpy( Buffer, "ACCESS DENIED" );
		break;

	case STATUS_SHARING_VIOLATION:
		strcpy( Buffer, "SHARING VIOLATION" );
		break; 

	case STATUS_INVALID_PARAMETER:
		strcpy( Buffer, "INVALID PARAMETER" );
		break;

	case STATUS_OPLOCK_BREAK_IN_PROGRESS:
		strcpy( Buffer, "OPLOCK BREAK" );
		break; 

	case STATUS_OPLOCK_NOT_GRANTED:
		strcpy( Buffer, "OPLOCK NOT GRANTED" );
		break;

	case STATUS_FILE_LOCK_CONFLICT:
		strcpy( Buffer, "FILE LOCK CONFLICT" );
		break;

	case STATUS_PENDING:
		strcpy( Buffer, "PENDING" );
		break; 

	case STATUS_REPARSE:
		strcpy( Buffer, "REPARSE" );
		break;

	case STATUS_MORE_ENTRIES:
		strcpy( Buffer, "MORE" );
		break;

	case STATUS_DELETE_PENDING:
		strcpy( Buffer, "DELETE PEND" );
		break; 

	case STATUS_CANNOT_DELETE:
		strcpy( Buffer, "CANNOT DELETE" );
		break; 

	case STATUS_LOCK_NOT_GRANTED:
		strcpy( Buffer, "NOT GRANTED" );
		break; 

	case STATUS_FILE_IS_A_DIRECTORY:
		strcpy( Buffer, "IS DIRECTORY" );
		break;

	case STATUS_ALREADY_COMMITTED:
		strcpy( Buffer, "ALREADY COMMITTED" );
		break;

	case STATUS_INVALID_EA_FLAG:
		strcpy( Buffer, "INVALID EA FLAG" );
		break;

	case STATUS_INVALID_INFO_CLASS:
		strcpy( Buffer, "INVALID INFO CLASS" );
		break;

	case STATUS_INVALID_HANDLE:
		strcpy( Buffer, "INVALID HANDLE" );
		break;

	case STATUS_INVALID_DEVICE_REQUEST:
		strcpy( Buffer, "INVALID DEVICE REQUEST" );
		break;

	case STATUS_WRONG_VOLUME:
		strcpy( Buffer, "WRONG VOLUME" );
		break;

	case STATUS_UNEXPECTED_NETWORK_ERROR:
		strcpy( Buffer, "NETWORK ERROR" );
		break;

	case STATUS_DFS_UNAVAILABLE:
		strcpy( Buffer, "DFS UNAVAILABLE" );
		break;

	case STATUS_LOG_FILE_FULL:
		strcpy( Buffer, "LOG FILE FULL" );
		break;

	case STATUS_INVALID_DEVICE_STATE:
		strcpy( Buffer, "INVALID DEVICE STATE" );
		break;

	case STATUS_NO_MEDIA_IN_DEVICE:
		strcpy( Buffer, "NO MEDIA");
		break;

	case STATUS_DISK_FULL:
		strcpy( Buffer, "DISK FULL");
		break;

	case STATUS_DIRECTORY_NOT_EMPTY:
		strcpy( Buffer, "NOT EMPTY");
		break;

	case STATUS_INSTANCE_NOT_AVAILABLE:
		strcpy( Buffer, "INSTANCE NOT AVAILABLE" );
		break;

	case STATUS_PIPE_NOT_AVAILABLE:
		strcpy( Buffer, "PIPE NOT AVAILABLE" );
		break;

	case STATUS_INVALID_PIPE_STATE:
		strcpy( Buffer, "INVALID PIPE STATE" );
		break;

	case STATUS_PIPE_BUSY:
		strcpy( Buffer, "PIPE BUSY" );
		break;

	case STATUS_PIPE_DISCONNECTED:
		strcpy( Buffer, "PIPE DISCONNECTED" );
		break;

	case STATUS_PIPE_CLOSING:
		strcpy( Buffer, "PIPE CLOSING" );
		break;

	case STATUS_PIPE_CONNECTED:
		strcpy( Buffer, "PIPE CONNECTED" );
		break;

	case STATUS_PIPE_LISTENING:
		strcpy( Buffer, "PIPE LISTENING" );
		break;

	case STATUS_INVALID_READ_MODE:
		strcpy( Buffer, "INVALID READ MODE" );
		break;

	case STATUS_PIPE_EMPTY:
		strcpy( Buffer, "PIPE EMPTY" );
		break;

	case STATUS_PIPE_BROKEN:
		strcpy( Buffer, "PIPE BROKEN" );
		break;

	case STATUS_IO_TIMEOUT:
		strcpy( Buffer, "IO TIMEOUT" );
		break;

	default:
		sprintf( Buffer, "* 0x%X", RetStat );
		break;
	}

	return Buffer;
}
//----- (08000A94) --------------------------------------------------------
VOID DMonNewLogBuffer()
{
	PLOG_BUFFER prev = LogBuffer, newLogBuffer;

	if( MaxLogBuffer == NumLogBuffer ) {

		LogBuffer->Len = 0;
		return; 
	}

	if( !LogBuffer->Len ) {

		return;
	}

	newLogBuffer = ExAllocatePool( NonPagedPool, sizeof(*LogBuffer) );
	if( newLogBuffer ) { 

		LogBuffer   = newLogBuffer;
		LogBuffer->Len  = 0;
		LogBuffer->Next = prev;
		NumLogBuffer++;

	} else {

		LogBuffer->Len = 0;
	}
}

//----- (08000AE8) --------------------------------------------------------
PLOG_BUFFER 
DMonOldestLogBuffer()
{
	PLOG_BUFFER  ptr = LogBuffer, prev = NULL;

	while ( ptr->Next ) {

		ptr = (prev = ptr)->Next;
	}

	if ( prev ) {

		prev->Next = NULL;    
	}

	NumLogBuffer--;

	return ptr;
}

//----- (08000B10) --------------------------------------------------------
VOID __cdecl LogRecord(
	ULONG Sequence, 
	PLARGE_INTEGER time, 
	ULONG DiskNum, 
	PCH format, 
	...
	)
{
	static CHAR     text[1000];
	PENTRY          Entry;
	ULONG           len;
	va_list         arg_ptr;

	KIRQL OldIrql;
	if ( g_bGUIActive )
	{
		if ( g_bStartMon )
		{
			OldIrql = KfAcquireSpinLock(&LogBufferLock);

			va_start( arg_ptr, format );
			len = vsprintf( text, format, arg_ptr );
			va_end( arg_ptr );

			len += 4; len &= 0xFFFFFFFC;

			if (LogBuffer->Len + len + sizeof(*Entry) >= 0x10000)
			{
				DMonNewLogBuffer();		
			}

			Entry = (PENTRY) (LogBuffer->Data + LogBuffer->Len);
			Entry->seq = Sequence;
			Entry->time.QuadPart = time->QuadPart;
			Entry->DiskNum = DiskNum;
			memcpy((void *)Entry->text, text, len);

			LogBuffer->Len += len + (PCHAR)Entry->text - (PCHAR)Entry;

			KfReleaseSpinLock(&LogBufferLock, OldIrql);
		}
	}
}

//----- (08000C5A) --------------------------------------------------------
NTSTATUS 
DriverEntry(
	PDRIVER_OBJECT	DriverObject, 
	PUNICODE_STRING	RegistryPath
	)
{
	NTSTATUS		status;
	UNICODE_STRING	DeviceName;
	UNICODE_STRING	SymbolicLinkName;

	g_pDriverObject = DriverObject;

	RtlInitUnicodeString(&DeviceName, L"\\Device\\Dmon");
	status = IoCreateDevice(DriverObject, 0, &DeviceName, 0x8310, 0, TRUE, &g_pDeviceObject);

	if ( NT_SUCCESS(status) )
	{
		RtlInitUnicodeString(&SymbolicLinkName, L"\\DosDevices\\Dmon");
		if ( !NT_SUCCESS(IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName)))
		{
			DbgPrint("Diskmon.SYS: IoCreateSymbolicLink failed\n");
		}

		DriverObject->MajorFunction[IRP_MJ_CREATE]			= (PDRIVER_DISPATCH)DMCreateClose;
		DriverObject->MajorFunction[IRP_MJ_CLOSE]			= (PDRIVER_DISPATCH)DMCreateClose;
		DriverObject->MajorFunction[IRP_MJ_READ]			= (PDRIVER_DISPATCH)DMReadWrite;
		DriverObject->MajorFunction[IRP_MJ_WRITE]			= (PDRIVER_DISPATCH)DMReadWrite;
		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]	= (PDRIVER_DISPATCH)DMDeviceControl;
		DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = (PDRIVER_DISPATCH)DMDeviceControl;
		DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]		= (PDRIVER_DISPATCH)DMShutDownFlushBuffer;
		DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]	= (PDRIVER_DISPATCH)DMShutDownFlushBuffer;

		KeInitializeSpinLock(&LogBufferLock);
		KeInitializeSpinLock(&HashLock);

		LogBuffer = ExAllocatePool(NonPagedPool, sizeof(*LogBuffer));

		if ( LogBuffer )
		{
			LogBuffer->Len  = 0;
			LogBuffer->Next = NULL;
			NumLogBuffer = 1;

			ExInitializeNPagedLookasideList(&ContextLookaside, NULL, NULL, 0, sizeof( MYCONTEXT ), 'nmkD', 0);
			HookDispatch(DriverObject, 0);

			status = STATUS_SUCCESS;
		}
		else
		{
			IoDeleteDevice(g_pDeviceObject);
			status = STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	return status;
}

//----- (08000DCF) --------------------------------------------------------
NTSTATUS 
HookDispatch(
	PDRIVER_OBJECT	DriverObject, 
	ULONG			DiskIndex
	)
{
	PCONFIGURATION_INFORMATION	ConfigInfo;
	NTSTATUS					status;
	PIRP						Irp;
	CHAR						SourceString[64] = "";
	STRING						astr;
	UNICODE_STRING				ustr;
	PFILE_OBJECT				FileObject, FileObject1;
	PDEVICE_OBJECT				DeviceObject, DeviceObject1;
	PDRIVE_LAYOUT_INFORMATION	LayoutInfo;
	KEVENT						Event;
	IO_STATUS_BLOCK				iosb;
	OBJECT_ATTRIBUTES			oa;
	HANDLE						Handle;
	ULONG						i, j;

	ConfigInfo = IoGetConfigurationInformation();	

	for(i = DiskIndex; i < ConfigInfo->DiskCount; i++)
	{
		sprintf(SourceString, "\\Device\\Harddisk%d\\Partition0", i);

		RtlInitAnsiString(&astr, SourceString);
		RtlAnsiStringToUnicodeString(&ustr, &astr, TRUE);

		status = IoGetDeviceObjectPointer(&ustr, 0x80, &FileObject, &DeviceObject);

		RtlFreeUnicodeString(&ustr);

		if (!NT_SUCCESS(status))
		{			
			continue;
		}

		AddDeviceToHookEntry(FileObject->DeviceObject, i, 0);

		LayoutInfo = (PDRIVE_LAYOUT_INFORMATION)ExAllocatePool(0, 0x2000);
		if ( LayoutInfo )
		{
			KeInitializeEvent(&Event, NotificationEvent, FALSE);
			Irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_LAYOUT, DeviceObject, NULL, 0, LayoutInfo, 0x2000, FALSE, &Event, &iosb);
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
						sprintf(SourceString, "\\Device\\Harddisk%d\\Partition%d", i, j);

						RtlInitAnsiString(&astr, SourceString);
						RtlAnsiStringToUnicodeString(&ustr, &astr, TRUE);

						status = IoGetDeviceObjectPointer(&ustr, 0x80, &FileObject1, &DeviceObject1);
						RtlFreeUnicodeString(&ustr);

						if (!NT_SUCCESS(status))
						{			
							continue;
						}

						AddDeviceToHookEntry(FileObject1->DeviceObject, i, j);

						ObfDereferenceObject(FileObject1);
					}
				}
			}

			ExFreePool(LayoutInfo);
		}

		ObfDereferenceObject(FileObject);
	}

	return status;
}

//----- (08000FDA) --------------------------------------------------------
NTSTATUS 
MyCompletionRoutine(
	PDEVICE_OBJECT	DeviceObject, 
	PIRP			Irp, 
	PVOID			Context
	)
{
	PMYCONTEXT				MyContext;
	NTSTATUS				status;
	UCHAR					Control;
	PCHAR					pError;
	LARGE_INTEGER			PerfCount;
	char					Error[64] = "";
	PIO_COMPLETION_ROUTINE	CompletionRoutine;

	MyContext = (PMYCONTEXT)Context;

	switch(MyContext->MajorFunction)
	{
	case IRP_MJ_READ:
		InterlockedDecrement(&g_ReadCount);
		break;

	case IRP_MJ_WRITE:
		InterlockedDecrement(&g_WriteCount);
		break;

	default:
		break;
	}

	PerfCount = KeQueryPerformanceCounter(0);
	if ( MyContext->bUsePerfCounter && MyContext->PerfCount.QuadPart != 0 )
	{		
		PerfCount.QuadPart -= MyContext->PerfCount.QuadPart;
	}
	else
	{
		PerfCount.QuadPart = MyContext->PerfCount.QuadPart;
	}

	pError = ErrorString(Irp->IoStatus.Status, Error);
	//LogRecord(MyContext->Seq, &PerfCount, -1, "%s", pError);

	Control = MyContext->Control;
	CompletionRoutine = MyContext->CompletionRoutine;
	Context = MyContext->Context;

	ExFreeToNPagedLookasideList(&ContextLookaside, MyContext);

	status = Irp->IoStatus.Status;
	if( NT_SUCCESS(status) )
	{
		if(Control & 0x40)
		{
			return CompletionRoutine(DeviceObject, Irp, Context);
		}
	}
	else if(status == STATUS_CANCELLED)
	{
		if(Control & 0x20)
		{
			return CompletionRoutine(DeviceObject, Irp, Context);
		}
	}
	else
	{
		if(Control & 0x80)
		{
			return CompletionRoutine(DeviceObject, Irp, Context);
		}
	}

	return status;
}


//----- (080010E1) --------------------------------------------------------
NTSTATUS 
DefaultDispatch(
	PDEVICE_OBJECT	DeviceObject, 
	PIRP			Irp, 
	ULONG			Seq, 
	BOOLEAN			bUsePerfCounter, 
	PLARGE_INTEGER	PerfCount
	)
{
	PIO_STACK_LOCATION		IrpStack;
	PMYCONTEXT				MyContext;
	NTSTATUS				status;
	PDRIVER_ENTRY			DrvEntry;
	PDEVICE_ENTRY			DevEntry;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	DevEntry = LookupEntryByDevObj(DeviceObject);
	if ( DevEntry )
	{
		switch(IrpStack->MajorFunction)
		{
		case IRP_MJ_READ:
			g_bRead = TRUE;
			InterlockedIncrement(&g_ReadCount);
			break;

		case IRP_MJ_WRITE:
			g_bWrite = TRUE;
			InterlockedIncrement(&g_WriteCount);
			break;

		default:
			break;
		}

		MyContext = ExAllocateFromNPagedLookasideList(&ContextLookaside);
		if ( MyContext )
		{
			MyContext->CompletionRoutine = IrpStack->CompletionRoutine;
			MyContext->Context = IrpStack->Context;
			MyContext->Control = IrpStack->Control;
			MyContext->MajorFunction = IrpStack->MajorFunction;
			MyContext->bUsePerfCounter = bUsePerfCounter;
			MyContext->PerfCount.QuadPart = PerfCount->QuadPart;
			MyContext->Seq = Seq;

			IrpStack->CompletionRoutine = MyCompletionRoutine;
			IrpStack->Context= MyContext;
			IrpStack->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;
		}

		status = (DevEntry->DrvEntry->DriverDispatch[IrpStack->MajorFunction])(DeviceObject, Irp);
	}
	else
	{
		DrvEntry = LookupEntryByDrvObj(DeviceObject->DriverObject);
		if ( !DrvEntry )
		{
			DbgPrint("***** UH-OH\n");
			return bUsePerfCounter;
		}

		status = (DrvEntry->DriverDispatch[IrpStack->MajorFunction])(DeviceObject, Irp);
	}
	
	return status;
}

//----- (08001203) --------------------------------------------------------
NTSTATUS 
DMCreateClose(
	PDEVICE_OBJECT	DeviceObject, 
	PIRP			Irp
	)
{
	PIO_STACK_LOCATION		IrpStack;
	NTSTATUS				status;
	LARGE_INTEGER			PerfCount;
	LARGE_INTEGER			CurrentTime;
	ULONG					seq;
	PDEVICE_ENTRY			DevEntry;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	if ( DeviceObject == g_pDeviceObject )
	{
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;

		g_bGUIActive = (IrpStack->MajorFunction == IRP_MJ_CREATE);

		IofCompleteRequest(Irp, 0);
		status = STATUS_SUCCESS;
	}
	else
	{
		if ( g_bUsePerfCounter )
		{
			PerfCount = KeQueryPerformanceCounter(NULL);
			CurrentTime.QuadPart = 0;
		}
		else
		{
			KeQuerySystemTime(&CurrentTime);
			PerfCount.QuadPart = 0;
		}

		seq = InterlockedIncrement(&Sequence);

		DevEntry = LookupEntryByDevObj(DeviceObject);
		if ( DevEntry )
		{
			LogRecord(
				seq,
				&CurrentTime,
				DevEntry->DiskNumber,
				"%s",
				IrpStack->MajorFunction == IRP_MJ_CLOSE ? "IRP_MJ_CLOSE" : "IRP_MJ_CREATE");
		}

		status = DefaultDispatch(DeviceObject, Irp, seq, g_bUsePerfCounter, &PerfCount);
	}

	return status;
}

//----- (080012CE) --------------------------------------------------------
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

//----- (0800130C) --------------------------------------------------------
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

//----- (0800134A) --------------------------------------------------------
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
	NewDevEntry = (PDEVICE_ENTRY)ExAllocatePool(0, 0x10);
	if ( NewDevEntry )
	{
		NewDevEntry->DeviceObject	= DeviceObject;
		NewDevEntry->DiskNumber		= PartitionIndex | (DiskIndex << 16);

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

			NewDrvEntry = (PDRIVER_ENTRY)ExAllocatePool(0, 0x78);
			if ( NewDrvEntry )
			{
				NewDrvEntry->DriverObject	= DeviceObject->DriverObject;
				NewDevEntry->DrvEntry	= NewDrvEntry;
				NewDrvEntry->Next			= g_pDrvObjList;

				g_pDrvObjList = NewDrvEntry;
				memcpy(NewDrvEntry->DriverDispatch, DeviceObject->DriverObject->MajorFunction, 0x6C);

				for(i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
				{
					if(g_pDriverObject->MajorFunction[i] != g_pDriverObject->MajorFunction[1])
					{
						DeviceObject->DriverObject->MajorFunction[i] = g_pDriverObject->MajorFunction[i];
					}
				}
			}
		}
	}

	KfReleaseSpinLock(&HashLock, OldIrql);
}

//----- (0800149E) --------------------------------------------------------
NTSTATUS 
DMReadWrite(
	PDEVICE_OBJECT	DeviceObject, 
	PIRP			Irp
	)
{
	PDEVICE_ENTRY		DevEntry;
	ULONG				SectorNum;
	ULONGLONG			ByteOffset;
	ULONGLONG			SectorOffset;
	PIO_STACK_LOCATION	IrpStack;
	NTSTATUS			status;
	LARGE_INTEGER		PerfCount;
	LARGE_INTEGER		CurrentTime;
	ULONG				seq;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);
	if ( g_bUsePerfCounter )
	{
		PerfCount = KeQueryPerformanceCounter(NULL);
		CurrentTime.QuadPart = 0;
	}
	else
	{
		KeQuerySystemTime(&CurrentTime);
		PerfCount.QuadPart = 0;
	}

	seq = InterlockedIncrement(&Sequence);

	DevEntry = LookupEntryByDevObj(DeviceObject);
	if ( DevEntry )
	{
		SectorNum		= IrpStack->Parameters.Read.Length >> 9;
		ByteOffset		= IrpStack->Parameters.Read.ByteOffset.QuadPart;
		SectorOffset	= GetSectorOffset(ByteOffset, 512);

		LogRecord(
			seq,
			&CurrentTime,
			DevEntry->DiskNumber,
			"%s\t%I64d\t%d",
			IrpStack->MajorFunction == IRP_MJ_READ ? "IRP_MJ_READ" : "IRP_MJ_WRITE", SectorOffset, SectorNum);
	}

	return DefaultDispatch(DeviceObject, Irp, seq, g_bUsePerfCounter, &PerfCount);
}

//----- (080015E0) --------------------------------------------------------
NTSTATUS 
DMDeviceIoCtl(
	IN PIRP		Irp, 
	IN PVOID	InputBuffer, 
	IN ULONG	InputBufferLength, 
	OUT PVOID	OutputBuffer, 
	IN ULONG	OutputBufferLength, 
	IN ULONG	IoControlCode
	)
{	
	NTSTATUS            status = STATUS_SUCCESS;
	PLOG_BUFFER         old;
	KIRQL				OldIrql;

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	switch ( IoControlCode ) {

	case IOCTL_DMON_ZEROSTATS/*0x83100004*/:
		{
			DbgPrint("Diskmon: zero stats\n");
			OldIrql = KfAcquireSpinLock(&LogBufferLock);

			while ( LogBuffer->Next )  
			{
				old = LogBuffer->Next;
				LogBuffer->Next = old->Next;
				ExFreePool( old );
				NumLogBuffer--;
			}

			LogBuffer->Len = 0;
			Sequence = 0;

			KfReleaseSpinLock(&LogBufferLock, OldIrql);
		}
		break;

	case IOCTL_DMON_GETSTATS/*0x8310000B*/:
		{
			DbgPrint("Diskmon: get stats\n");

			if ( MAX_STORE > OutputBufferLength ) 
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			OldIrql = KfAcquireSpinLock(&LogBufferLock);

			if ( LogBuffer->Len  ||  LogBuffer->Next )
			{
				DMonNewLogBuffer();
				old = DMonOldestLogBuffer();

				KfReleaseSpinLock(&LogBufferLock, OldIrql);

				memcpy(OutputBuffer, old->Data, old->Len);
				Irp->IoStatus.Information = old->Len;

				ExFreePool(old);
			}
			else
			{
				KfReleaseSpinLock(&LogBufferLock, OldIrql);
				Irp->IoStatus.Information = 0;
			}
		}
		break;

	case IOCTL_DMON_STOPFILTER/*0x83100010*/:
		DbgPrint("Diskmon: stop logging\n");

		g_bStartMon = FALSE;
		break;

	case IOCTL_DMON_STARTFILTER/*0x83100014*/:
		DbgPrint("Diskmon: start logging\n");

		g_bStartMon = TRUE;
		break;

	case 0x8310001C:
		g_bUsePerfCounter = *((PBOOLEAN)InputBuffer);
		break;

	case IOCTL_DMON_VERSION/*0x83100020*/://Version
		if ( OutputBufferLength >= 4 && OutputBuffer )
		{
			*(PULONG)OutputBuffer = DMONVERSION;
			Irp->IoStatus.Information = 4;
		}
		else
		{
			Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		}
		break;

	case 0x83100024:
		*(PULONG)OutputBuffer = 2;

		if ( g_WriteCount || g_bWrite )
		{
			*(PULONG)OutputBuffer = 1;
		}
		else if ( g_ReadCount || g_bRead )
		{
			*(PULONG)OutputBuffer = 0;
		}
		g_bRead = FALSE;
		g_bWrite = FALSE;

		Irp->IoStatus.Information = 4;
		break;

	default:
		DbgPrint("Diskmon: unknown IRP_MJ_DEVICE_CONTROL\n");
		Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	status = Irp->IoStatus.Status;
	IofCompleteRequest(Irp, 0);
	return status;
}

//----- (08001C0E) --------------------------------------------------------
PCHAR
GetIoctlName(
	PCHAR	Buffer, 
	ULONG	IoctlCode
	)
{
	switch( IoctlCode ) {

	case IOCTL_STORAGE_FIND_NEW_DEVICES:
		strcpy( Buffer, "IOCTL_STORAGE_FIND_NEW_DEVICES" );
		break;

	case IOCTL_STORAGE_RESET_BUS:
		strcpy( Buffer, "IOCTL_STORAGE_RESET_BUS" );
		break;

	case IOCTL_STORAGE_RESET_DEVICE:
		strcpy( Buffer, "IOCTL_STORAGE_RESET_DEVICE" );
		break;

	case IOCTL_STORAGE_RELEASE:
		strcpy( Buffer, "IOCTL_STORAGE_RELEASE" );
		break;

	case IOCTL_STORAGE_MEDIA_REMOVAL:
		strcpy( Buffer, "IOCTL_STORAGE_MEDIA_REMOVAL" );
		break;

	case IOCTL_STORAGE_EJECT_MEDIA:
		strcpy( Buffer, "IOCTL_STORAGE_EJECT_MEDIA" );
		break;

	case IOCTL_STORAGE_LOAD_MEDIA:
		strcpy( Buffer, "IOCTL_STORAGE_LOAD_MEDIA" );
		break;

	case IOCTL_STORAGE_RESERVE:
		strcpy( Buffer, "IOCTL_STORAGE_RESERVE" );
		break;

	case IOCTL_STORAGE_CHECK_VERIFY:
		strcpy( Buffer, "IOCTL_STORAGE_CHECK_VERIFY" );
		break;

	case IOCTL_STORAGE_GET_MEDIA_TYPES_EX:
		strcpy( Buffer, "IOCTL_STORAGE_GET_MEDIA_TYPES_EX" );
		break;

	case IOCTL_STORAGE_GET_DEVICE_NUMBER:
		strcpy( Buffer, "IOCTL_STORAGE_GET_DEVICE_NUMBER" );
		break;

	case IOCTL_STORAGE_PREDICT_FAILURE:
		strcpy( Buffer, "IOCTL_STORAGE_PREDICT_FAILURE" );
		break;

	case IOCTL_STORAGE_QUERY_PROPERTY:
		strcpy( Buffer, "IOCTL_STORAGE_QUERY_PROPERTY" );
		break;

	case IOCTL_STORAGE_GET_MEDIA_TYPES:
		strcpy( Buffer, "IOCTL_STORAGE_GET_MEDIA_TYPES" );
		break;

	case IOCTL_DISK_SET_DRIVE_LAYOUT:
		strcpy( Buffer, "IOCTL_DISK_SET_DRIVE_LAYOUT" );
		break;

	case IOCTL_DISK_FORMAT_TRACKS:
		strcpy( Buffer, "IOCTL_DISK_FORMAT_TRACKS" );
		break;

	case IOCTL_DISK_REASSIGN_BLOCKS:
		strcpy( Buffer, "IOCTL_DISK_REASSIGN_BLOCKS" );
		break;

	case IOCTL_DISK_FORMAT_TRACKS_EX:
		strcpy( Buffer, "IOCTL_DISK_FORMAT_TRACKS_EX" );
		break;

	case IOCTL_DISK_SET_PARTITION_INFO:
		strcpy( Buffer, "IOCTL_DISK_SET_PARTITION_INFO" );
		break;

	case IOCTL_DISK_RESERVE:
		strcpy( Buffer, "IOCTL_DISK_RESERVE" );
		break;

	case IOCTL_DISK_RELEASE:
		strcpy( Buffer, "IOCTL_DISK_RELEASE" );
		break;

	case IOCTL_DISK_FIND_NEW_DEVICES:
		strcpy( Buffer, "IOCTL_DISK_FIND_NEW_DEVICES" );
		break;

	case IOCTL_DISK_LOAD_MEDIA:
		strcpy( Buffer, "IOCTL_DISK_LOAD_MEDIA" );
		break;

	case IOCTL_DISK_GET_DRIVE_LAYOUT:
		strcpy( Buffer, "IOCTL_DISK_GET_DRIVE_LAYOUT" );
		break;

	case IOCTL_DISK_CHECK_VERIFY:
		strcpy( Buffer, "IOCTL_DISK_CHECK_VERIFY" );
		break;

	case IOCTL_DISK_MEDIA_REMOVAL:
		strcpy( Buffer, "IOCTL_DISK_MEDIA_REMOVAL" );
		break;

	case IOCTL_DISK_EJECT_MEDIA:
		strcpy( Buffer, "IOCTL_DISK_EJECT_MEDIA" );
		break;

	case IOCTL_DISK_GET_PARTITION_INFO:
		strcpy( Buffer, "IOCTL_DISK_GET_PARTITION_INFO" );
		break;

	case IOCTL_DISK_REQUEST_STRUCTURE:
		strcpy( Buffer, "IOCTL_DISK_REQUEST_STRUCTURE" );
		break;

	case IOCTL_DISK_REQUEST_DATA:
		strcpy( Buffer, "IOCTL_DISK_REQUEST_DATA" );
		break;

	case IOCTL_DISK_INTERNAL_SET_VERIFY:
		strcpy( Buffer, "IOCTL_DISK_INTERNAL_SET_VERIFY" );
		break;

	case IOCTL_DISK_INTERNAL_CLEAR_VERIFY:
		strcpy( Buffer, "IOCTL_DISK_INTERNAL_CLEAR_VERIFY" );
		break;

	case IOCTL_DISK_LOGGING:
		strcpy( Buffer, "IOCTL_DISK_LOGGING" );
		break;

	case IOCTL_DISK_GET_DRIVE_GEOMETRY:
		strcpy( Buffer, "IOCTL_DISK_GET_DRIVE_GEOMETRY" );
		break;

	case IOCTL_DISK_VERIFY:
		strcpy( Buffer, "IOCTL_DISK_VERIFY" );
		break;

	case IOCTL_DISK_PERFORMANCE:
		strcpy( Buffer, "IOCTL_DISK_PERFORMANCE" );
		break;

	case IOCTL_DISK_IS_WRITABLE:
		strcpy( Buffer, "IOCTL_DISK_IS_WRITABLE" );
		break;

	default:
		sprintf( Buffer, "* 0x%X", IoctlCode );
		break;		
	}

	return Buffer;
}

//----- (08001F19) --------------------------------------------------------
NTSTATUS 
DMDeviceControl(
	PDEVICE_OBJECT	DeviceObject, 
	PIRP			Irp
	)
{
	PIO_STACK_LOCATION	IrpStack;
	PVOID				OutputBuffer;
	PVOID				InputBuffer;
	ULONG				InputLength;
	ULONG				OutputLength;
	NTSTATUS			status;
	PDEVICE_ENTRY		DevEntry;
	LARGE_INTEGER		CurrentTime;
	LARGE_INTEGER		PerfCount; 
	ULONG				seq;
	char				IoctlName[64] = "";
	PCHAR				pIoCtlName;
	ULONG				IoControlCode;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);
	IoControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;

	if ( DeviceObject == g_pDeviceObject )
	{
		InputBuffer = Irp->AssociatedIrp.SystemBuffer;
		InputLength = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
		OutputLength = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
		OutputBuffer = Irp->AssociatedIrp.SystemBuffer;

		if ( METHOD_FROM_CTL_CODE(IoControlCode) == METHOD_NEITHER )
		{
			OutputBuffer = Irp->UserBuffer;
		}

		status = DMDeviceIoCtl(Irp, InputBuffer, InputLength, OutputBuffer, OutputLength, IoControlCode);
	}
	else
	{
		if ( g_bUsePerfCounter )
		{
			CurrentTime.QuadPart = 0;
			PerfCount = KeQueryPerformanceCounter(0);
		}
		else
		{
			KeQuerySystemTime(&CurrentTime);
			PerfCount.QuadPart = 0;
		}
		seq = InterlockedIncrement(&Sequence);
		DevEntry = LookupEntryByDevObj(DeviceObject);
		if ( DevEntry )
		{
			pIoCtlName = GetIoctlName(IoctlName, IrpStack->Parameters.DeviceIoControl.IoControlCode);
			LogRecord(
				seq,
				&CurrentTime,
				DevEntry->DiskNumber,
				"%s(%s)",
				IrpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ? "IRP_MJ_DEVICE_CONTROL" : "IRP_MJ_INTERNAL_DEVICE_CONTROL",
				IoctlName);
		}

		status = DefaultDispatch(DeviceObject, Irp, seq, g_bUsePerfCounter, &PerfCount);
		if ( IoControlCode == IOCTL_DISK_FIND_NEW_DEVICES || IoControlCode == IOCTL_DISK_SET_DRIVE_LAYOUT )
		{
			if ( NT_SUCCESS(status) )
			{
				HookDispatch(DeviceObject->DriverObject, 0);
			}
		}
	}

	return status;
}

//----- (08002059) --------------------------------------------------------
NTSTATUS 
DMShutDownFlushBuffer(
	PDEVICE_OBJECT	DeviceObject, 
	PIRP			Irp
	)
{
	ULONG					seq;
	PIO_STACK_LOCATION		IrpStack;	
	LARGE_INTEGER			PerfCount;
	LARGE_INTEGER			CurrentTime;
	PDEVICE_ENTRY			DevEntry;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	if ( g_bUsePerfCounter )
	{
		PerfCount = KeQueryPerformanceCounter(NULL);
		CurrentTime.QuadPart = 0;
	}
	else
	{
		KeQuerySystemTime(&CurrentTime);
		PerfCount.QuadPart = 0;
	}

	seq = InterlockedIncrement(&Sequence);

	DevEntry = LookupEntryByDevObj(DeviceObject);
	if ( DevEntry )
	{
		LogRecord(
			seq,
			&CurrentTime,
			DevEntry->DiskNumber,
			"%s",
			IrpStack->MajorFunction == IRP_MJ_SHUTDOWN ? "IRP_MJ_SHUTDOWN" : "IRP_MJ_FLUSH_BUFFERS");
	}

	return DefaultDispatch(DeviceObject, Irp, seq, g_bUsePerfCounter, &PerfCount);
}

//----- (08002100) --------------------------------------------------------
ULONGLONG 
GetSectorOffset(
	ULONGLONG	ByteOffset, 
	ULONG		SectorSize
	)
{
	return ByteOffset / SectorSize;
}