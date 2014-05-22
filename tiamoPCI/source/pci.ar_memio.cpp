//********************************************************************
//	created:	23:7:2008   23:00
//	file:		pci.ar_memio.cpp
//	author:		tiamo
//	purpose:	arbiter for memory and port
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",ario_Constructor)
#pragma alloc_text("PAGE",ario_Initializer)
#pragma alloc_text("PAGE",armemio_UnpackRequirement)
#pragma alloc_text("PAGE",armemio_PackResource)
#pragma alloc_text("PAGE",armemio_UnpackResource)
#pragma alloc_text("PAGE",armemio_ScoreRequirement)
#pragma alloc_text("PAGE",ario_FindSuitableRange)
#pragma alloc_text("PAGE",ario_PreprocessEntry)
#pragma alloc_text("PAGE",ario_OverrideConflict)
#pragma alloc_text("PAGE",ario_BacktrackAllocation)
#pragma alloc_text("PAGE",ario_AddAllocation)
#pragma alloc_text("PAGE",armem_FindSuitableRange)
#pragma alloc_text("PAGE",armem_PreprocessEntry)
#pragma alloc_text("PAGE",armem_StartArbiter)
#pragma alloc_text("PAGE",armem_GetNextAllocationRange)
#pragma alloc_text("PAGE",ario_GetNextAlias)
#pragma alloc_text("PAGE",ario_IsBridge)
#pragma alloc_text("PAGE",ario_AddOrBacktrackAllocation)

//
// constructor [checked]
//
NTSTATUS ario_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
						  __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	if(reinterpret_cast<ULONG_PTR>(Data) != CmResourceTypePort)
		return STATUS_INVALID_PARAMETER_5;

	PARBITER_INTERFACE ArbInterface						= reinterpret_cast<PARBITER_INTERFACE>(Interface);
	ArbInterface->ArbiterHandler						= reinterpret_cast<PARBITER_HANDLER>(&ArbArbiterHandler);
	ArbInterface->InterfaceDereference					= reinterpret_cast<PINTERFACE_DEREFERENCE>(&PciDereferenceArbiter);
	ArbInterface->InterfaceReference					= reinterpret_cast<PINTERFACE_REFERENCE>(&PciReferenceArbiter);
	ArbInterface->Size									= sizeof(ARBITER_INTERFACE);
	ArbInterface->Version								= 0;
	ArbInterface->Flags									= 0;

	return PciArbiterInitializeInterface(CONTAINING_RECORD(CommonExt,PCI_FDO_EXTENSION,Common),PciArb_Io,ArbInterface);
}

//
// initializer [checked]
//
NTSTATUS ario_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	ASSERT(!Instance->BusFdoExtension->BrokenVideoHackApplied);

	RtlZeroMemory(&Instance->CommonInstance,sizeof(Instance->CommonInstance));

	Instance->CommonInstance.UnpackRequirement			= &armemio_UnpackRequirement;
	Instance->CommonInstance.PackResource				= &armemio_PackResource;
	Instance->CommonInstance.UnpackResource				= &armemio_UnpackResource;
	Instance->CommonInstance.ScoreRequirement			= &armemio_ScoreRequirement;
	Instance->CommonInstance.FindSuitableRange			= &ario_FindSuitableRange;
	Instance->CommonInstance.PreprocessEntry			= &ario_PreprocessEntry;
	Instance->CommonInstance.StartArbiter				= &ario_StartArbiter;
	Instance->CommonInstance.GetNextAllocationRange		= &ario_GetNextAllocationRange;
	Instance->CommonInstance.OverrideConflict			= &ario_OverrideConflict;
	Instance->CommonInstance.BacktrackAllocation		= &ario_BacktrackAllocation;
	Instance->CommonInstance.AddAllocation				= &ario_AddAllocation;

	return ArbInitializeArbiterInstance(&Instance->CommonInstance,Instance->BusFdoExtension->FunctionalDeviceObject,
										CmResourceTypePort,Instance->InstanceName,L"Pci",0);
}

//
// memory arbiter constructor [checked]
//
NTSTATUS armem_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
						   __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	if(reinterpret_cast<ULONG_PTR>(Data) != CmResourceTypeMemory)
		return STATUS_INVALID_PARAMETER_5;

	PARBITER_INTERFACE ArbInterface						= reinterpret_cast<PARBITER_INTERFACE>(Interface);
	ArbInterface->ArbiterHandler						= reinterpret_cast<PARBITER_HANDLER>(&ArbArbiterHandler);
	ArbInterface->InterfaceDereference					= reinterpret_cast<PINTERFACE_DEREFERENCE>(&PciDereferenceArbiter);
	ArbInterface->InterfaceReference					= reinterpret_cast<PINTERFACE_REFERENCE>(&PciReferenceArbiter);
	ArbInterface->Size									= sizeof(ARBITER_INTERFACE);
	ArbInterface->Version								= 0;
	ArbInterface->Flags									= 0;

	return PciArbiterInitializeInterface(CONTAINING_RECORD(CommonExt,PCI_FDO_EXTENSION,Common),PciArb_Memory,ArbInterface);
}

