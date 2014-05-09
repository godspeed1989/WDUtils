#include "Cache.h"
#include "List.h"
#include "Queue.h"

#if defined(USE_LRU)

BOOLEAN InitCachePool(PCACHE_POOL CachePool
                    #ifndef USE_DRAM
                        ,ULONG DiskNum ,ULONG PartitionNum
                    #endif
                    )
{
    BOOLEAN ret;

    CachePool->Size = CachePool->Used = 0;
    CachePool->ReadHit = CachePool->WriteHit = 0;
    CachePool->Size = CACHE_POOL_NUM_BLOCKS;

    ret = InitStoragePool(&CachePool->Storage, CachePool->Size
        #ifndef USE_DRAM
            ,DiskNum ,PartitionNum
        #endif
        );
    if (ret == FALSE)
        goto l_error;
    CachePool->bpt_root = NULL;
    InitList(&CachePool->List);
    return TRUE;
l_error:
    DestroyStoragePool(&CachePool->Storage);
    return FALSE;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
    DestroyList(&CachePool->List);
    Destroy_Tree(CachePool->bpt_root);
    DestroyStoragePool(&CachePool->Storage);
}

BOOLEAN _IsFull(PCACHE_POOL CachePool)
{
    return (CachePool->Size == CachePool->Used);
}

/**
 * Query a Cache Block from Pool By its Index
 */
BOOLEAN _QueryPoolByIndex(PCACHE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock)
{
    *ppBlock = Find_Record(CachePool->bpt_root, Index);
    if (NULL == *ppBlock)
        return FALSE;
    else
        return TRUE;
}

VOID _IncreaseBlockReference(PCACHE_POOL CachePool, PCACHE_BLOCK pBlock)
{
    ListMoveToHead(&CachePool->List, pBlock);
}

/**
 * Add one Block to Cache Pool, When Pool is not Full
 */
PCACHE_BLOCK _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
{
    PCACHE_BLOCK pBlock;
    if ((pBlock = __GetFreeBlock(CachePool)) != NULL)
    {
        pBlock->Modified = Modified;
        pBlock->Index = Index;
        StoragePoolWrite (
            &CachePool->Storage,
            pBlock->StorageIndex, 0,
            Data,
            BLOCK_SIZE
        );
        CachePool->Used++;
        ListInsertToHead(&CachePool->List, pBlock);
        CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
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
        ListDelete(&CachePool->List, pBlock);
        StoragePoolFree(&CachePool->Storage, pBlock->StorageIndex);
        CachePool->bpt_root = Delete(CachePool->bpt_root, Index, TRUE);
        CachePool->Used--;
    }
}

/**
 * Find a Non-Modified Cache Block to Replace, when Pool is Full
 */
PCACHE_BLOCK _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
{
    PCACHE_BLOCK pBlock;

    pBlock = CachePool->List.Tail;
#ifdef WRITE_BACK_ENABLE
    // Backward find first Non-Modified
    while (pBlock && pBlock->Modified == TRUE)
        pBlock = pBlock->Prior;
#endif

    if (pBlock)
    {
        CachePool->bpt_root = Delete(CachePool->bpt_root, pBlock->Index, FALSE);
        pBlock->Modified = Modified;
        pBlock->Index = Index;
        StoragePoolWrite (
            &CachePool->Storage,
            pBlock->StorageIndex, 0,
            Data,
            BLOCK_SIZE
        );
        ListMoveToHead(&CachePool->List, pBlock);
        CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
    }
#ifdef WRITE_BACK_ENABLE
    else
    {
        // There always exist Non-Modified Blocks, When cold list is Full
        ASSERT(CachePool->List.Size < CachePool->Size);
        // Pool not full
        pBlock = _AddNewBlockToPool(CachePool, Index, Data, Modified);
        ASSERT(pBlock);
    }
#endif

    return pBlock;
}

#endif
