#pragma once

#include "bpt.h"
#include "common.h"
#include "Storage.h"

#define READ_VERIFY
//#define USE_LRU
//#define USE_SLRU
#define USE_OCP

#ifdef BLOCK_STORAGE_WRITE_BUFF
#undef READ_VERIFY
#endif

typedef struct _CACHE_BLOCK
{
    BOOLEAN                 Modified;
    LONGLONG                Index;
    ULONG                   StorageIndex;
#if defined(USE_SLRU) || defined(USE_OCP)
    ULONG                   Protected;
#endif
#if defined(USE_LRU) || defined(USE_SLRU) || defined(USE_OCP)
    struct _CACHE_BLOCK*    Prior;
    struct _CACHE_BLOCK*    Next;
    ULONG                   ReferenceCount;
#endif
#ifdef WRITE_BACK_ENABLE
    LIST_ENTRY              ListEntry;
#endif
}CACHE_BLOCK, *PCACHE_BLOCK;

#define LIST_DAT_T CACHE_BLOCK
typedef struct _List
{
    ULONG           Size;
    LIST_DAT_T*     Head;
    LIST_DAT_T*     Tail;
}List, *PList;

typedef struct _CACHE_POOL
{
    ULONG           Size;
    ULONG           Used;
    STORAGE_POOL    Storage;
    PVOID           DevExt;
    ULONG32         ReadHit;
    ULONG32         WriteHit;
#ifdef WRITE_BACK_ENABLE
    LIST_ENTRY      WbList;
    KSPIN_LOCK      WbQueueLock;
    BOOLEAN         WbFlushAll;
    KEVENT          WbThreadStartEvent;
    KEVENT          WbThreadFinishEvent;
#endif
#if defined(USE_LRU)
    List            List;
    node*           bpt_root;
#endif
#if defined(USE_SLRU)
    ULONG           ProbationarySize;
    List            ProbationaryList;
    node*           Probationary_bpt_root;
    ULONG           ProtectedSize;
    List            ProtectedList;
    node*           Protected_bpt_root;
#endif
#if defined(USE_OCP)
    List            HotList;
    ULONG           HotSize;
    node*           hot_bpt_root;
    List            ColdList;
    ULONG           ColdSize;
    node*           cold_bpt_root;
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
PCACHE_BLOCK    __GetFreeBlock(PCACHE_POOL CachePool);
PCACHE_BLOCK    _AddNewBlockToPool(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified);
VOID            _DeleteOneBlockFromPool(PCACHE_POOL CachePool, LONGLONG Index);
BOOLEAN         _QueryPoolByIndex(PCACHE_POOL CachePool, LONGLONG Index, PCACHE_BLOCK *ppBlock);
PCACHE_BLOCK    _FindBlockToReplace(PCACHE_POOL CachePool, LONGLONG Index, PVOID Data, BOOLEAN Modified);
VOID            _IncreaseBlockReference(PCACHE_POOL CachePool, PCACHE_BLOCK pBlock);
BOOLEAN         _IsFull(PCACHE_POOL CachePool);

/*
        off
    +----+----+---------+------+--+
    | fo | fs |   ...   |  ec  |  |
    +----+----+---------+------+--+
*/
#define detect_broken(Off,Len,front_broken,end_broken,front_offset,front_skip,end_cut)\
        do{                                                                     \
            front_broken = FALSE;                                               \
            end_broken = FALSE;                                                 \
            front_offset = 0;                                                   \
            front_skip = 0;                                                     \
            end_cut = 0;                                                        \
            front_offset = Off % BLOCK_SIZE;                                    \
            if(front_offset != 0)                                               \
            {                                                                   \
                front_broken = TRUE;                                            \
                front_skip = BLOCK_SIZE - front_offset;                         \
                Off += front_skip;                                              \
                front_skip = (Len>front_skip) ? front_skip : Len;               \
                Len = Len - front_skip;                                         \
            }                                                                   \
            if(Len % BLOCK_SIZE != 0)                                           \
            {                                                                   \
                end_broken = TRUE;                                              \
                end_cut = Len % BLOCK_SIZE;                                     \
                Len = Len - end_cut;                                            \
            }                                                                   \
        }while(0);

#ifdef READ_VERIFY
#define DO_READ_VERIFY(CachePool,Storage,pBlock)                                \
        while (g_bDataVerify && pBlock->Modified == FALSE)                      \
        {                                                                       \
            NTSTATUS Status;                                                    \
            SIZE_T matched;                                                     \
            PUCHAR D1, D2;                                                      \
            LARGE_INTEGER readOffset;                                           \
            D1 = ExAllocatePoolWithTag(NonPagedPool, BLOCK_SIZE, 'tmpb');       \
            if (D1 == NULL) break;                                              \
            D2 = ExAllocatePoolWithTag(NonPagedPool, BLOCK_SIZE, 'tmpb');       \
            if (D2 == NULL) { ExFreePoolWithTag(D1, 'tmpb'); break; }           \
            readOffset.QuadPart = BLOCK_SIZE * pBlock->Index;                   \
            Status = IoDoRWRequestSync (                                        \
                        IRP_MJ_READ,                                            \
                        LowerDeviceObject,                                      \
                        D1,                                                     \
                        BLOCK_SIZE,                                             \
                        &readOffset                                             \
                    );                                                          \
            StoragePoolRead(Storage, D2, pBlock->StorageIndex, 0, BLOCK_SIZE);  \
            if (NT_SUCCESS(Status))                                             \
                matched = RtlCompareMemory(D1, D2, BLOCK_SIZE);                 \
            else                                                                \
                matched = 9999999;                                              \
            if (matched != BLOCK_SIZE)                                          \
                DbgPrint("%s: %d-%d: --(%d)->(%Iu)--\n", __FUNCTION__,          \
                    DiskNumber, PartitionNumber, BLOCK_SIZE, matched);          \
            ExFreePoolWithTag(D1, 'tmpb');                                      \
            ExFreePoolWithTag(D2, 'tmpb');                                      \
            break;                                                              \
        }
#else
#define DO_READ_VERIFY(CachePool,Storage,pBlock)
#endif

#ifdef WRITE_BACK_ENABLE
#undef DO_READ_VERIFY
#define DO_READ_VERIFY(CachePool,Storage,pBlock)
#endif

#ifdef WRITE_BACK_ENABLE
#if 1
  #define LOCK_WB_QUEUE(Lock)    KeAcquireSpinLock(Lock, &Irql)
  #define UNLOCK_WB_QUEUE(Lock)  KeReleaseSpinLock(Lock, Irql)
#else
  #define LOCK_WB_QUEUE(Lock)                     \
  {                                               \
      DbgPrint("%s: Lock WB\n", __FUNCTION__);    \
      KeAcquireSpinLock(Lock, &Irql);             \
  }
  #define UNLOCK_WB_QUEUE(Lock)                   \
  {                                               \
      DbgPrint("%s: Unlock WB\n", __FUNCTION__);  \
      KeReleaseSpinLock(Lock, Irql);              \
  }
#endif
#else
  #define LOCK_WB_QUEUE
  #define UNLOCK_WB_QUEUE
#endif

#ifdef WRITE_BACK_ENABLE
#define ADD_TO_WBQUEUE_SAFE(pBlock)                                         \
        {                                                                   \
            KIRQL Irql;                                                     \
            LOCK_WB_QUEUE(&CachePool->WbQueueLock);                         \
            ADD_TO_WBQUEUE_NOT_SAFE(pBlock);                                \
            UNLOCK_WB_QUEUE(&CachePool->WbQueueLock);                       \
        }
#define ADD_TO_WBQUEUE_NOT_SAFE(pBlock)                                     \
        {                                                                   \
            if (pBlock->Modified == FALSE)                                  \
            {                                                               \
                pBlock->Modified = TRUE;                                    \
                InsertTailList(&CachePool->WbList, &pBlock->ListEntry);     \
            }                                                               \
        }
#else
#define ADD_TO_WBQUEUE_SAFE(pBlock)                                         \
        {                                                                   \
            pBlock->Modified = TRUE;                                        \
        }
#define ADD_TO_WBQUEUE_NOT_SAFE(pBlock)                                     \
        {                                                                   \
            pBlock->Modified = TRUE;                                        \
        }
#endif
