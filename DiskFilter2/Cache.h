#pragma once

#include "bpt.h"
#include "redblack.h"
#include "common.h"
#include "Storage.h"

#define READ_VERIFY
//#define USE_LRU
//#define USE_SLRU
#define USE_OCP

typedef struct _CACHE_BLOCK
{
	BOOLEAN					Modified;
	LONGLONG				Index;
	ULONG					StorageIndex;
#if 0
	ULONG					HeapIndex;
#endif
#if defined(USE_SLRU) || defined(USE_OCP)
	ULONG					Protected;
#endif
#if defined(USE_LRU) || defined(USE_SLRU) || defined(USE_OCP)
	struct _CACHE_BLOCK*	Prior;
	struct _CACHE_BLOCK*	Next;
	ULONG					ReferenceCount;
#endif
}CACHE_BLOCK, *PCACHE_BLOCK;

#if 0
#  define HEAP_VAL_T LONGLONG
#else
#  define HEAP_VAL_T LONG
#endif
#define HEAP_DAT_T CACHE_BLOCK
typedef struct _HeapEntry
{
	HEAP_VAL_T		Value;
	HEAP_DAT_T*		pData;
}HeapEntry, *PHeapEntry;

typedef struct _Heap
{
	ULONG			Size;
	ULONG			Used;
	PHeapEntry*		Entries;
}Heap, *PHeap;

#define LIST_DAT_T CACHE_BLOCK
typedef struct _List
{
	ULONG			Size;
	LIST_DAT_T*		Head;
	LIST_DAT_T*		Tail;
}List, *PList;

#define QUEUE_DAT_T PCACHE_BLOCK
typedef struct _Queue
{
	ULONG			Size;
	ULONG			Used;
	ULONG			Head;
	ULONG			Tail;
	QUEUE_DAT_T*	Data;
}Queue, *PQueue;

typedef struct _CACHE_POOL
{
	ULONG			Size;
	ULONG			Used;
	STORAGE_POOL	Storage;
	ULONG32			ReadHit;
	ULONG32			WriteHit;
#ifdef WRITE_BACK_ENABLE
	Queue			WbQueue;
	KSPIN_LOCK		WbQueueSpinLock;
	BOOLEAN			WbFlushAll;
	KEVENT			WbThreadStartEvent;
	KEVENT			WbThreadFinishEvent;
#endif
#if defined(USE_LRU)
	List			List;
	rb_tree_t		rb_tree;
#endif
#if defined(USE_SLRU)
	ULONG			ProbationarySize;
	List			ProbationaryList;
	node*			Probationary_bpt_root;
	ULONG			ProtectedSize;
	List			ProtectedList;
	node*			Protected_bpt_root;
#endif
#if defined(USE_OCP)
	List			HotList;
	ULONG			HotSize;
	node*			hot_bpt_root;
	List			ColdList;
	ULONG			ColdSize;
	node*			cold_bpt_root;
#endif
}CACHE_POOL, *PCACHE_POOL;

/**
 * Export Functions
 */
BOOLEAN
	InitCachePool (
		PCACHE_POOL CachePool
	#ifndef USE_DRAM
		,ULONG DiskNum
		,ULONG PartitionNum
	#endif
	);

VOID
	DestroyCachePool (PCACHE_POOL CachePool);

BOOLEAN
	QueryAndCopyFromCachePool (
		PCACHE_POOL CachePool,
		PUCHAR Buf,
		LONGLONG Offset,
		ULONG Length
	#ifdef READ_VERIFY
		,PDEVICE_OBJECT LowerDeviceObject
		,ULONG DiskNumber
		,ULONG PartitionNumber
	#endif
	);

BOOLEAN
	QueryAndWriteToCachePool (
		PCACHE_POOL CachePool,
		PUCHAR Buf,
		LONGLONG Offset,
		ULONG Length
	#ifdef READ_VERIFY
		,PDEVICE_OBJECT LowerDeviceObject
		,ULONG DiskNumber
		,ULONG PartitionNumber
	#endif
	);

VOID
	ReadUpdateCachePool (
		PCACHE_POOL CachePool,
		PUCHAR Buf,
		LONGLONG Offset,
		ULONG Length
	#ifdef READ_VERIFY
		,PDEVICE_OBJECT LowerDeviceObject
		,ULONG DiskNumber
		,ULONG PartitionNumber
	#endif
	);

VOID
	WriteUpdateCachePool (
		PCACHE_POOL CachePool,
		PUCHAR Buf,
		LONGLONG Offset,
		ULONG Length
	#ifdef READ_VERIFY
		,PDEVICE_OBJECT LowerDeviceObject
		,ULONG DiskNumber
		,ULONG PartitionNumber
	#endif
	);
extern void Free_Record ( record * n );
/**
 * Internal Functions Used by Common Function
 */
