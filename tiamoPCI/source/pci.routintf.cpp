//********************************************************************
//	created:	27:7:2008   2:49
//	file:		pci.routintf.cpp
//	author:		tiamo
//	purpose:	route interface
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",routeintrf_Constructor)
#pragma alloc_text("PAGE",routeintrf_Initializer)
#pragma alloc_text("PAGE",routeintrf_Reference)
#pragma alloc_text("PAGE",routeintrf_Dereference)
#pragma alloc_text("PAGE",PciGetInterruptRoutingInfoEx)
#pragma alloc_text("PAGE",PciSetRoutingTokenEx)
#pragma alloc_text("PAGE",PciUpdateInterruptLine)
#pragma alloc_text("PAGE",PciCacheLegacyDeviceRouting)
#pragma alloc_text("PAGE",PciSetLegacyDeviceToken)

//
// constructor [checked]
//
NTSTATUS routeintrf_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
								__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	if(Version != INT_ROUTE_INTRF_STANDARD_VER)
		return STATUS_NOINTERFACE;

	Interface->Version									= Version;
	Interface->Context									= CommonExt;
	Interface->Size										= sizeof(INT_ROUTE_INTERFACE_STANDARD);
	Interface->InterfaceDereference						= reinterpret_cast<PINTERFACE_DEREFERENCE>(&routeintrf_Dereference);
	Interface->InterfaceReference						= reinterpret_cast<PINTERFACE_REFERENCE>(&routeintrf_Reference);

	PINT_ROUTE_INTERFACE_STANDARD RouteInterface		= reinterpret_cast<PINT_ROUTE_INTERFACE_STANDARD>(Interface);
	RouteInterface->GetInterruptRouting					= reinterpret_cast<PGET_INTERRUPT_ROUTING>(&PciGetInterruptRoutingInfoEx);
	RouteInterface->SetInterruptRoutingToken			= reinterpret_cast<PSET_INTERRUPT_ROUTING_TOKEN>(&PciSetRoutingTokenEx);
	RouteInterface->UpdateInterruptLine					= reinterpret_cast<PUPDATE_INTERRUPT_LINE>(&PciUpdateInterruptLine);

	return STATUS_SUCCESS;
}

//
// initializer [checked]
//
NTSTATUS routeintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	ASSERTMSG("PCI routeintrf_Initializer, unexpected call.",FALSE);

	return STATUS_UNSUCCESSFUL;
}

//
// reference [checked]
//
VOID routeintrf_Reference(__in PVOID Context)
{
	PAGED_CODE();
}

//
// dereference [checked]
//
VOID routeintrf_Dereference(__in PVOID Context)
{
	PAGED_CODE();
}

//
// get interrupt routing info [checked]
//
NTSTATUS PciGetInterruptRoutingInfoEx(__in PDEVICE_OBJECT Pdo,__out PULONG Bus,__out PULONG PciSlot,__out PUCHAR InterruptLine,__out PUCHAR InterruptPin,
									  __out PUCHAR ClassCode,__out PUCHAR SubClassCode,__out PDEVICE_OBJECT* ParentPdo,__out PROUTING_TOKEN RoutingToken,PUCHAR Flags)
{
	PAGED_CODE();

	*Flags												= 0;
	return PciGetInterruptRoutingInfo(Pdo,Bus,PciSlot,InterruptLine,InterruptPin,ClassCode,SubClassCode,ParentPdo,RoutingToken);
}

//
// set routing token [checked]
//
NTSTATUS PciSetRoutingTokenEx(__in PDEVICE_OBJECT Pdo,__in PROUTING_TOKEN RoutingToken)
{
	PAGED_CODE();

	return PciSetRoutingToken(Pdo,RoutingToken);
}

//
// update interrupt line [checked]
//
VOID PciUpdateInterruptLine(__in PDEVICE_OBJECT Pdo,__in UCHAR LineRegister)
{
	PAGED_CODE();

	PPCI_LEGACY_DEVICE_INFO Info						= CONTAINING_RECORD(PciLegacyDeviceHead.Next,PCI_LEGACY_DEVICE_INFO,ListEntry);
	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);

	while(Info)
	{
		if(Info->OwnerDevice == Pdo)
		{
			PdoExt										= Info->PdoExt;
			break;
		}
		Info											= CONTAINING_RECORD(Info->ListEntry.Next,PCI_LEGACY_DEVICE_INFO,ListEntry);
	}

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	//
	// set raw/adjusted interrupt line
	//
	PdoExt->AdjustedInterruptLine						= LineRegister;
	PdoExt->RawInterruptLine							= LineRegister;

	//
	// and program hardware
	//
	PciWriteDeviceConfig(PdoExt,&LineRegister,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.InterruptLine),sizeof(LineRegister));

	//
	// update registry data
	//
	PCI_COMMON_HEADER BiosConfig;
	NTSTATUS Status										= PciGetBiosConfig(PdoExt,&BiosConfig);
	ASSERT(NT_SUCCESS(Status));

	if(BiosConfig.u.type0.InterruptLine != LineRegister)
	{
		Status											= PciSaveBiosConfig(PdoExt,&BiosConfig);
		ASSERT(NT_SUCCESS(Status));
	}
}

