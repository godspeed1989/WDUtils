#include "Storage.h"

BOOLEAN
InitStoragePool(PSTORAGE_POOL StoragePool, ULONG Size)
{
	PULONG Bitmap_Buffer;

	StoragePool->Used = 0;
	StoragePool->Size = Size;
	StoragePool->HintIndex = 0;

	Bitmap_Buffer = ExAllocatePoolWithTag (
		NonPagedPool,
		(ULONG)(((Size/8+1)/sizeof(ULONG) + 1)* sizeof(ULONG)),
		STORAGE_POOL_TAG
	);
	RtlInitializeBitMap(
		&StoragePool->Bitmap,
		(PULONG)(Bitmap_Buffer),
		(ULONG)(Size)
	);
	RtlClearAllBits(&StoragePool->Bitmap);

#ifdef USE_DRAM
	StoragePool->Buffer = ExAllocatePoolWithTag(NonPagedPool,
							(SIZE_T)(BLOCK_SIZE*Size), STORAGE_POOL_TAG);
	return (StoragePool->Buffer != NULL);
#endif
}

VOID
DestroyStoragePool(PSTORAGE_POOL StoragePool)
{
	StoragePool->Used = 0;
	StoragePool->Size = 0;
	StoragePool->HintIndex = 0;
#ifdef USE_DRAM
	if (StoragePool->Buffer)
		ExFreePoolWithTag(StoragePool->Buffer, STORAGE_POOL_TAG);
	StoragePool->Buffer = NULL;
#endif
	ExFreePoolWithTag(StoragePool->Bitmap.Buffer, STORAGE_POOL_TAG);
}

ULONG
StoragePoolAlloc(PSTORAGE_POOL StoragePool)
{
	ULONG Index;
	Index = -1;
#ifdef USE_DRAM
	Index = RtlFindClearBitsAndSet (
			&StoragePool->Bitmap,
			1, //NumberToFind
			StoragePool->HintIndex
		);
	if (Index != -1)
		StoragePool->Used++;
	StoragePool->HintIndex = (Index == -1)?0:Index;
#endif
	return Index;
}

VOID
StoragePoolFree(PSTORAGE_POOL StoragePool, ULONG Index)
{
	if (RtlCheckBit(&StoragePool->Bitmap, Index))
	{
		StoragePool->Used--;
		RtlClearBit(&StoragePool->Bitmap, Index);
	}
}

VOID
StoragePoolWrite(PSTORAGE_POOL StoragePool, ULONG Index, ULONG Offset, PVOID Data, ULONG Len)
{
#ifdef USE_DRAM
	PUCHAR Buffer;
	Buffer = StoragePool->Buffer;
	Buffer += Index * BLOCK_SIZE + Offset;
	RtlCopyMemory(Buffer, Data, Len);
#endif
}

VOID
StoragePoolRead(PSTORAGE_POOL StoragePool, PVOID Data, ULONG Index, ULONG Offset, ULONG Len)
{
#ifdef USE_DRAM
	PUCHAR Buffer;
	Buffer = StoragePool->Buffer;
	Buffer += Index * BLOCK_SIZE + Offset;
	RtlCopyMemory(Data, Buffer, Len);
#endif
}
