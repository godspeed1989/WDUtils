#include "DiskFilter.h"
#include "Utils.h"
#include "DiskFilterIoctl.h"
#include "Queue.h"

NTSTATUS DF_CreateRWThread(PDF_DEVICE_EXTENSION DevExt);

NTSTATUS
DF_DispatchDefault(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDF_DEVICE_EXTENSION    DevExt;
    PAGED_CODE();

    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

NTSTATUS
DF_DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDF_DEVICE_EXTENSION    DevExt;
    PIO_STACK_LOCATION      IrpSp;
    PAGED_CODE();

    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    DBG_PRINT(DBG_TRACE_ROUTINES, ("%s: ", __FUNCTION__));
    if (IrpSp->Parameters.Power.Type == SystemPowerState)
    {
        DBG_PRINT(DBG_TRACE_OPS, ("SystemPowerState...\n"));
        if (PowerSystemShutdown == IrpSp->Parameters.Power.State.SystemState)
        {
            DBG_PRINT(DBG_TRACE_OPS, ("%d-%d: Stopping Device...\n", DevExt->DiskNumber, DevExt->PartitionNumber));
            StopDevice(DeviceObject);
            DBG_PRINT(DBG_TRACE_OPS, ("%d-%d: Device Stopped.\n", DevExt->DiskNumber, DevExt->PartitionNumber));
        }
    }
    else if (IrpSp->Parameters.Power.Type == DevicePowerState)
        DBG_PRINT(DBG_TRACE_OPS, ("DevicePowerState...\n"));
    else
        DBG_PRINT(DBG_TRACE_OPS, ("\n"));

#if WINVER < _WIN32_WINNT_VISTA
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(DevExt->LowerDeviceObject, Irp);
#else
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DevExt->LowerDeviceObject, Irp);
#endif
}

