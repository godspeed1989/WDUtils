//********************************************************************
//	created:	5:10:2008   7:34
//	file:		pci.ppbridge.h
//	author:		tiamo
//	purpose:	pci-to-pci bridge
//********************************************************************

#pragma once

//
// reset
//
NTSTATUS PPBridge_ResetDevice(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config);

//
// get additional resource descriptors
//
VOID PPBridge_GetAdditionalResourceDescriptors(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in PIO_RESOURCE_DESCRIPTOR IoRes);

//
// massage header
//
VOID PPBridge_MassageHeaderForLimitsDetermination(__in PPCI_CONFIGURATOR_PARAM Param);

//
// restore current config
//
VOID PPBridge_RestoreCurrent(__in PPCI_CONFIGURATOR_PARAM Param);

//
// save limits
//
VOID PPBridge_SaveLimits(__in PPCI_CONFIGURATOR_PARAM Param);

//
// save current config
//
VOID PPBridge_SaveCurrentSettings(__in PPCI_CONFIGURATOR_PARAM Param);

//
// change resource config
//
VOID PPBridge_ChangeResourceSettings(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config);

//
// is positive decode
//
BOOLEAN PciBridgeIsPositiveDecode(__in PPCI_PDO_EXTENSION PdoExt);

//
// is subtractive decode
//
BOOLEAN PciBridgeIsSubtractiveDecode(__in PPCI_CONFIGURATOR_PARAM Param);

//
// get memory base
//
ULONG PciBridgeMemoryBase(__in PPCI_COMMON_HEADER Config);

//
// get meory limit
//
ULONG PciBridgeMemoryLimit(__in PPCI_COMMON_HEADER Config);

//
// get io base
//
ULONG PciBridgeIoBase(__in PPCI_COMMON_HEADER Config);

//
// get io limit
//
ULONG PciBridgeIoLimit(__in PPCI_COMMON_HEADER Config);

//
// get prefetch memory base
//
LONGLONG PciBridgePrefetchMemoryBase(__in PPCI_COMMON_HEADER Config);

//
// get prefetch memory limit
//
LONGLONG PciBridgePrefetchMemoryLimit(__in PPCI_COMMON_HEADER Config);

//
// compute alignment
//
ULONG PciBridgeMemoryWorstCaseAlignment(__in ULONG Length);