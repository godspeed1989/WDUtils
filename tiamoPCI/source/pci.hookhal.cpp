//********************************************************************
//	created:	22:7:2008   23:24
//	file:		pci.hookhal.cpp
//	author:		tiamo
//	purpose:	hal hook
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("INIT",PciHookHal)
#pragma alloc_text("PAGE",PciUnhookHal)
#pragma alloc_text("PAGE",PciAssignSlotResources)

//
// hook hal [checked]
//
VOID PciHookHal()
{
	PAGED_CODE();

	ASSERT(!PcipSavedAssignSlotResources);
	ASSERT(!PcipSavedTranslateBusAddress);

	PcipSavedAssignSlotResources						= *Add2Ptr(HalPrivateDispatchTable,0x20,pHalAssignSlotResources*);
	PcipSavedTranslateBusAddress						= *Add2Ptr(HalPrivateDispatchTable,0x1c,pHalTranslateBusAddress*);

	*Add2Ptr(HalPrivateDispatchTable,0x20,PVOID*)		= &PciAssignSlotResources;
	*Add2Ptr(HalPrivateDispatchTable,0x1c,PVOID*)		= &PciTranslateBusAddress;
}

//
// unhook hal [checked]
//
VOID PciUnhookHal()
{
	PAGED_CODE();

	ASSERT(PcipSavedAssignSlotResources);

	ASSERT(PcipSavedTranslateBusAddress);

	*Add2Ptr(HalPrivateDispatchTable,0x20,PVOID*)		= PcipSavedAssignSlotResources;
	*Add2Ptr(HalPrivateDispatchTable,0x1c,PVOID*)		= PcipSavedTranslateBusAddress;
}

//
// translate bus address [checked]
//
BOOLEAN PciTranslateBusAddress(__in INTERFACE_TYPE InterfaceType,__in ULONG BusNumber,__in PHYSICAL_ADDRESS BusAddress,
							   __inout PULONG AddressSpace,__out PPHYSICAL_ADDRESS TranslatedAddress)
{
	ULONG InputAddressSpace								= *AddressSpace;
	BOOLEAN CallHal										= TRUE;

	if(KeGetCurrentIrql() < DISPATCH_LEVEL)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&PciGlobalLock,Executive,KernelMode,FALSE,0);

		PPCI_FDO_EXTENSION FdoExt						= CONTAINING_RECORD(PciFdoExtensionListHead.Next,PCI_FDO_EXTENSION,Common.ListEntry);
		while(FdoExt)
		{
			if(FdoExt->BaseBus == BusNumber)
				break;

			FdoExt										= CONTAINING_RECORD(FdoExt->Common.ListEntry.Next,PCI_FDO_EXTENSION,Common.ListEntry);
		}

		while(FdoExt && FdoExt != FdoExt->BusRootFdoExtension)
		{
			PPCI_PDO_EXTENSION PdoExt					= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);
			if(!PdoExt->Dependent.type1.SubtractiveDecode)
				break;

			FdoExt										= PdoExt->ParentFdoExtension;
		}

		KeSetEvent(&PciGlobalLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();

		if(!FdoExt)
		{
			PciDebugPrintf(0,"Pci: Could not find PCI bus FDO. Bus Number = 0x%x\n",BusNumber);
			return FALSE;
		}

		PCI_SIGNATURE Type;
		switch(InputAddressSpace)
		{
		case 0:
		case 2:
		case 4:
		case 6:
			Type										= PciArb_Memory;
			break;

		case 1:
		case 3:
			Type										= PciArb_Io;
			break;

		default:
			ASSERT(FALSE);
			return FALSE;
		}

		PPCI_SECONDARY_EXTENSION SecondaryExt			= PciFindNextSecondaryExtension(FdoExt->SecondaryExtension.Next,Type);
		if(!SecondaryExt)
		{
			ASSERT(SecondaryExt);
			return FALSE;
		}

		PPCI_ARBITER_INSTANCE Arbiter					= CONTAINING_RECORD(SecondaryExt,PCI_ARBITER_INSTANCE,SecondaryExtension);
		ArbAcquireArbiterLock(&Arbiter->CommonInstance);

		RTL_RANGE_LIST_ITERATOR Iterator;
		PRTL_RANGE Range;
		FOR_ALL_RANGES(Arbiter->CommonInstance.Allocation,&Iterator,Range)
		{
			if(static_cast<ULONGLONG>(BusAddress.QuadPart) < Range->Start)
				break;

			if((Range->Start >= static_cast<ULONGLONG>(BusAddress.QuadPart) || Range->End >= static_cast<ULONGLONG>(BusAddress.QuadPart)) && !Range->Owner)
			{
				CallHal									= FALSE;
				break;
			}
		}

		ArbReleaseArbiterLock(&Arbiter->CommonInstance);
	}

	//
	// call saved hal routine
	//
	if(CallHal && PcipSavedTranslateBusAddress(InterfaceType,BusNumber,BusAddress,AddressSpace,TranslatedAddress))
		return TRUE;

	//
	// hal can't translate it
	//
	if(BusAddress.HighPart)
		return FALSE;

	//
	// check those special port and memory
	//
	if(InputAddressSpace == 0)
	{
		if( (BusAddress.LowPart >= 0xa0000 && BusAddress.LowPart <= 0xbffff) ||
			(BusAddress.LowPart >= 0x400 && BusAddress.LowPart <= 0x4ff) ||
			(BusAddress.LowPart == 0x70))
		{
			TranslatedAddress->HighPart					= 0;
			TranslatedAddress->LowPart					= BusAddress.LowPart;
			*AddressSpace								= 0;

			return TRUE;
		}
	}
	else if(InputAddressSpace == 1)
	{
		if(BusAddress.LowPart >= 0xcf8 && BusAddress.LowPart <= 0xcff)
		{
			TranslatedAddress->HighPart					= 0;
			TranslatedAddress->LowPart					= BusAddress.LowPart;
			*AddressSpace								= 1;

			return TRUE;
		}
	}

	return FALSE;
}

