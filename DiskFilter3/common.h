#pragma once

#ifndef WINVER
#define USER_APP
#endif

#define SECTOR_SIZE                     512
#define NSB                             8       /* Number Sectors per Block */
#define BLOCK_SIZE                      (SECTOR_SIZE*NSB)

#define WB_QUEUE_NUM_BLOCKS             3000
#define WRITE_BACK_ENABLE

#define CACHE_POOL_SIZE                 50      /* MB */
#define CACHE_POOL_NUM_BLOCKS           ((CACHE_POOL_SIZE << 20)/(BLOCK_SIZE))

#define PROFILE

#ifndef USER_APP
    #include <Ntifs.h>
    #define CACHE_POOL_TAG                      'cpiD'
    #define HEAP_POOL_TAG                       'hepD'
    #define STORAGE_POOL_TAG                    'stoD'
    #define BPT_POOL_TAG                        'bptD'
    #define assert(expr)                        ASSERT(expr)
    #define BPT_FREE(p)                         ExFreePoolWithTag(p,BPT_POOL_TAG)
    #define BPT_MALLOC(n)                       ExAllocatePoolWithTag ( \
                                                    NonPagedPool,       \
                                                    (SIZE_T)(n),        \
                                                    BPT_POOL_TAG        \
                                                )
    #define ZeroMemory(dest,len)                RtlZeroMemory(dest,len)
#else
    #include <stdlib.h>
    #include <string.h>
    #include <assert.h>
    #define CACHE_POOL_TAG                      '$'
    #define NonPagedPool                        '$'
    #define TRUE                                1
    #define FALSE                               0
    #define ASSERT(expr)                        assert((expr))
    #define DbgPrint(str)                       fprintf(stderr,str)
    #define BPT_MALLOC(length)                  malloc((length))
    #define ExAllocatePoolWithTag(t,length,tag) malloc((length))
    #define BPT_FREE(ptr)                       free((ptr))
    #define ExFreePoolWithTag(ptr,tag)          free((ptr))
    #define RtlCopyMemory(dst,src,len)          memcpy((dst),(src),(len))
    typedef void                                VOID;
    typedef void*                               PVOID;
    typedef long long                           LONGLONG;
    typedef unsigned int                        SIZE_T;
    typedef unsigned long                       ULONG;
    typedef unsigned char                       BOOLEAN;
    typedef unsigned char*                      PBOOLEAN;
    typedef unsigned char                       UCHAR;
    typedef unsigned char*                      PUCHAR;
    typedef union _LARGE_INTEGER
    {
        LONGLONG QuadPart;
    } LARGE_INTEGER, *PLARGE_INTEGER;
#endif

extern ULONG                g_TraceFlags;
extern PDEVICE_OBJECT       g_pDeviceObject;
extern PDRIVER_OBJECT       g_pDriverObject;
extern BOOLEAN              g_bDataVerify;
