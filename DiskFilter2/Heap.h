#pragma once

#include "Cache.h"

#define MIN_HEAP

BOOLEAN InitHeap (PHeap Heap, ULONG Size);

VOID DestroyHeap (PHeap Heap);

VOID HeapMake (PHeap Heap);

VOID HeapSort (PHeap Heap);

BOOLEAN HeapInsert (PHeap Heap, HEAP_DAT_T *pData);

VOID HeapDelete (PHeap Heap, ULONG HeapIndex);

VOID HeapZeroValue (PHeap Heap, ULONG HeapIndex);

VOID HeapIncreaseValue (PHeap Heap, ULONG HeapIndex, ULONG Inc);

VOID HeapDecreaseValue (PHeap Heap, ULONG HeapIndex, ULONG Dec);

HEAP_DAT_T* GetHeapTop(PHeap Heap);

HEAP_DAT_T* GetAndRemoveHeapTop(PHeap Heap);

#define HEAP_ENTRY_SWAP(h,Index1,Index2)			\
	do{												\
		PHeapEntry temp;							\
		h[Index1]->pData->HeapIndex = Index2;		\
		h[Index2]->pData->HeapIndex = Index1;		\
		temp = h[Index1];							\
		h[Index1] = h[Index2];						\
		h[Index2] = temp;							\
	}while(0)

#ifdef MIN_HEAP
#define HEAP_ENTRY_COMPARE(h,front,end)				\
		(h[end]->Value < h[front]->Value)
#else
#define HEAP_ENTRY_COMPARE(h,front,end)				\
		(h[end]->Value > h[front]->Value)
#endif
