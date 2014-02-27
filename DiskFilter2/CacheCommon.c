#include "Cache.h"
#include "Utils.h"

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
PCACHE_BLOCK GetFreeBlock(PCACHE_POOL CachePool)
{
	PCACHE_BLOCK pBlock;
	if (IsFull(CachePool) == FALSE)
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
 * Update Cache Pool with Buffer
 */
VOID UpdataCachePool(
	PCACHE_POOL CachePool,
	PUCHAR Buf, LONGLONG Offset, ULONG Length,
	BOOLEAN Type
#ifdef READ_VERIFY
	,PDEVICE_OBJECT LowerDeviceObject
	,ULONG DiskNumber
	,ULONG PartitionNumber
#endif
)
{
	ULONG i, front_skip, end_cut;
	PCACHE_BLOCK pBlock;
	BOOLEAN front_broken, end_broken;

	detect_broken(Offset, Length, front_broken, end_broken, front_skip, end_cut);
	Offset /= BLOCK_SIZE;
	Length /= BLOCK_SIZE;

	if (Type == _READ_)
	{
		if (front_broken == TRUE)
			Buf += front_skip;
		for (i = 0; i < Length; i++)
		{
			// Still have empty cache block
			if (IsFull(CachePool) == FALSE)
			{
				// Not to duplicate
				if(QueryPoolByIndex(CachePool, Offset+i, &pBlock) == FALSE)
					AddNewBlockToPool(CachePool, Offset+i, Buf+i*BLOCK_SIZE);
				else
				{
				#ifdef READ_VERIFY
					DO_READ_VERIFY(&CachePool->Storage, pBlock, LowerDeviceObject);
				#endif
					IncreaseBlockReference(CachePool, pBlock);
				}
			}
			else
				break;
		}
		// Pool is Full
		while (i < Length)
		{
			// Not to duplicate
			if(QueryPoolByIndex(CachePool, Offset+i, &pBlock) == FALSE)
				FindBlockToReplace(CachePool, Offset+i, Buf+i*BLOCK_SIZE);
			else
			{
			#ifdef READ_VERIFY
				DO_READ_VERIFY(&CachePool->Storage, pBlock, LowerDeviceObject);
			#endif
				IncreaseBlockReference(CachePool, pBlock);
			}
			i++;
		}
	}
	else /* Write */
	{
		if(front_broken == TRUE)
			DeleteOneBlockFromPool(CachePool, Offset-1);
		for (i = 0; i < Length; i++)
		{
		#if 0
			DeleteOneBlockFromPool(CachePool, Offset+i);
		#else
			if(QueryPoolByIndex(CachePool, Offset+i, &pBlock) == TRUE)
			{
				// Update
				StoragePoolWrite (
					&CachePool->Storage,
					pBlock->StorageIndex, 0,
					Buf + i * BLOCK_SIZE,
					BLOCK_SIZE
				);
				pBlock->Modified = TRUE;
				IncreaseBlockReference(CachePool, pBlock);
				continue;
			}
			if (IsFull(CachePool) == FALSE)
			{
				AddNewBlockToPool(CachePool, Offset+i, Buf+i*BLOCK_SIZE);
				continue;
			}
			else
			{
				FindBlockToReplace(CachePool, Offset+i, Buf+i*BLOCK_SIZE);
				continue;
			}
		#endif
		}
		if (end_broken == TRUE)
			DeleteOneBlockFromPool(CachePool, Offset+Length);
	}
}

/**
 * Query Cache Pool if the _READ_ Request is Matched
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
	ULONG i, front_skip, end_cut, origLen;
	BOOLEAN Ret = FALSE;
	BOOLEAN front_broken, end_broken;
	PCACHE_BLOCK *ppInternalBlocks = NULL;

	origBuf = Buf;
	origLen = Length;

	detect_broken(Offset, Length, front_broken, end_broken, front_skip, end_cut);
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
		IncreaseBlockReference(CachePool, ppInternalBlocks[bi]);

	if (front_broken == TRUE)
	{
		if (QueryPoolByIndex(CachePool, Offset-1, ppInternalBlocks+0) == FALSE)
			goto l_error;
		else
		{
			_copy_data(0, BLOCK_SIZE-front_skip, (front_skip>origLen)?origLen:front_skip);
		#ifdef READ_VERIFY
			DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[0], LowerDeviceObject);
		#endif
		}
	}

	// Query Cache Pool If it is Fully Matched
	for (i = 0; i < Length; i++)
	{
		if (QueryPoolByIndex(CachePool, Offset+i, ppInternalBlocks+i) == FALSE)
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
		if (QueryPoolByIndex(CachePool, Offset+Length, ppInternalBlocks+0) == FALSE)
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
