#include "Cache.h"

#if defined(USE_OCP)

#define HOT_RATIO    4
BOOLEAN InitCachePool(PCACHE_POOL CachePool
					#ifndef USE_DRAM
						,ULONG DiskNum ,ULONG PartitionNum
					#endif
					)
{
	BOOLEAN ret;

	CachePool->Used = 0;
	CachePool->Size = (CACHE_POOL_SIZE << 20)/(BLOCK_SIZE);
	CachePool->HotUsed = 0;
	CachePool->HotSize = CachePool->Size / HOT_RATIO;
	CachePool->HotListHead = NULL;
	CachePool->HotListTail = NULL;
	CachePool->ColdUsed = 0;
	CachePool->ColdSize = CachePool->Size - CachePool->HotSize;
	CachePool->ColdListHead = NULL;
	CachePool->ColdListTail = NULL;
	CachePool->bpt_root = NULL;

	ret = InitStoragePool(&CachePool->Storage, CachePool->Size
		#ifndef USE_DRAM
			, DiskNum, PartitionNum
		#endif
		);
	return ret;
}

VOID DestroyCachePool(PCACHE_POOL CachePool)
{
	CachePool->Used = 0;
	CachePool->Size = 0;
	CachePool->HotUsed = 0;
	CachePool->HotSize = 0;
	CachePool->ColdUsed = 0;
	CachePool->ColdSize = 0;
	// B+ Tree Destroy
	Destroy_Tree(CachePool->bpt_root);
	CachePool->bpt_root = NULL;
	DestroyStoragePool(&CachePool->Storage);
}

BOOLEAN _IsFull(PCACHE_POOL CachePool)
{
	return (CachePool->ColdUsed == CachePool->ColdSize);
}

/**
 * Query a Cache Block from Pool By its Index
 */
BOOLEAN _QueryPoolByIndex(PCACHE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock)
{
	// B+ Tree Find by Index
	*ppBlock = Find_Record(CachePool->bpt_root, Index);
	if (NULL == *ppBlock)
		return FALSE;
	else
		return TRUE;
}

#define APPEND_TO_HEAD(Head, Tail, OldHead)	\
		{									\
			Head->Prior = NULL;				\
			Tail->Next = OldHead; 			\
			if (OldHead != NULL)			\
				OldHead->Prior = Tail;		\
			OldHead = Head;					\
		}

VOID _IncreaseBlockReference(PCACHE_POOL CachePool, PCACHE_BLOCK pBlock)
{
	pBlock->ReferenceCount++;
	// Move it to the head of list
	if (pBlock->Protected == TRUE)
	{
		if (pBlock == CachePool->HotListHead)
			return;
		pBlock->Prior->Next = pBlock->Next;
		if (pBlock == CachePool->HotListTail)
			CachePool->HotListTail = pBlock->Prior;
		else
			pBlock->Next->Prior = pBlock->Prior;
		APPEND_TO_HEAD(pBlock, pBlock, CachePool->HotListHead);
	}
	else
	{
		if (pBlock == CachePool->ColdListHead)
			return;
		pBlock->Prior->Next = pBlock->Next;
		if (pBlock == CachePool->ColdListTail)
			CachePool->ColdListTail = pBlock->Prior;
		else
			pBlock->Next->Prior = pBlock->Prior;
		APPEND_TO_HEAD(pBlock, pBlock, CachePool->ColdListHead);
	}
}

/**
 * Add one Block to Cache Pool, When Pool is not Full
 * (Add to Cold List Head)
 */
