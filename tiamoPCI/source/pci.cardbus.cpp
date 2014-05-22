//********************************************************************
//	created:	27:7:2008   1:43
//	file:		pci.cardbus.cpp
//	author:		tiamo
//	purpose:	cardbus
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",pcicbintrf_Initializer)
#pragma alloc_text("PAGE",pcicbintrf_Constructor)
#pragma alloc_text("PAGE",pcicbintrf_Reference)
#pragma alloc_text("PAGE",pcicbintrf_Dereference)
#pragma alloc_text("PAGE",pcicbintrf_AddCardBus)
#pragma alloc_text("PAGE",pcicbintrf_DeleteCardBus)
#pragma alloc_text("PAGE",pcicbintrf_DispatchPnp)
#pragma alloc_text("PAGE",pcicbintrf_GetLocation)
#pragma alloc_text("PAGE",Cardbus_GetAdditionalResourceDescriptors)
#pragma alloc_text("PAGE",Cardbus_MassageHeaderForLimitsDetermination)
#pragma alloc_text("PAGE",Cardbus_RestoreCurrent)
#pragma alloc_text("PAGE",Cardbus_SaveLimits)
#pragma alloc_text("PAGE",Cardbus_SaveCurrentSettings)

//
// constructor [checked]
//
NTSTATUS pcicbintrf_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
								__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	Interface->Version									= PCI_CARDBUS_INTRF_STANDARD_VER;
	Interface->Context									= CommonExt;
	Interface->Size										= sizeof(CARDBUS_PRIVATE_INTERFACE);
	Interface->InterfaceDereference						= reinterpret_cast<PINTERFACE_DEREFERENCE>(&pcicbintrf_Dereference);
	Interface->InterfaceReference						= reinterpret_cast<PINTERFACE_REFERENCE>(&pcicbintrf_Reference);

	PCARDBUS_PRIVATE_INTERFACE CardbusInterface			= reinterpret_cast<PCARDBUS_PRIVATE_INTERFACE>(Interface);
	CardbusInterface->AddBus							= reinterpret_cast<PCARDBUS_ADD_BUS>(&pcicbintrf_AddCardBus);
	CardbusInterface->DeletBus							= reinterpret_cast<PCARDBUS_DELETE_BUS>(&pcicbintrf_DeleteCardBus);
	CardbusInterface->DispatchPnp						= reinterpret_cast<PCARDBUS_DISPATCH_PNP>(&pcicbintrf_DispatchPnp);
	CardbusInterface->GetLocation						= reinterpret_cast<PCARDBUS_GET_LOCATION>(&pcicbintrf_GetLocation);
	CardbusInterface->DriverObject						= PciDriverObject;

	return STATUS_SUCCESS;
}

//
// initializer [checked]
//
NTSTATUS pcicbintrf_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	ASSERTMSG("PCI pcicbintrf_Initializer, unexpected call.",FALSE);

	return STATUS_UNSUCCESSFUL;
}

//
// reference [checked]
//
VOID pcicbintrf_Reference(__in PDEVICE_OBJECT DeviceObject)
{
	PAGED_CODE();
}

//
// dereference [checked]
//
VOID pcicbintrf_Dereference(__in PDEVICE_OBJECT DeviceObject)
{
	PAGED_CODE();
}