//
// get routing info [checked]
//
NTSTATUS PciGetInterruptRoutingInfo(__in PDEVICE_OBJECT Pdo,__out PULONG Bus,__out PULONG PciSlot,__out PUCHAR InterruptLine,__out PUCHAR InterruptPin,
									__out PUCHAR ClassCode,__out PUCHAR SubClassCode,__out PDEVICE_OBJECT* ParentPdo,__out PROUTING_TOKEN RoutingToken)
{
	PAGED_CODE();

	ASSERT(Bus);
	ASSERT(PciSlot);
	ASSERT(InterruptLine);
	ASSERT(InterruptPin);
	ASSERT(ClassCode);
	ASSERT(SubClassCode);
	ASSERT(ParentPdo);
	ASSERT(RoutingToken);

	//
	// search legacy devices with this pdo
	//
	NTSTATUS Status										= PciFindLegacyDevice(Pdo,Bus,PciSlot,InterruptLine,InterruptPin,ClassCode,SubClassCode,ParentPdo,RoutingToken);
	if(NT_SUCCESS(Status))
		return Status;

	//
	// not found in legacy device list,get those info from pdo ext
	//
	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);
	if(!PdoExt || PdoExt->Common.ExtensionType != PciPdoExtensionType)
		return STATUS_NOT_FOUND;

	*Bus												= PdoExt->ParentFdoExtension->BaseBus;
	*PciSlot											= PdoExt->Slot.u.AsULONG;
	*InterruptLine										= PdoExt->RawInterruptLine;
	*InterruptPin										= PdoExt->InterruptPin;
	*ClassCode											= PdoExt->BaseClass;
	*SubClassCode										= PdoExt->SubClass;
	*ParentPdo											= PdoExt->ParentFdoExtension->PhysicalDeviceObject;

	PPCI_SECONDARY_EXTENSION SecondaryExt				= PciFindNextSecondaryExtension(PdoExt->SecondaryExtension.Next,PciInterface_IntRouteHandler);
	if(SecondaryExt)
		RtlCopyMemory(RoutingToken,SecondaryExt + 1,sizeof(ROUTING_TOKEN));
	else
		RtlZeroMemory(RoutingToken,sizeof(ROUTING_TOKEN));

	return STATUS_SUCCESS;
}

//
// set token [checked]
//
NTSTATUS PciSetRoutingToken(__in PDEVICE_OBJECT Pdo,__in PROUTING_TOKEN RoutingToken)
{
	PAGED_CODE();

	//
	// try to set legacy device token
	//
	if(NT_SUCCESS(PciSetLegacyDeviceToken(Pdo,RoutingToken)))
		return STATUS_SUCCESS;

	//
	// is not a legacy device,build an IntRouteHandler extension
	//
	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);

	if(PciFindNextSecondaryExtension(PdoExt->SecondaryExtension.Next,PciInterface_IntRouteHandler))
	{
		DbgPrint("PCI:  *** redundant PCI routing extesion being created ***\n");
		ASSERT(FALSE);
	}

	ULONG Length										= sizeof(PCI_INTTERUPT_ROUTING_SECONDARY_EXTENSION);
	PPCI_INTTERUPT_ROUTING_SECONDARY_EXTENSION IntExt	= static_cast<PPCI_INTTERUPT_ROUTING_SECONDARY_EXTENSION>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
	if(!IntExt)
		return STATUS_INSUFFICIENT_RESOURCES;

	RtlZeroMemory(IntExt,Length);
	RtlCopyMemory(&IntExt->RoutingToken,RoutingToken,sizeof(ROUTING_TOKEN));

	//
	// link it to the device
	//
	PcipLinkSecondaryExtension(&PdoExt->SecondaryExtension,&IntExt->SecondaryExtension,&PdoExt->Common.SecondaryExtLock,PciInterface_IntRouteHandler,0);

	return STATUS_SUCCESS;
}

