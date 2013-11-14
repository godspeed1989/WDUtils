#include "minifilter.h"

NTSTATUS
NPInstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOLUME_CONTEXT context = NULL;
    ULONG retLen;
    UCHAR volPropBuffer[sizeof(FLT_VOLUME_PROPERTIES)+512];
    PFLT_VOLUME_PROPERTIES volProp = (PFLT_VOLUME_PROPERTIES)volPropBuffer;
    PDEVICE_OBJECT devObj = NULL;

    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );
    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("minifilter2!NPInstanceSetup: Entered\n") );
    try
    {
        // Allocate a volume context structure.
        status = FltAllocateContext( FltObjects->Filter,
                                     FLT_VOLUME_CONTEXT,
                                     sizeof(VOLUME_CONTEXT),
                                     NonPagedPool,
                                     &context );
        if( !NT_SUCCESS(status) )
            leave;
        // Get the volume properties, so I can get a sector size
        status = FltGetVolumeProperties( FltObjects->Volume,
                                         volProp,
                                         sizeof(volPropBuffer),
                                         &retLen );
        if( !NT_SUCCESS(status) )
            leave;
        // Get sector size
        ASSERT((volProp->SectorSize == 0) || (volProp->SectorSize >= MIN_SECTOR_SIZE));
        context->SectorSize = max(volProp->SectorSize, MIN_SECTOR_SIZE);

        // Get device object, so I can get disk name
        context->Name.Buffer = NULL;
        status = FltGetDiskDeviceObject( FltObjects->Volume, &devObj );
        if( !NT_SUCCESS(status) )
            leave;

        status = IoVolumeDeviceToDosName( devObj, &context->Name);
        if( !NT_SUCCESS(status) )
            leave;

        // Set the context
        status = FltSetVolumeContext( FltObjects->Volume,
                                      FLT_SET_CONTEXT_KEEP_IF_EXISTS,
                                      context,
                                      NULL );

        DbgPrint("minifilter2!NPInstanceSetup: [%wZ 0x%04x/0x%04x]\n",
                  &context->Name, context->SectorSize, volProp->SectorSize);
        DbgPrint("[%wZ - %wZ - %wZ]\n",
                &volProp->FileSystemDriverName,
                &volProp->FileSystemDeviceName,
                &volProp->RealDeviceName);
        if (status == STATUS_FLT_CONTEXT_ALREADY_DEFINED )
        {
            status = STATUS_SUCCESS;
        }
    }
    finally
    {
        //  Always release the context.  If the set failed, it will free the
        //  context.  If not, it will remove the reference added by the set.
        //  Note that the name buffer in the context will get freed by the
        //  NPCleanupVolumeContext() routine.
        if(context)
            FltReleaseContext( context );
        if(devObj)
            ObDereferenceObject( devObj );
    }

    return status;
}

VOID
NPCleanupVolumeContext(
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    )
{
    PVOLUME_CONTEXT context = Context;

    UNREFERENCED_PARAMETER( ContextType );
    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                 ("minifilter2!NPCleanupVolumeContext: Entered\n") );
    ASSERT(ContextType == FLT_VOLUME_CONTEXT);
    if(context->Name.Buffer != NULL)
    {
        DbgPrint("minifilter2!NPCleanupVolumeContext: [%wZ]\n", &context->Name);
        ExFreePool(context->Name.Buffer);
        context->Name.Buffer = NULL;
    }
}

/*************************************************************************
    MiniFilter CREATE callback routines.
*************************************************************************/

FLT_PREOP_CALLBACK_STATUS
NPPreCreate (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION nameInfo;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    PAGED_CODE();

    __try
    {
        status = FltGetFileNameInformation( Data,
                                            FLT_FILE_NAME_NORMALIZED |
                                            FLT_FILE_NAME_QUERY_DEFAULT,
                                            &nameInfo );
        if ( !NT_SUCCESS( status ) )
            leave;

        status = FltParseFileNameInformation( nameInfo );
        if ( !NT_SUCCESS( status ) )
            leave;
        PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                     ("minifilter2!NPPretCreate: create [%wZ]\n", &nameInfo->Name) );
        FltReleaseFileNameInformation( nameInfo );
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("NPPreCreate EXCEPTION_EXECUTE_HANDLER\n");
    }
    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
