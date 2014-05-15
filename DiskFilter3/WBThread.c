#include "DiskFilter.h"
#include "Utils.h"
#include "Queue.h"

#ifdef WRITE_BACK_ENABLE

#define MERGE_CONSECUTIVE
#define CONSECUTIVE_BUF_SIZE    4    /* MB */
#define NUM_CONSECUTIVE_BLOCK   ((CONSECUTIVE_BUF_SIZE << 20) / BLOCK_SIZE)

//#define _DEBUG

VOID DF_WriteBackThread(PVOID Context)
{
    PUCHAR                  Data;
    PDF_DEVICE_EXTENSION    DevExt;
    PCACHE_BLOCK            pBlock;
    LARGE_INTEGER           Offset;
    ULONG                   Accumulate;
    LONGLONG                LastIndex;

    DevExt = (PDF_DEVICE_EXTENSION)Context;
    Data = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, CONSECUTIVE_BUF_SIZE << 20, 'tmpb');
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

    #ifdef _DEBUG
        DbgPrint("%s: Flush Start\n", __FUNCTION__);
    #endif
        // Flush Back All Data
        Offset.QuadPart = -1;
        Accumulate = 0;
        LastIndex = -1;
        LOCK_WB_QUEUE(&DevExt->CachePool.WbQueueLock);
        while (NULL != (pBlock = QueueRemove(&DevExt->CachePool.WbQueue)))
    #ifndef MERGE_CONSECUTIVE
        {
            Offset.QuadPart = pBlock->Index * BLOCK_SIZE;
            StoragePoolRead(&DevExt->CachePool.Storage,
                             Data + 0,
                             pBlock->StorageIndex, 0, BLOCK_SIZE);
            IoDoRWRequestSync (
                IRP_MJ_WRITE,
                DevExt->LowerDeviceObject,
                Data,
                BLOCK_SIZE,
                &Offset,
                2
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
            else if (pBlock->Index == LastIndex + 1 && Accumulate < NUM_CONSECUTIVE_BLOCK)
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
                    &Offset,
                    2
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
                &Offset,
                2
            );
        }
    #endif
        UNLOCK_WB_QUEUE(&DevExt->CachePool.WbQueueLock);
        KeSetEvent(&DevExt->CachePool.WbThreadFinishEvent, IO_NO_INCREMENT, FALSE);
    #ifdef _DEBUG
        DbgPrint("%s: Flush Finished\n", __FUNCTION__);
    #endif
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
