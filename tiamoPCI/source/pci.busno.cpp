//********************************************************************
//	created:	22:7:2008   23:39
//	file:		pci.busno.cpp
//	author:		tiamo
//	purpose:	bus number
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciAreBusNumbersConfigured)
#pragma alloc_text("PAGE",PciSetBusNumbers)
#pragma alloc_text("PAGE",PciDisableBridge)
#pragma alloc_text("PAGE",PciConfigureBusNumbers)
#pragma alloc_text("PAGE",PciFitBridge)
#pragma alloc_text("PAGE",PciUpdateAncestorSubordinateBuses)
#pragma alloc_text("PAGE",PciSpreadBridges)
#pragma alloc_text("PAGE",PciFindBridgeNumberLimit)
#pragma alloc_text("PAGE",PciFindBridgeNumberLimitWorker)

//
// are bus numbers configured [checked]
//
BOOLEAN PciAreBusNumbersConfigured(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	UCHAR BaseBus										= PdoExt->ParentFdoExtension->BaseBus;

	if(PdoExt->Dependent.type1.PrimaryBus != BaseBus)
		return FALSE;

	if(PdoExt->Dependent.type1.SecondaryBus <= BaseBus)
		return FALSE;

	if(PdoExt->Dependent.type1.SubordinateBus < PdoExt->Dependent.type1.SecondaryBus)
		return FALSE;

	return TRUE;
}

//
// set bus number [checked]
//
NTSTATUS PciSetBusNumbers(__in PPCI_PDO_EXTENSION PdoExt,__in UCHAR Primary,__in UCHAR Secondary,__in UCHAR Subordinate)
{
	PAGED_CODE();

	ASSERT(Primary < Secondary || (Primary == 0 && Secondary == 0));
	ASSERT(Secondary <= Subordinate);

	UCHAR Buffer[3]										= {Primary,Secondary,Subordinate};

	KeEnterCriticalRegion();
	KeWaitForSingleObject(&PciBusLock,Executive,KernelMode,FALSE,0);

	PdoExt->Dependent.type1.PrimaryBus					= Primary;
	PdoExt->Dependent.type1.SecondaryBus				= Secondary;
	PdoExt->Dependent.type1.SubordinateBus				= Subordinate;
	PdoExt->Dependent.type1.WeChangedBusNumbers			= TRUE;

	PciWriteDeviceConfig(PdoExt,Buffer,FIELD_OFFSET(PCI_COMMON_HEADER,u.type1.PrimaryBus),sizeof(Buffer));

	KeSetEvent(&PciBusLock,IO_NO_INCREMENT,FALSE);
	KeLeaveCriticalRegion();

	return STATUS_SUCCESS;
}

//
// disable bridge [checked]
//
VOID PciDisableBridge(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	ASSERT(PdoExt->Common.DeviceState == PciNotStarted);

	PciSetBusNumbers(PdoExt,0,0,0);

	PciDecodeEnable(PdoExt,FALSE,0);
}

//
// configure bus numbers [checked]
//
VOID PciConfigureBusNumbers(__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	PPCI_PDO_EXTENSION PdoExt							= 0;
	if(FdoExt != FdoExt->BusRootFdoExtension)
		PdoExt											= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);

	KeEnterCriticalRegion();
	KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

	PPCI_PDO_EXTENSION BridgeExt						= FdoExt->ChildBridgePdoList;
	ULONG BridgeCount									= 0;
	ULONG ConfiguredBridgeCount							= 0;
	while(BridgeExt)
	{
		if(BridgeExt->NotPresent)
		{
			PciDebugPrintf(0x100000,"Skipping not present bridge PDOX @ %p\n",BridgeExt);
		}
		else
		{
			BridgeCount									+= 1;

			if((!PdoExt || !PdoExt->Dependent.type1.WeChangedBusNumbers || BridgeExt->Common.DeviceState != PciNotStarted) && PciAreBusNumbersConfigured(BridgeExt))
				ConfiguredBridgeCount					+= 1;
			else
				PciDisableBridge(BridgeExt);

		}

		BridgeExt										= BridgeExt->NextBridge;
	}

	KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
	KeLeaveCriticalRegion();

	if(!BridgeCount)
	{
		PciDebugPrintf(0x100000,"PCI - No bridges found on bus 0x%x\n",FdoExt->BaseBus);
		return;
	}

	if(BridgeCount == ConfiguredBridgeCount)
	{
		PciDebugPrintf(0x100000,"PCI - 0x%x bridges found on bus 0x%x - all already configured\n",BridgeCount,FdoExt->BaseBus);
		return;
	}

	if(!ConfiguredBridgeCount)
	{
		PciDebugPrintf(0x100000,"PCI - 0x%x bridges found on bus 0x%x - all need configuration\n",BridgeCount,FdoExt->BaseBus);

		return PciSpreadBridges(FdoExt,BridgeCount);
	}

	ASSERT(ConfiguredBridgeCount < BridgeCount);

	PciDebugPrintf(0x100000,"PCI - 0x%x bridges found on bus 0x%x - 0x%x need configuration\n",BridgeCount,FdoExt->BaseBus,BridgeCount - ConfiguredBridgeCount);

	BridgeExt											= FdoExt->ChildBridgePdoList;
	while(BridgeExt)
	{
		if(BridgeExt->NotPresent)
		{
			PciDebugPrintf(0x100000,"Skipping not present bridge PDOX @ %p\n",BridgeExt);
		}
		else
		{
			if((PdoExt && PdoExt->Dependent.type1.WeChangedBusNumbers && BridgeExt->Common.DeviceState == PciNotStarted) || !PciAreBusNumbersConfigured(BridgeExt))
			{
				ASSERT(!BridgeExt->Dependent.type1.PrimaryBus && !BridgeExt->Dependent.type1.SecondaryBus && !BridgeExt->Dependent.type1.SubordinateBus);

				PciFitBridge(FdoExt,PdoExt);
			}
		}

		BridgeExt										= BridgeExt->NextBridge;
	}
}

