//********************************************************************
//	created:	26:7:2008   23:04
//	file:		pci.busintrf.cpp
//	author:		tiamo
//	purpose:	bus handler interface
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",busintrf_Constructor)
#pragma alloc_text("PAGE",busintrf_Initializer)
#pragma alloc_text("PAGE",busintrf_Reference)
#pragma alloc_text("PAGE",busintrf_Dereference)
#pragma alloc_text("PAGE",PciPnpTranslateBusAddress)
#pragma alloc_text("PAGE",PciPnpGetDmaAdapter)

//
// constructor [checked]
//
NTSTATUS busintrf_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
							  __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	Interface->Version									= 1;
	Interface->Context									= CommonExt;
	Interface->Size										= sizeof(BUS_INTERFACE_STANDARD);
	Interface->InterfaceDereference						= reinterpret_cast<PINTERFACE_DEREFERENCE>(&busintrf_Dereference);
	Interface->InterfaceReference						= reinterpret_cast<PINTERFACE_REFERENCE>(&busintrf_Reference);

	PBUS_INTERFACE_STANDARD BusInterface				= reinterpret_cast<PBUS_INTERFACE_STANDARD>(Interface);
	BusInterface->TranslateBusAddress					= reinterpret_cast<PTRANSLATE_BUS_ADDRESS>(&PciPnpTranslateBusAddress);
	BusInterface->GetDmaAdapter							= reinterpret_cast<PGET_DMA_ADAPTER>(&PciPnpGetDmaAdapter);
	BusInterface->SetBusData							= reinterpret_cast<PGET_SET_DEVICE_DATA>(&PciPnpWriteConfig);
	BusInterface->GetBusData							= reinterpret_cast<PGET_SET_DEVICE_DATA>(&PciPnpReadConfig);

	return STATUS_SUCCESS;
}

//
// initializer [checked]
//
NTSTATUS busintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	ASSERTMSG("PCI busintrf_Initializer, unexpected call.",FALSE);

	return STATUS_UNSUCCESSFUL;
}

//
// reference [checked]
//
VOID busintrf_Reference(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	InterlockedIncrement(&PdoExt->BusInterfaceReferenceCount);
}

//
// dereference [checked]
//
VOID busintrf_Dereference(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	InterlockedDecrement(&PdoExt->BusInterfaceReferenceCount);
}

//
// translate bus address [checked]
//
BOOLEAN PciPnpTranslateBusAddress(__in PPCI_PDO_EXTENSION PdoExt,__in PHYSICAL_ADDRESS BusAddress,__in ULONG Length,
								  __inout PULONG AddressSpace, __out PPHYSICAL_ADDRESS TranslatedAddress)
{
	PAGED_CODE();

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	return HalTranslateBusAddress(PCIBus,PdoExt->ParentFdoExtension->BaseBus,BusAddress,AddressSpace,TranslatedAddress);
}

//
// get dma adapter [checked]
//
PDMA_ADAPTER PciPnpGetDmaAdapter(__in PPCI_PDO_EXTENSION PdoExt,__in PDEVICE_DESCRIPTION DeviceDescriptor,__out PULONG NumberOfMapRegisters)
{
	PAGED_CODE();

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	if(DeviceDescriptor->InterfaceType == PCIBus)
		DeviceDescriptor->BusNumber						= PdoExt->ParentFdoExtension->BaseBus;

	return IoGetDmaAdapter(PdoExt->ParentFdoExtension->PhysicalDeviceObject,DeviceDescriptor,NumberOfMapRegisters);
}

//
// write config [checked]
//
ULONG PciPnpWriteConfig(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG DataType,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	PciWriteDeviceSpace(PdoExt,DataType,Buffer,Offset,Length,&Length);

	return Length;
}

//
// read config [checked]
//
ULONG PciPnpReadConfig(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG DataType,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	PciReadDeviceSpace(PdoExt,DataType,Buffer,Offset,Length,&Length);

	return Length;
}