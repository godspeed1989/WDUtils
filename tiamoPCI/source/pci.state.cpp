//********************************************************************
//	created:	23:7:2008   13:13
//	file:		pci.state.cpp
//	author:		tiamo
//	purpose:	state
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciBeginStateTransition)
#pragma alloc_text("PAGE",PciCommitStateTransition)
#pragma alloc_text("PAGE",PciCancelStateTransition)
#pragma alloc_text("PAGE",PciIsInTransitionToState)

//
// initialize device state [checked]
//
VOID PciInitializeState(__in PPCI_COMMON_EXTENSION CommonExtension)
{
	CommonExtension->DeviceState						= PciNotStarted;
	CommonExtension->TentativeNextState					= PciNotStarted;
}

//
// begine state transition [checked]
//
NTSTATUS PciBeginStateTransition(__in PPCI_COMMON_EXTENSION CommonExtension,__in PCI_DEVICE_STATE NewState)
{
	PAGED_CODE();

	PciDebugPrintf(1,"PCI Request to begin transition of Extension %p to %s ->",CommonExtension,PciTransitionText[NewState]);

	//
	// should not be in a transition state,and those states should be valid
	//
	ASSERT(CommonExtension->DeviceState == CommonExtension->TentativeNextState);
	ASSERT(NewState < PciMaxObjectState);
	ASSERT(CommonExtension->DeviceState < PciMaxObjectState);

	PCI_DEVICE_STATE CurrentState						= CommonExtension->DeviceState;
	NTSTATUS Status										= PnpStateTransitionArray[NewState][CurrentState];

	if(Status == STATUS_FAIL_CHECK)
	{
		PciDebugPrintf(1,"ERROR\nPCI: Error trying to enter state \"%s\" from state \"%s\"\n",PciTransitionText[NewState],PciTransitionText[CurrentState]);
		DbgBreakPoint();
	}
	else if(Status == STATUS_INVALID_DEVICE_REQUEST)
	{
		PciDebugPrintf(1,"ERROR\nPCI: Illegal request to try to enter state \"%s\" from state \"%s\"\n",PciTransitionText[NewState],PciTransitionText[CurrentState]);
	}

	ASSERT(NewState != CurrentState || !NT_SUCCESS(Status));

	if(NT_SUCCESS(Status))
		CommonExtension->TentativeNextState				= NewState;

	PciDebugPrintf(1,"->%x\n",Status);

	return Status;
}

//
// commit transition [checked]
//
NTSTATUS PciCommitStateTransition(__in PPCI_COMMON_EXTENSION CommonExtension,__in PCI_DEVICE_STATE NewState)
{
	PAGED_CODE();

	PciDebugPrintf(1,"PCI Commit transition of Extension %p to %s\n",CommonExtension,PciTransitionText[NewState]);

	ASSERT(NewState != PciSynchronizedOperation);
	ASSERT(CommonExtension->TentativeNextState == NewState);

	CommonExtension->DeviceState						= NewState;

	return STATUS_SUCCESS;
}

//
// cacnel transition [checked]
//
NTSTATUS PciCancelStateTransition(__in PPCI_COMMON_EXTENSION CommonExtension,__in PCI_DEVICE_STATE StateNotEntered)
{
	PAGED_CODE();

	PciDebugPrintf(1,"PCI Request to cancel transition of Extension %p to %s ->",CommonExtension,PciTransitionText[StateNotEntered]);

	if(CommonExtension->TentativeNextState == CommonExtension->DeviceState)
	{
		PciDebugPrintf(1,"%x\n",STATUS_INVALID_DEVICE_STATE);

		ASSERT(StateNotEntered < PciMaxObjectState);
		ASSERT(PnpStateCancelArray[StateNotEntered] != STATUS_FAIL_CHECK);

		return STATUS_INVALID_DEVICE_STATE;
	}

	ASSERT(CommonExtension->TentativeNextState == StateNotEntered);

	PciDebugPrintf(1,"%x\n",STATUS_SUCCESS);

	CommonExtension->TentativeNextState					= CommonExtension->DeviceState;

	return STATUS_SUCCESS;
}

//
// is in transition to state [checked]
//
BOOLEAN PciIsInTransitionToState(__in PPCI_COMMON_EXTENSION CommonExtension,__in PCI_DEVICE_STATE NextState)
{
	PAGED_CODE();

	ASSERT(NextState < PciMaxObjectState);

	return CommonExtension->TentativeNextState != CommonExtension->DeviceState && CommonExtension->TentativeNextState == NextState;
}