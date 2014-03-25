#include "Queue.h"

#define _QMALLOC(n)			ExAllocatePoolWithTag (	NonPagedPool,	\
													(SIZE_T)(n),	\
													HEAP_POOL_TAG )
#define _QFREE(p)			ExFreePoolWithTag (p, HEAP_POOL_TAG)
#define _QZEROMEM(p,len)	RtlZeroMemory(p,(SIZE_T)(len))

BOOLEAN InitQueue (PQueue Queue, ULONG Size)
{
	_QZEROMEM(Queue, sizeof(Queue));
	Queue->Data = (QUEUE_DAT_T*)_QMALLOC(Size*sizeof(QUEUE_DAT_T));
	if (Queue->Data == NULL)
		return FALSE;
	Queue->Head = 0;
	Queue->Tail = 0;
	Queue->Used = 0;
	Queue->Size = Size;
	return TRUE;
}

VOID DestroyQueue (PQueue Queue)
{
	if (Queue->Data != NULL)
		_QFREE(Queue->Data);
	_QZEROMEM(Queue, sizeof(Queue));
}

BOOLEAN QueueInsert (PQueue Queue, QUEUE_DAT_T Entry)
{
	if (QueueIsFull(Queue) == TRUE)
		return FALSE;
	Queue->Data[Queue->Tail] = Entry;
	Queue->Tail = (Queue->Tail + 1) % Queue->Size;
	Queue->Used++;
	return TRUE;
}

QUEUE_DAT_T QueueRemove (PQueue Queue)
{
	QUEUE_DAT_T ret;
	if (QueueIsEmpty(Queue) == TRUE)
		return NULL;
	ret = Queue->Data[Queue->Head];
	Queue->Head = (Queue->Head + 1) % Queue->Size;
	Queue->Used--;
	return ret;
}

BOOLEAN QueueIsEmpty (PQueue Queue)
{
	return (Queue->Used == 0);
}

BOOLEAN QueueIsFull (PQueue Queue)
{
	return (Queue->Used == Queue->Size);
}
