//********************************************************************
//	created:	5:10:2008   7:05
//	file:		pci.fdo.h
//	author:		tiamo
//	purpose:	fdo
//********************************************************************

#pragma once

//
// add device
//
NTSTATUS PciAddDevice(__in PDRIVER_OBJECT DriverObject,__in PDEVICE_OBJECT PhysicalDeviceObject);

//
// initialize fdo extension
//
VOID PciInitializeFdoExtensionCommonFields(__in PPCI_FDO_EXTENSION FdoExt,__in PDEVICE_OBJECT FunctionDeviceObject,__in PDEVICE_OBJECT PhyscialDeviceObject);

//
// get hotplug params
//
NTSTATUS PciGetHotPlugParameters(__in PPCI_FDO_EXTENSION FdoExt);

//
// start
//
NTSTATUS PciFdoIrpStartDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// query remove
//
NTSTATUS PciFdoIrpQueryRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// remove
//
NTSTATUS PciFdoIrpRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// cancel remove
//
NTSTATUS PciFdoIrpCancelRemoveDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// stop
//
NTSTATUS PciFdoIrpStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// query stop
//
NTSTATUS PciFdoIrpQueryStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// cancel stop
//
NTSTATUS PciFdoIrpCancelStopDevice(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// query relations
//
NTSTATUS PciFdoIrpQueryDeviceRelations(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// query interface
//
NTSTATUS PciFdoIrpQueryInterface(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// query caps
//
NTSTATUS PciFdoIrpQueryCapabilities(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// usage notification
//
NTSTATUS PciFdoIrpDeviceUsageNotification(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// surprise removal
//
NTSTATUS PciFdoIrpSurpriseRemoval(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// query legacy bus info
//
NTSTATUS PciFdoIrpQueryLegacyBusInformation(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);