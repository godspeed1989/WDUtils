//********************************************************************
//	created:	27:7:2008   0:07
//	file:		pci.agpintrf.cpp
//	author:		tiamo
//	purpose:	agp interface
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",agpintrf_Constructor)
#pragma alloc_text("PAGE",agpintrf_Initializer)
#pragma alloc_text("PAGE",agpintrf_Reference)
#pragma alloc_text("PAGE",agpintrf_Dereference)
#pragma alloc_text("PAGE",PciPnpTranslateBusAddress)
#pragma alloc_text("PAGE",PciPnpWriteConfig)

//
// constructor [checked]
//
NTSTATUS agpintrf_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
							  __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();
	PPCI_PDO_EXTENSION PdoExt							= reinterpret_cast<PPCI_PDO_EXTENSION>(CommonExt);

	if(PdoExt->BaseClass != PCI_CLASS_BRIDGE_DEV || PdoExt->SubClass != PCI_SUBCLASS_BR_PCI_TO_PCI)
		return STATUS_NOT_SUPPORTED;

	if(PdoExt->TargetAgpCapabilityId != PCI_CAPABILITY_ID_AGP_TARGET)
	{
		PPCI_FDO_EXTENSION ParentFdoExt					= PdoExt->ParentFdoExtension;
		if(ParentFdoExt != ParentFdoExt->BusRootFdoExtension)
			return STATUS_NOT_SUPPORTED;

		NTSTATUS Status									= STATUS_SUCCESS;
		PdoExt											= 0;
	
		__try
		{
			KeEnterCriticalRegion();
			KeWaitForSingleObject(&ParentFdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

			PPCI_PDO_EXTENSION ChildPdoExt				= CONTAINING_RECORD(ParentFdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);

			while(ChildPdoExt)
			{
				if(ChildPdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && ChildPdoExt->SubClass == PCI_SUBCLASS_BR_HOST && ChildPdoExt->TargetAgpCapabilityId)
				{
					if(PdoExt)
						try_leave(Status = STATUS_NOT_SUPPORTED)
					else
						PdoExt							= ChildPdoExt;
				}

				ChildPdoExt								= CONTAINING_RECORD(ChildPdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
			}
		}
		__finally
		{
			KeSetEvent(&ParentFdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
			KeLeaveCriticalRegion();
		}

		if(!NT_SUCCESS(Status))
			return Status;
	}

	if(PdoExt)
	{
		Interface->Version								= 1;
		Interface->Context								= PdoExt;
		Interface->Size									= sizeof(AGP_TARGET_BUS_INTERFACE_STANDARD);
		Interface->InterfaceDereference					= reinterpret_cast<PINTERFACE_DEREFERENCE>(&agpintrf_Dereference);
		Interface->InterfaceReference					= reinterpret_cast<PINTERFACE_REFERENCE>(&agpintrf_Reference);

		PAGP_TARGET_BUS_INTERFACE_STANDARD AgpInterface	= reinterpret_cast<PAGP_TARGET_BUS_INTERFACE_STANDARD>(Interface);
		AgpInterface->SetBusData						= reinterpret_cast<PGET_SET_DEVICE_DATA>(&PciWriteAgpConfig);
		AgpInterface->GetBusData						= reinterpret_cast<PGET_SET_DEVICE_DATA>(&PciReadAgpConfig);
		AgpInterface->CapabilityID						= PdoExt->TargetAgpCapabilityId;

		return STATUS_SUCCESS;
	}

	return STATUS_NO_SUCH_DEVICE;
}

//
// initializer [checked]
//
NTSTATUS agpintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	ASSERTMSG("PCI agpintrf_Initializer, unexpected call.",FALSE);

	return STATUS_UNSUCCESSFUL;
}

//
// reference [checked]
//
VOID agpintrf_Reference(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	if(InterlockedIncrement(&PdoExt->BusInterfaceReferenceCount) == 1)
		ObReferenceObject(PdoExt->PhysicalDeviceObject);
}

//
// dereference [checked]
//
VOID agpintrf_Dereference(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	if(InterlockedDecrement(&PdoExt->BusInterfaceReferenceCount) == 0)
		ObDereferenceObject(PdoExt->PhysicalDeviceObject);
}

//
// write config [checked]
//
ULONG PciWriteAgpConfig(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG DataType,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	PciWriteDeviceSpace(PdoExt,DataType,Buffer,Offset,Length,&Length);

	return Length;
}

//
// read config [checked]
//
ULONG PciReadAgpConfig(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG DataType,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	PciReadDeviceSpace(PdoExt,DataType,Buffer,Offset,Length,&Length);

	return Length;
}