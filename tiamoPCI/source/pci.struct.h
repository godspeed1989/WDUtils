//********************************************************************
//	created:	22:7:2008   12:04
//	file:		pci.struct.h
//	author:		tiamo
//	purpose:	struct
//********************************************************************

#pragma once

#include <pshpack1.h>

//
// ide controller progif
//
typedef union _PCI_NATIVE_IDE_CONTROLLER_PROGIF
{
	struct
	{
		//
		// primary state
		//
		UCHAR											PrimaryState		: 1;

		//
		// primary switchable
		//
		UCHAR											PrimarySwitchable	: 1;

		//
		// secondary state
		//
		UCHAR											SecondaryState		: 1;

		//
		// secondary switchable
		//
		UCHAR											SecondarySwitchable	: 1;

		//
		// reserved
		//
		UCHAR											Reserved			: 4;
	};

	//
	// progif
	//
	UCHAR												ProgIf;
}PCI_NATIVE_IDE_CONTROLLER_PROGIF,*PPCI_NATIVE_IDE_CONTROLLER_PROGIF;

//
// pci irq routing table header
//
typedef struct _PCI_IRQ_ROUTING_TABLE_HEAD
{
	//
	// signature = $PIR
	//
	ULONG												Signature;

	//
	// version
	//
	USHORT												Version;

	//
	// size
	//
	USHORT												Size;

	//
	// router bus
	//
	UCHAR												RouterBus;

	//
	// route dev func
	//
	UCHAR												RouterDevFunc;

	//
	// exclusive irqs
	//
	USHORT												ExclusiveIRQs;

	//
	// Compatible PCI Interrupt Router
	//
	ULONG												CompatiblePCIInterruptRouter;

	//
	// Miniport Data
	//
	ULONG												MiniportData;

	//
	// reserved
	//
	UCHAR												Reserved[11];

	//
	// checksum
	//
	UCHAR												Checksum;

}PCI_IRQ_ROUTING_TABLE_HEAD,*PPCI_IRQ_ROUTING_TABLE_HEAD;

//
// irq routine table entry
//
typedef struct _PCI_IRQ_ROUTING_TABLE_ENTRY
{
	//
	// bus
	//
	UCHAR												BusNumber;

	//
	// reserved
	//
	UCHAR												Reserved : 3;

	//
	// device
	//
	UCHAR												DeviceNumber : 5;

	//
	// link and bitmap
	//
	struct
	{
		//
		// link
		//
		UCHAR											Link;

		//
		// bitmap
		//
		USHORT											Bitmap;
	}LinkBitmap[4];

	//
	// slot
	//
	UCHAR												SlotNumber;

	//
	// reserved
	//
	UCHAR												Reserved2;

}PCI_IRQ_ROUTING_TABLE_ENTRY,*PPCI_IRQ_ROUTING_TABLE_ENTRY;

//
// rom header
//
typedef struct _PCI_ROM_HEADER
{
	//
	// signature
	//
	USHORT												Signature;

	//
	// reserved
	//
	UCHAR												Reserved[0x16];

	//
	// pci date ptr
	//
	USHORT												PciDataPtr;

}PCI_ROM_HEADER,*PPCI_ROM_HEADER;

//
// pci data structor
//
typedef struct _PCI_ROM_DATA_HEADER
{
	//
	// signature
	//
	ULONG												Signature;

	//
	// vendor id
	//
	USHORT												VendorId;

	//
	// device id
	//
	USHORT												DeviceId;

	//
	// vpd
	//
	USHORT												VpdPtr;

	//
	// length
	//
	USHORT												Length;

	//
	// revision
	//
	UCHAR												Revision;

	//
	// class code
	//
	UCHAR												ClassCode[3];

	//
	// image length
	//
	USHORT												ImageLength;

	//
	// data revision
	//
	USHORT												CodeDataRevisionLevel;

	//
	// code type
	//
	UCHAR												CodeType;

	//
	// indicator
	//
	UCHAR												Indicator;

	//
	// reserved
	//
	USHORT												Reserved;

}PCI_ROM_DATA_HEADER,*PPCI_ROM_DATA_HEADER;

#include <poppack.h>