NPPostCreate (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in_opt PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );
    PAGED_CODE();

    //
    //  If this create was failing anyway, don't bother scanning now.
    //
    if (!NT_SUCCESS( Data->IoStatus.Status ))
    {
        PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                     ("minifilter2!NPPostCreate: failed\n") );
    }
    return FLT_POSTOP_FINISHED_PROCESSING;
}

//
//  This is a lookAside list used to allocate our pre-2-post structure.
//
NPAGED_LOOKASIDE_LIST Pre2PostContextList;

/*************************************************************************
    MiniFilter CREATE callback routines.
*************************************************************************/

FLT_PREOP_CALLBACK_STATUS
NPPreRead(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
{
    NTSTATUS status;
    FLT_PREOP_CALLBACK_STATUS FltStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
    ULONG readLen = iopb->Parameters.Read.Length;

    PVOID newBuf = NULL;
    PMDL newMdl  = NULL;
    PVOLUME_CONTEXT volContext = NULL;
    PPRE_2_POST_CONTEXT p2pContext = NULL;

    PAGED_CODE();
    try
    {
        if (readLen == 0)
            leave;

        // Fast I/O path, disallow it, this will lead to
        // an equivalent irp request coming in
        if (FLT_IS_FASTIO_OPERATION(Data))
        {
            FltStatus = FLT_PREOP_DISALLOW_FASTIO;
            leave;
        }
        // Cache I/O IRP path
        if (!(iopb->IrpFlags & (IRP_NOCACHE | IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO)))
            leave;

        // Get volume context
        status = FltGetVolumeContext( FltObjects->Filter, FltObjects->Volume, &volContext );
        if (!NT_SUCCESS(status))
            return FltStatus;
        // NOCACHE read length must sector size aligned
        if (FlagOn(IRP_NOCACHE, iopb->IrpFlags))
            readLen = (ULONG)ROUND_TO_SIZE(readLen, volContext->SectorSize);

        // Allocate new Buf and MDL
        newBuf = ExAllocatePoolWithTag(NonPagedPool, readLen, BUFFER_SWAP_TAG);
        if (newBuf == NULL)
            leave;
        // We only need to build a MDL for IRP operations
        if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION))
        {
            newMdl = IoAllocateMdl( newBuf, readLen, FALSE, FALSE, NULL );
            if (newMdl == NULL)
                leave;
            MmBuildMdlForNonPagedPool( newMdl );
        }

        // Update buffer pointers and MDL address
        iopb->Parameters.Read.ReadBuffer = newBuf;
        iopb->Parameters.Read.MdlAddress = newMdl;
        FltSetCallbackDataDirty( Data );

        // Get pre 2 post context structure
        p2pContext = ExAllocateFromNPagedLookasideList( &Pre2PostContextList );
        if (p2pContext == NULL)
            leave;
        p2pContext->VolContext = volContext;
        p2pContext->SwappedBuffer = newBuf;
        *CompletionContext = p2pContext;

        FltStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
    }
    finally
    {
        if(FltStatus != FLT_PREOP_SUCCESS_WITH_CALLBACK)
        {
            if (newBuf != NULL)
                ExFreePool( newBuf );
            if (newMdl != NULL)
                IoFreeMdl( newMdl );
            if (volContext != NULL )
                FltReleaseContext( volContext );
        }
    }
    return FltStatus;
}

