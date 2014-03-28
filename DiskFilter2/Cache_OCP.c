#include "Cache.h"
#include "List.h"

#if defined(USE_OCP)

#define HOT_RATIO    2
BOOLEAN InitCachePool(PCACHE_POOL CachePool
					#ifndef USE_DRAM
						,ULONG DiskNum ,ULONG PartitionNum
					#endif
					)
{
	BOOLEAN ret;

	ZeroMemory(CachePool, sizeof(CACHE_POOL));
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(BLOCK_SIZE);
	InitList(&CachePool->HotList);
	InitList(&CachePool->ColdList);
	CachePool->HotSize = CachePool->Size / HOT_RATIO;
	CachePool->ColdSize = CachePool->Size - CachePool->HotSize;

	ret = InitStoragePool(&CachePool->Storage, CachePool->Size
		#ifndef USE_DRAM
			, DiskNum, PartitionNum
		#endif
		);
	if (ret == FALSE)
		goto l_error;
	return TRUE;
l_error:
	DestroyStoragePool(&CachePool->Storage);
	ZeroMemory(CachePool, sizeof(CACHE_POOL));
	return FALSE;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	DestroyList(&CachePool->HotList);
	DestroyList(&CachePool->ColdList);
	// B+ Tree Destroy
	Destroy_Tree(CachePool->hot_bpt_root);
	Destroy_Tree(CachePool->cold_bpt_root);
	DestroyStoragePool(&CachePool->Storage);
	ZeroMemory(CachePool, sizeof(CACHE_POOL));
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
BOOLEAN _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
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
 * Find a Non-Modified Cache Block to Replace, When Pool is Full
 */
PCACHE_BLOCK _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
{
	ULONG i, Count;
	PCACHE_BLOCK pBlock;
	// Backfoward find first refcnt < 2
	while ((pBlock = ListRemoveTail(&CachePool->ColdList)) && pBlock->ReferenceCount >= 2)
	{
		pBlock->Protected = TRUE;
		pBlock->ReferenceCount = 0;
		CachePool->cold_bpt_root = Delete(CachePool->cold_bpt_root, pBlock->Index, FALSE);
		// Move to hot list head
		ListInsertToHead(&CachePool->HotList, pBlock);
		CachePool->hot_bpt_root = Insert(CachePool->hot_bpt_root, pBlock->Index, pBlock);
	}
	// TODO: Find a Non-Modified to Replace
	if (pBlock != NULL)
	{
		// Replace data and Move it to Cold List Head
		CachePool->cold_bpt_root = Delete(CachePool->cold_bpt_root, pBlock->Index, FALSE);
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
	else
	{
		_AddNewBlockToPool(CachePool, Index, Data, Modified);
	}

	Count = CachePool->HotList.Size > CachePool->HotSize ?
			CachePool->HotList.Size - CachePool->HotSize : 0;
	// If Hot List is full, move extras to Cold List
	// The Cold List Will Never full for ...
	for (i = 0; i < Count; i++)
	{
		pBlock = ListRemoveTail(&CachePool->HotList);
		pBlock->Protected = FALSE;
		CachePool->hot_bpt_root = Delete(CachePool->hot_bpt_root, pBlock->Index, FALSE);
		// Move to hot list head
		ListInsertToHead(&CachePool->ColdList, pBlock);
		CachePool->cold_bpt_root = Insert(CachePool->cold_bpt_root, pBlock->Index, pBlock);
	}
	return pBlock;
}

#endif