//
// id buffer
//
typedef struct _PCI_ID_BUFFER
{
	//
	// count
	//
	ULONG												Count;

	//
	// ansi string
	//
	ANSI_STRING											AnsiString[8];

	//
	// unicode string size
	//
	USHORT												UnicodeStringSize[8];

	//
	// total length
	//
	USHORT												TotalLength;

	//
	// buffer
	//
	PCHAR												CurrentBuffer;

	//
	// storage
	//
	CHAR												StorageBuffer[0x100];

}PCI_ID_BUFFER,*PPCI_ID_BUFFER;

//
// pci hack table entry
//
typedef struct _PCI_HACK_TABLE_ENTRY
{
	//
	// vendor id
	//
	USHORT												VendorId;

	//
	// device id
	//
	USHORT												DeviceId;

	//
	// sub vendor id
	//
	USHORT												SubVendorId;

	//
	// sub system id
	//
	USHORT												SubSystemId;

	//
	// revision id
	//
	UCHAR												RevisionId;

	//
	// sub system and sub vendor id is valid
	//
	UCHAR												SubSystemVendorIdValid:1;

	//
	// revision is valid
	//
	UCHAR												RevisionIdValid:1;

	//
	// reserved for padding
	//
	UCHAR												Reserved0:6;

	//
	// hack flags
	//
	ULONGLONG											HackFlags;

}PCI_HACK_TABLE_ENTRY,*PPCI_HACK_TABLE_ENTRY;

//
// debug port
//
typedef struct _PCI_DEBUG_PORT
{
	//
	// bus
	//
	ULONG												Bus;

	//
	// slot number
	//
	PCI_SLOT_NUMBER										SlotNumber;

}PCI_DEBUG_PORT,*PPCI_DEBUG_PORT;

//
// minor dispatch table
//
typedef struct _PCI_MN_DISPATCH_TABLE
{
	//
	// style
	//
	PCI_DISPATCH_STYLE									DispatchStyle;

	//
	// dispatch routine
	//
	PCI_DISPATCH_ROUTINE								DispatchFunction;

}PCI_MN_DISPATCH_TABLE,*PPCI_MN_DISPATCH_TABLE;

//
// major dispatch table
//
typedef struct _PCI_MJ_DISPATCH_TABLE
{
	//
	// pnp max function
	//
	ULONG												PnpIrpMaximumMinorFunction;

	//
	// pnp dispatch table
	//
	PPCI_MN_DISPATCH_TABLE								PnpIrpDispatchTable;

	//
	// power max function
	//
	ULONG												PowerIrpMaximumMinorFunction;

	//
	// power dispatch table
	//
	PPCI_MN_DISPATCH_TABLE								PowerIrpDispatchTable;

	//
	// system control style
	//
	PCI_DISPATCH_STYLE									SystemControlIrpDispatchStyle;

	//
	// system control dispatch routine
	//
	PCI_DISPATCH_ROUTINE								SystemControlIrpDispatchFunction;

	//
	// other sytle
	//
	PCI_DISPATCH_STYLE									OtherIrpDispatchStyle;

	//
	// other dispatch routine
	//
	PCI_DISPATCH_ROUTINE								OtherIrpDispatchFunction;

}PCI_MJ_DISPATCH_TABLE,*PPCI_MJ_DISPATCH_TABLE;

//
// power state
//
typedef struct _PCI_POWER_STATE
{
	//
	// current system state
	//
	SYSTEM_POWER_STATE									CurrentSystemState;

	//
	// current device state
	//
	DEVICE_POWER_STATE									CurrentDeviceState;

	//
	// system wake level
	//
	SYSTEM_POWER_STATE									SystemWakeLevel;

	//
	// device wake level
	//
	DEVICE_POWER_STATE									DeviceWakeLevel;

	//
	// map system state to device state
	//
	DEVICE_POWER_STATE									SystemStateMapping[POWER_SYSTEM_MAXIMUM];

	//
	// wait wake irp
	//
	PIRP												WaitWakeIrp;

	//
	// saved cancel routine
	//
	PDRIVER_CANCEL										SavedCancelRoutine;

	//
	// paging count
	//
	LONG												Paging;

	//
	// hibernate count
	//
	LONG												Hibernate;

	//
	// crash dump count
	//
	LONG												CrashDump;

}PCI_POWER_STATE,*PPCI_POWER_STATE;

//
// pci lock
//
typedef struct _PCI_LOCK
{
	//
	// spinlock
	//
	KSPIN_LOCK											SpinLock;

	//
	// saved irql
	//
	KIRQL												OldIrql;

	//
	// file
	//
	PCHAR												File;

	//
	// line
	//
	ULONG												Line;

}PCI_LOCK,*PPCI_LOCK;

