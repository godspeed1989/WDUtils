#include "DiskFilter.h"
#include "Utils.h"
#include "Queue.h"

#ifdef WRITE_BACK_ENABLE

VOID DF_WriteBackThread(PVOID Context)
{
	PVOID					Data;
	PDF_DEVICE_EXTENSION	DevExt;
	KIRQL  					Irql;
	PCACHE_BLOCK			pBlock;
	LARGE_INTEGER			Offset = {0};

	DevExt = (PDF_DEVICE_EXTENSION)Context;
	Data = ExAllocatePoolWithTag(NonPagedPool, BLOCK_SIZE, 'tmpb');
	ASSERT(Data);
	// set thread priority.
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
	KdPrint(("%u-%u: Write Back Thread Start...\n", DevExt->DiskNumber, DevExt->PartitionNumber));
	for (;;)
	{
		KeWaitForSingleObject(&DevExt->CachePool.WbThreadStartEvent,
			Executive, KernelMode, FALSE, NULL);

		// Write Back Strategy
		if (DevExt->CachePool.WbQueue.Used == 0)
			continue;
		if (DevExt->CachePool.WbFlushAll == FALSE &&
			DevExt->CachePool.WbQueue.Used < DevExt->CachePool.WbQueue.Size)
			continue;

		// Flush Back All Data
		KeAcquireSpinLock(&DevExt->CachePool.WbQueueSpinLock, &Irql);
		while (NULL != (pBlock = QueueRemove(&DevExt->CachePool.WbQueue)))
		{
			Offset.QuadPart = pBlock->Index * BLOCK_SIZE;
			//pBlock
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
		KeReleaseSpinLock(&DevExt->CachePool.WbQueueSpinLock, Irql);
		KeSetEvent(&DevExt->CachePool.WbThreadFinishEvent, IO_NO_INCREMENT, FALSE);

		if (DevExt->bTerminalWbThread)
		{
			ExFreePoolWithTag(Data, 'tmpb');
			KdPrint(("Write Back Thread Exit...\n"));
			PsTerminateSystemThread(STATUS_SUCCESS);
			return;
		}
	} // forever loop
}

#endif
