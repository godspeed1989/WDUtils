//********************************************************************
//	created:	23:7:2008   19:29
//	file:		pci.id.cpp
//	author:		tiamo
//	purpose:	device id
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",PciGetDeviceDescriptionMessage)
#pragma alloc_text("PAGE",PciQueryId)
#pragma alloc_text("PAGE",PciQueryDeviceText)
//
// query id [checked]
//
NTSTATUS PciQueryId(__in PPCI_PDO_EXTENSION PdoExt,__in BUS_QUERY_ID_TYPE Type,__out PWCHAR* IdBuffer)
{
	PAGED_CODE();

	static CHAR Null[2]									= {0};
	NTSTATUS Status										= STATUS_SUCCESS;
	*IdBuffer											= 0;
	PWCHAR Buffer										= 0;
	ULONG SubSystemVendorId								= (PdoExt->SubSystemId << 16) | PdoExt->SubVendorId;
	PCI_ID_BUFFER PciIdBuffer;

	__try
	{
		__try
		{
			if(Type < BusQueryDeviceID || Type > BusQueryInstanceID)
				try_leave(Status = STATUS_NOT_SUPPORTED);

			PciInitIdBuffer(&PciIdBuffer);

			if(Type == BusQueryInstanceID)
			{
				PciIdPrintf(&PciIdBuffer,Null);

				PciIdPrintfAppend(&PciIdBuffer,"%02X",(PdoExt->Slot.u.bits.DeviceNumber << 3) | PdoExt->Slot.u.bits.FunctionNumber);

				PPCI_FDO_EXTENSION FdoExt				= PdoExt->ParentFdoExtension;
				while(FdoExt != FdoExt->BusRootFdoExtension)
				{
					PCI_SLOT_NUMBER Slot				= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension)->Slot;
					PciIdPrintfAppend(&PciIdBuffer,"%02X",(Slot.u.bits.DeviceNumber << 3) | PdoExt->Slot.u.bits.FunctionNumber);

					FdoExt								= static_cast<PPCI_PDO_EXTENSION>(FdoExt->PhysicalDeviceObject->DeviceExtension)->ParentFdoExtension;
				}
				try_leave(NOTHING);
			}

			if(Type == BusQueryDeviceID || Type == BusQueryHardwareIDs)
			{
				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X&DEV_%04X&SUBSYS_%08X&REV_%02X",PdoExt->VendorId,PdoExt->DeviceId,SubSystemVendorId,PdoExt->RevisionId);

				if(Type == BusQueryDeviceID)
					try_leave(NOTHING);

				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X&DEV_%04X&SUBSYS_%08X",PdoExt->VendorId,PdoExt->DeviceId,SubSystemVendorId);
			}

			if((Type == BusQueryHardwareIDs && !SubSystemVendorId) || (SubSystemVendorId && Type == BusQueryCompatibleIDs))
			{
				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X&DEV_%04X&REV_%02X",PdoExt->VendorId,PdoExt->DeviceId,PdoExt->RevisionId);

				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X&DEV_%04X",PdoExt->VendorId,PdoExt->DeviceId);
			}

			if(Type == BusQueryHardwareIDs)
			{
				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X&DEV_%04X&CC_%02X%02X%02X",PdoExt->VendorId,PdoExt->DeviceId,PdoExt->BaseClass,PdoExt->SubClass,PdoExt->ProgIf);

				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X&DEV_%04X&CC_%02X%02X",PdoExt->VendorId,PdoExt->DeviceId,PdoExt->BaseClass,PdoExt->SubClass);
			}

			if(Type == BusQueryCompatibleIDs)
			{
				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X&CC_%02X%02X%02X",PdoExt->VendorId,PdoExt->BaseClass,PdoExt->SubClass,PdoExt->ProgIf);

				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X&CC_%02X%02X",PdoExt->VendorId,PdoExt->BaseClass,PdoExt->SubClass);

				PciIdPrintf(&PciIdBuffer,"PCI\\VEN_%04X",PdoExt->VendorId);

				PciIdPrintf(&PciIdBuffer,"PCI\\&CC_%02X%02X%02X",PdoExt->BaseClass,PdoExt->SubClass,PdoExt->ProgIf);

				PciIdPrintf(&PciIdBuffer,"PCI\\&CC_%02X%02X",PdoExt->BaseClass,PdoExt->SubClass);
			}

			//
			// append a null
			//
			PciIdPrintf(&PciIdBuffer,Null);
		}
		__finally
		{
		}

		if(!NT_SUCCESS(Status))
			try_leave(NOTHING);

		ASSERT(PciIdBuffer.Count);

		//
		// allocate buffer
		//
		Buffer											= static_cast<PWCHAR>(PciAllocateColdPoolWithTag(PagedPool,PciIdBuffer.TotalLength,'BicP'));
		if(!Buffer)
			try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

		//
		// convert to unicode
		//
		UNICODE_STRING String;
		String.Buffer									= Buffer;
		String.MaximumLength							= PciIdBuffer.TotalLength;

		for(ULONG i = 0; i < PciIdBuffer.Count; i ++)
		{
			if(!NT_SUCCESS(RtlAnsiStringToUnicodeString(&String,&PciIdBuffer.AnsiString[i],FALSE)))
				try_leave(NOTHING);

			String.MaximumLength						-= PciIdBuffer.UnicodeStringSize[i];
			String.Buffer								+= PciIdBuffer.UnicodeStringSize[i] / sizeof(WCHAR);
		}

		*IdBuffer										= Buffer;
	}
	__finally
	{
		if(!NT_SUCCESS(Status) || AbnormalTermination())
		{
			if(Buffer)
				ExFreePool(Buffer);
		}
	}

	return Status;
}