//
// memory arbiter initializer [checked]
//
NTSTATUS armem_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	RtlZeroMemory(&Instance->CommonInstance,sizeof(Instance->CommonInstance));

	Instance->CommonInstance.UnpackRequirement			= &armemio_UnpackRequirement;
	Instance->CommonInstance.PackResource				= &armemio_PackResource;
	Instance->CommonInstance.UnpackResource				= &armemio_UnpackResource;
	Instance->CommonInstance.ScoreRequirement			= &armemio_ScoreRequirement;
	Instance->CommonInstance.FindSuitableRange			= &armem_FindSuitableRange;
	Instance->CommonInstance.PreprocessEntry			= &armem_PreprocessEntry;
	Instance->CommonInstance.StartArbiter				= &armem_StartArbiter;
	Instance->CommonInstance.GetNextAllocationRange		= &armem_GetNextAllocationRange;
	Instance->CommonInstance.Extension					= PciAllocateColdPoolWithTag(PagedPool,sizeof(PCI_MEMORY_ARBITER_EXTENSION),'BicP');
	if(!Instance->CommonInstance.Extension)
		return STATUS_INSUFFICIENT_RESOURCES;

	RtlZeroMemory(Instance->CommonInstance.Extension,sizeof(PCI_MEMORY_ARBITER_EXTENSION));

	return ArbInitializeArbiterInstance(&Instance->CommonInstance,Instance->BusFdoExtension->FunctionalDeviceObject,
										CmResourceTypeMemory,Instance->InstanceName,L"Pci",0);
}

//
// unpack requirement [checked]
//
NTSTATUS armemio_UnpackRequirement(__in PIO_RESOURCE_DESCRIPTOR Descriptor,__out PULONGLONG Minimum,__out PULONGLONG Maximum,__out PULONG Length,__out PULONG Alignment)
{
	PAGED_CODE();

	ASSERT(Descriptor);
	ASSERT(Descriptor->Type == CmResourceTypePort || Descriptor->Type == CmResourceTypeMemory);


	*Minimum											= static_cast<ULONGLONG>(Descriptor->u.Generic.MinimumAddress.QuadPart);
	*Maximum											= static_cast<ULONGLONG>(Descriptor->u.Generic.MaximumAddress.QuadPart);
	*Length												= Descriptor->u.Generic.Length;
	*Alignment											= Descriptor->u.Generic.Alignment;

	//
	// fix the broken hardware that reports 0 alignment
	//
	if(*Alignment == 0)
		*Alignment										= 1;

	//
	// fix broken INF's that report they support 24bit memory > 0xFFFFFF
	//
	if(Descriptor->Type == CmResourceTypeMemory && FlagOn(Descriptor->Flags,CM_RESOURCE_MEMORY_24) && Descriptor->u.Memory.MaximumAddress.QuadPart > 0xffffff)
	{
		if(Descriptor->u.Memory.MinimumAddress.QuadPart > 0xffffff)
		{
			PciDebugPrintf(0,"24 bit decode specified but both min and max are greater than 0xffffff, most probably due to broken INF!\n");
			ASSERT(Descriptor->u.Memory.MinimumAddress.QuadPart <= 0xffffff);

			return STATUS_UNSUCCESSFUL;
		}
	
		*Maximum									= 0xffffff;
	}

	return STATUS_SUCCESS;
}

//
// pack resource [checked]
//
NTSTATUS armemio_PackResource(__in PIO_RESOURCE_DESCRIPTOR Requirement,__in ULONGLONG Start,__out PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor)
{
	PAGED_CODE();

	ASSERT(Descriptor);
	ASSERT(Requirement);
	ASSERT(Requirement->Type == CmResourceTypePort || Requirement->Type == CmResourceTypeMemory);

	Descriptor->Type									= Requirement->Type;
	Descriptor->Flags									= Requirement->Flags;
	Descriptor->ShareDisposition						= Requirement->ShareDisposition;
	Descriptor->u.Generic.Start.QuadPart				= Start;
	Descriptor->u.Generic.Length						= Requirement->u.Generic.Length;

	return STATUS_SUCCESS;
}

//
// unpack resource [checked]
//
NTSTATUS armemio_UnpackResource(__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor,__out PULONGLONG Start,__out PULONG Length)
{
	PAGED_CODE();

	ASSERT(Descriptor);
	ASSERT(Descriptor->Type == CmResourceTypePort || Descriptor->Type == CmResourceTypeMemory);

	*Start												= Descriptor->u.Generic.Start.QuadPart;
	*Length												= Descriptor->u.Generic.Length;

	return STATUS_SUCCESS;
}

//
// score requirement [checked]
//
LONG armemio_ScoreRequirement(__in PIO_RESOURCE_DESCRIPTOR Descriptor)
{
	PAGED_CODE();

	ASSERT(Descriptor);
	ASSERT(Descriptor->Type == CmResourceTypePort || Descriptor->Type == CmResourceTypeMemory);

	ULONG Alignment										= Descriptor->u.Generic.Alignment;

	//
	// fix the broken hardware that reports 0 alignment
	//
	if(Alignment == 0)
		Alignment										= 1;

	ULONGLONG Start										= ALIGN_ADDRESS_UP(Descriptor->u.Generic.MinimumAddress.QuadPart,Alignment);
	ULONGLONG End										= Descriptor->u.Generic.MaximumAddress.QuadPart;

	//
	// the score is the number of possible allocations that could be made given the alignment and length constraints
	//
	ULONGLONG Score										= (End - Start - Descriptor->u.Generic.Length + 1) / Alignment + 1;

	LONG Ret											= 7;
	PUCHAR Buffer										= reinterpret_cast<PUCHAR>(&Score);

	for(; Ret >= 0; Ret -= 1)
	{
		if(Buffer[Ret])
		{
			Ret											= (Buffer[Ret] + 0x100) << Ret;
			break;
		}
	}

	if(FlagOn(Descriptor->Option,IO_RESOURCE_PREFERRED | IO_RESOURCE_ALTERNATIVE))
		Ret												-= 0x100;

	return Ret;
}

