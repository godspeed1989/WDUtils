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

	ZeroMemory(CachePool, sizeof(CACHE_POOL));
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(BLOCK_SIZE);

	ret = InitStoragePool(&CachePool->Storage, CachePool->Size
		#ifndef USE_DRAM
			, DiskNum, PartitionNum
		#endif
		);
	if (ret == FALSE)
		goto l_error;
	InitList(&CachePool->List);
	return TRUE;
l_error:
	DestroyStoragePool(&CachePool->Storage);
	ZeroMemory(CachePool, sizeof(CACHE_POOL));
	return FALSE;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	// B+ Tree Destroy
	Destroy_Tree(CachePool->bpt_root);
	DestroyStoragePool(&CachePool->Storage);
	ZeroMemory(CachePool, sizeof(CACHE_POOL));
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
	ListMoveToHead(&CachePool->List, pBlock);
}

/**
 * Add one Block to Cache Pool, When Pool is not Full
 */
BOOLEAN _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
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
		// Insert into bpt
		CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
		return TRUE;
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
	while (pBlock && pBlock->Modified == TRUE)
		pBlock = pBlock->Prior;
	// There always exist Non-Modified Blocks, when Pool is Full
	ASSERT(NULL == pBlock);

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
	return pBlock;
}

#endif
