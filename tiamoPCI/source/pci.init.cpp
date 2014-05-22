//********************************************************************
//	created:	22:7:2008   12:07
//	file:		pci.init.cpp
//	author:		tiamo
//	purpose:	init
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("INIT",DriverEntry)
#pragma alloc_text("INIT",PciGetIrqRoutingTableFromRegistry)
#pragma alloc_text("INIT",PciGetDebugPorts)
#pragma alloc_text("PAGE",PciDriverUnload)

//
// driver entry [checked]
//
NTSTATUS DriverEntry(__in PDRIVER_OBJECT DriverObject,__in PUNICODE_STRING RegPath)
{
	PAGED_CODE();

	//
	// setup driver object
	//
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]	= &PciDispatchIrp;
	DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]	= &PciDispatchIrp;
	DriverObject->MajorFunction[IRP_MJ_POWER]			= &PciDispatchIrp;
	DriverObject->MajorFunction[IRP_MJ_PNP]				= &PciDispatchIrp;
	DriverObject->DriverUnload							= &PciDriverUnload;
	DriverObject->DriverExtension->AddDevice			= &PciAddDevice;

	//
	// initialize global data
	//
	PciDriverObject										= DriverObject;
	PciFdoExtensionListHead.Next						= 0;
	PciRootBusCount										= 0;
	PciLockDeviceResources								= FALSE;
	PciSystemWideHackFlags								= 0;
	PciEnableNativeModeATA								= FALSE;

	KeInitializeEvent(&PciGlobalLock,SynchronizationEvent,TRUE);
	KeInitializeEvent(&PciBusLock,SynchronizationEvent,TRUE);

	NTSTATUS Status										= STATUS_SUCCESS;
	HANDLE ServiceKeyHandle								= 0;
	HANDLE ParamtersKeyHandle							= 0;
	HANDLE DebugKeyHandle								= 0;
	HANDLE CCSKeyHandle									= 0;

	PVOID SystemStartOptions							= 0;
	PVOID PCILock										= 0;
	PVOID HackFlags										= 0;
	PVOID EnableNativeModeATA							= 0;

	__try
	{
		//
		// open service key
		//
		OBJECT_ATTRIBUTES ObjectAttribute;
		InitializeObjectAttributes(&ObjectAttribute,RegPath,OBJ_CASE_INSENSITIVE,0,0);

		Status											= ZwOpenKey(&ServiceKeyHandle,KEY_READ,&ObjectAttribute);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// open parameters key
		//
		if(!PciOpenKey(L"Parameters",ServiceKeyHandle,&ParamtersKeyHandle,&Status))
			try_leave(NOTHING);

		//
		// read all values in the parameters to build hack table
		//
		Status											= PciBuildHackTable(ParamtersKeyHandle);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// if there is a debug sub key,get debug ports from registry
		//
		if(PciOpenKey(L"Debug",ServiceKeyHandle,&DebugKeyHandle,&Status))
		{
			Status										= PciGetDebugPorts(DebugKeyHandle);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);
		}

		//
		// open current control set key
		//
		if(!PciOpenKey(L"\\Registry\\Machine\\System\\CurrentControlSet",0,&CCSKeyHandle,&Status))
			try_leave(NOTHING);

		//
		// read start options and check if we are asked to lock device resources
		//
		ULONG Length									= 0;
		Status											= PciGetRegistryValue(L"SystemStartOptions",L"Control",CCSKeyHandle,&SystemStartOptions,&Length);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		ASSERT(Length < 0x100);

		//
		// find PCILOCK in the start options
		//
		PWCHAR Options									= static_cast<PWCHAR>(SystemStartOptions);
		for(ULONG i = 0; i < Length - sizeof(L"PCILOCK") - sizeof(WCHAR); i ++)
		{
			if(RtlCompareMemory(Add2Ptr(SystemStartOptions,i,PVOID),L"PCILOCK",sizeof(L"PCILOCK") - sizeof(WCHAR)) == sizeof(L"PCILOCK") - sizeof(WCHAR))
			{
				PciLockDeviceResources					= TRUE;
				break;
			}
		}

		//
		// we need check registry also
		//
		if(!PciLockDeviceResources)
		{
			if(NT_SUCCESS(PciGetRegistryValue(L"PCILock",L"Control\\Biosinfo\\PCI",CCSKeyHandle,&PCILock,&Length)) && Length == sizeof(ULONG))
				PciLockDeviceResources					= *static_cast<PULONG>(PCILock) == TRUE;
		}

		//
		// read global hack flags
		//
		if(NT_SUCCESS(PciGetRegistryValue(L"HackFlags",L"Control\\PnP\\PCI",CCSKeyHandle,&HackFlags,&Length)) && Length == sizeof(ULONG))
			PciSystemWideHackFlags						= *static_cast<PULONG>(HackFlags);

		//
		// read native ata flags
		//
		if(NT_SUCCESS(PciGetRegistryValue(L"EnableNativeModeATA",L"Control\\PnP\\PCI",CCSKeyHandle,&EnableNativeModeATA,&Length)) && Length == sizeof(ULONG))
			PciEnableNativeModeATA						= *static_cast<PULONG>(EnableNativeModeATA);
		
		//
		// build default exclusion list
		//
		Status											= PciBuildDefaultExclusionLists();
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// read irq routing table
		//
		PciGetIrqRoutingTableFromRegistry(&PciIrqRoutingTable);

		//
		// hook hal
		//
		PciHookHal();

		//
		// initialize verifier
		//
		PciVerifierInit(DriverObject);

		Status											= STATUS_SUCCESS;
	}
	__finally
	{
		if(PCILock)
			ExFreePool(PCILock);

		if(HackFlags)
			ExFreePool(HackFlags);

		if(EnableNativeModeATA)
			ExFreePool(EnableNativeModeATA);

		if(CCSKeyHandle)
			ZwClose(CCSKeyHandle);

		if(DebugKeyHandle)
			ZwClose(DebugKeyHandle);

		if(ParamtersKeyHandle)
			ZwClose(ParamtersKeyHandle);

		if(ServiceKeyHandle)
			ZwClose(ServiceKeyHandle);
	}

	return Status;
}