//
// port find range [checked]
//
BOOLEAN ario_FindSuitableRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	ClearFlag(State->Flags,ARBITER_STATE_FLAG_NULL_CONFLICT_OK);

	//
	// window decode
	//
	if(FlagOn(State->WorkSpace,PCI_IO_ARBITER_WINDOW_DECODE))
	{
		ASSERT(State->Entry->PhysicalDeviceObject->DriverObject == PciDriverObject);

		PPCI_FDO_EXTENSION FdoExt						= static_cast<PPCI_PDO_EXTENSION>(State->Entry->PhysicalDeviceObject->DeviceExtension)->ParentFdoExtension;
		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);

		ASSERT(FdoExt == FdoExt->BusRootFdoExtension || PdoExt);

		if(FdoExt == FdoExt->BusRootFdoExtension || (PdoExt->HeaderType == PCI_BRIDGE_TYPE && !PdoExt->MovedDevice))
		{
			if(FlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_FIXED))
				SetFlag(State->Flags,ARBITER_STATE_FLAG_NULL_CONFLICT_OK);
		}

		//
		// isa bit
		//
		if(FlagOn(State->WorkSpace,PCI_IO_ARBITER_BRIDGE_WITH_ISA_BIT) && State->CurrentMaximum <= 0xffff)
			return ario_FindWindowWithIsaBit(Arbiter,State);
	}

	if( State->Entry->RequestSource == ArbiterRequestLegacyReported ||
		State->Entry->RequestSource == ArbiterRequestLegacyAssigned ||
		FlagOn(State->Entry->Flags,ARBITER_FLAG_BOOT_CONFIG))
	{
		SetFlag(State->RangeAvailableAttributes,ARBITER_RANGE_BOOT_ALLOCATED);
	}

	if(FlagOn(State->CurrentAlternative->Descriptor->Flags,CM_RESOURCE_PORT_POSITIVE_DECODE))
		SetFlag(State->RangeAvailableAttributes,ARBITER_RANGE_ALIAS);

	while(State->CurrentMinimum <= State->CurrentMaximum)
	{
		//
		// find a suitable range
		//
		if(!ArbFindSuitableRange(Arbiter,State))
			return FALSE;

		//
		// zero-length request
		//
		if(!State->CurrentAlternative->Length)
		{
			State->Entry->Result						= ArbiterResultNullRequest;
			return TRUE;
		}

		//
		// make sure all aliased range are available
		//
		if(ario_IsAliasedRangeAvailable(Arbiter,State))
			return TRUE;

		//
		// wrapped
		//
		if(State->Start - 1 > State->Start)
			return FALSE;

		State->CurrentMaximum							= State->Start - 1;
	}

	return FALSE;
}

//
// preprocess [checked]
//
NTSTATUS ario_PreprocessEntry(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	//
	// already process
	//
	if(FlagOn(State->WorkSpace,PCI_IO_ARBITER_PREPROCESSED))
		return STATUS_SUCCESS;

	SetFlag(State->WorkSpace,PCI_IO_ARBITER_PREPROCESSED);

	//
	// we can not assign a pnp enumerated resource to a legacy driver
	//
	if(State->Entry->PhysicalDeviceObject->DriverObject == PciDriverObject && State->Entry->RequestSource == ArbiterRequestPnpEnumerated)
	{
		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(State->Entry->PhysicalDeviceObject->DeviceExtension);
		ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

		if(PdoExt->LegacyDriver)
			return STATUS_DEVICE_BUSY;
	}

	ULONGLONG Max										= 0;
	BOOLEAN WindowDetected								= FALSE;
	BOOLEAN FixDescriptor								= FALSE;
	for(ULONG i = 0; i < State->AlternativeCount; i ++)
	{
		ASSERT(State->Alternatives[i].Descriptor->Type == CmResourceTypePort);
		ASSERT(State->Alternatives[i].Descriptor->Flags == State->Alternatives[0].Descriptor->Flags);

		//
		// count the max port address
		//
		if(State->Alternatives[i].Maximum > Max)
			Max											= State->Alternatives[i].Maximum;

		//
		// window decode
		//
		if(FlagOn(State->Alternatives[i].Flags,CM_RESOURCE_PORT_WINDOW_DECODE))
		{
			if(i != 0)
				ASSERT(WindowDetected);

			WindowDetected								= TRUE;
		}

		ULONG Flags										= State->Alternatives[i].Descriptor->Flags;

		//
		// if all the descriptors did not set decode size,we should set it according to the interface type
		//
		if(!FlagOn(Flags,CM_RESOURCE_PORT_POSITIVE_DECODE | CM_RESOURCE_PORT_16_BIT_DECODE | CM_RESOURCE_PORT_12_BIT_DECODE | CM_RESOURCE_PORT_10_BIT_DECODE))
			FixDescriptor								= TRUE;
	}

	if(FixDescriptor)
	{
		//
		// we set decode length
		//
		SetFlag(State->WorkSpace,PCI_IO_ARBITER_ALIAS_AVAILABLE);

		INTERFACE_TYPE Type								= State->Entry->InterfaceType;
		ULONG Flags										= CM_RESOURCE_PORT_10_BIT_DECODE;

		switch(Type)
		{
		case Isa:
		case PNPISABus:
			if(!IsNEC_98 && Max < 0x400)
				break;

		case Internal:
		case Eisa:
		case MicroChannel:
		case PCMCIABus:
		default:
			Flags										= CM_RESOURCE_PORT_16_BIT_DECODE;
			break;

		case PCIBus:
			Flags										= CM_RESOURCE_PORT_POSITIVE_DECODE;
			break;
		}

		//
		// set decode length
		//
		for(ULONG i = 0; i < State->AlternativeCount; i ++)
			SetFlag(State->Alternatives[i].Descriptor->Flags,Flags);
	}
	else
	{
		//
		// force 16bits decode if the max port address is bigger than 0x400 (1 << 10)
		//
		for(ULONG i = 0; i < State->AlternativeCount; i ++)
		{
			if(FlagOn(State->Alternatives[i].Descriptor->Flags,CM_RESOURCE_PORT_10_BIT_DECODE) && Max >= 0x400)
			{
				ClearFlag(State->Alternatives[i].Descriptor->Flags,CM_RESOURCE_PORT_10_BIT_DECODE);
				SetFlag(State->Alternatives[i].Descriptor->Flags,CM_RESOURCE_PORT_16_BIT_DECODE);
			}
		}
	}

	if(WindowDetected)
	{
		ASSERT(State->Entry->PhysicalDeviceObject->DriverObject == PciDriverObject);

		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(State->Entry->PhysicalDeviceObject->DeviceExtension);
		ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

		ULONG DeviceType								= PciClassifyDeviceType(PdoExt);
		ASSERT(DeviceType == PCI_DEVICE_TYPE_CARDBUS || DeviceType == PCI_DEVICE_TYPE_PCI_TO_PCI);
		if(DeviceType != PCI_DEVICE_TYPE_PCI_TO_PCI && DeviceType != PCI_DEVICE_TYPE_CARDBUS)
			return STATUS_INVALID_PARAMETER;

		if(PdoExt->Dependent.type1.IsaBitSet)
		{
			ASSERT(DeviceType == PCI_DEVICE_TYPE_PCI_TO_PCI);

			//
			// isa bit
			//
			SetFlag(State->WorkSpace,PCI_IO_ARBITER_BRIDGE_WITH_ISA_BIT);
		}

		//
		// window decode
		//
		SetFlag(State->WorkSpace,PCI_IO_ARBITER_WINDOW_DECODE);
	}

	if(FlagOn(State->Alternatives->Descriptor->Flags,CM_RESOURCE_PORT_POSITIVE_DECODE))
		SetFlag(State->RangeAttributes,ARBITER_RANGE_POSITIVE_DECODE);

	return STATUS_SUCCESS;
}

