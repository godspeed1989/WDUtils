//********************************************************************
//	created:	7:8:2008   13:08
//	file:		pci.arbiter.cpp
//	author:		tiamo
//	purpose:	arbiter
//********************************************************************

#include "stdafx.h"

#if DBG
	#pragma alloc_text("PAGE",ArbpDumpArbiterInstance)
	#pragma alloc_text("PAGE",ArbpDumpArbiterRange)
	#pragma alloc_text("PAGE",ArbpDumpArbitrationList)
#endif

#pragma alloc_text("PAGE",ArbInitializeArbiterInstance)
#pragma alloc_text("PAGE",ArbDeleteArbiterInstance)
#pragma alloc_text("PAGE",ArbArbiterHandler)
#pragma alloc_text("PAGE",ArbStartArbiter)
#pragma alloc_text("PAGE",ArbTestAllocation)
#pragma alloc_text("PAGE",ArbRetestAllocation)
#pragma alloc_text("PAGE",ArbCommitAllocation)
#pragma alloc_text("PAGE",ArbRollbackAllocation)
#pragma alloc_text("PAGE",ArbAddReserved)
#pragma alloc_text("PAGE",ArbPreprocessEntry)
#pragma alloc_text("PAGE",ArbAllocateEntry)
#pragma alloc_text("PAGE",ArbSortArbitrationList)
#pragma alloc_text("PAGE",ArbBacktrackAllocation)
#pragma alloc_text("PAGE",ArbGetNextAllocationRange)
#pragma alloc_text("PAGE",ArbFindSuitableRange)
#pragma alloc_text("PAGE",ArbAddAllocation)
#pragma alloc_text("PAGE",ArbQueryConflict)
#pragma alloc_text("PAGE",ArbInitializeOrderingList)
#pragma alloc_text("PAGE",ArbCopyOrderingList)
#pragma alloc_text("PAGE",ArbPruneOrdering)
#pragma alloc_text("PAGE",ArbAddOrdering)
#pragma alloc_text("PAGE",ArbFreeOrderingList)
#pragma alloc_text("PAGE",ArbBuildAssignmentOrdering)
#pragma alloc_text("PAGE",ArbpGetRegistryValue)
#pragma alloc_text("PAGE",ArbpBuildAllocationStack)
#pragma alloc_text("PAGE",ArbpBuildAlternative)
#pragma alloc_text("PAGE",ArbpQueryConflictCallback)
#pragma	alloc_text("PAGE",ArbShareDriverExclusive)

//
// initialize arbiter instance [checked]
//
NTSTATUS ArbInitializeArbiterInstance(__out PARBITER_INSTANCE Arbiter,__in PDEVICE_OBJECT BusDeviceObject,__in CM_RESOURCE_TYPE ResourceType,__in PWSTR Name,
									  __in PWSTR OrderingName,__in_opt PARBITER_TRANSLATE_ALLOCATION_ORDER TranslateOrdering)
{
	PAGED_CODE();

	ASSERT(Arbiter->UnpackRequirement);
	ASSERT(Arbiter->PackResource);
	ASSERT(Arbiter->UnpackResource);

	ARB_PRINT(2,("Initializing %S Arbiter...\n",Name));

	//
	// initialize all pool allocation pointers to NULL so we can cleanup
	//
	ASSERT(!Arbiter->MutexEvent && !Arbiter->Allocation && !Arbiter->PossibleAllocation && !Arbiter->AllocationStack);

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		//
		// we are an arbiter
		//
		Arbiter->Signature								= 'sbrA';

		//
		// remember the bus that produced us
		//
		Arbiter->BusDeviceObject						= BusDeviceObject;

		//
		// initialize state lock (KEVENT must be non-paged)
		//
		Arbiter->MutexEvent								= static_cast<PKEVENT>(ExAllocatePoolWithTag(NonPagedPool,sizeof(KEVENT),'MbrA'));
		if(!Arbiter->MutexEvent)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		KeInitializeEvent(Arbiter->MutexEvent,SynchronizationEvent,TRUE);

		//
		// initialize the allocation stack to a reasonable size
		//
		Arbiter->AllocationStack						= static_cast<PARBITER_ALLOCATION_STATE>(ExAllocatePoolWithTag(PagedPool,PAGE_SIZE,'AbrA'));
		if(!Arbiter->AllocationStack)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		Arbiter->AllocationStackMaxSize					= PAGE_SIZE;

		//
		// allocate buffers to hold the range lists
		//
		Arbiter->Allocation								= static_cast<PRTL_RANGE_LIST>(ExAllocatePoolWithTag(PagedPool,sizeof(RTL_RANGE_LIST),'RbrA'));
		if(!Arbiter->Allocation)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		Arbiter->PossibleAllocation						= static_cast<PRTL_RANGE_LIST>(ExAllocatePoolWithTag(PagedPool,sizeof(RTL_RANGE_LIST),'RbrA'));
		if(!Arbiter->PossibleAllocation)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// initialize the range lists
		//
		RtlInitializeRangeList(Arbiter->Allocation);
		RtlInitializeRangeList(Arbiter->PossibleAllocation);

		//
		// Initialize the data fields
		//
		Arbiter->TransactionInProgress					= FALSE;
		Arbiter->Name									= Name;
		Arbiter->ResourceType							= ResourceType;

		//
		// if the caller has not supplied the optional functions set them to the defaults
		//
		if(!Arbiter->TestAllocation)
			Arbiter->TestAllocation						= &ArbTestAllocation;

		if(!Arbiter->RetestAllocation)
			Arbiter->RetestAllocation					= &ArbRetestAllocation;

		if(!Arbiter->CommitAllocation)
			Arbiter->CommitAllocation					= &ArbCommitAllocation;

		if(!Arbiter->RollbackAllocation)
			Arbiter->RollbackAllocation					= &ArbRollbackAllocation;

		if(!Arbiter->AddReserved)
			Arbiter->AddReserved						= &ArbAddReserved;

		if(!Arbiter->PreprocessEntry)
			Arbiter->PreprocessEntry					= &ArbPreprocessEntry;

		if(!Arbiter->AllocateEntry)
			Arbiter->AllocateEntry						= &ArbAllocateEntry;

		if(!Arbiter->GetNextAllocationRange)
			Arbiter->GetNextAllocationRange				= &ArbGetNextAllocationRange;

		if(!Arbiter->FindSuitableRange)
			Arbiter->FindSuitableRange					= &ArbFindSuitableRange;

		if(!Arbiter->AddAllocation)
			Arbiter->AddAllocation						= &ArbAddAllocation;

		if(!Arbiter->BacktrackAllocation)
			Arbiter->BacktrackAllocation				= &ArbBacktrackAllocation;

		if(!Arbiter->OverrideConflict)
			Arbiter->OverrideConflict					= &ArbOverrideConflict;

		if(!Arbiter->BootAllocation)
			Arbiter->BootAllocation						= &ArbBootAllocation;

		if(!Arbiter->QueryConflict)
			Arbiter->QueryConflict						= &ArbQueryConflict;

		if(!Arbiter->StartArbiter)
			Arbiter->StartArbiter						= &ArbStartArbiter;

		//
		// build the prefered assignment ordering - we assume that the reserved ranges have the same name as the assignment ordering
		//
		Status											= ArbBuildAssignmentOrdering(Arbiter,OrderingName,OrderingName,TranslateOrdering);
	}
	__finally
	{
		if(!NT_SUCCESS(Status))
		{
			if(Arbiter->MutexEvent)
				ExFreePool(Arbiter->MutexEvent);

			if(Arbiter->Allocation)
				ExFreePool(Arbiter->Allocation);

			if(Arbiter->PossibleAllocation)
				ExFreePool(Arbiter->PossibleAllocation);

			if(Arbiter->AllocationStack)
				ExFreePool(Arbiter->AllocationStack);
		}
	}

	return Status;
}

//
// reference [checked]
//
VOID ArbReferenceArbiterInstance(__in PARBITER_INSTANCE Arbiter)
{
	InterlockedIncrement(&Arbiter->ReferenceCount);
}

//
// dereference [checked]
//
VOID ArbDereferenceArbiterInstance(__in PARBITER_INSTANCE Arbiter)
{
	InterlockedDecrement(&Arbiter->ReferenceCount);

	if(!Arbiter->ReferenceCount)
		ArbDeleteArbiterInstance(Arbiter);
}

//
// delete arbiter instance [checked]
//
VOID ArbDeleteArbiterInstance(__in PARBITER_INSTANCE Arbiter)
{
	PAGED_CODE();

	if(Arbiter->MutexEvent)
		ExFreePool(Arbiter->MutexEvent);

	if(Arbiter->Allocation)
	{
		RtlFreeRangeList(Arbiter->Allocation);
		ExFreePool(Arbiter->Allocation);
	}

	if(Arbiter->PossibleAllocation)
	{
		RtlFreeRangeList(Arbiter->PossibleAllocation);
		ExFreePool(Arbiter->PossibleAllocation);
	}

	if(Arbiter->AllocationStack)
		ExFreePool(Arbiter->AllocationStack);

	ArbFreeOrderingList(&Arbiter->OrderingList);
	ArbFreeOrderingList(&Arbiter->ReservedList);

	RtlFillMemory(Arbiter,sizeof(ARBITER_INSTANCE),'A');
}

//
// test allocation [checked]
//
NTSTATUS ArbTestAllocation(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList)
{
	PAGED_CODE();

	ASSERT(Arbiter);

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		//
		// copy the current allocation
		//
		Status											= RtlCopyRangeList(Arbiter->PossibleAllocation,Arbiter->Allocation);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// free all the resources currently allocated to all the devices we are arbitrating for
		//
		PARBITER_LIST_ENTRY Current						= 0;
		ULONG Count										= 0;
		PDEVICE_OBJECT PreviousOwner					= 0;
		FOR_ALL_IN_LIST(ARBITER_LIST_ENTRY,ArbitrationList,Current)
		{
			Count										+= 1;
			PDEVICE_OBJECT CurrentOwner					= Current->PhysicalDeviceObject;

			if(PreviousOwner != CurrentOwner)
			{
				PreviousOwner							= CurrentOwner;

				ARB_PRINT(3,("Delete 0x%08x's resources\n",CurrentOwner));

				Status									= RtlDeleteOwnersRanges(Arbiter->PossibleAllocation,CurrentOwner);
				if(!NT_SUCCESS(Status))
					try_leave(NOTHING);
			}

			Current->WorkSpace							= 0;

			//
			// score the entries in the arbitration list if a scoring function was provided and this is not a legacy request
			// (which is guaranteed to be made of all fixed requests so sorting is pointless)
			//
			if(Arbiter->ScoreRequirement)
			{
				PIO_RESOURCE_DESCRIPTOR Alternative		= 0;
				FOR_ALL_IN_ARRAY(Current->Alternatives,Current->AlternativeCount,Alternative)
				{
					ARB_PRINT(3,("Scoring entry %p\n",CurrentOwner));

					LONG Score							= Arbiter->ScoreRequirement(Alternative);

					//
					// ensure the score is valid
					//
					if(Score < 0)
						try_leave(Status = STATUS_DEVICE_CONFIGURATION_ERROR);

					Current->WorkSpace					+= Score;
				}
			}
		}

		//
		// sort arbitration list according to the workspace value
		//
		Status											= ArbSortArbitrationList(ArbitrationList);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// build the arbitration stack
		//
		Status											= ArbpBuildAllocationStack(Arbiter,ArbitrationList,Count);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// try to allocate
		//
		Status											= Arbiter->AllocateEntry(Arbiter,Arbiter->AllocationStack);
	}
	__finally
	{
		//
		// We didn't succeed so empty the possible allocation list...
		//
		if(!NT_SUCCESS(Status) || AbnormalTermination())
			RtlFreeRangeList(Arbiter->PossibleAllocation);
	}

	return Status;
}

