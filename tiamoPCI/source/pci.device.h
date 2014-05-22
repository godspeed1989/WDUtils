//********************************************************************
//	created:	5:10:2008   7:32
//	file:		pci.device.h
//	author:		tiamo
//	purpose:	device
//********************************************************************

#pragma once

//
// reset
//
NTSTATUS Device_ResetDevice(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config);

//
// get additional resource descriptor
//
VOID Device_GetAdditionalResourceDescriptors(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in PIO_RESOURCE_DESCRIPTOR IoRes);

//
// massage header
//
VOID Device_MassageHeaderForLimitsDetermination(__in PPCI_CONFIGURATOR_PARAM Param);

//
// restore current config
//
VOID Device_RestoreCurrent(__in PPCI_CONFIGURATOR_PARAM Param);

//
// save limits
//
VOID Device_SaveLimits(__in PPCI_CONFIGURATOR_PARAM Param);

//
// save current config
//
VOID Device_SaveCurrentSettings(__in PPCI_CONFIGURATOR_PARAM Param);

//
// change resource config
//
VOID Device_ChangeResourceSettings(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config);