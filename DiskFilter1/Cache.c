#include "Cache.h"

BOOLEAN InitCachePool(PCAHCE_POOL CachePool)
{
	ULONG i;
	CachePool->Used = 0;
	CachePool->Size = CACHE_POOL_SIZE;
	CachePool->Blocks = (PCACHE_BLOCK) ExAllocatePoolWithTag (
		NonPagedPool,
		(SIZE_T)(CACHE_POOL_SIZE * sizeof(CACHE_BLOCK)),
		CACHE_POOL_TAG
	);
	if (CachePool->Blocks == NULL)
		return FALSE;
	for (i = 0; i < CACHE_POOL_SIZE; i++)
	{
		CachePool->Blocks[i].Index.QuadPart = (LONGLONG)-1;
	}
	return TRUE;
}

VOID DestroyCachePool(PCAHCE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = 0;
	ExFreePoolWithTag(CachePool->Blocks, CACHE_POOL_TAG);
}

/**
 * Query a Cache Block from Pool By its Index
 */
static BOOLEAN QueryPoolByIndex(PCAHCE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock)
{
	ULONG i;
	for (i = 0; i < CachePool->Size; i++)
	{
		if (CachePool->Blocks[i].Index.QuadPart == Index)
		{
			*ppBlock = CachePool->Blocks + i;
			return TRUE;
		}
	}
	return FALSE;
}

static BOOLEAN IsEmpty(PCAHCE_POOL CachePool)
{
	return (0 == CachePool->Used);
}

static BOOLEAN IsFull(PCAHCE_POOL CachePool)
{
	return (CachePool->Size == CachePool->Used);
}

/**
 * Get a Free Block From Cache Pool
 */
static PCACHE_BLOCK GetFreeBlock(PCAHCE_POOL CachePool)
{
	ULONG i;
	if (IsFull(CachePool) == TRUE)
		return NULL;
	for (i = 0; i < CachePool->Size; i++)
	{
		if (CachePool->Blocks[i].Index.QuadPart == (LONGLONG)-1)
		{
			return CachePool->Blocks + i;
		}
	}
	return NULL;
}

/**
 * Add one Block to Cache Pool
 */
static BOOLEAN AddOneBlockToPool(PCAHCE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;
	if ((pBlock = GetFreeBlock(CachePool)) != NULL)
	{
		pBlock->Index.QuadPart = Index;
		RtlCopyMemory (
			pBlock->Data,
			Data,
			SECTOR_SIZE
		);
		CachePool->Used++;
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
	PCAHCE_POOL CachePool, PUCHAR Buf, LARGE_INTEGER Offset, ULONG Length
)
{
	ULONG i;
	BOOLEAN Ret = FALSE;
	PCACHE_BLOCK *ppInternalBlocks = NULL;

	if (Offset.QuadPart % SECTOR_SIZE || Length % SECTOR_SIZE)
	{
		return FALSE;
	}
	Offset.QuadPart /= SECTOR_SIZE;
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
		if (QueryPoolByIndex(CachePool, Offset.QuadPart+i, ppInternalBlocks+i) == FALSE)
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
	}
	Ret = TRUE;
l_error:
	if (ppInternalBlocks != NULL)
		ExFreePoolWithTag(ppInternalBlocks, CACHE_POOL_TAG);
	return Ret;
}

/**
 * Update Cache Pool with Buffer
 */
VOID UpdataCachePool(
	PCAHCE_POOL CachePool, PUCHAR Buf, LARGE_INTEGER Offset, ULONG Length,
	BOOLEAN Type
)
{
	ULONG i, j, ii;
	PCACHE_BLOCK pBlock;
	if (Offset.QuadPart % SECTOR_SIZE || Length % SECTOR_SIZE)
	{
		return;
	}
	Offset.QuadPart /= SECTOR_SIZE;
	Length /= SECTOR_SIZE;

	// READ
	if (Type == _READ_)
	{
		for (i = 0; i < Length; i++)
		{
			if (IsFull(CachePool) == FALSE)
			{
				AddOneBlockToPool(CachePool, Offset.QuadPart + i, Buf + i*SECTOR_SIZE);
				continue;
			}
			else
			{
				// TODO: Do nothing if the Pool is Full
				// Need a replace algorithm for [Offset.QuadPart + i]
				return;
			}
		}
	}
	// WRITE
	else
	{
		for (i = 0; i < Length; i++)
		{
			// Update Cache Pool for the Block with same Index
			if (IsEmpty(CachePool) == FALSE &&
				QueryPoolByIndex(CachePool, Offset.QuadPart + i, &pBlock) == TRUE)
			{
				RtlCopyMemory (
					pBlock->Data,
					Buf + i * SECTOR_SIZE,
					SECTOR_SIZE
				);
				continue;
			}
			else if (IsFull(CachePool) == FALSE)
			{
				AddOneBlockToPool(CachePool, Offset.QuadPart + i, Buf + i * SECTOR_SIZE);
				continue;
			}
		}
	}
}
