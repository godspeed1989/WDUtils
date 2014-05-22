//********************************************************************
//	created:	22:7:2008   23:03
//	file:		pci.arbiter.h
//	author:		tiamo
//	purpose:	arbiter
//********************************************************************

#pragma once

//
// this indicates that the alternative can coexist with shared resources and should be added to the range lists shared
//
#define ARBITER_ALTERNATIVE_FLAG_SHARED					0x00000001

//
// this indicates that the request if for a specific range with no alternatives. ie (End - Start + 1 == Length) eg port 60-60 L1 A1
//
#define ARBITER_ALTERNATIVE_FLAG_FIXED					0x00000002

//
// this indicates that request is invalid
//
#define ARBITER_ALTERNATIVE_FLAG_INVALID				0x00000004

//
// retest
//
#define ARBITER_STATE_FLAG_RETEST						0x0001

//
// boot allocation
//
#define ARBITER_STATE_FLAG_BOOT							0x0002

//
// conflict
//
#define ARBITER_STATE_FLAG_CONFLICT						0x0004

//
// null conflict ok
//
#define ARBITER_STATE_FLAG_NULL_CONFLICT_OK				0x0008

//
// boot allocated
//
#define ARBITER_RANGE_BOOT_ALLOCATED					0x01

//
// alias
//
#define ARBITER_RANGE_ALIAS								0x10

//
// positive decode
//
#define ARBITER_RANGE_POSITIVE_DECODE					0x20

//
// normal prioritiy
//
#define ARBITER_PRIORITY_NULL							0

//
// preferred reserved
//
#define ARBITER_PRIORITY_PREFERRED_RESERVED				(MAXLONG - 2)

//
// reserved
//
#define ARBITER_PRIORITY_RESERVED						(MAXLONG - 1)

//
// exhausted
//
#define ARBITER_PRIORITY_EXHAUSTED						(MAXLONG)

//
// This aligns address to the previously correctly aligned value
//
#define ALIGN_ADDRESS_DOWN(address,alignment)			((address) & ~((ULONGLONG)alignment - 1))

//
// This aligns address to the next correctly aligned value
//
#define ALIGN_ADDRESS_UP(address,alignment)				(ALIGN_ADDRESS_DOWN( (address + alignment - 1), alignment))

//
// lock primitives that leave us at PASSIVE_LEVEL after acquiring the lock.
// (A FAST_MUTEX or CriticalRegion leave us at APC level and some people (ACPI) need to be at passive level in their arbiter)
//
#define ArbAcquireArbiterLock(_Arbiter)					KeWaitForSingleObject((_Arbiter)->MutexEvent,Executive,KernelMode,FALSE,0)

//
// unlock primitives
//
#define ArbReleaseArbiterLock(_Arbiter)					KeSetEvent((_Arbiter)->MutexEvent,0,FALSE)

//
// control macro (used like a for loop) which iterates over all entries in a standard doubly linked list.
// head is the list head and the entries are of type Type.
// a member called ListEntry is assumed to be the LIST_ENTRY structure linking the entries together.
// current contains a pointer to each entry in turn.
//
#define FOR_ALL_IN_LIST(T,H,C)							\
	for((C) = CONTAINING_RECORD((H)->Flink,T,ListEntry); (H) != &(C)->ListEntry; (C) = CONTAINING_RECORD((C)->ListEntry.Flink,T,ListEntry))

//
// similar to the above only iteration is over an array of length _Size.
//
#define FOR_ALL_IN_ARRAY(A,S,C)							for((C) = (A); (C) < (A) + (S); (C) ++)

//
// as above only iteration begins with the entry _Current
//
#define FOR_REST_IN_ARRAY(A,S,C)						for( ;(C) < (A) + (S); (C) ++ )

//
// iteration over a range list
//
#define FOR_ALL_RANGES(L,I,R)							for(RtlGetFirstRange(L,I,&R); R; RtlGetNextRange(I,&R,TRUE))

//
// Determines if the ranges s1-e1 and s2-e2 intersect
//
#define INTERSECT(s1,e1,s2,e2)							(!(((s1) < (s2) && (e1) < (s2)) || ((s2) < (s1) && (e2) < (s1))))

//
// Returns the size of the intersection of s1-e1 and s2-e2, undefined if they don't intersect
//
#define INTERSECT_SIZE(s1,e1,s2,e2)						(min((e1),(e2)) - max((s1),(s2)) + 1)

//
// get ordering array index from priority
//
#define ORDERING_INDEX_FROM_PRIORITY(P)                 ((ULONG) ( (P) > 0 ? (P) - 1 : ((P) * -1) - 1))

