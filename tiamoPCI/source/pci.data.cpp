//********************************************************************
//	created:	22:7:2008   16:12
//	file:		pci.data.cpp
//	author:		tiamo
//	purpose:	global data
//********************************************************************

#include "stdafx.h"

//
// driver object
//
PDRIVER_OBJECT											PciDriverObject;

//
// global lock
//
KEVENT													PciGlobalLock;

//
// fdo extension list
//
SINGLE_LIST_ENTRY										PciFdoExtensionListHead;

//
// root bus count
//
ULONG													PciRootBusCount;

//
// bus number lock
//
KEVENT													PciBusLock;

//
// lock device resources
//
BOOLEAN													PciLockDeviceResources;

//
// system wide hack flags
//
ULONG													PciSystemWideHackFlags;

//
// enable native ide
//
ULONG													PciEnableNativeModeATA;

//
// isa exclusion list
//
RTL_RANGE_LIST											PciIsaBitExclusionList;

//
// vga and isa exclusion list
//
RTL_RANGE_LIST											PciVgaAndIsaBitExclusionList;

//
// hack table
//
PPCI_HACK_TABLE_ENTRY									PciHackTable;

//
// irq routing table
//
PPCI_IRQ_ROUTING_TABLE_HEAD								PciIrqRoutingTable;

//
// debug ports
//
PCI_DEBUG_PORT											PciDebugPorts[2];

//
// debug ports count
//
ULONG													PciDebugPortsCount;

//
// old assign slot resources
//
pHalAssignSlotResources									PcipSavedAssignSlotResources;

//
// old translate bus address
//
pHalTranslateBusAddress									PcipSavedTranslateBusAddress;

//
// debug buffer
//
CHAR													PciDebugBuffer[0x101];

//
// debug level
//
ULONG													PciDebug;

//
// break on flags for pdo power irp
//
ULONG													PciBreakOnPdoPowerIrp;

//
// break on flags for fdo power irp
//
ULONG													PciBreakOnFdoPowerIrp;

//
// break on flags for pdo pnp irp
//
ULONG													PciBreakOnPdoPnpIrp;

//
// break on flags for fdo pnp irp
//
ULONG													PciBreakOnFdoPnpIrp;

//
// default bus number config used
//
BOOLEAN													PciDefaultConfigAlreadyUsed;

//
// we should assign bus number for bridges
//
BOOLEAN													PciAssignBusNumbers;

//
// pdo sequence number
//
LONG													PciPdoSequenceNumber = -1;

//
// zero requirements list
//
PIO_RESOURCE_REQUIREMENTS_LIST							PciZeroIoResourceRequirements;

//
// legacy device list
//
SINGLE_LIST_ENTRY										PciLegacyDeviceHead;

//
// verifier notification handle
//
PVOID													PciVerifierNotificationHandle;

//
// verifier registered
//
BOOLEAN													PciVerifierRegistered;

//
// bridge ordering
//
ARBITER_ORDERING										PciBridgeOrderings[2] = {0x10000,0xffffffffffffffff,0,0xffff};

//
// bridge ordering list
//
ARBITER_ORDERING_LIST									PciBridgeOrderingList = {2,2,PciBridgeOrderings};

//
// bus number arbiter interface
//
PCI_INTERFACE											ArbiterInterfaceBusNumber =
{
	&GUID_ARBITER_INTERFACE_STANDARD,
	sizeof(ARBITER_INTERFACE),
	0,
	0,
	PCI_INTERFACE_FLAGS_VALID_FOR_FDO,
	0,
	PciArb_BusNumber,
	&arbusno_Constructor,
	&arbusno_Initializer,
};

//
// memory arbiter interface
//
PCI_INTERFACE											ArbiterInterfaceMemory =
{
	&GUID_ARBITER_INTERFACE_STANDARD,
	sizeof(ARBITER_INTERFACE),
	0,
	0,
	PCI_INTERFACE_FLAGS_VALID_FOR_FDO,
	0,
	PciArb_Memory,
	&armem_Constructor,
	&armem_Initializer,
};

//
// io arbiter interface
//
PCI_INTERFACE											ArbiterInterfaceIo =
{
	&GUID_ARBITER_INTERFACE_STANDARD,
	sizeof(ARBITER_INTERFACE),
	0,
	0,
	PCI_INTERFACE_FLAGS_VALID_FOR_FDO,
	0,
	PciArb_Io,
	&ario_Constructor,
	&ario_Initializer,
};

