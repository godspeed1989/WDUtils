//********************************************************************
//	created:	5:10:2008   6:44
//	file:		pci.busno.h
//	author:		tiamo
//	purpose:	bus number
//********************************************************************

#pragma once

//
// bus numbers are configured
//
BOOLEAN PciAreBusNumbersConfigured(__in PPCI_PDO_EXTENSION PdoExt);

//
// set bus numbers for bridges
//
NTSTATUS PciSetBusNumbers(__in PPCI_PDO_EXTENSION PdoExt,__in UCHAR Primary,__in UCHAR Secondary,__in UCHAR Subordinate);

//
// disable pci bridge
//
VOID PciDisableBridge(__in PPCI_PDO_EXTENSION PdoExt);

//
// configure bus numbers
//
VOID PciConfigureBusNumbers(__in PPCI_FDO_EXTENSION FdoExt);

//
// spread child bridges
//
VOID PciSpreadBridges(__in PPCI_FDO_EXTENSION ParentFdoExt,__in ULONG BridgeCount);

//
// fit child bridges
//
VOID PciFitBridge(__in PPCI_FDO_EXTENSION ParentFdoExt,__in PPCI_PDO_EXTENSION BridgePdoExt);

//
// update subordinate bus numbers
//
VOID PciUpdateAncestorSubordinateBuses(__in PPCI_FDO_EXTENSION FdoExt,__in UCHAR MaxBus);

//
// find bus number limit
//
UCHAR PciFindBridgeNumberLimit(__in PPCI_FDO_EXTENSION FdoExt,__in UCHAR BaseBus);

//
// find bus number limit worker
//
UCHAR PciFindBridgeNumberLimitWorker(__in PPCI_FDO_EXTENSION StartFdoExt,__in PPCI_FDO_EXTENSION CurFdoExt,__in UCHAR BaseBus,__out PBOOLEAN Include);