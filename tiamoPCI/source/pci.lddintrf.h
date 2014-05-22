//********************************************************************
//	created:	5:10:2008   7:25
//	file:		pci.lddintrf.h
//	author:		tiamo
//	purpose:	legacy device detection
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS lddintrf_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntrf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS lddintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// reference
//
VOID lddintrf_Reference(__in PPCI_FDO_EXTENSION PdoExt);

//
// dereference
//
VOID lddintrf_Dereference(__in PPCI_FDO_EXTENSION PdoExt);

//
// detect legacy device
//
NTSTATUS PciLegacyDeviceDetection(__in PPCI_FDO_EXTENSION FdoExt,__in INTERFACE_TYPE LegacyBusType,__in ULONG BusNumber,__in ULONG SlotNumber,__out PDEVICE_OBJECT *Pdo);
