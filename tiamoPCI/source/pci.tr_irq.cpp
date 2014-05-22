//********************************************************************
//	created:	27:7:2008   0:31
//	file:		pci.tr_irq.cpp
//	author:		tiamo
//	purpose:	tranlate irq
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",tranirq_Constructor)
#pragma alloc_text("PAGE",tranirq_Initializer)

//
// constructor [checked]
//
NTSTATUS tranirq_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
							 __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	//
	// data should be interrupt resource type
	//
	if(reinterpret_cast<ULONG>(Data) != CmResourceTypeInterrupt)
		return STATUS_INVALID_PARAMETER_3;

	//
	// this is an fdo extension
	//
	PPCI_FDO_EXTENSION FdoExt							= CONTAINING_RECORD(CommonExt,PCI_FDO_EXTENSION,Common);
	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	ULONG ParentBusNumber								= 0;
	INTERFACE_TYPE ParentBusType						= Internal;
	ULONG BridgeBusNumber								= FdoExt->BaseBus;

	if(FdoExt->BusRootFdoExtension == FdoExt)
	{
		//
		// root fdo
		//	interface type		= internal
		//	parent bus number	= 0
		//
		PciDebugPrintf(0x7fffffff,"      Is root FDO\n");
	}
	else
	{
		//
		// pci bridge fdo
		//	interface type		= pci
		//	parent bus number	= parentfdo->base
		//
		ParentBusType									= PCIBus;
		ParentBusNumber									= FdoExt->ParentFdoExtension->BaseBus;
		PciDebugPrintf(0x7fffffff,"      Is bridge FDO, parent bus %x, secondary bus %x\n",ParentBusNumber,BridgeBusNumber);
	}

	//
	// call hal
	//
	PTRANSLATOR_INTERFACE TranInterface					= reinterpret_cast<PTRANSLATOR_INTERFACE>(Interface);
	return HalGetInterruptTranslator(ParentBusType,ParentBusNumber,PCIBus,sizeof(TRANSLATOR_INTERFACE),0,TranInterface,&BridgeBusNumber);
}

//
// initializer [checked]
//
NTSTATUS tranirq_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	return STATUS_SUCCESS;
}