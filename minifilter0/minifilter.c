#include "minifilter.h"

PFLT_FILTER gFilterHandle;
ULONG gTraceFlags = 2;

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    ( FlagOn(gTraceFlags,(_dbgLevel) ) ?            \
        DbgPrint _string : ((void)0))

NTSTATUS
NPInstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    PAGED_CODE();
    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("minifilter0!NPInstanceSetup: Entered\n") );
    return STATUS_SUCCESS;
}

NTSTATUS
NPInstanceQueryTeardown (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();
    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("minifilter0!NPInstanceQueryTeardown: Entered\n") );
    return STATUS_SUCCESS;
}

VOID
NPInstanceTeardownStart (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();
    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("minifilter0!NPInstanceTeardownStart: Entered\n") );
}

VOID
NPInstanceTeardownComplete (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();
    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("minifilter0!NPInstanceTeardownComplete: Entered\n") );
}

/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/
NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("minifilter0!DriverEntry: Entered\n") );
    //
    //  Register with FltMgr to tell it our callback routines
    //
    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    ASSERT( NT_SUCCESS( status ) );
    if (NT_SUCCESS( status ))
    {
        //
        //  Start filtering i/o
        //
        status = FltStartFiltering( gFilterHandle );
        if (!NT_SUCCESS( status ))
            FltUnregisterFilter( gFilterHandle );
    }
    return status;
}

NTSTATUS
NPUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();
    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("NPminifilter!NPUnload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
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
        if ( NT_SUCCESS( status ) )
        {
            FltParseFileNameInformation( nameInfo );
            PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                     ("minifilter0!NPPretCreate: create [%wZ]\n", nameInfo->Name) );
            FltReleaseFileNameInformation( nameInfo );
        }
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
    if (!NT_SUCCESS( Data->IoStatus.Status ) ||
        (STATUS_REPARSE == Data->IoStatus.Status))
    {
        PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                     ("minifilter0!NPPostCreate: failed\n") );
    }
    return FLT_POSTOP_FINISHED_PROCESSING;
}
