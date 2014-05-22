//********************************************************************
//	created:	22:7:2008   22:13
//	file:		pci.config.cpp
//	author:		tiamo
//	purpose:	config
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciQueryForPciBusInterface)

//
// get config handlers [checked]
//
NTSTATUS PciGetConfigHandlers(__in PPCI_FDO_EXTENSION FdoExt)
{
	ASSERT(!FdoExt->BusHandler);

	NTSTATUS Status										= STATUS_SUCCESS;

	if(FdoExt->BusRootFdoExtension == FdoExt)
	{
		//
		// this is root bus fdo
		//
		ASSERT(!FdoExt->PciBusInterface);

		//
		// query pci bus interface
		//
		Status											= PciQueryForPciBusInterface(FdoExt);
		if(NT_SUCCESS(Status))
		{
			//
			// acpi gave us a pci bus interfer,use it
			//
			PciAssignBusNumbers							= TRUE;
			return Status;
		}

		//
		// we need to call hal to get a bus handler
		//
		ASSERT(!PciAssignBusNumbers);
	}
	else
	{
		//
		// root bus has already got a pci bus interfer,done
		//
		if(FdoExt->BusRootFdoExtension->PciBusInterface)
			return STATUS_SUCCESS;

		Status											= STATUS_NOT_SUPPORTED;
	}

	ASSERT(Status == STATUS_NOT_SUPPORTED);

	ASSERT(!PciAssignBusNumbers);

	FdoExt->BusHandler									= HalReferenceHandlerForBus(PCIBus,FdoExt->BaseBus);

	return FdoExt->BusHandler ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

//
// query bus interface [checked]
//
NTSTATUS PciQueryForPciBusInterface(__in PPCI_FDO_EXTENSION FdoExt)
{
	PAGED_CODE();

	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType && FdoExt->BusRootFdoExtension == FdoExt);

	PPCI_BUS_INTERFACE_STANDARD Interface				= 0;
	PDEVICE_OBJECT DeviceObject							= 0;
	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		ULONG Length									= sizeof(PCI_BUS_INTERFACE_STANDARD);
		Interface										= static_cast<PPCI_BUS_INTERFACE_STANDARD>(ExAllocatePoolWithTag(NonPagedPool,Length,'BicP'));
		if(!Interface)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		DeviceObject									= IoGetAttachedDeviceReference(FdoExt->PhysicalDeviceObject);
		if(!DeviceObject)
			try_leave(Status = STATUS_NO_SUCH_DEVICE);

		KEVENT Event;
		KeInitializeEvent(&Event,SynchronizationEvent,FALSE);

		IO_STATUS_BLOCK IoStatus;
		PIRP Irp										= IoBuildSynchronousFsdRequest(IRP_MJ_PNP,DeviceObject,0,0,0,&Event,&IoStatus);
		if(!Irp)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		PIO_STACK_LOCATION IrpSp						= IoGetNextIrpStackLocation(Irp);
		IrpSp->MajorFunction							= IRP_MJ_PNP;
		IrpSp->MinorFunction							= IRP_MN_QUERY_INTERFACE;
		IrpSp->Parameters.QueryInterface.InterfaceType	= &GUID_PCI_BUS_INTERFACE_STANDARD;
		IrpSp->Parameters.QueryInterface.Interface		= reinterpret_cast<PINTERFACE>(Interface);
		IrpSp->Parameters.QueryInterface.Size			= static_cast<USHORT>(Length);
		IrpSp->Parameters.QueryInterface.Version		= PCI_BUS_INTERFACE_STANDARD_VERSION;
		IrpSp->Parameters.Others.Argument4				= 0;
		Irp->IoStatus.Status							= STATUS_NOT_SUPPORTED;
		Irp->IoStatus.Information						= 0;

		Status											= IoCallDriver(DeviceObject,Irp);
		if(Status == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,0);
			Status										= IoStatus.Status;
		}

		if(NT_SUCCESS(Status))
		{
			FdoExt->PciBusInterface						= Interface;
			Interface									= 0;
		}
	}
	__finally
	{
		if(Interface)
			ExFreePool(Interface);

		if(DeviceObject)
			ObDereferenceObject(DeviceObject);
	}

	return Status;
}

//
// read device config [checked]
//
ULONG PciReadDeviceConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	return PciReadWriteConfigSpace(PdoExt->ParentFdoExtension,PdoExt->Slot,Buffer,Offset,Length,TRUE);
}

//
// write device config [checked]
//
ULONG PciWriteDeviceConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	return PciReadWriteConfigSpace(PdoExt->ParentFdoExtension,PdoExt->Slot,Buffer,Offset,Length,FALSE);
}

//
// read slot config [checked]
//
ULONG PciReadSlotConfig(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SLOT_NUMBER Slot,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	return PciReadWriteConfigSpace(FdoExt,Slot,Buffer,Offset,Length,TRUE);
}

//
// write device config [checked]
//
ULONG PciWriteSlotConfig(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SLOT_NUMBER Slot,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	return PciReadWriteConfigSpace(FdoExt,Slot,Buffer,Offset,Length,FALSE);
}