//
// build hack table [checked]
//
NTSTATUS PciBuildHackTable(__in HANDLE KeyHandle)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;
	PKEY_FULL_INFORMATION KeyFullInfo					= 0;
	PKEY_VALUE_FULL_INFORMATION ValueFullInfo			= 0;
	PPCI_HACK_TABLE_ENTRY Entry							= 0;

	__try
	{
		//
		// get full info buffer length
		//
		ULONG Length									= 0;
		Status											= ZwQueryKey(KeyHandle,KeyFullInformation,0,0,&Length);
		if(Status != STATUS_BUFFER_TOO_SMALL && !NT_SUCCESS(Status))
			try_leave(NOTHING);

		ASSERT(Length > 0);

		//
		// allocate full info buffer
		//
		KeyFullInfo										= static_cast<PKEY_FULL_INFORMATION>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
		if(!KeyFullInfo)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// query it again
		//
		Status											= ZwQueryKey(KeyHandle,KeyFullInformation,KeyFullInfo,Length,&Length);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// allocate hack table
		//
		Length											= sizeof(PCI_HACK_TABLE_ENTRY) * (KeyFullInfo->Values + 1);
		PciHackTable									= static_cast<PPCI_HACK_TABLE_ENTRY>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
		if(!PciHackTable)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// zero it out
		//
		RtlZeroMemory(PciHackTable,Length);

		//
		// allocate a key value info buffer,with max 36 bytes name + 8 bytes data
		//
		Length											= sizeof(KEY_VALUE_FULL_INFORMATION) + sizeof(WCHAR) * 18 + sizeof(ULONGLONG);
		ValueFullInfo									= static_cast<PKEY_VALUE_FULL_INFORMATION>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
		if(!ValueFullInfo)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		Entry											= PciHackTable;
		for(ULONG Index = 0; Index < KeyFullInfo->Values; Index ++)
		{
			//
			// enumeate values under this key
			//
			ULONG Dummy									= 0;
			Status										= ZwEnumerateValueKey(KeyHandle,Index,KeyValueFullInformation,ValueFullInfo,Length,&Dummy);

			//
			// failed with those value other than buffer to small or buffer overflow
			//
			if(!NT_SUCCESS(Status) && Status != STATUS_BUFFER_TOO_SMALL && Status != STATUS_BUFFER_OVERFLOW)
				try_leave(Status);

			//
			// if we got a Status equals buffer_to_small or buffer_overflow,
			// there may be an error value entry in this key,we ignore those simply
			//
			if(NT_SUCCESS(Status))
			{
				//
				// value must have a type equals binary and length must be 8
				//
				if(ValueFullInfo->Type != REG_BINARY || ValueFullInfo->DataLength != sizeof(ULONGLONG))
					continue;

				//
				// name length should be 0x10,0x14,0x20,0x24
				//	vendor(%04x) + device(%04x)															= 0x10
				//	vendor(%04x) + device(%04x) + revision(%02x)										= 0x14
				//	vendor(%04x) + device(%04x) + subvendor(%04x) + subsystem(%04x)						= 0x20
				//	vendor(%04x) + device(%04x) + subvendor(%04x) + subsystem(%04x) + revision(%02x)	= 0x24
				//
				if(ValueFullInfo->NameLength != 0x10 && ValueFullInfo->NameLength != 0x14 && ValueFullInfo->NameLength != 0x20 && ValueFullInfo->NameLength != 0x24)
					continue;

				//
				// read vendor id
				//
				if(!PciStringToUSHORT(ValueFullInfo->Name,&Entry->VendorId))
					continue;

				//
				// read device id
				//
				if(!PciStringToUSHORT(ValueFullInfo->Name + 4,&Entry->DeviceId))
					continue;

				if(ValueFullInfo->NameLength == 0x20 || ValueFullInfo->NameLength == 0x24)
				{
					//
					// read sub vendor id
					//
					if(!PciStringToUSHORT(ValueFullInfo->Name + 8,&Entry->SubVendorId))
						continue;

					//
					// read sub system id
					//
					if(!PciStringToUSHORT(ValueFullInfo->Name + 0x0c,&Entry->SubSystemId))
						continue;

					//
					// sub system and sub vendor id is valid
					//
					Entry->SubSystemVendorIdValid		= TRUE;
				}

				if(ValueFullInfo->NameLength == 0x14 || ValueFullInfo->NameLength == 0x24)
				{
					//
					// read revision
					//
					USHORT Temp;
					if(!PciStringToUSHORT(ValueFullInfo->Name + ValueFullInfo->NameLength / sizeof(WCHAR) - 0x04,&Temp))
						continue;

					Entry->RevisionId					= static_cast<UCHAR>(Temp & 0xff);
					Entry->RevisionIdValid				= TRUE;
				}

				//
				// copy data
				//
				RtlCopyMemory(&Entry->HackFlags,Add2Ptr(ValueFullInfo,ValueFullInfo->DataOffset,PVOID),sizeof(ULONGLONG));

				//
				// must have a valid vendor id
				//
				ASSERT(Entry->VendorId != PCI_INVALID_VENDORID);

				PciDebugPrintf(1,"Adding Hack entry for Vendor:0x%04x Device:0x%04x ",Entry->VendorId,Entry->DeviceId);

				if(Entry->SubSystemVendorIdValid)
					PciDebugPrintf(1,"SybSys:0x%04x SubVendor:0x%04x ",Entry->SubSystemId,Entry->SubVendorId);

				if(Entry->RevisionIdValid)
					PciDebugPrintf(1,"Revision:0x%02x",Entry->RevisionId);

				PciDebugPrintf(1," = 0x%I64x\n",Entry->HackFlags);

				Entry									+= 1;
			}
			else
			{
				ASSERT(Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL);
				Status									= STATUS_SUCCESS;
			}
		}

		//
		// end of list
		//
		if(PciHackTable)
		{
			ASSERT(Entry < PciHackTable + KeyFullInfo->Values + 1);
			Entry->VendorId								= PCI_INVALID_VENDORID;
		}
	}
	__finally
	{
		if(AbnormalTermination() || !NT_SUCCESS(Status))
		{
			if(PciHackTable)
				ExFreePool(PciHackTable);

			PciHackTable								= 0;
		}

		if(KeyFullInfo)
			ExFreePool(KeyFullInfo);

		if(ValueFullInfo)
			ExFreePool(ValueFullInfo);
	}

	return Status;
}

