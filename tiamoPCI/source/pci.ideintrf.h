//********************************************************************
//	created:	5:10:2008   7:24
//	file:		pci.ideintrf.h
//	author:		tiamo
//	purpose:	native ide interface
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS nativeIde_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntrf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS nativeIde_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// reference
//
VOID nativeIde_Reference(__in PPCI_PDO_EXTENSION PdoExt);

//
// dereference
//
VOID nativeIde_Dereference(__in PPCI_PDO_EXTENSION PdoExt);

//
// interrupt control
//
VOID nativeIde_InterruptControl(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN Enable);