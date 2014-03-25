#include "DiskFilter.h"
#include "Utils.h"

VOID DF_ReadWriteThread(PVOID Context)
{
	PIRP					Irp;
	PLIST_ENTRY				ReqEntry;
	LONGLONG				Offset;
	ULONG					Length;
	PUCHAR					SysBuf;
	PIO_STACK_LOCATION		IrpSp;
	PDF_DEVICE_EXTENSION	DevExt;
	DevExt = (PDF_DEVICE_EXTENSION)Context;

	// set thread priority.
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
	KdPrint(("%u-%u: Read Write Thread Start...\n", DevExt->DiskNumber, DevExt->PartitionNumber));
	for (;;)
	{
		KeWaitForSingleObject(&DevExt->RwThreadEvent,
			Executive, KernelMode, FALSE, NULL);
		if (DevExt->bTerminalRwThread)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
			KdPrint(("Read Write Thread Exit...\n"));
			return;
		}

		while (NULL != (ReqEntry = ExInterlockedRemoveHeadList(
						&DevExt->RwList, &DevExt->RwListSpinLock)))
		{
			Irp = CONTAINING_RECORD(ReqEntry, IRP, Tail.Overlay.ListEntry);
			IrpSp = IoGetCurrentIrpStackLocation(Irp);

			IoSetCancelRoutine (Irp, NULL);
			if (Irp->Cancel)
			{
				continue;
			}

			// Get system buffer address
			if (Irp->MdlAddress != NULL)
				SysBuf = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
			else
				SysBuf = (PUCHAR)Irp->UserBuffer;
			if (SysBuf == NULL)
				SysBuf = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;

			// Get offset and length
			if (IRP_MJ_READ == IrpSp->MajorFunction)
			{
				Offset = IrpSp->Parameters.Read.ByteOffset.QuadPart;
				Length = IrpSp->Parameters.Read.Length;
			}
			else if (IRP_MJ_WRITE == IrpSp->MajorFunction)
			{
				Offset = IrpSp->Parameters.Write.ByteOffset.QuadPart;
				Length = IrpSp->Parameters.Write.Length;
			}
			else
			{
				Offset = 0;
				Length = 0;
			}

			if (!SysBuf || !Length) // Ignore this IRP.
			{
				DbgPrint("Wired IRP : %p %x :\n", SysBuf, Length);
				IoSkipCurrentIrpStackLocation(Irp);
				IoCallDriver(DevExt->LowerDeviceObject, Irp);
				continue;
			}

			if (IrpSp->MajorFunction == IRP_MJ_READ)
			{
				DevExt->ReadCount++;
				DBG_PRINT(DBG_TRACE_RW, ("%u-%u: R off(%I64d) len(%d)\n", DevExt->DiskNumber, DevExt->PartitionNumber, Offset, Length));
			}
			else
			{
				DevExt->WriteCount++;
				DBG_PRINT(DBG_TRACE_RW, ("%u-%u: W off(%I64d) len(%d)\n", DevExt->DiskNumber, DevExt->PartitionNumber, Offset, Length));
			}

			// Read Request
			if (IrpSp->MajorFunction == IRP_MJ_READ)
			{
				// Cache Full Hitted
				if (QueryAndCopyFromCachePool(
						&DevExt->CachePool,
						SysBuf,
						Offset,
						Length
					#ifdef READ_VERIFY
						,DevExt->LowerDeviceObject
						,DevExt->DiskNumber
						,DevExt->PartitionNumber
					#endif
						) == TRUE)
				{
					DBG_PRINT(DBG_TRACE_CACHE, ("hit:%u-%u: off(%I64d) len(%d)\n", DevExt->DiskNumber, DevExt->PartitionNumber, Offset, Length));
					DevExt->CacheHit++;
					Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = Length;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				}
				else
				{
					Irp->IoStatus.Information = 0;
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					ForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
					if (NT_SUCCESS(Irp->IoStatus.Status))
					{
						UpdataCachePool(&DevExt->CachePool,
										SysBuf, Offset,
										Length, _READ_
									#ifdef READ_VERIFY
										,DevExt->LowerDeviceObject
										,DevExt->DiskNumber
										,DevExt->PartitionNumber
									#endif
										);
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
			}
			// Write Request
			else
			{
				Irp->IoStatus.Information = 0;
				Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				ForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
				if (NT_SUCCESS(Irp->IoStatus.Status))
				{
					UpdataCachePool(&DevExt->CachePool,
									SysBuf, Offset,
									Length, _WRITE_
								#ifdef READ_VERIFY
									,DevExt->LowerDeviceObject
									,DevExt->DiskNumber
									,DevExt->PartitionNumber
								#endif
									);
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				}
				else
				{
					Irp->IoStatus.Information = 0;
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				}
			}
		} // while list not empty
	} // forever loop
}
