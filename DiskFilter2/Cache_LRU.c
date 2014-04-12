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
			, DiskNum, PartitionNum
		#endif
		);
	if (ret == FALSE)
		goto l_error;
	rb_tree_create(&CachePool->rb_tree, Free_Record);
	InitList(&CachePool->List);
	return TRUE;
l_error:
	DestroyStoragePool(&CachePool->Storage);
	return FALSE;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	DestroyList(&CachePool->List);
	rb_tree_destroy(&CachePool->rb_tree);
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
	rb_node_t * node = rb_find(&CachePool->rb_tree, Index);
	if (NULL == node)
		return FALSE;
	else
	{
		*ppBlock = (PCACHE_BLOCK)node->client;
		return TRUE;
	}
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
		rb_insert(&CachePool->rb_tree, Index, pBlock);
	}
	return pBlock;
}

/**
 * Delete one Block from Cache Pool and Free it
 */
VOID _DeleteOneBlockFromPool(PCACHE_POOL CachePool, LONGLONG Index)
{
	PCACHE_BLOCK pBlock;
	rb_node_t * node = rb_find(&CachePool->rb_tree, Index);
	if (node)
	{
		pBlock = node->client;
		ListDelete(&CachePool->List, pBlock);
		StoragePoolFree(&CachePool->Storage, pBlock->StorageIndex);
		rb_delete(&CachePool->rb_tree, node, TRUE);
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
	// Backfoward find first Non-Modified
	while (pBlock && pBlock->Modified == TRUE)
		pBlock = pBlock->Prior;
#endif

	if (pBlock)
	{
		rb_node_t * node = rb_find(&CachePool->rb_tree, pBlock->Index);
		rb_delete(&CachePool->rb_tree, node, FALSE);
		pBlock->Modified = Modified;
		pBlock->Index = Index;
		StoragePoolWrite (
			&CachePool->Storage,
			pBlock->StorageIndex, 0,
			Data,
			BLOCK_SIZE
		);
		ListMoveToHead(&CachePool->List, pBlock);
		rb_insert(&CachePool->rb_tree, Index, pBlock);
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
