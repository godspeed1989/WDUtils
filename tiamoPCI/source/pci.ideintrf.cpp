//********************************************************************
//	created:	26:7:2008   23:23
//	file:		pci.ideintrf.cpp
//	author:		tiamo
//	purpose:	native ide interface
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",nativeIde_Constructor)
#pragma alloc_text("PAGE",nativeIde_Initializer)
#pragma alloc_text("PAGE",nativeIde_Reference)
#pragma alloc_text("PAGE",nativeIde_Dereference)
#pragma alloc_text("PAGE",nativeIde_InterruptControl)

//
// constructor [checked]
//
NTSTATUS nativeIde_Constructor(__in PPCI_COMMON_EXTENSION CommonExt,__in PPCI_INTERFACE PciInterface,
							   __in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface)
{
	PAGED_CODE();

	PPCI_PDO_EXTENSION PdoExt							= CONTAINING_RECORD(CommonExt,PCI_PDO_EXTENSION,Common);
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	if(PdoExt->BaseClass != PCI_CLASS_MASS_STORAGE_CTLR || PdoExt->SubClass != PCI_SUBCLASS_MSC_IDE_CTLR || !IdeModeSwitchable(PdoExt->ProgIf))
		return STATUS_INVALID_DEVICE_REQUEST;

	Interface->Version									= PCI_NATIVE_IDE_INTRF_STANDARD_VER;
	Interface->Context									= CommonExt;
	Interface->Size										= sizeof(NATIVEIDE_INTERFACE);
	Interface->InterfaceDereference						= reinterpret_cast<PINTERFACE_DEREFERENCE>(&nativeIde_Dereference);
	Interface->InterfaceReference						= reinterpret_cast<PINTERFACE_REFERENCE>(&nativeIde_Reference);

	PNATIVEIDE_INTERFACE NativeIdeInterface				= reinterpret_cast<PNATIVEIDE_INTERFACE>(Interface);
	NativeIdeInterface->InterruptControl				= reinterpret_cast<PNATIVEIDE_INTERRUPT_CONTROL>(&nativeIde_InterruptControl);

	return STATUS_SUCCESS;
}

//
// initializer [checked]
//
NTSTATUS nativeIde_Initializer(__in PPCI_ARBITER_INSTANCE Instance)
{
	PAGED_CODE();

	ASSERTMSG("PCI nativeide_Initializer, unexpected call.",FALSE);

	return STATUS_UNSUCCESSFUL;
}

//
// reference [checked]
//
VOID nativeIde_Reference(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();
}

//
// dereference [checked]
//
VOID nativeIde_Dereference(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();
}

//
// interrupt control [checked]
//
VOID nativeIde_InterruptControl(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN Enable)
{
	PdoExt->IoSpaceUnderNativeIdeControl				= TRUE;
	USHORT Command										= 0;

	PciReadDeviceConfig(PdoExt,&Command,FIELD_OFFSET(PCI_COMMON_CONFIG,Command),sizeof(Command));

	if(Enable)
	{
		SetFlag(Command,PCI_ENABLE_IO_SPACE);
		SetFlag(PdoExt->CommandEnables,PCI_ENABLE_IO_SPACE);
	}
	else
	{
		ClearFlag(Command,PCI_ENABLE_IO_SPACE);
		ClearFlag(PdoExt->CommandEnables,PCI_ENABLE_IO_SPACE);
	}

	PciWriteDeviceConfig(PdoExt,&Command,FIELD_OFFSET(PCI_COMMON_CONFIG,Command),sizeof(Command));
}