//
// add cardbus [checked]
//
NTSTATUS pcicbintrf_AddCardBus(__in PDEVICE_OBJECT Pdo,__out PPCI_FDO_EXTENSION* NewFdoExt)
{
	PAGED_CODE();

	PciDebugPrintf(0x20000,"PCI - AddCardBus FDO for PDO %08x\n",Pdo);

	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	if(PdoExt->BaseClass != PCI_CLASS_BRIDGE_DEV || PdoExt->SubClass != PCI_SUBCLASS_BR_CARDBUS)
	{
		ASSERT(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV);
		ASSERT(PdoExt->SubClass == PCI_SUBCLASS_BR_CARDBUS);

		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PPCI_FDO_EXTENSION ParentFdoExt						= PdoExt->ParentFdoExtension;
	if( ParentFdoExt->BaseBus != PdoExt->Dependent.type2.PrimaryBus ||
		PdoExt->Dependent.type2.SecondaryBus <= ParentFdoExt->BaseBus ||
		PdoExt->Dependent.type2.SubordinateBus < PdoExt->Dependent.type2.SubordinateBus)
	{
		PciDebugPrintf(0x20000,"PCI Cardbus Bus Number configuration error (%02x>=%02x>%02x=%02x)\n",
					   PdoExt->Dependent.type2.SubordinateBus,PdoExt->Dependent.type2.SecondaryBus,PdoExt->Dependent.type2.PrimaryBus,ParentFdoExt->BaseBus);

		ASSERT(ParentFdoExt->BaseBus == PdoExt->Dependent.type1.PrimaryBus);
		ASSERT(PdoExt->Dependent.type2.SecondaryBus > ParentFdoExt->BaseBus);
		ASSERT(PdoExt->Dependent.type2.SubordinateBus >= PdoExt->Dependent.type2.SecondaryBus);

		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PPCI_FDO_EXTENSION FdoExt							= 0;
	PCM_RESOURCE_LIST CmResList							= 0;
	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		FdoExt											= static_cast<PPCI_FDO_EXTENSION>(ExAllocatePoolWithTag(NonPagedPool,sizeof(PCI_FDO_EXTENSION),'BciP'));
		if(!FdoExt)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		PciInitializeFdoExtensionCommonFields(FdoExt,ParentFdoExt->FunctionalDeviceObject,Pdo);

		FdoExt->PowerState.CurrentDeviceState			= PowerDeviceD0;
		FdoExt->PowerState.CurrentSystemState			= PowerSystemWorking;
		FdoExt->Common.DeviceState						= PciStarted;
		FdoExt->Common.TentativeNextState				= PciStarted;
		FdoExt->BaseBus									= PdoExt->Dependent.type2.SecondaryBus;
		FdoExt->BusRootFdoExtension						= ParentFdoExt->BusRootFdoExtension;

		Status											= PciInitializeArbiters(FdoExt);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		PdoExt->BridgeFdoExtension						= FdoExt;
		FdoExt->ParentFdoExtension						= ParentFdoExt;
		FdoExt->Fake									= TRUE;
		FdoExt->ChildDelete								= FALSE;

		if(NT_SUCCESS(PciQueryResources(PdoExt,&CmResList)))
		{
			ASSERT(CmResList);
			ASSERT(CmResList->Count == 1);
			ASSERT(CmResList->List->PartialResourceList.Count);
			PCM_PARTIAL_RESOURCE_DESCRIPTOR CmRes		= CmResList->List->PartialResourceList.PartialDescriptors;
			if(CmRes->Type == CmResourceTypeMemory)
			{
				ASSERT(CmRes->u.Generic.Length == 4096);
				CmRes->Type								= CmResourceTypeNull;
			}

			PciInitializeArbiterRanges(FdoExt,CmResList);
		}

		PciInvalidateResourceInfoCache(PdoExt);
		PciInsertEntryAtTail(&PciFdoExtensionListHead,&FdoExt->Common.ListEntry,&PciGlobalLock);
	}
	__finally
	{
		if(NT_SUCCESS(Status))
			*NewFdoExt									= FdoExt;
		else if(FdoExt)
			ExFreePool(FdoExt);

		if(CmResList)
			ExFreePool(CmResList);
	}

	return Status;
}

//
// delete bus [checked]
//
NTSTATUS pcicbintrf_DeleteCardBus(__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);
	ASSERT(PdoExt->BridgeFdoExtension == FdoExt);

	PciDebugPrintf(0x20000,"PCI - DeleteCardBus (fake) FDO %08x for PDO %08x\n",FdoExt,PdoExt);

	KeEnterCriticalRegion();
	KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

	if(FdoExt->ChildPdoList.Next)
		FdoExt->ChildDelete								= TRUE;

	KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
	KeLeaveCriticalRegion();

	PciRemoveEntryFromList(&PciFdoExtensionListHead,&FdoExt->Common.ListEntry,&PciGlobalLock);

	if(!FdoExt->ChildDelete)
		ExFreePool(FdoExt);

	return STATUS_SUCCESS;
}

