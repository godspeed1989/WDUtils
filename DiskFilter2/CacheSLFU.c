#include "Cache.h"
#include "Heap.h"

#ifdef USE_SLFU

#define PROTECT_RATIO    4
BOOLEAN InitCachePool(PCACHE_POOL CachePool
					#ifndef USE_DRAM
						,ULONG DiskNum ,ULONG PartitionNum
					#endif
					)
{
	BOOLEAN ret;

	CachePool->Used = 0;
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(BLOCK_SIZE);
	CachePool->ProtectedUsed = 0;
	CachePool->ProtectedSize = CachePool->Size / PROTECT_RATIO;
	CachePool->Protected_bpt_root = NULL;
	CachePool->ProbationaryUsed = 0;
	CachePool->ProbationarySize = CachePool->Size - CachePool->ProtectedSize;
	CachePool->Probationary_bpt_root = NULL;

	ret = InitStoragePool(&CachePool->Storage, CachePool->Size
		#ifndef USE_DRAM
			, DiskNum, PartitionNum
		#endif
		);
	if (ret == FALSE)
		return ret;
	ret = InitHeap(&CachePool->ProbationaryHeap, CachePool->ProbationarySize);
	if (ret == FALSE)
		goto l_error;
	ret = InitHeap(&CachePool->ProtectedHeap, CachePool->ProtectedSize);
	if (ret == FALSE)
		goto l_error;
	return ret;
l_error:
	DestroyHeap(&CachePool->ProbationaryHeap);
	DestroyHeap(&CachePool->ProtectedHeap);
	DestroyStoragePool(&CachePool->Storage);
	return ret;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = 0;
	CachePool->ProtectedUsed = 0;
	CachePool->ProtectedSize = 0;
	CachePool->ProbationaryUsed = 0;
	CachePool->ProbationarySize = 0;
	// B+ Tree Destroy
	Destroy_Tree(CachePool->Protected_bpt_root);
	CachePool->Protected_bpt_root = NULL;
	Destroy_Tree(CachePool->Probationary_bpt_root);
	CachePool->Probationary_bpt_root = NULL;
	DestroyStoragePool(&CachePool->Storage);
	DestroyHeap(&CachePool->ProbationaryHeap);
	DestroyHeap(&CachePool->ProtectedHeap);
}

BOOLEAN _IsFull(PCACHE_POOL CachePool)
{
	return (CachePool->ProbationaryUsed == CachePool->ProbationarySize);
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
		HeapIncreaseValue(&CachePool->ProtectedHeap, pBlock->HeapIndex, 1);
	}
	else
	{
		HeapDelete(&CachePool->ProbationaryHeap, pBlock->HeapIndex);
		CachePool->Probationary_bpt_root = Delete(CachePool->Probationary_bpt_root, pBlock->Index, FALSE);
		// Protected is Full
		if (CachePool->ProtectedUsed == CachePool->ProtectedSize)
		{
			// Remove one from Protected
			_pBlock = GetHeapTop(&CachePool->ProtectedHeap);
			HeapDelete(&CachePool->ProtectedHeap, _pBlock->HeapIndex);
			CachePool->Protected_bpt_root = Delete(CachePool->Protected_bpt_root, _pBlock->Index, FALSE);
			// Move to Probationary
			// Probationary Obviously Not Full for We just Remove one from it
			ASSERT(TRUE == HeapInsert(&CachePool->ProbationaryHeap, _pBlock, 1));
			CachePool->Probationary_bpt_root = Insert(CachePool->Probationary_bpt_root, _pBlock->Index, _pBlock);
		}
		else
		{
			CachePool->ProtectedUsed++;
			CachePool->ProbationaryUsed--;
		}
		pBlock->Protected = TRUE;
		// Add to Protected
		ASSERT(TRUE == HeapInsert(&CachePool->ProtectedHeap, pBlock, 1));
		CachePool->Protected_bpt_root = Insert(CachePool->Protected_bpt_root, pBlock->Index, pBlock);
	}
}

/**
 * Add one Block to Cache Pool, When Pool is not Full
 * (Add to Probationary Segment)
 */
BOOLEAN _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;
	if((pBlock = __GetFreeBlock(CachePool)) != NULL &&
		TRUE == HeapInsert(&CachePool->ProbationaryHeap, pBlock, 0))
	{
		pBlock->Modified = FALSE;
		pBlock->Index = Index;
		pBlock->Protected = FALSE;
		StoragePoolWrite (
			&CachePool->Storage,
			pBlock->StorageIndex, 0,
			Data,
			BLOCK_SIZE
		);
		CachePool->Used++;
		CachePool->ProbationaryUsed++;
		// Insert into bpt
		CachePool->Probationary_bpt_root = Insert(CachePool->Probationary_bpt_root, Index, pBlock);
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
		if (pBlock->Protected == TRUE)
		{
			HeapDelete(&CachePool->ProtectedHeap, pBlock->HeapIndex);
			CachePool->Protected_bpt_root = Delete(CachePool->Protected_bpt_root, Index, TRUE);
			CachePool->ProtectedUsed--;
		}
		else
		{
			HeapDelete(&CachePool->ProbationaryHeap, pBlock->HeapIndex);
			CachePool->Probationary_bpt_root = Delete(CachePool->Probationary_bpt_root, Index, TRUE);
			CachePool->ProbationaryUsed--;
		}
		CachePool->Used--;
	}
}

/**
 * Find a Cache Block to Replace for a Non-existed Block
 * (Find From Probationary Segment)
 */
VOID _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;

	pBlock = GetHeapTop(&CachePool->ProbationaryHeap);
	if (NULL == pBlock)
		return;
	CachePool->Probationary_bpt_root = Delete(CachePool->Probationary_bpt_root, pBlock->Index, FALSE);
	pBlock->Modified = FALSE;
	pBlock->Index = Index;
	pBlock->Protected = FALSE;
	StoragePoolWrite (
		&CachePool->Storage,
		pBlock->StorageIndex, 0,
		Data,
		BLOCK_SIZE
	);
	HeapZeroValue(&CachePool->ProbationaryHeap, pBlock->HeapIndex);
	CachePool->Probationary_bpt_root = Insert(CachePool->Probationary_bpt_root, Index, pBlock);
}

#endif
