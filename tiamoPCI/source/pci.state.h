//********************************************************************
//	created:	5:10:2008   7:17
//	file:		pci.state.h
//	author:		tiamo
//	purpose:	state
//********************************************************************

#pragma once

//
// initialize state
//
VOID PciInitializeState(__in PPCI_COMMON_EXTENSION CommonExtension);

//
// begin transition
//
NTSTATUS PciBeginStateTransition(__in PPCI_COMMON_EXTENSION CommonExtension,__in PCI_DEVICE_STATE NewState);

//
// commit transition
//
NTSTATUS PciCommitStateTransition(__in PPCI_COMMON_EXTENSION CommonExtension,__in PCI_DEVICE_STATE NewState);

//
// cancel transition
//
NTSTATUS PciCancelStateTransition(__in PPCI_COMMON_EXTENSION CommonExtension,__in PCI_DEVICE_STATE StateNotEntered);

//
// is in transition to state
//
BOOLEAN PciIsInTransitionToState(__in PPCI_COMMON_EXTENSION CommonExtension,__in PCI_DEVICE_STATE NextState);