//
// build alternative from io resource descriptor [checked]
//
NTSTATUS ArbpBuildAlternative(__in PARBITER_INSTANCE Arbiter,__in PIO_RESOURCE_DESCRIPTOR Requirement,__out PARBITER_ALTERNATIVE Alternative)
{
	PAGED_CODE();

	ASSERT(Alternative && Requirement);

	Alternative->Descriptor								= Requirement;

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		//
		// unpack the requirement into the alternatives table
		//
		Status											= Arbiter->UnpackRequirement(Requirement,&Alternative->Minimum,&Alternative->Maximum,
																					 &Alternative->Length,&Alternative->Alignment);

		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// align the minimum if necessary
		//
		if(Alternative->Minimum % Alternative->Alignment)
			ALIGN_ADDRESS_UP(Alternative->Minimum,Alternative->Alignment);

		Alternative->Flags								= 0;

		//
		// check if this alternative is shared
		//
		if(Requirement->ShareDisposition == CmResourceShareShared)
			SetFlag(Alternative->Flags,ARBITER_ALTERNATIVE_FLAG_SHARED);

		//
		// check if this alternative is fixed
		//
		if(Alternative->Maximum - Alternative->Minimum + 1 == Alternative->Length)
			SetFlag(Alternative->Flags,ARBITER_ALTERNATIVE_FLAG_FIXED);

		//
		// check for validity
		//
		if(Alternative->Maximum < Alternative->Minimum)
			SetFlag(Alternative->Flags,ARBITER_ALTERNATIVE_FLAG_INVALID);
	}
	__finally
	{

	}

	return Status;
}

//
// build stack [checked]
//
NTSTATUS ArbpBuildAllocationStack(__in PARBITER_INSTANCE Arbiter,__in PLIST_ENTRY ArbitrationList,__in ULONG ArbitrationListCount)
{
	PAGED_CODE();

	//
	// calculate the size the stack needs to be and the
	//
	PARBITER_LIST_ENTRY CurrentEntry					= 0;
	ULONG StackSize										= 0;
	ULONG AllocationCount								= ArbitrationListCount + 1;
	FOR_ALL_IN_LIST(ARBITER_LIST_ENTRY,ArbitrationList,CurrentEntry)
	{
		if(CurrentEntry->AlternativeCount)
			StackSize									+= CurrentEntry->AlternativeCount * sizeof(ARBITER_ALTERNATIVE);
		else
			AllocationCount								-= 1;
	}

	StackSize											+= AllocationCount * sizeof(ARBITER_ALLOCATION_STATE);

	//
	// make sure the allocation stack is large enough
	//
	if(Arbiter->AllocationStackMaxSize < StackSize)
	{
		//
		// enlarge the allocation stack
		//
		PARBITER_ALLOCATION_STATE Temp					= static_cast<PARBITER_ALLOCATION_STATE>(ExAllocatePoolWithTag(PagedPool,StackSize,'AbrA'));
		if(!Temp)
			return STATUS_INSUFFICIENT_RESOURCES;

		ExFreePool(Arbiter->AllocationStack);
		Arbiter->AllocationStack						= Temp;
		Arbiter->AllocationStackMaxSize					= StackSize;
	}

	RtlZeroMemory(Arbiter->AllocationStack,StackSize);

	//
	// fill in the locations
	//
	PARBITER_ALLOCATION_STATE CurrentState				= Arbiter->AllocationStack;
	PARBITER_ALTERNATIVE CurrentAlternative				= reinterpret_cast<PARBITER_ALTERNATIVE>(Arbiter->AllocationStack + AllocationCount);

	FOR_ALL_IN_LIST(ARBITER_LIST_ENTRY,ArbitrationList,CurrentEntry)
	{
		//
		// do we need to allocate anything for this entry?
		//
		if(CurrentEntry->AlternativeCount)
		{
			//
			// initialize the stack location
			//
			CurrentState->Entry							= CurrentEntry;
			CurrentState->AlternativeCount				= CurrentEntry->AlternativeCount;
			CurrentState->Alternatives					= CurrentAlternative;

			//
			// initialize the start and end values to an invalid range so that we don't skip the range 0-0 every time...
			//
			CurrentState->Start							= 1;
			ASSERT(CurrentState->End == 0);

			//
			// initialize the alternatives table
			//
			PIO_RESOURCE_DESCRIPTOR CurrentDescriptor	= 0;
			FOR_ALL_IN_ARRAY(CurrentEntry->Alternatives,CurrentEntry->AlternativeCount,CurrentDescriptor)
			{
				NTSTATUS Status							= ArbpBuildAlternative(Arbiter,CurrentDescriptor,CurrentAlternative);
				if(!NT_SUCCESS(Status))
					return Status;

				//
				// initialize the priority
				//
				CurrentAlternative->Priority			= ARBITER_PRIORITY_NULL;

				//
				// advance to the next alternative
				//
				CurrentAlternative						+= 1;

			}

			CurrentState								+= 1;
		}
	}

	//
	// terminate the stack with NULL entry
	//
	CurrentState->Entry									= 0;

	return STATUS_SUCCESS;
}

//
// sorts the arbitration list in order of each entry's WorkSpace value. [checked]
//
NTSTATUS ArbSortArbitrationList(__inout PLIST_ENTRY ArbitrationList)
{
	PAGED_CODE();

	ARB_PRINT(3, ("IoSortArbiterList(%p)\n", ArbitrationList));

	BOOLEAN Sorted										= FALSE;
	while(!Sorted)
	{
		Sorted											= TRUE;
		PARBITER_LIST_ENTRY Current						= CONTAINING_RECORD(ArbitrationList->Flink,ARBITER_LIST_ENTRY,ListEntry);
		PARBITER_LIST_ENTRY Next						= CONTAINING_RECORD(Current->ListEntry.Flink,ARBITER_LIST_ENTRY,ListEntry);
		while(ArbitrationList != &Current->ListEntry && ArbitrationList != &Next->ListEntry)
		{
			if(Current->WorkSpace > Next->WorkSpace)
			{
				PLIST_ENTRY Before						= Current->ListEntry.Blink;
				PLIST_ENTRY After						= Next->ListEntry.Flink;

				//
				// swap the locations of current and next
				//
				Before->Flink							= &Next->ListEntry;
				After->Blink							= &Current->ListEntry;
				Current->ListEntry.Flink				= After;
				Current->ListEntry.Blink				= &Next->ListEntry;
				Next->ListEntry.Flink					= &Current->ListEntry;;
				Next->ListEntry.Blink					= Before;

				Sorted									= FALSE;
			}

			Current										= CONTAINING_RECORD(Current->ListEntry.Flink,ARBITER_LIST_ENTRY,ListEntry);
			Next										= CONTAINING_RECORD(Current->ListEntry.Flink,ARBITER_LIST_ENTRY,ListEntry);
		}
	}

	return STATUS_SUCCESS;
}

//
// commit allocation [checked]
//
NTSTATUS ArbCommitAllocation(__in PARBITER_INSTANCE Arbiter)
{
	PAGED_CODE();

	//
	// free up the current allocation
	//
	RtlFreeRangeList(Arbiter->Allocation);

	//
	// swap the allocated and duplicate lists
	//
	PRTL_RANGE_LIST Temp								= Arbiter->Allocation;
	Arbiter->Allocation									= Arbiter->PossibleAllocation;
	Arbiter->PossibleAllocation							= Temp;

	return STATUS_SUCCESS;
}

//
// rollback [checked]
//
NTSTATUS ArbRollbackAllocation(__in PARBITER_INSTANCE Arbiter)
{
	PAGED_CODE();

	//
	// free up the possible allocation
	//
	RtlFreeRangeList(Arbiter->PossibleAllocation);

	return STATUS_SUCCESS;
}

//
// retest [checked]
//
NTSTATUS ArbRetestAllocation(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList)
{
	PAGED_CODE();

	//
	// initialize the state
	//
	ARBITER_ALLOCATION_STATE State;
	RtlZeroMemory(&State,sizeof(ARBITER_ALLOCATION_STATE));

	ARBITER_ALTERNATIVE Alternative;
	RtlZeroMemory(&Alternative,sizeof(ARBITER_ALTERNATIVE));

	State.AlternativeCount								= 1;
	State.Alternatives									= &Alternative;
	State.CurrentAlternative							= &Alternative;
	State.Flags											= ARBITER_STATE_FLAG_RETEST;

	NTSTATUS Status										= STATUS_SUCCESS;
	__try
	{
		//
		// copy the current allocation and reserved
		//
		ARB_PRINT(2,("Retest: Copy current allocation\n"));
		Status											= RtlCopyRangeList(Arbiter->PossibleAllocation,Arbiter->Allocation);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// free all the resources currently allocated to all the devices we are arbitrating for
		//
		PARBITER_LIST_ENTRY Current						= 0;
		FOR_ALL_IN_LIST(ARBITER_LIST_ENTRY,ArbitrationList,Current)
		{
			ARB_PRINT(3,("Retest: Delete 0x%08x's resources\n",Current->PhysicalDeviceObject));

			Status										= RtlDeleteOwnersRanges(Arbiter->PossibleAllocation,Current->PhysicalDeviceObject);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);
		}

		//
		// build an allocation state for the allocation and call AddAllocation to update the range lists accordingly
		//
		FOR_ALL_IN_LIST(ARBITER_LIST_ENTRY,ArbitrationList,Current)
		{
			ASSERT(Current->Assignment && Current->SelectedAlternative);

			State.WorkSpace									= 0;
			State.Entry										= Current;

			//
			// initialize the alternative
			//
			Status											= ArbpBuildAlternative(Arbiter,Current->SelectedAlternative,&Alternative);
			ASSERT(NT_SUCCESS(Status));

			//
			// update it with our allocation
			//
			ULONG Length									= 0;
			Status											= Arbiter->UnpackResource(Current->Assignment,&State.Start,&Length);
			ASSERT(NT_SUCCESS(Status));

			State.End										= State.Start + Length - 1;

			//
			// do any preprocessing that is required
			//
			Status											= Arbiter->PreprocessEntry(Arbiter,&State);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			//
			// if we had a requirement for length 0 then don't attemp to add the range - it will fail!
			//
			if(Length)
				Arbiter->AddAllocation(Arbiter,&State);
		}
	}
	__finally
	{
		if(!NT_SUCCESS(Status) || AbnormalTermination())
			RtlFreeRangeList(Arbiter->PossibleAllocation);
	}

	return Status;
}

