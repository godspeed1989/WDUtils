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
VOID ReadUpdataCachePool(
	PCACHE_POOL CachePool,
	PUCHAR Buf, LONGLONG Offset, ULONG Length
#ifdef READ_VERIFY
	,PDEVICE_OBJECT LowerDeviceObject
	,ULONG DiskNumber
	,ULONG PartitionNumber
#endif
)
{
	ULONG i, front_offset, front_skip, end_cut;
	PCACHE_BLOCK pBlock;
	BOOLEAN front_broken, end_broken;

	detect_broken(Offset, Length, front_broken, end_broken, front_offset, front_skip, end_cut);
	Offset /= BLOCK_SIZE;
	Length /= BLOCK_SIZE;

	if (front_broken == TRUE)
		Buf += front_skip;
	for (i = 0; i < Length; i++)
	{
		// Still have empty cache block
		if (_IsFull(CachePool) == FALSE)
		{
			// Not to duplicate
			if(_QueryPoolByIndex(CachePool, Offset+i, &pBlock) == FALSE)
				_AddNewBlockToPool(CachePool, Offset+i, Buf+i*BLOCK_SIZE, FALSE);
			else
			{
			#ifdef READ_VERIFY
				DO_READ_VERIFY(&CachePool->Storage, pBlock, LowerDeviceObject);
			#endif
				_IncreaseBlockReference(CachePool, pBlock);
				CachePool->ReadHit++;
			}
		}
		else
			break;
	}
	// Pool is Full
	while (i < Length)
	{
		// Not to duplicate
		if(_QueryPoolByIndex(CachePool, Offset+i, &pBlock) == FALSE)
			_FindBlockToReplace(CachePool, Offset+i, Buf+i*BLOCK_SIZE, FALSE);
		else
		{
		#ifdef READ_VERIFY
			DO_READ_VERIFY(&CachePool->Storage, pBlock, LowerDeviceObject);
		#endif
			_IncreaseBlockReference(CachePool, pBlock);
			CachePool->ReadHit++;
		}
		i++;
	}
}

/**
 * Write Update Cache Pool with Buffer
 */
VOID WriteUpdataCachePool(
	PCACHE_POOL CachePool,
	PUCHAR Buf, LONGLONG Offset, ULONG Length
#ifdef READ_VERIFY
	,PDEVICE_OBJECT LowerDeviceObject
	,ULONG DiskNumber
	,ULONG PartitionNumber
#endif
)
{
	ULONG i, front_offset, front_skip, end_cut;
	PCACHE_BLOCK pBlock;
	BOOLEAN front_broken, end_broken;

	detect_broken(Offset, Length, front_broken, end_broken, front_offset, front_skip, end_cut);
	Offset /= BLOCK_SIZE;
	Length /= BLOCK_SIZE;

	if(front_broken == TRUE)
	{
		Buf += front_skip;
		_DeleteOneBlockFromPool(CachePool, Offset-1);
	}
	for (i = 0; i < Length; i++)
	{
		if(_QueryPoolByIndex(CachePool, Offset+i, &pBlock) == TRUE)
		{
			// Update
			StoragePoolWrite (
				&CachePool->Storage,
				pBlock->StorageIndex, 0,
				Buf + i * BLOCK_SIZE,
				BLOCK_SIZE
			);
			// Add to write back queue
			if (pBlock->Modified == FALSE)
			{
				ADD_TO_WBQUEUE(pBlock);
			}
			_IncreaseBlockReference(CachePool, pBlock);
			CachePool->WriteHit++;
			continue;
		}
		if (_IsFull(CachePool) == FALSE)
		{
			_AddNewBlockToPool(CachePool, Offset+i, Buf+i*BLOCK_SIZE, TRUE);
			ADD_TO_WBQUEUE(pBlock);
			continue;
		}
		else
		{
			// TODO: pBlock = 
			_FindBlockToReplace(CachePool, Offset+i, Buf+i*BLOCK_SIZE, TRUE);
			ADD_TO_WBQUEUE(pBlock);
			continue;
		}
	}
	if (end_broken == TRUE)
		_DeleteOneBlockFromPool(CachePool, Offset+Length);
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
		(SIZE_T)((Length+1) * sizeof(PCACHE_BLOCK)),
		CACHE_POOL_TAG
	);
	if (ppInternalBlocks == NULL)
		goto l_error;

