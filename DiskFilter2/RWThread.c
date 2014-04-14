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
		KeWaitForSingleObject(&DevExt->RwThreadStartEvent, Executive, KernelMode, FALSE, NULL);
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
				DevExt->ReadCount += Length / BLOCK_SIZE;
				DBG_PRINT(DBG_TRACE_RW, ("%u-%u: R off(%I64d) len(%d)\n", DevExt->DiskNumber, DevExt->PartitionNumber, Offset, Length));
			}
			else
			{
				DevExt->WriteCount += Length / BLOCK_SIZE;
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
					DBG_PRINT(DBG_TRACE_CACHE, ("rhit:%u-%u: off(%I64d) len(%d)\n", DevExt->DiskNumber, DevExt->PartitionNumber, Offset, Length));
					DevExt->CachePool.ReadHit += Length / BLOCK_SIZE;
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
						ReadUpdateCachePool(&DevExt->CachePool,
											SysBuf, Offset, Length
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
				PUCHAR origBuf;
				LONGLONG origOffset;
				LARGE_INTEGER writeOffset;
				BOOLEAN front_broken, end_broken;
				ULONG front_offset, front_skip, end_cut, origLength;

				origBuf = SysBuf;
				origLength = Length;
				origOffset = Offset;
				writeOffset.QuadPart = Offset;
				// Cache Full Hitted
				if (QueryAndWriteToCachePool(
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
					DBG_PRINT(DBG_TRACE_CACHE, ("whit:%u-%u: off(%I64d) len(%d)\n", DevExt->DiskNumber, DevExt->PartitionNumber, Offset, Length));
					DevExt->CachePool.WriteHit += Length / BLOCK_SIZE;
				}
				else
				{
				#ifdef WRITE_BACK_ENABLE
					detect_broken(Offset, Length, front_broken, end_broken, front_offset, front_skip, end_cut);
					if (front_broken == TRUE)
					{
						IoDoRWRequestSync (
							IRP_MJ_WRITE,
							DevExt->LowerDeviceObject,
							SysBuf,
							front_skip,
							&writeOffset
						);
						SysBuf += front_skip;
						writeOffset.QuadPart += front_skip;
					}
				#endif
					WriteUpdateCachePool(&DevExt->CachePool,
										origBuf, origOffset, origLength
									#ifdef READ_VERIFY
										,DevExt->LowerDeviceObject
										,DevExt->DiskNumber
										,DevExt->PartitionNumber
									#endif
										);
				#ifdef WRITE_BACK_ENABLE
					SysBuf += Length;
					writeOffset.QuadPart += Length;
					if (end_broken == TRUE)
					{
						IoDoRWRequestSync (
							IRP_MJ_WRITE,
							DevExt->LowerDeviceObject,
							SysBuf,
							end_cut,
							&writeOffset
						);
						SysBuf += end_cut;
						writeOffset.QuadPart += end_cut;
					}
					ASSERT (SysBuf - origBuf == origLength);
					ASSERT (writeOffset.QuadPart - origOffset == origLength);

					Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = origLength;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				#endif
				}
			#ifndef WRITE_BACK_ENABLE
				Irp->IoStatus.Information = 0;
				Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				ForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
				if (!NT_SUCCESS(Irp->IoStatus.Status))
				{
					DbgPrint("Write Failed: %I64d %d\n", origOffset, origLength);
					Irp->IoStatus.Information = 0;
					Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				}
				else
				{
					Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = origLength;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);
					continue;
				}
			#endif
				continue;
			}
		} // while list not empty
		KeSetEvent(&DevExt->RwThreadFinishEvent, IO_NO_INCREMENT, FALSE);

		if (DevExt->bTerminalRwThread)
		{
			KdPrint(("%u-%u: Read Write Thread Exit...\n", DevExt->DiskNumber, DevExt->PartitionNumber));
			PsTerminateSystemThread(STATUS_SUCCESS);
			return;
		}
	} // forever loop
}
