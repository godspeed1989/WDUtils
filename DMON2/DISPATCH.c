#include "DMON.h"

//----------------------------------------------------------------------
//                     D I S P A T C H S
//----------------------------------------------------------------------

#define ENTER_DISPATCH    InterlockedIncrement(&g_uDispatchCount)
#define LEAVE_DISPATCH    InterlockedDecrement(&g_uDispatchCount)

NTSTATUS
MyCompletionRoutine(
	PDEVICE_OBJECT	DeviceObject,
	PIRP			Irp,
	PVOID			Context
	)
{
	NTSTATUS				status;
	UCHAR					Control;
	PMYCONTEXT				MyContext;
	PIO_COMPLETION_ROUTINE	CompletionRoutine;

	MyContext = (PMYCONTEXT)Context;

	Control = MyContext->Control;
	Context = MyContext->Context;
	CompletionRoutine = MyContext->CompletionRoutine;

	ExFreeToNPagedLookasideList(&ContextLookaside, MyContext);

	LEAVE_DISPATCH;

	status = Irp->IoStatus.Status;
	if( NT_SUCCESS(status) )
	{
		if(Control & SL_INVOKE_ON_SUCCESS)
		{
			return CompletionRoutine(DeviceObject, Irp, Context);
		}
	}
	else if(status == STATUS_CANCELLED)
	{
		if(Control & SL_INVOKE_ON_CANCEL)
		{
			return CompletionRoutine(DeviceObject, Irp, Context);
		}
	}
	else
	{
		if(Control & SL_INVOKE_ON_ERROR)
		{
			return CompletionRoutine(DeviceObject, Irp, Context);
		}
	}
	return status;
}

static NTSTATUS
DefaultDispatch(
	PDEVICE_OBJECT	DeviceObject,
	PIRP			Irp
	)
{
	NTSTATUS				status;
	PIO_STACK_LOCATION		IrpStack;
	PMYCONTEXT				MyContext;
	PDRIVER_ENTRY			DrvEntry;
	PDEVICE_ENTRY			DevEntry;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	DevEntry = LookupEntryByDevObj(DeviceObject);
	if ( DevEntry )
	{
		MyContext = ExAllocateFromNPagedLookasideList(&ContextLookaside);
		if ( MyContext )
		{
			MyContext->Context = IrpStack->Context;
			MyContext->Control = IrpStack->Control;
			MyContext->MajorFunction = IrpStack->MajorFunction;
			MyContext->CompletionRoutine = IrpStack->CompletionRoutine;

			IrpStack->CompletionRoutine = MyCompletionRoutine;
			IrpStack->Context = MyContext;
			IrpStack->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;
		}
		ENTER_DISPATCH;
		status = (DevEntry->DrvEntry->DriverDispatch[IrpStack->MajorFunction])(DeviceObject, Irp);
	}
	else
	{
		DrvEntry = LookupEntryByDrvObj(DeviceObject->DriverObject);
		if ( !DrvEntry )
		{
			DbgPrint("***** UH-OH\n");
			return STATUS_DEVICE_NOT_READY;
		}

		status = (DrvEntry->DriverDispatch[IrpStack->MajorFunction])(DeviceObject, Irp);
	}

	return status;
}

NTSTATUS
DMCreateClose(
	PDEVICE_OBJECT	DeviceObject,
	PIRP			Irp
	)
{
	NTSTATUS				status;
	PIO_STACK_LOCATION		IrpStack;
	PDEVICE_ENTRY			DevEntry;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	if ( DeviceObject == g_pDeviceObject )
	{
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
		KdPrint(("DMon: opened\n"));
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		status = STATUS_SUCCESS;
	}
	else
	{
		DevEntry = LookupEntryByDevObj(DeviceObject);
		if (DevEntry && g_bStartMon)
		{
			KdPrint(("%u-%u: Create\n", DevEntry->DiskNumber, DevEntry->PartitionNumber));
		}
		status = DefaultDispatch(DeviceObject, Irp);
	}

	return status;
}

