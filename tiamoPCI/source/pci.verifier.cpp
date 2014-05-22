//********************************************************************
//	created:	22:7:2008   23:26
//	file:		pci.verifier.cpp
//	author:		tiamo
//	purpose:	verifier
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("INIT",PciVerifierInit)
#pragma alloc_text("PAGE",PciVerifierUnload)
#pragma alloc_text("PAGE",PciVerifierProfileChangeCallback)
#pragma alloc_text("PAGE",PciVerifierEnsureTreeConsistancy)

//
// initialize verifier [checked]
//
VOID PciVerifierInit(__in PDRIVER_OBJECT DriverObject)
{
	PAGED_CODE();

	if(!VfIsVerificationEnabled(2,0))
		return;

	if(NT_SUCCESS(IoRegisterPlugPlayNotification(EventCategoryHardwareProfileChange,0,0,DriverObject,&PciVerifierProfileChangeCallback,0,&PciVerifierNotificationHandle)))
		PciVerifierRegistered							= TRUE;
}

//
// verifier unload [checked]
//
VOID PciVerifierUnload(__in PDRIVER_OBJECT DriverObject)
{
	PAGED_CODE();

	if(!PciVerifierRegistered)
		return;

	ASSERT(PciVerifierNotificationHandle);

	NTSTATUS Status										= IoUnregisterPlugPlayNotification(PciVerifierNotificationHandle);
	ASSERT(NT_SUCCESS(Status));

	PciVerifierRegistered								= FALSE;
	PciVerifierNotificationHandle						= 0;
}

//
// get failure data
//
PVERIFIER_FAILURE_DATA PciVerifierRetrieveFailureData(__in ULONG Id)
{
	for(ULONG i = 0; i < ARRAYSIZE(PciVerifierFailureTable); i ++)
	{
		if(PciVerifierFailureTable[i].Id == Id)
			return PciVerifierFailureTable + i;
	}

	ASSERT(FALSE);
	return 0;
}

//
// profile change callback
//
NTSTATUS PciVerifierProfileChangeCallback(__in PVOID NotificationStructure,__in PVOID Context)
{
	PAGED_CODE();

	PHWPROFILE_CHANGE_NOTIFICATION ProfileChange		= static_cast<PHWPROFILE_CHANGE_NOTIFICATION>(NotificationStructure);
	if(RtlCompareMemory(&GUID_HWPROFILE_CHANGE_COMPLETE,&ProfileChange->Event,sizeof(GUID)) == sizeof(GUID))
		PciVerifierEnsureTreeConsistancy();

	return STATUS_SUCCESS;
}

//
// ensure tree consistancy
//
VOID PciVerifierEnsureTreeConsistancy()
{
	PAGED_CODE();

	KeEnterCriticalRegion();
	KeWaitForSingleObject(&PciGlobalLock,Executive,KernelMode,FALSE,0);

	KeEnterCriticalRegion();
	KeWaitForSingleObject(&PciBusLock,Executive,KernelMode,FALSE,0);

	PPCI_FDO_EXTENSION FdoExt							= CONTAINING_RECORD(PciFdoExtensionListHead.Next,PCI_FDO_EXTENSION,Common.ListEntry.Next);
	while(FdoExt)
	{
		if(FdoExt != FdoExt->BusRootFdoExtension)
		{
			PPCI_PDO_EXTENSION PdoExt					= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);
			if(!PdoExt->NotPresent && PdoExt->Common.DeviceState != PciSurpriseRemoved)
			{
				ASSERT(PdoExt->HeaderType == PCI_BRIDGE_TYPE || PdoExt->HeaderType == PCI_CARDBUS_BRIDGE_TYPE);
				PCI_COMMON_CONFIG Config;
				PciReadDeviceConfig(PdoExt,&Config,0,sizeof(Config));

				if( Config.u.type1.PrimaryBus != PdoExt->Dependent.type1.PrimaryBus ||
					Config.u.type1.SecondaryBus != PdoExt->Dependent.type1.SecondaryBus ||
					Config.u.type1.SubordinateBus != PdoExt->Dependent.type1.SubordinateBus)
				{
					PVERIFIER_FAILURE_DATA Data			= PciVerifierRetrieveFailureData(1);
					ASSERT(Data);
					VfFailSystemBIOS(0xf6,1,Data->Offset4,&Data->Offset8,Data->FailureMessage,"%DevObj",PdoExt->PhysicalDeviceObject);
				}
			}
		}
		FdoExt											= CONTAINING_RECORD(FdoExt->Common.ListEntry.Next,PCI_FDO_EXTENSION,Common.ListEntry.Next);
	}

	KeSetEvent(&PciBusLock,IO_NO_INCREMENT,FALSE);
	KeLeaveCriticalRegion();

	KeSetEvent(&PciGlobalLock,IO_NO_INCREMENT,FALSE);
	KeLeaveCriticalRegion();
}