//
// query text [checked]
//
NTSTATUS PciQueryDeviceText(__in PPCI_PDO_EXTENSION PdoExt,__in DEVICE_TEXT_TYPE Type,__in LCID LocalId,__out PWCHAR* TextBuffer)
{
	PAGED_CODE();

	NTSTATUS Status										= STATUS_NOT_SUPPORTED;

	switch(Type)
	{
	case DeviceTextDescription:
		*TextBuffer										= PciGetDeviceDescriptionMessage(PdoExt->BaseClass,PdoExt->SubClass);
		Status											= *TextBuffer ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
		break;

	case DeviceTextLocationInformation:
		{
			PWCHAR Format								= 0;
			__try
			{
				Format									= PciGetDescriptionMessage(0x10000);
				if(!Format)
					try_leave(Status = STATUS_NOT_SUPPORTED);

				ULONG Length							= (wcslen(Format) + 7) * sizeof(WCHAR);

				PWCHAR Buffer							= static_cast<PWCHAR>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
				if(!Buffer)
					try_leave(Status = STATUS_INSUFFICIENT_RESOURCES);

				RtlStringCchPrintfW(Buffer,Length / sizeof(WCHAR) - 1,Format,PdoExt->ParentFdoExtension->BaseBus,
									PdoExt->Slot.u.bits.DeviceNumber,PdoExt->Slot.u.bits.FunctionNumber);

				*TextBuffer								= Buffer;
				Status									= STATUS_SUCCESS;
			}
			__finally
			{
				if(Format)
					ExFreePool(Format);
			}
		}
		break;
	}

	return Status;
}

//
// get device description message [checked]
//
PWCHAR PciGetDeviceDescriptionMessage(__in UCHAR BaseClass,__in UCHAR SubClass)
{
	PAGED_CODE();

	USHORT Class										= (BaseClass << 8) | SubClass;

	PWCHAR Ret											= PciGetDescriptionMessage(Class);

	if(Ret)
		return Ret;

	Ret													= static_cast<PWCHAR>(ExAllocatePoolWithTag(PagedPool,sizeof(L"PCI Device"),'BicP'));
	if(Ret)
		RtlCopyMemory(Ret,L"PCI Device",sizeof(L"PCI Device"));

	return Ret;
}

