//********************************************************************
//	created:	5:10:2008   7:21
//	file:		pci.pmeintf.h
//	author:		tiamo
//	purpose:	pme interface
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS PciPmeInterfaceConstructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Intf);

//
// initializer
//
NTSTATUS PciPmeInterfaceInitializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// reference
//
VOID PmeInterfaceReference(__in PVOID Context);

//
// dereferenc
//
VOID PmeInterfaceDereference(__in PVOID Context);

//
// adjuste pme enable
//
VOID PciPdoAdjustPmeEnable(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN Enable);

//
// update pme
//
VOID PciPmeUpdateEnable(__in PDEVICE_OBJECT Pdo,__in BOOLEAN Enable);

//
// clear pme status
//
VOID PciPmeClearPmeStatus(__in PDEVICE_OBJECT Pdo);

//
// adjust pme
//
VOID PciPmeAdjustPmeEnable(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN Enable,__in BOOLEAN ClearPmeStatus);

//
// get pme info
//
VOID PciPmeGetInformation(__in PDEVICE_OBJECT Pdo,__out_opt PBOOLEAN HasPowerMgrCaps,__out_opt PBOOLEAN PmeAsserted,__out_opt PBOOLEAN PmeEnabled);