//********************************************************************
//	created:	5:10:2008   7:30
//	file:		pci.routintf.h
//	author:		tiamo
//	purpose:	route interface
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS routeintrf_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntrf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS routeintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// reference
//
VOID routeintrf_Reference(__in PVOID Context);

//
// dereference
//
VOID routeintrf_Dereference(__in PVOID Context);

//
// get routing info
//
NTSTATUS PciGetInterruptRoutingInfoEx(__in PDEVICE_OBJECT Pdo,__out PULONG Bus,__out PULONG PciSlot,__out PUCHAR InterruptLine,
									  __out PUCHAR InterruptPin,__out PUCHAR ClassCode,__out PUCHAR SubClassCode,__out PDEVICE_OBJECT* ParentPdo,
									  __out PROUTING_TOKEN RoutingToken,PUCHAR Flags);

//
// set routing token
//
NTSTATUS PciSetRoutingTokenEx(__in PDEVICE_OBJECT Pdo,__in PROUTING_TOKEN RoutingToken);

//
// update interrupt line
//
VOID PciUpdateInterruptLine(__in PDEVICE_OBJECT Pdo,__in UCHAR LineRegister);

//
// get interrupt routing info
//
NTSTATUS PciGetInterruptRoutingInfo(__in PDEVICE_OBJECT Pdo,__out PULONG Bus,__out PULONG PciSlot,__out PUCHAR InterruptLine,__out PUCHAR InterruptPin,
									__out PUCHAR ClassCode,__out PUCHAR SubClassCode,__out PDEVICE_OBJECT* ParentPdo,__out PROUTING_TOKEN RoutingToken);

//
// set routing token
//
NTSTATUS PciSetRoutingToken(__in PDEVICE_OBJECT Pdo,__in PROUTING_TOKEN RoutingToken);

//
// cache legacy device routing
//
NTSTATUS PciCacheLegacyDeviceRouting(__in PDEVICE_OBJECT DeviceObject,__in ULONG BusNumber,__in ULONG SlotNumber,__in UCHAR InterruptLine,__in UCHAR InterruptPin,
									 __in UCHAR BaseClass,__in UCHAR SubClass,__in PDEVICE_OBJECT Pdo,__in PPCI_PDO_EXTENSION PdoExt,__out_opt PDEVICE_OBJECT* NewDO);

//
// set legacy device routing
//
NTSTATUS PciSetLegacyDeviceToken(__in PDEVICE_OBJECT Pdo,__in PROUTING_TOKEN RoutingToken);

//
// find legacy device
//
NTSTATUS PciFindLegacyDevice(__in PDEVICE_OBJECT Pdo,__out PULONG Bus,__out PULONG PciSlot,__out PUCHAR InterruptLine,__out PUCHAR InterruptPin,
							 __out PUCHAR ClassCode,__out PUCHAR SubClassCode,__out PDEVICE_OBJECT* ParentPdo,__out PROUTING_TOKEN RoutingToken);
