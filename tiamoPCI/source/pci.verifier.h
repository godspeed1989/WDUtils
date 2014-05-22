//********************************************************************
//	created:	5:10:2008   6:52
//	file:		pci.verifier.h
//	author:		tiamo
//	purpose:	verifier
//********************************************************************

#pragma once

//
// initialize verifier
//
VOID PciVerifierInit(__in PDRIVER_OBJECT DriverObject);

//
// unload
//
VOID PciVerifierUnload(__in PDRIVER_OBJECT DriverObject);

//
// get failure data
//
PVERIFIER_FAILURE_DATA PciVerifierRetrieveFailureData(__in ULONG Id);

//
// profile change callback
//
NTSTATUS PciVerifierProfileChangeCallback(__in PVOID NotificationStructure,__in PVOID Context);

//
// ensure tree consistancy
//
VOID PciVerifierEnsureTreeConsistancy();