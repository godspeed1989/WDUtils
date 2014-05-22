//********************************************************************
//	created:	5:10:2008   7:28
//	file:		pci.devhere.h
//	author:		tiamo
//	purpose:	device present interface
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS devpresent_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntrf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS devpresent_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// reference
//
VOID devpresent_Reference(__in PPCI_PDO_EXTENSION PdoExt);

//
// dereference
//
VOID devpresent_Dereference(__in PPCI_PDO_EXTENSION PdoExt);

//
// is device present
//
BOOLEAN devpresent_IsDevicePresent(__in USHORT VendorID,__in USHORT DeviceID,__in UCHAR RevisionID,__in USHORT SubVendorID,__in USHORT SubSystemID,__in ULONG Flags);

//
// is device present
//
BOOLEAN devpresent_IsDevicePresentEx(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_DEVICE_PRESENCE_PARAMETERS Parameters);

//
// is device present on bus
//
BOOLEAN PcipDevicePresentOnBus(__in PPCI_FDO_EXTENSION ParentFdoExt,__in_opt PPCI_PDO_EXTENSION ChildPdoExt,__in PPCI_DEVICE_PRESENCE_PARAMETERS Parameters);