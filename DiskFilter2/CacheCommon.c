#include "Cache.h"
#include "Utils.h"
#include "Queue.h"

/**
 * Common Cache Functions
 */

/**
 * Used In B+ Tree Delete Node
 */
void Free_Record( record * r )
{
	PCACHE_BLOCK p = (PCACHE_BLOCK)r;
	ExFreePoolWithTag(p, CACHE_POOL_TAG);
}

 /**
 * Get a Free Block From Cache Pool
 */
PCACHE_BLOCK __GetFreeBlock(PCACHE_POOL CachePool)
{
	PCACHE_BLOCK pBlock = NULL;
	if (_IsFull(CachePool) == FALSE)
	{
		pBlock = (PCACHE_BLOCK) ExAllocatePoolWithTag (
						NonPagedPool,
						(SIZE_T)sizeof(CACHE_BLOCK),
						CACHE_POOL_TAG
					);
		if (pBlock == NULL) goto l_error;
		pBlock->StorageIndex = StoragePoolAlloc(&CachePool->Storage);
		if (pBlock->StorageIndex == -1) goto l_error;
		return pBlock;
	}
l_error:
	if (pBlock)
		ExFreePoolWithTag(pBlock, CACHE_POOL_TAG);
	return NULL;
}

/**
 * Read Update Cache Pool with Buffer
 */
VOID ReadUpdateCachePool(
	PCACHE_POOL CachePool,
	PUCHAR Buf, LONGLONG Offset, ULONG Length
#ifdef READ_VERIFY
	,PDEVICE_OBJECT LowerDeviceObject
	,ULONG DiskNumber
	,ULONG PartitionNumber
#endif
)
{
	PUCHAR origBuf;
	ULONG i, front_offset, front_skip, end_cut, origLen;
	PCACHE_BLOCK pBlock;
	BOOLEAN front_broken, end_broken;

	origBuf = Buf;
	origLen = Length;

	detect_broken(Offset, Length, front_broken, end_broken, front_offset, front_skip, end_cut);
	Offset /= BLOCK_SIZE;
	Length /= BLOCK_SIZE;

	if(front_broken == TRUE)
	{
		Buf += front_skip;
	}
	for (i = 0; i < Length; i++)
	{
		LONGLONG Index = Offset + i;
		if(_QueryPoolByIndex(CachePool, Index, &pBlock) == TRUE)
		{
			DO_READ_VERIFY(CachePool, &CachePool->Storage, pBlock);
			_IncreaseBlockReference(CachePool, pBlock);
		}
		else if (_IsFull(CachePool) == FALSE)
		{
			pBlock = _AddNewBlockToPool(CachePool, Index, Buf, FALSE);
		}
		else
		{
			pBlock = _FindBlockToReplace(CachePool, Index, Buf, FALSE);
		}
		Buf += BLOCK_SIZE;
	}
	if (end_broken == TRUE)
	{
		Buf += end_cut;
	}
	ASSERT(Buf - origBuf == origLen);
}

#define _write_data(pBlock,off,Buf,len)					\
		StoragePoolWrite (								\
			&CachePool->Storage,						\
			pBlock->StorageIndex,						\
			off,										\
			Buf,										\
			len											\
		);												\
		_IncreaseBlockReference(CachePool, pBlock);		\
		if (pBlock->Modified == FALSE)					\
			ADD_TO_WBQUEUE_NOTSAFE(pBlock);
/**
 * Write Update Cache Pool with Buffer
 */
