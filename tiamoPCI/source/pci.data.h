//********************************************************************
//	created:	22:7:2008   12:05
//	file:		pci.data.h
//	author:		tiamo
//	purpose:	global data
//********************************************************************

#pragma once

//
// driver object
//
extern PDRIVER_OBJECT									PciDriverObject;

//
// global lock
//
extern KEVENT											PciGlobalLock;

//
// fdo extension list
//
extern SINGLE_LIST_ENTRY								PciFdoExtensionListHead;

//
// root bus count
//
extern ULONG											PciRootBusCount;

//
// bus number lock
//
extern KEVENT											PciBusLock;

//
// lock device resources
//
extern BOOLEAN											PciLockDeviceResources;

//
// system wide hack flags
//
extern ULONG											PciSystemWideHackFlags;

//
// enable native ide
//
extern ULONG											PciEnableNativeModeATA;

//
// isa exclusion list
//
extern RTL_RANGE_LIST									PciIsaBitExclusionList;

//
// vga and isa exclusion list
//
extern RTL_RANGE_LIST									PciVgaAndIsaBitExclusionList;

//
// hack table
//
extern PPCI_HACK_TABLE_ENTRY							PciHackTable;

//
// irq routing table
//
extern PPCI_IRQ_ROUTING_TABLE_HEAD						PciIrqRoutingTable;

//
// debug ports
//
extern PCI_DEBUG_PORT									PciDebugPorts[2];

//
// debug ports count
//
extern ULONG											PciDebugPortsCount;

//
// old assign slot resources
//
extern pHalAssignSlotResources							PcipSavedAssignSlotResources;

//
// old translate bus address
//
extern pHalTranslateBusAddress							PcipSavedTranslateBusAddress;

//
// debug buffer
//
extern CHAR												PciDebugBuffer[0x101];

//
// debug level
//
extern ULONG											PciDebug;

//
// break on flags for pdo power irp
//
extern ULONG											PciBreakOnPdoPowerIrp;

//
// break on flags for fdo power irp
//
extern ULONG											PciBreakOnFdoPowerIrp;

//
// break on flags for pdo pnp irp
//
extern ULONG											PciBreakOnPdoPnpIrp;

//
// break on flags for fdo pnp irp
//
extern ULONG											PciBreakOnFdoPnpIrp;

//
// default bus number config used
//
extern BOOLEAN											PciDefaultConfigAlreadyUsed;

//
// we should assign bus number for bridges
//
extern BOOLEAN											PciAssignBusNumbers;

//
// pci interfaces
//
extern PPCI_INTERFACE									PciInterfaces[12];

//
// pci interfaces
//
extern PPCI_INTERFACE									PciInterfacesLastResort[2];

//
// arbiter name
//
extern PCHAR											PciArbiterNames[4];

//
// fdo dispatch table
//
extern PCI_MJ_DISPATCH_TABLE							PciFdoDispatchTable;

//
// fdo pnp dispatch table
//
extern PCI_MN_DISPATCH_TABLE							PciFdoDispatchPnpTable[IRP_MN_QUERY_LEGACY_BUS_INFORMATION + 2];

//
// fdo power dispatch table
//
extern PCI_MN_DISPATCH_TABLE							PciFdoDispatchPowerTable[IRP_MN_QUERY_POWER + 2];

//
// pdo dispatch table
//
extern PCI_MJ_DISPATCH_TABLE							PciPdoDispatchTable;

//
// pdo pnp dispatch table
//
extern PCI_MN_DISPATCH_TABLE							PciPdoDispatchPnpTable[IRP_MN_QUERY_LEGACY_BUS_INFORMATION + 2];

//
// pdo power dispatch table
//
extern PCI_MN_DISPATCH_TABLE							PciPdoDispatchPowerTable[IRP_MN_QUERY_POWER + 2];

//
// state text
//
extern PCHAR											PciTransitionText[PciMaxObjectState + 1];

//
// cancel state status
//
extern NTSTATUS											PnpStateCancelArray[PciMaxObjectState];

//
// state transition status
//
extern NTSTATUS											PnpStateTransitionArray[PciMaxObjectState][PciMaxObjectState];

//
// pdo sequence number
//
extern LONG												PciPdoSequenceNumber;

//
// configurator
//
extern PCI_CONFIGURATOR									PciConfigurators[3];

//
// zero requirements list
//
extern PIO_RESOURCE_REQUIREMENTS_LIST					PciZeroIoResourceRequirements;

//
// legacy device list
//
extern SINGLE_LIST_ENTRY								PciLegacyDeviceHead;

//
// bridge ordering list
//
extern ARBITER_ORDERING_LIST							PciBridgeOrderingList;

//
// failure data
//
extern VERIFIER_FAILURE_DATA							PciVerifierFailureTable[4];

//
// verifier notification handle
//
extern PVOID											PciVerifierNotificationHandle;

//
// verifier registered
//
extern BOOLEAN											PciVerifierRegistered;