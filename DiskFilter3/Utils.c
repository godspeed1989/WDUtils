#include "Utils.h"
#include "DiskFilter.h"
#include "md5.h"
#include <ntstrsafe.h>

IO_COMPLETION_ROUTINE ForwardIrpCompletion;
static NTSTATUS
ForwardIrpCompletion (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    if (Irp->PendingReturned == TRUE)
    {
        KeSetEvent ((PKEVENT) Context, IO_NO_INCREMENT, FALSE);
    }
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS ForwardIrpSynchronously (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    KEVENT      event;
    NTSTATUS    status;

    IoCopyCurrentIrpStackLocationToNext(Irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine (Irp,
                            ForwardIrpCompletion,
                            &event,
                            TRUE, TRUE, TRUE);
    status = IoCallDriver(DeviceObject, Irp);

    if (status == STATUS_PENDING)
    {
        KeWaitForSingleObject (&event,
                                Executive,// WaitReason
                                KernelMode,// must be Kernelmode to prevent the stack getting paged out
                                FALSE,
                                NULL// indefinite wait
                                );
        status = Irp->IoStatus.Status;
    }
    return status;
}

IO_COMPLETION_ROUTINE RWRequestCompletion;
NTSTATUS RWRequestCompletion (PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    PMDL mdl;
    UNREFERENCED_PARAMETER(DeviceObject);

    if (Context)
        KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
    if(Irp->AssociatedIrp.SystemBuffer && (Irp->Flags & IRP_DEALLOCATE_BUFFER))
    {
        ExFreePool(Irp->AssociatedIrp.SystemBuffer);
    }
    while (Irp->MdlAddress)
    {
        mdl = Irp->MdlAddress;
        Irp->MdlAddress = mdl->Next;
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
    }
    IoFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS IoDoRWRequestAsync (
        ULONG           MajorFunction,
        PDEVICE_OBJECT  DeviceObject,
        PVOID           Buffer,
        ULONG           Length,
        PLARGE_INTEGER  StartingOffset
    )
{
    PIRP            Irp = NULL;
    IO_STATUS_BLOCK iosb;

    Irp = IoBuildAsynchronousFsdRequest (
        MajorFunction,
        DeviceObject,
        Buffer,
        Length,
        StartingOffset,
        &iosb
    );
    if (NULL == Irp)
    {
        KdPrint(("%s: Build IRP failed!\n", __FUNCTION__));
        return STATUS_UNSUCCESSFUL;
    }
    IoSetCompletionRoutine(Irp, RWRequestCompletion, NULL, TRUE, TRUE, TRUE);
    Irp->UserIosb = NULL;

    IoCallDriver(DeviceObject, Irp);
    return STATUS_SUCCESS;
}

NTSTATUS IoDoRWRequestSync (
        ULONG           MajorFunction,
        PDEVICE_OBJECT  DeviceObject,
        PVOID           Buffer,
        ULONG           Length,
        PLARGE_INTEGER  StartingOffset
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PIRP            Irp = NULL;
    IO_STATUS_BLOCK iosb;
    KEVENT          Event;

    Irp = IoBuildAsynchronousFsdRequest (
        MajorFunction,
        DeviceObject,
        Buffer,
        Length,
        StartingOffset,
        &iosb
    );
    if (NULL == Irp)
    {
        KdPrint(("%s: Build IRP failed!\n", __FUNCTION__));
        return STATUS_UNSUCCESSFUL;
    }

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(Irp, RWRequestCompletion, &Event, TRUE, TRUE, TRUE);
    if (IoCallDriver(DeviceObject, Irp) == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    }
    if (!NT_SUCCESS(Irp->IoStatus.Status))
    {
        KdPrint(("%s: %x Forward IRP failed!\n", __FUNCTION__, Irp->IoStatus.Status));
        Status = STATUS_UNSUCCESSFUL;
    }
    return Status;
}

NTSTATUS IoDoIoctl (
        ULONG           IoControlCode,
        PDEVICE_OBJECT  DeviceObject,
        PVOID           InputBuffer,
        ULONG           InputBufferLength,
        PVOID           OutputBuffer,
        ULONG           OutputBufferLength
    )
{
    PIRP                Irp;
    KEVENT              Event;
    IO_STATUS_BLOCK     ios;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    Irp = IoBuildDeviceIoControlRequest (
        IoControlCode,
        DeviceObject,
        InputBuffer,
        InputBufferLength,
        OutputBuffer,
        OutputBufferLength,
        FALSE,
        &Event,
        &ios
    );
    if (NULL == Irp)
    {
        KdPrint(("%s: Build IRP failed!\n", __FUNCTION__));
        return STATUS_UNSUCCESSFUL;
    }
    if (IoCallDriver(DeviceObject, Irp) == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        if (!NT_SUCCESS(Irp->IoStatus.Status))
        {
            KdPrint(("%s: %x Forward IRP failed!\n", __FUNCTION__, Irp->IoStatus.Status));
            return STATUS_UNSUCCESSFUL;
        }
    }
    return STATUS_SUCCESS;
}

static NTSTATUS
DF_CreateSystemThread (
        PKSTART_ROUTINE         StartRoutine,
        PDF_DEVICE_EXTENSION    DevExt,
        PVOID                   *ThreadObject,
        PBOOLEAN                TerminalThread
    )
{
    NTSTATUS            Status;
    HANDLE              hThread;
    DBG_PRINT(DBG_TRACE_ROUTINES, (": %s: Enter\n", __FUNCTION__));

    Status = PsCreateSystemThread (
        &hThread,
        (ULONG)0, NULL, NULL, NULL,
        StartRoutine,
        (PVOID)DevExt
    );

    if (NT_SUCCESS(Status))
    {
        // Reference thread object.
        Status = ObReferenceObjectByHandle(
            hThread,
            THREAD_ALL_ACCESS,
            NULL,
            KernelMode,
            ThreadObject,
            NULL
        );
        if (NT_SUCCESS(Status))
        {
            ZwClose(hThread);
            return STATUS_SUCCESS;
        }
    }
    // Terminate thread
    KdPrint(("%s Failed\n", __FUNCTION__));
    *ThreadObject = NULL;
    *TerminalThread = TRUE;
    ZwClose(hThread);
    return STATUS_UNSUCCESSFUL;
}

VOID StartDevice(PDEVICE_OBJECT DeviceObject)
{
    PDF_DEVICE_EXTENSION    DevExt;
    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    DevExt->bIsStart = FALSE;

    if (!NT_SUCCESS(DF_QueryDeviceInfo(DeviceObject)))
        return;

    DevExt->RwThreadObject = NULL;
    DevExt->bTerminalRwThread = FALSE;
    InitializeListHead(&DevExt->RwList);
    KeInitializeSpinLock(&DevExt->RwListSpinLock);
    KeInitializeEvent(&DevExt->RwThreadStartEvent, SynchronizationEvent, FALSE);
    KeInitializeEvent(&DevExt->RwThreadFinishEvent, SynchronizationEvent, FALSE);
    if (NT_SUCCESS( DF_CreateSystemThread(DF_ReadWriteThread, DevExt,
                    &DevExt->RwThreadObject, &DevExt->bTerminalRwThread) ))
        KdPrint((": %p RW Thread Start\n", DeviceObject));
    else
        return;
#ifdef WRITE_BACK_ENABLE
    DevExt->WbThreadObject = NULL;
    DevExt->bTerminalWbThread = FALSE;
    DevExt->CachePool.WbFlushAll = FALSE;
    ZeroMemory(&DevExt->CachePool.WbQueue, sizeof(Queue));
    KeInitializeSpinLock(&DevExt->CachePool.WbQueueSpinLock);
    KeInitializeEvent(&DevExt->CachePool.WbThreadStartEvent, SynchronizationEvent, FALSE);
    KeInitializeEvent(&DevExt->CachePool.WbThreadFinishEvent, SynchronizationEvent, FALSE);
    if (NT_SUCCESS( DF_CreateSystemThread(DF_WriteBackThread, DevExt,
                    &DevExt->WbThreadObject, &DevExt->bTerminalWbThread) ))
        KdPrint((": %p WB Thread Start\n", DeviceObject));
    else
        return;
#endif
    KdPrint(("\n"));

    DevExt->bIsStart = TRUE;
}

VOID StopDevice(PDEVICE_OBJECT DeviceObject)
{
    PDF_DEVICE_EXTENSION    DevExt;
    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    if (DevExt->LowerDeviceObject)
    {
        IoDetachDevice(DevExt->LowerDeviceObject);
    }
    if (DevExt->RwThreadObject)
    {
        DevExt->bTerminalRwThread = TRUE;
        while (FALSE == IsListEmpty(&DevExt->RwList))
        {
            KeSetEvent(&DevExt->RwThreadStartEvent, IO_NO_INCREMENT, FALSE);
            KeWaitForSingleObject(&DevExt->RwThreadFinishEvent, Executive, KernelMode, FALSE, NULL);
        }
        ObDereferenceObject(DevExt->RwThreadObject);
    }
#ifdef WRITE_BACK_ENABLE
    if (DevExt->WbThreadObject)
    {
        DevExt->bTerminalWbThread = TRUE;
        while (DevExt->CachePool.WbQueue.Used)
        {
            KeSetEvent(&DevExt->CachePool.WbThreadStartEvent, IO_NO_INCREMENT, FALSE);
            KeWaitForSingleObject(&DevExt->CachePool.WbThreadFinishEvent, Executive, KernelMode, FALSE, NULL);
        }
        ObDereferenceObject(DevExt->WbThreadObject);
    }
#endif
    if (DevExt->bIsProtected == TRUE)
        DestroyCachePool(&DevExt->CachePool);
    DevExt->bIsStart = FALSE;
    DevExt->bIsProtected = FALSE;
    IoDeleteDevice(DeviceObject);
}

NTSTATUS DF_QueryDeviceInfo(PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS                Status;
    PDF_DEVICE_EXTENSION    DevExt;
    PARTITION_INFORMATION   PartitionInfo;
    VOLUME_DISK_EXTENTS     VolumeDiskExt;
#define FAT16_SIG_OFFSET    54
#define FAT32_SIG_OFFSET    82
#define NTFS_SIG_OFFSET     3
#define DBR_LENGTH          512
    // File system signature
    const UCHAR FAT16FLG[4] = {'F','A','T','1'};
    const UCHAR FAT32FLG[4] = {'F','A','T','3'};
    const UCHAR NTFSFLG[4] = {'N','T','F','S'};
    UCHAR DBR[DBR_LENGTH] = {0};
    LARGE_INTEGER readOffset = {0};
    PDP_NTFS_BOOT_SECTOR pNtfsBootSector = (PDP_NTFS_BOOT_SECTOR)DBR;
    PDP_FAT32_BOOT_SECTOR pFat32BootSector = (PDP_FAT32_BOOT_SECTOR)DBR;
    PDP_FAT16_BOOT_SECTOR pFat16BootSector = (PDP_FAT16_BOOT_SECTOR)DBR;

    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    KdPrint((": %s: %p Enter\n", __FUNCTION__, DeviceObject));

    // Get Partition Length
    Status = IoDoIoctl (
        IOCTL_DISK_GET_PARTITION_INFO,
        DevExt->LowerDeviceObject,
        NULL,
        0,
        &PartitionInfo,
        sizeof(PARTITION_INFORMATION)
    );
    if (!NT_SUCCESS(Status))
    {
        KdPrint(("Get Partition Length failed\n"));
        return Status;
    }
    DevExt->TotalSize = PartitionInfo.PartitionLength;
    DevExt->PartitionNumber = PartitionInfo.PartitionNumber;

    // Get Disk Number
    Status = IoDoIoctl (
        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
        DevExt->LowerDeviceObject,
        NULL,
        0,
        &VolumeDiskExt,
        sizeof(VOLUME_DISK_EXTENTS)
    );
    if (!NT_SUCCESS(Status))
    {
        KdPrint(("Get Disk Number failed\n"));
        return Status;
    }
    DevExt->DiskNumber = VolumeDiskExt.Extents[0].DiskNumber;

    // Read DBR
    Status = IoDoRWRequestSync (
        IRP_MJ_READ,
        DevExt->LowerDeviceObject,
        DBR,
        DBR_LENGTH,
        &readOffset
    );
    if (!NT_SUCCESS(Status))
    {
        KdPrint(("Read DBR failed\n"));
        return Status;
    }

    // Distinguish the file system.
    if (*(ULONG32*)NTFSFLG == *(ULONG32*)&DBR[NTFS_SIG_OFFSET])
    {
        KdPrint((": Current file system is NTFS\n"));
        DevExt->SectorSize = pNtfsBootSector->BytesPerSector;
        DevExt->ClusterSize = DevExt->SectorSize * pNtfsBootSector->SectorsPerCluster;
    }
    else if (*(ULONG32*)FAT32FLG == *(ULONG32*)&DBR[FAT32_SIG_OFFSET])
    {
        KdPrint((": Current file system is FAT32\n"));
        DevExt->SectorSize = pFat32BootSector->BytesPerSector;
        DevExt->ClusterSize = DevExt->SectorSize * pFat32BootSector->SectorsPerCluster;
    }
    else if (*(ULONG32*)FAT16FLG == *(ULONG32*)&DBR[FAT16_SIG_OFFSET])
    {
        KdPrint((": Current file system is FAT16\n"));
        DevExt->SectorSize = pFat16BootSector->BytesPerSector;
        DevExt->ClusterSize = DevExt->SectorSize * pFat16BootSector->SectorsPerCluster;
    }
    else
    {
        KdPrint((": File system can't be recongnized\n"));
        DevExt->SectorSize = 512;
        DevExt->ClusterSize = 512 * 63;
    }
    KdPrint((": %u-%u Sector = %u, Cluster = %u, Total = %I64d\n",
        DevExt->DiskNumber, DevExt->PartitionNumber,
        DevExt->SectorSize, DevExt->ClusterSize, DevExt->TotalSize.QuadPart));

    return STATUS_SUCCESS;
}

NTSTATUS
DF_GetDiskDeviceObjectPointer(
    ULONG           DiskIndex,
    ULONG           PartitionIndex,
    PFILE_OBJECT    *FileObject,
    PDEVICE_OBJECT  *DeviceObject
    )
{
    NTSTATUS                    status;
    CHAR                        SourceString[64] = "";
    STRING                      astr;
    UNICODE_STRING              ustr;

    RtlStringCbPrintfA(SourceString, 64, "\\Device\\Harddisk%d\\Partition%d", DiskIndex, PartitionIndex);
    RtlInitAnsiString(&astr, SourceString);
    RtlAnsiStringToUnicodeString(&ustr, &astr, TRUE);

    status = IoGetDeviceObjectPointer(&ustr, FILE_READ_ATTRIBUTES, FileObject, DeviceObject);

    RtlFreeUnicodeString(&ustr);
    return status;
}

VOID
DF_CalMD5(PVOID buf, ULONG len, UCHAR digest[16])
{
    md5_state_t state;
    md5_byte_t* _digest;

    _digest = digest;
    md5_init(&state);
    md5_append(&state, buf, len);
    md5_finish(&state, _digest);
}