//
// dispatch pnp [checked]
//
NTSTATUS pcicbintrf_DispatchPnp(__in PPCI_FDO_EXTENSION FdoExt,__in PIRP Irp)
{
	PAGED_CODE();

	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);
	ASSERT(FdoExt->Fake);

	PIO_STACK_LOCATION IrpSp							= IoGetCurrentIrpStackLocation(Irp);
	ASSERT(IrpSp->MajorFunction == IRP_MJ_PNP);

	PciDebugPrintf(0x20000,"PCI CardBus Dispatch PNP: FDO(%x, bus 0x%02x)<-%s\n",FdoExt,FdoExt->BaseBus,PciDebugPnpIrpTypeToText(IrpSp->MinorFunction));

	return PciFdoIrpQueryDeviceRelations(Irp,IrpSp,FdoExt);
}

//
// get location [checked]
//
NTSTATUS pcicbintrf_GetLocation(__in PDEVICE_OBJECT Pdo,__out PUCHAR Bus,__out PUCHAR Device,__out PUCHAR Function,__out PBOOLEAN OnDebugPath)
{
	PAGED_CODE();

	ASSERT(Bus);
	ASSERT(Device);
	ASSERT(Function);

	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);
	if(!PdoExt || PdoExt->Common.ExtensionType != PciPdoExtensionType)
		return STATUS_NOT_FOUND;

	*Bus												= PdoExt->ParentFdoExtension->BaseBus;
	*Device												= PdoExt->Slot.u.bits.DeviceNumber;
	*Function											= PdoExt->Slot.u.bits.FunctionNumber;
	*OnDebugPath										= PdoExt->OnDebugPath;

	return STATUS_SUCCESS;
}

//
// reset device [checked]
//
NTSTATUS Cardbus_ResetDevice(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config)
{
	return STATUS_SUCCESS;
}

//
// get additional resource [checked]
//
VOID Cardbus_GetAdditionalResourceDescriptors(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in PIO_RESOURCE_DESCRIPTOR IoRes)
{
	PAGED_CODE();
}

//
// massage header [checked]
//
VOID Cardbus_MassageHeaderForLimitsDetermination(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	Param->Working->u.type2.SocketRegistersBaseAddress	= 0xffffffff;

	for(ULONG i = 0; i < ARRAYSIZE(Param->Working->u.type2.Range); i ++)
	{
		Param->Working->u.type2.Range[i].Base			= 0xffffffff;
		Param->Working->u.type2.Range[i].Limit			= 0xffffffff;
	}

	Param->Working->u.type2.SecondaryStatus				= 0;
	Param->SavedSecondaryStatus							= Param->OriginalConfig->u.type2.SecondaryStatus;
	Param->OriginalConfig->u.type2.SecondaryStatus		= 0;

	if(Param->PdoExt->OnDebugPath)
		return;

	for(ULONG i = 0; i < ARRAYSIZE(Param->OriginalConfig->u.type2.Range); i ++)
	{
		Param->OriginalConfig->u.type2.Range[i].Base	= i > 2 ? 0xfffffffc : 0xfffff000;
		Param->OriginalConfig->u.type2.Range[i].Limit	= 0;
	}
}

//
// restore current [checked]
//
VOID Cardbus_RestoreCurrent(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	Param->OriginalConfig->u.type2.SecondaryStatus		= Param->SavedSecondaryStatus;
}

