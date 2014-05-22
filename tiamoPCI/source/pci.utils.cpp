//********************************************************************
//	created:	22:7:2008   21:18
//	file:		pci.utils.cpp
//	author:		tiamo
//	purpose:	utils
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciOpenKey)
#pragma alloc_text("PAGE",PciGetRegistryValue)
#pragma alloc_text("PAGE",PciGetDeviceProperty)
#pragma alloc_text("PAGE",PciGetBiosConfig)
#pragma alloc_text("PAGE",PciSaveBiosConfig)
#pragma alloc_text("INIT",PciBuildDefaultExclusionLists)
#pragma alloc_text("PAGE",PciRangeListFromResourceList)
#pragma alloc_text("PAGE",PcipInitializePartialListContext)
#pragma alloc_text("PAGE",PcipGetNextRangeFromList)
#pragma alloc_text("PAGE",PciFindDescriptorInCmResourceList)
#pragma alloc_text("PAGE",PciInsertEntryAtHead)
#pragma alloc_text("PAGE",PciInsertEntryAtTail)
#pragma alloc_text("PAGE",PcipLinkSecondaryExtension)
#pragma alloc_text("PAGE",PcipDestroySecondaryExtension)
#pragma alloc_text("PAGE",PciStringToUSHORT)
#pragma alloc_text("PAGE",PciSendIoctl)
#pragma alloc_text("PAGE",PciIsDeviceOnDebugPath)
#pragma alloc_text("PAGE",PciIsSlotPresentInParentMethod)
#pragma alloc_text("PAGE",PciQueryInterface)
#pragma alloc_text("PAGE",PciQueryLegacyBusInformation)
#pragma alloc_text("PAGE",PciQueryBusInformation)
#pragma alloc_text("PAGE",PciQueryCapabilities)
#pragma alloc_text("PAGE",PciGetDeviceCapabilities)

//
// open key [checked]
//
BOOLEAN PciOpenKey(__in PWCH SubKeyName,__in HANDLE KeyHandle,__out HANDLE* SubKeyHandle,__out NTSTATUS* Status)
{
	PAGED_CODE();

	OBJECT_ATTRIBUTES ObjectAttribute;
	UNICODE_STRING SubKeyString;

	RtlInitUnicodeString(&SubKeyString,SubKeyName);

	InitializeObjectAttributes(&ObjectAttribute,&SubKeyString,OBJ_CASE_INSENSITIVE,KeyHandle,0);

	NTSTATUS LocalStatus								= ZwOpenKey(SubKeyHandle,KEY_READ,&ObjectAttribute);

	if(Status)
		*Status											= LocalStatus;

	return NT_SUCCESS(LocalStatus);
}

//
// get registry value [checked]
//
NTSTATUS PciGetRegistryValue(__in PWCH Name,__in PWCH SubKeyName,__in HANDLE KeyHandle,__out PVOID* Data,__out PULONG Length)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_SUCCESS;
	HANDLE SubKeyHandle									= 0;
	PKEY_VALUE_PARTIAL_INFORMATION ValueInfo			= 0;

	__try
	{
		//
		// open sub key first
		//
		if(!PciOpenKey(SubKeyName,KeyHandle,&SubKeyHandle,&Status))
			try_leave(NOTHING);

		//
		// build unicode value name
		//
		UNICODE_STRING ValueName;
		RtlInitUnicodeString(&ValueName,Name);

		//
		// get length
		//
		ULONG InfoLength;
		Status											= ZwQueryValueKey(SubKeyHandle,&ValueName,KeyValuePartialInformation,0,0,&InfoLength);
		if(Status != STATUS_BUFFER_TOO_SMALL)
			try_leave(NOTHING);

		ASSERT(InfoLength);

		//
		// allocate value info
		//
		ValueInfo										= static_cast<PKEY_VALUE_PARTIAL_INFORMATION>(PciAllocateColdPoolWithTag(PagedPool,InfoLength,'BicP'));
		if(!ValueInfo)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// query it again
		//
		Status											= ZwQueryValueKey(SubKeyHandle,&ValueName,KeyValuePartialInformation,ValueInfo,InfoLength,&InfoLength);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// strip header
		//
		InfoLength										-= FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION,Data);

		//
		// allocate buffer
		//
		*Data											= PciAllocateColdPoolWithTag(PagedPool,InfoLength,'Bicp');
		if(!*Data)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// copy it
		//
		RtlCopyMemory(*Data,ValueInfo->Data,ValueInfo->DataLength);

		//
		// output buffer length
		//
		if(Length)
			*Length										= InfoLength;

		Status											= STATUS_SUCCESS;
	}
	__finally
	{
		if(SubKeyHandle)
			ZwClose(SubKeyHandle);

		if(ValueInfo)
			ExFreePool(ValueInfo);
	}

	return Status;
}

//
// get device property [checked]
//
NTSTATUS PciGetDeviceProperty(__in PDEVICE_OBJECT PhysicalDeviceObject,__in DEVICE_REGISTRY_PROPERTY Property,__out PVOID* Data)
{
	PAGED_CODE();

	//
	// get length
	//
	ULONG Length										= 0;
	NTSTATUS Status										= IoGetDeviceProperty(PhysicalDeviceObject,Property,0,0,&Length);
	if(Status != STATUS_BUFFER_TOO_SMALL)
	{
		PciDebugPrintf(0,"PCI - Unexpected status from GetDeviceProperty, saw %08X, expected %08X.\n",Status,STATUS_BUFFER_TOO_SMALL);

		if(Status == STATUS_SUCCESS)
		{
			ASSERTMSG("PCI Successfully did the impossible!",FALSE);
			Status										= STATUS_UNSUCCESSFUL;
		}

		return Status;
	}

	//
	// allocate buffer
	//
	PVOID Buffer										= PciAllocateColdPoolWithTag(PagedPool,Length,'BicP');
	if(!Buffer)
	{
		PciDebugPrintf(0,"PCI - Failed to allocate DeviceProperty buffer (%d bytes).\n",Length);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ULONG Length2;
	Status												= IoGetDeviceProperty(PhysicalDeviceObject,Property,Length,Buffer,&Length2);
	if(NT_SUCCESS(Status))
	{
		ASSERT(Length2 == Length);
		*Data											= Buffer;
	}

	return Status;
}

//
// get bios config [checked]
//
NTSTATUS PciGetBiosConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	PDEVICE_OBJECT Pdo									= PdoExt->ParentFdoExtension->PhysicalDeviceObject;
	HANDLE KeyHandle									= 0;
	HANDLE SubKeyHandle									= 0;
	NTSTATUS Status										= STATUS_SUCCESS;

	UCHAR Buffer[sizeof(PCI_COMMON_HEADER) + sizeof(KEY_VALUE_PARTIAL_INFORMATION)];

	__try
	{
		//
		// open hardware key
		//
		Status											= IoOpenDeviceRegistryKey(Pdo,PLUGPLAY_REGKEY_DEVICE,KEY_READ | KEY_WRITE,&KeyHandle);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// open bios config key
		//
		UNICODE_STRING SubKeyString;
		RtlInitUnicodeString(&SubKeyString,L"BiosConfig");

		OBJECT_ATTRIBUTES ObjectAttribute;
		InitializeObjectAttributes(&ObjectAttribute,&SubKeyString,OBJ_KERNEL_HANDLE,KeyHandle,0);

		Status											= ZwOpenKey(&SubKeyHandle,KEY_READ,&ObjectAttribute);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// read value
		//
		WCHAR ValueNameBuffer[0x1d]						= {0};
		RtlStringCchPrintfW(ValueNameBuffer,ARRAYSIZE(ValueNameBuffer) - 1,L"DEV_%02x&FUN_%02x",PdoExt->Slot.u.bits.DeviceNumber,PdoExt->Slot.u.bits.FunctionNumber);

		UNICODE_STRING ValueName;
		RtlInitUnicodeString(&ValueName,ValueNameBuffer);

		ULONG Length									= 0;
		Status											= ZwQueryValueKey(SubKeyHandle,&ValueName,KeyValuePartialInformation,Buffer,sizeof(Buffer),&Length);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		PKEY_VALUE_PARTIAL_INFORMATION Info				= reinterpret_cast<PKEY_VALUE_PARTIAL_INFORMATION>(Buffer);
		ASSERT(Info->DataLength == sizeof(PCI_COMMON_HEADER));

		RtlCopyMemory(Config,Info->Data,Info->DataLength);
	}
	__finally
	{
		if(KeyHandle)
			ZwClose(KeyHandle);

		if(SubKeyHandle)
			ZwClose(SubKeyHandle);
	}

	return Status;
}

//
// save bios config [checked]
//
NTSTATUS PciSaveBiosConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config)
{
	PAGED_CODE();

	PDEVICE_OBJECT Pdo									= PdoExt->ParentFdoExtension->PhysicalDeviceObject;
	HANDLE KeyHandle									= 0;
	HANDLE SubKeyHandle									= 0;
	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		//
		// open hardware key
		//
		Status											= IoOpenDeviceRegistryKey(Pdo,PLUGPLAY_REGKEY_DEVICE,KEY_ALL_ACCESS,&KeyHandle);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// open bios config key
		//
		UNICODE_STRING SubKeyString;
		RtlInitUnicodeString(&SubKeyString,L"BiosConfig");

		OBJECT_ATTRIBUTES ObjectAttribute;
		InitializeObjectAttributes(&ObjectAttribute,&SubKeyString,OBJ_KERNEL_HANDLE,KeyHandle,0);

		Status											= ZwCreateKey(&SubKeyHandle,KEY_ALL_ACCESS,&ObjectAttribute,0,0,REG_OPTION_VOLATILE,0);
		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		//
		// set it
		//
		WCHAR ValueNameBuffer[0x1d]						= {0};
		RtlStringCchPrintfW(ValueNameBuffer,ARRAYSIZE(ValueNameBuffer) - 1,L"DEV_%02x&FUN_%02x",PdoExt->Slot.u.bits.DeviceNumber,PdoExt->Slot.u.bits.FunctionNumber);

		UNICODE_STRING ValueName;
		RtlInitUnicodeString(&ValueName,ValueNameBuffer);

		ULONG Length									= 0;
		Status											= ZwSetValueKey(SubKeyHandle,&ValueName,0,REG_BINARY,Config,sizeof(PCI_COMMON_HEADER));
	}
	__finally
	{
		if(KeyHandle)
			ZwClose(KeyHandle);

		if(SubKeyHandle)
			ZwClose(SubKeyHandle);
	}

	return Status;
}