//
// boot allocation [checked]
//
NTSTATUS ArbBootAllocation(__in PARBITER_INSTANCE Arbiter,__inout PLIST_ENTRY ArbitrationList)
{
	//
	// initialize the state
	//
	ARBITER_ALLOCATION_STATE State;
	RtlZeroMemory(&State,sizeof(ARBITER_ALLOCATION_STATE));

	ARBITER_ALTERNATIVE Alternative;
	RtlZeroMemory(&Alternative,sizeof(ARBITER_ALTERNATIVE));

	State.AlternativeCount								= 1;
	State.Alternatives									= &Alternative;
	State.CurrentAlternative							= &Alternative;
	State.Flags											= ARBITER_STATE_FLAG_BOOT;
	State.RangeAttributes								= ARBITER_RANGE_BOOT_ALLOCATED;

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		//
		// work on the possible allocation list
		//
		Status											= RtlCopyRangeList(Arbiter->PossibleAllocation,Arbiter->Allocation);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		PARBITER_LIST_ENTRY Current;
		FOR_ALL_IN_LIST(ARBITER_LIST_ENTRY,ArbitrationList,Current)
		{
			ASSERT(Current->AlternativeCount == 1);
			ASSERT(Current->PhysicalDeviceObject);

			//
			// build an alternative and state structure for this allocation and add it to the range list
			//
			State.Entry									= Current;

			//
			// initialize the alternative
			//
			Status										= ArbpBuildAlternative(Arbiter,&Current->Alternatives[0],&Alternative);

			ASSERT(NT_SUCCESS(Status));
			ASSERT(FlagOn(Alternative.Flags,ARBITER_ALTERNATIVE_FLAG_FIXED | ARBITER_ALTERNATIVE_FLAG_INVALID));

			State.Start									= Alternative.Minimum;
			State.End									= Alternative.Maximum;

			//
			// blow away the old workspace and masks
			//
			State.WorkSpace								= 0;
			State.RangeAvailableAttributes				= 0;

			//
			// validate the requirement
			//
			if( !Alternative.Length || !Alternative.Alignment || State.End < State.Start ||
				State.Start % Alternative.Alignment || State.End - State.Start + 1 != Alternative.Length)
			{
				ARB_PRINT(1,("Skipping invalid boot allocation 0x%I64x-0x%I64x L 0x%x A 0x%x for 0x%08x\n",
							 State.Start,State.End,Alternative.Length,Alternative.Alignment,Current->PhysicalDeviceObject));

				continue;
			}

	#if 0
			if(FlagOn(Alternative.Flags,ARBITER_ALTERNATIVE_FLAG_SHARED))
			{
				ARB_PRINT(1,("Skipping shared boot allocation 0x%I64x-0x%I64x L 0x%x A 0x%x for 0x%08x\n",
							 State.Start,State.End,Alternative.Length,Alternative.Alignment,Current->PhysicalDeviceObject));

				continue;
			}
	#endif

			//
			// Do any preprocessing that is required
			//
			Status										= Arbiter->PreprocessEntry(Arbiter,&State);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			Arbiter->AddAllocation(Arbiter,&State);
		}

		//
		// everything went OK so make this our allocated range
		//
		RtlFreeRangeList(Arbiter->Allocation);
		PRTL_RANGE_LIST Temp							= Arbiter->Allocation;
		Arbiter->Allocation								= Arbiter->PossibleAllocation;
		Arbiter->PossibleAllocation						= Temp;
	}
	__finally
	{
		if(!NT_SUCCESS(Status) || AbnormalTermination())
			RtlFreeRangeList(Arbiter->PossibleAllocation);
	}

	return Status;
}

//
// handler [checked]
//
NTSTATUS ArbArbiterHandler(__in PVOID Context,__in ARBITER_ACTION Action,__inout PARBITER_PARAMETERS Params)
{
	PAGED_CODE();

	ASSERT(Context);
	ASSERT(Action >= 0 && Action <= ArbiterActionBootAllocation);
	PARBITER_INSTANCE Arbiter							= static_cast<PARBITER_INSTANCE>(Context);
	ASSERT(Arbiter->Signature == 'sbrA');

	//
	// acquire the state lock
	//
	ArbAcquireArbiterLock(Arbiter);

	//
	// announce ourselves
	//
	ARB_PRINT(2,("%s %S\n",ArbpActionStrings[Action],Arbiter->Name));

	//
	// check the transaction flag
	//
	if(Action == ArbiterActionTestAllocation || Action == ArbiterActionRetestAllocation || Action == ArbiterActionBootAllocation)
		ASSERT(!Arbiter->TransactionInProgress);
	else if(Action == ArbiterActionCommitAllocation || Action == ArbiterActionRollbackAllocation)
		ASSERT(Arbiter->TransactionInProgress);

	NTSTATUS Status										= STATUS_SUCCESS;

	#if DBG
	do
	{
	#endif
		//
		// do the appropriate thing
		//
		switch(Action)
		{
		case ArbiterActionTestAllocation:
			//
			// suballocation can not occur for a root arbiter
			//
			ASSERT(!Params->Parameters.TestAllocation.AllocateFromCount);
			ASSERT(!Params->Parameters.TestAllocation.AllocateFrom);
			Status										= Arbiter->TestAllocation(Arbiter,Params->Parameters.TestAllocation.ArbitrationList);
			break;

		case ArbiterActionRetestAllocation:
			//
			// suballocation can not occur for a root arbiter
			//
			ASSERT(!Params->Parameters.TestAllocation.AllocateFromCount);
			ASSERT(!Params->Parameters.TestAllocation.AllocateFrom);
			Status										= Arbiter->RetestAllocation(Arbiter,Params->Parameters.TestAllocation.ArbitrationList);
			break;

		case ArbiterActionCommitAllocation:
			Status										= Arbiter->CommitAllocation(Arbiter);
			break;

		case ArbiterActionRollbackAllocation:
			Status										= Arbiter->RollbackAllocation(Arbiter);
			break;

		case ArbiterActionBootAllocation:
			Status										= Arbiter->BootAllocation(Arbiter,Params->Parameters.BootAllocation.ArbitrationList);
			break;

		case ArbiterActionQueryConflict:
			Status										= Arbiter->QueryConflict(Arbiter,Params->Parameters.QueryConflict.PhysicalDeviceObject,
																				 Params->Parameters.QueryConflict.ConflictingResource,
																				 Params->Parameters.QueryConflict.ConflictCount,
																				 Params->Parameters.QueryConflict.Conflicts);
			break;

		case ArbiterActionQueryArbitrate:
		case ArbiterActionQueryAllocatedResources:
		case ArbiterActionWriteReservedResources:
		case ArbiterActionAddReserved:
			Status										= STATUS_NOT_IMPLEMENTED;
			break;

		default:
			Status										= STATUS_INVALID_PARAMETER;
			break;
		}

	#if DBG
		//
		// check if we failed and want to stop or replay on errors
		//
		if(!NT_SUCCESS(Status))
		{
			ARB_PRINT(1,("*** %s for %S FAILED status = %08x\n",ArbpActionStrings[Action],Arbiter->Name,Status));

			if(ArbStopOnError)
				DbgBreakPoint();

			if(ArbReplayOnError)
				continue;
		}

		break;
	}while(1);
	#endif

	if(NT_SUCCESS(Status))
	{
		if(Action == ArbiterActionTestAllocation || Action == ArbiterActionRetestAllocation)
			Arbiter->TransactionInProgress = TRUE;
		else if(Action == ArbiterActionCommitAllocation || Action == ArbiterActionRollbackAllocation)
			Arbiter->TransactionInProgress = FALSE;
	}

	ArbReleaseArbiterLock(Arbiter);

	return Status;
}