VOID WriteUpdateCachePool(
	PCACHE_POOL CachePool,
	PUCHAR Buf, LONGLONG Offset, ULONG Length
#ifdef READ_VERIFY
	,PDEVICE_OBJECT LowerDeviceObject
	,ULONG DiskNumber
	,ULONG PartitionNumber
#endif
)
{
	PUCHAR origBuf;
	ULONG i, front_offset, front_skip, end_cut, origLen;
	PCACHE_BLOCK pBlock;
	BOOLEAN front_broken, end_broken;

	origBuf = Buf;
	origLen = Length;

	detect_broken(Offset, Length, front_broken, end_broken, front_offset, front_skip, end_cut);
	Offset /= BLOCK_SIZE;
	Length /= BLOCK_SIZE;

	if (front_broken)
	{
		if (_QueryPoolByIndex(CachePool, Offset-1, &pBlock) == TRUE)
		{
			DO_READ_VERIFY(CachePool, &CachePool->Storage, pBlock);
			_write_data(pBlock, front_offset, Buf, front_skip);
		}
		Buf += front_skip;
	}
	for (i = 0; i < Length; i++)
	{
		LONGLONG Index = Offset + i;
		if(_QueryPoolByIndex(CachePool, Index, &pBlock) == TRUE)
		{
			DO_READ_VERIFY(CachePool, &CachePool->Storage, pBlock);
			_write_data(pBlock, 0, Buf, BLOCK_SIZE);
		}
		else if (_IsFull(CachePool) == FALSE)
		{
			pBlock = _AddNewBlockToPool(CachePool, Index, Buf, TRUE);
			ADD_TO_WBQUEUE_NOTSAFE(pBlock);
		}
		else
		{
			pBlock = _FindBlockToReplace(CachePool, Index, Buf, TRUE);
			ADD_TO_WBQUEUE_NOTSAFE(pBlock);
		}
		Buf += BLOCK_SIZE;
	}
	if (end_broken)
	{
		if (_QueryPoolByIndex(CachePool, Offset+Length, &pBlock) == TRUE)
		{
			DO_READ_VERIFY(CachePool, &CachePool->Storage, pBlock);
			_write_data(pBlock, 0, Buf, end_cut);
		}
		Buf += end_cut;
	}
	ASSERT (Buf - origBuf == origLen);
}

/**
 * Query Cache Pool (If the _READ_ Request is Matched)
 * If it's fully matched, copy to the request buffer and return TRUE,
 * else return FALSE
 */
BOOLEAN QueryAndCopyFromCachePool (
	PCACHE_POOL CachePool, PUCHAR Buf, LONGLONG Offset, ULONG Length
#ifdef READ_VERIFY
	,PDEVICE_OBJECT LowerDeviceObject
	,ULONG DiskNumber
	,ULONG PartitionNumber
#endif
)
{
	PUCHAR origBuf;
	ULONG i, front_offset, front_skip, end_cut, origLen;
	BOOLEAN Ret = FALSE;
	BOOLEAN front_broken, end_broken;
	PCACHE_BLOCK *ppInternalBlocks = NULL;

	origBuf = Buf;
	origLen = Length;

	detect_broken(Offset, Length, front_broken, end_broken, front_offset, front_skip, end_cut);
	Offset /= BLOCK_SIZE;
	Length /= BLOCK_SIZE;

	ppInternalBlocks = (PCACHE_BLOCK*) ExAllocatePoolWithTag (
		NonPagedPool,
		(SIZE_T)((Length+2) * sizeof(PCACHE_BLOCK)),
		CACHE_POOL_TAG
	);
	if (ppInternalBlocks == NULL)
		goto l_error;

	// Query Pool to Check If Fully Matched
	if (front_broken == TRUE && _QueryPoolByIndex(CachePool, Offset-1, ppInternalBlocks+0) == FALSE)
		goto l_error;
	for (i = 0; i < Length; i++)
		if (_QueryPoolByIndex(CachePool, Offset+i, ppInternalBlocks+i+1) == FALSE)
			goto l_error;
	if (end_broken == TRUE && _QueryPoolByIndex(CachePool, Offset+Length, ppInternalBlocks+Length+1) == FALSE)
		goto l_error;

#define _copy_data(pBlock,off,len) 						\
		StoragePoolRead (								\
			&CachePool->Storage,						\
			Buf,										\
			pBlock->StorageIndex,						\
			off,										\
			len											\
		);												\
		Buf += len;										\
		_IncreaseBlockReference(CachePool, pBlock);

	if (front_broken == TRUE)
	{
		_copy_data(ppInternalBlocks[0], front_offset, front_skip);
		DO_READ_VERIFY(CachePool, &CachePool->Storage, ppInternalBlocks[0]);
	}

	for (i = 0; i < Length; i++)
	{
		_copy_data(ppInternalBlocks[i+1], 0, BLOCK_SIZE);
		DO_READ_VERIFY(CachePool, &CachePool->Storage, ppInternalBlocks[i+1]);
	}

	if (end_broken == TRUE)
	{
		_copy_data(ppInternalBlocks[Length+1], 0, end_cut);
		DO_READ_VERIFY(CachePool, &CachePool->Storage, ppInternalBlocks[Length+1]);
	}

	ASSERT(Buf - origBuf == origLen);
	Ret = TRUE;
l_error:
	if (ppInternalBlocks != NULL)
		ExFreePoolWithTag(ppInternalBlocks, CACHE_POOL_TAG);
	return Ret;
}