#define _copy_data(bi,off,len) 					\
		StoragePoolRead (						\
			&CachePool->Storage,				\
			Buf,								\
			ppInternalBlocks[bi]->StorageIndex,	\
			off,								\
			len									\
		);										\
		Buf += len;								\
		_IncreaseBlockReference(CachePool, ppInternalBlocks[bi]);

	if (front_broken == TRUE)
	{
		if (_QueryPoolByIndex(CachePool, Offset-1, ppInternalBlocks+0) == FALSE)
			goto l_error;
		else
		{
			_copy_data(0, front_offset, front_skip);
		#ifdef READ_VERIFY
			DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[0], LowerDeviceObject);
		#endif
		}
	}

	// Query Cache Pool If it is Fully Matched
	for (i = 0; i < Length; i++)
	{
		if (_QueryPoolByIndex(CachePool, Offset+i, ppInternalBlocks+i) == FALSE)
			goto l_error;
	}
	// Copy From Cache Pool
	for (i = 0; i < Length; i++)
	{
		_copy_data(i, 0, BLOCK_SIZE);
	#ifdef READ_VERIFY
		DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[i], LowerDeviceObject);
	#endif
	}

	if (end_broken == TRUE)
	{
		if (_QueryPoolByIndex(CachePool, Offset+Length, ppInternalBlocks+0) == FALSE)
			goto l_error;
		else
		{
			_copy_data(0, 0, end_cut);
		#ifdef READ_VERIFY
			DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[0], LowerDeviceObject);
		#endif
		}
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
 * If it's fully matched, copy to the request buffer and return TRUE,
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
		(SIZE_T)((Length+1) * sizeof(PCACHE_BLOCK)),
		CACHE_POOL_TAG
	);
	if (ppInternalBlocks == NULL)
		goto l_error;

#define _write_data(bi,off,len) 									\
		StoragePoolWrite (											\
			&CachePool->Storage,									\
			ppInternalBlocks[bi]->StorageIndex,						\
			off,													\
			Buf,													\
			len														\
		);															\
		Buf += len;													\
		_IncreaseBlockReference(CachePool, ppInternalBlocks[bi]);	\
		if (ppInternalBlocks[bi]->Modified == FALSE)				\
			ADD_TO_WBQUEUE(ppInternalBlocks[bi]);

	if (front_broken == TRUE)
	{
		if (_QueryPoolByIndex(CachePool, Offset-1, ppInternalBlocks+0) == FALSE)
			goto l_error;
		else
		{
		#ifdef READ_VERIFY
			DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[0], LowerDeviceObject);
		#endif
			_write_data(0, front_offset, front_skip);
		}
	}

	// Query Cache Pool If it is Fully Matched
	for (i = 0; i < Length; i++)
	{
		if (_QueryPoolByIndex(CachePool, Offset+i, ppInternalBlocks+i) == FALSE)
			goto l_error;
	}
	// Copy From Cache Pool
	for (i = 0; i < Length; i++)
	{
	#ifdef READ_VERIFY
		DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[i], LowerDeviceObject);
	#endif
		_write_data(i, 0, BLOCK_SIZE);
	}

	if (end_broken == TRUE)
	{
		if (_QueryPoolByIndex(CachePool, Offset+Length, ppInternalBlocks+0) == FALSE)
			goto l_error;
		else
		{
		#ifdef READ_VERIFY
			DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[0], LowerDeviceObject);
		#endif
			_write_data(0, 0, end_cut);
		}
	}

	ASSERT(Buf - origBuf == origLen);
	Ret = TRUE;
l_error:
	if (ppInternalBlocks != NULL)
		ExFreePoolWithTag(ppInternalBlocks, CACHE_POOL_TAG);
	return Ret;
}