//
// spread bridges [checked]
//
VOID PciSpreadBridges(__in PPCI_FDO_EXTENSION ParentFdoExt,__in ULONG BridgeCount)
{
	PAGED_CODE();

	ASSERT(ParentFdoExt->BaseBus < PCI_MAX_BRIDGE_NUMBER);

	UCHAR BaseBus										= ParentFdoExt->BaseBus;
	UCHAR Limit											= PciFindBridgeNumberLimit(ParentFdoExt,BaseBus);

	ASSERT(Limit >= BaseBus);

	UCHAR Gaps											= Limit - BaseBus;
	if(!Gaps)
		return;

	UCHAR Step											= static_cast<UCHAR>(BridgeCount < Gaps ? Gaps / (BridgeCount + 1) : 1);
	UCHAR SecondaryBus									= BaseBus + 1;
	UCHAR MaxAssigned									= 0;

	PPCI_PDO_EXTENSION BridgeExt						= ParentFdoExt->ChildBridgePdoList;
	while(BridgeExt)
	{
		if(BridgeExt->NotPresent)
		{
			PciDebugPrintf(0x100000,"Skipping not present bridge PDOX @ %p\n",BridgeExt);
		}
		else
		{
			ASSERT(!PciAreBusNumbersConfigured(BridgeExt));

			PciSetBusNumbers(BridgeExt,BaseBus,SecondaryBus,SecondaryBus);

			MaxAssigned									= SecondaryBus;

			if(static_cast<ULONG>(SecondaryBus) + static_cast<ULONG>(Step) < static_cast<ULONG>(SecondaryBus))
				break;

			if(static_cast<ULONG>(SecondaryBus) + static_cast<ULONG>(Step) > static_cast<ULONG>(Limit))
				break;

			SecondaryBus								= SecondaryBus + Step;
		}

		BridgeExt										= BridgeExt->NextBridge;
	}

	ASSERT(MaxAssigned);

	PciUpdateAncestorSubordinateBuses(ParentFdoExt,MaxAssigned);
}

