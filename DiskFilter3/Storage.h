#pragma once
#include "common.h"
#include "DiskFilterIoctl.h"

#ifdef USE_DRAM
  #define BLOCK_RESERVE                   (0)
#else
  #define BLOCK_RESERVE                   (16<<20)
#endif

typedef struct _STORAGE_POOL
{
    ULONG           Size;
    ULONG           Used;
    LONGLONG        TotalSize;
    RTL_BITMAP      Bitmap;
    PULONG          Bitmap_Buffer;
#ifdef USE_DRAM
    PUCHAR          Buffer;
#else
    PDEVICE_OBJECT  BlockDevice;
#endif
    // Opaque
    ULONG           HintIndex;
}STORAGE_POOL, *PSTORAGE_POOL;

BOOLEAN
InitStoragePool (PSTORAGE_POOL StoragePool, ULONG Size
                 #ifndef USE_DRAM
                     ,ULONG DiskNum
                     ,ULONG PartitionNum
                 #endif
                );

VOID
DestroyStoragePool(PSTORAGE_POOL StoragePool);

ULONG
StoragePoolAlloc(PSTORAGE_POOL StoragePool);

VOID
StoragePoolFree(PSTORAGE_POOL StoragePool, ULONG Index);

VOID
StoragePoolWrite(PSTORAGE_POOL StoragePool, ULONG StartIndex, ULONG Offset, PVOID Data, ULONG Len);

VOID
StoragePoolRead(PSTORAGE_POOL StoragePool, PVOID Data, ULONG StartIndex, ULONG Offset, ULONG Len);
