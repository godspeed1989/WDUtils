#include "Cache.h"
#include "Heap.h"

#if defined(USE_LFU) || defined(USE_LRU)

#ifdef USE_LRU
#include <Ntddk.h>
#define QueryTickCount(tc) KeQueryTickCount(tc)
#endif

BOOLEAN InitCachePool(PCACHE_POOL CachePool
					#ifndef USE_DRAM
						,ULONG DiskNum ,ULONG PartitionNum
					#endif
					)
{
	BOOLEAN ret;
	CachePool->Used = 0;
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(BLOCK_SIZE);
	CachePool->bpt_root = NULL;
	ret = InitStoragePool(&CachePool->Storage, CachePool->Size
		#ifndef USE_DRAM
			, DiskNum, PartitionNum
		#endif
		);
	if (ret == FALSE)
		return ret;
	ret = InitHeap(&CachePool->Heap, CachePool->Size);
	if (ret == FALSE)
		DestroyStoragePool(&CachePool->Storage);
	return ret;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = 0;
	// B+ Tree Destroy
	Destroy_Tree(CachePool->bpt_root);
	CachePool->bpt_root = NULL;
	DestroyStoragePool(&CachePool->Storage);
	DestroyHeap(&CachePool->Heap);
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
	// B+ Tree Find by Index
	*ppBlock = Find_Record(CachePool->bpt_root, Index);
	if (NULL == *ppBlock)
		return FALSE;
	else
		return TRUE;
}

VOID _IncreaseBlockReference(PCACHE_POOL CachePool, PCACHE_BLOCK pBlock)
{
#if defined(USE_LRU)
	LARGE_INTEGER TickCount;
	QueryTickCount(&TickCount);
	HeapUpdateValue(&CachePool->Heap, pBlock->HeapIndex, TickCount.QuadPart);
#elif defined(USE_LFU)
	HeapIncreaseValue(&CachePool->Heap, pBlock->HeapIndex, 1);
#endif
}

/**
 * Add one Block to Cache Pool, When Pool is not Full
 */
BOOLEAN _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;
#if defined(USE_LRU)
	LARGE_INTEGER TickCount;
	QueryTickCount(&TickCount);
	if((pBlock = __GetFreeBlock(CachePool)) != NULL &&
		TRUE == HeapInsert(&CachePool->Heap, pBlock, TickCount.QuadPart))
#elif defined(USE_LFU)
	if((pBlock = __GetFreeBlock(CachePool)) != NULL &&
		TRUE == HeapInsert(&CachePool->Heap, pBlock, 0))
#endif
	{
		pBlock->Modified = FALSE;
		pBlock->Index = Index;
		StoragePoolWrite (
			&CachePool->Storage,
			pBlock->StorageIndex, 0,
			Data,
			BLOCK_SIZE
		);
		CachePool->Used++;
		// Insert into bpt
		CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
		return TRUE;
	}
	if (pBlock != NULL)
	{
		StoragePoolFree(&CachePool->Storage, pBlock->StorageIndex);
		ExFreePoolWithTag(pBlock, CACHE_POOL_TAG);
	}
	return FALSE;
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
		HeapDelete(&CachePool->Heap, pBlock->HeapIndex);
		CachePool->bpt_root = Delete(CachePool->bpt_root, Index, TRUE);
		CachePool->Used--;
	}
}

/**
 * Find a Cache Block to Replace when Pool is Full
 */
VOID _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;
#if defined(USE_LRU)
	LARGE_INTEGER TickCount;
#endif
	pBlock = GetHeapTop(&CachePool->Heap);
	if (NULL == pBlock)
		return;
	CachePool->bpt_root = Delete(CachePool->bpt_root, pBlock->Index, FALSE);
	pBlock->Modified = FALSE;
	pBlock->Index = Index;
	StoragePoolWrite (
		&CachePool->Storage,
		pBlock->StorageIndex, 0,
		Data,
		BLOCK_SIZE
	);
#if defined(USE_LRU)
	QueryTickCount(&TickCount);
	HeapUpdateValue(&CachePool->Heap, pBlock->HeapIndex, TickCount.QuadPart);
#elif defined(USE_LFU)
	HeapZeroValue(&CachePool->Heap, pBlock->HeapIndex);
#endif
	CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
}

#endif
