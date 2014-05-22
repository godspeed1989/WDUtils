//********************************************************************
//	created:	22:7:2008   12:05
//	file:		pci.const.h
//	author:		tiamo
//	purpose:	const
//********************************************************************

#pragma once

//
// device state
//
enum PCI_DEVICE_STATE
{
	//
	// not started
	//
	PciNotStarted										= 0,

	//
	// started
	//
	PciStarted											= 1,

	//
	// deleted
	//
	PciDeleted											= 2,

	//
	// stopped
	//
	PciStopped											= 3,

	//
	// surprise removed
	//
	PciSurpriseRemoved									= 4,

	//
	// sync operation
	//
	PciSynchronizedOperation							= 5,

	//
	// max state
	//
	PciMaxObjectState									= 6,
};

//
// pci signature
//
enum PCI_SIGNATURE
{
	//
	// pdo extention
	//
	PciPdoExtensionType									= 'icP0',

	//
	// fdo extension
	//
	PciFdoExtensionType									= 'icP1',

	//
	// io arbiter
	//
	PciArb_Io											= 'icP2',

	//
	// memory arbiter
	//
	PciArb_Memory										= 'icP3',

	//
	// interrupt arbiter
	//
	PciArb_Interrupt									= 'icP4',

	//
	// bus number arbiter
	//
	PciArb_BusNumber									= 'icP5',

	//
	// interrupt translator
	//
	PciTrans_Interrupt									= 'icP6',

	//
	// bus handler interface
	//
	PciInterface_BusHandler								= 'icP7',

	//
	// interrupt route handler interface
	//
	PciInterface_IntRouteHandler						= 'icP8',

	//
	// carbus interface
	//
	PciInterface_PciCb									= 'icP9',

	//
	// legacy device detection interface
	//
	PciInterface_LegacyDeviceDetection					= 'icP:',

	//
	// pme handler interface
	//
	PciInterface_PmeHandler								= 'icP;',

	//
	// device present interface
	//
	PciInterface_DevicePresent							= 'icP<',

	//
	// native ide interface
	//
	PciInterface_NativeIde								= 'icP=',

	//
	// agp target interface
	//
	PciInterface_AgpTarget								= 'icP>',
};

//
// dispatch style
//
enum PCI_DISPATCH_STYLE
{
	//
	// call dispatch routine,then complete the irp
	//
	IRP_COMPLETE										= 0,

	//
	// call dispatch routine,then pass it down
	//
	IRP_DOWNWARD										= 1,

	//
	// pass it down,and wait it to be completed,then call dispatch routine
	//
	IRP_UPWARD											= 2,

	//
	// call dispatch routine only
	//
	IRP_DISPATCH										= 3,
};

//
// pci interface flags
//
enum
{
	//
	// valid pdo interface
	//
	PCI_INTERFACE_FLAGS_VALID_FOR_PDO					= 1,

	//
	// valid fdo interface
	//
	PCI_INTERFACE_FLAGS_VALID_FOR_FDO					= 2,

	//
	// valid root interface
	//
	PCI_INTERFACE_FLAGS_ONLY_FOR_ROOT					= 4,
};

//
// device type
//
enum
{
	//
	// host bridge
	//
	PCI_DEVICE_TYPE_HOST								= 1,

	//
	// pci-to-pci bridge
	//
	PCI_DEVICE_TYPE_PCI_TO_PCI							= 2,

	//
	// pci-to-cardbus bridge
	//
	PCI_DEVICE_TYPE_CARDBUS								= 3,

	//
	// other devices
	//
	PCI_DEVICE_TYPE_DEVICE								= 4,
};

//
// bus hack flags
//
enum
{
	//
	// lock resource for those devices on this bus
	//
	PCI_BUS_HACK_LOCK_RES								= 1,
};

//
// system hack flags
//
enum
{
	//
	// disable prefetch processing
	//
	PCI_SYSTEM_HACK_ROOT_NO_PREFETCH_MEMORY				= 1,
};

//
// hack flags
//
enum
{
	//
	// skip 8
	//
	PCI_HACK_FLAGS_LOW_SKIP_DEVICE_TYPE1_8				= 0x00000008,

	//
	// skip 10
	//
	PCI_HACK_FLAGS_LOW_SKIP_DEVICE_TYPE2_10				= 0x00000010,

	//
	// ghost device
	//
	PCI_HACK_FLAGS_LOW_GHOST_DEVICE						= 0x00001000,

	//
	// only one device on this bus
	//
	PCI_HACK_FLAGS_LOW_ONLY_ONE_DEVICE_ON_BUS			= 0x00002000,

	//
	// do not touch command
	//
	PCI_HACK_FLAGS_LOW_DONOT_TOUCH_COMMAND				= 0x00004000,

	//
	// no disable decode
	//
	PCI_HACK_FLAGS_NO_DISABLE_DECODE					= 0x00010000,

	//
	// force subtractive decode
	//
	PCI_HACK_FLAGS_LOW_SUBTRACTIVE_DECODE				= 0x00040000,

	//
	// no sub id
	//
	PCI_HACK_FLAGS_LOW_NO_SUB_IDS						= 0x00400000,

	//
	// no power caps
	//
	PCI_HACK_FLAGS_LOW_NO_PME_CAPS						= 0x20000000,

	//
	// no disable decode
	//
	PCI_HACK_FLAGS_NO_DISABLE_DECODE2					= 0x40000000,

	//
	// no sub id
	//
	PCI_HACK_FLAGS_LOW_NO_SUB_IDS2						= 0x80000000,

	//
	// broken video
	//
	PCI_HACK_FLAGS_HIGH_BROKEN_VIDEO					= 0x00000001,