//
// build ordering [checked]
//
NTSTATUS ArbBuildAssignmentOrdering(__inout PARBITER_INSTANCE Arbiter,__in PWSTR AllocationOrderName,__in PWSTR ReservedResourcesName,
									__in_opt PARBITER_TRANSLATE_ALLOCATION_ORDER Translate)
{
	NTSTATUS Status										= STATUS_SUCCESS;
	HANDLE ArbitersHandle								= 0;
	HANDLE TempHandle									= 0;
	PKEY_VALUE_FULL_INFORMATION Info					= 0;
	UNICODE_STRING UnicodeString;

	PAGED_CODE();

	ArbAcquireArbiterLock(Arbiter);

	//
	// if we are reinitializing the orderings free the old ones
	//
	ArbFreeOrderingList(&Arbiter->OrderingList);
	ArbFreeOrderingList(&Arbiter->ReservedList);

	__try
	{
		//
		// initialize the orderings
		//
		Status											= ArbInitializeOrderingList(&Arbiter->OrderingList);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		Status											= ArbInitializeOrderingList(&Arbiter->ReservedList);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// open HKLM\System\CurrentControlSet\Control\Arbiters
		//
		OBJECT_ATTRIBUTES Attributes;
		WstrToUnicodeString(&UnicodeString,L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Arbiters");
		InitializeObjectAttributes(&Attributes,&UnicodeString,OBJ_CASE_INSENSITIVE,0,0);

		Status											= ZwOpenKey(&ArbitersHandle,KEY_READ,&Attributes);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// open AllocationOrder
		//
		WstrToUnicodeString(&UnicodeString,L"AllocationOrder");
		InitializeObjectAttributes(&Attributes,&UnicodeString,OBJ_CASE_INSENSITIVE,ArbitersHandle,0);

		Status											= ZwOpenKey(&TempHandle,KEY_READ,&Attributes);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// extract the value the user asked for
		//
		Status											= ArbpGetRegistryValue(TempHandle,AllocationOrderName,&Info);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// check if the value we retrieved was a string and if so then it was a short cut to a value of that name - open it.
		//
		if(Info->Type == REG_SZ)
		{
			//
			// BUGBUG - check this is a valid string...
			//
			PKEY_VALUE_FULL_INFORMATION TempInfo		= 0;
			Status										= ArbpGetRegistryValue(TempHandle,Add2Ptr(Info,Info->DataOffset,PWCHAR),&TempInfo);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			ExFreePool(Info);
			Info										= TempInfo;
		}

		ZwClose(TempHandle);
		TempHandle										= 0;

		//
		// we only support one level of short cuts so this should be a REG_RESOURCE_REQUIREMENTS_LIST
		//
		if(Info->Type != REG_RESOURCE_REQUIREMENTS_LIST)
			try_leave(Status = STATUS_INVALID_PARAMETER);

		//
		// extract the resource list
		//
		ASSERT(Add2Ptr(Info,Info->DataOffset,PIO_RESOURCE_REQUIREMENTS_LIST)->AlternativeLists == 1);

		PIO_RESOURCE_LIST ResourceList					= Add2Ptr(Info,Info->DataOffset,PIO_RESOURCE_REQUIREMENTS_LIST)->List;

		//
		// convert the resource list into an ordering list
		//
		PIO_RESOURCE_DESCRIPTOR Current					= 0;
		FOR_ALL_IN_ARRAY(ResourceList->Descriptors,ResourceList->Count,Current)
		{
			//
			// Perform any translation that is necessary on the resources
			//
			IO_RESOURCE_DESCRIPTOR Translated;
			if(Translate)
			{
				Status									= Translate(&Translated,Current);
				if(!NT_SUCCESS(Status))
					try_leave(NOTHING);
			}
			else
			{
				Translated								= *Current;
			}

			if(Translated.Type == Arbiter->ResourceType)
			{
				ULONG Dummy								= 0;
				ULONGLONG Start							= 0;
				ULONGLONG End							= 0;
				Status									= Arbiter->UnpackRequirement(&Translated,&Start,&End,&Dummy,&Dummy);
				if(!NT_SUCCESS(Status))
					try_leave(NOTHING);

				Status									= ArbAddOrdering(&Arbiter->OrderingList,Start,End);
				if(!NT_SUCCESS(Status))
					try_leave(NOTHING);
			}
		}

		//
		// We're finished with info...
		//
		ExFreePool(Info);
		Info											= 0;

		//
		// open ReservedResources
		//
		WstrToUnicodeString(&UnicodeString,L"ReservedResources");
		InitializeObjectAttributes(&Attributes,&UnicodeString,OBJ_CASE_INSENSITIVE,ArbitersHandle,0);

		Status											= ZwCreateKey(&TempHandle,KEY_READ,&Attributes,0,0,REG_OPTION_NON_VOLATILE,0);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// extract the arbiter's reserved resources
		//
		Status											= ArbpGetRegistryValue(TempHandle,ReservedResourcesName,&Info);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// check if the value we retrieved was a string and if so then it was a short cut to a value of that name - open it.
		//
		if(Info->Type == REG_SZ)
		{
			PKEY_VALUE_FULL_INFORMATION TempInfo;
			Status										= ArbpGetRegistryValue(TempHandle,Add2Ptr(Info,Info->DataOffset,PWCHAR),&TempInfo);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			ExFreePool(Info);
			Info										= TempInfo;
		}

		ZwClose(TempHandle);
		TempHandle										= 0;

		if(NT_SUCCESS(Status))
		{
			ASSERT(Add2Ptr(Info,Info->DataOffset,PIO_RESOURCE_REQUIREMENTS_LIST)->AlternativeLists == 1);

			PIO_RESOURCE_LIST ResourceList				= Add2Ptr(Info,Info->DataOffset,PIO_RESOURCE_REQUIREMENTS_LIST)->List;

			//
			// apply the reserved ranges to the ordering
			//
			FOR_ALL_IN_ARRAY(ResourceList->Descriptors,ResourceList->Count,Current)
			{
				//
				// perform any translation that is necessary on the resources
				//
				IO_RESOURCE_DESCRIPTOR Translated;
				if(Translate)
				{
					Status								= Translate(&Translated,Current);
					if(!NT_SUCCESS(Status))
						try_leave(NOTHING);
				}
				else
				{
					Translated							= *Current;
				}

				if(Translated.Type == Arbiter->ResourceType)
				{
					ULONG Dummy							= 0;
					ULONGLONG Start						= 0;
					ULONGLONG End						= 0;
					Status								= Arbiter->UnpackRequirement(&Translated,&Start,&End,&Dummy,&Dummy);
					if(!NT_SUCCESS(Status))
						try_leave(NOTHING);

					//
					// add the reserved range to the reserved ordering
					//
					Status								= ArbAddOrdering(&Arbiter->ReservedList,Start,End);
					if(!NT_SUCCESS(Status))
						try_leave(NOTHING);

					//
					// prune the reserved range from the current ordering
					//
					Status								= ArbPruneOrdering(&Arbiter->OrderingList,Start,End);
					if(!NT_SUCCESS(Status))
						try_leave(NOTHING);
				}
			}

			ExFreePool(Info);
			Info											= 0;
		}

		//
		// all done!
		//
		ZwClose(ArbitersHandle);
		ArbitersHandle										= 0;

	#if DBG
		{
			PARBITER_ORDERING Current;

			FOR_ALL_IN_ARRAY(Arbiter->OrderingList.Orderings,Arbiter->OrderingList.Count,Current)
			{
				ARB_PRINT(2,("Ordering: 0x%I64x-0x%I64x\n",Current->Start,Current->End));
			}

			ARB_PRINT(2, ("\n"));

			FOR_ALL_IN_ARRAY(Arbiter->ReservedList.Orderings,Arbiter->ReservedList.Count,Current)
			{
				ARB_PRINT(2,("Reserved: 0x%I64x-0x%I64x\n",Current->Start,Current->End));
			}
		}
	#endif
	}
	__finally
	{
		if(!NT_SUCCESS(Status) || AbnormalTermination())
		{
			ArbFreeOrderingList(&Arbiter->OrderingList);
			ArbFreeOrderingList(&Arbiter->ReservedList);
		}

		if(ArbitersHandle)
			ZwClose(ArbitersHandle);

		if(TempHandle)
			ZwClose(TempHandle);

		if(Info)
			ExFreePool(Info);

		ArbReleaseArbiterLock(Arbiter);
	}

	return Status;
}

//
// find range [checked]
//
BOOLEAN ArbFindSuitableRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	ASSERT(State->CurrentAlternative);

	//
	// catch the case where we backtrack and advance past the maximum
	//
	if(State->CurrentMinimum > State->CurrentMaximum)
		return FALSE;

	//
	// if we are asking for zero ports then trivially succeed with the minimum value and remember that backtracking this is a recipe for infinite loops
	//
	if(!State->CurrentAlternative->Length)
	{
		State->Start									= State->CurrentMinimum;
		State->End										= State->Start;
		return TRUE;
	}

	//
	// for legacy requests from IoAssignResources (directly or by way of HalAssignSlotResources) or IoReportResourceUsage
	// we consider preallocated resources to be available for backward compatibility reasons.
	//
	// if we are allocating a devices boot config then we consider all other boot configs to be available.
	// BUGBUG(andrewth) - this behavior is bad!
	//
	if(State->Entry->RequestSource == ArbiterRequestLegacyReported || State->Entry->RequestSource == ArbiterRequestLegacyAssigned)
		SetFlag(State->RangeAvailableAttributes,ARBITER_RANGE_BOOT_ALLOCATED);

	//
	// check if null conflicts are OK...
	//
	ULONG FindRangeFlags								= 0;
	if(FlagOn(State->Flags,ARBITER_STATE_FLAG_NULL_CONFLICT_OK))
		SetFlag(FindRangeFlags,RTL_RANGE_LIST_NULL_CONFLICT_OK);

	//
	// ...or we are shareable...
	//
	if(FlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_SHARED))
		SetFlag(FindRangeFlags,RTL_RANGE_LIST_SHARED_OK);

	//
	// select the first free alternative from the current alternative
	//
	NTSTATUS Status										= RtlFindRange(Arbiter->PossibleAllocation,State->CurrentMinimum,State->CurrentMaximum,
																	   State->CurrentAlternative->Length,State->CurrentAlternative->Alignment,
																	   FindRangeFlags,State->RangeAvailableAttributes,Arbiter->ConflictCallbackContext,
																	   Arbiter->ConflictCallback,&State->Start);


	if(NT_SUCCESS(Status))
	{
		//
		// we found a suitable range
		//
		State->End										= State->Start + State->CurrentAlternative->Length - 1;
		return TRUE;
	}

	if(ArbShareDriverExclusive(Arbiter,State))
		return TRUE;

	//
	// we couldn't find any range so check if we will allow this conflict - if so don'd fail!
	//
	return Arbiter->OverrideConflict(Arbiter,State);
}

//
// add allocation [checked]
//
VOID ArbAddAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	ULONG Flags											= RTL_RANGE_LIST_ADD_IF_CONFLICT;
	Flags												+= FlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_SHARED) ? RTL_RANGE_LIST_ADD_SHARED : 0;
	NTSTATUS Status										= RtlAddRange(Arbiter->PossibleAllocation,State->Start,State->End,State->RangeAttributes,
																	  Flags,0,State->Entry->PhysicalDeviceObject);

	ASSERT(NT_SUCCESS(Status));
}

//
// backtrack [checked]
//
VOID ArbBacktrackAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	//
	// we couldn't allocate for the rest of the ranges then backtrack
	//
	NTSTATUS Status										= RtlDeleteRange(Arbiter->PossibleAllocation,State->Start,State->End,State->Entry->PhysicalDeviceObject);

	ASSERT(NT_SUCCESS(Status));

	ARB_PRINT(2,("\t\tBacktracking on 0x%I64x-0x%I64x for %p\n",State->Start,State->End,State->Entry->PhysicalDeviceObject));
}

//
// preprocess [checked]
//
NTSTATUS ArbPreprocessEntry(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	return STATUS_SUCCESS;
}

