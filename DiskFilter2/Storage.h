#pragma once
#include "common.h"

#define USE_DRAM

#define SECTOR_SIZE						512
#define NSB								1		/* Number Sectors per Block */
#define BLOCK_SIZE						(SECTOR_SIZE*NSB)

typedef struct _STORAGE_POOL
{
	ULONG			Size;
	ULONG			Used;
	RTL_BITMAP		Bitmap;
	PULONG			Bitmap_Buffer;
#ifdef USE_DRAM
	PUCHAR			Buffer;
#endif
	// Opaque
	ULONG			HintIndex;
}STORAGE_POOL, *PSTORAGE_POOL;

BOOLEAN
InitStoragePool(PSTORAGE_POOL StoragePool, ULONG Size);

VOID
DestroyStoragePool(PSTORAGE_POOL StoragePool);

ULONG
StoragePoolAlloc(PSTORAGE_POOL StoragePool);

VOID
StoragePoolFree(PSTORAGE_POOL StoragePool, ULONG Index);

VOID
StoragePoolWrite(PSTORAGE_POOL StoragePool, ULONG Index, ULONG Offset, PVOID Data, ULONG Len);

VOID
StoragePoolRead(PSTORAGE_POOL StoragePool, PVOID Data, ULONG Index, ULONG Offset, ULONG Len);
