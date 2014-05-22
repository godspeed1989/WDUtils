//********************************************************************
//	created:	5:10:2008   7:20
//	file:		pci.romimage.h
//	author:		tiamo
//	purpose:	read rom image
//********************************************************************

#pragma once

//
// read rom image
//
NTSTATUS PciReadRomImage(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG WhichSpace,__in PVOID Buffer,__in ULONG Offset,__inout PULONG Length);

//
// test write buffer
//
NTSTATUS PciRomTestWriteAccessToBuffer(__in PVOID Buffer,__in ULONG Length);

//
// read rom data
//
VOID PciTransferRomData(__in PUCHAR Register,__in PVOID Buffer,__in ULONG Length);