NTSTATUS
DMReadWrite(
	PDEVICE_OBJECT	DeviceObject,
	PIRP			Irp
	)
{
	NTSTATUS			status;
	PDEVICE_ENTRY		DevEntry;
	PUCHAR				SysBuf, SysBuf1;
	ULONG				Length;
	ULONGLONG			Offset;
	PIO_STACK_LOCATION	IrpStack;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	DevEntry = LookupEntryByDevObj(DeviceObject);
	if (DevEntry && g_bStartMon)
	{
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
		if (IRP_MJ_READ == IrpStack->MajorFunction)
		{
			Offset = IrpStack->Parameters.Read.ByteOffset.QuadPart;
			Length = IrpStack->Parameters.Read.Length;
		}
		else if (IRP_MJ_WRITE == IrpStack->MajorFunction)
		{
			Offset = IrpStack->Parameters.Write.ByteOffset.QuadPart;
			Length = IrpStack->Parameters.Write.Length;
		}
		else
		{
			Offset = 0;
			Length = 0;
		}
		if (!SysBuf || !Length) // Ignore this IRP.
		{
			status = DefaultDispatch(DeviceObject, Irp);
			goto ret;
		}

		// Read or Write
		if (IrpStack->MajorFunction == IRP_MJ_READ)
		{
			KdPrint(("%u-%u: R %p off=%I64d, len=%u\n", DevEntry->DiskNumber, DevEntry->PartitionNumber,
						SysBuf, Offset/DevEntry->SectorSize, Length/DevEntry->SectorSize));
			InterlockedIncrement(&DevEntry->ReadCount);
			// if matched
			if ( TRUE == QueryAndCopyFromCachePool(&DevEntry->CachePool,
													SysBuf, Offset, Length) )
			{
				KdPrint(("cache hit\n"));
				status = STATUS_SUCCESS;
				Irp->IoStatus.Status = STATUS_SUCCESS;
				Irp->IoStatus.Information = Length;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);
			}
			else
			{
				status = DefaultDispatch(DeviceObject, Irp);
				if (NT_SUCCESS(Irp->IoStatus.Status) && Irp->IoStatus.Information != 0)
				{
					UpdataCachePool(&DevEntry->CachePool,
									SysBuf,
									Offset,
									Irp->IoStatus.Information,
									_READ_);
				}
			}
		}
		else
		{
			//KdPrint(("%u-%u: W %p off=%I64d, len=%u\n", DevEntry->DiskNumber, DevEntry->PartitionNumber,
			//			SysBuf, Offset/DevEntry->SectorSize, Length/DevEntry->SectorSize));
			InterlockedIncrement(&DevEntry->WriteCount);
			// Write through
			status = DefaultDispatch(DeviceObject, Irp);
			if (NT_SUCCESS(Irp->IoStatus.Status) && Irp->IoStatus.Information != 0)
			{
				UpdataCachePool(&DevEntry->CachePool,
								SysBuf,
								Offset,
								Irp->IoStatus.Information,
								_WRITE_);
			}
		}
	}
	else
	{
		status = DefaultDispatch(DeviceObject, Irp);
	}
ret:
	return status;
}

static NTSTATUS
DMDeviceIoCtl(
	IN PIRP		Irp,
	IN PVOID	InputBuffer,
	IN ULONG	InputBufferLength,
	OUT PVOID	OutputBuffer,
	IN ULONG	OutputBufferLength,
	IN ULONG	IoControlCode
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	switch ( IoControlCode )
	{
	case IOCTL_DMON_ZEROSTATS:
		{
			DbgPrint("Diskmon: zero stats\n");
			//todo
		}
		break;
	case IOCTL_DMON_GETSTATS:
		{
			DbgPrint("Diskmon: get stats\n");
			//todo
		}
		break;
	case IOCTL_DMON_STOPFILTER:
		{
			DbgPrint("Diskmon: stop logging\n");
			g_bStartMon = FALSE;
		}
		break;
	case IOCTL_DMON_STARTFILTER:
		{
			DbgPrint("Diskmon: start logging\n");
			g_bStartMon = TRUE;
		}
		break;
	default:
		{
			DbgPrint("Diskmon: unknown IRP_MJ_DEVICE_CONTROL\n");
			Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		}
		break;
	}

	status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS
DMDeviceControl(
	PDEVICE_OBJECT	DeviceObject,
	PIRP			Irp
	)
{
	NTSTATUS			status;
	PIO_STACK_LOCATION	IrpStack;
	PVOID				InputBuffer;
	ULONG				InputLength;
	PVOID				OutputBuffer;
	ULONG				OutputLength;
	PDEVICE_ENTRY		DevEntry;
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
		DevEntry = LookupEntryByDevObj(DeviceObject);
		if ( DevEntry && g_bStartMon)
		{
			//log it
			KdPrint(("%u-%u: %s\n", DevEntry->DiskNumber, DevEntry->PartitionNumber, "IRP_MJ_DEVICE_CONTROL"));
		}

		status = DefaultDispatch(DeviceObject, Irp);
	}

	return status;
}

NTSTATUS
DMShutDownFlushBuffer(
	PDEVICE_OBJECT	DeviceObject,
	PIRP			Irp
	)
{
	NTSTATUS				status;
	PIO_STACK_LOCATION		IrpStack;
	PDEVICE_ENTRY			DevEntry;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	DevEntry = LookupEntryByDevObj(DeviceObject);
	if (DevEntry && g_bStartMon)
	{
		KdPrint(("%u-%u: %s\n", DevEntry->DiskNumber, DevEntry->PartitionNumber, IrpStack->MajorFunction == IRP_MJ_SHUTDOWN ? "IRP_MJ_SHUTDOWN" : "IRP_MJ_FLUSH_BUFFERS"));
	}
	status = DefaultDispatch(DeviceObject, Irp);

	return status;
}
