#include "DiskFilter.h"
#include "Utils.h"
#include "Queue.h"

#ifdef WRITE_BACK_ENABLE

VOID DF_WriteBackThread(PVOID Context)
{
	PUCHAR					Data;
	PDF_DEVICE_EXTENSION	DevExt;
	PCACHE_BLOCK			pBlock;
	LARGE_INTEGER			Offset;
	ULONG					Accumulate;
	LONGLONG				LastIndex;

	DevExt = (PDF_DEVICE_EXTENSION)Context;
	Data = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, WB_QUEUE_SIZE << 20, 'tmpb');
	ASSERT(Data);
	// set thread priority.
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
	KdPrint(("%u-%u: Write Back Thread Start...\n", DevExt->DiskNumber, DevExt->PartitionNumber));
	for (;;)
	{
		KeWaitForSingleObject(&DevExt->CachePool.WbThreadStartEvent,
			Executive, KernelMode, FALSE, NULL);

		// Write Back Strategy
		if (DevExt->bTerminalWbThread == FALSE &&
			DevExt->CachePool.WbFlushAll == FALSE &&
			DevExt->CachePool.WbQueue.Used < DevExt->CachePool.WbQueue.Size)
			continue;

		// Flush Back All Data
		Offset.QuadPart = -1;
		Accumulate = 0;
		LastIndex = -1;
		spin_lock(&DevExt->CachePool.WbQueueSpinLock);
		while (NULL != (pBlock = QueueRemove(&DevExt->CachePool.WbQueue)))
	#if 0
		{
			Offset.QuadPart = pBlock->Index * BLOCK_SIZE;
			StoragePoolRead(&DevExt->CachePool.Storage,
							Data, pBlock->StorageIndex, 0, BLOCK_SIZE);
			IoDoRWRequestSync (
				IRP_MJ_WRITE,
				DevExt->LowerDeviceObject,
				Data,
				BLOCK_SIZE,
				&Offset
			);
			pBlock->Modified = FALSE;
		}
	#else
		// Merge Consecutive Writes
		{
			if (Accumulate == 0)
			{
				Offset.QuadPart = pBlock->Index * BLOCK_SIZE;
				StoragePoolRead(&DevExt->CachePool.Storage,
								Data + 0,
								pBlock->StorageIndex, 0, BLOCK_SIZE);
				Accumulate++;
				LastIndex = pBlock->Index;
			}
			else if (pBlock->Index == LastIndex + 1)
			{
				StoragePoolRead(&DevExt->CachePool.Storage,
								Data + Accumulate*BLOCK_SIZE,
								pBlock->StorageIndex, 0, BLOCK_SIZE);
				Accumulate++;
				LastIndex++;
			}
			else
			{
				IoDoRWRequestSync (
					IRP_MJ_WRITE,
					DevExt->LowerDeviceObject,
					Data,
					Accumulate * BLOCK_SIZE,
					&Offset
				);
				// Restart
				StoragePoolRead(&DevExt->CachePool.Storage,
								Data + 0,
								pBlock->StorageIndex, 0, BLOCK_SIZE);
				Offset.QuadPart = pBlock->Index * BLOCK_SIZE;
				Accumulate = 1;
				LastIndex = pBlock->Index;
			}
			pBlock->Modified = FALSE;
		}
		if (Accumulate)
		{
			IoDoRWRequestSync (
				IRP_MJ_WRITE,
				DevExt->LowerDeviceObject,
				Data,
				Accumulate * BLOCK_SIZE,
				&Offset
			);
		}
	#endif
		spin_unlock(&DevExt->CachePool.WbQueueSpinLock);
		KeSetEvent(&DevExt->CachePool.WbThreadFinishEvent, IO_NO_INCREMENT, FALSE);

		if (DevExt->bTerminalWbThread)
		{
			ExFreePoolWithTag(Data, 'tmpb');
			KdPrint(("%u-%u: Write Back Thread Exit...\n", DevExt->DiskNumber, DevExt->PartitionNumber));
			PsTerminateSystemThread(STATUS_SUCCESS);
			return;
		}
	} // forever loop
}

#endif
