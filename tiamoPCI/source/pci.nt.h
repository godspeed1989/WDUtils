//********************************************************************
//	created:	22:7:2008   16:29
//	file:		pci.nt.h
//	author:		tiamo
//	purpose:	ntoskrnl internal
//********************************************************************

#pragma once

//
// legacy device detection interface version
//
#define LEGACY_DEVICE_DETECTION_INTRF_STANDARD_VER		0

//
// PME interface version
//
#define PCI_PME_INTRF_STANDARD_VER						1

//
// native ide interface  version
//
#define PCI_NATIVE_IDE_INTRF_STANDARD_VER				1

//
// interrupt route interface version
//
#define INT_ROUTE_INTRF_STANDARD_VER					1

//
// carbus interface version
//
#define PCI_CARDBUS_INTRF_STANDARD_VER					1

//
// text is unicode encoding
//
#define MESSAGE_RESOURCE_UNICODE						0x0001

//
// add to range list if conflict
//
#define RTL_RANGE_LIST_ADD_IF_CONFLICT					1

//
// new range can be shared with others
//
#define RTL_RANGE_LIST_ADD_SHARED						2

//
// shared ok
//
#define RTL_RANGE_LIST_SHARED_OK						1

//
// null conflict ok
//
#define RTL_RANGE_LIST_NULL_CONFLICT_OK					2

//
// conflict range callback
//
typedef BOOLEAN (*PRTL_CONFLICT_RANGE_CALLBACK)(__in PVOID Context,__in struct _RTL_RANGE* Range);

//
// legacy device detection handler
//
typedef NTSTATUS (*PLEGACY_DEVICE_DETECTION_HANDLER)(__in PVOID Context,__in INTERFACE_TYPE LegacyBusType,__in ULONG Bus,__in ULONG Slot,__out PDEVICE_OBJECT *Pdo);

//
// get PME info
//
typedef VOID (*PPCI_PME_GET_PME_INFORMATION)(__in PVOID Context,__out_opt PBOOLEAN HasPowerMgrCaps,__out_opt PBOOLEAN PmeAsserted,__out_opt PBOOLEAN PmeEnabled);

//
// clear PME status
//
typedef VOID (*PPCI_PME_CLEAR_PME_STATUS)(__in PVOID Context);

//
// update PME enable
//
typedef VOID (*PPCI_PME_UPDATE_ENABLE)(__in PVOID Context,__in BOOLEAN Enable);

//
// native ide interrupt control
//
typedef VOID (*PNATIVEIDE_INTERRUPT_CONTROL)(__in PVOID Context,__in BOOLEAN Enable);

//
// get interrupt routing token
//
typedef NTSTATUS (*PGET_INTERRUPT_ROUTING)(__in PDEVICE_OBJECT Pdo,__out PULONG Bus,__out PULONG PciSlot,__out PUCHAR IntLine,__out PUCHAR IntPin,__out PUCHAR Class,
										   __out PUCHAR SubClass,__out PDEVICE_OBJECT* ParentPdo,__out struct _ROUTING_TOKEN* RoutingToken,__out PUCHAR Flags);

//
// set interrupt routing token
//
typedef NTSTATUS (*PSET_INTERRUPT_ROUTING_TOKEN)(__in PDEVICE_OBJECT Pdo,__in struct _ROUTING_TOKEN* RoutingToken);

//
// update interrupt line
//
typedef VOID (*PUPDATE_INTERRUPT_LINE)(__in PDEVICE_OBJECT Pdo,__in UCHAR LineRegister);

//
// add bus
//
typedef NTSTATUS (*PCARDBUS_ADD_BUS)(__in PDEVICE_OBJECT Pdo,__out PVOID* NewFdoExt);

//
// delete bus
//
typedef NTSTATUS (*PCARDBUS_DELETE_BUS)(__in PVOID FdoExt);

//
// dispatch pnp
//
typedef NTSTATUS (*PCARDBUS_DISPATCH_PNP)(__in PVOID FdoExt,__in PIRP Irp);

//
// get location
//
typedef NTSTATUS (*PCARDBUS_GET_LOCATION)(__in PDEVICE_OBJECT Pdo,__out PUCHAR Bus,__out PUCHAR Device,__out PUCHAR Function,__out PBOOLEAN OnDebugPath);

