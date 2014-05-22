//********************************************************************
//	created:	22:7:2008   16:13
//	file:		pci.debug.cpp
//	author:		tiamo
//	purpose:	debug
//********************************************************************

#include "stdafx.h"
#include <stdarg.h>

//
// deubg printf [checked]
//
VOID PciDebugPrintf(__in ULONG Level,__in PCHAR Format,...)
{
	if(FlagOn(Level,PciDebug))
	{
		va_list list;
		va_start(list,Format);

		RtlStringCchVPrintfA(PciDebugBuffer,ARRAYSIZE(PciDebugBuffer) - 1,Format,list);

		va_end(list);

		DbgPrint("%s",PciDebugBuffer);
	}
}

//
// debug irp [checked]
//
BOOLEAN  PciDebugIrpDispatchDisplay(__in PIO_STACK_LOCATION IrpSp,__in PPCI_COMMON_EXTENSION CommonExtension,__in ULONG MaxFunction)
{
	ULONG BreakFlags									= 0;
	PCHAR MinorTextString								= 0;

	if(IrpSp->MajorFunction == IRP_MJ_POWER)
	{
		BreakFlags										= CommonExtension->ExtensionType == PciPdoExtensionType ? PciBreakOnPdoPowerIrp : PciBreakOnFdoPowerIrp;
		MinorTextString									= PciDebugPoIrpTypeToText(IrpSp->MinorFunction);
	}
	else if(IrpSp->MajorFunction == IRP_MJ_PNP)
	{
		BreakFlags										= CommonExtension->ExtensionType == PciPdoExtensionType ? PciBreakOnPdoPnpIrp : PciBreakOnFdoPnpIrp;
		MinorTextString									= PciDebugPnpIrpTypeToText(IrpSp->MinorFunction);
	}

	ULONG DebugLevel									= 0;
	if(CommonExtension->ExtensionType == PciPdoExtensionType)
	{
		if(IrpSp->MajorFunction == IRP_MJ_PNP)
			DebugLevel									= 0x200;
		else if(IrpSp->MajorFunction == IRP_MJ_POWER)
			DebugLevel									= 0x800;

		PPCI_PDO_EXTENSION PdoExt						= static_cast<PPCI_PDO_EXTENSION>(static_cast<PVOID>(CommonExtension));

		PciDebugPrintf(DebugLevel,"PDO(b=0x%x, d=0x%x, f=0x%x)<-%s\n",PdoExt->ParentFdoExtension->BaseBus,PdoExt->Slot.u.bits.DeviceNumber,
					   PdoExt->Slot.u.bits.FunctionNumber,MinorTextString);
	}
	else
	{
		if(IrpSp->MajorFunction == IRP_MJ_PNP)
			DebugLevel									= 0x100;
		else if(IrpSp->MajorFunction == IRP_MJ_POWER)
			DebugLevel									= 0x400;

		PciDebugPrintf(DebugLevel,"FDO(%x)<-%s\n",CommonExtension,MinorTextString);
	}

	if(IrpSp->MinorFunction > MaxFunction)
		PciDebugPrintf(1,"Unknown IRP, minor = 0x%x\n");

	return BooleanFlagOn(BreakFlags,1 << IrpSp->MinorFunction);
}

//
// get power minor type text [checked]
//
PCHAR PciDebugPoIrpTypeToText(__in UCHAR Minor)
{
	static PCHAR PoTypeText[] =
	{
		"WAIT_WAKE",
		"POWER_SEQUENCE",
		"SET_POWER",
		"QUERY_POWER",
	};

	if(Minor > IRP_MN_QUERY_POWER)
		return "** UNKNOWN PO IRP Minor Code **";

	return PoTypeText[Minor];
}

//
// get pnp minor type text [checked]
//
PCHAR PciDebugPnpIrpTypeToText(__in UCHAR Minor)
{
	static PCHAR PnpTypeText[] =
	{
		"START_DEVICE",
		"QUERY_REMOVE_DEVICE",
		"REMOVE_DEVICE",
		"CANCEL_REMOVE_DEVICE",
		"STOP_DEVICE",
		"QUERY_STOP_DEVICE",
		"CANCEL_STOP_DEVICE",
		"QUERY_DEVICE_RELATIONS",
		"QUERY_INTERFACE",
		"QUERY_CAPABILITIES",
		"QUERY_RESOURCE",
		"QUERY_RESOURCE_REQUIREMENTS",
		"QUERY_DEVICE_TEXT",
		"FILTER_RESOURCE_REQUIREMENTS",
		"** UNKNOWN PNP IRP Minor Code **",
		"READ_CONFIG",
		"WRITE_CONFIG",
		"EJECT",
		"SET_LOCK",
		"QUERY_ID",
		"QUERY_PNP_DEVICE_STATE",
		"QUERY_BUS_INFORMATION",
		"DEVICE_USAGE_NOTIFICATION",
		"SURPRISE_REMOVAL",
		"QUERY_LEGACY_BUS_INFORMATION",
	};

	if(Minor > IRP_MN_QUERY_LEGACY_BUS_INFORMATION)
		return "** UNKNOWN PNP IRP Minor Code **";

	return PnpTypeText[Minor];
}