//
// ordering list item
//
typedef struct _ARBITER_ORDERING
{
	//
	// start
	//
    ULONGLONG											Start;

	//
	// end
	//
    ULONGLONG											End;

}ARBITER_ORDERING,*PARBITER_ORDERING;

//
// ordering list
//
typedef struct _ARBITER_ORDERING_LIST
{
    //
    // the number of valid entries in the array
    //
    USHORT												Count;

    //
    // the maximum number of entries that can fit in the Ordering buffer
    //
    USHORT												Maximum;

    //
    // array of orderings
    //
    PARBITER_ORDERING									Orderings;

}ARBITER_ORDERING_LIST,*PARBITER_ORDERING_LIST;

//
// alternative
//
typedef struct _ARBITER_ALTERNATIVE
{
    //
    // the minimum acceptable start value from the requirement descriptor
    //
    ULONGLONG											Minimum;

    //
    // the maximum acceptable end value from the requirement descriptor
    //
    ULONGLONG											Maximum;

    //
    // the length from the requirement descriptor
    //
    ULONG												Length;

    //
    // the alignment from the requirement descriptor
    //
    ULONG												Alignment;

    //
    // priority index
    //
    LONG												Priority;

    //
    // flags - ARBITER_ALTERNATIVE_FLAG_SHARED - indicates the current requirement was for a shared resource.
    //         ARBITER_ALTERNATIVE_FLAG_FIXED - indicates the current requirement is for a specific resource (eg ports 220-230 and nothing else)
    //
    ULONG												Flags;

    //
    // descriptor - the descriptor describing this alternative
    //
    PIO_RESOURCE_DESCRIPTOR								Descriptor;

    //
    // padding
    //
    ULONG												Reserved[3];

}ARBITER_ALTERNATIVE,*PARBITER_ALTERNATIVE;

//
// state
//
typedef struct _ARBITER_ALLOCATION_STATE
{
    //
    // the current value being considered as a possible start value
    //
    ULONGLONG											Start;

    //
    // the current value being considered as a possible end value
    //
    ULONGLONG											End;

    //
    // the values currently being considered as the Minimum and Maximum
	// (this is different because the prefered orderings can restrict the ranges where we can allocate)
    //
    ULONGLONG											CurrentMinimum;

	//
	// maximum
	//
    ULONGLONG											CurrentMaximum;

    //
    // the entry in the arbitration list containing this request.
    //
    PARBITER_LIST_ENTRY									Entry;

    //
    // the alternative currently being considered
    //
    PARBITER_ALTERNATIVE								CurrentAlternative;

    //
    // the number of alternatives in the Alternatives array
    //
    ULONG												AlternativeCount;

    //
    // the arbiters representation of the alternatives being considered
    //
    PARBITER_ALTERNATIVE								Alternatives;

    //
    // Flags - ARBITER_STATE_FLAG_RETEST - indicates that we are in a retest operation not a test.
    //         ARBITER_STATE_FLAG_BOOT - indicates we are in a boot allocation operation not a test.
    //
    USHORT												Flags;

    //
    // rangeAttributes - these are logically ORed in to the attributes for all ranges added to the range list.
    //
    UCHAR												RangeAttributes;

    //
    // ranges that are to be considered available
    //
    UCHAR												RangeAvailableAttributes;

    //
    // space for the arbiter to use as it wishes
    //
    ULONG_PTR											WorkSpace;

}ARBITER_ALLOCATION_STATE,*PARBITER_ALLOCATION_STATE;

//
// forward declare
//
typedef struct _ARBITER_INSTANCE *PARBITER_INSTANCE;

//
// unpack requirement
//
typedef NTSTATUS (*PARBITER_UNPACK_REQUIREMENT)(__in PIO_RESOURCE_DESCRIPTOR Descriptor,__out PULONGLONG Min,__out PULONGLONG Max,__out PULONG Length,__out PULONG Align);

//
// pack resource
//
typedef NTSTATUS (*PARBITER_PACK_RESOURCE)(__in PIO_RESOURCE_DESCRIPTOR Requirement,__in ULONGLONG Start,__out PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor);

//
// unpack resource
//
typedef NTSTATUS (*PARBITER_UNPACK_RESOURCE)(__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor,__out PULONGLONG Start,__out PULONG Length);

//
// score requirement
//
typedef LONG (*PARBITER_SCORE_REQUIREMENT)(__in PIO_RESOURCE_DESCRIPTOR Descriptor);