//
// allocate entry [checked]
//
NTSTATUS ArbAllocateEntry(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	PARBITER_ALLOCATION_STATE CurrentState				= State;
	BOOLEAN Backtracking								= FALSE;
	NTSTATUS Status										= STATUS_SUCCESS;

	//
	// have we reached the end of the list? if so then we have a working allocation.
	//
	while(CurrentState >= State && CurrentState->Entry)
	{
		BOOLEAN FailAllocation							= FALSE;
		BOOLEAN SkipGetNextAllocationRange				= FALSE;

		//
		// do any preprocessing that is required
		//
		Status											= Arbiter->PreprocessEntry(Arbiter,CurrentState);
		if(!NT_SUCCESS(Status))
			return Status;

		//
		// if we need to backtrack do so!
		//
		while(Backtracking)
		{
			Backtracking								= FALSE;

			//
			// clear the CurrentAlternative of the *next* alternative - this will cause the priorities to be recalculated next time through
			// so we will attempt to explore the search space again
			//
			// the currentState+1 is guaranteed to be safe because the only way we can get here is from where we currentState-- below.
			//
			(CurrentState + 1)->CurrentAlternative		= 0;

			//
			// we can't backtrack length 0 requests because there is nothing to backtrack so we would get stuck in an inifinite loop...
			//
			if(!CurrentState->CurrentAlternative->Length)
			{
				FailAllocation							= TRUE;
				break;
			}

			//
			// backtrack
			//
			Arbiter->BacktrackAllocation(Arbiter,CurrentState);

			//
			// reduce allocation window to not include the range we backtracked and check that that doesn't underflow the minimum or wrap
			//
			ULONGLONG PossibleCurrentMinimum			= CurrentState->Start - 1;

			//
			// have we run out space in this alternative move on to the next?
			//
			if(PossibleCurrentMinimum <= CurrentState->CurrentMinimum && PossibleCurrentMinimum >= CurrentState->CurrentAlternative->Minimum)
			{
				//
				// get back into arbitrating at the right point
				//
				CurrentState->CurrentMaximum			= PossibleCurrentMinimum;
			
				SkipGetNextAllocationRange				= TRUE;
			}

			break;
		}

		//
		// try to allocate for this entry
		//
		if(!FailAllocation)
		{
			//
			// select the next alternative with the lowest priority
			// and set CurrentState->CurrentMinimum,CurrentState->CurrentMaximum accoring to the selected alternative's resource descriptor
			//
			while(1)
			{
				if(!SkipGetNextAllocationRange)
				{
					FailAllocation						= !Arbiter->GetNextAllocationRange(Arbiter, CurrentState);
					if(FailAllocation)
						break;

					ARB_INDENT(2,static_cast<ULONG>(CurrentState -State));

					ARB_PRINT(2,("Testing 0x%I64x-0x%I64x %s\n",CurrentState->CurrentMinimum,CurrentState->CurrentMaximum,
								 FlagOn(CurrentState->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_SHARED) ? "shared" : "non-shared"));
				}

				SkipGetNextAllocationRange				= FALSE;

				//
				// use CurrentState->CurrentMinimum and CurrentState->CurrentMaximum to find a unused range in the prosible allocation range list
				// and set CurrentState->Start,CurrentState->End to the selected range's Start and End.
				//
				if(Arbiter->FindSuitableRange(Arbiter, CurrentState))
				{
					//
					// we found a possible solution
					//
					ARB_INDENT(2, static_cast<ULONG>(CurrentState - State));

					if(CurrentState->CurrentAlternative->Length)
					{
						ARB_PRINT(2,("Possible solution for %p = 0x%I64x-0x%I64x, %s\n",CurrentState->Entry->PhysicalDeviceObject,CurrentState->Start,CurrentState->End,
									 FlagOn(CurrentState->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_SHARED) ? "shared" : "non-shared"));

						//
						// update the arbiter with the possible allocation
						//
						Arbiter->AddAllocation(Arbiter, CurrentState);
					}
					else
					{
						ARB_PRINT(2,("Zero length solution solution for %p = 0x%I64x-0x%I64x, %s\n",CurrentState->Entry->PhysicalDeviceObject,CurrentState->Start,
									 CurrentState->End,FlagOn(CurrentState->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_SHARED) ? "shared" : "non-shared"));

						//
						// set the result in the arbiter appropriatley so that we don't try and translate this zero requirement - it won't!
						//
						CurrentState->Entry->Result		= ArbiterResultNullRequest;
					}

					//
					// move on to the next entry
					//
					CurrentState						+= 1;
					break;
				}
			}
		}

		//
		// we couldn't allocate for this device
		//
		if(FailAllocation)
		{
			if(CurrentState == State)
			{
				//
				// we are at the top of the allocation stack to we can't backtrack
				//
				return STATUS_UNSUCCESSFUL;
			}
			else
			{
				//
				// Backtrack and try again
				//
				ARB_INDENT(2, static_cast<ULONG>(CurrentState - State));

				ARB_PRINT(2,("Allocation failed for %p - backtracking\n",CurrentState->Entry->PhysicalDeviceObject));

				Backtracking							= TRUE;

				//
				// pop the last state off the stack and try a different path
				//
				CurrentState							-= 1;
			}
		}
	}

	//
	// we have successfully allocated for all ranges so fill in the allocation
	//
	CurrentState										= State;

	while(CurrentState->Entry)
	{
		Status											= Arbiter->PackResource(CurrentState->CurrentAlternative->Descriptor,
																				CurrentState->Start,CurrentState->Entry->Assignment);

		ASSERT(NT_SUCCESS(Status));

		//
		// remember the alternative we chose from so we can retrieve it during retest
		//
		CurrentState->Entry->SelectedAlternative		= CurrentState->CurrentAlternative->Descriptor;

		ARB_PRINT(2,("Assigned - 0x%I64x-0x%I64x\n",CurrentState->Start,CurrentState->End));

		CurrentState									+= 1;
	}

	return STATUS_SUCCESS;
}

//
// get next allocation range [checked]
//
BOOLEAN ArbGetNextAllocationRange(__in PARBITER_INSTANCE Arbiter,__inout PARBITER_ALLOCATION_STATE State)
{
	while(TRUE)
	{
		if(State->CurrentAlternative)
		{
			//
			// update the priority of the alternative we selected last time
			//
			ArbpUpdatePriority(Arbiter,State->CurrentAlternative);
		}
		else
		{
			//
			// this is the first time we are looking at this alternative or a backtrack - either way we need to update all the priorities
			//
			PARBITER_ALTERNATIVE Current				= 0;
			FOR_ALL_IN_ARRAY(State->Alternatives,State->AlternativeCount,Current)
			{
				Current->Priority						= ARBITER_PRIORITY_NULL;
				ArbpUpdatePriority(Arbiter,Current);
			}
		}

		//
		// find the lowest priority of the alternatives
		//
		PARBITER_ALTERNATIVE LowestAlternative			= State->Alternatives;
		PARBITER_ALTERNATIVE Current					= 0;
		FOR_ALL_IN_ARRAY(State->Alternatives + 1,State->AlternativeCount - 1,Current)
		{
			if(Current->Priority < LowestAlternative->Priority)
				LowestAlternative						= Current;
		}

		ARB_INDENT(2,static_cast<ULONG>(State - Arbiter->AllocationStack));

		//
		// check if we have run out of allocation ranges
		//
		if(LowestAlternative->Priority == ARBITER_PRIORITY_EXHAUSTED)
		{
			if(FlagOn(LowestAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_FIXED))
			{
				ARB_PRINT(2,("Fixed alternative exhausted\n"));
			}
			else
			{
				ARB_PRINT(2,("Alternative exhausted\n"));
			}

			return FALSE;
		}
		else
		{
			ARB_PRINT(2,("LowestAlternative: [%i] 0x%I64x-0x%I64x L=0x%08x A=0x%08x\n",LowestAlternative->Priority,
						 LowestAlternative->Minimum,LowestAlternative->Maximum,LowestAlternative->Length,LowestAlternative->Alignment));
		}

		//
		// check if we are now allowing reserved ranges
		//
		ULONGLONG Min									= 0;
		ULONGLONG Max									= 0;
		if(LowestAlternative->Priority == ARBITER_PRIORITY_RESERVED || LowestAlternative->Priority == ARBITER_PRIORITY_PREFERRED_RESERVED)
		{
			//
			// set min and max to be the Minimum and Maximum that the descriptor specified ignoring any reservations or orderings
			// this is our last chance
			//
			Min											= LowestAlternative->Minimum;
			Max											= LowestAlternative->Maximum;

			ARB_INDENT(2,static_cast<ULONG>(State - Arbiter->AllocationStack));

			ARB_PRINT(2,("Allowing reserved ranges\n"));
		}
		else
		{
			ASSERT(ORDERING_INDEX_FROM_PRIORITY(LowestAlternative->Priority) >= 0);
			ASSERT(ORDERING_INDEX_FROM_PRIORITY(LowestAlternative->Priority) < Arbiter->OrderingList.Count);

			//
			// locate the ordering we match
			//
			PARBITER_ORDERING Ordering					= &Arbiter->OrderingList.Orderings[ORDERING_INDEX_FROM_PRIORITY(LowestAlternative->Priority)];

			//
			// make sure they overlap and are big enough - this is just paranoia
			//
			ASSERT(INTERSECT(LowestAlternative->Minimum,LowestAlternative->Maximum,Ordering->Start,Ordering->End));
			ASSERT(INTERSECT_SIZE(LowestAlternative->Minimum,LowestAlternative->Maximum,Ordering->Start,Ordering->End) >= LowestAlternative->Length);

			//
			// calculate the allocation range
			//
			Min											= max(LowestAlternative->Minimum,Ordering->Start);
			Max											= min(LowestAlternative->Maximum,Ordering->End);
		}

		//
		// if this is a length 0 requirement then succeed now and avoid much trauma later
		//
		if(!LowestAlternative->Length)
		{
			Min											= LowestAlternative->Minimum;
			Max											= LowestAlternative->Maximum;
		}
		else
		{
			//
			// trim range to match alignment.
			//
			Min											+= LowestAlternative->Alignment - 1;
			Min											-= Min % LowestAlternative->Alignment;

			if(LowestAlternative->Length - 1 > Max - Min)
			{
				ARB_INDENT(3,static_cast<ULONG>(State - Arbiter->AllocationStack));
				ARB_PRINT(3,("Range cannot be aligned ... Skipping\n"));

				//
				// set CurrentAlternative so we will update the priority of this alternative
				//
				State->CurrentAlternative				= LowestAlternative;

				continue;
			}

			Max											-= LowestAlternative->Length - 1;
			Max											-= Max % LowestAlternative->Alignment;
			Max											+= LowestAlternative->Length - 1;
		}

		//
		// check if we handed back the same range last time, for the same alternative, if so try to find another range
		//
		if(Min == State->CurrentMinimum && Max == State->CurrentMaximum && State->CurrentAlternative == LowestAlternative)
		{
			ARB_INDENT(2,static_cast<ULONG>(State - Arbiter->AllocationStack));

			ARB_PRINT(2,("Skipping identical allocation range\n"));

			continue;
		}

		State->CurrentMinimum							= Min;
		State->CurrentMaximum							= Max;
		State->CurrentAlternative						= LowestAlternative;

		ARB_INDENT(2,static_cast<ULONG>(State - Arbiter->AllocationStack));
		ARB_PRINT(1,("AllocationRange: 0x%I64x-0x%I64x\n",Min,Max));

		return TRUE;
	}

	return FALSE;
}

