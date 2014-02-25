#pragma once

#include "bpt.h"
#include "common.h"
#include "Storage.h"

#define READ_VERIFY

#define _READ_								TRUE
#define _WRITE_								FALSE
#define CACHE_POOL_SIZE						50		/* MB */

typedef struct _CACHE_BLOCK
{
	BOOLEAN				Modified;
	LONGLONG			Index;
	ULONG				StorageIndex;
	ULONG				HeapIndex;
}CACHE_BLOCK, *PCACHE_BLOCK;

#define HEAP_DAT_T CACHE_BLOCK
typedef struct _HeapEntry
{
	ULONG Value;
	HEAP_DAT_T* pData;
}HeapEntry, *PHeapEntry;

typedef struct _Heap
{
	ULONG		Size;
	ULONG		Used;
	PHeapEntry*	Entries;
}Heap, *PHeap;

typedef struct _CACHE_POOL
{
	ULONG				Size;
	ULONG				Used;
	node*				bpt_root;
	STORAGE_POOL		Storage;
	Heap				Heap;
}CACHE_POOL, *PCACHE_POOL;

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

VOID
	UpdataCachePool (
		PCACHE_POOL CachePool,
		PUCHAR Buf,
		LONGLONG Offset,
		ULONG Length,
		BOOLEAN Type
	#ifdef READ_VERIFY
		,PDEVICE_OBJECT LowerDeviceObject
		,ULONG DiskNumber
		,ULONG PartitionNumber
	#endif
	);

#define detect_broken(Off,Len,front_broken,end_broken,front_skip,end_cut)	\
		do{												\
			front_broken=FALSE;							\
			end_broken=FALSE;							\
			front_skip=0;								\
			end_cut=0;									\
			if(Off%BLOCK_SIZE!=0)						\
			{											\
				front_broken=TRUE;						\
				front_skip=BLOCK_SIZE-(Off%BLOCK_SIZE);	\
				Off+=front_skip;						\
				Len=(Len>front_skip)?(Len-front_skip):0;\
			}											\
			if(Len%BLOCK_SIZE!=0)						\
			{											\
				end_broken=TRUE;						\
				end_cut=Len%BLOCK_SIZE;					\
			}											\
		}while(0);

#define DO_READ_VERIFY(Storage,pBlock,PhysicalDeviceObject)						\
		while (1 && g_bDataVerify)												\
		{																		\
			NTSTATUS Status;													\
			ULONG matched;														\
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
				DbgPrint("%s: %d-%d: --(%d)->(%d)--\n", __FUNCTION__,			\
					DiskNumber, PartitionNumber, BLOCK_SIZE, matched);			\
			ExFreePoolWithTag(D1, 'tmpb');										\
			ExFreePoolWithTag(D2, 'tmpb');										\
			break;																\
		}
