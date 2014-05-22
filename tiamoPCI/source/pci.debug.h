//********************************************************************
//	created:	5:10:2008   7:36
//	file:		pci.debug.h
//	author:		tiamo
//	purpose:	debug
//********************************************************************

#pragma once

//
// debug printf
//
VOID PciDebugPrintf(__in ULONG Level,__in PCHAR Format,...);

//
// debug irp
//
BOOLEAN PciDebugIrpDispatchDisplay(__in PIO_STACK_LOCATION IrpSp,__in PPCI_COMMON_EXTENSION CommonExtension,__in ULONG MaxFunction);

//
// get power irp text
//
PCHAR PciDebugPoIrpTypeToText(__in UCHAR Minor);

//
// get pnp irp text
//
PCHAR PciDebugPnpIrpTypeToText(__in UCHAR Minor);

//
// print cm res list
//
VOID PciDebugPrintCmResList(__in ULONG Level,__in PCM_RESOURCE_LIST CmResList);

//
// print partial res
//
VOID PciDebugPrintPartialResource(__in ULONG Level,__in PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDesc);

//
// get resource type text
//
PCHAR PciDebugCmResourceTypeToText(__in UCHAR Type);

//
// print io res list
//
VOID PciDebugPrintIoResReqList(__in PIO_RESOURCE_REQUIREMENTS_LIST IoReqList);

//
// print io res
//
VOID PciDebugPrintIoResource(__in PIO_RESOURCE_DESCRIPTOR Desc);

//
// print pci config
//
VOID PciDebugDumpCommonConfig(__in PPCI_COMMON_HEADER Config);

//
// print caps
//
VOID PciDebugDumpQueryCapabilities(__in PDEVICE_CAPABILITIES Capabilities);