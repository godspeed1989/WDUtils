//********************************************************************
//	created:	5:10:2008   6:30
//	file:		pci.agpintrf.h
//	author:		tiamo
//	purpose:	agp interface
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS agpintrf_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS agpintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// reference interface
//
VOID agpintrf_Reference(__in PPCI_PDO_EXTENSION PdoExt);

//
// dereferenc interface
//
VOID agpintrf_Dereference(__in PPCI_PDO_EXTENSION PdoExt);

//
// write agp config
//
ULONG PciWriteAgpConfig(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG DataType,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);

//
// read agp config
//
ULONG PciReadAgpConfig(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG DataType,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length);