//
// hotplug
//
typedef struct _PCI_HOTPLUG_PARAMETERS
{
	//
	// acquired
	//
	BOOLEAN												Acquired;

	//
	// cache line size
	//
	UCHAR												CacheLineSize;

	//
	// latency timer
	//
	UCHAR												LatencyTimer;

	//
	// enable PERR
	//
	BOOLEAN												EnablePERR;

	//
	// enable SERR
	//
	BOOLEAN												EnableSERR;

}PCI_HOTPLUG_PARAMETERS,*PPCI_HOTPLUG_PARAMETERS;

//
// function resource
//
typedef struct _PCI_FUNCTION_RESOURCES
{
	//
	// limit
	//
	IO_RESOURCE_DESCRIPTOR								Limit[7];

	//
	// current
	//
	CM_PARTIAL_RESOURCE_DESCRIPTOR						Current[7];

}PCI_FUNCTION_RESOURCES,*PPCI_FUNCTION_RESOURCES;

//
// header
//
typedef union _PCI_HEADER_TYPE_DEPENDENT
{
	//
	// device type
	//
	struct
	{
		UCHAR											Spare[4];
	}type0;

	//
	// pci-to-pci bridge
	//
	struct
	{
		//
		// primary bus
		//
		UCHAR											PrimaryBus;

		//
		// sencodary bus
		//
		UCHAR											SecondaryBus;

		//
		// subordinate bus
		//
		UCHAR											SubordinateBus;

		//
		// subtractive decode
		//
		UCHAR											SubtractiveDecode : 1;

		//
		// isa
		//
		UCHAR											IsaBitSet : 1;

		//
		// vga
		//
		UCHAR											VgaBitSet : 1;

		//
		// bus number changed
		//
		UCHAR											WeChangedBusNumbers : 1;

		//
		// require isa
		//
		UCHAR											IsaBitRequired : 1;

		//
		// pading
		//
		UCHAR											Padding : 3;
	}type1;

	struct
	{
		//
		// primary bus
		//
		UCHAR											PrimaryBus;

		//
		// sencodary bus
		//
		UCHAR											SecondaryBus;

		//
		// subordinate bus
		//
		UCHAR											SubordinateBus;

		//
		// subtractive decode
		//
		UCHAR											SubtractiveDecode : 1;

		//
		// isa
		//
		UCHAR											IsaBitSet : 1;

		//
		// vga
		//
		UCHAR											VgaBitSet : 1;

		//
		// bus number changed
		//
		UCHAR											WeChangedBusNumbers : 1;

		//
		// require isa
		//
		UCHAR											IsaBitRequired : 1;

		//
		// pading
		//
		UCHAR											Padding : 3;
	}type2;

}PCI_HEADER_TYPE_DEPENDENT,*PPCI_HEADER_TYPE_DEPENDENT;

//
// common extension
//
typedef struct _PCI_COMMON_EXTENSION
{
	//
	// next
	//
	SINGLE_LIST_ENTRY									ListEntry;

	//
	// signature
	//
	PCI_SIGNATURE										ExtensionType;

	//
	// irp dispatch table
	//
	PPCI_MJ_DISPATCH_TABLE								IrpDispatchTable;

	//
	// device state
	//
	PCI_DEVICE_STATE									DeviceState;

	//
	// next state
	//
	PCI_DEVICE_STATE									TentativeNextState;

	//
	// secondary extension lock
	//
	KEVENT												SecondaryExtLock;

}PCI_COMMON_EXTENSION,*PPCI_COMMON_EXTENSION;