//
// start io arbiter [checked]
//
NTSTATUS ario_StartArbiter(__in PARBITER_INSTANCE Arbiter,__in PCM_RESOURCE_LIST StartResources)
{
	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		ArbAcquireArbiterLock(Arbiter);

		PPCI_FDO_EXTENSION FdoExt						= static_cast<PPCI_FDO_EXTENSION>(Arbiter->BusDeviceObject->DeviceExtension);
		ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

		if(!StartResources || FdoExt == FdoExt->BusRootFdoExtension)
			try_leave(Status = STATUS_SUCCESS);

		ASSERT(StartResources->Count == 1);

		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);
		PRTL_RANGE_LIST RangeList						= 0;

		//
		// exclude from isa or vgaisa range list
		//
		if(PdoExt->Dependent.type1.IsaBitSet)
		{
			if(PdoExt->Dependent.type1.VgaBitSet)
				RangeList								= &PciVgaAndIsaBitExclusionList;
			else
				RangeList								= &PciIsaBitExclusionList;
		}

		for(ULONG i = 0; i < StartResources->List->PartialResourceList.Count; i ++)
		{
			if(StartResources->List->PartialResourceList.PartialDescriptors[i].Type == CmResourceTypePort)
			{
				ULONGLONG Start							= static_cast<ULONGLONG>(StartResources->List->PartialResourceList.PartialDescriptors->u.Port.Start.QuadPart);
				ULONGLONG End							= Start + StartResources->List->PartialResourceList.PartialDescriptors->u.Port.Length - 1;

				if(RangeList)
				{
					//
					// if current descriptor has intersection with exclude range list,remove it from allocation range list
					//
					Status								= PciExcludeRangesFromWindow(Start,End,Arbiter->Allocation,RangeList);
					if(!NT_SUCCESS(Status))
						try_leave(NOTHING);
				}

				PPCI_SECONDARY_EXTENSION SecondaryExt	= PciFindNextSecondaryExtension(FdoExt->BusRootFdoExtension->SecondaryExtension.Next,PciArb_Io);
				if(!SecondaryExt)
					try_leave(Status = STATUS_INVALID_PARAMETER);

				PPCI_ARBITER_INSTANCE RootArbiter		= CONTAINING_RECORD(SecondaryExt,PCI_ARBITER_INSTANCE,SecondaryExtension);

				ArbAcquireArbiterLock(&RootArbiter->CommonInstance);

				//
				// also check root's allocation range list
				//
				PciExcludeRangesFromWindow(Start,End,Arbiter->Allocation,RootArbiter->CommonInstance.Allocation);
			
				ArbReleaseArbiterLock(&RootArbiter->CommonInstance);

				//
				// result an empty range?
				//
				Status									= RtlFindRange(Arbiter->Allocation,0,0xffffffffffffffff,4,4,0,0,0,0,&Start);
				if(!NT_SUCCESS(Status))
					Status								= STATUS_INSUFFICIENT_RESOURCES;

				try_leave(NOTHING);
			}
		}
	}
	__finally
	{
		ArbReleaseArbiterLock(Arbiter);
	}

	return Status;
}

