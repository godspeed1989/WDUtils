#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <ntddscsi.h>

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

/*************************************************************************
    Prototypes
*************************************************************************/

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    );

NTSTATUS
NPInstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
NPInstanceTeardownStart (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
NPInstanceTeardownComplete (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
NPUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
NPInstanceQueryTeardown (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
NPPreCreate (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
NPPostCreate (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in_opt PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );


//  Assign text sections for each routine.
#ifdef ALLOC_PRAGMA
        #pragma alloc_text(INIT, DriverEntry)
        #pragma alloc_text(PAGE, NPUnload)
        #pragma alloc_text(PAGE, NPInstanceQueryTeardown)
        #pragma alloc_text(PAGE, NPInstanceSetup)
        #pragma alloc_text(PAGE, NPInstanceTeardownStart)
        #pragma alloc_text(PAGE, NPInstanceTeardownComplete)
        #pragma alloc_text(PAGE, NPPreCreate)
        #pragma alloc_text(PAGE, NPPostCreate)
#endif

//  operation registration
const FLT_OPERATION_REGISTRATION Callbacks[] =
{
    { IRP_MJ_CREATE,
      0,
      NPPreCreate,
      NPPostCreate
    },
    { IRP_MJ_OPERATION_END }
};

//  This defines what we want to filter with FltMgr
const FLT_REGISTRATION FilterRegistration =
{
    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    NPUnload,                           //  MiniFilterUnload

    NPInstanceSetup,                    //  InstanceSetup
    NPInstanceQueryTeardown,            //  InstanceQueryTeardown
    NPInstanceTeardownStart,            //  InstanceTeardownStart
    NPInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};
