//********************************************************************
//	created:	5:10:2008   6:49
//	file:		pci.config.h
//	author:		tiamo
//	purpose:	config
//********************************************************************

#pragma once

//
// get config handlers
//
NTSTATUS PciGetConfigHandlers(__in PPCI_FDO_EXTENSION FdoExt);

//
// query pci bus interface
//
NTSTATUS PciQueryForPciBusInterface(__in PPCI_FDO_EXTENSION FdoExt);

//
// read device config
//
ULONG PciReadDeviceConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// write device config
//
ULONG PciWriteDeviceConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// read slot config
//
ULONG PciReadSlotConfig(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SLOT_NUMBER Slot,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// write slot config
//
ULONG PciWriteSlotConfig(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SLOT_NUMBER Slot,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// read/write config space
//
ULONG PciReadWriteConfigSpace(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SLOT_NUMBER Slot,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length,__in BOOLEAN Read);

//
// read device config
//
NTSTATUS PciExternalReadDeviceConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// write device config
//
NTSTATUS PciExternalWriteDeviceConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// get adjusted interrupt line
//
UCHAR PciGetAdjustedInterruptLine(__in PPCI_PDO_EXTENSION PdoExt);