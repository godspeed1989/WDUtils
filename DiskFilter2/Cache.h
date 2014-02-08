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

typedef struct _CACHE_BLOCK
{
	BOOLEAN				Accessed;
	BOOLEAN				Modified;
	LONGLONG			Index;
	UCHAR				Data[BLOCK_SIZE];
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
