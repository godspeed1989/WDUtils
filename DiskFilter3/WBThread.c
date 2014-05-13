#include "DiskFilter.h"
#include "Utils.h"
#include "List.h"

#ifdef WRITE_BACK_ENABLE

#define MERGE_CONSECUTIVE
#define CONSECUTIVE_BUF_SIZE    4   /* MB */
#define NUM_CONSECUTIVE_BLOCK   ((CONSECUTIVE_BUF_SIZE << 20) / BLOCK_SIZE)
#define WB_INTERVAL             2   /* Seconds */

#define _DEBUG

VOID DF_WriteBackThread(PVOID Context)
{
    PUCHAR                  Data;
    PDF_DEVICE_EXTENSION    DevExt;
    KIRQL                   Irql;
    PCACHE_BLOCK            pBlock;
    LARGE_INTEGER           Offset;
    ULONG                   Accumulate;
    LONGLONG                LastIndex;
    PLIST_ENTRY             ReqEntry;

    DevExt = (PDF_DEVICE_EXTENSION)Context;
    Data = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, CONSECUTIVE_BUF_SIZE << 20, 'tmpb');
    ASSERT(Data);
    // set thread priority.
    KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
    KdPrint(("%u-%u: Write Back Thread Start...\n", DevExt->DiskNumber, DevExt->PartitionNumber));
    for (;;)
    {
        ULONG           i;
        KTIMER	        timer;
        LARGE_INTEGER   timeout;
        PVOID           Object[2] = {&timer, &DevExt->CachePool.WbThreadStartEvent};
    #define s2us(_s_) (_s_*1000*1000)
        timeout.QuadPart = s2us(WB_INTERVAL) * -10;
        KeInitializeTimerEx(&timer, NotificationTimer);
        KeSetTimer(&timer, timeout, NULL);
        KeWaitForMultipleObjects(2, Object, WaitAny, Executive, KernelMode, FALSE, NULL, NULL);
        KeCancelTimer(&timer);

        // Write Back Strategy
        if (IsListEmpty(&DevExt->CachePool.WbList))
            continue;
        for (i = 0; i < 3; i++)
        {
            if (DevExt->CachePool.WbQueueLock == 0)
            {
                LOCK_WB_QUEUE(&DevExt->CachePool.WbQueueLock);
                break;
            }
        }
        if (i == 3)
            continue;
    #ifdef _DEBUG
        DbgPrint("%s: Flush Start\n", __FUNCTION__);
    #endif
        i = 0;
        // Flush Back Data
        Offset.QuadPart = -1;
        Accumulate = 0;
        LastIndex = -1;
        while (!IsListEmpty(&DevExt->CachePool.WbList))
        {
            ReqEntry = RemoveHeadList(&DevExt->CachePool.WbList);
            pBlock = CONTAINING_RECORD(ReqEntry, CACHE_BLOCK, ListEntry);
            if (pBlock->Modified == FALSE)
                continue;
        #ifndef MERGE_CONSECUTIVE
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
        #else // Merge Consecutive Writes
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
        #endif
            i++;
        }
    #ifdef MERGE_CONSECUTIVE
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
        DbgPrint("%s: Flush %d Finished\n", __FUNCTION__, i);
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