//
// load irq routing table [checked]
//
NTSTATUS PciGetIrqRoutingTableFromRegistry(__out PPCI_IRQ_ROUTING_TABLE_HEAD* Table)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;
	HANDLE KeyHandle									= 0;
	HANDLE SubKeyHandle									= 0;
	PKEY_FULL_INFORMATION KeyFullInfo					= 0;
	PKEY_BASIC_INFORMATION KeyBasicInfo					= 0;
	PKEY_VALUE_PARTIAL_INFORMATION ValuePartialInfo		= 0;
	PVOID ConfigData									= 0;

	UNICODE_STRING NameString;
	RtlInitUnicodeString(&NameString,L"Identifier");

	__try
	{
		//
		// open key
		//
		if(!PciOpenKey(L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\MultiFunctionAdapter",0,&KeyHandle,&Status))
			try_leave(NOTHING);

		//
		// get full info length
		//
		ULONG Length									= 0;
		Status											= ZwQueryKey(KeyHandle,KeyFullInformation,0,0,&Length);
		if(Status != STATUS_BUFFER_TOO_SMALL)
			try_leave(NOTHING);

		//
		// allocate full info buffer
		//
		KeyFullInfo										= static_cast<PKEY_FULL_INFORMATION>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
		if(!KeyFullInfo)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// query full info again
		//
		Status											= ZwQueryKey(KeyHandle,KeyFullInformation,KeyFullInfo,Length,&Length);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// allocate a basic info buffer
		//
		Length											= sizeof(KEY_BASIC_INFORMATION) + KeyFullInfo->MaxNameLen + sizeof(WCHAR);
		KeyBasicInfo									= static_cast<PKEY_BASIC_INFORMATION>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
		if(!KeyBasicInfo)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// allocate a partial info buffer
		//
		ULONG PartialLength								= sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(L"PCI BIOS");
		ValuePartialInfo								= static_cast<PKEY_VALUE_PARTIAL_INFORMATION>(PciAllocateColdPoolWithTag(PagedPool,PartialLength,'BicP'));
		if(!ValuePartialInfo)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		for(ULONG Index = 0; Index < KeyFullInfo->SubKeys; Index ++)
		{
			//
			// enumate sub keys
			//
			ULONG Dummy									= 0;
			Status										= ZwEnumerateKey(KeyHandle,Index,KeyBasicInformation,KeyBasicInfo,Length,&Dummy);
			if(!NT_SUCCESS(Status))
				try_leave(ASSERT(Status == STATUS_NO_MORE_ENTRIES));

			//
			// write a NULL
			//
			Dummy										= KeyBasicInfo->NameLength / sizeof(WCHAR);
			KeyBasicInfo->Name[Dummy]					= 0;

			//
			// open it
			//
			if(!PciOpenKey(KeyBasicInfo->Name,KeyHandle,&SubKeyHandle,&Status))
				try_leave(NOTHING);

			//
			// read Identifier under it
			//
			Status										= ZwQueryValueKey(SubKeyHandle,&NameString,KeyValuePartialInformation,ValuePartialInfo,PartialLength,&Dummy);
			if(NT_SUCCESS(Status))
			{
				//
				// check it is PCI BIOS
				//
				if(RtlCompareMemory(ValuePartialInfo->Data,L"PCI BIOS",ValuePartialInfo->DataLength) == ValuePartialInfo->DataLength)
				{
					//
					// read config data
					//
					Status								= PciGetRegistryValue(L"Configuration Data",L"RealModeIrqRoutingTable\\0",SubKeyHandle,&ConfigData,&Length);
					if(!NT_SUCCESS(Status))
						try_leave(NOTHING);

					//
					// empty?
					//
					if(!ConfigData)
						try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

					//
					// check total length
					//
					if(Length < sizeof(CM_FULL_RESOURCE_DESCRIPTOR) + sizeof(PCI_IRQ_ROUTING_TABLE_HEAD))
						try_leave(Status = STATUS_UNSUCCESSFUL);

					//
					// strip cm desc
					//
					Length								-= sizeof(CM_FULL_RESOURCE_DESCRIPTOR);

					//
					// check irq routing table length
					//
					PPCI_IRQ_ROUTING_TABLE_HEAD Head	= Add2Ptr(ConfigData,sizeof(CM_FULL_RESOURCE_DESCRIPTOR),PPCI_IRQ_ROUTING_TABLE_HEAD);
					if(Head->Size > Length)
						try_leave(Status = STATUS_UNSUCCESSFUL);

					//
					// make a copy of it
					//
					PVOID Buffer						= PciAllocateColdPoolWithTag(PagedPool,Length,'BicP');
					if(!Buffer)
						try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

					RtlCopyMemory(Buffer,Head,Length);

					*Table								= static_cast<PPCI_IRQ_ROUTING_TABLE_HEAD>(Buffer);
					try_leave(Status = STATUS_SUCCESS);
				}
			}
		
			//
			// close it
			//
			ZwClose(SubKeyHandle);
			SubKeyHandle								= 0;

			//
			// break
			//
			if(Status == STATUS_NO_MORE_ENTRIES)
				try_leave(NOTHING);
		}
	}
	__finally
	{
		if(SubKeyHandle)
			ZwClose(SubKeyHandle);

		if(KeyHandle)
			ZwClose(KeyHandle);

		if(ValuePartialInfo)
			ExFreePool(ValuePartialInfo);

		if(KeyFullInfo)
			ExFreePool(KeyFullInfo);

		if(KeyBasicInfo)
			ExFreePool(KeyBasicInfo);

		if(ConfigData)
			ExFreePool(ConfigData);
	}

	return Status;
}

//
// get debug port [checked]
//
NTSTATUS PciGetDebugPorts(__in HANDLE KeyHandle)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;

	for(ULONG i = 0; i < ARRAYSIZE(PciDebugPorts); i ++)
	{
		PVOID Bus										= 0;
		PVOID Slot										= 0;
		WCHAR KeyName[3]								= {0};
		ULONG Length									= 0;
		RtlStringCchPrintfW(KeyName,ARRAYSIZE(KeyName),L"%d",i);

		__try
		{
			//
			// read bus
			//
			Status										= PciGetRegistryValue(L"Bus",KeyName,KeyHandle,&Bus,&Length);
			if(!NT_SUCCESS(Status) || Length != sizeof(ULONG))
				try_leave(Status = STATUS_SUCCESS);

			ULONG SegmentBus							= *static_cast<PULONG>(Bus);

			//
			// read slot
			//
			Status										= PciGetRegistryValue(L"Slot",KeyName,KeyHandle,&Slot,&Length);
			if(!NT_SUCCESS(Status) || Length != sizeof(ULONG))
				try_leave(i = ARRAYSIZE(PciDebugPorts));

			PCI_SLOT_NUMBER SlotNumber;
			SlotNumber.u.AsULONG						= *static_cast<PULONG>(Slot);
			SlotNumber.u.bits.Reserved					= 0;

			PciDebugPrintf(1,"Debug device @ Segment %x, %x.%x.%x\n",SegmentBus >> 8,SegmentBus & 0xff,SlotNumber.u.bits.DeviceNumber,SlotNumber.u.bits.FunctionNumber);
			ASSERT((SegmentBus >> 8) == 0);

			PciDebugPorts[PciDebugPortsCount].Bus		= SegmentBus & 0xff;
			PciDebugPorts[PciDebugPortsCount].SlotNumber= SlotNumber;
			PciDebugPortsCount							+= 1;
		}
		__finally
		{
			if(Bus)
				ExFreePool(Bus);

			if(Slot)
				ExFreePool(Slot);
		}
	}

	return Status;
}

//
// unload [checked]
//
VOID PciDriverUnload(__in PDRIVER_OBJECT DriverObject)
{
	PAGED_CODE();

	//
	// verifier unload
	//
	PciVerifierUnload(DriverObject);

	//
	// free exclusive lists
	//
	RtlFreeRangeList(&PciIsaBitExclusionList);
	RtlFreeRangeList(&PciVgaAndIsaBitExclusionList);

	//
	// free irq routing table
	//
	if(PciIrqRoutingTable)
		ExFreePool(PciIrqRoutingTable);

	//
	// free hack table
	//
	if(PciHackTable)
		ExFreePool(PciHackTable);

	//
	// unhook hal
	//
	PciUnhookHal();
}