//
// build default exclusive list [checked]
//
NTSTATUS PciBuildDefaultExclusionLists()
{
	PAGED_CODE();

	ASSERT(PciIsaBitExclusionList.Count == 0);
	ASSERT(PciVgaAndIsaBitExclusionList.Count == 0);

	RtlInitializeRangeList(&PciIsaBitExclusionList);
	RtlInitializeRangeList(&PciVgaAndIsaBitExclusionList);

	NTSTATUS Status										= STATUS_SUCCESS;
	ULONG Start											= 0x100;

	__try
	{
		do
		{
			Status										= RtlAddRange(&PciIsaBitExclusionList,Start,Start + 0x2ff,0,RTL_RANGE_LIST_ADD_IF_CONFLICT,0,0);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			Status										= RtlAddRange(&PciVgaAndIsaBitExclusionList,Start,Start + 0x2af,0,RTL_RANGE_LIST_ADD_IF_CONFLICT,0,0);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			Status										= RtlAddRange(&PciVgaAndIsaBitExclusionList,Start + 0x2bc,Start + 0x2bf,0,RTL_RANGE_LIST_ADD_IF_CONFLICT,0,0);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			Status										= RtlAddRange(&PciVgaAndIsaBitExclusionList,Start + 0x2e0,Start + 0x2ff,0,RTL_RANGE_LIST_ADD_IF_CONFLICT,0,0);
			if(!NT_SUCCESS(Status))
				try_leave(NOTHING);

			Start										+= 0x400;
		}while(Start - 0x100 <= 0xffff);
	}
	__finally
	{
		if(AbnormalTermination() || !NT_SUCCESS(Status))
		{
			RtlFreeRangeList(&PciIsaBitExclusionList);
			RtlFreeRangeList(&PciVgaAndIsaBitExclusionList);
		}
	}

	return Status;
}

//
// find parent fdo extension [checked]
//
PPCI_FDO_EXTENSION PciFindParentPciFdoExtension(__in PDEVICE_OBJECT PhysicalDeviceObject,__in_opt PKEVENT Lock)
{
	//
	// acquire global lock first
	//
	if(Lock)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(Lock,Executive,KernelMode,FALSE,0);
	}

	PPCI_PDO_EXTENSION SearchPdoExt						= static_cast<PPCI_PDO_EXTENSION>(PhysicalDeviceObject->DeviceExtension);
	PPCI_FDO_EXTENSION RetFdoExt						= 0;

	//
	// for each fdo ext search those children
	//
	PPCI_FDO_EXTENSION FdoExt							= CONTAINING_RECORD(PciFdoExtensionListHead.Next,PCI_FDO_EXTENSION,Common.ListEntry);
	while(FdoExt && !RetFdoExt)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);

		PPCI_PDO_EXTENSION PdoExt						= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		while(PdoExt)
		{
			if(PdoExt == SearchPdoExt)
			{
				RetFdoExt								= FdoExt;
				break;
			}

			PdoExt										= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
		}

		KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();

		FdoExt											= CONTAINING_RECORD(FdoExt->Common.ListEntry.Next,PCI_FDO_EXTENSION,Common.ListEntry);
	}

	//
	// release global lock
	//
	if(Lock)
	{
		KeSetEvent(Lock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}

	return RetFdoExt;
}

//
// find pdo by function [checked]
//
PPCI_PDO_EXTENSION PciFindPdoByFunction(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SLOT_NUMBER Slot,__in PPCI_COMMON_HEADER Config)
{
	KIRQL SavedIrql										= KeGetCurrentIrql();

	if(SavedIrql < DISPATCH_LEVEL)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(&FdoExt->ChildListLock,Executive,KernelMode,FALSE,0);
	}

	PPCI_PDO_EXTENSION PdoExt							= CONTAINING_RECORD(FdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);
	PPCI_PDO_EXTENSION Found							= 0;
	while(PdoExt)
	{
		if(!PdoExt->ReportedMissing && PdoExt->Slot.u.bits.DeviceNumber == Slot.u.bits.DeviceNumber && PdoExt->Slot.u.bits.FunctionNumber == Slot.u.bits.FunctionNumber)
		{
			if(PdoExt->VendorId == Config->VendorID && PdoExt->DeviceId == Config->DeviceID && PdoExt->RevisionId == Config->RevisionID)
			{
				Found									= PdoExt;
				break;
			}
		}

		PdoExt											= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
	}

	if(SavedIrql < DISPATCH_LEVEL)
	{
		KeSetEvent(&FdoExt->ChildListLock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}

	return Found;
}

//
// invalid resource cache [checked]
//
VOID PciInvalidateResourceInfoCache(__in PPCI_PDO_EXTENSION PdoExt)
{
}