//
// get next range [checked]
//
BOOLEAN ario_GetNextAllocationRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	ARBITER_ORDERING_LIST OrderingList					= Arbiter->OrderingList;
	BOOLEAN UseBridgeOrderingList						= TRUE;

	//
	// isa window decode we use a buid-in ordering list,otherwise use the default ordering list came from registry
	//
	if(FlagOn(State->WorkSpace,PCI_IO_ARBITER_ISA_WINDOW) == PCI_IO_ARBITER_ISA_WINDOW)
		Arbiter->OrderingList							= PciBridgeOrderingList;
	else
		UseBridgeOrderingList							= FALSE;

	BOOLEAN Ret											= ArbGetNextAllocationRange(Arbiter,State);
	if(!UseBridgeOrderingList)
		return Ret;

	//
	// build-in ordering list should not return an alternative with priority greater than reserved
	//
	if(Ret && State->CurrentAlternative->Priority > ARBITER_PRIORITY_PREFERRED_RESERVED)
		Ret												= FALSE;

	Arbiter->OrderingList								= OrderingList;

	return Ret;
}

//
// overide conflict [checked]
//
BOOLEAN ario_OverrideConflict(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	if(!FlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_FIXED))
		return FALSE;

	RTL_RANGE_LIST_ITERATOR Iterator;
	PRTL_RANGE Range									= 0;
	BOOLEAN Ret											= FALSE;

	FOR_ALL_RANGES(Arbiter->PossibleAllocation,&Iterator,Range)
	{
		if(INTERSECT(Range->Start,Range->End,State->CurrentMinimum,State->CurrentMaximum))
		{
			if(FlagOn(Range->Attributes,State->RangeAvailableAttributes))
				continue;

			if(Range->Owner != State->Entry->PhysicalDeviceObject || !FlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_FIXED))
			{
				if(!FlagOn(State->CurrentAlternative->Descriptor->Flags,CM_RESOURCE_PORT_PASSIVE_DECODE))
					return FALSE;

				if(!ario_IsBridge(State->Entry->PhysicalDeviceObject) && Range->Owner)
					return FALSE;
			}

			State->Start								= State->CurrentMinimum;
			State->End									= State->CurrentMaximum;
			Ret											= TRUE;
		}
	}

	return Ret;
}

//
// backtrack allocation [checked]
//
VOID ario_BacktrackAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	ario_AddOrBacktrackAllocation(Arbiter,State,&ArbBacktrackAllocation);
}

//
// add port allocation [checked]
//
VOID ario_AddAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	ario_AddOrBacktrackAllocation(Arbiter,State,&ArbAddAllocation);
}

//
// memory find range [checked]
//
BOOLEAN armem_FindSuitableRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	//
	// if this was a boot config then consider other boot configs to be available
	//
	if(FlagOn(State->Entry->Flags,ARBITER_FLAG_BOOT_CONFIG))
		SetFlag(State->RangeAvailableAttributes,ARBITER_RANGE_BOOT_ALLOCATED);

	//
	// do the default thing
	//
	return ArbFindSuitableRange(Arbiter, State);
}

//
// according to the prefetchable flags of the descriptor,we change the ordering list [checked]
//
NTSTATUS armem_PreprocessEntry(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	PPCI_MEMORY_ARBITER_EXTENSION MemExt				= static_cast<PPCI_MEMORY_ARBITER_EXTENSION>(Arbiter->Extension);

	//
	// if this is a pnp enumerated request,we can't assign those resources to a legacy driver
	//
	if(State->Entry->PhysicalDeviceObject->DriverObject == PciDriverObject && State->Entry->RequestSource == ArbiterRequestPnpEnumerated)
	{
		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(State->Entry->PhysicalDeviceObject->DeviceExtension);
		ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

		if(PdoExt->LegacyDriver)
			return STATUS_DEVICE_BUSY;
	}

	if(FlagOn(State->Alternatives[0].Descriptor->Flags,CM_RESOURCE_MEMORY_READ_ONLY) ||
	   (FlagOn(State->Alternatives[0].Flags,ARBITER_ALTERNATIVE_FLAG_FIXED) && State->AlternativeCount == 1 && State->Entry->RequestSource == ArbiterRequestLegacyReported))
	{
		if(FlagOn(State->Alternatives[0].Descriptor->Flags,CM_RESOURCE_MEMORY_READ_ONLY))
			ASSERT(FlagOn(State->Alternatives[0].Flags,ARBITER_ALTERNATIVE_FLAG_FIXED));

		SetFlag(State->RangeAttributes,ARBITER_RANGE_ALIAS);
		SetFlag(State->RangeAvailableAttributes,ARBITER_RANGE_ALIAS);
		SetFlag(State->Flags,ARBITER_STATE_FLAG_NULL_CONFLICT_OK);
	}

	//
	// setup correct ordering list
	//
	if(MemExt->HasPrefetchableResource)
	{
		//
		// for legacy reported request,we always use default ordering list(came from registry)
		//
		if(State->Entry->RequestSource == ArbiterRequestLegacyReported)
		{
			Arbiter->OrderingList						= MemExt->DefaultOrderingList;
			return STATUS_SUCCESS;
		}

		//
		// if this is a prefetchable descriptor,use the prefetchable ordering list,otherwise use the normal ordering list
		//
		BOOLEAN Prefetchable							= BooleanFlagOn(State->Alternatives[0].Descriptor->Flags,CM_RESOURCE_MEMORY_PREFETCHABLE);
		if(Prefetchable)
			Arbiter->OrderingList						= MemExt->PrefetchableOrderingList;
		else
			Arbiter->OrderingList						= MemExt->NormalOrderingList;

		//
		// make sure all the descriptors have the same prefetchable setting
		//
		for(ULONG i = 0; i < State->AlternativeCount; i ++)
			ASSERT(BooleanFlagOn(State->Alternatives[i].Descriptor->Flags,CM_RESOURCE_MEMORY_PREFETCHABLE) == Prefetchable);
	}

	return STATUS_SUCCESS;
}