//
// get registry value [checked]
//
NTSTATUS ArbpGetRegistryValue(__in HANDLE KeyHandle,__in PWSTR  ValueName,__out PKEY_VALUE_FULL_INFORMATION *Information)
{
	PAGED_CODE();

	UNICODE_STRING UnicodeString;
	RtlInitUnicodeString(&UnicodeString,ValueName);

	//
	// figure out how big the data value is so that a buffer of the appropriate size can be allocated.
	//
	ULONG Length;
	NTSTATUS Status										= ZwQueryValueKey(KeyHandle,&UnicodeString,KeyValueFullInformationAlign64,0,0,&Length);
	if(Status != STATUS_BUFFER_OVERFLOW && Status != STATUS_BUFFER_TOO_SMALL)
		return Status;

	//
	// allocate a buffer large enough to contain the entire key data value.
	//
	PKEY_VALUE_FULL_INFORMATION InfoBuffer				= static_cast<PKEY_VALUE_FULL_INFORMATION>(ExAllocatePoolWithTag(PagedPool,Length,'MbrA'));
	if(!InfoBuffer)
		return STATUS_INSUFFICIENT_RESOURCES;

	//
	// query the data for the key value.
	//
	Status												= ZwQueryValueKey(KeyHandle,&UnicodeString,KeyValueFullInformationAlign64,InfoBuffer,Length,&Length);
	if(!NT_SUCCESS(Status))
	{
		ExFreePool(InfoBuffer);
		return Status;
	}

	//
	// everything worked, so simply return the address of the allocated buffer to the caller, who is now responsible for freeing it.
	//
	*Information										= InfoBuffer;

	return STATUS_SUCCESS;
}

//
// init ordering list [checked]
//
NTSTATUS ArbInitializeOrderingList(__inout PARBITER_ORDERING_LIST List)
{
	PAGED_CODE();

	ASSERT(List);

	List->Orderings										= static_cast<PARBITER_ORDERING>(ExAllocatePoolWithTag(PagedPool,16 *sizeof(ARBITER_ORDERING),'LbrA'));
	if(!List->Orderings)
	{
		List->Maximum									= 0;
		List->Count										= 0;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	List->Count											= 0;
	List->Maximum										= 16;

	return STATUS_SUCCESS;
}

//
// copy ordering list [checked]
//
NTSTATUS ArbCopyOrderingList(__out PARBITER_ORDERING_LIST Destination,__in PARBITER_ORDERING_LIST Source)
{
	PAGED_CODE();

	ASSERT(Source->Count <= Source->Maximum);
	ASSERT(Source->Maximum > 0);

	ULONG Length										= Source->Maximum * sizeof(ARBITER_ORDERING);
	Destination->Orderings								= static_cast<PARBITER_ORDERING>(ExAllocatePoolWithTag(PagedPool,Length,'LbrA'));

	if(!Destination->Orderings)
		return STATUS_INSUFFICIENT_RESOURCES;

	Destination->Count									= Source->Count;
	Destination->Maximum								= Source->Maximum;

	if(Source->Count > 0)
		RtlCopyMemory(Destination->Orderings,Source->Orderings,Source->Count * sizeof(ARBITER_ORDERING));

	return STATUS_SUCCESS;
}

//
// add ordering [checked]
//
NTSTATUS ArbAddOrdering(__out PARBITER_ORDERING_LIST List,__in ULONGLONG Start,__in ULONGLONG End)
{
	//
	// validate parameters
	//
	if(End < Start)
		return STATUS_INVALID_PARAMETER;

	//
	// check if the buffer is full
	//
	if(List->Count == List->Maximum)
	{
		//
		// grow the buffer
		//
		ULONG Length									= (List->Count + 8) * sizeof(ARBITER_ORDERING);
		PARBITER_ORDERING Temp							= static_cast<PARBITER_ORDERING>(ExAllocatePoolWithTag(PagedPool,Length,'LbrA'));
		if(!Temp)
			return STATUS_INSUFFICIENT_RESOURCES;

		//
		// if we had any orderings copy them
		//
		if(List->Orderings)
		{
			RtlCopyMemory(Temp,List->Orderings,List->Count * sizeof(ARBITER_ORDERING));

			ExFreePool(List->Orderings);
		}

		List->Maximum									+= 8;
		List->Orderings									= Temp;
	}

	//
	// add the entry to the list
	//
	List->Orderings[List->Count].Start					= Start;
	List->Orderings[List->Count].End					= End;
	List->Count											+= 1;

	ASSERT(List->Count <= List->Maximum);

	return STATUS_SUCCESS;
}

//
// removes the range Start-End from all entries in the ordering list, splitting ranges into two or deleting them as necessary. [checked]
//
NTSTATUS ArbPruneOrdering(__inout PARBITER_ORDERING_LIST OrderingList,__in ULONGLONG Start,__in ULONGLONG End)
{
	ASSERT(OrderingList);
	ASSERT(OrderingList->Orderings);

	NTSTATUS Status										= STATUS_SUCCESS;
	PARBITER_ORDERING NewOrdering						= 0;
	PARBITER_ORDERING Temp								= 0;
	USHORT Count										= OrderingList->Count;

	__try
	{
		//
		// validate parameters
		//
		if(End < Start)
			try_leave(Status = STATUS_INVALID_PARAMETER)

		//
		// Allocate a buffer big enough for all eventualities
		//
		ULONG Length									= (OrderingList->Count * 2 + 1) *sizeof(ARBITER_ORDERING);
		NewOrdering										= static_cast<PARBITER_ORDERING>(ExAllocatePoolWithTag(PagedPool,Length,'LbrA'));
		if(!NewOrdering)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		PARBITER_ORDERING CurrentInsert					= NewOrdering;
		//
		// do we have a current ordering?
		//
		if(OrderingList->Count > 0)
		{
			//
			// iterate through the current ordering and prune accordingly
			//
			PARBITER_ORDERING Current					= 0;
			FOR_ALL_IN_ARRAY(OrderingList->Orderings, OrderingList->Count, Current)
			{
				if(End < Current->Start || Start > Current->End)
				{
					//
					// ****      or      ****
					//      ----    ----
					//
					// we don't overlap so copy the range unchanged
					//
					*CurrentInsert						= *Current;
					CurrentInsert						+= 1;
				}
				else if(Start > Current->Start)
				{
					if(End < Current->End)
					{
						//
						//   ****
						// --------
						//
						// split the range into two
						//
						CurrentInsert->Start			= End + 1;
						CurrentInsert->End				= Current->End;
						CurrentInsert					+= 1;

						CurrentInsert->Start			= Current->Start;
						CurrentInsert->End				= Start - 1;
						CurrentInsert					+= 1;
					}
					else
					{
						//
						//       **** or     ****
						// --------      --------
						//
						// prune the end of the range
						//
						ASSERT(End >= Current->End);

						CurrentInsert->Start			= Current->Start;
						CurrentInsert->End				= Start - 1;
						CurrentInsert					+= 1;
					}
				}
				else
				{
					ASSERT(Start <= Current->Start);

					if(End < Current->End)
					{
						//
						// ****       or ****
						//   --------    --------
						//
						// prune the start of the range
						//
						CurrentInsert->Start			= End + 1;
						CurrentInsert->End				= Current->End;
						CurrentInsert					+= 1;
					}
					else
					{
						ASSERT(End >= Current->End);

						//
						// ******** or ********
						//   ----      --------
						//
						// don't copy the range (ie. Delete it)
						//
					}
				}
			}
		}

		ASSERT(CurrentInsert - NewOrdering >= 0);

		Count											= static_cast<USHORT>(CurrentInsert - NewOrdering);

		//
		// check if we have any orderings left
		//
		if(Count > 0)
		{
			if(Count > OrderingList->Maximum)
			{
				//
				// there isn't enough space so allocate a new buffer
				//
				Temp									= static_cast<PARBITER_ORDERING>(ExAllocatePoolWithTag(PagedPool,Count * sizeof(ARBITER_ORDERING),'LbrA'));
				if(!Temp)
					try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);
			
				if(OrderingList->Orderings)
					ExFreePool(OrderingList->Orderings);

				OrderingList->Orderings					= Temp;
				OrderingList->Maximum					= Count;

			}

			//
			// copy the new ordering
			//
			RtlCopyMemory(OrderingList->Orderings,NewOrdering,Count * sizeof(ARBITER_ORDERING));
		}
	}
	__finally
	{
		if((!NT_SUCCESS(Status) || AbnormalTermination()))
		{
			if(Temp)
				ExFreePool(Temp);
		}
		else
		{
			OrderingList->Count								= Count;
		}

		//
		// free our temporary buffer
		//
		ExFreePool(NewOrdering);
	}

	return Status;
}

//
// free ordering list [checked]
//
VOID ArbFreeOrderingList(__in PARBITER_ORDERING_LIST List)
{
	if(List->Orderings)
	{
		ASSERT(List->Maximum);
		ExFreePool(List->Orderings);
	}

	List->Count											= 0;
	List->Maximum										= 0;
	List->Orderings										= 0;
}

//
// override conflict [checked]
//
BOOLEAN ArbOverrideConflict(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	if(!FlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_FIXED))
		return FALSE;

	RTL_RANGE_LIST_ITERATOR Iterator;
	PRTL_RANGE Current									= 0;
	BOOLEAN Ok											= FALSE;
	FOR_ALL_RANGES(Arbiter->PossibleAllocation,&Iterator,Current)
	{
		//
		// only test the overlapping ones
		//
		if(INTERSECT(Current->Start,Current->End,State->CurrentMinimum,State->CurrentMaximum))
		{
			//
			// check if we should ignore the range because of its attributes
			//
			if(FlagOn(Current->Attributes,State->RangeAvailableAttributes))
			{
				//
				// we DON'T set ok to true because we are just ignoring the range,
				// as RtlFindRange would have and thus it can't be the cause of RtlFindRange failing,
				// so ignoring it can't fix the conflict.
				//
				continue;
			}

			//
			// check if we are conflicting with ourselves AND the conflicting range is a fixed requirement
			//
			if(Current->Owner == State->Entry->PhysicalDeviceObject && FlagOn(State->CurrentAlternative->Flags,ARBITER_ALTERNATIVE_FLAG_FIXED))
			{
				State->Start							= State->CurrentMinimum;
				State->End								= State->CurrentMaximum;

				Ok										= TRUE;
				continue;
			}

			//
			// the conflict is still valid
			//
			return FALSE;
		}
	}

	return Ok;
}