//
// build range list from resource list [checked]
//
NTSTATUS PciRangeListFromResourceList(__in PPCI_FDO_EXTENSION FdoExt,__in PCM_RESOURCE_LIST CmResList,__in UCHAR Type,__in BOOLEAN ArbRes,__in PRTL_RANGE_LIST RangeList)
{
	PAGED_CODE();

	ASSERT(Type == CmResourceTypePort || Type == CmResourceTypeMemory);

	BOOLEAN MemoryResourceForBus0						= Type == CmResourceTypeMemory && FdoExt && FdoExt->BaseBus == 0;

	//
	// count elements
	//
	ULONG Elements										= 0;

	if(CmResList)
	{
		PCM_FULL_RESOURCE_DESCRIPTOR FullDesc			= CmResList->List;
		for(ULONG i = 0; i < CmResList->Count; i ++)
		{
			PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDesc	= FullDesc->PartialResourceList.PartialDescriptors;

			for(ULONG j = 0; j < FullDesc->PartialResourceList.Count; j ++)
			{
				if(PartialDesc->Type == Type)
				{
					if(Type == CmResourceTypePort)
					{
						if(FlagOn(PartialDesc->Flags,CM_RESOURCE_PORT_10_BIT_DECODE))
							Elements					+= 0x3f;
						else if(FlagOn(PartialDesc->Flags,CM_RESOURCE_PORT_12_BIT_DECODE))
							Elements					+= 0x0f;
					}

					Elements							+= 1;
				}
				PartialDesc								= PciNextPartialDescriptor(PartialDesc);
			}

			FullDesc									= reinterpret_cast<PCM_FULL_RESOURCE_DESCRIPTOR>(PartialDesc);
		}
	}

	PciDebugPrintf(0x7fffffff,"PCI - PciRangeListFromResourceList processing %d elements.\n",Elements);

	//
	// 0x70,0x400-0x4ff,0xa0000-0xbffff
	//
	if(MemoryResourceForBus0)
		Elements										+= 3;

	//
	// start and end
	//
	Elements											+= 2;

	ULONG Length										= sizeof(PCI_RANGE_LIST_ENTRY) * Elements;
	PPCI_RANGE_LIST_ENTRY RangeListArray				= static_cast<PPCI_RANGE_LIST_ENTRY>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
	if(!RangeListArray)
		return STATUS_INSUFFICIENT_RESOURCES;

	PPCI_RANGE_LIST_ENTRY First							= RangeListArray;
	PPCI_RANGE_LIST_ENTRY Last							= RangeListArray + 1;

	First->Valid										= FALSE;
	First->Start										= 0xffffffffffffffff;
	First->End											= 0xffffffffffffffff;
	First->ListEntry.Blink								= &Last->ListEntry;
	First->ListEntry.Flink								= &Last->ListEntry;

	Last->Valid											= FALSE;
	Last->Start											= 0;
	Last->End											= 0;
	Last->ListEntry.Blink								= &First->ListEntry;
	Last->ListEntry.Flink								= &First->ListEntry;

	ULONG NextIndex										= 2;

	if(MemoryResourceForBus0)
	{
		PPCI_RANGE_LIST_ENTRY Memory70					= RangeListArray + 2;
		PPCI_RANGE_LIST_ENTRY Memory400					= RangeListArray + 3;
		PPCI_RANGE_LIST_ENTRY MemoryA0000				= RangeListArray + 4;
		NextIndex										= 5;

		Memory70->Valid									= TRUE;
		Memory70->Start									= 0x70;
		Memory70->End									= 0x70;
		Memory70->ListEntry.Blink						= &Last->ListEntry;
		Memory70->ListEntry.Flink						= &Memory400->ListEntry;

		Memory400->Valid								= TRUE;
		Memory400->Start								= 0x400;
		Memory400->End									= 0x4ff;
		Memory400->ListEntry.Blink						= &Memory70->ListEntry;
		Memory400->ListEntry.Flink						= &MemoryA0000->ListEntry;

		MemoryA0000->Valid								= TRUE;
		MemoryA0000->Start								= 0xa0000;
		MemoryA0000->End								= 0xbffff;
		MemoryA0000->ListEntry.Blink					= &Memory400->ListEntry;
		MemoryA0000->ListEntry.Flink					= &First->ListEntry;

		Last->ListEntry.Flink							= &Memory70->ListEntry;
		First->ListEntry.Blink							= &MemoryA0000->ListEntry;
	}

	PPCI_RANGE_LIST_ENTRY CurrentEntry					= Last;

	PciDebugPrintf(0x7fffffff,"    === PCI added default initial ranges ===\n");

	do
	{
		if(CurrentEntry->Valid)
			PciDebugPrintf(0x7fffffff,"    %I64x .. %I64x\n",CurrentEntry->Start,CurrentEntry->End);

		CurrentEntry									= CONTAINING_RECORD(CurrentEntry->ListEntry.Flink,PCI_RANGE_LIST_ENTRY,ListEntry);

	}while(CurrentEntry != Last);

	PciDebugPrintf(0x7fffffff,"    === end added default initial ranges ===\n");

	if(CmResList && CmResList->Count)
	{
		PCI_PARTIAL_LIST_CONTEXT Context;
		PCM_FULL_RESOURCE_DESCRIPTOR FullDesc			= CmResList->List;

		for(ULONG i = 0; i < CmResList->Count; i ++)
		{
			PcipInitializePartialListContext(&Context,&FullDesc->PartialResourceList,Type);
			PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDesc	= PcipGetNextRangeFromList(&Context);

			while(PartialDesc)
			{
				ASSERT(PartialDesc->Type == Type);

				ULONGLONG Start							= static_cast<ULONGLONG>(PartialDesc->u.Generic.Start.QuadPart);
				ULONGLONG End							= Start	+ PartialDesc->u.Generic.Length - 1;

				PPCI_RANGE_LIST_ENTRY Prev				= CurrentEntry;

				//
				// 2d037
				//
				while(Start > Prev->End)
					Prev								= CONTAINING_RECORD(Prev->ListEntry.Flink,PCI_RANGE_LIST_ENTRY,ListEntry);

				//
				// 2d052
				//
				while(Start <= Prev->End)
				{
					if(Start >= Prev->Start)
						break;

					if(Start < Prev->Start)
						Prev							= CONTAINING_RECORD(Prev->ListEntry.Blink,PCI_RANGE_LIST_ENTRY,ListEntry);
				}

				//
				// 2d06d
				//
				if(Start < Prev->Start || End > Prev->End)
				{
					//
					// 2d0bd
					//
					PPCI_RANGE_LIST_ENTRY Next			= Prev;

					if(End > Next->Start)
					{
						//
						// 2d0ce
						//
						while(End > Next->End)
							Next						= CONTAINING_RECORD(Next->ListEntry.Flink,PCI_RANGE_LIST_ENTRY,ListEntry);
					}
				
					//
					// 2d0eb
					//
					CurrentEntry						= RangeListArray + NextIndex;
					NextIndex							+= 1;

					CurrentEntry->Valid					= TRUE;
					CurrentEntry->Start					= Start;
					CurrentEntry->End					= End;
					CurrentEntry->ListEntry.Flink		= &Next->ListEntry;
					CurrentEntry->ListEntry.Blink		= &Prev->ListEntry;
					Next->ListEntry.Blink				= &CurrentEntry->ListEntry;
					Prev->ListEntry.Flink				= &CurrentEntry->ListEntry;

					PciDebugPrintf(0x7fffffff,"    (%I64x .. %I64x) <= (%I64x .. %I64x) <= (%I64x .. %I64x)\n",Prev->Start,Prev->End,Start,End,Next->Start,Next->End);

					if(Prev->Valid && Start > 0)
						Start							-= 1;

					if(Prev->End >= Start)
					{
						CurrentEntry->Start				= Prev->Start;
						CurrentEntry->ListEntry.Blink	= Prev->ListEntry.Blink;
						Prev->ListEntry.Blink->Flink	= &CurrentEntry->ListEntry;

						PciDebugPrintf(0x7fffffff,"    -- Overlaps lower, merged to (%I64x .. %I64x)\n",CurrentEntry->Start,CurrentEntry->End);
					}

					if(Next->Valid && End < 0xffffffffffffffff)
						End								+= 1;

					if(End >= Next->Start && CurrentEntry != Next)
					{
						CurrentEntry->End				= Next->End;
						CurrentEntry->ListEntry.Flink	= Next->ListEntry.Flink;
						Next->ListEntry.Flink->Blink	= &CurrentEntry->ListEntry;

						PciDebugPrintf(0x7fffffff,"    -- Overlaps upper, merged to (%I64x .. %I64x)\n",CurrentEntry->Start,CurrentEntry->End);
					}
				}
				else
				{
					//
					// 2d08c
					//
					PciDebugPrintf(0x7fffffff,"    -- (%I64x .. %I64x) swallows (%I64x .. %I64x)\n",Prev->Start,Prev->End,Start,End);
					Prev->Valid							= TRUE;
					CurrentEntry						= Prev;
				}

				PartialDesc								= PcipGetNextRangeFromList(&Context);
			}

			FullDesc									= reinterpret_cast<PCM_FULL_RESOURCE_DESCRIPTOR>(Context.CurrentDescriptor);
		}
	}

	while(CurrentEntry->Valid)
	{
		PPCI_RANGE_LIST_ENTRY PrevEntry					= CONTAINING_RECORD(CurrentEntry->ListEntry.Blink,PCI_RANGE_LIST_ENTRY,ListEntry);
		if(!PrevEntry->Valid)
			break;

		if(PrevEntry->Start > CurrentEntry->Start)
			break;

		CurrentEntry									= PrevEntry;
	}

	NTSTATUS Status										= STATUS_SUCCESS;

	if(CurrentEntry->Valid)
	{
		PciDebugPrintf(0x7fffffff,"    === ranges ===\n");

		PPCI_RANGE_LIST_ENTRY Temp						= CurrentEntry;
		do
		{
			if(Temp->Valid)
				PciDebugPrintf(0x7fffffff,"    %I64x .. %I64x\n",Temp->Start,Temp->End);

			Temp										= CONTAINING_RECORD(Temp->ListEntry.Flink,PCI_RANGE_LIST_ENTRY,ListEntry);

		}while(Temp != CurrentEntry);
	}
	else
	{
		PciDebugPrintf(0x7fffffff,"    ==== No ranges in results list. ====\n");
	}

	__try
	{
		if(ArbRes)
		{
			if(!CurrentEntry->Valid)
			{
				ULONGLONG Start							= 0;
				ULONGLONG End							= 0xffffffffffffffff;

				PciDebugPrintf(0x7fffffff,"    Adding to RtlRange  %I64x thru %I64x\n",Start,End);

				Status									= RtlAddRange(RangeList,Start,End,0,0,0,0);
				if(!NT_SUCCESS(Status))
					try_leave(ASSERT(NT_SUCCESS(Status)));
			}
			else
			{
				if(CurrentEntry->Start)
				{
					//
					// add [0 - currentStart - 1]
					//
					ULONGLONG Start						= 0;
					ULONGLONG End						= CurrentEntry->Start - 1;

					PciDebugPrintf(0x7fffffff,"    Adding to RtlRange  %I64x thru %I64x\n",Start,End);

					Status								= RtlAddRange(RangeList,Start,End,0,0,0,0);
					if(!NT_SUCCESS(Status))
						try_leave(ASSERT(NT_SUCCESS(Status)));
				}

				PPCI_RANGE_LIST_ENTRY Temp					= CurrentEntry;

				do
				{
					if(Temp->Valid)
					{
						//
						// add [currentEnd + 1,nextStart - 1]
						//
						PPCI_RANGE_LIST_ENTRY Next		= CONTAINING_RECORD(Temp->ListEntry.Flink,PCI_RANGE_LIST_ENTRY,ListEntry);

						ULONGLONG Start					= Temp->End + 1;
						ULONGLONG End					= Next->Start - 1;

						if(End < Start || Next == First)
							End							= 0xffffffffffffffff;

						PciDebugPrintf(0x7fffffff,"    Adding to RtlRange  %I64x thru %I64x\n",Start,End);

						Status							= RtlAddRange(RangeList,Start,End,0,0,0,0);
						if(!NT_SUCCESS(Status))
							try_leave(ASSERT(NT_SUCCESS(Status)));
					}

					Temp								= CONTAINING_RECORD(Temp->ListEntry.Flink,PCI_RANGE_LIST_ENTRY,ListEntry);

				}while(Temp != CurrentEntry);
			}
		}
		else if(CurrentEntry->Valid)
		{
			PPCI_RANGE_LIST_ENTRY Temp					= CurrentEntry;

			do
			{
				ULONGLONG Start							= Temp->Start;
				ULONGLONG End							= Temp->End;

				PciDebugPrintf(0x7fffffff,"    Adding to RtlRange  %I64x thru %I64x\n",Start,End);

				Status									= RtlAddRange(RangeList,Start,End,0,0,0,0);
				if(!NT_SUCCESS(Status))
					try_leave(ASSERT(NT_SUCCESS(Status)));

				Temp									= CONTAINING_RECORD(Temp->ListEntry.Flink,PCI_RANGE_LIST_ENTRY,ListEntry);

			}while(Temp != CurrentEntry);
		}
	}
	__finally
	{

	}

	ExFreePool(RangeListArray);

	return Status;
}

//
// initialize partial list context [checked]
//
VOID PcipInitializePartialListContext(__in PPCI_PARTIAL_LIST_CONTEXT Context,__in PCM_PARTIAL_RESOURCE_LIST List,__in UCHAR Type)
{
	PAGED_CODE();
	ASSERT(Type != CmResourceTypeNull);

	Context->AliasPortDescriptor.Type					= CmResourceTypeNull;
	Context->CurrentDescriptor							= List->PartialDescriptors;
	Context->DescriptorCount							= List->Count;
	Context->PartialList								= List;
	Context->ResourceType								= Type;
}

