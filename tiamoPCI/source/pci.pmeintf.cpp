//********************************************************************
//	created:	25:7:2008   20:53
//	file:		pci.pme_intf.cpp
//	author:		tiamo
//	purpose:	pme interface
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciPmeInterfaceConstructor)
#pragma alloc_text("PAGE",PciPmeInterfaceInitializer)
#pragma alloc_text("PAGE",PmeInterfaceDereference)
#pragma alloc_text("PAGE",PmeInterfaceReference)

//
// constructor [checked]
//
NTSTATUS PciPmeInterfaceConstructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
									__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	if(Version != PCI_PME_INTRF_STANDARD_VER)
		return STATUS_NOINTERFACE;

	Interface->Version									= Version;
	Interface->Context									= CommonExt;
	Interface->Size										= sizeof(PCI_PME_INTERFACE);
	Interface->InterfaceDereference						= &PmeInterfaceDereference;
	Interface->InterfaceReference						= &PmeInterfaceReference;

	PPCI_PME_INTERFACE PmeInterface						= CONTAINING_RECORD(Interface,PCI_PME_INTERFACE,Common);
	PmeInterface->GetPmeInformation						= reinterpret_cast<PPCI_PME_GET_PME_INFORMATION>(&PciPmeGetInformation);
	PmeInterface->ClearPmeStatus						= reinterpret_cast<PPCI_PME_CLEAR_PME_STATUS>(&PciPmeClearPmeStatus);
	PmeInterface->UpdateEnable							= reinterpret_cast<PPCI_PME_UPDATE_ENABLE>(&PciPmeUpdateEnable);

	return STATUS_SUCCESS;
}

//
// initializer [checked]
//
NTSTATUS PciPmeInterfaceInitializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	ASSERTMSG("PCI PciPmeInterfaceInitializer, unexpected call.",FALSE);

	return STATUS_UNSUCCESSFUL;
}

//
// reference [checked]
//
VOID PmeInterfaceReference(__in PVOID Context)
{
	PAGED_CODE();
}

//
// dereference [checked]
//
VOID PmeInterfaceDereference(__in PVOID Context)
{
	PAGED_CODE();
}

//
// update enable [checked]
//
VOID PciPmeUpdateEnable(__in PDEVICE_OBJECT Pdo,__in BOOLEAN Enable)
{
	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	PdoExt->NoTouchPmeEnable							= TRUE;
	PciPmeAdjustPmeEnable(PdoExt,Enable,FALSE);
}

//
// clear pme status [checked]
//
VOID PciPmeClearPmeStatus(__in PDEVICE_OBJECT Pdo)
{
	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	PciPmeAdjustPmeEnable(PdoExt,FALSE,TRUE);
}

//
// get pme info [checked]
//
VOID PciPmeGetInformation(__in PDEVICE_OBJECT Pdo,__out_opt PBOOLEAN HasPowerMgrCaps,__out_opt PBOOLEAN PmeAsserted,__out_opt PBOOLEAN PmeEnabled)
{
	PPCI_PDO_EXTENSION PdoExt							= static_cast<PPCI_PDO_EXTENSION>(Pdo->DeviceExtension);
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	if(HasPowerMgrCaps)
		*HasPowerMgrCaps								= 0;

	if(PmeAsserted)
		*PmeAsserted									= 0;

	if(PmeEnabled)
		*PmeEnabled										= 0;

	if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_PME_CAPS))
		return;

	//
	// read power management capabilites
	//
	PCI_PM_CAPABILITY PmeCaps;
	UCHAR PmeOffset										= PdoExt->CapabilitiesPtr;
	PmeOffset											= PciReadDeviceCapability(PdoExt,PmeOffset,PCI_CAPABILITY_ID_POWER_MANAGEMENT,&PmeCaps,sizeof(PmeCaps));

	//
	// unable to found power caps
	//
	if(!PmeOffset)
		return;

	if(HasPowerMgrCaps)
		*HasPowerMgrCaps								= TRUE;

	if(PmeEnabled)
		*PmeEnabled										= PmeCaps.PMCSR.ControlStatus.PMEEnable;

	if(PmeAsserted)
		*PmeAsserted									= PmeCaps.PMCSR.ControlStatus.PMEStatus;
}

//
// adjust pme [checked]
//
VOID PciPdoAdjustPmeEnable(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN Enable)
{
	if(PdoExt->NoTouchPmeEnable)
		PciDebugPrintf(7,"AdjustPmeEnable on pdox %08x but PME not owned.\n",PdoExt);
	else
		PciPmeAdjustPmeEnable(PdoExt,Enable,FALSE);
}

//
// adjust pme [checked]
//
VOID PciPmeAdjustPmeEnable(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN Enable,__in BOOLEAN ClearPmeStatus)
{
	if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_PME_CAPS))
		return;

	//
	// read power management capabilites
	//
	PCI_PM_CAPABILITY PmeCaps;
	UCHAR PmeOffset										= PdoExt->CapabilitiesPtr;
	PmeOffset											= PciReadDeviceCapability(PdoExt,PmeOffset,PCI_CAPABILITY_ID_POWER_MANAGEMENT,&PmeCaps,sizeof(PmeCaps));

	//
	// unable to found power caps
	//
	if(!PmeOffset)
		return;

	if(!ClearPmeStatus)
		PmeCaps.PMCSR.ControlStatus.PMEEnable			= Enable ? TRUE : FALSE;

	//
	// write Power Management Control
	//
	PciWriteDeviceConfig(PdoExt,&PmeCaps.PMCSR,PmeOffset + FIELD_OFFSET(PCI_PM_CAPABILITY,PMCSR),sizeof(PmeCaps.PMCSR));
}