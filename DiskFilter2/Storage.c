#include "Storage.h"

BOOLEAN
InitStoragePool(PSTORAGE_POOL StoragePool, ULONG Size)
{
	StoragePool->Used = 0;
	StoragePool->Size = Size;
	StoragePool->HintIndex = 0;

	StoragePool->Bitmap_Buffer = ExAllocatePoolWithTag (
		NonPagedPool,
		(ULONG)(((Size/8+1)/sizeof(ULONG) + 1)* sizeof(ULONG)),
		STORAGE_POOL_TAG
	);
	if (StoragePool->Bitmap_Buffer == NULL)
		return FALSE;
	RtlInitializeBitMap(
		&StoragePool->Bitmap,
		(PULONG)(StoragePool->Bitmap_Buffer),
		(ULONG)(Size)
	);
	RtlClearAllBits(&StoragePool->Bitmap);

#ifdef USE_DRAM
	StoragePool->Buffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool,
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
	if (StoragePool->Bitmap_Buffer)
		ExFreePoolWithTag(StoragePool->Bitmap_Buffer, STORAGE_POOL_TAG);
	StoragePool->Bitmap_Buffer = NULL;
#ifdef USE_DRAM
	if (StoragePool->Buffer)
		ExFreePoolWithTag(StoragePool->Buffer, STORAGE_POOL_TAG);
	StoragePool->Buffer = NULL;
#endif
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
	if ((Buffer + Len) <= (StoragePool->Buffer + StoragePool->Size*BLOCK_SIZE))
		RtlCopyMemory(Buffer, Data, Len);
	else
		KdPrint(("%s: Access Error\n", __FUNCTION__));
#endif
}

VOID
StoragePoolRead(PSTORAGE_POOL StoragePool, PVOID Data, ULONG Index, ULONG Offset, ULONG Len)
{
#ifdef USE_DRAM
	PUCHAR Buffer;
	Buffer = StoragePool->Buffer;
	Buffer += Index * BLOCK_SIZE + Offset;
	if ((Buffer + Len) <= (StoragePool->Buffer + StoragePool->Size*BLOCK_SIZE))
		RtlCopyMemory(Data, Buffer, Len);
	else
		KdPrint(("%s: Access Error", __FUNCTION__));
#endif
}