	//
	// force system device
	//
	PCI_HACK_FLAGS_HIGH_WRITEBACK_FAILURE				= 0x00000002,

	//
	// can reset bridge
	//
	PCI_HACK_FLAGS_HIGH_RESET_BRIDGE_OK					= 0x00000004,

	//
	// no native ide
	//
	PCI_HACK_FLAGS_HIGH_DISABLE_NATIVE_IDE				= 0x00000008,

	//
	// can not be removed
	//
	PCI_HACK_FLAGS_HIGH_NO_REMOVE						= 0x00000010,

	//
	// preseve config
	//
	PCI_HACK_FLAGS_HIGH_PRESERVE_BRIDGE_CONFIG			= 0x00000080,
};

//
// port io arbiter flags
//
enum
{
	//
	// entry has already been preprocessed
	//
	PCI_IO_ARBITER_PREPROCESSED							= 1,

	//
	// alias is always available
	//
	PCI_IO_ARBITER_ALIAS_AVAILABLE						= 2,

	//
	// isa bridge
	//
	PCI_IO_ARBITER_BRIDGE_WITH_ISA_BIT					= 4,

	//
	// window decode (allocate io resource for bridge)
	//
	PCI_IO_ARBITER_WINDOW_DECODE						= 8,

	//
	// isa window
	//
	PCI_IO_ARBITER_ISA_WINDOW							= PCI_IO_ARBITER_BRIDGE_WITH_ISA_BIT | PCI_IO_ARBITER_WINDOW_DECODE,
};

//
// disaptch routine
//
typedef NTSTATUS (*PCI_DISPATCH_ROUTINE)(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PVOID CommonExtension);

//
// arbiter instance constructor
//
typedef NTSTATUS (*PCI_INTERFACE_CONSTRUCTOR)(__in struct _PCI_COMMON_EXTENSION* CommonExt,__in struct _PCI_INTERFACE* PciInterface,
											  __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// arbiter instance initializer
//
typedef NTSTATUS (*PCI_ARBITER_INSTANCE_INITIALIZER)(__in struct _PCI_ARBITER_INSTANCE* Instance);

//
// arbiter instance destructor
//
typedef VOID (*PCI_ARBITER_INSTANCE_DESTRUCTOR)(__in struct _PCI_ARBITER_INSTANCE* Instance);

//
// massage header for limits determination
//
typedef VOID (*PCI_MASSAGE_HEADER_FOR_LIMITS_DETERMINATION)(__in struct _PCI_CONFIGURATOR_PARAM* Param);

//
// restore current
//
typedef VOID (*PCI_RESTORE_CURRENT)(__in struct _PCI_CONFIGURATOR_PARAM* Param);

//
// save limits
//
typedef VOID (*PCI_SAVE_LIMITS)(__in struct _PCI_CONFIGURATOR_PARAM* Param);

//
// save current settings
//
typedef VOID (*PCI_SAVE_CURRENT_SETTINGS)(__in struct _PCI_CONFIGURATOR_PARAM* Param);

//
// change resource settings
//
typedef VOID (*PCI_CHANGE_RESOURCE_SETTINGS)(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config);

//
// get additional resource descriptor
//
typedef VOID (*PCI_GET_ADDITIONAL_RESOURCE_DESCRIPTORS)(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config,__in PIO_RESOURCE_DESCRIPTOR IoResDesc);

//
// reset device
//
typedef NTSTATUS (*PCI_RESET_DEVICE)(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config);

//
// try leave
//
#define try_leave(S)									{S;__leave;}

//
// add to pointer
//
#define Add2Ptr(P,S,T)									static_cast<T>(static_cast<PVOID>(static_cast<PUCHAR>(static_cast<PVOID>(P)) + (S)))

//
// const unicode string
//
#define WstrToUnicodeString(u,p)						(u)->Length = ((u)->MaximumLength = (USHORT) (sizeof((p))) - sizeof(WCHAR)); (u)->Buffer = (p)

//
// allocate cold poll
//
#define PciAllocateColdPoolWithTag(P,L,T)				ExAllocatePoolWithTag((POOL_TYPE)((P)|POOL_COLD_ALLOCATION),(L),T)

//
// acquire pci lock
//
#define PciAcquireLock(L)								{(L)->File = __FILE__;(L)->Line = __LINE__;KeAcquireSpinLock(&((L)->SpinLock),&((L)->OldIrql));}

//
// release pci lock
//
#define PciReleaseLock(L)								{KeReleaseSpinLock(&((L)->SpinLock),(L)->OldIrql);}

//
// convert to string
//
#define PCI_STRING1(a)									#a

//
// convert to string
//
#define PCI_STRING2(a)									PCI_STRING1(a)

//
// make pragma message
//
#define PCI_PRAGMA_MSG(x)								message(__FILE__ ## "(" ## PCI_STRING2(__LINE__) ##"):  <" __FUNCTION__ "> " ## x)

//
// flag on
//
#define FlagOn(_F,_SF)									((_F) & (_SF))

//
// boolean flag on
//
#define BooleanFlagOn(F,SF)								((BOOLEAN)(((F) & (SF)) != 0))

//
// set flag
//
#define SetFlag(_F,_SF)									((_F) |= (_SF))

//
// clear flag
//
#define ClearFlag(_F,_SF)								((_F) &= ~(_SF))

//
// both ide channels are in native mode
//
#define NativeModeIde(ProgIf)							(((ProgIf) & 5) == 5)

//
// mode switchable
//
#define IdeModeSwitchable(ProgIf)						(((ProgIf) & 10) == 10)

//
// set native mode
//
#define SetNativeModeIde(ProgIf)						((ProgIf) |= 5)

//
// set legacy mode
//
#define SetLegacyModeIde(ProgIf)						((ProgIf) &= ~5)