//
// save limits [checked]
//
VOID Cardbus_SaveLimits(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	ULONG Bar[2]										= {Param->Working->u.type2.SocketRegistersBaseAddress,0};
	BOOLEAN DbgChk64Bits								= PciCreateIoDescriptorFromBarLimit(Param->PdoExt->Resources->Limit,Bar,FALSE);
	ASSERT(!DbgChk64Bits);

	for(ULONG i = 0; i < ARRAYSIZE(Param->Working->u.type2.Range); i ++)
	{
		PIO_RESOURCE_DESCRIPTOR IoDesc					= Param->PdoExt->Resources->Limit + 1 + i;

		ULONG Mask;

		if(i >= 2)
		{
			if(!FlagOn(Param->Working->u.type2.Range[i].Base,3))
			{
				ASSERT(!FlagOn(Param->Working->u.type2.Range[i].Limit,0x03));
				Param->Working->u.type2.Range[i].Base	&= 0x0000ffff;
				Param->Working->u.type2.Range[i].Limit	&= 0x0000ffff;
			}

			Mask										= 0x3;
			IoDesc->Flags								= CM_RESOURCE_PORT_POSITIVE_DECODE | CM_RESOURCE_PORT_WINDOW_DECODE | CM_RESOURCE_PORT_IO;
			IoDesc->Type								= CmResourceTypePort;
		}
		else
		{
			Mask										= 0xfff;
			IoDesc->Flags								= 0;
			IoDesc->Type								= CmResourceTypeMemory;
		}

		ULONG Temp										= Param->Working->u.type2.Range[i].Base & ~Mask;
		if(!Temp || Temp >= (Param->Working->u.type2.Range[i].Limit | Mask))
		{
			IoDesc->Type								= CmResourceTypeNull;
		}
		else
		{
			IoDesc->u.Generic.Alignment					= Mask + 1;
			IoDesc->u.Generic.Length					= 0;
			IoDesc->u.Generic.MinimumAddress.QuadPart	= 0;
			IoDesc->u.Generic.MaximumAddress.LowPart	= Param->Working->u.type2.Range[i].Limit | Mask;
			IoDesc->u.Generic.MaximumAddress.HighPart	= 0;
		}
	}

	USHORT Ids[4];
	PciReadDeviceConfig(Param->PdoExt,Ids,sizeof(PCI_COMMON_HEADER),sizeof(Ids));

	Param->PdoExt->SubVendorId							= Ids[0];
	Param->PdoExt->SubSystemId							= Ids[1];

	ASSERT(Param->PdoExt->Resources->Limit[1].u.Generic.Length == 0);

	Param->PdoExt->Resources->Limit[1].u.Generic.Length = 0x1000;
}

