#pragma once

#ifdef WINVER
	#include <Ntddk.h>
	#define CACHE_POOL_TAG						'cpiD'
#else
	#include <stdlib.h>
	#include <string.h>
	#include <assert.h>
	#define CACHE_POOL_TAG		'$'
	#define NonPagedPool		'$'
	#define TRUE				1
	#define FALSE				0
	#define ASSERT(expr)						assert((expr))
	#define ExAllocatePoolWithTag(t,length,tag)	malloc((length))
	#define ExFreePoolWithTag(ptr,tag)			free((ptr))
	#define RtlCopyMemory(dst,src,len)			memcpy((dst),(src),(len))
	typedef void				VOID;
	typedef void*				PVOID;
	typedef long long			LONGLONG;
	typedef unsigned int		SIZE_T;
	typedef unsigned long		ULONG;
	typedef unsigned char		BOOLEAN;
	typedef unsigned char		UCHAR;
	typedef unsigned char*		PUCHAR;
	typedef union _LARGE_INTEGER
	{
		LONGLONG QuadPart;
	} LARGE_INTEGER, *PLARGE_INTEGER;
#endif

#define CACHE_POOL_SIZE						8192
#define SECTOR_SIZE							512
#define _READ_								TRUE
#define _WRITE_								FALSE

typedef struct _CACHE_BLOCK
{
	LARGE_INTEGER		Index;
	UCHAR				Data[SECTOR_SIZE];
}CACHE_BLOCK, *PCACHE_BLOCK;

typedef struct _CACHE_POOL
{
	ULONG				Size;
	ULONG				Used;
	PCACHE_BLOCK		Blocks;
}CACHE_POOL, *PCACHE_POOL;

BOOLEAN
	InitCachePool (PCACHE_POOL CachePool);

VOID
	DestroyCachePool (PCACHE_POOL CachePool);

BOOLEAN
	QueryAndCopyFromCachePool (
		PCACHE_POOL CachePool,
		PUCHAR Buf,
		LARGE_INTEGER Offset,
		ULONG Length
	);

VOID
	UpdataCachePool (
		PCACHE_POOL CachePool,
		PUCHAR Buf,
		LARGE_INTEGER Offset,
		ULONG Length,
		BOOLEAN Type
	);
