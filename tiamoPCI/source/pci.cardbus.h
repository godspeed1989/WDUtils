//********************************************************************
//	created:	5:10:2008   6:46
//	file:		pci.cardbus.h
//	author:		tiamo
//	purpose:	cardbus
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS pcicbintrf_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS pcicbintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// reference
//
VOID pcicbintrf_Reference(__in PDEVICE_OBJECT DeviceObject);

//
// dereference
//
VOID pcicbintrf_Dereference(__in PDEVICE_OBJECT DeviceObject);

//
// add cardbus
//
NTSTATUS pcicbintrf_AddCardBus(__in PDEVICE_OBJECT Pdo,__out PPCI_FDO_EXTENSION* NewFdoExt);

//
// delete cardbus
//
NTSTATUS pcicbintrf_DeleteCardBus(__in PPCI_FDO_EXTENSION FdoExt);

//
// dispatch pnp
//
NTSTATUS pcicbintrf_DispatchPnp(__in PPCI_FDO_EXTENSION FdoExt,__in PIRP Irp);

//
// get location
//
NTSTATUS pcicbintrf_GetLocation(__in PDEVICE_OBJECT Pdo,__out PUCHAR Bus,__out PUCHAR Device,__out PUCHAR Function,__out PBOOLEAN OnDebugPath);

//
// reset device
//
NTSTATUS Cardbus_ResetDevice(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config);

//
// get additional resource descriptor
//
VOID Cardbus_GetAdditionalResourceDescriptors(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in PIO_RESOURCE_DESCRIPTOR IoRes);

//
// massage header
//
VOID Cardbus_MassageHeaderForLimitsDetermination(__in PPCI_CONFIGURATOR_PARAM Param);

//
// restore current config
//
VOID Cardbus_RestoreCurrent(__in PPCI_CONFIGURATOR_PARAM Param);

//
// save limits
//
VOID Cardbus_SaveLimits(__in PPCI_CONFIGURATOR_PARAM Param);

//
// save current config
//
VOID Cardbus_SaveCurrentSettings(__in PPCI_CONFIGURATOR_PARAM Param);

//
// change resource config
//
VOID Cardbus_ChangeResourceSettings(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config);