/**
 * Query Cache Pool (If the _WRITE_ Request is Matched)
 * If it's fully matched, update cache and return TRUE,
 * else return FALSE
 */
BOOLEAN QueryAndWriteToCachePool (
	PCACHE_POOL CachePool, PUCHAR Buf, LONGLONG Offset, ULONG Length
#ifdef READ_VERIFY
	,PDEVICE_OBJECT LowerDeviceObject
	,ULONG DiskNumber
	,ULONG PartitionNumber
#endif
)
{
	PUCHAR origBuf;
	ULONG i, front_offset, front_skip, end_cut, origLen;
	BOOLEAN Ret = FALSE;
	BOOLEAN front_broken, end_broken;
	PCACHE_BLOCK *ppInternalBlocks = NULL;

	origBuf = Buf;
	origLen = Length;

	detect_broken(Offset, Length, front_broken, end_broken, front_offset, front_skip, end_cut);
	Offset /= BLOCK_SIZE;
	Length /= BLOCK_SIZE;

	ppInternalBlocks = (PCACHE_BLOCK*) ExAllocatePoolWithTag (
		NonPagedPool,
		(SIZE_T)((Length+2) * sizeof(PCACHE_BLOCK)),
		CACHE_POOL_TAG
	);
	if (ppInternalBlocks == NULL)
		goto l_error;

	// Query Pool to Check If Fully Matched
	if (front_broken == TRUE && _QueryPoolByIndex(CachePool, Offset-1, ppInternalBlocks+0) == FALSE)
		goto l_error;
	for (i = 0; i < Length; i++)
		if (_QueryPoolByIndex(CachePool, Offset+i, ppInternalBlocks+i+1) == FALSE)
			goto l_error;
	if (end_broken == TRUE && _QueryPoolByIndex(CachePool, Offset+Length, ppInternalBlocks+Length+1) == FALSE)
		goto l_error;

	if (front_broken == TRUE)
	{
		DO_READ_VERIFY(CachePool, &CachePool->Storage, ppInternalBlocks[0]);
		_write_data(ppInternalBlocks[0], front_offset, Buf, front_skip);
		Buf += front_skip;
	}

	for (i = 0; i < Length; i++)
	{
		DO_READ_VERIFY(CachePool, &CachePool->Storage, ppInternalBlocks[i+1]);
		_write_data(ppInternalBlocks[i+1], 0, Buf, BLOCK_SIZE);
		Buf += BLOCK_SIZE;
	}

	if (end_broken == TRUE)
	{
		DO_READ_VERIFY(CachePool, &CachePool->Storage, ppInternalBlocks[Length+1]);
		_write_data(ppInternalBlocks[Length+1], 0, Buf, end_cut);
		Buf += end_cut;
	}

	ASSERT(Buf - origBuf == origLen);
	Ret = TRUE;
l_error:
	if (ppInternalBlocks != NULL)
		ExFreePoolWithTag(ppInternalBlocks, CACHE_POOL_TAG);
	return Ret;
}