NTSTATUS
DF_DispatchIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS                Status;
    PVOID                   InputBuffer;
    ULONG                   InputLength;
    PVOID                   OutputBuffer;
    ULONG                   OutputLength;
    PIO_STACK_LOCATION      IrpSp;
    PDF_DEVICE_EXTENSION    DevExt;
    BOOLEAN                 Type;
    PAGED_CODE();

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    if (DeviceObject == g_pDeviceObject)
    {
        InputBuffer = Irp->AssociatedIrp.SystemBuffer;
        InputLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
        OutputBuffer = Irp->AssociatedIrp.SystemBuffer;
        OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

        switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
        {
        // Test IOCTL
        case IOCTL_DF_TEST:
            DBG_PRINT(DBG_TRACE_OPS, ("%s: Test Ioctl\n", __FUNCTION__));
            if (InputLength >= 2*sizeof(ULONG32) && OutputLength >= sizeof(ULONG32))
            {
                *(ULONG32*)OutputBuffer = ((ULONG32*)InputBuffer)[0] + ((ULONG32*)InputBuffer)[1];
                Irp->IoStatus.Information = sizeof(ULONG32);
            }
            else
                Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Irp->IoStatus.Status;
        // Start or Stop One Filter
        case IOCTL_DF_START:
        case IOCTL_DF_STOP:
            Type = (IOCTL_DF_START == IrpSp->Parameters.DeviceIoControl.IoControlCode);
            DBG_PRINT(DBG_TRACE_OPS, ("%s: %s One Filter\n", __FUNCTION__, Type?"Start":"Stop"));
            DeviceObject = g_pDriverObject->DeviceObject;
            Status = STATUS_UNSUCCESSFUL;
            while (DeviceObject != NULL)
            {
                DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
                if (DeviceObject != g_pDeviceObject &&
                    DevExt->bIsStart == TRUE &&
                    DevExt->DiskNumber == ((ULONG32*)InputBuffer)[0] &&
                    DevExt->PartitionNumber == ((ULONG32*)InputBuffer)[1])
                {
                    DBG_PRINT(DBG_TRACE_OPS, ("%s Filter on disk(%u) partition(%u)\n", Type?"Start":"Stop",
                        ((ULONG32*)InputBuffer)[0], ((ULONG32*)InputBuffer)[1]));
                #ifndef USE_DRAM
                    DBG_PRINT(DBG_TRACE_OPS, ("Use disk(%u) partition(%u) as Cache\n",
                        ((ULONG32*)InputBuffer)[2], ((ULONG32*)InputBuffer)[3]));
                #endif
                    Status = STATUS_SUCCESS;
                    if (Type == FALSE)
                    {
                        DevExt->bIsProtected = FALSE;
                        // Wait for unfinished Ops in RW Thread
                        while (FALSE == IsListEmpty(&DevExt->RwList))
                        {
                            KeSetEvent(&DevExt->RwThreadStartEvent, IO_NO_INCREMENT, FALSE);
                            KeWaitForSingleObject(&DevExt->RwThreadFinishEvent, Executive, KernelMode, FALSE, NULL);
                        }
                        KeClearEvent(&DevExt->RwThreadStartEvent);
                        KeClearEvent(&DevExt->RwThreadFinishEvent);
                    #ifdef WRITE_BACK_ENABLE
                        // Flush Back All Data
                        DevExt->CachePool.WbFlushAll = TRUE;
                        while (DevExt->CachePool.WbQueue.Used)
                        {
                            KeSetEvent(&DevExt->CachePool.WbThreadStartEvent, IO_NO_INCREMENT, FALSE);
                            KeWaitForSingleObject(&DevExt->CachePool.WbThreadFinishEvent,
                                                  Executive, KernelMode, FALSE, NULL);
                        }
                        DevExt->CachePool.WbFlushAll = FALSE;
                    #endif
                        DevExt->ReadCount = 0;
                        DevExt->WriteCount = 0;
                    #ifdef PROFILE
                        DevExt->CachePool.NumQuery = 0;
                        DevExt->CachePool.SumQueryTickCount = 0;
                        DevExt->CachePool.NumRWUpdate = 0;
                        DevExt->CachePool.SumRWUpdateTickCount = 0;
                    #endif
                        DevExt->CachePool.Size = 0;
                        DevExt->CachePool.Used = 0;
                        DevExt->CachePool.ReadHit = 0;
                        DevExt->CachePool.WriteHit = 0;
                        DestroyCachePool(&DevExt->CachePool);
                    }
                    else if (DevExt->bIsProtected == FALSE)
                    {
                        if (InitCachePool(&DevExt->CachePool
                        #ifndef USE_DRAM
                            ,((ULONG32*)InputBuffer)[2] ,((ULONG32*)InputBuffer)[3]
                        #endif
                            ) == TRUE
                        #ifdef WRITE_BACK_ENABLE
                            && InitQueue(&DevExt->CachePool.WbQueue, WB_QUEUE_NUM_BLOCKS) == TRUE
                        #endif
                        )
                            DevExt->bIsProtected = TRUE;
                        else
                        {
                        #ifdef WRITE_BACK_ENABLE
                            DestroyQueue(&DevExt->CachePool.WbQueue);
                        #endif
                            DestroyCachePool(&DevExt->CachePool);
                            DevExt->bIsProtected = FALSE;
                            KdPrint(("%s: %d-%d: Init Cache Pool Error\n", __FUNCTION__,
                                        DevExt->DiskNumber, DevExt->PartitionNumber));
                            Status = STATUS_UNSUCCESSFUL;
                        }
                    }
                    break;
                }
                DeviceObject = DeviceObject->NextDevice;
            }
            COMPLETE_IRP(Irp, Status);
            return Irp->IoStatus.Status;
        // Get or Clear Statistic
        case IOCTL_DF_GET_STAT:
        case IOCTL_DF_CLEAR_STAT:
            Type = (IOCTL_DF_GET_STAT == IrpSp->Parameters.DeviceIoControl.IoControlCode);
            DBG_PRINT(DBG_TRACE_OPS, ("%s: %s Statistic\n", __FUNCTION__, Type?"Get":"Clear"));
            DeviceObject = g_pDriverObject->DeviceObject;
            Status = STATUS_UNSUCCESSFUL;
            while (DeviceObject != NULL)
            {
                DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
                if (DeviceObject != g_pDeviceObject &&
                    DevExt->bIsStart == TRUE &&
                    DevExt->DiskNumber == ((ULONG32*)InputBuffer)[0] &&
                    DevExt->PartitionNumber == ((ULONG32*)InputBuffer)[1])
                {
                    DBG_PRINT(DBG_TRACE_OPS, ("On disk(%u) partition(%u)\n", DevExt->DiskNumber, DevExt->PartitionNumber));
                    if (Type && OutputLength >= 6 * sizeof(ULONG32))
                    {
                    #ifdef PROFILE
                        ULONG AverQueryuSec = 0;
                        ULONG AverUpdateuSec = 0;
                    #endif
                        ((ULONG32*)OutputBuffer)[0] = DevExt->CachePool.ReadHit;
                        ((ULONG32*)OutputBuffer)[1] = DevExt->CachePool.WriteHit;
                        ((ULONG32*)OutputBuffer)[2] = DevExt->ReadCount;
                        ((ULONG32*)OutputBuffer)[3] = DevExt->WriteCount;
                        ((ULONG32*)OutputBuffer)[4] = DevExt->CachePool.Size;
                        ((ULONG32*)OutputBuffer)[5] = DevExt->CachePool.Used;
                    #ifdef PROFILE
                        if (DevExt->CachePool.NumQuery)
                        {
                            DBG_PRINT(DBG_TRACE_OPS, ("NumQuery: %d %d\n",
                                DevExt->CachePool.NumQuery, DevExt->CachePool.SumQueryTickCount));
                            AverQueryuSec = DevExt->CachePool.SumQueryTickCount / DevExt->CachePool.NumQuery;
                        }
                        if (DevExt->CachePool.NumRWUpdate)
                        {
                            DBG_PRINT(DBG_TRACE_OPS, ("NumUpdate: %d %d\n",
                                DevExt->CachePool.NumRWUpdate, DevExt->CachePool.SumRWUpdateTickCount));
                            AverUpdateuSec = DevExt->CachePool.SumRWUpdateTickCount / DevExt->CachePool.NumRWUpdate;
                        }
                        ((ULONG32*)OutputBuffer)[6] = (ULONG32)AverQueryuSec;
                        ((ULONG32*)OutputBuffer)[7] = (ULONG32)AverUpdateuSec;
                    #endif
                        Irp->IoStatus.Information = 8 * sizeof(ULONG32);
                    }
                    else
                    {
                        DevExt->CachePool.ReadHit = 0;
                        DevExt->CachePool.WriteHit = 0;
                        DevExt->CachePool.NumQuery = 0;
                        DevExt->CachePool.SumQueryTickCount = 0;
                        DevExt->CachePool.NumRWUpdate = 0;
                        DevExt->CachePool.SumRWUpdateTickCount = 0;
                        DevExt->ReadCount = 0;
                        DevExt->WriteCount = 0;
                        Irp->IoStatus.Information = 0;
                    }
                    Status = STATUS_SUCCESS;
                    break;
                }
                DeviceObject = DeviceObject->NextDevice;
            }
            Irp->IoStatus.Status = Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Irp->IoStatus.Status;
        // Setup Output
        case IOCTL_DF_QUIET:
            DBG_PRINT(DBG_TRACE_OPS, ("%s: Quite All Output\n", __FUNCTION__));
            g_TraceFlags = 0;
            COMPLETE_IRP(Irp, STATUS_SUCCESS);
            return Irp->IoStatus.Status;
        case IOCTL_DF_VERBOSE:
            g_TraceFlags = -1;
            DBG_PRINT(DBG_TRACE_OPS, ("%s: Verbose All Output\n", __FUNCTION__));
            COMPLETE_IRP(Irp, STATUS_SUCCESS);
            return Irp->IoStatus.Status;
        // Setup Data Verify
        case IOCTL_DF_VERIFY:
            g_bDataVerify = (g_bDataVerify==TRUE)?FALSE:TRUE;
            DBG_PRINT(DBG_TRACE_OPS, ("%s: %s Data Verify\n", __FUNCTION__, g_bDataVerify?"Start":"Stop"));
            COMPLETE_IRP(Irp, STATUS_SUCCESS);
            return Irp->IoStatus.Status;
        // Unknown Ioctl
        default:
            DBG_PRINT(DBG_TRACE_OPS, ("%s: Unknown User Ioctl\n", __FUNCTION__));
            COMPLETE_IRP(Irp, STATUS_UNSUCCESSFUL);
            return Irp->IoStatus.Status;
        }
    }
    else
    {
        switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
        {
        case IOCTL_VOLUME_ONLINE:
            DBG_PRINT(DBG_TRACE_OPS, ("%s: IOCTL_VOLUME_ONLINE\n", __FUNCTION__));
            if (IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp))
            {
                StartDevice(DeviceObject);
            }
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Irp->IoStatus.Status;
        case IOCTL_VOLUME_OFFLINE:
            DBG_PRINT(DBG_TRACE_OPS, ("%s: IOCTL_VOLUME_OFFLINE\n", __FUNCTION__));
            // ... Flush back Cache
            break;
        case IOCTL_DISK_COPY_DATA:
            DBG_PRINT(DBG_TRACE_OPS, ("%s: IOCTL_DISK_COPY_DATA\n", __FUNCTION__));
            COMPLETE_IRP(Irp, STATUS_UNSUCCESSFUL);
            return Irp->IoStatus.Status;
        default:
            //DBG_PRINT(DBG_TRACE_OPS, ("%s: 0x%X\n", __FUNCTION__, IrpSp->Parameters.DeviceIoControl.IoControlCode));
            break;
        }
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