//
// read write config space [checked]
//
ULONG PciReadWriteConfigSpace(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SLOT_NUMBER Slot,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length,__in BOOLEAN Read)
{
	PPCI_FDO_EXTENSION RootFdoExt						= FdoExt->BusRootFdoExtension;
	ASSERT(RootFdoExt == RootFdoExt->BusRootFdoExtension);

	if(RootFdoExt->PciBusInterface)
	{
		ULONG ResultLength								= 0;
		PCI_READ_WRITE_CONFIG Function					= Read ? RootFdoExt->PciBusInterface->ReadConfig : RootFdoExt->PciBusInterface->WriteConfig;
	
		ResultLength									= Function(RootFdoExt->PciBusInterface->Context,FdoExt->BaseBus,Slot.u.AsULONG,Buffer,Offset,Length);
		if(ResultLength != Length)
		{
			ASSERT(ResultLength == Length);

			KeBugCheckEx(0xc0,FdoExt->BaseBus,Slot.u.AsULONG,Offset,Read);
		}

		return ResultLength;
	}

	ASSERT(RootFdoExt->BusHandler);

	ASSERT(!PciAssignBusNumbers);

	PPCIBUSDATA BusData									= static_cast<PPCIBUSDATA>(FdoExt->BusHandler->BusData);

	PciReadWriteConfig Function							= Read ? BusData->ReadConfig : BusData->WriteConfig;

	Function(RootFdoExt->BusHandler,Slot,Buffer,Offset,Length);

	return Length;
}

//
// external read device config
//
NTSTATUS PciExternalReadDeviceConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	PCI_COMMON_CONFIG LocalConfig;

	if(Offset + Length > sizeof(LocalConfig))
		return STATUS_INVALID_DEVICE_REQUEST;

	PciReadDeviceConfig(PdoExt,Add2Ptr(&LocalConfig,Offset,PVOID),Offset,Length);

	ULONG InterruptLineOffset							= FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.InterruptLine);
	if(PdoExt->InterruptPin && Offset <= InterruptLineOffset && Offset + Length > InterruptLineOffset)
		LocalConfig.u.type0.InterruptLine				= PdoExt->AdjustedInterruptLine;

	RtlCopyMemory(Buffer,Add2Ptr(&LocalConfig,Offset,PVOID),Length);

	return STATUS_SUCCESS;
}

//
// external write device config [checked]
//
NTSTATUS PciExternalWriteDeviceConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length)
{
	PCI_COMMON_CONFIG LocalConfig;

	if(Offset + Length > sizeof(PCI_COMMON_CONFIG))
		return STATUS_INVALID_DEVICE_REQUEST;

	NTSTATUS Status										= STATUS_SUCCESS;

	#define CONTAIN_RANGE(s1,e1,s2,e2)					(((s1) >= (s2)) ? ((s1) < (e2)) : ((e1) > (s2)))

	switch(PdoExt->HeaderType)
	{
	case PCI_DEVICE_TYPE:
		{
			//
			// attempt to write bar
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.BaseAddresses),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.CIS)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;

			//
			// attempt to write rom address
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.ROMBaseAddress),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.CapabilitiesPtr)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;
		}
		break;

	case PCI_BRIDGE_TYPE:
		{
			//
			// attempt to write bar and bus number
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type1.BaseAddresses),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type1.SecondaryLatency)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;

			//
			// attempt to write io base
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type1.IOBase),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type1.SecondaryStatus)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;

			//
			// attemp to write memory
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type1.MemoryBase),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type1.CapabilitiesPtr)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;

			//
			// attemp to write rom address
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type1.ROMBaseAddress),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type1.InterruptLine)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;
		}
		break;

	case PCI_CARDBUS_BRIDGE_TYPE:
		{
			//
			// attempt to write socket address
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type2),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type2.CapabilitiesPtr)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;

			//
			// attempt to write bus number
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type2.PrimaryBus),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type2.SecondaryStatus)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;

			//
			// attemp to write range
			//
			if(CONTAIN_RANGE(Offset,Offset + Length,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type2.Range),FIELD_OFFSET(PCI_COMMON_CONFIG,u.type2.InterruptLine)))
				Status									= STATUS_INVALID_DEVICE_REQUEST;
		}
		break;

	default:
		break;
	}

	#undef CONTAIN_RANGE

	if(!NT_SUCCESS(Status))
	{
		PDEVICE_OBJECT Pdo								= PdoExt->PhysicalDeviceObject;
		PVERIFIER_FAILURE_DATA Data						= PciVerifierRetrieveFailureData(3);
		ASSERT(Data);
		VfFailDeviceNode(Pdo,0xf6,3,Data->Offset4,&Data->Offset8,Data->FailureMessage,"%DevObj%Ulong%Ulong",Pdo,Offset,Length);
	}

	RtlCopyMemory(Add2Ptr(&LocalConfig,Offset,PVOID),Buffer,Length);

	ULONG InterruptLineOffset							= FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.InterruptLine);
	if(PdoExt->InterruptPin && Offset <= InterruptLineOffset && Offset + Length > InterruptLineOffset)
		LocalConfig.u.type0.InterruptLine				= PdoExt->AdjustedInterruptLine;

	PciWriteDeviceConfig(PdoExt,Add2Ptr(&LocalConfig,Offset,PVOID),Offset,Length);

	return STATUS_SUCCESS;
}

//
// get adjusted interrupt line [checked]
//
UCHAR PciGetAdjustedInterruptLine(__in PPCI_PDO_EXTENSION PdoExt)
{
	if(!PdoExt->InterruptPin)
		return 0;

	UCHAR Line											= 0;
	ULONG Offset										= FIELD_OFFSET(PCI_COMMON_HEADER,u.type0.InterruptLine);
	ULONG Length										= sizeof(Line);

	if(!HalGetBusDataByOffset(PCIConfiguration,PdoExt->ParentFdoExtension->BaseBus,PdoExt->Slot.u.AsULONG,&Line,Offset,Length))
		Line											= PdoExt->RawInterruptLine;

	return Line;
}