//
// print cm resource list [checked]
//
VOID PciDebugPrintCmResList(__in ULONG Level,__in PCM_RESOURCE_LIST CmResList)
{
	if(!CmResList)
		return;

	if(!FlagOn(Level,PciDebug))
		return;

	DbgPrint("    CM_RESOURCE_LIST (PCI Bus Driver) (List Count = %d)\n",CmResList->Count);

	PCM_FULL_RESOURCE_DESCRIPTOR FullDesc				= CmResList->List;

	for(ULONG i = 0; i < CmResList->Count; i ++)
	{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDesc		= FullDesc->PartialResourceList.PartialDescriptors;

		DbgPrint("     InterfaceType        %d\n",FullDesc->InterfaceType);
		DbgPrint("     BusNumber            0x%x\n",FullDesc->BusNumber);

		for(ULONG j = 0; j < FullDesc->PartialResourceList.Count; j ++)
		{
			PciDebugPrintPartialResource(Level,PartialDesc);

			PartialDesc									= PciNextPartialDescriptor(PartialDesc);
		}

		FullDesc										= reinterpret_cast<PCM_FULL_RESOURCE_DESCRIPTOR>(PartialDesc);
	}

	DbgPrint("\n");
}

//
// print partial desc [checked]
//
VOID PciDebugPrintPartialResource(__in ULONG Level,__in PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDesc)
{
	if(!PartialDesc)
		return;

	if(!FlagOn(Level,PciDebug))
		return;

	DbgPrint("     Partial Resource Descriptor @0x%x\n",PartialDesc);
	DbgPrint("        Type             = %d (%s)\n",PartialDesc->Type,PciDebugCmResourceTypeToText(PartialDesc->Type));
	DbgPrint("        ShareDisposition = %d\n",PartialDesc->ShareDisposition);
	DbgPrint("        Flags            = 0x%04X\n",PartialDesc->Flags);
	DbgPrint("        Data[%d] = %08x  %08x  %08x\n",0,PartialDesc->u.Generic.Start.u.HighPart,PartialDesc->u.Generic.Start.u.LowPart,PartialDesc->u.Generic.Length);
}

//
// cm resource type text [checked]
//
PCHAR PciDebugCmResourceTypeToText(__in UCHAR Type)
{
	switch(Type)
	{
	case CmResourceTypeNull:
		return "CmResourceTypeNull";

	case CmResourceTypePort:
		return "CmResourceTypePort";

	case CmResourceTypeInterrupt:
		return "CmResourceTypeInterrupt";

	case CmResourceTypeMemory:
		return "CmResourceTypeMemory";

	case CmResourceTypeDma:
		return "CmResourceTypeDma";

	case CmResourceTypeDeviceSpecific:
		return "CmResourceTypeDeviceSpecific";

	case CmResourceTypeBusNumber:
		return "CmResourceTypeBusNumber";

	case CmResourceTypeMemoryLarge:
		return "CmResourceTypeMemoryLarge";

	case CmResourceTypeConfigData:
		return "CmResourceTypeConfigData";

	case CmResourceTypeDevicePrivate:
		return "CmResourceTypeDevicePrivate";

	case CmResourceTypePcCardConfig:
		return "CmResourceTypePcCardConfig";

	case CmResourceTypeMfCardConfig:
		return "CmResourceTypeMfCardConfig";
	}

	return "*** INVALID RESOURCE TYPE ***";
}

