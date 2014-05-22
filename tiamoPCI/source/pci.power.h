//********************************************************************
//	created:	5:10:2008   7:15
//	file:		pci.power.h
//	author:		tiamo
//	purpose:	power
//********************************************************************

#pragma once

//
// fdo wait wake
//
NTSTATUS PciFdoWaitWake(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// set fdo power
//
NTSTATUS PciFdoSetPowerState(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// query fdo power
//
NTSTATUS PciFdoIrpQueryPower(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt);

//
// fdo wait wake complete
//
NTSTATUS PciFdoWaitWakeCompletion(__in PDEVICE_OBJECT DeviceObject,__in PIRP Irp,__in PVOID Context);

//
// fdo wait wake callback
//
VOID PciFdoWaitWakeCallBack(__in PDEVICE_OBJECT DeviceObject,__in UCHAR MinorFunction,__in POWER_STATE PowerState,__in_opt PVOID Context,__in PIO_STATUS_BLOCK IoStatus);

//
// fdo set power complete
//
VOID PciFdoSetPowerStateCompletion(__in PDEVICE_OBJECT DeviceObject,__in UCHAR Minor,__in POWER_STATE PowerState,__in_opt PVOID Context,__in PIO_STATUS_BLOCK IoStatus);

//
// pdo wait wake
//
NTSTATUS PciPdoWaitWake(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt);

//
// set pdo power
//
NTSTATUS PciPdoSetPowerState(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION FdoExt);

//
// query pdo power
//
NTSTATUS PciPdoIrpQueryPower(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION FdoExt);

//
// cancel pdo wait wake
//
VOID PciPdoWaitWakeCancelRoutine(__in PDEVICE_OBJECT DeviceObject,__in PIRP Irp);

//
// pdo wait wake callback
//
VOID PciPdoWaitWakeCallBack(__in PDEVICE_OBJECT DeviceObject,__in UCHAR MinorFunction,__in POWER_STATE PowerState,__in_opt PVOID Context,__in PIO_STATUS_BLOCK IoStatus);

//
// set device pme
//
NTSTATUS PciSetPowerManagedDevicePowerState(__in PPCI_PDO_EXTENSION PdoExt,__in DEVICE_POWER_STATE PowerState,__in BOOLEAN SetResource);

//
// delay for power change
//
NTSTATUS PciStallForPowerChange(__in PPCI_PDO_EXTENSION PdoExt,__in DEVICE_POWER_STATE PowerState,__in UCHAR PmCapsOffset);