//
// assign slot resource [checked]
//
NTSTATUS PciAssignSlotResources(__in PUNICODE_STRING RegistryPath,__in_opt PUNICODE_STRING DriverClassName,
								__in PDRIVER_OBJECT DriverObject,__in PDEVICE_OBJECT DeviceObject,__in INTERFACE_TYPE BusType,
								__in ULONG BusNumber,__in ULONG SlotNumber,__inout PCM_RESOURCE_LIST *AllocatedResources)
{
	PAGED_CODE();

	ASSERT(PcipSavedAssignSlotResources);
	ASSERT(BusType == PCIBus);

	*AllocatedResources									= 0;

	//
	// find pdo ext
	//
	PPCI_PDO_EXTENSION PdoExt							= PciFindPdoByLocation(BusNumber,SlotNumber);
	if(!PdoExt)
		return STATUS_DEVICE_DOES_NOT_EXIST;

	//
	// this pdo must in not start state
	//
	if(PdoExt->Common.DeviceState != PciNotStarted)
		return STATUS_INVALID_OWNER;

	NTSTATUS Status										= STATUS_SUCCESS;
	PIO_RESOURCE_REQUIREMENTS_LIST IoReqList			= 0;
	PCM_RESOURCE_LIST CmResList							= 0;
	UCHAR IntLine										= 0;
	UCHAR IntPin										= 0;
	UCHAR Base											= 0;
	UCHAR Sub											= 0;
	PDEVICE_OBJECT ParentPdo							= PdoExt->ParentFdoExtension->PhysicalDeviceObject;
	PDEVICE_OBJECT NewDO								= 0;

	__try
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&PciGlobalLock,Executive,KernelMode,FALSE,0);

		ASSERT(DeviceObject != PdoExt->PhysicalDeviceObject);

		//
		// read device config
		//
		PCI_COMMON_HEADER Config;
		PciReadDeviceConfig(PdoExt,&Config,0,sizeof(Config));

		//
		// cache it
		//
		IntLine											= Config.u.type0.InterruptLine;
		IntPin											= Config.u.type0.InterruptPin;
		Base											= Config.BaseClass;
		Sub												= Config.SubClass;
		Status											= PciCacheLegacyDeviceRouting(DeviceObject,BusNumber,SlotNumber,IntLine,IntPin,Base,Sub,ParentPdo,PdoExt,&NewDO);
		if(!NT_SUCCESS(Status))
			try_leave(NewDO = 0);

		//
		// this is a legacy driver
		//
		PdoExt->LegacyDriver							= TRUE;

		//
		// build requirement list
		//
		Status											= PciBuildRequirementsList(PdoExt,&Config,&IoReqList);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// call kernel assign routine
		//
		Status											= IoAssignResources(RegistryPath,DriverClassName,DriverObject,DeviceObject,IoReqList,&CmResList);
		if(!NT_SUCCESS(Status))
			try_leave(ASSERT(!CmResList));

		//
		// enable this device
		//
		SetFlag(PdoExt->CommandEnables,PCI_ENABLE_BUS_MASTER | PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);

		//
		// set device's resources
		//
		PciComputeNewCurrentSettings(PdoExt,CmResList);
		Status											= PciSetResources(PdoExt,TRUE,TRUE);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		ASSERT(CmResList->Count == 1);

		//
		// remove all device private descriptors
		//
		PCM_PARTIAL_RESOURCE_LIST PartialList			= &CmResList->List->PartialResourceList;
		PCM_PARTIAL_RESOURCE_DESCRIPTOR Dest			= PartialList->PartialDescriptors;
		PCM_PARTIAL_RESOURCE_DESCRIPTOR Src				= Dest;
		ULONG DestIndex									= 0;
		ULONG SrcIndex									= 0;

		for(ULONG i = 0; i < CmResList->List->PartialResourceList.Count; i ++)
		{
			if(Src->Type != CmResourceTypeDevicePrivate)
			{
				if(DestIndex < SrcIndex)
					RtlCopyMemory(Dest,Src,sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
			
				DestIndex								+= 1;
				Dest									+= 1;
			}
			else
			{
				ASSERT(PartialList->Count > 0);
				PartialList->Count						-= 1;
			}

			SrcIndex									+= 1;
			Src											+= 1;
		}

		ASSERT(PartialList->Count > 0);
	}
	__finally
	{
		if(NewDO && (!NT_SUCCESS(Status) || AbnormalTermination()))
			PciCacheLegacyDeviceRouting(NewDO,BusNumber,SlotNumber,IntLine,IntPin,Base,Sub,ParentPdo,PdoExt,0);

		KeSetEvent(&PciGlobalLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();

		if(IoReqList)
			ExFreePool(IoReqList);

		if(NT_SUCCESS(Status) && !AbnormalTermination())
			*AllocatedResources							= CmResList;
		else if(CmResList)
			ExFreePool(CmResList);
	}

	return Status;
}

