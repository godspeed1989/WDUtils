//********************************************************************
//	created:	5:10:2008   7:07
//	file:		pci.pdo.h
//	author:		tiamo
//	purpose:	pdo
//********************************************************************

#pragma once

//
// create pdo
//
NTSTATUS PciPdoCreate(__in PPCI_FDO_EXTENSION ParentFdoExt,__in PCI_SLOT_NUMBER Slot,__out PDEVICE_OBJECT* Pdo);

//
// destroy pdo
//
VOID PciPdoDestroy(__in PDEVICE_OBJECT Pdo);

//
// start
//
NTSTATUS PciPdoIrpStartDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query remove
//
NTSTATUS PciPdoIrpQueryRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// remove
//
NTSTATUS PciPdoIrpRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// cancel remove
//
NTSTATUS PciPdoIrpCancelRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// stop
//
NTSTATUS PciPdoIrpStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query stop
//
NTSTATUS PciPdoIrpQueryStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// cancel stop
//
NTSTATUS PciPdoIrpCancelStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query relations
//
NTSTATUS PciPdoIrpQueryDeviceRelations(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query interface
//
NTSTATUS PciPdoIrpQueryInterface(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query caps
//
NTSTATUS PciPdoIrpQueryCapabilities(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query resources
//
NTSTATUS PciPdoIrpQueryResources(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query resource requirements
//
NTSTATUS PciPdoIrpQueryResourceRequirements(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query text
//
NTSTATUS PciPdoIrpQueryDeviceText(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// read config
//
NTSTATUS PciPdoIrpReadConfig(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// write config
//
NTSTATUS PciPdoIrpWriteConfig(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query id
//
NTSTATUS PciPdoIrpQueryId(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query device state
//
NTSTATUS PciPdoIrpQueryDeviceState(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query bus info
//
NTSTATUS PciPdoIrpQueryBusInformation(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// usage notification
//
NTSTATUS PciPdoIrpDeviceUsageNotification(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// surprise removal
//
NTSTATUS PciPdoIrpSurpriseRemoval(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// query legacy bus info
//
NTSTATUS PciPdoIrpQueryLegacyBusInformation(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);