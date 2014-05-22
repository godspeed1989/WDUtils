//********************************************************************
//	created:	5:10:2008   6:42
//	file:		pci.busintrf.h
//	author:		tiamo
//	purpose:	bus handler interface
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS busintrf_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS busintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// reference
//
VOID busintrf_Reference(__in PPCI_PDO_EXTENSION PdoExt);

//
// dereference
//
VOID busintrf_Dereference(__in PPCI_PDO_EXTENSION PdoExt);

//
// translate bus address
//
BOOLEAN PciPnpTranslateBusAddress(__in PPCI_PDO_EXTENSION PdoExt,__in PHYSICAL_ADDRESS BusAddress,__in ULONG Length,
								  __inout PULONG AddressSpace, __out PPHYSICAL_ADDRESS TranslatedAddress);

//
// get dma adapter
//
PDMA_ADAPTER PciPnpGetDmaAdapter(__in PPCI_PDO_EXTENSION PdoExt,__in PDEVICE_DESCRIPTION DeviceDescriptor,__out PULONG NumberOfMapRegisters);

//
// write config space
//
ULONG PciPnpWriteConfig(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG DataType,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// read config space
//
ULONG PciPnpReadConfig(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG DataType,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);