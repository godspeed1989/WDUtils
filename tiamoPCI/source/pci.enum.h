//********************************************************************
//	created:	5:10:2008   7:10
//	file:		pci.enum.h
//	author:		tiamo
//	purpose:	enum
//********************************************************************

#pragma once

//
// query bus relations
//
NTSTATUS PciQueryDeviceRelations(__in PPCI_FDO_EXTENSION FdoExt,__inout PDEVICE_RELATIONS* Relations);

//
// query target relations
//
NTSTATUS PciQueryTargetDeviceRelations(__in PPCI_PDO_EXTENSION PdoExt,__inout PDEVICE_RELATIONS* Relations);

//
// query ejection relations
//
NTSTATUS PciQueryEjectionRelations(__in PPCI_PDO_EXTENSION PdoExt,__inout PDEVICE_RELATIONS* Relations);

//
// scan bus
//
NTSTATUS PciScanBus(__in PPCI_FDO_EXTENSION FdoExt);

//
// apply hacks
//
NTSTATUS PciApplyHacks(__in PPCI_FDO_EXTENSION FdoExt,__in PPCI_COMMON_HEADER PciConfig,__in PCI_SLOT_NUMBER Slot,__in ULONG Phase,__in PPCI_PDO_EXTENSION PdoExt);

//
// skip this function
//
BOOLEAN PciSkipThisFunction(__in PPCI_COMMON_HEADER Config,__in PCI_SLOT_NUMBER Slot,__in ULONG Type,__in ULARGE_INTEGER HackFlags);

//
// is the same device
//
BOOLEAN PcipIsSameDevice(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config);

//
// process bus
//
VOID PciProcessBus(__in PPCI_FDO_EXTENSION FdoExt);

//
// scan hibernated bus
//
VOID PciScanHibernatedBus(__in PPCI_FDO_EXTENSION FdoExt);

//
// query requirements
//
NTSTATUS PciQueryRequirements(__in PPCI_PDO_EXTENSION PdoExt,__out PIO_RESOURCE_REQUIREMENTS_LIST* IoResRequirementsList);

//
// build requirements list
//
NTSTATUS PciBuildRequirementsList(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__out PIO_RESOURCE_REQUIREMENTS_LIST* IoReqList);

//
// query resources
//
NTSTATUS PciQueryResources(__in PPCI_PDO_EXTENSION PdoExt,__out PCM_RESOURCE_LIST* CmResList);

//
// get enhanced caps
//
VOID PciGetEnhancedCapabilities(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config);

//
// get function limits
//
NTSTATUS PciGetFunctionLimits(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in ULARGE_INTEGER HackFlags);

//
// get function limits
//
NTSTATUS PcipGetFunctionLimits(__in PPCI_CONFIGURATOR_PARAM Param);

//
// compute new settings
//
BOOLEAN PciComputeNewCurrentSettings(__in PPCI_PDO_EXTENSION PdoExt,__in PCM_RESOURCE_LIST CmResList);

//
// write limits and restore current config
//
VOID PciWriteLimitsAndRestoreCurrent(__in PPCI_CONFIGURATOR_PARAM Param);

//
//set resources
//
NTSTATUS PciSetResources(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN ResetDevice,__in BOOLEAN AssignResource);

//
// configure ide controller
//
BOOLEAN PciConfigureIdeController(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in BOOLEAN DisableIoDecode);

//
// allocate cm resource list
//
PCM_RESOURCE_LIST PciAllocateCmResourceList(__in ULONG Count,__in UCHAR BusNumber);

//
// allocate io resource list
//
PIO_RESOURCE_REQUIREMENTS_LIST PciAllocateIoRequirementsList(__in ULONG Count,__in UCHAR BusNumber,__in ULONG Slot);

//
// get in use ranges
//
VOID PciGetInUseRanges(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc);

//
// build graduated window
//
VOID PciBuildGraduatedWindow(__in PIO_RESOURCE_DESCRIPTOR PrototypeDesc,__in ULONG Window,__in ULONG Count,__in PIO_RESOURCE_DESCRIPTOR OutputDesc);

//
// initialize private resource descriptor
//
VOID PciPrivateResourceInitialize(__in PIO_RESOURCE_DESCRIPTOR Desc,__in ULONG Type,__in ULONG Index);