PCACHE_BLOCK	__GetFreeBlock(PCACHE_POOL CachePool);
PCACHE_BLOCK	_AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified);
VOID			_DeleteOneBlockFromPool(PCACHE_POOL CachePool, LONGLONG Index);
BOOLEAN			_QueryPoolByIndex(PCACHE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock);
PCACHE_BLOCK	_FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified);
VOID			_IncreaseBlockReference(PCACHE_POOL CachePool, PCACHE_BLOCK pBlock);
BOOLEAN			_IsFull(PCACHE_POOL CachePool);

/*
    	off
    +----+----+---------+------+--+
    | fo | fs |   ...   |  ec  |  |
    +----+----+---------+------+--+
*/
#define detect_broken(Off,Len,front_broken,end_broken,front_offset,front_skip,end_cut)\
		do{																		\
			front_broken = FALSE;												\
			end_broken = FALSE;													\
			front_offset = 0;													\
			front_skip = 0;														\
			end_cut = 0;														\
			front_offset = Off % BLOCK_SIZE;									\
			if(front_offset !=0)												\
			{																	\
				front_broken = TRUE;											\
				front_skip = BLOCK_SIZE - front_offset;							\
				Off += front_skip;												\
				front_skip = (Len>front_skip) ? front_skip : Len;				\
				Len = Len - front_skip;											\
			}																	\
			if(Len % BLOCK_SIZE != 0)											\
			{																	\
				end_broken = TRUE;												\
				end_cut = Len % BLOCK_SIZE;										\
				Len = Len - end_cut;											\
			}																	\
		}while(0);

#ifdef READ_VERIFY
#define DO_READ_VERIFY(CachePool,Storage,pBlock)								\
		while (g_bDataVerify && pBlock->Modified == FALSE)						\
		{																		\
			NTSTATUS Status;													\
			SIZE_T matched;														\
			PUCHAR D1, D2;														\
			LARGE_INTEGER readOffset;											\
			KIRQL Irql;															\
			D1 = ExAllocatePoolWithTag(NonPagedPool, BLOCK_SIZE, 'tmpb');		\
			if (D1 == NULL) break;												\
			D2 = ExAllocatePoolWithTag(NonPagedPool, BLOCK_SIZE, 'tmpb');		\
			if (D2 == NULL) { ExFreePoolWithTag(D1, 'tmpb'); break; }			\
			KeAcquireSpinLock(&CachePool->WbQueueSpinLock, &Irql);				\
			readOffset.QuadPart = BLOCK_SIZE * pBlock->Index;					\
			Status = IoDoRWRequestSync (										\
						IRP_MJ_READ,											\
						LowerDeviceObject,										\
						D1,														\
						BLOCK_SIZE,												\
						&readOffset												\
					);															\
			KeReleaseSpinLock(&CachePool->WbQueueSpinLock, Irql);				\
			StoragePoolRead(Storage, D2, pBlock->StorageIndex, 0, BLOCK_SIZE);	\
			if (NT_SUCCESS(Status))												\
				matched = RtlCompareMemory(D1, D2, BLOCK_SIZE);					\
			else																\
				matched = 9999999;												\
			if (matched != BLOCK_SIZE)											\
				DbgPrint("%s: %d-%d: --(%d)->(%Iu)--\n", __FUNCTION__,			\
					DiskNumber, PartitionNumber, BLOCK_SIZE, matched);			\
			ExFreePoolWithTag(D1, 'tmpb');										\
			ExFreePoolWithTag(D2, 'tmpb');										\
			break;																\
		}
#else
#define DO_READ_VERIFY(CachePool,Storage,pBlock)
#endif

#ifdef WRITE_BACK_ENABLE
#define ADD_TO_WBQUEUE(pBlock)														\
		{																			\
			KIRQL Irql;																\
			while (QueueIsFull(&CachePool->WbQueue))								\
			{																		\
				KeSetEvent(&CachePool->WbThreadStartEvent, IO_NO_INCREMENT, FALSE);	\
				KeWaitForSingleObject(&CachePool->WbThreadFinishEvent,				\
										Executive, KernelMode, FALSE, NULL);		\
			}																		\
			KeAcquireSpinLock(&CachePool->WbQueueSpinLock, &Irql);					\
			pBlock->Modified = TRUE;												\
			ASSERT(TRUE == QueueInsert(&CachePool->WbQueue, pBlock));				\
			KeReleaseSpinLock(&CachePool->WbQueueSpinLock, Irql);					\
			KeSetEvent(&CachePool->WbThreadStartEvent, IO_NO_INCREMENT, FALSE);		\
		}
#else
#define ADD_TO_WBQUEUE(pBlock)													\
		{																		\
			if (pBlock)															\
				pBlock->Modified = TRUE;										\
		}
#endif
