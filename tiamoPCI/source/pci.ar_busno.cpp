//********************************************************************
//	created:	27:7:2008   2:05
//	file:		pci.ar_busno.cpp
//	author:		tiamo
//	purpose:	bus number arbiter
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",arbusno_Constructor)
#pragma alloc_text("PAGE",arbusno_Initializer)
#pragma alloc_text("PAGE",arbusno_UnpackRequirement)
#pragma alloc_text("PAGE",arbusno_PackResource)
#pragma alloc_text("PAGE",arbusno_UnpackResource)
#pragma alloc_text("PAGE",arbusno_ScoreRequirement)

//
// constructor [checked]
//
NTSTATUS arbusno_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
							 __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	if(reinterpret_cast<ULONG_PTR>(Data) != CmResourceTypeBusNumber)
		return STATUS_INVALID_PARAMETER_5;

	PARBITER_INTERFACE ArbInterface						= reinterpret_cast<PARBITER_INTERFACE>(Interface);
	ArbInterface->ArbiterHandler						= reinterpret_cast<PARBITER_HANDLER>(&ArbArbiterHandler);
	ArbInterface->InterfaceDereference					= reinterpret_cast<PINTERFACE_DEREFERENCE>(&PciDereferenceArbiter);
	ArbInterface->InterfaceReference					= reinterpret_cast<PINTERFACE_REFERENCE>(&PciReferenceArbiter);
	ArbInterface->Size									= sizeof(ARBITER_INTERFACE);
	ArbInterface->Version								= 0;
	ArbInterface->Flags									= 0;

	return PciArbiterInitializeInterface(CONTAINING_RECORD(CommonExt,PCI_FDO_EXTENSION,Common),PciArb_BusNumber,ArbInterface);
}

//
// initializer [checked]
//
NTSTATUS arbusno_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	RtlZeroMemory(&Instance->CommonInstance,sizeof(Instance->CommonInstance));

	Instance->CommonInstance.UnpackRequirement			= &arbusno_UnpackRequirement;
	Instance->CommonInstance.PackResource				= &arbusno_PackResource;
	Instance->CommonInstance.UnpackResource				= &arbusno_UnpackResource;
	Instance->CommonInstance.ScoreRequirement			= &arbusno_ScoreRequirement;

	return ArbInitializeArbiterInstance(&Instance->CommonInstance,Instance->BusFdoExtension->FunctionalDeviceObject,
										CmResourceTypeBusNumber,Instance->InstanceName,L"Pci",0);
}

//
// upack requirement [checked]
//
NTSTATUS arbusno_UnpackRequirement(__in PIO_RESOURCE_DESCRIPTOR Descriptor,__out PULONGLONG Minimum,__out PULONGLONG Maximum,__out PULONG Length,__out PULONG Alignment)
{
	PAGED_CODE();

	ASSERT(Descriptor);
	ASSERT(Descriptor->Type == CmResourceTypeBusNumber);

	*Minimum											= Descriptor->u.BusNumber.MinBusNumber;
	*Maximum											= Descriptor->u.BusNumber.MaxBusNumber;
	*Length												= Descriptor->u.BusNumber.Length;
	*Alignment											= 1;

	return STATUS_SUCCESS;
}

//
// pack resource [checked]
//
NTSTATUS arbusno_PackResource(__in PIO_RESOURCE_DESCRIPTOR Requirement,__in ULONGLONG Start,__out PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor)
{
	PAGED_CODE();

	ASSERT(Descriptor);
	ASSERT(Start < MAXULONG);
	ASSERT(Requirement);
	ASSERT(Requirement->Type == CmResourceTypeBusNumber);

	Descriptor->Type									= CmResourceTypeBusNumber;
	Descriptor->ShareDisposition						= Requirement->ShareDisposition;
	Descriptor->Flags									= Requirement->Flags;
	Descriptor->u.BusNumber.Start						= static_cast<ULONG>(Start);
	Descriptor->u.BusNumber.Length						= Requirement->u.BusNumber.Length;

	return STATUS_SUCCESS;
}

//
// unpack resource [checked]
//
NTSTATUS arbusno_UnpackResource(__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor,__out PULONGLONG Start,__out PULONG Length)
{
	PAGED_CODE();

	ASSERT(Descriptor);
	ASSERT(Start);
	ASSERT(Length);
	ASSERT(Descriptor->Type == CmResourceTypeBusNumber);

	*Start												= Descriptor->u.BusNumber.Start;
	*Length												= Descriptor->u.BusNumber.Length;

	return STATUS_SUCCESS;
}

//
// score requirement [checked]
//
LONG arbusno_ScoreRequirement(__in PIO_RESOURCE_DESCRIPTOR Descriptor)
{
	PAGED_CODE();

	ASSERT(Descriptor);
	ASSERT(Descriptor->Type == CmResourceTypeBusNumber);

	return (Descriptor->u.BusNumber.MaxBusNumber - Descriptor->u.BusNumber.MinBusNumber) / Descriptor->u.BusNumber.Length;
}