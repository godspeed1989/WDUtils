//********************************************************************
//	created:	5:10:2008   7:20
//	file:		pci.usage.h
//	author:		tiamo
//	purpose:	device usage
//********************************************************************

#pragma once

//
// local usage
//
NTSTATUS PciLocalDeviceUsage(__in PPCI_POWER_STATE PowerState,__in PIRP Irp);

//
// pdo usage
//
NTSTATUS PciPdoDeviceUsage(__in PPCI_PDO_EXTENSION PdoExt,__in PIRP Irp);