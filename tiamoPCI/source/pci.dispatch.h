//********************************************************************
//	created:	5:10:2008   6:55
//	file:		pci.dispatch.h
//	author:		tiamo
//	purpose:	dispatch
//********************************************************************

#pragma once

//
// dispatch irp
//
NTSTATUS PciDispatchIrp(__in PDEVICE_OBJECT DeviceObject,__in PIRP Irp);

//
// call down stack
//
NTSTATUS PciCallDownIrpStack(__in PPCI_COMMON_EXTENSION CommonExtension,__in PIRP Irp);

//
// pass to pdo
//
NTSTATUS PciPassIrpFromFdoToPdo(__in PPCI_COMMON_EXTENSION CommonExtension,__in PIRP Irp);

//
// set event
//
NTSTATUS PciSetEventCompletion(__in PDEVICE_OBJECT DeviceObject,__in PIRP Irp,__in PVOID Context);

//
// return not supported
//
NTSTATUS PciIrpNotSupported(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PVOID CommonExtension);

//
// return invalid request
//
NTSTATUS PciIrpInvalidDeviceRequest(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PVOID CommonExtension);