//
// get string from resource [checked]
//
PWCHAR PciGetDescriptionMessage(__in ULONG ResourceId)
{
	PMESSAGE_RESOURCE_ENTRY Entry						= 0;
	if(!NT_SUCCESS(RtlFindMessage(PciDriverObject->DriverStart,0x0b,0x0,ResourceId,&Entry)))
		return 0;

	if(FlagOn(Entry->Flags,MESSAGE_RESOURCE_UNICODE))
	{
		ULONG Length									= Entry->Length - sizeof(MESSAGE_RESOURCE_ENTRY) - sizeof(WCHAR);
		PWCHAR Buffer									= reinterpret_cast<PWCHAR>(Entry->Text);
		if(!Buffer[Length / sizeof(WCHAR)])
			Length										-= sizeof(WCHAR);

		ASSERT(Length > 1);
		ASSERT(Buffer[Length / sizeof(WCHAR)] == L'\n');

		PWCHAR Ret										= static_cast<PWCHAR>(PciAllocateColdPoolWithTag(PagedPool,Length,'BicP'));
		if(Ret)
		{
			RtlCopyMemory(Ret,Buffer,Length);
			Ret[Length / sizeof(WCHAR) - 1]				= 0;
		}

		return Ret;
	}

	ANSI_STRING AnsiString;
	RtlInitAnsiString(&AnsiString,reinterpret_cast<PCHAR>(Entry->Text));

	AnsiString.Length									-= 2;

	UNICODE_STRING UnicodeString;
	RtlAnsiStringToUnicodeString(&UnicodeString,&AnsiString,TRUE);

	return UnicodeString.Buffer;
}

//
// init id buffer [checked]
//
VOID PciInitIdBuffer(__in PPCI_ID_BUFFER IdBuffer)
{
	PAGED_CODE();

	IdBuffer->Count										= 0;
	IdBuffer->TotalLength								= 0;
	IdBuffer->CurrentBuffer								= IdBuffer->StorageBuffer;
}

//
// id printf [checked]
//
VOID PciIdPrintf(__in PPCI_ID_BUFFER IdBuffer,__in PCHAR Format,...)
{
	PAGED_CODE();

	ASSERT(IdBuffer->Count < ARRAYSIZE(IdBuffer->AnsiString));

	ULONG Length										= IdBuffer->StorageBuffer + ARRAYSIZE(IdBuffer->StorageBuffer) - IdBuffer->CurrentBuffer;
	size_t LeftLength;

	va_list list;
	va_start(list,Format);
	RtlStringCchVPrintfExA(IdBuffer->CurrentBuffer,Length,0,&LeftLength,0,Format,list);
	va_end(list);

	Length												= static_cast<ULONG>(Length - LeftLength);
	IdBuffer->AnsiString[IdBuffer->Count].Buffer		= IdBuffer->CurrentBuffer;
	IdBuffer->AnsiString[IdBuffer->Count].Length		= static_cast<USHORT>(Length);
	IdBuffer->AnsiString[IdBuffer->Count].MaximumLength	= static_cast<USHORT>(Length + sizeof(CHAR));

	ULONG Size											= RtlAnsiStringToUnicodeSize(&IdBuffer->AnsiString[IdBuffer->Count]);
	IdBuffer->UnicodeStringSize[IdBuffer->Count]		= static_cast<USHORT>(Size);
	IdBuffer->TotalLength								+= static_cast<USHORT>(Size);
	IdBuffer->Count										+= 1;
	IdBuffer->CurrentBuffer								+= Length + 1;
}

//
// append printf [checked]
//
VOID PciIdPrintfAppend(__in PPCI_ID_BUFFER IdBuffer,__in PCHAR Format,...)
{
	PAGED_CODE();

	ASSERT(IdBuffer->Count);

	ULONG Length										= IdBuffer->StorageBuffer + ARRAYSIZE(IdBuffer->StorageBuffer) - IdBuffer->CurrentBuffer + 1;
	size_t LeftLength;

	va_list list;
	va_start(list,Format);
	RtlStringCchVPrintfExA(IdBuffer->CurrentBuffer - 1,Length,0,&LeftLength,0,Format,list);
	va_end(list);

	Length													= static_cast<ULONG>(Length - LeftLength);
	IdBuffer->AnsiString[IdBuffer->Count - 1].Length		+= static_cast<USHORT>(Length);
	IdBuffer->AnsiString[IdBuffer->Count - 1].MaximumLength	+= static_cast<USHORT>(Length);

	ULONG Size											= RtlAnsiStringToUnicodeSize(&IdBuffer->AnsiString[IdBuffer->Count]);
	IdBuffer->UnicodeStringSize[IdBuffer->Count - 1]	= static_cast<USHORT>(Size);
	IdBuffer->TotalLength								+= static_cast<USHORT>(Size);
	IdBuffer->CurrentBuffer								+= Length;
}