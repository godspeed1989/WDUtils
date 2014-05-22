//********************************************************************
//	created:	22:7:2008   22:25
//	file:		pci.commarb.cpp
//	author:		tiamo
//	purpose:	common arbiter
//********************************************************************

#include "stdafx.h"

//
// initialize arbiters [checked]
//
NTSTATUS PciInitializeArbiters(__in PPCI_FDO_EXTENSION FdoExt)
{
	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	NTSTATUS Status										= STATUS_SUCCESS;

	for(PCI_SIGNATURE CurrentType = PciArb_Io; CurrentType != PciTrans_Interrupt; CurrentType = static_cast<PCI_SIGNATURE>(static_cast<ULONG>(CurrentType) + 1))
	{
		//
		// check subtrace decode bridge
		//
		if(FdoExt->BusRootFdoExtension != FdoExt)
		{
			PPCI_PDO_EXTENSION PdoExt					= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);

			ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

			if(PdoExt->Dependent.type1.SubtractiveDecode)
			{
				PciDebugPrintf(3,"PCI Not creating arbiters for subtractive bus %d\n",PdoExt->Dependent.type1.SecondaryBus);
				continue;
			}
		}

		//
		// search pci interface with current type
		//
		PPCI_INTERFACE Interface						= 0;
		for(ULONG i = 0; Interface = PciInterfaces[i]; i ++)
		{
			if(Interface->Signature == CurrentType)
				break;
		}

		//
		// unable to find the interfece
		//
		if(!Interface)
		{
			PciDebugPrintf(0x7fffffff,"PCI - FDO ext 0x%08x no %s arbiter.\n",FdoExt,PciArbiterNames[CurrentType - PciArb_Io]);
			continue;
		}

		//
		// allocate an instance
		//
		PPCI_ARBITER_INSTANCE Instance					= static_cast<PPCI_ARBITER_INSTANCE>(PciAllocateColdPoolWithTag(PagedPool,sizeof(PCI_ARBITER_INSTANCE),'BicP'));
		if(!Instance)
			return STATUS_INSUFFICIENT_RESOURCES;

		Instance->BusFdoExtension						= FdoExt;
		Instance->Interface								= Interface;

		RtlStringCchPrintfW(Instance->InstanceName,ARRAYSIZE(Instance->InstanceName),L"PCI %S (b=%02x)",PciArbiterNames[CurrentType - PciArb_Io],FdoExt->BaseBus);

		//
		// call interface's initializer
		//
		Status											= Interface->Initializer(Instance);
		if(!NT_SUCCESS(Status))
			return Status;

		//
		// link it
		//
		PcipLinkSecondaryExtension(&FdoExt->SecondaryExtension,&Instance->SecondaryExtension,&FdoExt->Common.SecondaryExtLock,CurrentType,&PciArbiterDestructor);

		PciDebugPrintf(0x7fffffff,"PCI - FDO ext 0x%08x %S arbiter initialized (context 0x%08x).\n",FdoExt,Instance->CommonInstance.Name,Instance);
	}

	return STATUS_SUCCESS;
}

//
// destructor [checked]
//
VOID PciArbiterDestructor(__in PPCI_ARBITER_INSTANCE Instance)
{
	ASSERT(!Instance->CommonInstance.ReferenceCount);
	ASSERT(!Instance->CommonInstance.TransactionInProgress);

	if(Instance->CommonInstance.ResourceType == CmResourceTypeMemory)
	{
		ASSERT(Instance->CommonInstance.Extension);

		PPCI_MEMORY_ARBITER_EXTENSION Ext				= static_cast<PPCI_MEMORY_ARBITER_EXTENSION>(Instance->CommonInstance.Extension);

		ArbFreeOrderingList(&Ext->PrefetchableOrderingList);

		ArbFreeOrderingList(&Ext->NormalOrderingList);

		ArbFreeOrderingList(&Ext->DefaultOrderingList);

		Instance->CommonInstance.OrderingList.Count		= 0;
		Instance->CommonInstance.OrderingList.Maximum	= 0;
		Instance->CommonInstance.OrderingList.Orderings	= 0;
	}

	ArbDeleteArbiterInstance(&Instance->CommonInstance);
}

