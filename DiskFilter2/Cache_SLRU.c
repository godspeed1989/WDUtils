#include "Cache.h"
#include "List.h"
#include "Queue.h"

#if defined(USE_SLRU)

#define PROTECT_RATIO    2
BOOLEAN InitCachePool(PCACHE_POOL CachePool
					#ifndef USE_DRAM
						,ULONG DiskNum ,ULONG PartitionNum
					#endif
					)
{
	BOOLEAN ret;

	ZeroMemory(CachePool, sizeof(CACHE_POOL));
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(BLOCK_SIZE);
	CachePool->ProtectedSize = CachePool->Size / PROTECT_RATIO;
	CachePool->ProbationarySize = CachePool->Size - CachePool->ProtectedSize;

	ret = InitStoragePool(&CachePool->Storage, CachePool->Size
		#ifndef USE_DRAM
			, DiskNum, PartitionNum
		#endif
		);
	if (ret == FALSE)
		goto l_error;
	InitList(&CachePool->ProbationaryList);
	InitList(&CachePool->ProtectedList);
	return TRUE;
l_error:
	DestroyList(&CachePool->ProtectedList);
	DestroyList(&CachePool->ProbationaryList);
	DestroyStoragePool(&CachePool->Storage);
	ZeroMemory(CachePool, sizeof(CACHE_POOL));
	return FALSE;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	// B+ Tree Destroy
	DestroyList(&CachePool->ProtectedList);
	DestroyList(&CachePool->ProbationaryList);
	Destroy_Tree(CachePool->Protected_bpt_root);
	Destroy_Tree(CachePool->Probationary_bpt_root);
	DestroyStoragePool(&CachePool->Storage);
	ZeroMemory(CachePool, sizeof(CACHE_POOL));
}

BOOLEAN _IsFull(PCACHE_POOL CachePool)
{
	return (CachePool->ProbationaryList.Size == CachePool->ProbationarySize);
}

/**
 * Query a Cache Block from Pool By its Index
 */
BOOLEAN _QueryPoolByIndex(PCACHE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock)
{
	// B+ Tree Find by Index
	*ppBlock = Find_Record(CachePool->Probationary_bpt_root, Index);
	if (NULL != *ppBlock)
		return TRUE;
	*ppBlock = Find_Record(CachePool->Protected_bpt_root, Index);
	if (NULL != *ppBlock)
		return TRUE;
	return FALSE;
}

VOID _IncreaseBlockReference(PCACHE_POOL CachePool, PCACHE_BLOCK pBlock)
{
	PCACHE_BLOCK _pBlock, Top;

	if (pBlock->Protected == TRUE)
	{
		ListMoveToHead(&CachePool->ProtectedList, pBlock);
	}
	else
	{
		ListDelete(&CachePool->ProbationaryList, pBlock);
		CachePool->Probationary_bpt_root = Delete(CachePool->Probationary_bpt_root, pBlock->Index, FALSE);
		// Protected is Full
		if (CachePool->ProtectedList.Size == CachePool->ProtectedSize)
		{
			// Remove one from Protected
			_pBlock = ListRemoveTail(&CachePool->ProtectedList);
			CachePool->Protected_bpt_root = Delete(CachePool->Protected_bpt_root, _pBlock->Index, FALSE);
			// Move to Probationary
			// Probationary Obviously Not Full for We just Remove one from it
			_pBlock->Protected = FALSE;
			ListInsertToHead(&CachePool->ProbationaryList, _pBlock);
			CachePool->Probationary_bpt_root = Insert(CachePool->Probationary_bpt_root, _pBlock->Index, _pBlock);
		}
		// Add to Protected
		pBlock->Protected = TRUE;
		ListInsertToHead(&CachePool->ProtectedList, pBlock);
		CachePool->Protected_bpt_root = Insert(CachePool->Protected_bpt_root, pBlock->Index, pBlock);
	}
}

/**
 * Add one Block to Cache Pool, When Pool is not Full
 * (Add to Probationary Segment)
 */
PCACHE_BLOCK _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
{
	PCACHE_BLOCK pBlock;
	if ((pBlock = __GetFreeBlock(CachePool)) != NULL)
	{
		pBlock->Modified = Modified;
		pBlock->Index = Index;
		pBlock->Protected = FALSE;
		StoragePoolWrite (
			&CachePool->Storage,
			pBlock->StorageIndex, 0,
			Data,
			BLOCK_SIZE
		);
		CachePool->Used++;
		ListInsertToHead(&CachePool->ProbationaryList, pBlock);
		// Insert into bpt
		CachePool->Probationary_bpt_root = Insert(CachePool->Probationary_bpt_root, Index, pBlock);
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
			ListDelete(&CachePool->ProtectedList, pBlock);
			CachePool->Protected_bpt_root = Delete(CachePool->Protected_bpt_root, Index, TRUE);
		}
		else
		{
			ListDelete(&CachePool->ProbationaryList, pBlock);
			CachePool->Probationary_bpt_root = Delete(CachePool->Probationary_bpt_root, Index, TRUE);
		}
		CachePool->Used--;
	}
}

/**
 * Find a Non-Modified Cache Block to Replace when Pool is Full
 * (Find From Probationary Segment)
 */
PCACHE_BLOCK _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified)
{
	PCACHE_BLOCK pBlock;

	pBlock = CachePool->ProbationaryList.Tail;
#ifdef WRITE_BACK_ENABLE
	// Backfoward find first Non-Modified in Probationary List
	while (pBlock && pBlock->Modified == TRUE)
		pBlock = pBlock->Prior;
#endif

	if (pBlock)
	{
		CachePool->Probationary_bpt_root = Delete(CachePool->Probationary_bpt_root, pBlock->Index, FALSE);
		pBlock->Modified = Modified;
		pBlock->Index = Index;
		pBlock->Protected = FALSE;
		StoragePoolWrite (
			&CachePool->Storage,
			pBlock->StorageIndex, 0,
			Data,
			BLOCK_SIZE
		);
		ListMoveToHead(&CachePool->ProbationaryList, pBlock);
		CachePool->Probationary_bpt_root = Insert(CachePool->Probationary_bpt_root, Index, pBlock);
	}
#ifdef WRITE_BACK_ENABLE
	else
	{
		// There always exist Non-Modified Blocks, When Probationary is Full
		ASSERT(CachePool->ProbationaryList.Size < CachePool->ProbationarySize);
		// Cold List not full
		pBlock = _AddNewBlockToPool(CachePool, Index, Data, Modified);
		ASSERT(pBlock);
	}
#endif

	return pBlock;
}

#endif
