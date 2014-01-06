#pragma once

#ifndef WINVER
#define USER_APP
#endif

#ifdef USER_APP
  #include <stdio.h>
  #include <stdlib.h>
  #include <assert.h>

#else
  #include <Ntddk.h>
  #define BPT_POOL_TAG	'bptD'
  #define MALLOC(n)		ExAllocatePoolWithTag (	\
							NonPagedPool,		\
							(SIZE_T)(n),		\
							BPT_POOL_TAG		\
						)
  #define FREE(p)		ExFreePoolWithTag(p,BPT_POOL_TAG)
  #define assert(expr)	ASSERT(expr)
#endif

#ifdef WINVER
	#include <Ntddk.h>
	#define CACHE_POOL_TAG						'cpiD'
#else
	#include <stdlib.h>
	#include <string.h>
	#include <assert.h>
	#define CACHE_POOL_TAG						'$'
	#define NonPagedPool						'$'
	#define TRUE								1
	#define FALSE								0
	#define ASSERT(expr)						assert((expr))
	#define DbgPrint(str)						fprintf(stderr,str)
	#define MALLOC(length)						malloc((length))
	#define ExAllocatePoolWithTag(t,length,tag)	malloc((length))
	#define FREE(ptr)							free((ptr))
	#define ExFreePoolWithTag(ptr,tag)			free((ptr))
	#define RtlCopyMemory(dst,src,len)			memcpy((dst),(src),(len))
	typedef void								VOID;
	typedef void*								PVOID;
	typedef long long							LONGLONG;
	typedef unsigned int						SIZE_T;
	typedef unsigned long						ULONG;
	typedef unsigned char						BOOLEAN;
	typedef unsigned char						UCHAR;
	typedef unsigned char*						PUCHAR;
	typedef union _LARGE_INTEGER
	{
		LONGLONG QuadPart;
	} LARGE_INTEGER, *PLARGE_INTEGER;
#endif