//
// bus handler interface
//
PCI_INTERFACE											BusHandlerInterface =
{
	&GUID_BUS_INTERFACE_STANDARD,
	sizeof(BUS_INTERFACE_STANDARD),
	1,
	1,
	PCI_INTERFACE_FLAGS_VALID_FOR_PDO,
	0,
	PciInterface_BusHandler,
	&busintrf_Constructor,
	&busintrf_Initializer,
};

//
// interrupt route interface
//
PCI_INTERFACE											PciRoutingInterface =
{
	&GUID_INT_ROUTE_INTERFACE_STANDARD,
	sizeof(INT_ROUTE_INTERFACE_STANDARD),
	INT_ROUTE_INTRF_STANDARD_VER,
	INT_ROUTE_INTRF_STANDARD_VER,
	PCI_INTERFACE_FLAGS_VALID_FOR_FDO,
	0,
	PciInterface_IntRouteHandler,
	&routeintrf_Constructor,
	&routeintrf_Initializer,
};

//
// cardbus interface guid
//
GUID													GUID_PCI_CARDBUS_INTERFACE_PRIVATE = {0xcca82f31,0x54d6,0x11d1,0x82,0x24,0x00,0xa0,0xc9,0x32,0x43,0x85};

//
// cardbus interface
//
PCI_INTERFACE											PciCardbusPrivateInterface =
{
	&GUID_PCI_CARDBUS_INTERFACE_PRIVATE,
	sizeof(CARDBUS_PRIVATE_INTERFACE),
	PCI_CARDBUS_INTRF_STANDARD_VER,
	PCI_CARDBUS_INTRF_STANDARD_VER,
	PCI_INTERFACE_FLAGS_VALID_FOR_PDO,
	0,
	PciInterface_PciCb,
	&pcicbintrf_Constructor,
	&pcicbintrf_Initializer,
};

//
// legacy device detection interface
//
PCI_INTERFACE											PciLegacyDeviceDetectionInterface =
{
	&GUID_LEGACY_DEVICE_DETECTION_STANDARD,
	sizeof(LEGACY_DEVICE_DETECTION_INTERFACE),
	LEGACY_DEVICE_DETECTION_INTRF_STANDARD_VER,
	LEGACY_DEVICE_DETECTION_INTRF_STANDARD_VER,
	PCI_INTERFACE_FLAGS_VALID_FOR_FDO,
	0,
	PciInterface_LegacyDeviceDetection,
	&lddintrf_Constructor,
	&lddintrf_Initializer,
};

//
// PME interface guid
//
GUID													GUID_PCI_PME_INTERFACE = {0xaac7e6ac,0xbb0b,0x11d2,0xb4,0x84,0x00,0xc0,0x4f,0x72,0xde,0x8b};

//
// PME interface
//
PCI_INTERFACE											PciPmeInterface =
{
	&GUID_PCI_PME_INTERFACE,
	sizeof(PCI_PME_INTERFACE),
	PCI_PME_INTRF_STANDARD_VER,
	PCI_PME_INTRF_STANDARD_VER,
	PCI_INTERFACE_FLAGS_VALID_FOR_FDO | PCI_INTERFACE_FLAGS_ONLY_FOR_ROOT,
	0,
	PciInterface_PmeHandler,
	&PciPmeInterfaceConstructor,
	&PciPmeInterfaceInitializer,
};

//
// device present interface
//
PCI_INTERFACE											PciDevicePresentInterface =
{
	&GUID_PCI_DEVICE_PRESENT_INTERFACE,
	sizeof(PCI_DEVICE_PRESENT_INTERFACE),
	PCI_DEVICE_PRESENT_INTERFACE_VERSION,
	PCI_DEVICE_PRESENT_INTERFACE_VERSION,
	PCI_INTERFACE_FLAGS_VALID_FOR_PDO,
	0,
	PciInterface_DevicePresent,
	&devpresent_Constructor,
	&devpresent_Initializer,
};

//
// native ide interface guid
//
GUID													GUID_PCI_NATIVE_IDE_INTERFACE = {0x98f37d63,0x42ae,0x42d9,0x8c,0x36,0x93,0x2d,0x28,0x38,0x3d,0xf8};

//
// native ide interface
//
PCI_INTERFACE											PciNativeIdeInterface =
{
	&GUID_PCI_NATIVE_IDE_INTERFACE,
	sizeof(NATIVEIDE_INTERFACE),
	PCI_NATIVE_IDE_INTRF_STANDARD_VER,
	PCI_NATIVE_IDE_INTRF_STANDARD_VER,
	PCI_INTERFACE_FLAGS_VALID_FOR_PDO,
	0,
	PciInterface_NativeIde,
	&nativeIde_Constructor,
	&nativeIde_Initializer,
};