//
// preprocess entry
//
typedef NTSTATUS (*PARBITER_PREPROCESS_ENTRY)(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE Entry);

//
// allocate entry
//
typedef NTSTATUS (*PARBITER_ALLOCATE_ENTRY)(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE Entry);

//
// test allocation
//
typedef NTSTATUS (*PARBITER_TEST_ALLOCATION)(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList);

//
// commit allocation
//
typedef NTSTATUS (*PARBITER_COMMIT_ALLOCATION)(__in PARBITER_INSTANCE Arbiter);

//
// rollback allocation
//
typedef NTSTATUS (*PARBITER_ROLLBACK_ALLOCATION)(__in PARBITER_INSTANCE Arbiter);

//
// retest allocation
//
typedef NTSTATUS (*PARBITER_RETEST_ALLOCATION)(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList);

//
// boot allocation
//
typedef NTSTATUS (*PARBITER_BOOT_ALLOCATION)(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList);

//
// add reserved
//
typedef NTSTATUS (*PARBITER_ADD_RESERVED)(__in PARBITER_INSTANCE Arbiter,__in_opt PIO_RESOURCE_DESCRIPTOR Requirement,__in_opt PCM_PARTIAL_RESOURCE_DESCRIPTOR Resource);

//
// get next allocation range
//
typedef BOOLEAN (*PARBITER_GET_NEXT_ALLOCATION_RANGE)(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// find suitable range
//
typedef BOOLEAN (*PARBITER_FIND_SUITABLE_RANGE)(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// add allocation
//
typedef VOID (*PARBITER_ADD_ALLOCATION)(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// backtrack allocation
//
typedef VOID (*PARBITER_BACKTRACK_ALLOCATION)(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// override conflict
//
typedef BOOLEAN (*PARBITER_OVERRIDE_CONFLICT)(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// query arbitrate
//
typedef NTSTATUS (*PARBITER_QUERY_ARBITRATE)(__in PARBITER_INSTANCE Arbiter,__in PLIST_ENTRY ArbitrationList);

//
// query conflict
//
typedef NTSTATUS (*PARBITER_QUERY_CONFLICT)(__in PARBITER_INSTANCE Arbiter,__in PDEVICE_OBJECT PhysicalDeviceObject,
											__in PIO_RESOURCE_DESCRIPTOR ConflictingResource,__out PULONG ConflictCount,__out PARBITER_CONFLICT_INFO *Conflicts);

//
// start arbiter
//
typedef NTSTATUS (*PARBITER_START_ARBITER)(__in PARBITER_INSTANCE Arbiter,__in PCM_RESOURCE_LIST StartResources);

//
// translate allocation order
//
typedef NTSTATUS (*PARBITER_TRANSLATE_ALLOCATION_ORDER)(__out PIO_RESOURCE_DESCRIPTOR TranslatedDescriptor,__in PIO_RESOURCE_DESCRIPTOR RawDescriptor);

//
// arbiter instance
//
typedef struct _ARBITER_INSTANCE
{
    //
    // signature - must be 'sbrA'
    //
    ULONG												Signature;

    //
    // synchronisation lock
    //
    PKEVENT												MutexEvent;

    //
    // The name of this arbiter - used for debugging and registry storage
    //
    PWSTR Name;

    //
    // the resource type this arbiter arbitrates.
    //
    CM_RESOURCE_TYPE									ResourceType;

    //
    // pointer to a pool allocated range list which contains the current allocation
    //
    PRTL_RANGE_LIST										Allocation;

    //
    // pointer to a pool allocated range list which contains the allocation under considetation.
	// this is set by test allocation.
    //
    PRTL_RANGE_LIST										PossibleAllocation;

    //
    // the order in which these resources should be allocated.
	// taken from the HKLM\System\CurrentControlSet\Control\SystemResources\AssignmentOrdering key and modified based on the reserved resources.
    //
    ARBITER_ORDERING_LIST								OrderingList;

    //
    // the resources that should be reserved (not allocated until absolutley necessary)
    //
    ARBITER_ORDERING_LIST								ReservedList;

    //
    // the reference count of the number of entities that are using the ARBITER_INTERFACE associated with this instance.
    //
    LONG												ReferenceCount;

    //
    // the ARBITER_INTERFACE associated with this instance.
    //
    PARBITER_INTERFACE									Interface;

    //
    // the size in bytes of the currently allocated AllocationStack
    //
    ULONG												AllocationStackMaxSize;

    //
    // a pointer to an array of ARBITER_ALLOCATION_STATE entries encapsulating the state of the current arbitration
    //
    PARBITER_ALLOCATION_STATE							AllocationStack;

    //
	// unpack requirement
	//
    PARBITER_UNPACK_REQUIREMENT							UnpackRequirement;

	//
	// pack resource
	//
    PARBITER_PACK_RESOURCE								PackResource;

	//
	// unpack resource
	//
    PARBITER_UNPACK_RESOURCE							UnpackResource;

	//
	// score requirement
	//
    PARBITER_SCORE_REQUIREMENT							ScoreRequirement;

	//
	// test allocation
	//
    PARBITER_TEST_ALLOCATION							TestAllocation;

	//
	// retest
	//
    PARBITER_RETEST_ALLOCATION							RetestAllocation;

	//
	// commit
	//
    PARBITER_COMMIT_ALLOCATION							CommitAllocation;

	//
	// rollback
	//
    PARBITER_ROLLBACK_ALLOCATION						RollbackAllocation;

	//
	// boot allocation
	//
    PARBITER_BOOT_ALLOCATION							BootAllocation;

	//
	// query arbitrate
	//
    PARBITER_QUERY_ARBITRATE							QueryArbitrate;

	//
	// query conflict
	//
    PARBITER_QUERY_CONFLICT								QueryConflict;

	//
	// add reserved
	//
    PARBITER_ADD_RESERVED								AddReserved;

	//
	// start arbiter
	//
    PARBITER_START_ARBITER								StartArbiter;

    //
	// preprocess entry
	//
    PARBITER_PREPROCESS_ENTRY							PreprocessEntry;

	//
	// allocate entry
	//
    PARBITER_ALLOCATE_ENTRY								AllocateEntry;

	//
	// get next allocation range
	//
    PARBITER_GET_NEXT_ALLOCATION_RANGE					GetNextAllocationRange;

	//
	// find suitable range
	//
    PARBITER_FIND_SUITABLE_RANGE						FindSuitableRange;

	//
	// add allocation
	//
    PARBITER_ADD_ALLOCATION								AddAllocation;

	//
	// backtrack allocation
	//
    PARBITER_BACKTRACK_ALLOCATION						BacktrackAllocation;

	//
	// override conflict
	//
    PARBITER_OVERRIDE_CONFLICT							OverrideConflict;

    //
    // debugging support
    //
    BOOLEAN												TransactionInProgress;

    //
    // arbiter specific extension - can be used to store extra arbiter specific information
    //
    PVOID												Extension;

    //
    // the bus device we arbitrate for
    //
    PDEVICE_OBJECT										BusDeviceObject;

    //
    // callback and context for RtlFindRange/RtlIsRangeAvailable to allow complex conflicts
    //
    PVOID												ConflictCallbackContext;

	//
	// callback
	//
    PRTL_CONFLICT_RANGE_CALLBACK						ConflictCallback;

}ARBITER_INSTANCE, *PARBITER_INSTANCE;

//
// initialize ordering list
//
NTSTATUS ArbInitializeOrderingList(__inout PARBITER_ORDERING_LIST List);

//
// free ordering list
//
VOID ArbFreeOrderingList(__inout PARBITER_ORDERING_LIST List);

//
// copy ordering list
//
NTSTATUS ArbCopyOrderingList(__out PARBITER_ORDERING_LIST Destination,__in PARBITER_ORDERING_LIST Source);

//
// add ordering
//
NTSTATUS ArbAddOrdering(__out PARBITER_ORDERING_LIST List,__in ULONGLONG Start,__in ULONGLONG End);

//
// prune ordering
//
NTSTATUS ArbPruneOrdering(__inout PARBITER_ORDERING_LIST OrderingList,__in ULONGLONG Start,__in ULONGLONG End);

//
// initialize arbiter instance
//
NTSTATUS ArbInitializeArbiterInstance(__out PARBITER_INSTANCE Arbiter,__in PDEVICE_OBJECT BusDevice,__in CM_RESOURCE_TYPE ResourceType,
									  __in PWSTR Name,__in PWSTR OrderingName,__in PARBITER_TRANSLATE_ALLOCATION_ORDER TranslateOrdering);

//
// delete arbiter instance
//
VOID ArbDeleteArbiterInstance(__in PARBITER_INSTANCE Arbiter);

//
// arbiter handler
//
NTSTATUS ArbArbiterHandler(__in PVOID Context,__in ARBITER_ACTION Action,__inout PARBITER_PARAMETERS Params);

//
// test allocation
//
NTSTATUS ArbTestAllocation(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList);

//
// retest allocation
//
NTSTATUS ArbRetestAllocation(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList);

//
// commit allocation
//
NTSTATUS ArbCommitAllocation(__in PARBITER_INSTANCE Arbiter);

//
// rollback allocation
//
NTSTATUS ArbRollbackAllocation(__in PARBITER_INSTANCE Arbiter);

//
// add reserved
//
NTSTATUS ArbAddReserved(__in PARBITER_INSTANCE Arbiter,__in_opt PIO_RESOURCE_DESCRIPTOR Requirement,__in_opt PCM_PARTIAL_RESOURCE_DESCRIPTOR Resource);

//
// preprocess entry
//
NTSTATUS ArbPreprocessEntry(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// allocate entry
//
NTSTATUS ArbAllocateEntry(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// sort arbitration list
//
NTSTATUS ArbSortArbitrationList(__inout PLIST_ENTRY ArbitrationList);

//
// confirm allocation
//
VOID ArbConfirmAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// override conflict
//
BOOLEAN ArbOverrideConflict(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// query conflict
//
NTSTATUS ArbQueryConflict(__in PARBITER_INSTANCE Arbiter,__in PDEVICE_OBJECT PhysicalDeviceObject,
						  __in PIO_RESOURCE_DESCRIPTOR ConflictingResource,__out PULONG ConflictCount,__out PARBITER_CONFLICT_INFO *Conflicts);

//
// backtrack allocation
//
VOID ArbBacktrackAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// get next allocation range
//
BOOLEAN ArbGetNextAllocationRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// find suitable range
//
BOOLEAN ArbFindSuitableRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// add allocation
//
VOID ArbAddAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// boot allocation
//
NTSTATUS ArbBootAllocation(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList);

//
// start arbiter
//
NTSTATUS ArbStartArbiter(__in PARBITER_INSTANCE Arbiter,__in PCM_RESOURCE_LIST StartResources);

//
// build assignment ordering
//
NTSTATUS ArbBuildAssignmentOrdering(__inout PARBITER_INSTANCE Arbiter,__in PWSTR AllocationOrderName,
									__in PWSTR ReservedResourcesName,__in_opt PARBITER_TRANSLATE_ALLOCATION_ORDER Translate);

//
// build allocation stack
//
NTSTATUS ArbpBuildAllocationStack(__in PARBITER_INSTANCE Arbiter,__in PLIST_ENTRY ArbitrationList,__in ULONG ArbitrationListCount);

//
// read registry value
//
NTSTATUS ArbpGetRegistryValue(__in HANDLE KeyHandle,__in PWSTR  ValueName,__out PKEY_VALUE_FULL_INFORMATION *Information);

//
// build alternative
//
NTSTATUS ArbpBuildAlternative(__in PARBITER_INSTANCE Arbiter,__in PIO_RESOURCE_DESCRIPTOR Requirement,__out PARBITER_ALTERNATIVE Alternative);

//
// update priority
//
VOID ArbpUpdatePriority(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALTERNATIVE Alternative);

//
// query conflict callback
//
BOOLEAN ArbpQueryConflictCallback(__in PVOID Context,__in PRTL_RANGE Range);

//
// share driver exclusive
//
BOOLEAN ArbShareDriverExclusive(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

#if DBG
//
	// indent
	//
	VOID ArbpIndent(__in ULONG Count);

	//
	// dump arbiter
	//
	VOID ArbpDumpArbiterInstance(__in LONG Level,__in PARBITER_INSTANCE Arbiter);

	//
	// dump arbiter range
	//
	VOID ArbpDumpArbiterRange(__in LONG Level,__in PRTL_RANGE_LIST List,__in PUCHAR RangeText);

	//
	// dump arbitration list
	//
	VOID ArbpDumpArbitrationList(__in LONG Level,__in PLIST_ENTRY ArbitrationList);

	//
	// debug level
	//
	extern LONG ArbDebugLevel;

	//
	// debug print
	//
	#define ARB_PRINT(Level,Message)					{if (Level <= ArbDebugLevel) DbgPrint Message;}

	//
	// indent
	//
	#define ARB_INDENT(Level,Count)						{if (Level < ArbDebugLevel) ArbpIndent(Count);}

	//
	// action string
	//
	extern PCHAR ArbpActionStrings[];

	//
	// debug break on error
	//
	extern ULONG ArbStopOnError;

	//
	// replay on error
	//
	extern ULONG ArbReplayOnError;

#else
	//
	// debug print
	//
	#define ARB_PRINT(Level,Message)					__noop

	//
	// indent
	//
	#define ARB_INDENT(Level,Count)						__noop
#endif