//
// fdo extension
//
typedef struct _PCI_FDO_EXTENSION
{
	//
	// common header
	//
	PCI_COMMON_EXTENSION								Common;

	//
	// physical device object
	//
	PDEVICE_OBJECT										PhysicalDeviceObject;

	//
	// function device object
	//
	PDEVICE_OBJECT										FunctionalDeviceObject;

	//
	// attached device object
	//
	PDEVICE_OBJECT										AttachedDeviceObject;

	//
	// child list lock
	//
	KEVENT												ChildListLock;

	//
	// child pdo list
	//
	SINGLE_LIST_ENTRY									ChildPdoList;

	//
	// bus root fdo extension
	//
	struct _PCI_FDO_EXTENSION*							BusRootFdoExtension;

	//
	// parent fdo extension
	//
	struct _PCI_FDO_EXTENSION*							ParentFdoExtension;

	//
	// child bridge pdo list
	//
	struct _PCI_PDO_EXTENSION*							ChildBridgePdoList;

	//
	// pci bus interface
	//
	PPCI_BUS_INTERFACE_STANDARD							PciBusInterface;

	//
	// max subordinate bus
	//
	UCHAR												MaxSubordinateBus;

	//
	// bus handler
	//
	PBUS_HANDLER										BusHandler;

	//
	// base bus
	//
	UCHAR												BaseBus;

	//
	// fake
	//
	BOOLEAN												Fake;

	//
	// child delete
	//
	BOOLEAN												ChildDelete;

	//
	// scaned
	//
	BOOLEAN												Scanned;

	//
	// arbiter initialized
	//
	BOOLEAN												ArbitersInitialized;

	//
	// broken video hack applied
	//
	BOOLEAN												BrokenVideoHackApplied;

	//
	// hibernated
	//
	BOOLEAN												Hibernated;

	//
	// power state
	//
	PCI_POWER_STATE										PowerState;

	//
	// secondary extension
	//
	SINGLE_LIST_ENTRY									SecondaryExtension;

	//
	// wait wake count
	//
	LONG												ChildWaitWakeCount;

	//
	// preserved config
	//
	PPCI_COMMON_CONFIG									PreservedConfig;

	//
	// pci lock
	//
	PCI_LOCK											Lock;

	//
	// hotplug parameters
	//
	PCI_HOTPLUG_PARAMETERS								HotPlugParameters;

	//
	// bus hack flags
	//
	ULONG												BusHackFlags;

}PCI_FDO_EXTENSION,*PPCI_FDO_EXTENSION;

//
// pdo extension
//
typedef struct _PCI_PDO_EXTENSION
{
	//
	// common
	//
	PCI_COMMON_EXTENSION								Common;

	//
	// slot
	//
	PCI_SLOT_NUMBER										Slot;

	//
	// physical device object
	//
	PDEVICE_OBJECT										PhysicalDeviceObject;

	//
	// parent fdo extension
	//
	PPCI_FDO_EXTENSION									ParentFdoExtension;

	//
	// secondary extension
	//
	SINGLE_LIST_ENTRY									SecondaryExtension;

	//
	// bus interface reference count
	//
	LONG												BusInterfaceReferenceCount;

	//
	// agp interface reference count
	//
	LONG												AgpInterfaceReferenceCount;

	//
	// vendor id
	//
	USHORT												VendorId;

	//
	// device id
	//
	USHORT												DeviceId;

	//
	// sub system id
	//
	USHORT												SubSystemId;

	//
	// sub vendor id
	//
	USHORT												SubVendorId;

	//
	// revision id
	//
	UCHAR												RevisionId;

	//
	// prog if
	//
	UCHAR												ProgIf;

	//
	// sub class
	//
	UCHAR												SubClass;

	//
	// base class
	//
	UCHAR												BaseClass;

	//
	// additional resources count
	//
	UCHAR												AdditionalResourceCount;

	//
	// adjusted interrupt line
	//
	UCHAR												AdjustedInterruptLine;

	//
	// interrupt pin
	//
	UCHAR												InterruptPin;

	//
	// raw interrupt line
	//
	UCHAR												RawInterruptLine;

	//
	// capabilities ptr
	//
	UCHAR												CapabilitiesPtr;

	//
	// saved latency timer
	//
	UCHAR												SavedLatencyTimer;

	//
	// saved cache line size
	//
	UCHAR												SavedCacheLineSize;

	//
	// head type
	//
	UCHAR												HeaderType;

	//
	// not present
	//
	BOOLEAN												NotPresent;

	//
	// reported missing
	//
	BOOLEAN												ReportedMissing;

	//
	// expected writeback failure
	//
	BOOLEAN												ExpectedWritebackFailure;

	//
	// do not touch PME
	//
	BOOLEAN												NoTouchPmeEnable;

	//
	// legacy driver
	//
	BOOLEAN												LegacyDriver;

	//
	// update hardware
	//
	BOOLEAN												UpdateHardware;

	//
	// moved device
	//
	BOOLEAN												MovedDevice;

	//
	// disable power down
	//
	BOOLEAN												DisablePowerDown;

	//
	// need hotplug
	//
	BOOLEAN												NeedsHotPlugConfiguration;

	//
	// switch to native mode
	//
	BOOLEAN												SwitchedIDEToNativeMode;

	//
	// BIOS allow native mode
	//
	BOOLEAN												BIOSAllowsIDESwitchToNativeMode;

	//
	// io space under native ide control
	//
	BOOLEAN												IoSpaceUnderNativeIdeControl;

	//
	// on debug path
	//
	BOOLEAN												OnDebugPath;

	//
	// power state
	//
	PCI_POWER_STATE										PowerState;

	//
	// header
	//
	PCI_HEADER_TYPE_DEPENDENT							Dependent;

	//
	// hack flags
	//
	ULARGE_INTEGER										HackFlags;

	//
	// resource
	//
	PPCI_FUNCTION_RESOURCES								Resources;

	//
	// bridge fdo extension
	//
	PPCI_FDO_EXTENSION									BridgeFdoExtension;

	//
	// next bridge
	//
	struct _PCI_PDO_EXTENSION*							NextBridge;

	//
	// next hash entry
	//
	struct _PCI_PDO_EXTENSION*							NextHashEntry;

	//
	// lock
	//
	PCI_LOCK											Lock;

	//
	// pmc
	//
	PCI_PMC												PowerCapabilities;

	//
	// target agp capabilites ptr
	//
	UCHAR												TargetAgpCapabilityId;

	//
	// command enables
	//
	USHORT												CommandEnables;

	//
	// initial command
	//
	USHORT												InitialCommand;

}PCI_PDO_EXTENSION,*PPCI_PDO_EXTENSION;

