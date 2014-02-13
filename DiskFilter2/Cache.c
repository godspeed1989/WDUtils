#include "Cache.h"
#include "Utils.h"

BOOLEAN InitCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(BLOCK_SIZE);
	CachePool->bpt_root = NULL;
	return InitStoragePool(&CachePool->Storage, CachePool->Size);
}

void Free_Record( record * r )
{
	PCACHE_BLOCK p = (PCACHE_BLOCK)r;
	ExFreePoolWithTag(p, CACHE_POOL_TAG);
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = 0;
	// B+ Tree Destroy
	Destroy_Tree(CachePool->bpt_root);
	CachePool->bpt_root = NULL;
	DestroyStoragePool(&CachePool->Storage);
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
 * Add one Block to Cache Pool
 */
static BOOLEAN AddOneBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;
	if ((pBlock = GetFreeBlock(CachePool)) != NULL)
	{
		pBlock->Accessed = FALSE;
		pBlock->Modified = FALSE;
		pBlock->Index = Index;
		StoragePoolWrite (
			&CachePool->Storage,
			pBlock->StorageIndex, 0,
			Data,
			BLOCK_SIZE
		);
		CachePool->Used++;
		// Insert into bpt
		CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
		return TRUE;
	}
	return FALSE;
}

/**
 * Delete one Block from Cache Pool and Free it
 */
static VOID DeleteOneBlockFromPool(PCACHE_POOL CachePool, LONGLONG Index)
{
	PCACHE_BLOCK pBlock;

	if (QueryPoolByIndex(CachePool, Index, &pBlock) == TRUE)
	{
		StoragePoolFree(&CachePool->Storage, pBlock->StorageIndex);
		CachePool->bpt_root = Delete(CachePool->bpt_root, Index, TRUE);		
		CachePool->Used--;
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
		ppInternalBlocks[bi]->Accessed = TRUE;	\
		Buf += len;

	if (front_broken == TRUE)
	{
		if (QueryPoolByIndex(CachePool, Offset-1, ppInternalBlocks+0) == FALSE)
			goto l_error;
		else
		{
			_copy_data(0, BLOCK_SIZE-front_skip, (front_skip>origLen)?origLen:front_skip);
		#ifdef READ_VERIFY
			DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[0]);
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
		DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[i]);
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
			DO_READ_VERIFY(&CachePool->Storage, ppInternalBlocks[0]);
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
	return;
found:
	CachePool->bpt_root = Delete(CachePool->bpt_root, key, FALSE);
	if (pBlock != NULL)
	{
		pBlock->Accessed = FALSE;
		pBlock->Modified = FALSE;
		pBlock->Index = Index;
		StoragePoolWrite (
			&CachePool->Storage,
			pBlock->StorageIndex, 0,
			Data,
			BLOCK_SIZE
		);
		CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
	}
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
					AddOneBlockToPool(CachePool, Offset+i, Buf+i*BLOCK_SIZE);
				else
				{
				#ifdef READ_VERIFY
					DO_READ_VERIFY(&CachePool->Storage, pBlock);
				#endif
					pBlock->Accessed = TRUE;
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
				DO_READ_VERIFY(&CachePool->Storage, pBlock);
			#endif
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
		#if 1
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
				continue;
			}
			if (IsFull(CachePool) == FALSE)
			{
				AddOneBlockToPool(CachePool, Offset+i, Buf+i*BLOCK_SIZE);
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