//
// debug print io req list [checked]
//
VOID PciDebugPrintIoResReqList(__in PIO_RESOURCE_REQUIREMENTS_LIST IoReqList)
{
	if(PciDebug < 7 || !IoReqList)
		return;

	DbgPrint("  IO_RESOURCE_REQUIREMENTS_LIST (PCI Bus Driver)\n");
	DbgPrint("     InterfaceType        %d\n",IoReqList->InterfaceType);
	DbgPrint("     BusNumber            0x%x\n",IoReqList->BusNumber);
	DbgPrint("     SlotNumber           %d (0x%x)\n",IoReqList->SlotNumber,IoReqList->SlotNumber);
	DbgPrint("     AlternativeLists     %d\n",IoReqList->AlternativeLists);

	for(ULONG i = 0; i < IoReqList->AlternativeLists; i ++)
	{
		DbgPrint("\n     List[%d].Count = %d\n",i,IoReqList->List[i].Count);
		for(ULONG j = 0; j < IoReqList->List[i].Count; j ++)
			PciDebugPrintIoResource(IoReqList->List[i].Descriptors + j);
	}
}

//
// debug print io resource descriptor [checked]
//
VOID PciDebugPrintIoResource(__in PIO_RESOURCE_DESCRIPTOR Desc)
{
	DbgPrint("     IoResource Descriptor dump:  Descriptor @0x%x\n",Desc);
	DbgPrint("        Option           = 0x%x\n",Desc->Option);
	DbgPrint("        Type             = %d (%s)\n",Desc->Type,PciDebugCmResourceTypeToText(Desc->Type));
	DbgPrint("        ShareDisposition = %d\n",Desc->ShareDisposition);
	DbgPrint("        Flags            = 0x%04X\n",Desc->Flags);

	PULONG Buffer										= Add2Ptr(Desc,FIELD_OFFSET(IO_RESOURCE_DESCRIPTOR,u),PULONG);
	for(ULONG i = 0; i < sizeof(Desc->u) / sizeof(ULONG); i += 3)
		DbgPrint("        Data[%d] = %08x  %08x  %08x\n",i,Buffer[i],Buffer[i + 1],Buffer[i + 2]);
}

//
// debug print common config [checked]
//
VOID PciDebugDumpCommonConfig(__in PPCI_COMMON_HEADER Config)
{
	if(PciDebug < 7)
		return;

	PULONG Buffer										= reinterpret_cast<PULONG>(Config);

	for(ULONG i = 0; i < sizeof(PCI_COMMON_HEADER) / sizeof(ULONG); i ++)
		DbgPrint("  %02x - %08x\n",i * sizeof(ULONG),Buffer[i]);
}

//
// dump device capabilites [checked]
//
VOID PciDebugDumpQueryCapabilities(__in PDEVICE_CAPABILITIES Capabilities)
{
	static PCHAR SystemPowerStateName[POWER_SYSTEM_MAXIMUM] =
	{
		"Unspecified",
		"Working",
		"Sleeping1",
		"Sleeping2",
		"Sleeping3",
		"Hibernate",
		"Shutdown",
	};

	static PCHAR DevicePowerStateName[PowerDeviceMaximum] =
	{
		"Unspecified",
		"D0",
		"D1",
		"D2",
		"D3",
	};

	DbgPrint("Capabilities\n  Lock:%d, Eject:%d, Remove:%d, Dock:%d, UniqueId:%d\n",
			 Capabilities->LockSupported,Capabilities->EjectSupported,Capabilities->Removable,Capabilities->DockDevice,Capabilities->UniqueID);

	DbgPrint("  SilentInstall:%d, RawOk:%d, SurpriseOk:%d\n",
		     Capabilities->SilentInstall,Capabilities->RawDeviceOK,Capabilities->SurpriseRemovalOK);

	DbgPrint("  Address %08x, UINumber %08x, Latencies D1 %d, D2 %d, D3 %d\n",
			 Capabilities->Address,Capabilities->UINumber,Capabilities->D1Latency,Capabilities->D2Latency,Capabilities->D3Latency);

	DbgPrint("    System Wake: %s, Device Wake: %s\n  DeviceState[PowerState] [",
			 SystemPowerStateName[Capabilities->SystemWake > PowerSystemMaximum ? PowerSystemMaximum : Capabilities->SystemWake],
			 DevicePowerStateName[Capabilities->DeviceWake > PowerDeviceMaximum ? PowerDeviceMaximum : Capabilities->DeviceWake]);

	for(ULONG i = 1; i < POWER_SYSTEM_MAXIMUM; i ++)
		DbgPrint(" %s",DevicePowerStateName[Capabilities->DeviceState[i] > PowerDeviceMaximum ? PowerDeviceMaximum : Capabilities->DeviceState[i]]);

	DbgPrint(" ]\n");
}