//
// pci interface
//
typedef struct _PCI_INTERFACE
{
	//
	// interface guid
	//
	GUID const*											Guid;

	//
	// min size
	//
	USHORT												MinSize;

	//
	// min version
	//
	USHORT												MinVersion;

	//
	// max version
	//
	USHORT												MaxVersion;

	//
	// flags
	//
	USHORT												Flags;

	//
	// reference count
	//
	ULONG												ReferenceCount;

	//
	// signature
	//
	PCI_SIGNATURE										Signature;

	//
	// constructor
	//
	PCI_INTERFACE_CONSTRUCTOR							Constructor;

	//
	// initializer
	//
	PCI_ARBITER_INSTANCE_INITIALIZER					Initializer;

}PCI_INTERFACE,*PPCI_INTERFACE;

//
// secondary extension
//
typedef struct _PCI_SECONDARY_EXTENSION
{
	//
	// next
	//
	SINGLE_LIST_ENTRY									ListEntry;

	//
	// type
	//
	PCI_SIGNATURE										Type;

	//
	// destrutor
	//
	PCI_ARBITER_INSTANCE_DESTRUCTOR						Destructor;

}PCI_SECONDARY_EXTENSION,*PPCI_SECONDARY_EXTENSION;

//
// arbiter instance
//
typedef struct _PCI_ARBITER_INSTANCE
{
	//
	// secondary extension
	//
	PCI_SECONDARY_EXTENSION								SecondaryExtension;

	//
	// interface
	//
	PPCI_INTERFACE										Interface;

	//
	// fdo extension
	//
	PPCI_FDO_EXTENSION									BusFdoExtension;

	//
	// name
	//
	WCHAR												InstanceName[24];

	//
	// common instance
	//
	ARBITER_INSTANCE									CommonInstance;

}PCI_ARBITER_INSTANCE,*PPCI_ARBITER_INSTANCE;

//
// configurator
//
typedef struct _PCI_CONFIGURATOR
{
	//
	// massage header for limit determination
	//
	PCI_MASSAGE_HEADER_FOR_LIMITS_DETERMINATION				MassageHeaderForLimitsDetermination;

	//
	// restore current
	//
	PCI_RESTORE_CURRENT										RestoreCurrent;

	//
	// save limits
	//
	PCI_SAVE_LIMITS											SaveLimits;

	//
	// save current setting
	//
	PCI_SAVE_CURRENT_SETTINGS								SaveCurrentSettings;

	//
	// change resource settings
	//
	PCI_CHANGE_RESOURCE_SETTINGS							ChangeResourceSettings;

	//
	// get additional descriptors
	//
	PCI_GET_ADDITIONAL_RESOURCE_DESCRIPTORS					GetAdditionalResourceDescriptors;

	//
	// reset device
	//
	PCI_RESET_DEVICE										ResetDevice;

}PCI_CONFIGURATOR,*PPCI_CONFIGURATOR;

