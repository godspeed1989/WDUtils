#include "Cache.h"
#include "Utils.h"

BOOLEAN InitCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(512);
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
		pBlock->Index = Index;
		RtlCopyMemory (
			pBlock->Data,
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
	BOOLEAN deleted = FALSE;
	CachePool->bpt_root = Delete(CachePool->bpt_root, Index, TRUE, &deleted);
	if (deleted == TRUE)
		CachePool->Used--;
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
		RtlCopyMemory (							\
			Buf,								\
			ppInternalBlocks[bi]->Data+off,		\
			len									\
		);										\
		ppInternalBlocks[bi]->Accessed = TRUE;	\
		Buf += len;

	if (front_broken == TRUE)
	{
		if (QueryPoolByIndex(CachePool, Offset-1, ppInternalBlocks+0) == FALSE)
			goto l_error;
		else
			_copy_data(0, BLOCK_SIZE-front_skip, front_skip>origLen?origLen:front_skip);
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
	}

	if (end_broken == TRUE)
	{
		if (QueryPoolByIndex(CachePool, Offset+Length, ppInternalBlocks+0) == FALSE)
			goto l_error;
		else
			_copy_data(0, 0, end_cut);
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
	BOOLEAN deleted;
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
	CachePool->bpt_root = Delete(CachePool->bpt_root, key, FALSE, &deleted);
	if (pBlock != NULL && deleted == TRUE)
	{
		pBlock->Accessed = FALSE;
		pBlock->Modified = FALSE;
		pBlock->Index = Index;
		RtlCopyMemory (
			pBlock->Data,
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
					NTSTATUS Status;
					ULONG matched1, matched2;
					LARGE_INTEGER readOffset;
					UCHAR Data[BLOCK_SIZE];
					readOffset.QuadPart = BLOCK_SIZE * pBlock->Index;
					Status = IoDoRWRequestSync (
						IRP_MJ_READ,
						LowerDeviceObject,
						Data,
						BLOCK_SIZE,
						&readOffset
					);
					if (NT_SUCCESS(Status))
					{
						matched1 = RtlCompareMemory(Data, pBlock->Data, BLOCK_SIZE);
						matched2 = RtlCompareMemory(Data, Buf+i*BLOCK_SIZE, BLOCK_SIZE);
					}
					else
					{
						matched1 = 9999999;
						matched2 = 9999999;
					}
					if (matched1 != BLOCK_SIZE || matched2 != BLOCK_SIZE)
						DbgPrint("XX:%d-%d:--(%d)<-(%d)->(%d)--\n",
							DiskNumber, PartitionNumber, matched1, BLOCK_SIZE, matched2);
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
				RtlCopyMemory (
					pBlock->Data,
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
