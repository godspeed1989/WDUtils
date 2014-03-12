#include "Cache.h"

#if defined(USE_OCP)

#define HOT_RATIO    4
BOOLEAN InitCachePool(PCACHE_POOL CachePool
					#ifndef USE_DRAM
						,ULONG DiskNum ,ULONG PartitionNum
					#endif
					)
{
	BOOLEAN ret;

	CachePool->Used = 0;
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(BLOCK_SIZE);
	CachePool->HotUsed = 0;
	CachePool->HotSize = CachePool->Size / HOT_RATIO;
	CachePool->HotListHead = NULL;
	CachePool->HotListTail = NULL;
	CachePool->ColdUsed = 0;
	CachePool->ColdSize = CachePool->Size - CachePool->HotSize;
	CachePool->ColdListHead = NULL;
	CachePool->ColdListTail = NULL;
	CachePool->bpt_root = NULL;

	ret = InitStoragePool(&CachePool->Storage, CachePool->Size
		#ifndef USE_DRAM
			, DiskNum, PartitionNum
		#endif
		);
	if (ret == FALSE)
		return ret;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = 0;
	CachePool->HotUsed = 0;
	CachePool->HotSize = 0;
	CachePool->ColdUsed = 0;
	CachePool->ColdSize = 0;
	// B+ Tree Destroy
	Destroy_Tree(CachePool->bpt_root);
	CachePool->bpt_root = NULL;
	DestroyStoragePool(&CachePool->Storage);
}

BOOLEAN _IsFull(PCACHE_POOL CachePool)
{
	return (CachePool->ColdUsed == CachePool->ColdSize);
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
	pBlock->ReferenceCount++;
	// Move it to the head of list
	if (pBlock->Protected == TRUE)
	{
	}
	else
	{}
}

/**
 * Add one Block to Cache Pool, When Pool is not Full
 * (Add to Cold List Head)
 */
BOOLEAN _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;
	if (pBlock = __GetFreeBlock(CachePool)) != NULL)
	{
		pBlock->Modified = FALSE;
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
		CachePool->ColdUsed++;
		// Insert ot Cold List Head
		if (CachePool->ColdListHead == NULL)
			CachePool->ColdListHead = CachePool->ColdListTail = pBlock;
		else
		{
			CachePool->ColdListHead->Next = pBlock;
			pBlock->Next = CachePool->ColdListHead;
			CachePool->ColdListHead = pBlock;
		}
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
	// !!!!!!!!!!!!! We should never delete a block in OCP !!!!!!
	// Just insert to replace, and write back dirty
	PCACHE_BLOCK pBlock;
	ASSERT(0 == 1);
	if (_QueryPoolByIndex(CachePool, Index, &pBlock) == TRUE)
	{
		StoragePoolFree(&CachePool->Storage, pBlock->StorageIndex);
		CachePool->bpt_root = Delete(CachePool->bpt_root, Index, TRUE);
		CachePool->Used--;
		if (pBlock->Protected == TRUE)
			;//Oops
		else
			;//Oops
	}
}

/**
 * Find a Cache Block to Replace
 */
VOID _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	// !!!!!!!!!!!!! We should never replace a block in OCP !!!!!!
}

#endif