//
// get next cm descriptor [checked]
//
PCM_PARTIAL_RESOURCE_DESCRIPTOR PcipGetNextRangeFromList(__in PPCI_PARTIAL_LIST_CONTEXT Context)
{
	PAGED_CODE();

	if(Context->AliasPortDescriptor.Type == Context->ResourceType)
	{
		ULONG NextPort									= Context->AliasPortDescriptor.u.Port.Start.LowPart;
		if(FlagOn(Context->AliasPortDescriptor.Flags,CM_RESOURCE_PORT_10_BIT_DECODE))
			NextPort									+= (1 << 10);
		else if(FlagOn(Context->AliasPortDescriptor.Flags,CM_RESOURCE_PORT_12_BIT_DECODE))
			NextPort									+= (1 << 12);

		if(NextPort > 0xffff)
		{
			Context->AliasPortDescriptor.Type			= CmResourceTypeNull;
		}
		else
		{
			Context->AliasPortDescriptor.u.Port.Start.QuadPart	= NextPort;		
			return &Context->AliasPortDescriptor;
		}
	}

	PCM_PARTIAL_RESOURCE_DESCRIPTOR CurrentDesc			= Context->CurrentDescriptor;

	while(Context->DescriptorCount)
	{
		Context->CurrentDescriptor						= PciNextPartialDescriptor(CurrentDesc);
		if(CurrentDesc->Type == Context->ResourceType)
			break;

		CurrentDesc										= Context->CurrentDescriptor;
		Context->DescriptorCount						-= 1;
	}

	if(!Context->DescriptorCount)
		return 0;

	if(FlagOn(CurrentDesc->Flags,CM_RESOURCE_PORT_10_BIT_DECODE | CM_RESOURCE_PORT_12_BIT_DECODE))
		RtlCopyMemory(&Context->AliasPortDescriptor,CurrentDesc,sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));

	return CurrentDesc;
}

//
// find descriptor in cm resource list [checked]
//
PCM_PARTIAL_RESOURCE_DESCRIPTOR PciFindDescriptorInCmResourceList(__in UCHAR Type,__in PCM_RESOURCE_LIST CmResList,__in PCM_PARTIAL_RESOURCE_DESCRIPTOR StartPoint)
{
	PAGED_CODE();

	if(!CmResList)
		return 0;

	PCM_FULL_RESOURCE_DESCRIPTOR FullDesc				= CmResList->List;

	for(ULONG i = 0; i < CmResList->Count; i ++)
	{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDesc		= FullDesc->PartialResourceList.PartialDescriptors;

		for(ULONG j = 0; j < FullDesc->PartialResourceList.Count; j ++)
		{
			if(PartialDesc->Type == Type)
			{
				if(!StartPoint)
					return PartialDesc;

				if(StartPoint == PartialDesc)
					StartPoint							= 0;
			}

			PartialDesc									= PciNextPartialDescriptor(PartialDesc);
		}

		FullDesc										= reinterpret_cast<PCM_FULL_RESOURCE_DESCRIPTOR>(PartialDesc);
	}

	return 0;
}

//
// get next partial desc [checked]
//
PCM_PARTIAL_RESOURCE_DESCRIPTOR PciNextPartialDescriptor(__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Current)
{
	ULONG Length										= sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
	if(Current->Type == CmResourceTypeDeviceSpecific)
		Length											+= Current->u.DeviceSpecificData.DataSize;

	return Add2Ptr(Current,Length,PCM_PARTIAL_RESOURCE_DESCRIPTOR);
}

//
// insert list head [checked]
//
VOID PciInsertEntryAtHead(__in PSINGLE_LIST_ENTRY ListHead,__in PSINGLE_LIST_ENTRY ListEntry,__in PKEVENT Lock)
{
	PAGED_CODE();

	//
	// acquire lock
	//
	if(Lock)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(Lock,Executive,KernelMode,FALSE,0);
	}

	ListEntry->Next										= ListHead->Next;
	ListHead->Next										= ListEntry;

	//
	// release lock
	//
	if(Lock)
	{
		KeSetEvent(Lock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}
}

//
// insert list tail [checked]
//
VOID PciInsertEntryAtTail(__in PSINGLE_LIST_ENTRY ListHead,__in PSINGLE_LIST_ENTRY ListEntry,__in PKEVENT Lock)
{
	PAGED_CODE();

	//
	// acquire lock
	//
	if(Lock)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(Lock,Executive,KernelMode,FALSE,0);
	}

	PSINGLE_LIST_ENTRY Last								= ListHead;

	while(Last->Next)
		Last											= Last->Next;

	Last->Next											= ListEntry;

	//
	// release lock
	//
	if(Lock)
	{
		KeSetEvent(Lock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}
}

//
// remove from list [checked]
//
VOID PciRemoveEntryFromList(__in PSINGLE_LIST_ENTRY ListHead,__in PSINGLE_LIST_ENTRY ListEntry,__in PKEVENT Lock)
{
	ASSERT(ListEntry != ListHead);

	//
	// acquire lock
	//
	if(Lock)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(Lock,Executive,KernelMode,FALSE,0);
	}

	PSINGLE_LIST_ENTRY Previous							= ListHead;

	while(Previous && Previous->Next != ListEntry)
		Previous										= Previous->Next;

	ASSERT(Previous);
	Previous->Next										= ListEntry->Next;

	//
	// release lock
	//
	if(Lock)
	{
		KeSetEvent(Lock,IO_NO_INCREMENT,FALSE);
		KeLeaveCriticalRegion();
	}
}

//
// link secondary extension [checked]
//
VOID PcipLinkSecondaryExtension(__in PSINGLE_LIST_ENTRY ListHead,__in PPCI_SECONDARY_EXTENSION SecondaryExtension,__in PKEVENT Lock,
								__in PCI_SIGNATURE Type,__in PCI_ARBITER_INSTANCE_DESTRUCTOR Destructor)
{
	PAGED_CODE();

	SecondaryExtension->Destructor						= Destructor;
	SecondaryExtension->Type							= Type;

	PciInsertEntryAtHead(ListHead,&SecondaryExtension->ListEntry,Lock);
}

//
// destroy secondary extension [checked]
//
VOID PcipDestroySecondaryExtension(__in PSINGLE_LIST_ENTRY ListHead,__in PKEVENT Lock,__in PPCI_SECONDARY_EXTENSION SecondaryExtension)
{
	PAGED_CODE();

	PciRemoveEntryFromList(ListHead,&SecondaryExtension->ListEntry,Lock);

	if(SecondaryExtension->Destructor)
		SecondaryExtension->Destructor(CONTAINING_RECORD(SecondaryExtension,PCI_ARBITER_INSTANCE,SecondaryExtension));

	ExFreePool(SecondaryExtension);
}

//
// find secondary extensions [checked]
//
PPCI_SECONDARY_EXTENSION PciFindNextSecondaryExtension(__in PSINGLE_LIST_ENTRY FirstEntry,__in PCI_SIGNATURE Type)
{
	PPCI_SECONDARY_EXTENSION SecondaryExtension			= CONTAINING_RECORD(FirstEntry,PCI_SECONDARY_EXTENSION,ListEntry);

	while(SecondaryExtension && SecondaryExtension->Type != Type)
		SecondaryExtension								= CONTAINING_RECORD(SecondaryExtension->ListEntry.Next,PCI_SECONDARY_EXTENSION,ListEntry);

	return SecondaryExtension;
}

//
// string to ushort [checked]
//
BOOLEAN PciStringToUSHORT(__in PWCH String,__out PUSHORT UShort)
{
	PAGED_CODE();

	USHORT Ret											= 0;
	for(ULONG i = 0; i < 4; i ++)
	{
		WCHAR Ch										= String[i];

		if(Ch >= L'0' && Ch <= L'9')
			Ch 											-= L'0';
		else if(Ch >= L'a' && Ch <= L'f')
			Ch											-= (L'a' - 10);
		else if(Ch >= L'A' && Ch <= L'F')
			Ch											-= (L'A' - 10);
		else
			return FALSE;

		Ret												= (Ret << 4) | Ch;
	}

	*UShort												= Ret;

	return TRUE;
}

//
// send io control [checked]
//
NTSTATUS PciSendIoctl(__in PDEVICE_OBJECT DeviceObject,__in ULONG IoCode,__in PVOID Input,__in ULONG InputLength,__in PVOID Output,__in ULONG OutputLength)
{
	PAGED_CODE();

	DeviceObject										= IoGetAttachedDeviceReference(DeviceObject);
	if(!DeviceObject)
		return STATUS_INVALID_PARAMETER;

	NTSTATUS Status										= STATUS_SUCCESS;
	__try
	{
		KEVENT Event;
		KeInitializeEvent(&Event,SynchronizationEvent,FALSE);

		IO_STATUS_BLOCK Ios;
		PIRP Irp										= IoBuildDeviceIoControlRequest(IoCode,DeviceObject,Input,InputLength,Output,OutputLength,FALSE,&Event,&Ios);
		if(!Irp)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		Status											= IoCallDriver(DeviceObject,Irp);
		if(Status == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,0);
			Status										= Ios.Status;
		}
	}
	__finally
	{
		ObDereferenceObject(DeviceObject);
	}

	return Status;
}

//
// get hack flags [checked]
//
ULONGLONG PciGetHackFlags(__in USHORT VendorId,__in USHORT DeviceId,__in USHORT SubVendorId,__in USHORT SubSystemId,__in UCHAR RevisionId)
{
	ASSERT(PciHackTable);

	PPCI_HACK_TABLE_ENTRY BestEnry						= 0;
	ULONG BestWeight									= 0;

	for(PPCI_HACK_TABLE_ENTRY Entry	= PciHackTable; Entry->VendorId != PCI_INVALID_VENDORID; Entry ++)
	{
		if(Entry->VendorId != VendorId)
			continue;

		if(Entry->DeviceId != DeviceId)
			continue;

		ULONG Weight									= 1;

		if(Entry->RevisionIdValid && RevisionId != Entry->RevisionId)
			continue;
		else if(Entry->RevisionIdValid)
			Weight										= 3;

		if(Entry->SubSystemVendorIdValid && (Entry->SubSystemId != SubSystemId || Entry->SubVendorId != SubVendorId))
			continue;
		else if(Entry->SubSystemVendorIdValid)
			Weight										+= 4;

		if(Weight > BestWeight)
		{
			BestEnry									= Entry;
			BestWeight									= Weight;
		}
	}

	if(BestEnry)
		return BestEnry->HackFlags;

	return 0;
}