//
// update priority [checked]
//
VOID ArbpUpdatePriority(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALTERNATIVE Alternative)
{
	/*
		The priorities are a LONG values organised as:

		<------Preferred priorities-----> <-----Ordinary Priorities----->

		MINLONG--------------------------0-----------------------------MAXLONG
										 ^                               ^ ^ ^
										 |                               | | |
										NULL            PREFERRED_RESERVED | |
																	RESERVED |
																		EXHAUSTED

		An ordinary priority is calculated the (index + 1) of the next ordering it intersects with (and has enough space for an allocation).

		A preferred priority is the ordinary priority * - 1

		In this way by examining each of the alternatives in priority order (lowest first) we achieve the desired allocation order of:

			(1) Preferred alternative with non-reserved resources
			(2) Alternatives with non-reserved resources
			(3) Preferred reserved resources
			(4) Reserved Resources

		MAXLONG the worst priority indicates that there are no more allocation ranges left.
	*/

	PAGED_CODE();

	LONG Priority										= Alternative->Priority;

	//
	// if we have already tried the reserved resources then we are out of luck!
	//
	if(Priority == ARBITER_PRIORITY_RESERVED || Priority == ARBITER_PRIORITY_PREFERRED_RESERVED)
	{
		Alternative->Priority							= ARBITER_PRIORITY_EXHAUSTED;
		return;
	}

	//
	// check if this is a preferred value - we treat them specially
	//
	BOOLEAN Preferred									= BooleanFlagOn(Alternative->Descriptor->Option,IO_RESOURCE_PREFERRED);

	//
	// if priority is NULL then we haven't started calculating one so we should start the search from the initial ordering
	//
	PARBITER_ORDERING Ordering							= 0;
	if(Priority == ARBITER_PRIORITY_NULL)
	{
		Ordering										= Arbiter->OrderingList.Orderings;
	}
	else
	{
		//
		// if we are a fixed resource then there is no point in trying to find another range
		// it will be the same and thus still conflict.
		// mark this alternative as exhausted
		//
		if(FlagOn(Alternative->Flags,ARBITER_ALTERNATIVE_FLAG_FIXED))
		{
			Alternative->Priority						= ARBITER_PRIORITY_EXHAUSTED;
			return;
		}

		ASSERT(ORDERING_INDEX_FROM_PRIORITY(Alternative->Priority) >= 0 && ORDERING_INDEX_FROM_PRIORITY(Alternative->Priority) < Arbiter->OrderingList.Count);

		Ordering										= &Arbiter->OrderingList.Orderings[ORDERING_INDEX_FROM_PRIORITY(Alternative->Priority) + 1];
	}

	//
	// now find the first member of the assignent ordering for this arbiter where we have an overlap big enough
	//
	FOR_REST_IN_ARRAY(Arbiter->OrderingList.Orderings,Arbiter->OrderingList.Count,Ordering)
	{
		//
		// is the ordering applicable?
		//
		if( INTERSECT(Alternative->Minimum,Alternative->Maximum,Ordering->Start,Ordering->End) &&
			INTERSECT_SIZE(Alternative->Minimum,Alternative->Maximum,Ordering->Start,Ordering->End) >= Alternative->Length)
		{
			//
			// this is out guy, calculate his priority
			//
			Alternative->Priority						= static_cast<LONG>(Ordering - Arbiter->OrderingList.Orderings + 1);

			//
			// preferred priorities are -ve
			//
			if(Preferred)
				Alternative->Priority					*= -1;

			return;
		}
	}

	//
	// we have runout of non-reserved resources so try the reserved ones
	//
	if(Preferred)
		Alternative->Priority							= ARBITER_PRIORITY_PREFERRED_RESERVED;
	else
		Alternative->Priority							= ARBITER_PRIORITY_RESERVED;
}

//
// add reserved [checked]
//
NTSTATUS ArbAddReserved(__in PARBITER_INSTANCE Arbiter,__in_opt PIO_RESOURCE_DESCRIPTOR Requirement,__in_opt PCM_PARTIAL_RESOURCE_DESCRIPTOR Resource)
{
	PAGED_CODE();

	return STATUS_NOT_SUPPORTED;
}

//
// query conflict callback [checked]
//
BOOLEAN ArbpQueryConflictCallback(__in PVOID Context,__in PRTL_RANGE Range)
{
	PRTL_RANGE *ConflictingRange						= static_cast<PRTL_RANGE*>(Context);

	PAGED_CODE();

	ARB_PRINT(2,("Possible conflict: (%p) 0x%I64x-0x%I64x Owner: %p",Range,Range->Start,Range->End,Range->Owner));

	//
	// remember the conflicting range
	//
	*ConflictingRange									= Range;

	//
	// we want to allow the rest of FindSuitableRange to determine if this really is a conflict.
	//
	return FALSE;
}

//
// query conflict [checked]
//
NTSTATUS ArbQueryConflict(__in PARBITER_INSTANCE Arbiter,__in PDEVICE_OBJECT PhysicalDeviceObject,__in PIO_RESOURCE_DESCRIPTOR ConflictingResource,
						  __out PULONG ConflictCount,__out PARBITER_CONFLICT_INFO *Conflicts)
{
	PAGED_CODE();

	ASSERT(PhysicalDeviceObject);
	ASSERT(ConflictingResource);
	ASSERT(ConflictCount);
	ASSERT(Conflicts);

	//
	// set up our conflict callback
	//
	RTL_RANGE_LIST BackupAllocation;
	BOOLEAN BackedUp									= FALSE;
	NTSTATUS Status										= STATUS_SUCCESS;
	PRTL_RANGE ConflictingRange							= 0;
	PARBITER_CONFLICT_INFO ConflictInfo					= 0;
	ULONG Count											= 0;
	ULONG Size											= 10;
	PVOID SavedContext									= Arbiter->ConflictCallbackContext;;
	PRTL_CONFLICT_RANGE_CALLBACK SavedCallback			= Arbiter->ConflictCallback;
	Arbiter->ConflictCallback							= &ArbpQueryConflictCallback;
	Arbiter->ConflictCallbackContext					= &ConflictingRange;

	__try
	{
		//
		// if there is a transaction in progress then we need to backup the the possible allocation so we can restore it when we are done.
		//
		if(Arbiter->TransactionInProgress)
		{
			RtlInitializeRangeList(&BackupAllocation);

			Status										= RtlCopyRangeList(&BackupAllocation,Arbiter->PossibleAllocation);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			RtlFreeRangeList(Arbiter->PossibleAllocation);

			BackedUp									= TRUE;
		}

		//
		// fake up the allocation state
		//
		Status											= RtlCopyRangeList(Arbiter->PossibleAllocation,Arbiter->Allocation);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		ARBITER_ALTERNATIVE Alternative;
		Status											= ArbpBuildAlternative(Arbiter,ConflictingResource,&Alternative);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

	
		ARBITER_ALLOCATION_STATE State;
		ARBITER_LIST_ENTRY Entry;
		RtlZeroMemory(&State,sizeof(ARBITER_ALLOCATION_STATE));

		State.Start										= Alternative.Minimum;
		State.End										= Alternative.Maximum;
		State.CurrentMinimum							= State.Start;
		State.CurrentMaximum							= State.End;
		State.CurrentAlternative						= &Alternative;
		State.AlternativeCount							= 1;
		State.Alternatives								= &Alternative;
		State.Flags										= ARBITER_STATE_FLAG_CONFLICT;
		State.Entry										= &Entry;

		//
		// BUGBUG(andrewth) - need to fill in more of this false entry
		//
		RtlZeroMemory(&Entry,sizeof(ARBITER_LIST_ENTRY));
		Entry.RequestSource								= ArbiterRequestPnpEnumerated;
		Entry.PhysicalDeviceObject						= PhysicalDeviceObject;

		//
		// BUGBUG(jamiehun) - we didn't allow for passing interface type
		// now we have to live with the decision
		// this really only comes into being for PCI Translator AFAIK
		// upshot of not doing this is we get false conflicts when PCI Boot alloc
		// maps over top of ISA alias
		// IoGetDeviceProperty generally does the right thing for the times we have to use this info
		//
		ULONG Dummy;
		if(!NT_SUCCESS(IoGetDeviceProperty(PhysicalDeviceObject,DevicePropertyLegacyBusType,sizeof(Entry.InterfaceType),&Entry.InterfaceType,&Dummy)))
		{
			//
			// not what I want to do! However this has the right effect - good enough for conflict detection
			//
			Entry.InterfaceType							= Isa;
		}

		if(!NT_SUCCESS(IoGetDeviceProperty(PhysicalDeviceObject,DevicePropertyBusNumber,sizeof(Entry.InterfaceType),&Entry.BusNumber,&Dummy)))
		{
			//
			// not what I want to do! However this has the right effect - good enough for conflict detection
			//
			Entry.BusNumber								= 0;
		}

		//
		// initialize the return buffers
		//
		Dummy											= Size * sizeof(ARBITER_CONFLICT_INFO);
		ConflictInfo									= static_cast<PARBITER_CONFLICT_INFO>(ExAllocatePoolWithTag(PagedPool,Dummy,'CbrA'));
		if(!ConflictInfo)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// perform any necessary preprocessing
		//
		Status											= Arbiter->PreprocessEntry(Arbiter,&State);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// remove self from list of possible allocations
		// status may be set, but can be ignored
		// we take ourself out of test completely, so that a user can
		// pick new values in context of rest of the world
		// if we decide to use RtlDeleteRange instead
		// make sure we do it for every alias formed in PreprocessEntry
		//
		RtlDeleteOwnersRanges(Arbiter->PossibleAllocation,State.Entry->PhysicalDeviceObject);

		//
		// keep trying to find a suitable range and each time we fail remember why.
		//
		ConflictingRange								= 0;
		State.CurrentMinimum							= State.Start;
		State.CurrentMaximum							= State.End;

		while(!Arbiter->FindSuitableRange(Arbiter,&State))
		{
			if(Count == Size)
			{
				//
				// we need to resize the return buffer
				//
				PARBITER_CONFLICT_INFO Temp				= ConflictInfo;
				Size									+= 5;
				Dummy									= Size * sizeof(ARBITER_CONFLICT_INFO);
				ConflictInfo							= static_cast<PARBITER_CONFLICT_INFO>(ExAllocatePoolWithTag(PagedPool,Dummy,'CbrA'));
				if(!ConflictInfo)
					try_leave(Status = STATUS_INSUFFICIENT_RESOURCES;ConflictInfo = Temp);

				RtlCopyMemory(ConflictInfo,Temp,Count * sizeof(ARBITER_CONFLICT_INFO));

				ExFreePool(Temp);
			}

			if(ConflictingRange)
			{
				ConflictInfo[Count].OwningObject		= static_cast<PDEVICE_OBJECT>(ConflictingRange->Owner);
				ConflictInfo[Count].Start				= ConflictingRange->Start;
				ConflictInfo[Count].End					= ConflictingRange->End;

				//
				// BUGBUG - maybe need some more info...
				//
				Count									+= 1;

				//
				// delete the range we conflicted with so we don't loop forever
				//
				Status									= RtlDeleteOwnersRanges(Arbiter->PossibleAllocation,ConflictingRange->Owner);
				if(!NT_SUCCESS(Status))
					try_leave(NOTHING);
			}
			else
			{
				//
				// someone isn't playing by the rules (such as ACPI!)
				//
				ARB_PRINT(0,("Conflict detected - but someone hasn't set conflicting info\n"));

				ConflictInfo[Count].OwningObject		= 0;
				ConflictInfo[Count].Start				= 0;
				ConflictInfo[Count].End					= 0xffffffffffffffff;

				//
				// BUGBUG - maybe need some more info...
				//
				Count									+= 1;

				//
				// we daren't continue at risk of looping forever
				//
				break;
			}

			//
			// reset for next round
			//
			ConflictingRange							= 0;
			State.CurrentMinimum						= State.Start;
			State.CurrentMaximum						= State.End;
		}

		RtlFreeRangeList(Arbiter->PossibleAllocation);

		if(Arbiter->TransactionInProgress)
		{
			Status										= RtlCopyRangeList(Arbiter->PossibleAllocation,&BackupAllocation);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			RtlFreeRangeList(&BackupAllocation);
			BackedUp									= FALSE;
		}

		*Conflicts										= ConflictInfo;
		*ConflictCount									= Count;
	}
	__finally
	{
		if(!NT_SUCCESS(Status) || AbnormalTermination())
		{
			if(ConflictInfo)
				ExFreePool(ConflictInfo);

			RtlFreeRangeList(Arbiter->PossibleAllocation);

			if(Arbiter->TransactionInProgress && BackedUp)
			{
				Status									= RtlCopyRangeList(Arbiter->PossibleAllocation,&BackupAllocation);
				RtlFreeRangeList(&BackupAllocation);
			}

			*Conflicts									= 0;
			*ConflictCount								= 0;
		}

		Arbiter->ConflictCallback						= SavedCallback;
		Arbiter->ConflictCallbackContext				= SavedContext;
	}

	return Status;
}

