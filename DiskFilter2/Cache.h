#pragma once

#include "bpt.h"
#include "common.h"
#include "Storage.h"

#define READ_VERIFY
#define USE_LRU
//#define USE_LFU
//#define USE_SLRU
//#define USE_SLFU
//#define USE_OCP

#define CACHE_POOL_SIZE						200		/* MB */

typedef struct _CACHE_BLOCK
{
	BOOLEAN					Modified;
	LONGLONG				Index;
	ULONG					StorageIndex;
#if defined(USE_LFU) || defined(USE_LRU) || \
	defined(USE_SLFU) || defined(USE_SLRU)
	ULONG					HeapIndex;
#endif
#if defined(USE_SLFU) || defined(USE_SLRU) || defined(USE_OCP)
	ULONG					Protected;
#endif
#if defined(USE_OCP)
	struct _CACHE_BLOCK*	Prior;
	struct _CACHE_BLOCK*	Next;
	ULONG					ReferenceCount;
#endif
}CACHE_BLOCK, *PCACHE_BLOCK;

#if defined(USE_LFU) || defined(USE_SLFU)
#  define HEAP_VAL_T LONG
#elif  defined(USE_LRU) || defined(USE_SLRU)
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
#endif
#if defined(USE_LFU) || defined(USE_LRU)
	Heap			Heap;
	node*			bpt_root;
#endif
#if defined(USE_SLFU) || defined(USE_SLRU)
	ULONG			ProbationarySize;
	Heap			ProbationaryHeap;
	node*			Probationary_bpt_root;
	ULONG			ProtectedSize;
	Heap			ProtectedHeap;
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
	ReadUpdataCachePool (
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
	WriteUpdataCachePool (
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
/**
 * Internal Functions Used by Common Function
 */
PCACHE_BLOCK	__GetFreeBlock(PCACHE_POOL CachePool);
BOOLEAN			_AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified);
VOID			_DeleteOneBlockFromPool(PCACHE_POOL CachePool, LONGLONG Index);
BOOLEAN			_QueryPoolByIndex(PCACHE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock);
VOID			_FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified);
VOID			_IncreaseBlockReference(PCACHE_POOL CachePool, PCACHE_BLOCK pBlock);
BOOLEAN			_IsFull(PCACHE_POOL CachePool);

/*
    	off
    +---------+---------+------+--+
    |    | fb |   ...   |  eb  |  |
    +---------+---------+------+--+
*/
#define detect_broken(Off,Len,front_broken,end_broken,front_offset,front_skip,end_cut)\
		do{																		\
			front_broken = FALSE;												\
			end_broken = FALSE;													\
			front_offset = 0;													\
			front_skip = 0;														\
			end_cut = 0;														\
			if(Off % BLOCK_SIZE !=0)											\
			{																	\
				front_broken = TRUE;											\
				front_offset = Off % BLOCK_SIZE;								\
				front_skip = BLOCK_SIZE - front_offset;							\
				Off += front_skip;												\
				front_skip = (Len>front_skip) ? front_skip : Len;				\
				Len = Len - front_skip;											\
			}																	\
			if(Len % BLOCK_SIZE != 0)											\
			{																	\
				end_broken = TRUE;												\
				end_cut = Len % BLOCK_SIZE;										\
			}																	\
		}while(0);

#define DO_READ_VERIFY(Storage,pBlock,PhysicalDeviceObject)						\
		while (g_bDataVerify)													\
		{																		\
			NTSTATUS Status;													\
			SIZE_T matched;														\
			PUCHAR D1, D2;														\
			LARGE_INTEGER readOffset;											\
			D1 = ExAllocatePoolWithTag(NonPagedPool, BLOCK_SIZE, 'tmpb');		\
			if (D1 == NULL) break;												\
			D2 = ExAllocatePoolWithTag(NonPagedPool, BLOCK_SIZE, 'tmpb');		\
			if (D2 == NULL) { ExFreePoolWithTag(D1, 'tmpb'); break; }			\
			readOffset.QuadPart = BLOCK_SIZE * pBlock->Index;					\
			Status = IoDoRWRequestSync (										\
						IRP_MJ_READ,											\
						PhysicalDeviceObject,									\
						D1,														\
						BLOCK_SIZE,												\
						&readOffset												\
					);															\
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

#define ADD_TO_WBQUEUE(pBlock)													\
		{																		\
			KIRQL Irql;															\
			KeAcquireSpinLock(&CachePool->WbQueueSpinLock, &Irql);				\
			pBlock->Modified = TRUE;											\
			QueueInsert(&CachePool->WbQueue, pBlock);							\
			KeReleaseSpinLock(&CachePool->WbQueueSpinLock, Irql);				\
		}
