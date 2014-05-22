//********************************************************************
//	created:	27:7:2008   0:49
//	file:		pci.devhere.cpp
//	author:		tiamo
//	purpose:	device present interface
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",devpresent_Constructor)
#pragma alloc_text("PAGE",devpresent_Initializer)
#pragma alloc_text("PAGE",devpresent_Reference)
#pragma alloc_text("PAGE",devpresent_Dereference)
#pragma alloc_text("PAGE",devpresent_IsDevicePresent)
#pragma alloc_text("PAGE",devpresent_IsDevicePresentEx)
#pragma alloc_text("PAGE",PcipDevicePresentOnBus)

//
// constructor [checked]
//
NTSTATUS devpresent_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
								__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	PPCI_DEVICE_PRESENT_INTERFACE DevHereInterface		= reinterpret_cast<PPCI_DEVICE_PRESENT_INTERFACE>(Interface);

	DevHereInterface->Version							= PCI_DEVICE_PRESENT_INTERFACE_VERSION;
	DevHereInterface->Context							= CommonExt;
	DevHereInterface->Size								= FIELD_OFFSET(PCI_DEVICE_PRESENT_INTERFACE,IsDevicePresentEx);
	DevHereInterface->InterfaceReference				= reinterpret_cast<PINTERFACE_REFERENCE>(&devpresent_Reference);
	DevHereInterface->InterfaceDereference				= reinterpret_cast<PINTERFACE_DEREFERENCE>(&devpresent_Dereference);
	DevHereInterface->IsDevicePresent					= reinterpret_cast<PPCI_IS_DEVICE_PRESENT>(&devpresent_IsDevicePresent);

	if(Size >= sizeof(PCI_DEVICE_PRESENT_INTERFACE))
	{
		DevHereInterface->Size							= sizeof(PCI_DEVICE_PRESENT_INTERFACE);
		DevHereInterface->IsDevicePresentEx				= reinterpret_cast<PPCI_IS_DEVICE_PRESENT_EX>(&devpresent_IsDevicePresentEx);
	}

	return STATUS_SUCCESS;
}

//
// initializer [checked]
//
NTSTATUS devpresent_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	return STATUS_SUCCESS;
}

//
// reference [checked]
//
VOID devpresent_Reference(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();
}

//
// dereference [checked]
//
VOID devpresent_Dereference(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();
}

//
// device is here [checked]
//
BOOLEAN devpresent_IsDevicePresent(__in USHORT VendorID,__in USHORT DeviceID,__in UCHAR RevisionID,__in USHORT SubVendorID,__in USHORT SubSystemID,__in ULONG Flags)
{
	PAGED_CODE();

	PCI_DEVICE_PRESENCE_PARAMETERS Parameters			= {0};
	Parameters.Size										= sizeof(Parameters);
	Parameters.DeviceID									= DeviceID;
	Parameters.RevisionID								= RevisionID;
	Parameters.SubSystemID								= SubSystemID;
	Parameters.SubVendorID								= SubVendorID;
	Parameters.VendorID									= VendorID;
	Parameters.Flags									= FlagOn(Flags,PCI_USE_SUBSYSTEM_IDS | PCI_USE_REVISION);
	SetFlag(Parameters.Flags,PCI_USE_VENDEV_IDS);

	return devpresent_IsDevicePresentEx(0,&Parameters);
}

//
// device is here [checked]
//
BOOLEAN devpresent_IsDevicePresentEx(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_DEVICE_PRESENCE_PARAMETERS Parameters)
{
	PAGED_CODE();

	ASSERT(Parameters);
	ASSERT(Parameters->Size >= sizeof(PCI_DEVICE_PRESENCE_PARAMETERS));
	ASSERT(FlagOn(Parameters->Flags,PCI_USE_VENDEV_IDS | PCI_USE_CLASS_SUBCLASS));

	if(FlagOn(Parameters->Flags,PCI_USE_SUBSYSTEM_IDS | PCI_USE_REVISION))
		ASSERT(FlagOn(Parameters->Flags,PCI_USE_VENDEV_IDS));

	if(FlagOn(Parameters->Flags,PCI_USE_PROGIF))
		ASSERT(FlagOn(Parameters->Flags,PCI_USE_CLASS_SUBCLASS));

	BOOLEAN Found										= FALSE;

	__try
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&PciGlobalLock,Executive,KernelMode,FALSE,0);

		if(FlagOn(Parameters->Flags,PCI_USE_LOCAL_BUS | PCI_USE_LOCAL_DEVICE))
		{
			ASSERT(PdoExt);
			try_leave(Found = PcipDevicePresentOnBus(PdoExt->ParentFdoExtension,PdoExt,Parameters));
		}

		PPCI_FDO_EXTENSION FdoExt						= CONTAINING_RECORD(PciFdoExtensionListHead.Next,PCI_FDO_EXTENSION,Common.ListEntry);

		while(FdoExt)
		{
			Found										= PcipDevicePresentOnBus(FdoExt,0,Parameters);
			if(Found)
				try_leave(NOTHING);

			FdoExt										= CONTAINING_RECORD(FdoExt->Common.ListEntry.Next,PCI_FDO_EXTENSION,Common.ListEntry);
		}
	}
	__finally
	{
		KeSetEvent(&PciGlobalLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}

	return Found;
}

//
// prenset on bus [checked]
//
BOOLEAN PcipDevicePresentOnBus(__in PPCI_FDO_EXTENSION ParentFdoExt,__in_opt PPCI_PDO_EXTENSION ChildPdoExt,__in PPCI_DEVICE_PRESENCE_PARAMETERS Parameters)
{
	PAGED_CODE();

	BOOLEAN Found										= FALSE;

	__try
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&ParentFdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

		PPCI_PDO_EXTENSION PdoExt						= CONTAINING_RECORD(ParentFdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);

		for( ; PdoExt; PdoExt = CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry))
		{
			if(ChildPdoExt && FlagOn(Parameters->Flags,PCI_USE_LOCAL_DEVICE) && ChildPdoExt->Slot.u.bits.DeviceNumber != PdoExt->Slot.u.bits.DeviceNumber)
				continue;

			if(FlagOn(Parameters->Flags,PCI_USE_VENDEV_IDS))
			{
				if(PdoExt->VendorId != Parameters->VendorID)
					continue;

				if(PdoExt->DeviceId != Parameters->DeviceID)
					continue;

				if(FlagOn(Parameters->Flags,PCI_USE_SUBSYSTEM_IDS))
				{
					if(PdoExt->SubSystemId != Parameters->SubSystemID)
						continue;

					if(PdoExt->SubVendorId != Parameters->SubVendorID)
						continue;
				}

				if(FlagOn(Parameters->Flags,PCI_USE_REVISION))
				{
					if(Parameters->RevisionID != PdoExt->RevisionId)
						continue;
				}
			}

			if(FlagOn(Parameters->Flags,PCI_USE_CLASS_SUBCLASS))
			{
				if(PdoExt->BaseClass != Parameters->BaseClass)
					continue;

				if(PdoExt->SubClass != Parameters->SubClass)
					continue;

				if(FlagOn(Parameters->Flags,PCI_USE_PROGIF))
				{
					if(PdoExt->ProgIf != Parameters->ProgIf)
						continue;
				}
			}

			try_leave(Found = TRUE);
		}
	}
	__finally
	{
		KeSetEvent(&ParentFdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}

	return Found;
}