//
// find pdo by location [checked]
//
PPCI_PDO_EXTENSION PciFindPdoByLocation(__in ULONG BusNumber,__in ULONG SlotNumber)
{
	KeEnterCriticalRegion();
	KeWaitForSingleObject(&PciGlobalLock,Executive,KernelMode,FALSE,0);

	PPCI_FDO_EXTENSION FdoExt						= CONTAINING_RECORD(PciFdoExtensionListHead.Next,PCI_FDO_EXTENSION,Common.ListEntry);
	while(FdoExt)
	{
		if(FdoExt->BaseBus == BusNumber)
			break;

		FdoExt										= CONTAINING_RECORD(FdoExt->Common.ListEntry.Next,PCI_FDO_EXTENSION,Common.ListEntry);
	}

	KeSetEvent(&PciGlobalLock,IO_NO_INCREMENT,FALSE);
	KeLeaveCriticalRegion();

	if(!FdoExt)
	{
		PciDebugPrintf(0,"Pci: Could not find PCI bus FDO. Bus Number = 0x%x\n",BusNumber);
		return 0;
	}

	KeEnterCriticalRegion();
	KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

	PCI_SLOT_NUMBER PciSlot;
	PciSlot.u.AsULONG								= SlotNumber;
	PciSlot.u.bits.Reserved							= 0;
	PPCI_PDO_EXTENSION PdoExt						= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);
	while(PdoExt)
	{
		if(PdoExt->Slot.u.AsULONG == PciSlot.u.AsULONG)
			break;

		PdoExt										= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
	}

	KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
	KeLeaveCriticalRegion();

	return PdoExt;
}