//
// initialize arbiter ranges [checked]
//
NTSTATUS PciInitializeArbiterRanges(__in PPCI_FDO_EXTENSION FdoExt,__in PCM_RESOURCE_LIST CmResList)
{
	//
	// already initialized
	//
	if(FdoExt->ArbitersInitialized)
	{
		PciDebugPrintf(1,"PCI Warning hot start FDOx %08x, resource ranges not checked.\n",FdoExt);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	//
	// skip subtractive decode bridges
	//
	if(FdoExt != FdoExt->BusRootFdoExtension)
	{
		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);

		ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

		if(PdoExt->Dependent.type1.SubtractiveDecode)
		{
			PciDebugPrintf(1,"PCI Skipping arbiter initialization for subtractive bridge FDOX %p\n",FdoExt);

			return STATUS_SUCCESS;
		}
	}

	static PCI_SIGNATURE Types[2]						= {PciArb_Io,PciArb_Memory};
	static UCHAR ResType[2]								= {CmResourceTypePort,CmResourceTypeMemory};
	for(ULONG i = 0; i < ARRAYSIZE(Types); i ++)
	{
		//
		// find arbiter intance
		//
		PPCI_SECONDARY_EXTENSION SecondaryExtension		= PciFindNextSecondaryExtension(FdoExt->SecondaryExtension.Next,Types[i]);
		if(!SecondaryExtension)
		{
			PciDebugPrintf(1,"PCI - FDO ext 0x%08x %s arbiter (REQUIRED) is missing.\n",FdoExt,PciArbiterNames[i]);
		}
		else
		{
			//
			// start arbiter
			//
			PPCI_ARBITER_INSTANCE Instance				= CONTAINING_RECORD(SecondaryExtension,PCI_ARBITER_INSTANCE,SecondaryExtension);
			if(NT_SUCCESS(PciRangeListFromResourceList(FdoExt,CmResList,ResType[i],TRUE,Instance->CommonInstance.Allocation)))
			{
				ASSERT(Instance->CommonInstance.StartArbiter);

				NTSTATUS Status							= Instance->CommonInstance.StartArbiter(&Instance->CommonInstance,CmResList);
				if(!NT_SUCCESS(Status))
					return Status;
			}
		}
	}

	return STATUS_SUCCESS;
}

//
// initialize arbiter interface [checked]
//
NTSTATUS PciArbiterInitializeInterface(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SIGNATURE Type,__in PARBITER_INTERFACE Interface)
{
	PPCI_SECONDARY_EXTENSION SecondaryExtension			= PciFindNextSecondaryExtension(FdoExt->SecondaryExtension.Next,Type);

	if(!SecondaryExtension)
	{
		if(FdoExt->BusRootFdoExtension != FdoExt)
		{
			PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);
			ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

			if(PdoExt->Dependent.type1.SubtractiveDecode)
				return STATUS_INVALID_PARAMETER_2;
		}

		ASSERTMSG("couldn't locate arbiter for resource.",0);
		return STATUS_INVALID_PARAMETER_5;
	}

	PPCI_ARBITER_INSTANCE Arbiter						= CONTAINING_RECORD(SecondaryExtension,PCI_ARBITER_INSTANCE,SecondaryExtension);
	Interface->Context									= &Arbiter->CommonInstance;
	PciDebugPrintf(0x7fffffff,"PCI - %S Arbiter Interface Initialized.\n",Arbiter->CommonInstance.Name);

	return STATUS_SUCCESS;
}

//
// reference arbiter [checked]
//
VOID PciReferenceArbiter(__in PARBITER_INSTANCE Instance)
{
	InterlockedIncrement(&Instance->ReferenceCount);
}

//
// dereference arbiter [checked]
//
VOID PciDereferenceArbiter(__in PARBITER_INSTANCE Instance)
{
	InterlockedDecrement(&Instance->ReferenceCount);
}