//
// start memory arbiter [checked]
//
NTSTATUS armem_StartArbiter(__in PARBITER_INSTANCE Arbiter,__in PCM_RESOURCE_LIST StartResources)
{
	PAGED_CODE();

	PPCI_MEMORY_ARBITER_EXTENSION MemExt				= static_cast<PPCI_MEMORY_ARBITER_EXTENSION>(Arbiter->Extension);

	//
	// copy the default ordering list,and empty the arbiter's ordering list
	//
	if(!MemExt->Initialized)
	{
		MemExt->DefaultOrderingList						= Arbiter->OrderingList;
		Arbiter->OrderingList.Count						= 0;
		Arbiter->OrderingList.Maximum					= 0;
		Arbiter->OrderingList.Orderings					= 0;
	}
	else if(MemExt->HasPrefetchableResource)
	{
		//
		// free previous ordering list
		//
		ArbFreeOrderingList(&MemExt->PrefetchableOrderingList);
		ArbFreeOrderingList(&MemExt->NormalOrderingList);
	}

	MemExt->PrefetchableResourceCount					= 0;
	MemExt->HasPrefetchableResource						= FALSE;

	//
	// check our start resource to see whether we have a prefetchable resource or not
	//
	if(StartResources)
	{
		ASSERT(StartResources->Count == 1);
		for(ULONG i = 0; i < StartResources->List->PartialResourceList.Count; i ++)
		{
			PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc		= StartResources->List->PartialResourceList.PartialDescriptors + i;
			if(Desc->Type == CmResourceTypeMemory && FlagOn(Desc->Flags,CM_RESOURCE_MEMORY_PREFETCHABLE))
			{
				MemExt->HasPrefetchableResource			= TRUE;
				break;
			}
		}
	}

	//
	// system wide hack flags prevent us from doing pretchable processing
	//
	if(FlagOn(PciSystemWideHackFlags,PCI_SYSTEM_HACK_ROOT_NO_PREFETCH_MEMORY))
	{
		PPCI_FDO_EXTENSION FdoExt						= static_cast<PPCI_FDO_EXTENSION>(Arbiter->BusDeviceObject->DeviceExtension);
		ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

		if(FdoExt == FdoExt->BusRootFdoExtension)
			MemExt->HasPrefetchableResource				= FALSE;
	}

	//
	// restore default ordering list to arbiter,if we don't support prefetchable memory
	//
	if(!MemExt->HasPrefetchableResource)
	{
		Arbiter->OrderingList							= MemExt->DefaultOrderingList;
		return STATUS_SUCCESS;
	}

	//
	// build ordering lists for prefetchable processing
	//
	NTSTATUS Status										= ArbInitializeOrderingList(&MemExt->PrefetchableOrderingList);
	if(!NT_SUCCESS(Status))
		return Status;

	Status												= ArbInitializeOrderingList(&MemExt->NormalOrderingList);
	if(!NT_SUCCESS(Status))
		return Status;

	//
	// copy default ordering list
	//
	Status												= ArbCopyOrderingList(&MemExt->NormalOrderingList,&MemExt->DefaultOrderingList);
	if(!NT_SUCCESS(Status))
		return Status;

	Status												= ArbAddOrdering(&MemExt->NormalOrderingList,0,0xffffffffffffffff);
	if(!NT_SUCCESS(Status))
		return Status;

	PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc				= StartResources->List->PartialResourceList.PartialDescriptors;

	for(ULONG i = 0; i < StartResources->List->PartialResourceList.Count; i ++,Desc ++)
	{
		if(Desc->Type == CmResourceTypeMemory && FlagOn(Desc->Flags,CM_RESOURCE_MEMORY_PREFETCHABLE))
		{
			MemExt->PrefetchableResourceCount			+= 1;
			ULONGLONG Start								= static_cast<ULONGLONG>(Desc->u.Memory.Start.QuadPart);
			ULONGLONG End								= Start + Desc->u.Memory.Length - 1;

			//
			// add [Start,End] in the descriptor to prefecthable ordering list
			//
			Status										= ArbAddOrdering(&MemExt->PrefetchableOrderingList,Start,End);
			if(!NT_SUCCESS(Status))
				return Status;

			//
			// and remove it from normal ordering list
			// make sure we do not return a prefetchable range for those memory request without prefethable flags set
			//
			Status										= ArbPruneOrdering(&MemExt->NormalOrderingList,Start,End);
			if(!NT_SUCCESS(Status))
				return Status;
		}
	}

	//
	// remove reserved range from pretechable list
	//
	for(ULONG i = 0; i < Arbiter->ReservedList.Count; i ++)
	{
		ULONGLONG Start									= Arbiter->ReservedList.Orderings[i].Start;
		ULONGLONG End									= Arbiter->ReservedList.Orderings[i].End;
		Status											= ArbPruneOrdering(&MemExt->PrefetchableOrderingList,Start,End);
		if(!NT_SUCCESS(Status))
			return Status;
	}

	//
	// add normal range to pretechable list
	// if we can not allocate a pretachable range,this allows us to return a normal range
	//
	for(ULONG i = 0; i < MemExt->NormalOrderingList.Count; i ++)
	{
		ULONGLONG Start									= MemExt->NormalOrderingList.Orderings[i].Start;
		ULONGLONG End									= MemExt->NormalOrderingList.Orderings[i].End;
		Status											= ArbAddOrdering(&MemExt->PrefetchableOrderingList,Start,End);
		if(!NT_SUCCESS(Status))
			return Status;
	}

	MemExt->Initialized										= TRUE;

	return STATUS_SUCCESS;
}

