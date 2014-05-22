//********************************************************************
//	created:	5:10:2008   7:18
//	file:		pci.id.h
//	author:		tiamo
//	purpose:	id
//********************************************************************

#pragma once

//
// query id
//
NTSTATUS PciQueryId(__in PPCI_PDO_EXTENSION PdoExt,__in BUS_QUERY_ID_TYPE Type,__out PWCHAR* IdBuffer);

//
// query device text
//
NTSTATUS PciQueryDeviceText(__in PPCI_PDO_EXTENSION PdoExt,__in DEVICE_TEXT_TYPE Type,__in LCID LocalId,__out PWCHAR* TextBuffer);

//
// get device description
//
PWCHAR PciGetDeviceDescriptionMessage(__in UCHAR BaseClass,__in UCHAR SubClass);

//
// get description
//
PWCHAR PciGetDescriptionMessage(__in ULONG ResourceId);

//
// init id buffer
//
VOID PciInitIdBuffer(__in PPCI_ID_BUFFER IdBuffer);

//
// id printf
//
VOID PciIdPrintf(__in PPCI_ID_BUFFER IdBuffer,__in PCHAR Format,...);

//
// append printf
//
VOID PciIdPrintfAppend(__in PPCI_ID_BUFFER IdBuffer,__in PCHAR Format,...);