//
// agp target interface
//
PCI_INTERFACE											AgpTargetInterface =
{
	&GUID_AGP_TARGET_BUS_INTERFACE_STANDARD,
	sizeof(AGP_TARGET_BUS_INTERFACE_STANDARD),
	1,
	1,
	PCI_INTERFACE_FLAGS_VALID_FOR_PDO,
	0,
	PciInterface_AgpTarget,
	&agpintrf_Constructor,
	&agpintrf_Initializer,
};

//
// interrupt translator interface
//
PCI_INTERFACE											TranslatorInterfaceInterrupt =
{
	&GUID_TRANSLATOR_INTERFACE_STANDARD,
	sizeof(TRANSLATOR_INTERFACE),
	0,
	0,
	PCI_INTERFACE_FLAGS_VALID_FOR_FDO,
	0,
	PciTrans_Interrupt,
	&tranirq_Constructor,
	&tranirq_Initializer,
};

//
// pci interfaces
//
PPCI_INTERFACE											PciInterfaces[12] =
{
	&ArbiterInterfaceBusNumber,
	&ArbiterInterfaceMemory,
	&ArbiterInterfaceIo,
	&BusHandlerInterface,
	&PciRoutingInterface,
	&PciCardbusPrivateInterface,
	&PciLegacyDeviceDetectionInterface,
	&PciPmeInterface,
	&PciDevicePresentInterface,
	&PciNativeIdeInterface,
	&AgpTargetInterface,
	0,
};

//
// pci interfaces
//
PPCI_INTERFACE											PciInterfacesLastResort[2] =
{
	&TranslatorInterfaceInterrupt,
	0,
};

//
// pci configurators
//
PCI_CONFIGURATOR										PciConfigurators[3] =
{
	&Device_MassageHeaderForLimitsDetermination,
	&Device_RestoreCurrent,
	&Device_SaveLimits,
	&Device_SaveCurrentSettings,
	&Device_ChangeResourceSettings,
	&Device_GetAdditionalResourceDescriptors,
	&Device_ResetDevice,

	&PPBridge_MassageHeaderForLimitsDetermination,
	&PPBridge_RestoreCurrent,
	&PPBridge_SaveLimits,
	&PPBridge_SaveCurrentSettings,
	&PPBridge_ChangeResourceSettings,
	&PPBridge_GetAdditionalResourceDescriptors,
	&PPBridge_ResetDevice,

	&Cardbus_MassageHeaderForLimitsDetermination,
	&Cardbus_RestoreCurrent,
	&Cardbus_SaveLimits,
	&Cardbus_SaveCurrentSettings,
	&Cardbus_ChangeResourceSettings,
	&Cardbus_GetAdditionalResourceDescriptors,
	&Cardbus_ResetDevice,
};

//
// arbiter name
//
PCHAR													PciArbiterNames[4] =
{
	"I/O Port",
	"Memory",
	"Interrupt",
	"Bus Number",
};

//
// fdo dispatch table
//
PCI_MJ_DISPATCH_TABLE									PciFdoDispatchTable =
{
	IRP_MN_QUERY_LEGACY_BUS_INFORMATION,
	PciFdoDispatchPnpTable,
	IRP_MN_QUERY_POWER,
	PciFdoDispatchPowerTable,
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
};

//
// fdo pnp dispatch table
//
PCI_MN_DISPATCH_TABLE									PciFdoDispatchPnpTable[IRP_MN_QUERY_LEGACY_BUS_INFORMATION + 2] =
{
	IRP_UPWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpStartDevice),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpQueryRemoveDevice),
	IRP_DISPATCH,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpRemoveDevice),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpCancelRemoveDevice),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpStopDevice),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpQueryStopDevice),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpCancelStopDevice),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpQueryDeviceRelations),
	IRP_DISPATCH,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpQueryInterface),
	IRP_UPWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpQueryCapabilities),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_UPWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpDeviceUsageNotification),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpSurpriseRemoval),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpQueryLegacyBusInformation),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
};

//
// fdo power dispatch table
//
PCI_MN_DISPATCH_TABLE									PciFdoDispatchPowerTable[IRP_MN_QUERY_POWER + 2] =
{
	IRP_DISPATCH,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoWaitWake),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoSetPowerState),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciFdoIrpQueryPower),
	IRP_DOWNWARD,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
};

//
// pdo dispatch table
//
PCI_MJ_DISPATCH_TABLE									PciPdoDispatchTable =
{
	IRP_MN_QUERY_LEGACY_BUS_INFORMATION,
	PciPdoDispatchPnpTable,
	IRP_MN_QUERY_POWER,
	PciPdoDispatchPowerTable,
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpInvalidDeviceRequest),
};

