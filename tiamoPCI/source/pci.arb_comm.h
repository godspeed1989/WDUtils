//********************************************************************
//	created:	5:10:2008   6:39
//	file:		pci.arb_comm.h
//	author:		tiamo
//	purpose:	arbiter common routine
//********************************************************************

#pragma once

//
// initialize arbiters
//
NTSTATUS PciInitializeArbiters(__in PPCI_FDO_EXTENSION FdoExt);

//
// destroy arbiter
//
VOID PciArbiterDestructor(__in PPCI_ARBITER_INSTANCE Instance);

//
// initialize arbiter ranges
//
NTSTATUS PciInitializeArbiterRanges(__in PPCI_FDO_EXTENSION FdoExt,__in PCM_RESOURCE_LIST CmResList);

//
// initialize arbiter interface
//
NTSTATUS PciArbiterInitializeInterface(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SIGNATURE Type,__in PARBITER_INTERFACE Interface);

//
// reference arbiter
//
VOID PciReferenceArbiter(__in PARBITER_INSTANCE Instance);

//
// dereference arbiter
//
VOID PciDereferenceArbiter(__in PARBITER_INSTANCE Instance);