#include "minifilter.h"

static PFLT_FILTER gFilterHandle;
ULONG gTraceFlags = PTDBG_TRACE_ROUTINES;

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
                  ("minifilter1!NPInstanceQueryTeardown: Entered\n") );
    return STATUS_SUCCESS;
}

VOID
NPInstanceTeardownStart (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOLUME_CONTEXT context = NULL;
    UNREFERENCED_PARAMETER( Flags );
    PAGED_CODE();
    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("minifilter1!NPInstanceTeardownStart: Entered\n") );
    status = FltGetVolumeContext(FltObjects->Filter, FltObjects->Volume, &context);
    if( NT_SUCCESS(status) && context->Name.Buffer != NULL)
        DbgPrint("[%wZ]\n", &context->Name);
    FltReleaseContext( context );
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
                  ("minifilter1!NPInstanceTeardownComplete: Entered\n") );
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
                  ("minifilter1!DriverEntry: Entered\n") );
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
                  ("NPminifilter!NPUnload: Entered: Unregister Filter...\n") );

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}
