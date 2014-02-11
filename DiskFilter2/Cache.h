#pragma once

#include "bpt.h"
#include "common.h"

#define READ_VERIFY

#define _READ_								TRUE
#define _WRITE_								FALSE
#define SECTOR_SIZE							512
#define NSB									1		/* Number Sectors per Block */
#define BLOCK_SIZE							(SECTOR_SIZE*NSB)
#define CACHE_POOL_SIZE						50		/* MB */

#define CACHE_DATA_T		PUCHAR
#define CACHE_DATA_ALLOC()	ExAllocatePoolWithTag(NonPagedPool, (SIZE_T)BLOCK_SIZE, CACHE_POOL_TAG);
#define CACHE_DATA_FREE(p)	ExFreePoolWithTag(p, CACHE_POOL_TAG);
#define CACHE_DATA_WRITE(cdata,off,p,len) \
			RtlCopyMemory(cdata+off, p, len);
#define CACHE_DATA_READ(p,cdata,off,len) \
			RtlCopyMemory(p, cdata+off, len);

typedef struct _CACHE_BLOCK
{
	BOOLEAN				Accessed;
	BOOLEAN				Modified;
	LONGLONG			Index;
	CACHE_DATA_T		Data;
}CACHE_BLOCK, *PCACHE_BLOCK;

typedef struct _CACHE_POOL
{
	ULONG				Size;
	ULONG				Used;
	node*				bpt_root;
}CACHE_POOL, *PCACHE_POOL;

BOOLEAN
	InitCachePool (PCACHE_POOL CachePool);

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

#define DO_READ_VERIFY(pBlock,Buffer)	\
		if(g_bDataVerify){														\
			NTSTATUS Status;													\
			ULONG matched1, matched2;											\
			LARGE_INTEGER readOffset;											\
			UCHAR Data[BLOCK_SIZE];												\
			readOffset.QuadPart = BLOCK_SIZE * pBlock->Index;					\
			Status = IoDoRWRequestSync (										\
						IRP_MJ_READ,											\
						LowerDeviceObject,										\
						Data,													\
						BLOCK_SIZE,												\
						&readOffset												\
					);															\
			if (NT_SUCCESS(Status))												\
			{																	\
				matched1 = RtlCompareMemory(Data, pBlock->Data, BLOCK_SIZE);	\
				matched2 = RtlCompareMemory(Data, Buffer, BLOCK_SIZE);			\
			}																	\
			else																\
			{																	\
				matched1 = 9999999;												\
				matched2 = 9999999;												\
			}																	\
			if (matched1 != BLOCK_SIZE || matched2 != BLOCK_SIZE)				\
				DbgPrint("%s: %d-%d: --(%d)<-(%d)->(%d)--\n", __FUNCTION__,		\
				DiskNumber, PartitionNumber, matched1, BLOCK_SIZE, matched2);	\
		}
