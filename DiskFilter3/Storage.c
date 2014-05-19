#include "Storage.h"
#include "Utils.h"
#include <Ntdddisk.h>

BOOLEAN
InitStoragePool (PSTORAGE_POOL StoragePool, ULONG Size
                 #ifndef USE_DRAM
                     ,ULONG DiskNum
                     ,ULONG PartitionNum
                 #endif
                )
{
#ifndef USE_DRAM
    NTSTATUS                Status;
    PFILE_OBJECT            FileObject;
    PARTITION_INFORMATION   PartitionInfo;
#endif
    RtlZeroMemory(StoragePool, sizeof(STORAGE_POOL));
    StoragePool->Size = Size;
    StoragePool->TotalSize = BLOCK_SIZE * Size + BLOCK_RESERVE;

    StoragePool->Bitmap_Buffer = ExAllocatePoolWithTag (
        NonPagedPool,
        (ULONG)(((Size/8+1)/sizeof(ULONG) + 1)* sizeof(ULONG)),
        STORAGE_POOL_TAG
    );
    if (StoragePool->Bitmap_Buffer == NULL)
        return FALSE;
    RtlInitializeBitMap (
        &StoragePool->Bitmap,
        (PULONG)(StoragePool->Bitmap_Buffer),
        (ULONG)(Size)
    );
    RtlClearAllBits(&StoragePool->Bitmap);

#ifdef USE_DRAM
    StoragePool->Buffer = (PUCHAR)ExAllocatePoolWithTag(PagedPool,
                            (SIZE_T)(StoragePool->TotalSize), STORAGE_POOL_TAG);
    if (StoragePool->Buffer == NULL)
        goto l_error;
#else
    Status = DF_GetDiskDeviceObjectPointer(DiskNum, PartitionNum, &FileObject, &StoragePool->BlockDevice);
    if (!NT_SUCCESS(Status))
        goto l_error;
    ObfDereferenceObject(FileObject);
    // Get Partition Length
    Status = IoDoIoctl (
        IOCTL_DISK_GET_PARTITION_INFO,
        StoragePool->BlockDevice,
        NULL,
        0,
        &PartitionInfo,
        sizeof(PARTITION_INFORMATION)
    );
    if (!NT_SUCCESS(Status))
        goto l_error;
    if (StoragePool->TotalSize > PartitionInfo.PartitionLength.QuadPart)
        goto l_error;
#endif

    return TRUE;
l_error:
    if (StoragePool->Bitmap_Buffer)
        ExFreePoolWithTag(StoragePool->Bitmap_Buffer, STORAGE_POOL_TAG);
#ifdef USE_DRAM
    if (StoragePool->Buffer)
        ExFreePoolWithTag(StoragePool->Buffer, STORAGE_POOL_TAG);
#endif
    RtlZeroMemory(StoragePool, sizeof(STORAGE_POOL));
    return FALSE;
}

VOID
DestroyStoragePool(PSTORAGE_POOL StoragePool)
{
    StoragePool->Used = 0;
    StoragePool->Size = 0;
    StoragePool->TotalSize = 0;
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
    Index = RtlFindClearBitsAndSet (
            &StoragePool->Bitmap,
            1, //NumberToFind
            StoragePool->HintIndex
        );
    if (Index != -1)
        StoragePool->Used++;
    StoragePool->HintIndex = (Index == -1)?0:Index;
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
StoragePoolWrite(PSTORAGE_POOL StoragePool, ULONG StartIndex, ULONG Offset, PVOID Data, ULONG Len)
{
    LARGE_INTEGER   writeOffset;
    writeOffset.QuadPart = StartIndex * BLOCK_SIZE + Offset;
    writeOffset.QuadPart += BLOCK_RESERVE;
    ASSERT ((writeOffset.QuadPart + Len) <= (StoragePool->TotalSize));
#ifdef USE_DRAM
    RtlCopyMemory(StoragePool->Buffer + writeOffset.QuadPart, Data, Len);
#else
    ASSERT (NT_SUCCESS(IoDoRWRequestSync(
        IRP_MJ_WRITE,
        StoragePool->BlockDevice,
        Data,
        Len,
        &writeOffset,
        2
    )));
#endif
}

VOID
StoragePoolRead(PSTORAGE_POOL StoragePool, PVOID Data, ULONG StartIndex, ULONG Offset, ULONG Len)
{
    LARGE_INTEGER   readOffset;
    readOffset.QuadPart = StartIndex * BLOCK_SIZE + Offset;
    readOffset.QuadPart += BLOCK_RESERVE;
    ASSERT ((readOffset.QuadPart + Len) <= (StoragePool->TotalSize));
#ifdef USE_DRAM
    RtlCopyMemory(Data, StoragePool->Buffer + readOffset.QuadPart, Len);
#else
    ASSERT (NT_SUCCESS(IoDoRWRequestSync(
        IRP_MJ_READ,
        StoragePool->BlockDevice,
        Data,
        Len,
        &readOffset,
        2
    )));
#endif
}