//
// cache legacy device routing [checked]
//
NTSTATUS PciCacheLegacyDeviceRouting(__in PDEVICE_OBJECT DeviceObject,__in ULONG BusNumber,__in ULONG SlotNumber,__in UCHAR InterruptLine,__in UCHAR InterruptPin,
									 __in UCHAR BaseClass,__in UCHAR SubClass,__in PDEVICE_OBJECT Pdo,__in PPCI_PDO_EXTENSION PdoExt,__out_opt PDEVICE_OBJECT* NewDO)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;

	PSINGLE_LIST_ENTRY Prev								= &PciLegacyDeviceHead;
	PSINGLE_LIST_ENTRY Current							= PciLegacyDeviceHead.Next;

	while(Current)
	{
		PPCI_LEGACY_DEVICE_INFO Info					= CONTAINING_RECORD(Current,PCI_LEGACY_DEVICE_INFO,ListEntry);
		if(Info->BusNumber == BusNumber && Info->SlotNumber == SlotNumber)
		{
			if(Info->OwnerDevice == DeviceObject)
			{
				if(NewDO)
					*NewDO								= DeviceObject;
			}
			else
			{
				if(NewDO)
					*NewDO								= Info->OwnerDevice;

				Info->OwnerDevice						= DeviceObject;
			}

			return STATUS_SUCCESS;
		}
		else
		{
			if(Info->OwnerDevice == DeviceObject)
			{
				Prev->Next								= Current->Next;
				ExFreePool(Current);

				Current									= Prev->Next;
			}
			else
			{
				Prev									= Current;
				Current									= Current->Next;
			}
		}
	}

	PPCI_LEGACY_DEVICE_INFO Info						= static_cast<PPCI_LEGACY_DEVICE_INFO>(ExAllocatePoolWithTag(PagedPool,sizeof(PCI_LEGACY_DEVICE_INFO),'BicP'));
	if(!Info)
		return STATUS_INSUFFICIENT_RESOURCES;

	RtlZeroMemory(Info,sizeof(PCI_LEGACY_DEVICE_INFO));
	Info->BaseClass										= BaseClass;
	Info->BusNumber										= BusNumber;
	Info->InterruptLine									= InterruptLine;
	Info->InterruptPin									= InterruptPin;
	Info->OwnerDevice									= DeviceObject;
	Info->ParentPdo										= Pdo;
	Info->PdoExt										= PdoExt;
	Info->SlotNumber									= SlotNumber;
	Info->SubClass										= SubClass;

	Info->ListEntry.Next								= PciLegacyDeviceHead.Next;
	PciLegacyDeviceHead.Next							= &Info->ListEntry;

	if(NewDO)
		*NewDO											= DeviceObject;

	return STATUS_SUCCESS;
}

//
// set legacy device token [checked]
//
NTSTATUS PciSetLegacyDeviceToken(__in PDEVICE_OBJECT Pdo,__in PROUTING_TOKEN RoutingToken)
{
	PAGED_CODE();

	PPCI_LEGACY_DEVICE_INFO Info						= CONTAINING_RECORD(PciLegacyDeviceHead.Next,PCI_LEGACY_DEVICE_INFO,ListEntry);
	while(Info)
	{
		if(Info->OwnerDevice == Pdo)
			break;

		Info											= CONTAINING_RECORD(Info->ListEntry.Next,PCI_LEGACY_DEVICE_INFO,ListEntry);
	}

	if(!Info)
		return STATUS_NOT_FOUND;

	RtlCopyMemory(&Info->RoutingToken,RoutingToken,sizeof(ROUTING_TOKEN));

	return STATUS_SUCCESS;
}

//
// find legacy device [checked]
//
NTSTATUS PciFindLegacyDevice(__in PDEVICE_OBJECT Pdo,__out PULONG Bus,__out PULONG PciSlot,__out PUCHAR InterruptLine,__out PUCHAR InterruptPin,
							 __out PUCHAR ClassCode,__out PUCHAR SubClassCode,__out PDEVICE_OBJECT* ParentPdo,__out PROUTING_TOKEN RoutingToken)
{
	PPCI_LEGACY_DEVICE_INFO Info						= CONTAINING_RECORD(PciLegacyDeviceHead.Next,PCI_LEGACY_DEVICE_INFO,ListEntry);
	while(Info)
	{
		if(Info->OwnerDevice == Pdo)
			break;

		if(Info->BusNumber == *Bus && Info->SlotNumber == *PciSlot)
		{
			ASSERT(!Info->OwnerDevice);

			if(Info->OwnerDevice)
			{
				PciDebugPrintf(0,"Two PDOs (Legacy = %08x, Pnp = %08x) for device on bus %08x, slot 0x08x\n",Info->OwnerDevice,Pdo,*Bus,*PciSlot);
				Info									= 0;
			}
			else
			{
				Info->OwnerDevice						= Pdo;
			}

			break;
		}

		Info											= CONTAINING_RECORD(Info->ListEntry.Next,PCI_LEGACY_DEVICE_INFO,ListEntry);
	}

	if(!Info)
		return STATUS_NOT_FOUND;

	*Bus												= Info->BusNumber;
	*PciSlot											= Info->SlotNumber;
	*InterruptLine										= Info->InterruptLine;
	*InterruptPin										= Info->InterruptPin;
	*ClassCode											= Info->BaseClass;
	*SubClassCode										= Info->SubClass;
	*ParentPdo											= Info->ParentPdo;

	RtlCopyMemory(RoutingToken,&Info->RoutingToken,sizeof(ROUTING_TOKEN));

	return STATUS_SUCCESS;
}