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
                  ("minifilter1!NPInstanceSetup: Entered\n") );
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

        DbgPrint("minifilter1!NPInstanceSetup: [%wZ 0x%04x/0x%04x]\n",
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
                 ("minifilter1!NPCleanupVolumeContext: Entered\n") );
    ASSERT(ContextType == FLT_VOLUME_CONTEXT);
    if(context->Name.Buffer != NULL)
    {
		DbgPrint("minifilter1!NPCleanupVolumeContext: [%wZ]\n", &context->Name);
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
                     ("minifilter1!NPPretCreate: create [%wZ]\n", &nameInfo->Name) );
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
                     ("minifilter1!NPPostCreate: failed\n") );
    }
    return FLT_POSTOP_FINISHED_PROCESSING;
}
