#include "Cache.h"
#include "List.h"
#include "Utils.h"

#if defined(USE_OCP)

#define HOT_RATIO    2
BOOLEAN InitCachePool (PCACHE_POOL CachePool
                       #ifndef USE_DRAM
                       ,ULONG DiskNum ,ULONG PartitionNum
                       #endif
                      )
{
    BOOLEAN ret;

    CachePool->Size = CachePool->Used = 0;
    CachePool->ReadHit = CachePool->WriteHit = 0;
    CachePool->Size = CACHE_POOL_NUM_BLOCKS;
    CachePool->HotSize = CachePool->Size / HOT_RATIO;
    CachePool->ColdSize = CachePool->Size - CachePool->HotSize;

    ret = InitStoragePool(&CachePool->Storage, CachePool->Size
        #ifndef USE_DRAM
            ,DiskNum ,PartitionNum
        #endif
        );
    if (ret == FALSE)
        goto l_error;
    CachePool->hot_bpt_root = NULL;
    CachePool->cold_bpt_root = NULL;
    InitList(&CachePool->HotList);
    InitList(&CachePool->ColdList);
    return TRUE;
l_error:
    DestroyStoragePool(&CachePool->Storage);
    return FALSE;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
    DestroyList(&CachePool->HotList);
    DestroyList(&CachePool->ColdList);
    // B+ Tree Destroy
    Destroy_Tree(CachePool->hot_bpt_root);
    Destroy_Tree(CachePool->cold_bpt_root);
    CachePool->hot_bpt_root = NULL;
    CachePool->cold_bpt_root = NULL;
    DestroyStoragePool(&CachePool->Storage);
}

BOOLEAN _IsFull(PCACHE_POOL CachePool)
{
    return (CachePool->ColdList.Size == CachePool->ColdSize);
}

/**
 * Query a Cache Block from Pool By its Index
 */
BOOLEAN _QueryPoolByIndex(PCACHE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock)
{
    // B+ Tree Find by Index
    *ppBlock = Find_Record(CachePool->hot_bpt_root, Index);
    if (NULL != *ppBlock)
        return TRUE;
    *ppBlock = Find_Record(CachePool->cold_bpt_root, Index);
    if (NULL != *ppBlock)
        return TRUE;
    return FALSE;
}

VOID _IncreaseBlockReference(PCACHE_POOL CachePool, PCACHE_BLOCK pBlock)
{
    pBlock->ReferenceCount++;
    // Move it to the head of list
    if (pBlock->Protected == TRUE)
    {
        ListMoveToHead(&CachePool->HotList, pBlock);
    }
    else
    {
        ListMoveToHead(&CachePool->ColdList, pBlock);
    }
}

/**
 * Add one Block to Cache Pool, When Pool is not Full
 * (Add to Cold List Head)
 */
PCACHE_BLOCK _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
{
    PCACHE_BLOCK pBlock;
    if ((pBlock = __GetFreeBlock(CachePool)) != NULL)
    {
        pBlock->Modified = Modified;
        pBlock->Index = Index;
        pBlock->Protected = FALSE;
        pBlock->Prior = NULL;
        pBlock->Next = NULL;
        pBlock->ReferenceCount = 0;
        StoragePoolWrite (
            &CachePool->Storage,
            pBlock->StorageIndex, 0,
            Data,
            BLOCK_SIZE
        );
        CachePool->Used++;
        // Insert to Cold List Head
        ListInsertToHead(&CachePool->ColdList, pBlock);
        // Insert into bpt
        CachePool->cold_bpt_root = Insert(CachePool->cold_bpt_root, Index, pBlock);
    }
    return pBlock;
}

/**
 * Delete one Block from Cache Pool and Free it
 */
VOID _DeleteOneBlockFromPool(PCACHE_POOL CachePool, LONGLONG Index)
{
    PCACHE_BLOCK pBlock;
    if (_QueryPoolByIndex(CachePool, Index, &pBlock) == TRUE)
    {
        StoragePoolFree(&CachePool->Storage, pBlock->StorageIndex);
        if (pBlock->Protected == TRUE)
        {
            ListDelete(&CachePool->HotList, pBlock);
            CachePool->hot_bpt_root = Delete(CachePool->hot_bpt_root, Index, TRUE);
        }
        else
        {
            ListDelete(&CachePool->ColdList, pBlock);
            CachePool->cold_bpt_root = Delete(CachePool->cold_bpt_root, Index, TRUE);
        }
        CachePool->Used--;
    }
}

/**
 * Find a Non-Modified Cache Block to Replace, When Pool(Cold List) is Full
 */
PCACHE_BLOCK _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
{
    ULONG i, Count;
    PCACHE_BLOCK pBlock, ret;

    // Backward find first refcnt < 2 in Cold List
    while ((pBlock = CachePool->ColdList.Tail) && pBlock->ReferenceCount >= 2)
    {
        pBlock->Protected = TRUE;
        pBlock->ReferenceCount = 0;
        ListDelete(&CachePool->ColdList, pBlock);
        CachePool->cold_bpt_root = Delete(CachePool->cold_bpt_root, pBlock->Index, FALSE);
        // Move to hot list head
        ListInsertToHead(&CachePool->HotList, pBlock);
        CachePool->hot_bpt_root = Insert(CachePool->hot_bpt_root, pBlock->Index, pBlock);
    }

    // If Hot List is Overfull, move extras to Cold List Head
    // The Cold List Will Never Overfull for ...
    Count = CachePool->HotList.Size > CachePool->HotSize ?
            CachePool->HotList.Size - CachePool->HotSize : 0;
    for (i = 0; i < Count; i++)
    {
        pBlock = ListRemoveTail(&CachePool->HotList);
        CachePool->hot_bpt_root = Delete(CachePool->hot_bpt_root, pBlock->Index, FALSE);
        pBlock->Protected = FALSE;
        // Move to cold list head
        ListInsertToHead(&CachePool->ColdList, pBlock);
        CachePool->cold_bpt_root = Insert(CachePool->cold_bpt_root, pBlock->Index, pBlock);
    }

    if (CachePool->ColdList.Size < CachePool->ColdSize)
    {
        pBlock = _AddNewBlockToPool(CachePool, Index, Data, Modified);
        ASSERT(pBlock);
    }
    else /* Cold List is Full */
    {
        pBlock = CachePool->ColdList.Tail;
    #ifdef WRITE_BACK_ENABLE
        if (pBlock->Modified == TRUE)
        {
            KIRQL   Irql;
            if (!SyncOneCacheBlock (CachePool, pBlock))
                DbgPrint("%s: SyncOneCacheBlock Error\n", __FUNCTION__);
            LOCK_WB_QUEUE(&CachePool->WbQueueLock);
            pBlock->Modified = FALSE;
            UNLOCK_WB_QUEUE(&CachePool->WbQueueLock);
        }
    #endif
        // Replace Block's Data
        {
            ListDelete(&CachePool->ColdList, pBlock);
            CachePool->cold_bpt_root = Delete(CachePool->cold_bpt_root, pBlock->Index, FALSE);
            // Update Data
            pBlock->Index = Index;
            pBlock->ReferenceCount = 1;
            pBlock->Modified = Modified;
            StoragePoolWrite (
                &CachePool->Storage,
                pBlock->StorageIndex, 0,
                Data,
                BLOCK_SIZE
            );
            ListInsertToHead(&CachePool->ColdList, pBlock);
            CachePool->cold_bpt_root = Insert(CachePool->cold_bpt_root, pBlock->Index, pBlock);
        }
    }

    return pBlock;
}

#endif