//
// configurator param
//
typedef struct _PCI_CONFIGURATOR_PARAM
{
	//
	// pdo ext
	//
	PPCI_PDO_EXTENSION										PdoExt;

	//
	// original config
	//
	PPCI_COMMON_HEADER										OriginalConfig;

	//
	// config workspace
	//
	PPCI_COMMON_HEADER										Working;

	//
	// configurator
	//
	PPCI_CONFIGURATOR										Configurator;

	//
	// saved secondary status
	//
	USHORT													SavedSecondaryStatus;

	//
	// saved status
	//
	USHORT													SavedStatus;

	//
	// saved command
	//
	USHORT													SavedCommand;

}PCI_CONFIGURATOR_PARAM,*PPCI_CONFIGURATOR_PARAM;

//
// legacy device info
//
typedef struct _PCI_LEGACY_DEVICE_INFO
{
	//
	// list entry
	//
	SINGLE_LIST_ENTRY										ListEntry;

	//
	// owner device
	//
	PDEVICE_OBJECT											OwnerDevice;

	//
	// bus
	//
	ULONG													BusNumber;

	//
	// slot
	//
	ULONG													SlotNumber;

	//
	// base class
	//
	UCHAR													BaseClass;

	//
	// sub class
	//
	UCHAR													SubClass;

	//
	// interrupt line
	//
	UCHAR													InterruptLine;

	//
	// interrupt pin
	//
	UCHAR													InterruptPin;

	//
	// parent pdo
	//
	PDEVICE_OBJECT											ParentPdo;

	//
	// routing token
	//
	ROUTING_TOKEN											RoutingToken;

	//
	// pdo ext
	//
	PPCI_PDO_EXTENSION										PdoExt;

}PCI_LEGACY_DEVICE_INFO,*PPCI_LEGACY_DEVICE_INFO;

//
// int routing extension
//
typedef struct _PCI_INTTERUPT_ROUTING_SECONDARY_EXTENSION
{
	//
	// common
	//
	PCI_SECONDARY_EXTENSION									SecondaryExtension;

	//
	// token
	//
	ROUTING_TOKEN											RoutingToken;

}PCI_INTTERUPT_ROUTING_SECONDARY_EXTENSION,*PPCI_INTTERUPT_ROUTING_SECONDARY_EXTENSION;

//
// memory arbiter extension
//
typedef struct _PCI_MEMORY_ARBITER_EXTENSION
{
	//
	// have prefetchable resource
	//
	BOOLEAN													HasPrefetchableResource;

	//
	// memory extension initialized
	//
	BOOLEAN													Initialized;

	//
	// prefetchable resource count
	//
	USHORT													PrefetchableResourceCount;

	//
	// prefetchable ordering list
	//
	ARBITER_ORDERING_LIST									PrefetchableOrderingList;

	//
	// normal ordering list
	//
	ARBITER_ORDERING_LIST									NormalOrderingList;

	//
	// default ordering list
	//
	ARBITER_ORDERING_LIST									DefaultOrderingList;

}PCI_MEMORY_ARBITER_EXTENSION,*PPCI_MEMORY_ARBITER_EXTENSION;

//
// partial list context
//
typedef struct _PCI_PARTIAL_LIST_CONTEXT
{
	//
	// partial list
	//
	PCM_PARTIAL_RESOURCE_LIST								PartialList;

	//
	// resourc type
	//
	UCHAR													ResourceType;

	//
	// count
	//
	ULONG													DescriptorCount;

	//
	// current descriptor
	//
	PCM_PARTIAL_RESOURCE_DESCRIPTOR							CurrentDescriptor;

	//
	// alias io descriptor
	//
	CM_PARTIAL_RESOURCE_DESCRIPTOR							AliasPortDescriptor;

}PCI_PARTIAL_LIST_CONTEXT,*PPCI_PARTIAL_LIST_CONTEXT;

//
// range list entry
//
typedef struct _PCI_RANGE_LIST_ENTRY
{
	//
	// list entry
	//
	LIST_ENTRY												ListEntry;

	//
	// start
	//
	ULONGLONG												Start;

	//
	// end
	//
	ULONGLONG												End;

	//
	// valid
	//
	BOOLEAN													Valid;

}PCI_RANGE_LIST_ENTRY,*PPCI_RANGE_LIST_ENTRY;