BOOLEAN _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	PCACHE_BLOCK pBlock;
	if ((pBlock = __GetFreeBlock(CachePool)) != NULL)
	{
		pBlock->Modified = FALSE;
		pBlock->Index = Index;
		pBlock->Protected = FALSE;
		pBlock->Prior = NULL;
		pBlock->Next = NULL;
		pBlock->ReferenceCount = 0;
		StoragePoolWrite (
			&CachePool->Storage,
			pBlock->StorageIndex, 0,
			Data,
			BLOCK_SIZE
		);
		CachePool->Used++;
		CachePool->ColdUsed++;
		// Insert to Cold List Head
		if (CachePool->ColdListHead == NULL)
			CachePool->ColdListHead = CachePool->ColdListTail = pBlock;
		else
			APPEND_TO_HEAD(pBlock, pBlock, CachePool->ColdListHead);
		// Insert into bpt
		CachePool->bpt_root = Insert(CachePool->bpt_root, Index, pBlock);
		return TRUE;
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
		if (pBlock->Protected == TRUE)
		{
			CachePool->HotUsed--;
			if (pBlock == CachePool->HotListHead)
			{
				CachePool->HotListHead = pBlock->Next;
				if (CachePool->HotListHead != NULL)
					CachePool->HotListHead->Prior = NULL;
			}
			else
				pBlock->Prior->Next = pBlock->Next;
			if (pBlock == CachePool->HotListTail)
			{
				CachePool->HotListTail = pBlock->Prior;
				if (CachePool->HotListTail != NULL)
					CachePool->HotListTail->Next = NULL;
			}
			else
				pBlock->Next->Prior = pBlock->Prior;
		}
		else
		{
			CachePool->ColdUsed--;
			if (pBlock == CachePool->ColdListHead)
			{
				CachePool->ColdListHead = pBlock->Next;
				if (CachePool->ColdListHead != NULL)
					CachePool->ColdListHead->Prior = NULL;
			}
			else
				pBlock->Prior->Next = pBlock->Next;
			if (pBlock == CachePool->ColdListTail)
			{
				CachePool->ColdListTail = pBlock->Prior;
				if (CachePool->ColdListTail != NULL)
					CachePool->ColdListTail->Next = NULL;
			}
			else
				pBlock->Next->Prior = pBlock->Prior;
		}
		StoragePoolFree(&CachePool->Storage, pBlock->StorageIndex);
		CachePool->bpt_root = Delete(CachePool->bpt_root, Index, TRUE);
		CachePool->Used--;
	}
}

/**
 * Find a Cache Block to Replace, When Pool is Full
 */
VOID _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data)
{
	ULONG i, Count;
	PCACHE_BLOCK Start, Tail;
	// Backfoward find first refcnt < 2
	Count = 0;
	Start = CachePool->ColdListTail;
	while (Start->ReferenceCount >= 2 && Start != CachePool->ColdListHead)
	{
		Count++;
		Start->ReferenceCount = 0;
		Start->Protected = TRUE;
		Start = Start->Prior;
	}

	// Move to Hot List Head
	if (Count > 0)
		APPEND_TO_HEAD(Start->Next, CachePool->ColdListTail, CachePool->HotListHead);
	// Replace Start's data and Move it to Cold List Head
	Start->Index = Index;
	Start->ReferenceCount = 1;
	StoragePoolWrite (
		&CachePool->Storage,
		Start->StorageIndex, 0,
		Data,
		BLOCK_SIZE
	);
	if (Start != CachePool->ColdListHead)
	{
		CachePool->ColdListTail = Start->Prior;
		APPEND_TO_HEAD(Start, Start, CachePool->ColdListHead);
	}
	else
		CachePool->ColdListTail = Start;
	CachePool->ColdListTail->Next = NULL;

	CachePool->HotUsed += Count;
	CachePool->ColdUsed -= Count;

	Count = CachePool->HotUsed > CachePool->HotSize ?
			CachePool->HotUsed - CachePool->HotSize : 0;
	// If Hot List is full, move extras to Cold List
	// The Cold List Will Never full for ...
	if (Count > 0)
	{
		Start = CachePool->HotListTail;
		for (i = 0; i < Count; i++)
		{
			Start->Protected = FALSE;
			Start = Start->Prior;
		}
		APPEND_TO_HEAD(Start->Next, CachePool->HotListTail, CachePool->ColdListHead);
		// Reassign Hot List Tail
		CachePool->HotListTail = Start;
		CachePool->HotListTail->Next = NULL;
		CachePool->HotUsed -= Count;
		CachePool->ColdUsed += Count;
	}
}

#endif