//
// read capabilities [checked]
//
UCHAR PciReadDeviceCapability(__in PPCI_PDO_EXTENSION PdoExt,__in UCHAR StartOffset,__in UCHAR SearchId,__in PVOID Buffer,__in ULONG Length)
{
	if(!StartOffset)
		return 0;

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);
	ASSERT(PdoExt->CapabilitiesPtr);
	ASSERT(Buffer);
	ASSERT(Length >= sizeof(PCI_CAPABILITIES_HEADER));

	PPCI_CAPABILITIES_HEADER Header						= static_cast<PPCI_CAPABILITIES_HEADER>(Buffer);

	for(ULONG Retry = 0; Retry < 0x30 && StartOffset; Retry ++)
	{
		ASSERT(StartOffset >= sizeof(PCI_COMMON_HEADER) && !FlagOn(StartOffset,3));

		//
		// read header
		//
		PciReadDeviceConfig(PdoExt,Header,StartOffset,sizeof(Header));

		//
		// this is the one that we are looking for
		//
		if(Header->CapabilityID == SearchId || !SearchId)
		{
			ASSERT(Length < sizeof(PCI_COMMON_CONFIG) - StartOffset);

			PciReadDeviceConfig(PdoExt,Header + 1,StartOffset + sizeof(PCI_CAPABILITIES_HEADER),Length - sizeof(PCI_CAPABILITIES_HEADER));

			return StartOffset;
		}

		StartOffset										= Header->Next;
	}

	if(StartOffset)
		PciDebugPrintf(0,"PCI device %p capabilities list is broken.\n",PdoExt);

	return 0;
}

//
// is on debug path [checked]
//
BOOLEAN PciIsDeviceOnDebugPath(__in PPCI_PDO_EXTENSION PdoExt)
{
	PAGED_CODE();

	ASSERT(PciDebugPortsCount <= 2);

	UCHAR Bus;
	PCI_SLOT_NUMBER Slot								= PdoExt->Slot;

	if(PdoExt->HeaderType == PCI_CARDBUS_BRIDGE_TYPE || PdoExt->HeaderType == PCI_BRIDGE_TYPE)
	{
		PCI_COMMON_HEADER BiosConfig;

		PciGetBiosConfig(PdoExt,&BiosConfig);

		for(ULONG i = 0; i < PciDebugPortsCount; i ++)
		{
			if(PciDebugPorts[i].Bus < BiosConfig.u.type1.SecondaryBus)
				continue;

			if(PciDebugPorts[i].Bus > BiosConfig.u.type1.SubordinateBus)
				continue;

			if(BiosConfig.u.type1.SecondaryBus == 0)
				continue;

			if(BiosConfig.u.type1.SubordinateBus)
				return TRUE;
		}
	}
	else
	{
		if(PdoExt->ParentFdoExtension == PdoExt->ParentFdoExtension->BusRootFdoExtension)
		{
			Bus											= PdoExt->ParentFdoExtension->BaseBus;
		}
		else
		{
			PCI_COMMON_HEADER BiosConfig;

			PciGetBiosConfig(static_cast<PPCI_PDO_EXTENSION>(PdoExt->ParentFdoExtension->PhysicalDeviceObject->DeviceExtension),&BiosConfig);

			if(BiosConfig.u.type1.SecondaryBus == 0 || BiosConfig.u.type1.SubordinateBus == 0)
				return FALSE;

			Bus											= BiosConfig.u.type1.SecondaryBus;
		}

		for(ULONG i = 0; i < PciDebugPortsCount; i ++)
		{
			if(PciDebugPorts[i].Bus == Bus && PciDebugPorts[i].SlotNumber.u.AsULONG == Slot.u.AsULONG)
				return TRUE;
		}
	}

	return FALSE;
}

//
// check acpi method [checked]
//
BOOLEAN PciIsSlotPresentInParentMethod(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG Method)
{
	PAGED_CODE();

	BOOLEAN Ret											=  FALSE;
	PACPI_EVAL_OUTPUT_BUFFER Output						= 0;

	__try
	{
		//
		// allocate output buffer
		//
		ULONG Length									= sizeof(ACPI_EVAL_OUTPUT_BUFFER) + sizeof(ACPI_METHOD_ARGUMENT) * 0x100;
		Output											= static_cast<PACPI_EVAL_OUTPUT_BUFFER>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
		if(!Output)
			try_leave(Ret = FALSE);

		//
		// eval acpi method
		//
		ACPI_EVAL_INPUT_BUFFER Input;
		Input.MethodNameAsUlong							= Method;
		Input.Signature									= ACPI_EVAL_INPUT_BUFFER_SIGNATURE;
		PDEVICE_OBJECT DeviceObject						= PdoExt->ParentFdoExtension->PhysicalDeviceObject;
		NTSTATUS Status									= PciSendIoctl(DeviceObject,IOCTL_ACPI_EVAL_METHOD,&Input,sizeof(Input),Output,Length);
		if(!NT_SUCCESS(Status))
			try_leave(Ret = FALSE);

		//
		// search result buffer
		//
		ULONG Slot										= (PdoExt->Slot.u.bits.DeviceNumber << 16) | PdoExt->Slot.u.bits.FunctionNumber;
		for(ULONG i = 0; i < Output->Count; i ++)
		{
			if(Output->Argument[i].Type != ACPI_METHOD_ARGUMENT_INTEGER)
				try_leave(Ret = FALSE);

			if(Output->Argument[i].Argument == Slot)
				try_leave(Ret = TRUE);
		}
	}
	__finally
	{
		if(Output)
			ExFreePool(Output);
	}

	return Ret;
}

//
// can we disable the device [checked]
//
BOOLEAN PciCanDisableDecodes(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in ULONG HackFlagsLow,__in ULONG HackFlagsHi,__in BOOLEAN DefaultRet)
{
	//
	// we can't stop those devices on a debug path
	//
	if(PdoExt && PdoExt->OnDebugPath)
		return FALSE;

	UCHAR BaseClass										= 0;
	UCHAR SubClass										= 0;

	if(PdoExt)
	{
		ASSERT(!HackFlagsLow && !HackFlagsHi);
		HackFlagsHi										= PdoExt->HackFlags.HighPart;
		HackFlagsLow									= PdoExt->HackFlags.LowPart;
		BaseClass										= PdoExt->BaseClass;
		SubClass										= PdoExt->SubClass;
	}
	else
	{
		ASSERT(Config);
		BaseClass										= Config->BaseClass;
		SubClass										= Config->SubClass;
	}

	if(FlagOn(HackFlagsLow,PCI_HACK_FLAGS_LOW_DONOT_TOUCH_COMMAND))
		return FALSE;

	if(FlagOn(HackFlagsLow,PCI_HACK_FLAGS_NO_DISABLE_DECODE))
		return FALSE;

	if(FlagOn(HackFlagsLow,PCI_HACK_FLAGS_NO_DISABLE_DECODE2))
		return FALSE;

	if(BaseClass == PCI_CLASS_DISPLAY_CTLR && SubClass == PCI_SUBCLASS_VID_VGA_CTLR)
		return DefaultRet;

	if(BaseClass == PCI_CLASS_PRE_20)
		return SubClass == PCI_SUBCLASS_PRE_20_VGA ? DefaultRet : TRUE;

	if(BaseClass == PCI_CLASS_DISPLAY_CTLR)
		return SubClass == PCI_SUBCLASS_VID_VGA_CTLR ? DefaultRet : TRUE;

	if(BaseClass != PCI_CLASS_BRIDGE_DEV)
		return TRUE;

	if(SubClass == PCI_SUBCLASS_BR_ISA || SubClass == PCI_SUBCLASS_BR_MCA || SubClass == PCI_SUBCLASS_BR_EISA)
		return FALSE;

	if(SubClass == PCI_SUBCLASS_BR_HOST || SubClass == PCI_SUBCLASS_BR_OTHER)
		return FALSE;

	if(SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI || SubClass == PCI_SUBCLASS_BR_CARDBUS)
		return (PdoExt ? PdoExt->Dependent.type1.VgaBitSet : BooleanFlagOn(Config->u.type1.BridgeControl,8)) ? DefaultRet : TRUE;

	return TRUE;
}

//
// enable/disable decode [checked]
//
VOID PciDecodeEnable(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN Enable,__in PUSHORT Command)
{
	//
	// check we can disable this device
	//
	if(!Enable && !PciCanDisableDecodes(PdoExt,0,0,0,FALSE))
		return;

	//
	// hack flags prevent from changing this device
	//
	if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_DONOT_TOUCH_COMMAND))
		return;

	USHORT LocalCommand									= 0;
	if(!Command)
		PciReadDeviceConfig(PdoExt,&LocalCommand,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(LocalCommand));
	else
		LocalCommand									= *Command;

	ClearFlag(LocalCommand,PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER);

	if(Enable)
		SetFlag(LocalCommand,FlagOn(PdoExt->CommandEnables,PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER));

	PciWriteDeviceConfig(PdoExt,&LocalCommand,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(LocalCommand));
}