//
// get next range
//
BOOLEAN armem_GetNextAllocationRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	//
	// if arb package can't find a valid range
	//
	if(!ArbGetNextAllocationRange(Arbiter,State))
		return FALSE;

	//
	// check prefetchable setting
	//
	if(!static_cast<PPCI_MEMORY_ARBITER_EXTENSION>(Arbiter->Extension)->HasPrefetchableResource)
		return TRUE;

	//
	// we always use default ordering list for legacy reported request
	//
	if(State->Entry->RequestSource == ArbiterRequestLegacyReported)
		return TRUE;

	//
	// got here means that we are using prefetchable ordering list or normal ordering list
	// remember that we add a range from 0 to 0xffffffffffffffff to normal ordering list and prefetchable ordering list
	// this range should be always valid for any request,so the priority should always be less than reserved
	// if we got a reserved priority which means the request descriptor is buggy
	//
	if(State->CurrentAlternative->Priority <= ARBITER_PRIORITY_PREFERRED_RESERVED)
		return TRUE;

	return FALSE;
}

//
// get next alias port [checked]
//
BOOLEAN ario_GetNextAlias(__in ULONG IoDescriptorFlags,__in ULONGLONG LastAlias,__out PULONGLONG NextAlias)
{
	PAGED_CODE();

	ULONGLONG Next										= 0;

	if(FlagOn(IoDescriptorFlags,CM_RESOURCE_PORT_10_BIT_DECODE))
		Next											= LastAlias + (1 << 10);
	else if(FlagOn(IoDescriptorFlags,CM_RESOURCE_PORT_12_BIT_DECODE))
		Next											= LastAlias + (1 << 12);
	else
		return FALSE;

	//
	// check that we are below the maximum aliased port
	//
	if(Next > 0xffff)
		return FALSE;

	*NextAlias											= Next;

	return TRUE;
}

//
// is aliased range available [checked]
//
BOOLEAN ario_IsAliasedRangeAvailable(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	//
	// we guessed decode length
	//
	if(FlagOn(State->WorkSpace,PCI_IO_ARBITER_ALIAS_AVAILABLE))
		return TRUE;

	//
	// for legacy requests from IoAssignResources (directly or by way of HalAssignSlotResources) or IoReportResourceUsage
	// we consider preallocated resources to be available for backward compatibility reasons.
	//
	UCHAR UserFlagsMask									= ARBITER_RANGE_POSITIVE_DECODE;
	if( State->Entry->RequestSource == ArbiterRequestLegacyReported ||
		State->Entry->RequestSource == ArbiterRequestLegacyAssigned ||
		FlagOn(State->Entry->Flags,ARBITER_FLAG_BOOT_CONFIG))
	{
		SetFlag(UserFlagsMask,ARBITER_RANGE_BOOT_ALLOCATED);
	}

	ULONGLONG Alias										= State->Start;
	while(ario_GetNextAlias(State->CurrentAlternative->Descriptor->Flags,Alias,&Alias))
	{
		ULONG Flags										= BooleanFlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_SHARED) ? RTL_RANGE_LIST_SHARED_OK : 0;
		ULONGLONG End									= Alias + State->CurrentAlternative->Length - 1;
		PVOID Context									= Arbiter->ConflictCallbackContext;
		PRTL_CONFLICT_RANGE_CALLBACK Callback			= Arbiter->ConflictCallback;
		BOOLEAN	Available								= FALSE;
		SetFlag(Flags,RTL_RANGE_LIST_NULL_CONFLICT_OK);
		NTSTATUS Status									= RtlIsRangeAvailable(Arbiter->PossibleAllocation,Alias,End,Flags,UserFlagsMask,Context,Callback,&Available);

		ASSERT(NT_SUCCESS(Status));

		if(!Available)
		{
			ARBITER_ALLOCATION_STATE TempState;

			//
			// check if we allow this conflict by calling OverrideConflict - we will need to falsify ourselves an allocation state first
			//
			// BUGBUG - this works but relies on knowing what OverrideConflict looks at.
			// A better fix invloves storing the aliases in another list but this it too much of a change for Win2k
			//
			RtlCopyMemory(&TempState,State,sizeof(ARBITER_ALLOCATION_STATE));

			TempState.CurrentMinimum					= Alias;
			TempState.CurrentMaximum					= End;

			if(Arbiter->OverrideConflict(Arbiter,&TempState))
			{
				//
				// we decided this conflict was ok so contine checking the rest of the aliases
				//
				continue;
			}

			//
			// an alias isn't available - get another possibility
			//
			return FALSE;
		}
	}

	return TRUE;
}

//
// apply broken video hack [checked]
//
VOID ario_ApplyBrokenVideoHack(__in PPCI_FDO_EXTENSION FdoExt)
{
	ASSERT(!FdoExt->BrokenVideoHackApplied);
	ASSERT(FdoExt == FdoExt->BusRootFdoExtension);

	PPCI_SECONDARY_EXTENSION SecondaryExtension			= PciFindNextSecondaryExtension(FdoExt->SecondaryExtension.Next,PciArb_Io);
	ASSERT(SecondaryExtension);

	PPCI_ARBITER_INSTANCE Arbiter						= CONTAINING_RECORD(SecondaryExtension,PCI_ARBITER_INSTANCE,SecondaryExtension);

	ArbFreeOrderingList(&Arbiter->CommonInstance.OrderingList);

	ArbFreeOrderingList(&Arbiter->CommonInstance.ReservedList);

	NTSTATUS Status										= ArbBuildAssignmentOrdering(&Arbiter->CommonInstance,L"Pci",L"BrokenVideo",0);
	ASSERT(NT_SUCCESS(Status));

	FdoExt->BrokenVideoHackApplied						= TRUE;
}

