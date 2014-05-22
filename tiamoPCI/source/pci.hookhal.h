//********************************************************************
//	created:	5:10:2008   6:53
//	file:		pci.hookhal.h
//	author:		tiamo
//	purpose:	hook hal
//********************************************************************

#pragma once

//
// hook
//
VOID PciHookHal();

//
// unhook
//
VOID PciUnhookHal();

//
// translate bus address
//
BOOLEAN  PciTranslateBusAddress(__in INTERFACE_TYPE  InterfaceType,__in ULONG BusNumber,__in PHYSICAL_ADDRESS BusAddress,
								__inout PULONG AddressSpace,__out PPHYSICAL_ADDRESS TranslatedAddress);

//
// assign slot resources
//
NTSTATUS PciAssignSlotResources(__in PUNICODE_STRING RegistryPath,__in_opt PUNICODE_STRING DriverClassName,
								__in PDRIVER_OBJECT DriverObject,__in PDEVICE_OBJECT DeviceObject,__in INTERFACE_TYPE BusType,
								__in ULONG BusNumber,__in ULONG SlotNumber,__inout PCM_RESOURCE_LIST *AllocatedResources);

//
// find pdo by location
//
PPCI_PDO_EXTENSION PciFindPdoByLocation(__in ULONG BusNumber,__in ULONG SlotNumber);