//
// query interface [checked]
//
NTSTATUS PciQueryInterface(__in PPCI_COMMON_EXTENSION CommonExtension,__in GUID const* Type,__in USHORT Size,__in USHORT Version,
						   __in PVOID Data,__in PINTERFACE Interface,__in BOOLEAN LastChance)
{
	PAGED_CODE();

	UNICODE_STRING InterfaceType;

	if(NT_SUCCESS(RtlStringFromGUID(*Type,&InterfaceType)))
	{
		PciDebugPrintf(3,"PCI - PciQueryInterface TYPE = %wZ\n",&InterfaceType);
		PciDebugPrintf(0x7fffffff,"      Size = %d, Version = %d, InterfaceData = %x, LastChance = %s\n",Size,Version,Data,LastChance ? "TRUE" : "FASE");

		RtlFreeUnicodeString(&InterfaceType);
	}

	BOOLEAN IsPdo									= CommonExtension->ExtensionType == PciPdoExtensionType;
	PPCI_PDO_EXTENSION PdoExt						= CONTAINING_RECORD(CommonExtension,PCI_PDO_EXTENSION,Common);
	PPCI_FDO_EXTENSION FdoExt						= CONTAINING_RECORD(CommonExtension,PCI_FDO_EXTENSION,Common);
	PPCI_INTERFACE* InterfaceArray					= LastChance ? PciInterfacesLastResort : PciInterfaces;

	for(ULONG i = 0; InterfaceArray[i]; i ++)
	{
		PPCI_INTERFACE Current						= InterfaceArray[i];

		//
		// check interface flags
		//
		if(IsPdo)
		{
			if(!FlagOn(Current->Flags,PCI_INTERFACE_FLAGS_VALID_FOR_PDO))
			{
				if(NT_SUCCESS(RtlStringFromGUID(*Current->Guid,&InterfaceType)))
				{
					PciDebugPrintf(3,"PCI - PciQueryInterface: guid = %wZ only for PDOs\n",&InterfaceType);
					RtlFreeUnicodeString(&InterfaceType);
				}

				continue;
			}
		}
		else
		{
			if(!FlagOn(Current->Flags,PCI_INTERFACE_FLAGS_VALID_FOR_FDO))
			{
				if(NT_SUCCESS(RtlStringFromGUID(*Current->Guid,&InterfaceType)))
				{
					PciDebugPrintf(3,"PCI - PciQueryInterface: guid = %wZ only for FDOs\n",&InterfaceType);
					RtlFreeUnicodeString(&InterfaceType);
				}

				continue;
			}

			if(FlagOn(Current->Flags,PCI_INTERFACE_FLAGS_ONLY_FOR_ROOT) && FdoExt != FdoExt->BusRootFdoExtension)
			{
				if(NT_SUCCESS(RtlStringFromGUID(*Current->Guid,&InterfaceType)))
				{
					PciDebugPrintf(3,"PCI - PciQueryInterface: guid = %wZ only for ROOT\n",&InterfaceType);
					RtlFreeUnicodeString(&InterfaceType);
				}

				continue;
			}
		}

		if(NT_SUCCESS(RtlStringFromGUID(*Current->Guid,&InterfaceType)))
		{
			PciDebugPrintf(3,"PCI - PciQueryInterface looking at guid = %wZ\n",&InterfaceType);
			RtlFreeUnicodeString(&InterfaceType);
		}

		//
		// check guid,size and vesion
		//
		if(RtlCompareMemory(Current->Guid,Type,sizeof(GUID)) != sizeof(GUID))
			continue;

		if(Version < Current->MinVersion || Version > Current->MaxVersion)
			continue;

		if(Size < Current->MinSize)
			continue;

		//
		// call constructor
		//
		NTSTATUS Status								= Current->Constructor(CommonExtension,Current,Data,Version,Size,Interface);
		if(NT_SUCCESS(Status))
		{
			//
			// reference it before we return to our caller
			//
			Interface->InterfaceReference(Interface->Context);

			PciDebugPrintf(0x7fffffff,"PCI - PciQueryInterface returning SUCCESS\n");

			return Status;
		}

		PciDebugPrintf(3,"PCI - PciQueryInterface - Contructor %08lx = %08lx\n",Current->Constructor,Status);
	}

	PciDebugPrintf(0x7fffffff,"PCI - PciQueryInterface FAILED TO FIND INTERFACE\n");

	return STATUS_NOT_SUPPORTED;
}

//
// query legacy bus info [checked]
//
NTSTATUS PciQueryLegacyBusInformation(__in PPCI_FDO_EXTENSION FdoExt,__out PLEGACY_BUS_INFORMATION* Info)
{
	PAGED_CODE();

	ULONG Length										= sizeof(LEGACY_BUS_INFORMATION);
	PLEGACY_BUS_INFORMATION Buffer						= static_cast<PLEGACY_BUS_INFORMATION>(PciAllocateColdPoolWithTag(PagedPool,Length,'Bicp'));
	if(!Buffer)
		return STATUS_INSUFFICIENT_RESOURCES;

	Buffer->BusNumber									= FdoExt->BaseBus;
	Buffer->LegacyBusType								= PCIBus;
	Buffer->BusTypeGuid									= GUID_BUS_TYPE_PCI;
	*Info												= Buffer;

	return STATUS_SUCCESS;
}

//
// query pnp bus info [checked]
//
NTSTATUS PciQueryBusInformation(__in PPCI_PDO_EXTENSION PdoExt,__in PPNP_BUS_INFORMATION* Info)
{
	PAGED_CODE();

	ULONG Length										= sizeof(PNP_BUS_INFORMATION);
	PPNP_BUS_INFORMATION Buffer							= static_cast<PPNP_BUS_INFORMATION>(PciAllocateColdPoolWithTag(PagedPool,Length,'Bicp'));
	if(!Buffer)
		return STATUS_INSUFFICIENT_RESOURCES;

	Buffer->BusNumber									= PdoExt->ParentFdoExtension->BaseBus;
	Buffer->LegacyBusType								= PCIBus;
	Buffer->BusTypeGuid									= GUID_BUS_TYPE_PCI;
	*Info												= Buffer;

	return STATUS_SUCCESS;
}

//
// on vga path [checked]
//
BOOLEAN PciIsOnVGAPath(__in PPCI_PDO_EXTENSION PdoExt)
{
	if(PdoExt->BaseClass == PCI_CLASS_PRE_20)
		return PdoExt->SubClass == PCI_SUBCLASS_PRE_20_VGA;

	if(PdoExt->BaseClass == PCI_CLASS_DISPLAY_CTLR)
		return PdoExt->SubClass == PCI_SUBCLASS_VID_VGA_CTLR;

	if(PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && (PdoExt->SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI || PdoExt->SubClass == PCI_SUBCLASS_BR_CARDBUS))
		return PdoExt->Dependent.type1.VgaBitSet;

	return FALSE;
}

//
// is the same device [checked]
//
BOOLEAN PciIsSameDevice(__in PPCI_PDO_EXTENSION PdoExt)
{
	PCI_COMMON_HEADER Config;

	PciReadDeviceConfig(PdoExt,&Config,0,sizeof(Config));

	return PcipIsSameDevice(PdoExt,&Config);
}

//
// classify device type [checked]
//
ULONG PciClassifyDeviceType(__in PPCI_PDO_EXTENSION PdoExt)
{
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	if(PdoExt->BaseClass != PCI_CLASS_BRIDGE_DEV)
		return PCI_DEVICE_TYPE_DEVICE;

	if(PdoExt->SubClass == PCI_SUBCLASS_BR_CARDBUS)
		return PCI_DEVICE_TYPE_CARDBUS;

	if(PdoExt->SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI)
		return PCI_DEVICE_TYPE_PCI_TO_PCI;

	if(PdoExt->SubClass == PCI_SUBCLASS_BR_HOST)
		return PCI_DEVICE_TYPE_HOST;

	return PCI_DEVICE_TYPE_DEVICE;
}

//
// read device config space [checked]
//
NTSTATUS PciReadDeviceSpace(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG WhichSpace,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length,__out PULONG ReadLength)
{
	*ReadLength											= 0;

	NTSTATUS Status										= STATUS_INVALID_PARAMETER;

	if(WhichSpace == PCI_WHICHSPACE_CONFIG)
	{
		Status											= PciExternalReadDeviceConfig(PdoExt,Buffer,Offset,Length);
		if(NT_SUCCESS(Status))
			*ReadLength									= Length;
	}
	else if(WhichSpace == PCI_WHICHSPACE_ROM)
	{
		*ReadLength										= Length;
		Status											= PciReadRomImage(PdoExt,WhichSpace,Buffer,Offset,ReadLength);
	}
	else
	{
		PVERIFIER_FAILURE_DATA Data							= PciVerifierRetrieveFailureData(4);
		ASSERT(Data);
		VfFailDeviceNode(PdoExt->PhysicalDeviceObject,0xf6,4,Data->Offset4,&Data->Offset8,Data->FailureMessage,"%DevObj%Ulong",PdoExt->PhysicalDeviceObject,WhichSpace);
	}

	return Status;
}

//
// write device config space [checked]
//
NTSTATUS PciWriteDeviceSpace(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG WhichSpace,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length,__out PULONG WritenLength)
{
	*WritenLength										= 0;
	NTSTATUS Status										= STATUS_SUCCESS;

	if(WhichSpace == PCI_WHICHSPACE_CONFIG)
	{
		Status											= PciExternalWriteDeviceConfig(PdoExt,Buffer,Offset,Length);
	}
	else if(WhichSpace == PCI_WHICHSPACE_ROM)
	{
		PciDebugPrintf(0,"PCI (%08x) WRITE_CONFIG IRP for ROM, failing.\n",PdoExt);
		Status											= STATUS_INVALID_DEVICE_REQUEST;
	}
	else
	{
		PVERIFIER_FAILURE_DATA Data						= PciVerifierRetrieveFailureData(4);
		ASSERT(Data);
		VfFailDeviceNode(PdoExt->PhysicalDeviceObject,0xf6,4,Data->Offset4,&Data->Offset8,Data->FailureMessage,"%DevObj%Ulong",PdoExt->PhysicalDeviceObject,WhichSpace);
		Status											= STATUS_INVALID_PARAMETER;
	}

	if(NT_SUCCESS(Status))
		*WritenLength									= Length;

	return Status;
}