NTSTATUS
DF_DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS                status;
    PDF_DEVICE_EXTENSION    DevExt;
    PIO_STACK_LOCATION      IrpSp;
    // For handling paging requests.
    BOOLEAN                 setPageable;
    BOOLEAN                 bAddPageFile;
    PAGED_CODE();

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    switch(IrpSp->MinorFunction)
    {
    case IRP_MN_START_DEVICE:
        DBG_PRINT(DBG_TRACE_OPS, ("%s: Start Device...\n", __FUNCTION__));
        status = Irp->IoStatus.Status;
        DevExt->CurrentPnpState = IRP_MN_START_DEVICE;
        break;
    case IRP_MN_DEVICE_USAGE_NOTIFICATION :
        setPageable = FALSE;
        bAddPageFile = IrpSp->Parameters.UsageNotification.InPath;
        DBG_PRINT(DBG_TRACE_OPS, ("%s: Paging file request...\n", __FUNCTION__));
        if (IrpSp->Parameters.UsageNotification.Type == DeviceUsageTypePaging)
        //  Indicated it will create or delete a paging file.
        {
            if(bAddPageFile && !DevExt->CurrentPnpState)
            {
                status = STATUS_DEVICE_NOT_READY;
                break;
            }

            //  Waiting other paging requests.
            KeWaitForSingleObject(&DevExt->PagingCountEvent,
                Executive, KernelMode,
                FALSE, NULL);

            //  Removing last paging device.
            if (!bAddPageFile && DevExt->PagingCount == 1 )
            {
                // The last paging file is no longer active.
                // Set the DO_POWER_PAGABLE bit before
                // forwarding the paging request down the
                // stack.
                if (!(DeviceObject->Flags & DO_POWER_INRUSH))
                {
                    DeviceObject->Flags |= DO_POWER_PAGABLE;
                    setPageable = TRUE;
                }
            }
            //  Waiting lower device complete.
            IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);

            if (NT_SUCCESS(Irp->IoStatus.Status))
            {
                IoAdjustPagingPathCount(&DevExt->PagingCount, bAddPageFile);
                if (bAddPageFile && DevExt->PagingCount == 1) {
                    //  Once the lower device objects have succeeded the addition of the paging
                    //  file, it is illegal to fail the request. It is also the time to clear
                    //  the Filter DO's DO_POWER_PAGABLE flag.
                    DeviceObject->Flags &= ~DO_POWER_PAGABLE;
                }
            }
            else
            {
                if (setPageable == TRUE) {
                    DeviceObject->Flags &= ~DO_POWER_PAGABLE;
                    setPageable = FALSE;
                }
            }
            KeSetEvent(&DevExt->PagingCountEvent, IO_NO_INCREMENT, FALSE);
            status = Irp->IoStatus.Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;
        }
        break;
    case IRP_MN_REMOVE_DEVICE:
        DBG_PRINT(DBG_TRACE_OPS, ("%s: Removing Device...\n", __FUNCTION__));
        IoForwardIrpSynchronously(DevExt->LowerDeviceObject, Irp);
        status = Irp->IoStatus.Status;
        if (NT_SUCCESS(status))
        {
            DBG_PRINT(DBG_TRACE_OPS, ("%d-%d: Stopping Device...\n", DevExt->DiskNumber, DevExt->PartitionNumber));
            StopDevice(DeviceObject);
            DBG_PRINT(DBG_TRACE_OPS, ("%d-%d: Device Stopped.\n", DevExt->DiskNumber, DevExt->PartitionNumber));
        }
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

NTSTATUS
DF_DispatchReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDF_DEVICE_EXTENSION    DevExt;

    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    if (DevExt->bIsProtected == TRUE)
    {
        IoMarkIrpPending(Irp);
        // Queue this IRP
        ExInterlockedInsertTailList(&DevExt->RwList, &Irp->Tail.Overlay.ListEntry,
                                    &DevExt->RwListSpinLock);
        // Set Event
        KeSetEvent(&DevExt->RwThreadStartEvent, IO_NO_INCREMENT, FALSE);
        return STATUS_PENDING;
    }
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DevExt->LowerDeviceObject, Irp);
}

NTSTATUS
DF_CtlDevDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PDF_DEVICE_EXTENSION    DevExt;
    PAGED_CODE();

    DevExt = (PDF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    if (DeviceObject == g_pDeviceObject)
    {
        COMPLETE_IRP(Irp, STATUS_SUCCESS);
    }
    else
    {
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(DevExt->LowerDeviceObject, Irp);
    }
    return STATUS_SUCCESS;
}
