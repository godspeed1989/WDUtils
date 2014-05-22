//********************************************************************
//	created:	26:7:2008   23:39
//	file:		pci.lddintrf.cpp
//	author:		tiamo
//	purpose:	legacy device detection
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",lddintrf_Constructor)
#pragma alloc_text("PAGE",lddintrf_Initializer)
#pragma alloc_text("PAGE",lddintrf_Reference)
#pragma alloc_text("PAGE",lddintrf_Dereference)
#pragma alloc_text("PAGE",PciLegacyDeviceDetection)

//
// constructor [checked]
//
NTSTATUS lddintrf_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
							   __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	Interface->Version									= LEGACY_DEVICE_DETECTION_INTRF_STANDARD_VER;
	Interface->Context									= CommonExt;
	Interface->Size										= sizeof(LEGACY_DEVICE_DETECTION_INTERFACE);
	Interface->InterfaceDereference						= reinterpret_cast<PINTERFACE_DEREFERENCE>(&lddintrf_Dereference);
	Interface->InterfaceReference						= reinterpret_cast<PINTERFACE_REFERENCE>(&lddintrf_Reference);

	PLEGACY_DEVICE_DETECTION_INTERFACE lddInterface		= reinterpret_cast<PLEGACY_DEVICE_DETECTION_INTERFACE>(Interface);
	lddInterface->LegacyDeviceDetection					= reinterpret_cast<PLEGACY_DEVICE_DETECTION_HANDLER>(&PciLegacyDeviceDetection);

	return STATUS_SUCCESS;
}

//
// initializer [checked]
//
NTSTATUS lddintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	ASSERTMSG("PCI lddintrf_Initializer, unexpected call.",FALSE);

	return STATUS_UNSUCCESSFUL;
}

//
// reference [checked]
//
VOID lddintrf_Reference(__in PPCI_FDO_EXTENSION PdoExt)
{
	PAGED_CODE();
	ASSERT(PdoExt->Common.ExtensionType == PciFdoExtensionType);
}

//
// dereference [checked]
//
VOID lddintrf_Dereference(__in PPCI_FDO_EXTENSION PdoExt)
{
	PAGED_CODE();
	ASSERT(PdoExt->Common.ExtensionType == PciFdoExtensionType);
}

//
// legacy device dectetion [checked]
//
NTSTATUS PciLegacyDeviceDetection(__in PPCI_FDO_EXTENSION FdoExt,__in INTERFACE_TYPE LegacyBusType,__in ULONG BusNumber,__in ULONG SlotNumber,__out PDEVICE_OBJECT *Pdo)
{
	PAGED_CODE();

	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	if(LegacyBusType != PCIBus || BusNumber != FdoExt->BaseBus)
		return STATUS_UNSUCCESSFUL;

	NTSTATUS Status										= STATUS_UNSUCCESSFUL;
	__try
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

		PCI_SLOT_NUMBER Search;
		Search.u.AsULONG								= SlotNumber;
		Search.u.bits.Reserved							= 0;

		PPCI_PDO_EXTENSION PdoExt						= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);

		while(PdoExt)
		{
			if(PdoExt->Slot.u.AsULONG == Search.u.AsULONG)
			{
				if(PdoExt->Common.DeviceState == PciNotStarted)
				{
					Status								= STATUS_SUCCESS;
					ObReferenceObject(PdoExt->PhysicalDeviceObject);
					*Pdo								= PdoExt->PhysicalDeviceObject;
				}
			
				try_leave(NOTHING);
			}

			PdoExt										= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		}
	}
	__finally
	{
		KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}

	return Status;
}