//
// query caps [checked]
//
NTSTATUS PciQueryCapabilities(__in PPCI_PDO_EXTENSION PdoExt,__in PDEVICE_CAPABILITIES DeviceCaps)
{
	PAGED_CODE();

	DeviceCaps->Address									= (PdoExt->Slot.u.bits.DeviceNumber << 16) | PdoExt->Slot.u.bits.FunctionNumber;
	DeviceCaps->RawDeviceOK								= (PdoExt->BaseClass == PCI_CLASS_BRIDGE_DEV && PdoExt->SubClass == PCI_SUBCLASS_BR_HOST) ? TRUE : FALSE;
	DeviceCaps->DockDevice								= FALSE;
	DeviceCaps->Removable								= FALSE;
	DeviceCaps->EjectSupported							= FALSE;
	DeviceCaps->LockSupported							= FALSE;
	DeviceCaps->UniqueID								= FALSE;

	PciDetermineSlotNumber(PdoExt,&DeviceCaps->UINumber);

	NTSTATUS Status										= PciQueryPowerCapabilities(PdoExt,DeviceCaps);
	if(NT_SUCCESS(Status) && FlagOn(PciDebug,0x10000))
		PciDebugDumpQueryCapabilities(DeviceCaps);

	return Status;
}

//
// determine slot number [checked]
//
NTSTATUS PciDetermineSlotNumber(__in PPCI_PDO_EXTENSION PdoExt,__out PULONG SlotNumber)
{
	if(!PciIrqRoutingTable || !PdoExt->ParentFdoExtension)
		return STATUS_UNSUCCESSFUL;

	for(ULONG i = sizeof(PCI_IRQ_ROUTING_TABLE_HEAD); i < PciIrqRoutingTable->Size; i += sizeof(PCI_IRQ_ROUTING_TABLE_ENTRY))
	{
		PPCI_IRQ_ROUTING_TABLE_ENTRY Entry				= Add2Ptr(PciIrqRoutingTable,i,PPCI_IRQ_ROUTING_TABLE_ENTRY);
		if(Entry->BusNumber == PdoExt->ParentFdoExtension->BaseBus && Entry->DeviceNumber == PdoExt->Slot.u.bits.DeviceNumber && Entry->SlotNumber)
		{
			*SlotNumber									= Entry->SlotNumber;
			return STATUS_SUCCESS;
		}
	}

	if(PdoExt->ParentFdoExtension == PdoExt->ParentFdoExtension->BusRootFdoExtension)
		return STATUS_UNSUCCESSFUL;

	ULONG Length										= sizeof(ULONG);
	return IoGetDeviceProperty(PdoExt->ParentFdoExtension->PhysicalDeviceObject,DevicePropertyUINumber,sizeof(ULONG),SlotNumber,&Length);
}

//
// query power caps [checked]
//
NTSTATUS PciQueryPowerCapabilities(__in PPCI_PDO_EXTENSION PdoExt,__in PDEVICE_CAPABILITIES DeviceCaps)
{
	//
	// query parent's device caps
	//
	DEVICE_CAPABILITIES ParentCaps;
	NTSTATUS Status										= PciGetDeviceCapabilities(PdoExt->ParentFdoExtension->PhysicalDeviceObject,&ParentCaps);
	if(!NT_SUCCESS(Status))
		return Status;

	//
	// setup default power state for working and shutdown
	//
	if(ParentCaps.DeviceState[PowerSystemWorking] == PowerDeviceUnspecified)
		ParentCaps.DeviceState[PowerSystemWorking]		= PowerDeviceD0;

	if(ParentCaps.DeviceState[PowerSystemShutdown] == PowerDeviceUnspecified)
		ParentCaps.DeviceState[PowerSystemShutdown]		= PowerDeviceD3;

	//
	// device does not have power management capabilities
	//
	if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_PME_CAPS))
	{
		DeviceCaps->DeviceWake							= PowerDeviceUnspecified;
		DeviceCaps->SystemWake							= PowerSystemUnspecified;
		DeviceCaps->DeviceD1							= FALSE;
		DeviceCaps->DeviceD2							= FALSE;
		DeviceCaps->WakeFromD0							= FALSE;
		DeviceCaps->WakeFromD1							= FALSE;
		DeviceCaps->WakeFromD2							= FALSE;
		DeviceCaps->WakeFromD3							= FALSE;
		RtlCopyMemory(DeviceCaps->DeviceState,ParentCaps.DeviceState,sizeof(DeviceCaps->DeviceState));

		return STATUS_SUCCESS;
	}

	//
	// setup those according to the power capabilities info in device's pci config space
	//
	DeviceCaps->DeviceD1								= PdoExt->PowerCapabilities.Support.D1;
	DeviceCaps->DeviceD2								= PdoExt->PowerCapabilities.Support.D2;
	DeviceCaps->WakeFromD0								= PdoExt->PowerCapabilities.Support.PMED0;
	DeviceCaps->WakeFromD1								= PdoExt->PowerCapabilities.Support.PMED1;
	DeviceCaps->WakeFromD2								= PdoExt->PowerCapabilities.Support.PMED2;

	if(ParentCaps.DeviceWake != PowerDeviceD3 || PdoExt->ParentFdoExtension == PdoExt->ParentFdoExtension->BusRootFdoExtension)
		DeviceCaps->WakeFromD3							= PdoExt->PowerCapabilities.Support.PMED3Hot;
	else
		DeviceCaps->WakeFromD3							= PdoExt->PowerCapabilities.Support.PMED3Cold;

	DEVICE_POWER_STATE DeviceWakeLevel					= PowerDeviceUnspecified;
	SYSTEM_POWER_STATE SystemWakeLevel					= PowerSystemUnspecified;
	SYSTEM_POWER_STATE MaxSystemSleepLevel				= PowerSystemUnspecified;

	for(SYSTEM_POWER_STATE i = PowerSystemWorking; i < PowerSystemMaximum; i = static_cast<SYSTEM_POWER_STATE>(i + 1))
	{
		//
		// basically,child's device map is the same as parent's
		// but we should make sure that the child dose support those states
		//
		DEVICE_POWER_STATE State						= ParentCaps.DeviceState[i];
		if(State == PowerDeviceD1 && !PdoExt->PowerCapabilities.Support.D1)
			State										= PowerDeviceD2;

		if(State == PowerDeviceD2 && !PdoExt->PowerCapabilities.Support.D2)
			State										= PowerDeviceD3;

		DeviceCaps->DeviceState[i]						= State;

		//
		// save the max system level for a sleep state
		//
		if(i < PowerSystemHibernate && State != PowerDeviceUnspecified)
			MaxSystemSleepLevel							= i;

		DEVICE_POWER_STATE ParentState					= ParentCaps.DeviceState[i];
		if(i < ParentCaps.SystemWake && State >= ParentState && ParentState != PowerDeviceUnspecified)
		{
			if( (State == PowerDeviceD0 && DeviceCaps->WakeFromD0) ||
				(State == PowerDeviceD1 && DeviceCaps->WakeFromD1) ||
				(State == PowerDeviceD2 && DeviceCaps->WakeFromD2) ||
				(State == PowerDeviceD3 && PdoExt->PowerCapabilities.Support.PMED3Hot && (ParentState < PowerDeviceD3 || PdoExt->PowerCapabilities.Support.PMED3Cold)))
			{
				SystemWakeLevel							= i;
				DeviceWakeLevel							= State;
			}
		}
	}

	//
	// parent support wake and child can stay in the parent's wake state
	//
	if( ParentCaps.SystemWake != PowerSystemUnspecified &&
		ParentCaps.DeviceWake != PowerDeviceUnspecified &&
		PdoExt->PowerState.DeviceWakeLevel != PowerDeviceUnspecified &&
		PdoExt->PowerState.DeviceWakeLevel >= ParentCaps.DeviceWake)
	{
		//
		// child's system wake level is the same as parent's
		//
		DeviceCaps->SystemWake							= ParentCaps.SystemWake;

		//
		// child's device wake leve is the max level it supports
		// PowerState->DeviceWakeLevel comes from PMC.Support.PMEDx
		//
		DeviceCaps->DeviceWake							= PdoExt->PowerState.DeviceWakeLevel;

		//
		// why does those check make sense?
		// if WakeFromDx is FALSE which means PMC.Support.PMEDx is FALSE,
		// we have not set DeviceWakeLevel in this situation?
		//
		if(DeviceCaps->DeviceWake == PowerDeviceD0 && !DeviceCaps->WakeFromD0)
			DeviceCaps->DeviceWake						= PowerDeviceD1;

		if(DeviceCaps->DeviceWake == PowerDeviceD1 && !DeviceCaps->WakeFromD1)
			DeviceCaps->DeviceWake						= PowerDeviceD2;

		if(DeviceCaps->DeviceWake == PowerDeviceD2 && !DeviceCaps->WakeFromD2)
			DeviceCaps->DeviceWake						= PowerDeviceD3;

		//
		// DeviceWakeLevel only depends on those values in PMC,but WakeFromD3 will account parent's wake capabilites
		// so there is a channce that DeviceWakeLevel is D3,but parent can not support wake from D3(in this situation we set child's WakeFromD3 to FALSE)
		//
		if(DeviceCaps->DeviceWake == PowerDeviceD3 && !DeviceCaps->WakeFromD3)
			DeviceCaps->DeviceWake						= PowerDeviceUnspecified;

		//
		// if those value changed because of the above check
		// it means that hardware provide wake info will not work because it conflicts with parent's capabilities
		// we should correct them
		//
		if(DeviceCaps->DeviceWake == PowerDeviceUnspecified || DeviceCaps->SystemWake == PowerSystemUnspecified)
		{
			if(SystemWakeLevel != PowerSystemUnspecified && DeviceWakeLevel != PowerDeviceUnspecified)
			{
				DeviceCaps->DeviceWake					= DeviceWakeLevel;
				DeviceCaps->SystemWake					= SystemWakeLevel;

				if(DeviceCaps->DeviceWake == PowerDeviceD3)
					DeviceCaps->WakeFromD3				= TRUE;

				//
				// if parent does not support wake from D3 and child does not support to wake from D3cold
				// reset it to the max sleep level
				//
				if(DeviceCaps->SystemWake > PowerSystemSleeping3 && (DeviceCaps->DeviceWake != PowerDeviceD3 || !PdoExt->PowerCapabilities.Support.PMED3Cold))
					DeviceCaps->SystemWake				= MaxSystemSleepLevel;
			}
		}

		DeviceCaps->D1Latency							= 0;
		DeviceCaps->D2Latency							= 2;
		DeviceCaps->D3Latency							= 100;

		ASSERT(DeviceCaps->DeviceState[PowerSystemWorking] == PowerDeviceD0);
	}
	else
	{
		DeviceCaps->D1Latency							= 0;
		DeviceCaps->D2Latency							= 0;
		DeviceCaps->D3Latency							= 0;
	}

	return STATUS_SUCCESS;
}