//
// fit bridge [checked]
//
VOID PciFitBridge(__in PPCI_FDO_EXTENSION ParentFdoExt,__in PPCI_PDO_EXTENSION BridgePdoExt)
{
	PAGED_CODE();

	UCHAR Lowest										= 0xff;
	UCHAR MaxGaps										= 0;
	UCHAR SecondaryBusForMaxGaps						= 0;

	PPCI_PDO_EXTENSION BridgeExt						= ParentFdoExt->ChildBridgePdoList;
	while(BridgeExt)
	{
		if(BridgeExt->NotPresent)
		{
			PciDebugPrintf(0x100000,"Skipping not present bridge PDOX @ %p\n",BridgeExt);
		}
		else if(PciAreBusNumbersConfigured(BridgeExt))
		{
			UCHAR BaseBus								= BridgeExt->Dependent.type1.SubordinateBus;
			UCHAR Limit									= PciFindBridgeNumberLimit(ParentFdoExt,BaseBus);

			ASSERT(Limit >= BaseBus);

			if(Limit - BaseBus > MaxGaps)
			{
				ASSERT(Limit > BaseBus);
				MaxGaps									= Limit - BaseBus;
				SecondaryBusForMaxGaps					= BaseBus + 1;
			}

			if(BridgeExt->Dependent.type1.SecondaryBus < Lowest)
				Lowest									= BridgeExt->Dependent.type1.SecondaryBus;
		}

		BridgeExt										= BridgeExt->NextBridge;
	}

	ASSERT(Lowest > ParentFdoExt->BaseBus);

	UCHAR Gaps											= 0;
	UCHAR SecondaryBus									= 0;

	if(Lowest - ParentFdoExt->BaseBus - 1 <= MaxGaps)
	{
		Gaps											= MaxGaps;
		SecondaryBus									= SecondaryBusForMaxGaps;
	}
	else
	{
		SecondaryBus									= ParentFdoExt->BaseBus + 1;
		Gaps											= Lowest - ParentFdoExt->BaseBus - 1;
	}

	if(Gaps >= 1)
	{
		SecondaryBus									= static_cast<UCHAR>(SecondaryBus + Gaps / 2);
		PciSetBusNumbers(BridgePdoExt,ParentFdoExt->BaseBus,SecondaryBus,SecondaryBus);

		PciUpdateAncestorSubordinateBuses(ParentFdoExt,BridgePdoExt->Dependent.type1.SecondaryBus);
	}
}

//
// update subordinate bus [checked]
//
VOID PciUpdateAncestorSubordinateBuses(__in PPCI_FDO_EXTENSION FdoExt,__in UCHAR MaxBus)
{
	PAGED_CODE();

	while(FdoExt->ParentFdoExtension)
	{
		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension);
		ASSERT(!PdoExt->NotPresent);

		if(PdoExt->Dependent.type1.SubordinateBus < MaxBus)
		{
			PdoExt->Dependent.type1.SubordinateBus		= MaxBus;
			PciWriteDeviceConfig(PdoExt,&MaxBus,FIELD_OFFSET(PCI_COMMON_HEADER,u.type1.SubordinateBus),sizeof(MaxBus));
		}

		FdoExt											= FdoExt->ParentFdoExtension;
	}

	ASSERT(FdoExt == FdoExt->BusRootFdoExtension);
	ASSERT(MaxBus <= FdoExt->MaxSubordinateBus);
}

//
// find bridge limit [checked]
//
UCHAR PciFindBridgeNumberLimit(__in PPCI_FDO_EXTENSION FdoExt,__in UCHAR BaseBus)
{
	PAGED_CODE();

	BOOLEAN Include										= FALSE;
	UCHAR Limit											= PciFindBridgeNumberLimitWorker(FdoExt,FdoExt,BaseBus,&Include);
	if(Include)
		return Limit;

	ASSERT(Limit > 0);
	return Limit - 1;
}

//
// find bridge limit worker [checked]
//
UCHAR PciFindBridgeNumberLimitWorker(__in PPCI_FDO_EXTENSION StartFdoExt,__in PPCI_FDO_EXTENSION CurFdoExt,__in UCHAR BaseBus,__out PBOOLEAN Include)
{
	PAGED_CODE();

	if(CurFdoExt != StartFdoExt)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&CurFdoExt->ChildListLock,Executive,KernelMode,FALSE,0);
	}

	PPCI_PDO_EXTENSION ChildBridgeExt					= CurFdoExt->ChildBridgePdoList;
	UCHAR Limit											= 0;
	if(ChildBridgeExt)
	{
		while(ChildBridgeExt)
		{
			if(ChildBridgeExt->NotPresent)
			{
				PciDebugPrintf(0x100000,"Skipping not present bridge PDOX @ %p\n",ChildBridgeExt);
			}
			else if(PciAreBusNumbersConfigured(ChildBridgeExt))
			{
				if(ChildBridgeExt->Dependent.type1.SecondaryBus > BaseBus && (ChildBridgeExt->Dependent.type1.SecondaryBus < Limit || !Limit))
					Limit								= ChildBridgeExt->Dependent.type1.SecondaryBus;
			}

			ChildBridgeExt								= ChildBridgeExt->NextBridge;
		}
	}

	if(Limit)
	{
		*Include										= FALSE;
	}
	else
	{
		if(CurFdoExt->ParentFdoExtension)
		{
			Limit										= PciFindBridgeNumberLimitWorker(StartFdoExt,CurFdoExt->ParentFdoExtension,BaseBus,Include);
		}
		else
		{
			Limit										= CurFdoExt->MaxSubordinateBus;
			*Include									= TRUE;
		}
	}

	if(CurFdoExt != StartFdoExt)
	{
		KeSetEvent(&CurFdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}

	return Limit;
}