//
// is bridge [checked]
//
BOOLEAN ario_IsBridge(__in PDEVICE_OBJECT Pdo)
{
	PAGED_CODE();

	BOOLEAN Ret											= FALSE;
	PPCI_FDO_EXTENSION FdoExt							= CONTAINING_RECORD(PciFdoExtensionListHead.Next,PCI_FDO_EXTENSION,Common.ListEntry);
	while(FdoExt)
	{
		if(FdoExt->PhysicalDeviceObject == Pdo)
		{
			ULONG DeviceType							= PciClassifyDeviceType(static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension));
			if(DeviceType == PCI_DEVICE_TYPE_CARDBUS || DeviceType == PCI_DEVICE_TYPE_PCI_TO_PCI)
			{
				Ret										= TRUE;
				break;
			}
		}

		FdoExt											= CONTAINING_RECORD(FdoExt->Common.ListEntry.Next,PCI_FDO_EXTENSION,Common.ListEntry);
	}

	return Ret;
}

//
// find windows with isa bit [checked]
//
BOOLEAN ario_FindWindowWithIsaBit(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	ASSERT(State->Entry->PhysicalDeviceObject->DriverObject == PciDriverObject);

	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(State->Entry->PhysicalDeviceObject->DeviceExtension);

	ASSERT(PciClassifyDeviceType(PdoExt) == PCI_DEVICE_TYPE_PCI_TO_PCI);
	ASSERT(FlagOn(State->CurrentAlternative->Descriptor->Flags,CM_RESOURCE_PORT_POSITIVE_DECODE));

	SetFlag(State->RangeAvailableAttributes,ARBITER_RANGE_ALIAS);

	ASSERT(State->CurrentAlternative->Length % State->CurrentAlternative->Alignment == 0);
	ASSERT(State->CurrentMinimum % State->CurrentAlternative->Alignment == 0);
	ASSERT((State->CurrentMaximum + 1) % State->CurrentAlternative->Alignment == 0);

	ULONG Flags											= 0;
	if(FlagOn(State->Flags,ARBITER_STATE_FLAG_NULL_CONFLICT_OK))
		SetFlag(Flags,RTL_RANGE_LIST_NULL_CONFLICT_OK);

	if(FlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_SHARED))
		SetFlag(Flags,RTL_RANGE_LIST_SHARED_OK);

	if(State->CurrentMaximum < static_cast<ULONG>(State->CurrentMinimum) + 1)
		return FALSE;

	ULONGLONG Test										= static_cast<ULONG>(State->CurrentMinimum) + 1;
	BOOLEAN Available									= FALSE;

	while(!Available)
	{
		if(Test < State->CurrentMinimum)
			break;

		if(Test < Test + State->CurrentAlternative->Length - 1)
		{
			ULONGLONG Start								= Test;

			while(Start < 0xffff)
			{
				PVOID Context							= Arbiter->ConflictCallbackContext;
				PRTL_CONFLICT_RANGE_CALLBACK Callback	= Arbiter->ConflictCallback;
				ULONGLONG End							= Start + 0xff;
				UCHAR RangeFlags						= State->RangeAvailableAttributes;
				NTSTATUS Status							= RtlIsRangeAvailable(Arbiter->PossibleAllocation,Start,End,Flags,RangeFlags,Context,Callback,&Available);

				ASSERT(NT_SUCCESS(Status));

				if(!Available)
					break;

				Start									+= 0x400;

				if(Start >= Test + State->CurrentAlternative->Length - 1)
					break;
			}
		}

		if(Available)
		{
			State->Start								= Test;
			State->End									= Test + State->CurrentAlternative->Length - 1;
		
			ASSERT(State->Start >= State->CurrentMinimum);
			ASSERT(State->End <= State->CurrentMaximum);
			break;
		}
		else
		{
			if(Test < 0x1000)
				break;

			Test										-= 0x1000;
		}
	}

	return Available;
}

//
// add or backtrack allocation [checked]
//
VOID ario_AddOrBacktrackAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State,__in PVOID Routine)
{
	PAGED_CODE();

	PARBITER_ADD_ALLOCATION AddOrBacktrack				= static_cast<PARBITER_ADD_ALLOCATION>(Routine);

	ASSERT(Arbiter);
	ASSERT(State);

	ARBITER_ALLOCATION_STATE SavedState;
	RtlCopyMemory(&SavedState,State,sizeof(ARBITER_ALLOCATION_STATE));

	if(FlagOn(State->WorkSpace,PCI_IO_ARBITER_ISA_WINDOW) == PCI_IO_ARBITER_ISA_WINDOW && State->Start < 0xffff)
	{
		ASSERT(State->End <= 0xFFFF);

		while(SavedState.Start < State->End && SavedState.Start < 0xffff)
		{
			SavedState.End								= SavedState.Start + 0xff;

			AddOrBacktrack(Arbiter,&SavedState);

			SavedState.Start							+= 0x400;
		}
	}
	else
	{
		AddOrBacktrack(Arbiter,State);

		if(!FlagOn(State->CurrentAlternative->Descriptor->Flags,CM_RESOURCE_PORT_POSITIVE_DECODE))
			SetFlag(SavedState.RangeAttributes,ARBITER_RANGE_ALIAS);

		while(ario_GetNextAlias(State->CurrentAlternative->Descriptor->Flags,SavedState.Start,&SavedState.Start))
		{
			SavedState.End								= SavedState.Start + State->CurrentAlternative->Descriptor->u.Port.Length - 1;

			AddOrBacktrack(Arbiter,&SavedState);
		}
	}
}