//
// get interrupt assigment [checked]
//
NTSTATUS PciGetInterruptAssignment(__in PPCI_PDO_EXTENSION PdoExt,__out PUCHAR MinVector,__out PUCHAR MaxVector)
{
	NTSTATUS Status										= STATUS_SUCCESS;
	PIO_RESOURCE_REQUIREMENTS_LIST List					= 0;

	__try
	{
		//
		// no interrupt resource
		//
		if(!PdoExt->InterruptPin)
			try_leave(Status = STATUS_RESOURCE_TYPE_NOT_FOUND);

		//
		// allocate a req list
		//
		List											= PciAllocateIoRequirementsList(1,PdoExt->ParentFdoExtension->BaseBus,PdoExt->Slot.u.AsULONG);
		if(!List)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// setup it
		//
		PIO_RESOURCE_DESCRIPTOR IoRes					= List->List[0].Descriptors;
		IoRes->Type										= CmResourceTypeInterrupt;
		IoRes->ShareDisposition							= CmResourceShareShared;
		IoRes->u.Interrupt.MaximumVector				= 0xff;
		IoRes->u.Interrupt.MinimumVector				= 0;
		IoRes->Option									= 0;
		IoRes->Flags									= 0;

		//
		// let hal adjust it
		//
		Status											= HalAdjustResourceList(List);
		if(!NT_SUCCESS(Status))
			try_leave(Status = STATUS_UNSUCCESSFUL;PciDebugPrintf(1,"PIN %02x, HAL FAILED Interrupt Assignment, status %08x\n",PdoExt->InterruptPin,Status));

		//
		// valid range?
		//
		UCHAR Min										= static_cast<UCHAR>(IoRes->u.Interrupt.MinimumVector);
		UCHAR Max										= static_cast<UCHAR>(IoRes->u.Interrupt.MaximumVector);
		if(Min <= Max)
			try_leave(*MinVector = Min;*MaxVector = Max;PciDebugPrintf(0x7fffffff,"    Interrupt assigned = 0x%x through 0x%x'\n",Min,Max));

		//
		// read interrupt line
		//
		UCHAR InterruptLine;
		PciReadDeviceConfig(PdoExt,&InterruptLine,FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.InterruptLine),sizeof(InterruptLine));

		if(InterruptLine || !PdoExt->RawInterruptLine)
			try_leave(Status = STATUS_UNSUCCESSFUL;PciDebugPrintf(1,"    PIN %02x, HAL could not assign interrupt.\n",PdoExt->InterruptPin));

		*MinVector										= PdoExt->RawInterruptLine;
		*MaxVector										= PdoExt->RawInterruptLine;
	}
	__finally
	{
		if(List)
			ExFreePool(List);
	}

	return Status;
}

//
// get device capabilities [checked]
//
NTSTATUS PciGetDeviceCapabilities(__in PDEVICE_OBJECT DeviceObject,__in PDEVICE_CAPABILITIES DeviceCaps)
{
	PAGED_CODE();

	RtlZeroMemory(DeviceCaps,sizeof(DEVICE_CAPABILITIES));
	DeviceCaps->Size									= sizeof(DeviceCaps);
	DeviceCaps->Version									= 1;
	DeviceCaps->Address									= 0xffffffff;
	DeviceCaps->UINumber								= 0xffffffff;

	KEVENT Event;
	KeInitializeEvent(&Event,SynchronizationEvent,FALSE);

	DeviceObject										= IoGetAttachedDeviceReference(DeviceObject);
	IO_STATUS_BLOCK IoStatus;
	PIRP Irp											= IoBuildSynchronousFsdRequest(IRP_MJ_PNP,DeviceObject,0,0,0,&Event,&IoStatus);
	NTSTATUS Status										= STATUS_INSUFFICIENT_RESOURCES;

	if(Irp)
	{
		PIO_STACK_LOCATION Sp							= IoGetNextIrpStackLocation(Irp);
		Sp->MajorFunction								= IRP_MJ_PNP;
		Sp->MinorFunction								= IRP_MN_QUERY_CAPABILITIES;
		Sp->Parameters.DeviceCapabilities.Capabilities	= DeviceCaps;
		Sp->Context										= 0;
		Sp->CompletionRoutine							= 0;
		Sp->Control										= 0;

		Status											= IoCallDriver(DeviceObject,Irp);
		if(Status == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,0);
			Status										= IoStatus.Status;
		}
	}

	ObDereferenceObject(DeviceObject);

	return Status;
}

//
// get length from bar [checked]
//
ULONG PciGetLengthFromBar(__in ULONG BaseAddress)
{
	if(FlagOn(BaseAddress,PCI_ADDRESS_IO_SPACE))
		BaseAddress										&= PCI_ADDRESS_IO_ADDRESS_MASK;
	else
		BaseAddress										&= PCI_ADDRESS_MEMORY_ADDRESS_MASK;

	ULONG Length										= ~BaseAddress + 1;

	if(Length & (Length - 1))
	{
		Length											= 4;
		while((Length | BaseAddress) != BaseAddress)
			Length										<<= 1;
	}

	return Length;
}

//
// create io resource descriptor from bar limit [checked]
//
BOOLEAN PciCreateIoDescriptorFromBarLimit(__in PIO_RESOURCE_DESCRIPTOR IoResDesc,__in PULONG BaseAddress,__in BOOLEAN RomAddress)
{
	ULONG BaseAddress0									= *BaseAddress;

	//
	// this bar does not exist
	//
	if(!FlagOn(BaseAddress0,~PCI_ADDRESS_IO_SPACE))
	{
		IoResDesc->Type									= CmResourceTypeNull;
		return FALSE;
	}

	BOOLEAN AddressIs64Bits								= FALSE;
	IoResDesc->Flags									= 0;
	IoResDesc->u.Generic.MinimumAddress.QuadPart		= 0;
	IoResDesc->u.Generic.MaximumAddress.HighPart		= 0;

	//
	// rom address is read-only
	//
	if(RomAddress)
	{
		BaseAddress0									&= PCI_ADDRESS_ROM_ADDRESS_MASK;
		IoResDesc->Flags								= CM_RESOURCE_MEMORY_READ_ONLY;
	}

	//
	// get length
	//
	ULONG Length										= PciGetLengthFromBar(BaseAddress0);
	IoResDesc->u.Generic.Length							= Length;
	IoResDesc->u.Generic.Alignment						= Length;
	ULONG Mask											= 0;

	if(FlagOn(BaseAddress0,PCI_ADDRESS_IO_SPACE))
	{
		//
		// port bar
		//
		IoResDesc->Type									= CmResourceTypePort;
		IoResDesc->Flags								= CM_RESOURCE_PORT_IO;
		Mask											= PCI_ADDRESS_IO_ADDRESS_MASK;
	}
	else
	{
		IoResDesc->Type									= CmResourceTypeMemory;
		ULONG MemoryType								= FlagOn(BaseAddress[0],PCI_ADDRESS_MEMORY_TYPE_MASK);
		Mask											= PCI_ADDRESS_MEMORY_ADDRESS_MASK;

		if(MemoryType == PCI_TYPE_64BIT)
		{
			//
			// 64bits memory
			//
			AddressIs64Bits								= TRUE;
			IoResDesc->u.Memory.MaximumAddress.HighPart	= BaseAddress[1];
		}
		else if(MemoryType == PCI_TYPE_20BIT)
		{
			//
			// below 1MB,clear high 12 bits
			//
			ClearFlag(Mask,0xfff00000);
		}

		//
		// prefectchable
		//
		if(FlagOn(BaseAddress[0],PCI_ADDRESS_MEMORY_PREFETCHABLE))
			SetFlag(IoResDesc->Flags,CM_RESOURCE_MEMORY_PREFETCHABLE);
	}

	IoResDesc->u.Generic.MaximumAddress.LowPart			= BaseAddress[0] & Mask;
	IoResDesc->u.Generic.MaximumAddress.QuadPart		+= (Length - 1);

	return AddressIs64Bits;
}

//
// exclude range list [checked]
//
NTSTATUS PciExcludeRangesFromWindow(__in ULONGLONG Start,__in ULONGLONG End,__in PRTL_RANGE_LIST FromList,__in PRTL_RANGE_LIST RemoveList)
{
	RTL_RANGE_LIST_ITERATOR Iterator;
	PRTL_RANGE Range									= 0;
	FOR_ALL_RANGES(RemoveList,&Iterator,Range)
	{
		if(!Range->Owner && INTERSECT(Range->Start,Range->End,Start,End))
		{
			NTSTATUS Status								= RtlAddRange(FromList,Range->Start,Range->End,0,RTL_RANGE_LIST_ADD_IF_CONFLICT,0,0);
			ASSERT(NT_SUCCESS(Status));

			if(!NT_SUCCESS(Status))
				return Status;
		}
	}

	return STATUS_SUCCESS;
}