//
// get/set bus data
//
typedef ULONG (*PGETSETBUSDATA)(__in PBUS_HANDLER BusHandler,__in PBUS_HANDLER RootHandler,__in ULONG Slot,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// get interrupt vector
//
typedef ULONG (*PGETINTERRUPTVECTOR)(__in PBUS_HANDLER BusHandler,__in PBUS_HANDLER Root,__in ULONG Level,__in ULONG Vector,__out PKIRQL Irql,__out PKAFFINITY Affinity);

//
// translate bus address
//
typedef BOOLEAN (*PTRANSLATEBUSADDRESS)(__in PBUS_HANDLER BusHandler,__in PBUS_HANDLER RootHandler,__in PHYSICAL_ADDRESS BusAddress,
										__inout PULONG AddressSpace,__out PPHYSICAL_ADDRESS TranslatedAddress);

//
// adjust resource list
//
typedef NTSTATUS (*PADJUSTRESOURCELIST)(__in PBUS_HANDLER BusHandler,__in PBUS_HANDLER RootHandler,__inout PIO_RESOURCE_REQUIREMENTS_LIST* ResourceList);

//
// reference device handler
//
typedef PDEVICE_HANDLER_OBJECT (*PREFERENCE_DEVICE_HANDLER)(__in PBUS_HANDLER BusHandler,__in PBUS_HANDLER RootHandler,__in ULONG SlotNumber);

//
// assign slot resources
//
typedef NTSTATUS (*PASSIGNSLOTRESOURCES)(__in PBUS_HANDLER BusHandler,__in PBUS_HANDLER RootHandler,__in PUNICODE_STRING RegistryPath,
										 __in_opt PUNICODE_STRING DriverClassName,__in PDRIVER_OBJECT DriverObject,__in_opt PDEVICE_OBJECT DeviceObject,
										 __in ULONG SlotNumber,__inout PCM_RESOURCE_LIST* AllocatedResources);

//
// range list
//
typedef struct _RTL_RANGE_LIST
{
	//
	// list head
	//
	LIST_ENTRY											ListHead;

	//
	// flags
	//
	ULONG												Flags;

	//
	// count
	//
	ULONG												Count;

	//
	// stamp
	//
	ULONG												Stamp;

}RTL_RANGE_LIST,*PRTL_RANGE_LIST;

//
// range
//
typedef struct _RTL_RANGE
{
	//
	// start
	//
	ULONGLONG											Start;

	//
	// end
	//
	ULONGLONG											End;

	//
	// user data
	//
	PVOID												UserData;

	//
	// owner
	//
	PVOID												Owner;

	//
	// attribute
	//
	UCHAR												Attributes;

	//
	// flags
	//
	UCHAR												Flags;

	//
	// padding
	//
	UCHAR												_padding[2];

}RTL_RANGE,*PRTL_RANGE;

//
// iterator
//
typedef struct _RTL_RANGE_LIST_ITERATOR
{
	//
	// range list head
	//
	PLIST_ENTRY											RangeListHead;

	//
	// merged list head
	//
	PLIST_ENTRY											MergedHead;

	//
	// current range
	//
	PRTL_RANGE											Current;

	//
	// stamp
	//
	ULONG												Stamp;

}RTL_RANGE_LIST_ITERATOR,*PRTL_RANGE_LIST_ITERATOR;

//
// message resource entry
//
typedef struct _MESSAGE_RESOURCE_ENTRY
{
	//
	// length
	//
	USHORT												Length;

	//
	// flags
	//
	USHORT												Flags;

	//
	// text buffer
	//
	UCHAR												Text[1];

}MESSAGE_RESOURCE_ENTRY,*PMESSAGE_RESOURCE_ENTRY;

//
// Legacy Device Detection Interface
//
typedef struct _LEGACY_DEVICE_DETECTION_INTERFACE
{
	//
	// common header
	//
	INTERFACE											Common;

	//
	// legacy device detection hanlder
	//
	PLEGACY_DEVICE_DETECTION_HANDLER					LegacyDeviceDetection;

}LEGACY_DEVICE_DETECTION_INTERFACE,*PLEGACY_DEVICE_DETECTION_INTERFACE;

//
// PME interface
//
typedef struct _PCI_PME_INTERFACE
{
	//
	// common header
	//
	INTERFACE											Common;

	//
	// get PME info
	//
	PPCI_PME_GET_PME_INFORMATION						GetPmeInformation;

	//
	// clear PME status
	//
	PPCI_PME_CLEAR_PME_STATUS							ClearPmeStatus;

	//
	// update PME enable
	//
	PPCI_PME_UPDATE_ENABLE								UpdateEnable;

}PCI_PME_INTERFACE,*PPCI_PME_INTERFACE;

//
// native ide interface
//
typedef struct _NATIVEIDE_INTERFACE
{
	//
	// common header
	//
	INTERFACE											Common;

	//
	// interrupt control
	//
	PNATIVEIDE_INTERRUPT_CONTROL						InterruptControl;

}NATIVEIDE_INTERFACE,*PNATIVEIDE_INTERFACE;

//
// routing token
//
typedef struct _ROUTING_TOKEN
{
	//
	// link node
	//
	PVOID												LinkNode;

	//
	// start vector
	//
	ULONG												StaticVector;

	//
	// flags
	//
	UCHAR												Flags;

	//
	// padding
	//
	UCHAR												Reserved[3];

}ROUTING_TOKEN,*PROUTING_TOKEN;

//
// interrupt route interface
//
typedef struct _INT_ROUTE_INTERFACE_STANDARD
{
	//
	// common header
	//
	INTERFACE											Common;

	//
	// get routing
	//
	PGET_INTERRUPT_ROUTING								GetInterruptRouting;

	//
	// set routing
	//
	PSET_INTERRUPT_ROUTING_TOKEN						SetInterruptRoutingToken;

	//
	// update interrupt line
	//
	PUPDATE_INTERRUPT_LINE								UpdateInterruptLine;

}INT_ROUTE_INTERFACE_STANDARD,*PINT_ROUTE_INTERFACE_STANDARD;

//
// cardbus interface
//
typedef struct _CARDBUS_PRIVATE_INTERFACE
{
	//
	// common header
	//
	INTERFACE											Common;

	//
	// driver object
	//
	PDRIVER_OBJECT										DriverObject;

	//
	// add bus
	//
	PCARDBUS_ADD_BUS									AddBus;

	//
	// delete bus
	//
	PCARDBUS_DELETE_BUS									DeletBus;

	//
	// dispatch pnp
	//
	PCARDBUS_DISPATCH_PNP								DispatchPnp;

	//
	// get location
	//
	PCARDBUS_GET_LOCATION								GetLocation;

}CARDBUS_PRIVATE_INTERFACE,*PCARDBUS_PRIVATE_INTERFACE;

//
// bus handler
//
typedef struct _BUS_HANDLER
{
	//
	// version
	//
	ULONG												Version;

	//
	// interface type
	//
	INTERFACE_TYPE										InterfaceType;

	//
	// bus data type
	//
	BUS_DATA_TYPE										ConfigurationType;

	//
	// bus number
	//
	ULONG												BusNumber;

	//
	// device object
	//
	PDEVICE_OBJECT										DeviceObject;

	//
	// parent handler
	//
	PBUS_HANDLER										ParentHandler;

	//
	// bus data
	//
	PVOID												BusData;

	//
	// amount of bus specific strogare needed for DeviceControl function calls
	//
	ULONG												DeviceControlExtensionSize;

	//
	// supported address ranges this bus allows
	//
	PVOID												BusAddresses;

	//
	// reserved
	//
	ULONG												Reserved[4];

	//
	// get bus data
	//
	PGETSETBUSDATA										GetBusData;

	//
	// set bus data
	//
	PGETSETBUSDATA										SetBusData;

	//
	// adjust resource list
	//
	PADJUSTRESOURCELIST									AdjustResourceList;

	//
	// assign slot resource
	//
	PASSIGNSLOTRESOURCES								AssignSlotResources;

	//
	// get interrupt vector
	//
	PGETINTERRUPTVECTOR									GetInterruptVector;

	//
	// tranlate bus address
	//
	PTRANSLATEBUSADDRESS								TranslateBusAddress;

	//
	// reserved
	//
	PVOID												Spare[8];

}BUS_HANDLER,*PBUS_HANDLER;

//
//
//
typedef struct _VERIFIER_FAILURE_DATA
{
	//
	// id
	//
	ULONG												Id;

	//
	// offset = 4
	//
	ULONG												Offset4;

	//
	// offset = 8
	//
	ULONG												Offset8;

	//
	// offset = c
	//
	PCHAR												FailureMessage;
}VERIFIER_FAILURE_DATA,*PVERIFIER_FAILURE_DATA;

//
// safe boot mode
//
extern PULONG											InitSafeBootMode;

//
// hal private dispatch table
//
extern PVOID											HalPrivateDispatchTable;

//
// init range list
//
VOID RtlInitializeRangeList(__inout PRTL_RANGE_LIST RangeList);

//
// free range list
//
VOID RtlFreeRangeList(__inout PRTL_RANGE_LIST RangeList);

//
// add range
//
NTSTATUS RtlAddRange(__inout PRTL_RANGE_LIST RangeList,__in ULONGLONG Start,__in ULONGLONG End,__in UCHAR Attributes,__in ULONG Flags,__in PVOID UserData,__in PVOID Owner);

//
// find range
//
NTSTATUS RtlFindRange(__in PRTL_RANGE_LIST RangeList,__in ULONGLONG Minimum,__in ULONGLONG Maximum,__in ULONG Length,__in ULONG Alignment,__in ULONG Flags,
					  __in UCHAR AttributeAvailableMask,__in_opt PVOID Context,__in_opt PRTL_CONFLICT_RANGE_CALLBACK Callback,__out PULONGLONG Start);

//
// is range available
//
NTSTATUS RtlIsRangeAvailable(__in PRTL_RANGE_LIST RangeList,__in ULONGLONG Start,__in ULONGLONG End,__in ULONG Flags,__in UCHAR AttributeAvailableMask,
							 __in_opt PVOID Context,__in_opt PRTL_CONFLICT_RANGE_CALLBACK Callback,__out PBOOLEAN Available);

//
// get first range
//
NTSTATUS RtlGetFirstRange(__in PRTL_RANGE_LIST RangeList,__out PRTL_RANGE_LIST_ITERATOR Iterator,__out PRTL_RANGE* Range);

//
// get next range
//
NTSTATUS RtlGetNextRange(__inout PRTL_RANGE_LIST_ITERATOR Iterator,__out PRTL_RANGE *Range,__in BOOLEAN MoveForwards);

//
// copy range list
//
NTSTATUS RtlCopyRangeList(__out PRTL_RANGE_LIST CopyRangeList,__in PRTL_RANGE_LIST RangeList);

//
// delete owners ranges
//
NTSTATUS RtlDeleteOwnersRanges(__inout PRTL_RANGE_LIST RangeList,__in PVOID Owner);

//
// delete range
//
NTSTATUS RtlDeleteRange(__inout PRTL_RANGE_LIST RangeList,__in ULONGLONG Start,__in ULONGLONG End,__in PVOID Owner);

//
// find message
//
NTSTATUS RtlFindMessage(__in PVOID DllHandle,__in ULONG MessageTableId,__in ULONG MessageLanguageId,__in ULONG MessageId,__out PMESSAGE_RESOURCE_ENTRY* MessageEntry);

//
// power transition
//
VOID KdPowerTransition(__in DEVICE_POWER_STATE State);

//
// adjust resource list
//
NTSTATUS HalAdjustResourceList(__in PIO_RESOURCE_REQUIREMENTS_LIST List);

//
// is verification enabled
//
BOOLEAN VfIsVerificationEnabled(__in ULONG MajorVersion,__in ULONG MinorVersion);

//
// fail system bios
//
VOID VfFailSystemBIOS(__in ULONG FailuerComponent,__in ULONG FailureId,__in ULONG Arg8,__in PVOID ArgC,__in PCHAR FailureMessage,__in PCHAR Format,...);

//
// fail device node
//
VOID VfFailDeviceNode(__in PDEVICE_OBJECT FailurePhysicalDeviceObject,__in ULONG FailuerComponent,__in ULONG FailureId,
					  __in ULONG Arg8,__in PVOID ArgC,__in PCHAR FailureMessage,__in PCHAR Format,...);