#pragma once

#include "bpt.h"
#include "common.h"

#define _READ_								TRUE
#define _WRITE_								FALSE
#define SECTOR_SIZE							512
#define CACHE_POOL_SIZE						8192

typedef struct _CACHE_BLOCK
{
	BOOLEAN				Accessed;
	BOOLEAN				Modified;
	UCHAR				Data[SECTOR_SIZE];
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
	);
