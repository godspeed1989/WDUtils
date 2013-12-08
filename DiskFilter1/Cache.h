#pragma once

#include <Ntddk.h>

#define CACHE_POOL_SIZE						4096
#define SECTOR_SIZE							512
#define CACHE_POOL_TAG						'cpiD'
#define _READ_								TRUE
#define _WRITE_								FALSE

typedef struct _CACHE_BLOCK
{
	LARGE_INTEGER		Index;
	UCHAR				Data[SECTOR_SIZE];
}CACHE_BLOCK, *PCACHE_BLOCK;

typedef struct _CAHCE_POOL
{
	ULONG				Size;
	ULONG				Used;
	PCACHE_BLOCK		Blocks;
}CACHE_POOL, *PCAHCE_POOL;

BOOLEAN
	InitCachePool(PCAHCE_POOL CachePool);

VOID
	DestroyCachePool(PCAHCE_POOL CachePool);

BOOLEAN
	QueryAndCopyFromCachePool (
		PCAHCE_POOL CachePool,
		PUCHAR Buf,
		LARGE_INTEGER Offset,
		ULONG Length
	);

VOID
	UpdataCachePool (
		PCAHCE_POOL CachePool,
		PUCHAR Buf,
		LARGE_INTEGER Offset,
		ULONG Length,
		BOOLEAN Type
	);