//
// pdo pnp dispatch table
//
PCI_MN_DISPATCH_TABLE									PciPdoDispatchPnpTable[IRP_MN_QUERY_LEGACY_BUS_INFORMATION + 2] =
{
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpStartDevice),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryRemoveDevice),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpRemoveDevice),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpCancelRemoveDevice),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpStopDevice),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryStopDevice),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpCancelStopDevice),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryDeviceRelations),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryInterface),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryCapabilities),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryResources),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryResourceRequirements),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryDeviceText),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpReadConfig),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpWriteConfig),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryId),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryDeviceState),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryBusInformation),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpDeviceUsageNotification),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpSurpriseRemoval),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryLegacyBusInformation),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
};

//
// pdo power dispatch table
//
PCI_MN_DISPATCH_TABLE									PciPdoDispatchPowerTable[IRP_MN_QUERY_POWER + 2] =
{
	IRP_DISPATCH,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoWaitWake),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoSetPowerState),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciPdoIrpQueryPower),
	IRP_COMPLETE,
	reinterpret_cast<PCI_DISPATCH_ROUTINE>(&PciIrpNotSupported),
};

//
// state text
//
PCHAR													PciTransitionText[PciMaxObjectState + 1] =
{
	"PciNotStarted",
	"PciStarted",
	"PciDeleted",
	"PciStopped",
	"PciSurpriseRemoved",
	"PciSynchronizedOperation",
	"PciMaxObjectState",
};

//
// cancel state status
//
NTSTATUS												PnpStateCancelArray[PciMaxObjectState] =
{
	STATUS_INVALID_DEVICE_REQUEST,
	STATUS_FAIL_CHECK,
	STATUS_INVALID_DEVICE_STATE,
	STATUS_INVALID_DEVICE_STATE,
	STATUS_FAIL_CHECK,
	STATUS_FAIL_CHECK,
};

//
// transition state status
//
NTSTATUS												PnpStateTransitionArray[PciMaxObjectState][PciMaxObjectState] =
{
	{STATUS_FAIL_CHECK,				STATUS_SUCCESS,		STATUS_FAIL_CHECK,			STATUS_SUCCESS,		STATUS_FAIL_CHECK,			STATUS_FAIL_CHECK},
	{STATUS_SUCCESS,				STATUS_FAIL_CHECK,	STATUS_FAIL_CHECK,			STATUS_SUCCESS,		STATUS_FAIL_CHECK,			STATUS_FAIL_CHECK},
	{STATUS_SUCCESS,				STATUS_SUCCESS,		STATUS_FAIL_CHECK,			STATUS_FAIL_CHECK,	STATUS_SUCCESS,				STATUS_FAIL_CHECK},
	{STATUS_INVALID_DEVICE_REQUEST,	STATUS_SUCCESS,		STATUS_FAIL_CHECK,			STATUS_FAIL_CHECK,	STATUS_FAIL_CHECK,			STATUS_FAIL_CHECK},
	{STATUS_INVALID_DEVICE_REQUEST,	STATUS_SUCCESS,		STATUS_FAIL_CHECK,			STATUS_SUCCESS,		STATUS_FAIL_CHECK,			STATUS_FAIL_CHECK},
	{STATUS_SUCCESS,				STATUS_SUCCESS,		STATUS_INVALID_DEVICE_STATE,STATUS_SUCCESS,		STATUS_INVALID_DEVICE_STATE,STATUS_FAIL_CHECK},
};

//
// failure data
//
VERIFIER_FAILURE_DATA									PciVerifierFailureTable[4] = 
{
	{1,1,0,"The BIOS has reprogrammed the bus numbers of an active PCI device (!devstack %DevObj) during a dock or undock!"},
	{2,1,0,"A device in the system did not update it's PMCSR register in the spec mandated time (!devstack %DevObj, Power state D%Ulong)"},
	{3,1,0,"A driver controlling a PCI device has tried to access OS controlled configuration space registers (!devstack %DevObj, Offset 0x%Ulong1, Length 0x%Ulong2)"},
	{4,2,0,"A driver controlling a PCI device has tried to read or write from an invalid space using IRP_MN_READ/WRITE_CONFIG or via BUS_INTERFACE_STANDARD."
		   "NB: These functions take WhichSpace parameters of the form PCI_WHICHSPACE_* and not a BUS_DATA_TYPE (!devstack %DevObj, WhichSpace 0x%Ulong1)"},
};

//
// init guids
//
#include <initguid.h>
#include <wdmguid.h>