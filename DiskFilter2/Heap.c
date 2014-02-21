#include "Heap.h"

#define Parent(i)	((i-1)/2)
#define Left(i)		(2*i+1)
#define Right(i)	(2*i+2)

#define _HMALLOC(n)			ExAllocatePoolWithTag (	NonPagedPool,	\
													(SIZE_T)(n),	\
													HEAP_POOL_TAG )
#define _HFREE(p)			ExFreePoolWithTag (p, HEAP_POOL_TAG)
#define _HZEROMEM(p,len)	RtlZeroMemory(p,(SIZE_T)(len))

BOOLEAN
InitHeap(PHeap Heap, ULONG Size)
{
	Heap->Used = 0;
	Heap->Size = Size;
	Heap->Entries = (PHeapEntry*)_HMALLOC(Size*sizeof(PHeapEntry));
	if (Heap->Entries == NULL)
		return FALSE;
	_HZEROMEM(Heap->Entries, Size*sizeof(PHeapEntry));
	return TRUE;
}

VOID
DestroyHeap(PHeap Heap)
{
	ULONG i;
	for (i=0; i<Heap->Used; i++)
		_HFREE(Heap->Entries[i]);
	Heap->Used = 0;
	Heap->Size = 0;
	if (Heap->Entries != NULL)
	{
		_HFREE(Heap->Entries);
		Heap->Entries = NULL;
	}
}

static VOID
HeapSiftUp(PHeap Heap, ULONG HeapIndex)
{
	BOOLEAN done = FALSE;
	PHeapEntry* h = Heap->Entries;
	while (HeapIndex != 0 && done == FALSE)
	{
		ULONG ParentIndex = Parent(HeapIndex);
		if (HEAP_ENTRY_COMPARE(h, ParentIndex, HeapIndex))
		{
			HEAP_ENTRY_SWAP(h, ParentIndex, HeapIndex);
			HeapIndex = ParentIndex;
		}
		else
			done = TRUE;
	}
}

static VOID
HeapSiftDown(PHeap Heap, ULONG HeapIndex)
{
	BOOLEAN done = FALSE;
	PHeapEntry* h = Heap->Entries;
	while (Left(HeapIndex) < Heap->Used && done == FALSE)
	{
		ULONG Index = Left(HeapIndex);
		if (Right(HeapIndex) < Heap->Used &&
			HEAP_ENTRY_COMPARE(h, Left(HeapIndex), Right(HeapIndex)))
			Index = Right(HeapIndex);
		if (HEAP_ENTRY_COMPARE(h, HeapIndex, Index))
		{
			HEAP_ENTRY_SWAP(h, HeapIndex, Index);
			HeapIndex = Index;
		}
		else
			done = TRUE;
	}
}

BOOLEAN
HeapInsert(PHeap Heap, HEAP_DAT_T *pData)
{
	PHeapEntry entry = NULL;
	if (Heap->Used == Heap->Size)
		return FALSE;
	entry = (PHeapEntry)_HMALLOC(sizeof(HeapEntry));
	if (entry == NULL)
		return FALSE;

	entry->Value = 0;
	entry->pData = pData;
	pData->HeapIndex = Heap->Used;
	Heap->Used++;
	Heap->Entries[pData->HeapIndex] = entry;

	HeapSiftUp(Heap, pData->HeapIndex);
	return TRUE;
}

VOID
HeapDelete(PHeap Heap, ULONG HeapIndex)
{
	PHeapEntry* h = Heap->Entries;
	if (Heap->Used == 0)
		return;
	Heap->Used--;
	HEAP_ENTRY_SWAP(h, HeapIndex, Heap->Used);

	if (HEAP_ENTRY_COMPARE(h, HeapIndex, Heap->Used))
		HeapSiftDown(Heap, HeapIndex);
	else
		HeapSiftUp(Heap, HeapIndex);

	_HFREE(Heap->Entries[Heap->Used]);
	Heap->Entries[Heap->Used] = NULL;
}

// Heapify
VOID
HeapMake(PHeap Heap)
{
	ULONG i;
	if (Heap->Used < 2)
		return;
	i = Parent(Heap->Used - 1);
	while (i != 0)
	{
		HeapSiftDown(Heap, i);
		i--;
	}
	HeapSiftDown(Heap, 0);
}

VOID
HeapSort(PHeap Heap)
{
	ULONG i, Used;
	PHeapEntry* h = Heap->Entries;
	if (Heap->Used < 2)
		return;
	Used = Heap->Used;
	HeapMake(Heap);
	for (i=Used-1; i>0; i--)
	{
		HEAP_ENTRY_SWAP(h, 0, i);
		Heap->Used--;
		HeapSiftDown(Heap, 0);
	}
	Heap->Used = Used;
}


VOID
HeapZeroValue(PHeap Heap, ULONG HeapIndex)
{
	if (Heap->Entries[HeapIndex]->Value)
	{
		Heap->Entries[HeapIndex]->Value = 0;
		#ifdef MIN_HEAP
			HeapSiftUp(Heap, HeapIndex);
		#else
			HeapSiftDown(Heap, HeapIndex);
		#endif
	}
}

VOID
HeapIncreaseValue(PHeap Heap, ULONG HeapIndex, ULONG Inc)
{
	Heap->Entries[HeapIndex]->Value += Inc;
	#ifdef MIN_HEAP
		HeapSiftDown(Heap, HeapIndex);
	#else
		HeapSiftUp(Heap, HeapIndex);
	#endif
}

VOID
HeapDecreaseValue(PHeap Heap, ULONG HeapIndex, ULONG Dec)
{
	if (Heap->Entries[HeapIndex]->Value == 0 || Dec == 0)
		return;
	if (Heap->Entries[HeapIndex]->Value > Dec)
		Heap->Entries[HeapIndex]->Value -= Dec;
	else
		Heap->Entries[HeapIndex]->Value = 0;
	#ifdef MIN_HEAP
		HeapSiftUp(Heap, HeapIndex);
	#else
		HeapSiftDown(Heap, HeapIndex);
	#endif
}

HEAP_DAT_T*
GetHeapTop(PHeap Heap)
{
	if (Heap->Used == 0)
		return NULL;
	return Heap->Entries[0]->pData;
}

HEAP_DAT_T*
GetAndRemoveHeapTop(PHeap Heap)
{
	HEAP_DAT_T* ret;
	if (Heap->Used == 0)
		return NULL;
	ret = Heap->Entries[0]->pData;
	HeapDelete(Heap, 0);
	return ret;
}
