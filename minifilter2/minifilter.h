/**
 *   Add Read operation
 */
#include <fltKernel.h>
#include <ntddscsi.h>

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    ( FlagOn(gTraceFlags,(_dbgLevel) ) ?            \
        DbgPrint _string : ((void)0))

extern ULONG gTraceFlags;

extern NPAGED_LOOKASIDE_LIST Pre2PostContextList;

#define CONTEXT_TAG                    'xcBS'
#define BUFFER_SWAP_TAG                'bdBS'
#define PRE_2_POST_TAG                 'ppBS'
#define MIN_SECTOR_SIZE                0x200
/*************************************************************************
    Prototypes
*************************************************************************/
#if 1
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

FLT_PREOP_CALLBACK_STATUS
NPPreRead (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
NPPostRead (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

VOID
NPCleanupVolumeContext(
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    );
#endif
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
        #pragma alloc_text(PAGE, NPPreRead)
        #pragma alloc_text(PAGE, NPPostRead)
        #pragma alloc_text(PAGE, NPCleanupVolumeContext)
#endif

//  This is a volume context
//  one of these are attached to each volume we monitor.
typedef struct _VOLUME_CONTEXT
{
    // Holds the name to display
    UNICODE_STRING Name;
    // Holds the sector size for this volume
    ULONG SectorSize;
} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

//
//  This is a lookAside list used to allocate our pre-2-post structure.
//
typedef struct _PRE_2_POST_CONTEXT
{
    //  Pointer to our volume context structure.  We always get the context
    //  in the preOperation path because you can not safely get it at DPC
    //  level.  We then release it in the postOperation path.  It is safe
    //  to release contexts at DPC level.
    PVOLUME_CONTEXT VolContext;

    // Since post-operation parameters always receive the
    // original parameters passed to the operation, we need to
    // pass our buffer to our post-operation routine so we can free it.
    PVOID SwappedBuffer;

} PRE_2_POST_CONTEXT, *PPRE_2_POST_CONTEXT;

//  Context definitions we currently care about.
extern CONST FLT_CONTEXT_REGISTRATION ContextNotifications[];

//  operation registration
extern const FLT_OPERATION_REGISTRATION Callbacks[];

//  This defines what we want to filter with FltMgr
extern const FLT_REGISTRATION FilterRegistration;