//
// save current [checked]
//
VOID Cardbus_SaveCurrentSettings(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	for(ULONG i = 0; i < 6; i ++)
	{
		PIO_RESOURCE_DESCRIPTOR	IoDesc					= Param->PdoExt->Resources->Limit + i;
		PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc			= Param->PdoExt->Resources->Current + i;

		CmDesc->Type									= IoDesc->Type;
		if(IoDesc->Type != CmResourceTypeNull)
		{
			CmDesc->Flags								= IoDesc->Flags;
			CmDesc->ShareDisposition					= IoDesc->ShareDisposition;

			if(i == 0)
			{
				CmDesc->u.Generic.Length				= IoDesc->u.Generic.Length;
				CmDesc->u.Generic.Start.HighPart		= 0;
				CmDesc->u.Generic.Start.LowPart			= ~(IoDesc->u.Generic.Length - 1) & Param->OriginalConfig->u.type2.SocketRegistersBaseAddress;
			}
			else if(i != 5)
			{
				ULONG Mask								= 0xfff;
				ULONG Limit								= Param->OriginalConfig->u.type2.Range[i - 1].Limit;
				ULONG Start								= Param->OriginalConfig->u.type2.Range[i - 1].Base;

				if(i >= 3)
				{
					if(!FlagOn(Start,0x03))
					{
						Start							&= 0x0000ffff;
						Limit							&= 0x0000ffff;
					}

					Mask								= 0x3;
				}

				Start									&= ~Mask;
				Limit									|= Mask;

				if(!Start || Start >= Limit)
				{
					CmDesc->Type						= CmResourceTypeNull;
				}
				else
				{
					CmDesc->u.Generic.Start.HighPart	= 0;
					CmDesc->u.Generic.Start.LowPart		= Start;
					CmDesc->u.Generic.Length			= Limit - Start + 1;
				}
			}
		}
	}

	Param->PdoExt->Dependent.type2.IsaBitSet			= FALSE;

	if(FlagOn(Param->OriginalConfig->u.type2.BridgeControl,0x304))
		Param->PdoExt->UpdateHardware					= TRUE;

	Param->PdoExt->Dependent.type2.PrimaryBus			= Param->OriginalConfig->u.type2.PrimaryBus;
	Param->PdoExt->Dependent.type2.SecondaryBus			= Param->OriginalConfig->u.type2.SecondaryBus;
	Param->PdoExt->Dependent.type2.SubordinateBus		= Param->OriginalConfig->u.type2.SubordinateBus;
}

//
// change resource settings [checked]
//
VOID Cardbus_ChangeResourceSettings(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config)
{
	for(ULONG i = 0; i < ARRAYSIZE(Config->u.type2.Range); i ++)
	{
		Config->u.type2.Range[i].Base					= 0xffffffff;
		Config->u.type2.Range[i].Limit					= 0;
	}

	if(PdoExt->Resources)
	{
		for(ULONG i = 0; i < 6; i ++)
		{
			PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc		= PdoExt->Resources->Current + i;
			PIO_RESOURCE_DESCRIPTOR IoDesc				= PdoExt->Resources->Limit + i;

			if(CmDesc->Type != CmResourceTypeNull)
			{
				ASSERT(CmDesc->Type == IoDesc->Type);

				ULONG Start								= CmDesc->u.Generic.Start.LowPart;

				ASSERT(!CmDesc->u.Generic.Start.HighPart);

				if(i == 0)
				{
					ASSERT(CmDesc->Type == CmResourceTypeMemory);
					Config->u.type2.SocketRegistersBaseAddress	= Start;
				}
				else if(i == 5)
				{
					CmDesc->Type						= CmResourceTypeNull;
					ASSERT(CmDesc->Type == CmResourceTypeNull);
				}
				else
				{
					ULONG Align							= 0xfff;
					ULONG Length						= CmDesc->u.Generic.Length;

					if(i >= 3)
					{
						if(!FlagOn(Config->u.type2.Range[i - 1].Base,0x03))
							ASSERT(((Start + Length - 1) & 0xffff0000) == 0);

						Align							= 0x3;
					}

					ASSERT((Start & Align) == 0);
					ASSERT((Length & Align) == 0 && Length > Align);

					Config->u.type2.Range[i - 1].Base	= Start;
					Config->u.type2.Range[i - 1].Limit	= Start + Length - 1;
				}
			}
		}
	}

	Config->u.type2.PrimaryBus							= PdoExt->Dependent.type2.PrimaryBus;
	Config->u.type2.SecondaryBus						= PdoExt->Dependent.type2.SecondaryBus;
	Config->u.type2.SubordinateBus						= PdoExt->Dependent.type2.SubordinateBus;

	ASSERT(!PdoExt->Dependent.type2.IsaBitSet);

	ClearFlag(Config->u.type2.BridgeControl,0x304);

	if(PdoExt->Dependent.type2.VgaBitSet)
		SetFlag(Config->u.type2.BridgeControl,0x08);
}