static
FLT_POSTOP_CALLBACK_STATUS
PostReadWhenSafe (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

FLT_POSTOP_CALLBACK_STATUS
NPPostRead(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    FLT_POSTOP_CALLBACK_STATUS FltStatus = FLT_POSTOP_FINISHED_PROCESSING;
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
    PPRE_2_POST_CONTEXT p2pContext = CompletionContext;
    BOOLEAN cleanupAllocatedBuffer = TRUE;

    PVOID origBuf;
    //  This system won't draining an operation with swapped buffers
    ASSERT(!FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING));

    PAGED_CODE();
    try
    {
        // If the operation failed or the count is zero,
        // there is no data to copy so just return now.
        if (!NT_SUCCESS(Data->IoStatus.Status) ||  (Data->IoStatus.Information == 0))
            leave;
        // We need to copy the read data back into the users buffer.
        // The parameters passed in are for the users original buffers
        // not our swapped buffers
        if (iopb->Parameters.Read.MdlAddress != NULL)
        {
            origBuf = MmGetSystemAddressForMdlSafe( iopb->Parameters.Read.MdlAddress, NormalPagePriority );
            if (origBuf == NULL)
            {
                Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                Data->IoStatus.Information = 0;
                leave;
            }
        }
        else if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) ||
                 FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_FAST_IO_OPERATION))
        {
            origBuf = iopb->Parameters.Read.ReadBuffer;
        }
        else
        {
            //
            //  They don't have a MDL and this is not a system buffer
            //  or a fastio so this is probably some arbitrary user
            //  buffer.  We can not do the processing at DPC level so
            //  try and get to a safe IRQL so we can do the processing.
            //
            if (FltDoCompletionProcessingWhenSafe( Data,
                                                   FltObjects,
                                                   CompletionContext,
                                                   Flags,
                                                   PostReadWhenSafe,
                                                   &FltStatus ))
            {
                //
                //  This operation has been moved to a safe IRQL, the called
                //  routine will do (or has done) the freeing so don't do it
                //  in our routine.
                //
                cleanupAllocatedBuffer = FALSE;
            }
            else
            {
                //
                //  We are in a state where we can not get to a safe IRQL and
                //  we do not have a MDL.  There is nothing we can do to safely
                //  copy the data back to the users buffer, fail the operation
                //  and return.  This shouldn't ever happen because in those
                //  situations where it is not safe to post, we should have
                //  a MDL.
                Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
                Data->IoStatus.Information = 0;
            }
            leave;
        }

        try
        {
            Data->IoStatus.Information = iopb->Parameters.Read.Length;
            RtlCopyMemory( origBuf,
                           p2pContext->SwappedBuffer, // data need to be decrypted
                           Data->IoStatus.Information );
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            Data->IoStatus.Status = GetExceptionCode();
            Data->IoStatus.Information = 0;
        }
    }
    finally
    {
        if (cleanupAllocatedBuffer)
        {
            ExFreePool( p2pContext->SwappedBuffer );
            FltReleaseContext( p2pContext->VolContext );
            ExFreeToNPagedLookasideList( &Pre2PostContextList, p2pContext );
        }
    }

    return FltStatus;
}

static
FLT_POSTOP_CALLBACK_STATUS
PostReadWhenSafe (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
    NTSTATUS status;
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
    PPRE_2_POST_CONTEXT p2pContext = CompletionContext;
    PVOID origBuf;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    ASSERT(Data->IoStatus.Information != 0);

    status = FltLockUserBuffer( Data );

    if (!NT_SUCCESS(status))
    {
        Data->IoStatus.Status = status;
        Data->IoStatus.Information = 0;
    }
    else
    {
        //  Get a system address for this buffer.
        origBuf = MmGetSystemAddressForMdlSafe( iopb->Parameters.Read.MdlAddress,
                                                NormalPagePriority );
        if (origBuf == NULL)
        {
            Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            Data->IoStatus.Information = 0;
        }
        else
        {
            Data->IoStatus.Information = iopb->Parameters.Read.Length;
            try
            {
                RtlCopyMemory( origBuf,
                               p2pContext->SwappedBuffer, // data need to be decrypted
                               Data->IoStatus.Information );
            }
            except (EXCEPTION_EXECUTE_HANDLER)
            {
                Data->IoStatus.Status = GetExceptionCode();
                Data->IoStatus.Information = 0;
            }
        }
    }

    ExFreePool( p2pContext->SwappedBuffer );
    FltReleaseContext( p2pContext->VolContext );
    ExFreeToNPagedLookasideList( &Pre2PostContextList, p2pContext );

    return FLT_POSTOP_FINISHED_PROCESSING;
}