//
// start arbiter [checked]
//
NTSTATUS ArbStartArbiter(__in PARBITER_INSTANCE Arbiter,__in PCM_RESOURCE_LIST StartResources)
{
	PAGED_CODE();

	return STATUS_SUCCESS;
}

//
// share driver exclusive [checked]
//
BOOLEAN ArbShareDriverExclusive(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State)
{
	PAGED_CODE();

	//
	// is this a madeup pdo?
	//
	WCHAR Name[0x5]										= {0};
	ULONG Length										= 0;
	BOOLEAN MadeupPdo									= FALSE;
	if(NT_SUCCESS(IoGetDeviceProperty(State->Entry->PhysicalDeviceObject,DevicePropertyEnumeratorName,sizeof(Name),Name,&Length)) && !_wcsicmp(Name,L"ROOT"))
		MadeupPdo										= TRUE;

	PRTL_RANGE Range									= 0;
	PDEVICE_OBJECT FoundDevice							= 0;
	RTL_RANGE_LIST_ITERATOR Iterator;
	FOR_ALL_RANGES(Arbiter->PossibleAllocation,&Iterator,Range)
	{
		//
		// intersect with current
		//
		if(INTERSECT(Range->Start,Range->End,State->CurrentMinimum,State->CurrentMaximum))
		{
			//
			// ignore this range because of its attributes
			//
			if(FlagOn(Range->Attributes,State->RangeAvailableAttributes))
				continue;
			
			if(State->CurrentAlternative->Descriptor->ShareDisposition != CmResourceShareDriverExclusive && !FlagOn(Range->Attributes,2))
				continue;
		
			if(!Range->Owner)
				continue;

			PDEVICE_OBJECT OwerDevice					= static_cast<PDEVICE_OBJECT>(Range->Owner);
			if(MadeupPdo && NT_SUCCESS(IoGetDeviceProperty(OwerDevice,DevicePropertyEnumeratorName,sizeof(Name),Name,&Length)) && _wcsicmp(Name,L"ROOT"))
				MadeupPdo								= FALSE;
						
			if(MadeupPdo)
				break;

			BOOLEAN Found								= FALSE;
			for(FoundDevice	= OwerDevice->AttachedDevice; FoundDevice; FoundDevice = FoundDevice->AttachedDevice)
			{
				for(PDEVICE_OBJECT Temp = State->Entry->PhysicalDeviceObject->AttachedDevice; Temp; Temp = Temp->AttachedDevice)
				{
					if(Temp->DriverObject == FoundDevice->DriverObject)
					{
						Found							= TRUE;
						break;
					}
				}

				if(Found)
					break;
			}

			if(FoundDevice)
				break;
		}
	}

	if(FoundDevice)
	{
		ARB_PRINT(2,("Overriding conflict on IRQ %04x for driver %wZ\n",static_cast<USHORT>(State->Start),&FoundDevice->DriverObject->DriverName));

		State->Start									= State->CurrentMinimum;
		State->End										= State->CurrentMaximum;

		if(State->CurrentAlternative->Descriptor->ShareDisposition == CmResourceShareDriverExclusive)
			SetFlag(State->RangeAttributes,2);

		return TRUE;
	}

	return FALSE;
}

#if DBG
//
// indent [checked]
//
VOID ArbpIndent(__in ULONG Count)
{
	UCHAR spaces[81];

	ASSERT(Count <= 80);

	RtlFillMemory(spaces, Count, '*');

	spaces[Count]										= 0;

	DbgPrint("%s", spaces);
}

//
// Debug print level:
//    -1 = no messages
//     0 = vital messages only
//     1 = call trace
//     2 = verbose messages
//
LONG ArbDebugLevel										= -1;

//
// arbStopOnError works just like a debug level variable except instead of controlling whether a message is printed
// it controls whether we breakpoint on an error or not.
// likewise ArbReplayOnError controls if we replay fail arbitrations so we can debug them.
//
ULONG ArbStopOnError									= 0;
ULONG ArbReplayOnError									= 0;

PCHAR ArbpActionStrings[] =
{
	"ArbiterActionTestAllocation",
	"ArbiterActionRetestAllocation",
	"ArbiterActionCommitAllocation",
	"ArbiterActionRollbackAllocation",
	"ArbiterActionQueryAllocatedResources",
	"ArbiterActionWriteReservedResources",
	"ArbiterActionQueryConflict",
	"ArbiterActionQueryArbitrate",
	"ArbiterActionAddReserved",
	"ArbiterActionBootAllocation"
};

//
// dump range [checked]
//
VOID ArbDumpArbiterRange(__in LONG Level,__in PRTL_RANGE_LIST List,__in PCHAR RangeText)
{
	PAGED_CODE();

	BOOLEAN HeaderDisplayed								= FALSE;
	PRTL_RANGE Current									= 0;
	RTL_RANGE_LIST_ITERATOR Iterator;
	FOR_ALL_RANGES(List,&Iterator,Current)
	{
		if(!HeaderDisplayed)
		{
			HeaderDisplayed								= TRUE;
			ARB_PRINT(Level, ("  %s:\n", RangeText));
		}

		ARB_PRINT(Level,("    %I64x-%I64x %s%s O=0x%08x U=0x%08x\n",Current->Start,Current->End,
						 FlagOn(Current->Flags,1) ? "S" : " ",
						 FlagOn(Current->Flags,2) ? "C" : " ",
						 Current->Owner,Current->UserData));
	}

	if(!HeaderDisplayed)
	{
		ARB_PRINT(Level, ("  %s: <None>\n", RangeText));
	}
}

//
// dump arbiter instance [checked]
//
VOID ArbDumpArbiterInstance(__in LONG Level,__in PARBITER_INSTANCE Arbiter)
{

	PAGED_CODE();

	ARB_PRINT(Level,("---%S Arbiter State---\n",Arbiter->Name));

	ArbDumpArbiterRange(Level,Arbiter->Allocation,"Allocation");

	ArbDumpArbiterRange(Level,Arbiter->PossibleAllocation,"PossibleAllocation");
}

//
// dump arbitration list [checked]
//
VOID ArbDumpArbitrationList(__in LONG Level,__in PLIST_ENTRY ArbitrationList)
{
	PAGED_CODE();

	ARB_PRINT(Level, ("Arbitration List\n"));

	PARBITER_LIST_ENTRY Current							= 0;
	PDEVICE_OBJECT PreviousOwner						= 0;
	UCHAR andOr											= ' ';
	FOR_ALL_IN_LIST(ARBITER_LIST_ENTRY, ArbitrationList, Current)
	{
		if(PreviousOwner != Current->PhysicalDeviceObject)
		{
			PreviousOwner								= Current->PhysicalDeviceObject;

			ARB_PRINT(Level,("  Owning object 0x%08x\n",Current->PhysicalDeviceObject));

			ARB_PRINT(Level,("    Length  Alignment   Minimum Address - Maximum Address\n"));
		}

		PIO_RESOURCE_DESCRIPTOR Alternative					= 0;
		FOR_ALL_IN_ARRAY(Current->Alternatives,Current->AlternativeCount,Alternative)
		{
			ARB_PRINT(Level,("%c %8x   %8x  %08x%08x - %08x%08x  %s\n",andOr,Alternative->u.Generic.Length,Alternative->u.Generic.Alignment,
							 Alternative->u.Generic.MinimumAddress.HighPart,Alternative->u.Generic.MinimumAddress.LowPart,
							 Alternative->u.Generic.MaximumAddress.HighPart,Alternative->u.Generic.MaximumAddress.LowPart,
							 Alternative->Type == CmResourceTypeMemory ? "Memory" : "Port"));

			andOr = '|';
		}

		andOr = '&';
	}
}
#endif