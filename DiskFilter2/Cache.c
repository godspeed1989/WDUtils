#include "Cache.h"

BOOLEAN InitCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = CACHE_POOL_SIZE;
	CachePool->bpt_root = NULL;
	return TRUE;
}

void Free_Record( record * r )
{
	ExFreePoolWithTag(r, CACHE_POOL_TAG);
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = 0;
	// B+ Tree Destroy
	Destroy_Tree(CachePool->bpt_root);
	CachePool->bpt_root = NULL;
}

/**
 * Query a Cache Block from Pool By its Index
 */
static BOOLEAN QueryPoolByIndex(PCACHE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock)
{
	// B+ Tree Find by Index
	*ppBlock = Find_Record(CachePool->bpt_root, Index);
	if(NULL == *ppBlock)
		return FALSE;
	else
		return TRUE;
}

static BOOLEAN IsEmpty(PCACHE_POOL CachePool)
{
	return (0 == CachePool->Used);
}

static BOOLEAN IsFull(PCACHE_POOL CachePool)
{
	return (CachePool->Size == CachePool->Used);
}

/**
 * Get a Free Block From Cache Pool
 */
static PCACHE_BLOCK GetFreeBlock(PCACHE_POOL CachePool)
{
	PCACHE_BLOCK pBlock;
	if (IsFull(CachePool) == FALSE)
	{
		pBlock = (PCACHE_BLOCK) ExAllocatePoolWithTag (
						NonPagedPool,
						(SIZE_T)sizeof(CACHE_BLOCK),
						CACHE_POOL_TAG
					);
		return pBlock;
	}
	return NULL;
}

/**
 * Add one Block to Cache Pool
 */
static BOOLEAN AddOneBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;
	if ((pBlock = GetFreeBlock(CachePool)) != NULL)
	{
		pBlock->Accessed = FALSE;
		pBlock->Modified = FALSE;
		RtlCopyMemory (
			pBlock->Data,
			Data,
			SECTOR_SIZE
		);
		CachePool->Used++;
		// Insert into bpt
		CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
		return TRUE;
	}
	return FALSE;
}

/**
 * Query Cache Pool if the _READ_ Request is Matched
 * If it's fully matched, copy to the request buffer and return TRUE,
 * else return FALSE
 */
BOOLEAN QueryAndCopyFromCachePool (
	PCACHE_POOL CachePool, PUCHAR Buf, LONGLONG Offset, ULONG Length
)
{
	ULONG i;
	BOOLEAN Ret = FALSE;
	PCACHE_BLOCK *ppInternalBlocks = NULL;

	ASSERT(Offset % SECTOR_SIZE == 0);
	ASSERT(Length % SECTOR_SIZE == 0);

	Offset /= SECTOR_SIZE;
	Length /= SECTOR_SIZE;

	ppInternalBlocks = (PCACHE_BLOCK*) ExAllocatePoolWithTag (
		NonPagedPool,
		(SIZE_T)(Length * sizeof(PCACHE_BLOCK)),
		CACHE_POOL_TAG
	);
	if (ppInternalBlocks == NULL)
		goto l_error;
	// Query Cache Pool If is Fully Matched
	for (i = 0; i < Length; i++)
	{
		if (QueryPoolByIndex(CachePool, Offset+i, ppInternalBlocks+i) == FALSE)
			goto l_error;
	}
	// Copy From Cache Pool
	for (i = 0; i < Length; i++)
	{
		RtlCopyMemory (
			Buf + i * SECTOR_SIZE,
			ppInternalBlocks[i]->Data,
			SECTOR_SIZE
		);
		ppInternalBlocks[i]->Accessed = TRUE;
	}
	Ret = TRUE;
l_error:
	if (ppInternalBlocks != NULL)
		ExFreePoolWithTag(ppInternalBlocks, CACHE_POOL_TAG);
	return Ret;
}

/**
 * Replace a Cache Block
 */
static VOID FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	KEY_T i, key;
	node *leaf;
	PCACHE_BLOCK pBlock = NULL;
	leaf = Get_Leftmost_Leaf(CachePool->bpt_root);
	while(leaf)
	{
		for(i = 0; i < leaf->num_keys; i++)
		{
			pBlock = leaf->pointers[i];
			if (pBlock->Accessed == FALSE &&
				pBlock->Modified == FALSE)
			{
				key = leaf->keys[i];
				goto found;
			}
		}
		leaf = leaf->pointers[order - 1];
	}
	//DbgPrint("No Block to replace\n");
	return;
found:
	if (pBlock != NULL)
	{
		CachePool->bpt_root = Delete(CachePool->bpt_root, key, FALSE);
		pBlock->Accessed = FALSE;
		pBlock->Modified = FALSE;
		RtlCopyMemory (
			pBlock->Data,
			Data,
			SECTOR_SIZE
		);
		CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
	}
}

/**
 * Update Cache Pool with Buffer
 */
VOID UpdataCachePool(
	PCACHE_POOL CachePool, PUCHAR Buf, LONGLONG Offset, ULONG Length,
	BOOLEAN Type
)
{
	ULONG i;
	PCACHE_BLOCK pBlock;
	BOOLEAN broken;

	ASSERT(Offset % SECTOR_SIZE == 0);
	broken = FALSE;
	if (Length % SECTOR_SIZE != 0)
		broken = TRUE;

	Offset /= SECTOR_SIZE;
	Length /= SECTOR_SIZE;

	if (Type == _READ_)
	{
		for (i = 0; i < Length; i++)
		{
			// Still have empty cache block
			if (IsFull(CachePool) == FALSE)
			{
				// Not to duplicate
				if(QueryPoolByIndex(CachePool, Offset+i, &pBlock) == FALSE)
					AddOneBlockToPool(CachePool, Offset+i, Buf+i*SECTOR_SIZE);
				else
					pBlock->Accessed = TRUE;
			}
			else
				break;
		}
		// Pool is Full
		while (i < Length)
		{
			FindBlockToReplace(CachePool, Offset+i, Buf+i*SECTOR_SIZE);
			i++;
		}
	}
	else
	{
		for (i = 0; i < Length; i++)
		{
		#if 1
			CachePool->bpt_root = Delete(CachePool->bpt_root, Offset+i, TRUE);
		#else
			if(QueryPoolByIndex(CachePool, Offset+i, &pBlock) == TRUE)
			{
				// Update
				RtlCopyMemory (
					pBlock->Data,
					Buf + i * SECTOR_SIZE,
					SECTOR_SIZE
				);
				pBlock->Modified = TRUE;
				continue;
			}
			if (IsFull(CachePool) == FALSE)
			{
				AddOneBlockToPool(CachePool, Offset+i, Buf+i*SECTOR_SIZE);
				continue;
			}
			else
			{
				FindBlockToReplace(CachePool, Offset+i, Buf+i*SECTOR_SIZE);
				continue;
			}
		#endif
		}
		if (broken == TRUE)
			CachePool->bpt_root = Delete(CachePool->bpt_root, Offset+i, TRUE);
	}
}
