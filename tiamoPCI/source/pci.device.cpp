//********************************************************************
//	created:	27:7:2008   1:46
//	file:		pci.device.cpp
//	author:		tiamo
//	purpose:	device
//********************************************************************

#include "stdafx.h"

#pragma alloc_text("PAGE",Device_GetAdditionalResourceDescriptors)
#pragma alloc_text("PAGE",Device_MassageHeaderForLimitsDetermination)
#pragma alloc_text("PAGE",Device_RestoreCurrent)
#pragma alloc_text("PAGE",Device_SaveLimits)
#pragma alloc_text("PAGE",Device_SaveCurrentSettings)

//
// reset device [checked]
//
NTSTATUS Device_ResetDevice(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config)
{
	return STATUS_SUCCESS;
}

//
// get additional resources [checked]
//
VOID Device_GetAdditionalResourceDescriptors(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in PIO_RESOURCE_DESCRIPTOR IoRes)
{
	PAGED_CODE();
}

//
// massage header [checked]
//
VOID Device_MassageHeaderForLimitsDetermination(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	//
	// skip first 4 bars for legacy mode ide controller
	//
	ULONG i												= 0;
	if(Param->PdoExt->BaseClass == PCI_CLASS_MASS_STORAGE_CTLR && Param->PdoExt->SubClass == PCI_SUBCLASS_MSC_IDE_CTLR && !NativeModeIde(Param->PdoExt->ProgIf))
		i												= 4;

	for( ; i < ARRAYSIZE(Param->Working->u.type0.BaseAddresses); i ++)
		Param->Working->u.type0.BaseAddresses[i]		= 0xffffffff;

	Param->Working->u.type0.ROMBaseAddress				= PCI_ADDRESS_ROM_ADDRESS_MASK;
}

//
// restore current [checked]
//
VOID Device_RestoreCurrent(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();
}

//
// save limits [checked]
//
VOID Device_SaveLimits(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	//
	// progIf & 5 != 5 means the ide controller is working under legacy mode
	//
	if(Param->PdoExt->BaseClass == PCI_CLASS_MASS_STORAGE_CTLR && Param->PdoExt->SubClass == PCI_SUBCLASS_MSC_IDE_CTLR && !NativeModeIde(Param->PdoExt->ProgIf))
		RtlZeroMemory(Param->Working->u.type0.BaseAddresses,sizeof(Param->Working->u.type0.BaseAddresses[0]) * 4);

	if(Param->PdoExt->VendorId == 0x5333 && (Param->PdoExt->DeviceId == 0x8880 || Param->PdoExt->DeviceId == 0x88f0))
	{
		for(ULONG i = 0; i < ARRAYSIZE(Param->Working->u.type0.BaseAddresses); i ++)
		{
			if(Param->Working->u.type0.BaseAddresses[i] == 0xfe000000)
			{
				PciDebugPrintf(0x7fffffff,"PCI - Adjusted broken S3 requirement from 32MB to 64MB\n");
				Param->Working->u.type0.BaseAddresses[i] = 0xfc000000;
			}
		}
	}

	if(Param->PdoExt->VendorId == 0x1013 && Param->PdoExt->DeviceId == 0x00a0)
	{
		ULONG Bar1										= Param->Working->u.type0.BaseAddresses[1];
		if((Bar1 & 0xffff) == 0xfc01)
		{
			if(Param->OriginalConfig->u.type0.BaseAddresses[1] != 1)
			{
				PciDebugPrintf(1,"PCI - Cirrus GD54xx 400 port IO requirement has a valid setting (%08x)\n",Param->OriginalConfig->u.type0.BaseAddresses[1]);
			}
			else
			{
				Param->Working->u.type0.BaseAddresses[1] = 0;
				PciDebugPrintf(0x7fffffff,"PCI - Ignored Cirrus GD54xx broken IO requirement (400 ports)\n");
			}
		}
		else if(Bar1)
		{
			PciDebugPrintf(1,"PCI - Warning Cirrus Adapter 101300a0 has unexpected resource requirement (%08x)\n",Bar1);
		}
	}

	//
	// build io descriptor from bars
	//
	for(ULONG i = 0; i < ARRAYSIZE(Param->Working->u.type0.BaseAddresses); i ++)
	{
		if(PciCreateIoDescriptorFromBarLimit(Param->PdoExt->Resources->Limit + i,Param->Working->u.type0.BaseAddresses + i,FALSE))
		{
			//
			// this is a 64 bits memory bar
			//
			ASSERT(i + 1 < ARRAYSIZE(Param->Working->u.type0.BaseAddresses));
			i											+= 1;
			Param->PdoExt->Resources->Limit[i].Type		= CmResourceTypeNull;
		}
	}

	//
	// rom address
	//
	PciCreateIoDescriptorFromBarLimit(Param->PdoExt->Resources->Limit + ARRAYSIZE(Param->Working->u.type0.BaseAddresses),&Param->Working->u.type0.ROMBaseAddress,TRUE);
}

//
// save current [checked]
//
VOID Device_SaveCurrentSettings(__in PPCI_CONFIGURATOR_PARAM Param)
{
	PAGED_CODE();

	//
	// for each io resource,we build a cm resource according to the bar
	//
	PPCI_FUNCTION_RESOURCES Resource					= Param->PdoExt->Resources;
	for(ULONG i = 0; i < ARRAYSIZE(Param->PdoExt->Resources->Current); i ++)
	{
		Resource->Current[i].Type						= Resource->Limit[i].Type;

		//
		// this bar is valid?
		//
		if(Resource->Limit[i].Type != CmResourceTypeNull)
		{
			PCM_PARTIAL_RESOURCE_DESCRIPTOR Current		= Resource->Current + i;
			PIO_RESOURCE_DESCRIPTOR Limit				= Resource->Limit + i;
			Current->Flags								= Limit->Flags;
			Current->ShareDisposition					= Limit->ShareDisposition;
			Current->u.Generic.Length					= Limit->u.Generic.Length;
			Current->u.Generic.Start.QuadPart			= 0;

			if(i == ARRAYSIZE(Param->OriginalConfig->u.type0.BaseAddresses))
			{
				//
				// descriptor for rom
				//
				ULONG RomAddress						= Param->OriginalConfig->u.type0.ROMBaseAddress;
				if(FlagOn(RomAddress,PCI_ROMADDRESS_ENABLED))
					Current->u.Memory.Start.LowPart		= RomAddress & PCI_ADDRESS_ROM_ADDRESS_MASK;
				else
					Current->Type						= CmResourceTypeNull;
			}
			else
			{
				ULONG Mask								= 0;
				PULONG BaseAddress						= Param->OriginalConfig->u.type0.BaseAddresses + i;
				if(FlagOn(BaseAddress[0],PCI_ADDRESS_IO_SPACE))
				{
					Mask								= PCI_ADDRESS_IO_ADDRESS_MASK;
					ASSERT(Current->Type == CmResourceTypePort);
				}
				else
				{
					ASSERT(Current->Type == CmResourceTypeMemory);
					ULONG MemoryType					= FlagOn(BaseAddress[0],PCI_ADDRESS_MEMORY_TYPE_MASK);

					Mask								= PCI_ADDRESS_MEMORY_ADDRESS_MASK;

					//
					// note that even the 64bit bar will occupy 2 ULONGs,we did not add an additional 1 to i,
					// because we have already set the next IoResDesc's type to CmResourceTypeNull,etc. the if statement above will skip it
					//
					if(MemoryType == PCI_TYPE_64BIT)
						Current->u.Memory.Start.HighPart= BaseAddress[1];
					else if(MemoryType == PCI_TYPE_20BIT)
						ClearFlag(Mask,0xfff00000);
				}

				Current->u.Generic.Start.LowPart		= BaseAddress[0] & Mask;

				if(!Current->u.Generic.Start.QuadPart)
					Current->Type						= CmResourceTypeNull;
			}
		}
	}

	//
	// save sub system id and sub vendor id
	//
	Param->PdoExt->SubSystemId							= Param->OriginalConfig->u.type0.SubSystemID;
	Param->PdoExt->SubVendorId							= Param->OriginalConfig->u.type0.SubVendorID;
}

//
// change resource settings [checked]
//
VOID Device_ChangeResourceSettings(__in struct _PCI_PDO_EXTENSION* PdoExt,__in PPCI_COMMON_HEADER Config)
{
	if(!PdoExt->Resources)
		return;

	for(ULONG i = 0; i < ARRAYSIZE(PdoExt->Resources->Current); i ++)
	{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR Current			= PdoExt->Resources->Current + i;
		if(Current->Type == CmResourceTypeNull)
			continue;

		ULONG StartLow									= Current->u.Generic.Start.LowPart;

		if(i == ARRAYSIZE(Config->u.type0.BaseAddresses))
		{
			ASSERT(Current->Type == CmResourceTypeMemory);

			ClearFlag(Config->u.type0.ROMBaseAddress,PCI_ADDRESS_ROM_ADDRESS_MASK);
			SetFlag(Config->u.type0.ROMBaseAddress,FlagOn(StartLow,PCI_ADDRESS_ROM_ADDRESS_MASK));
		}
		else
		{
			ULONG OldBaseAddress						= Config->u.type0.BaseAddresses[i];
			if(FlagOn(OldBaseAddress,PCI_ADDRESS_IO_SPACE))
			{
				ASSERT(Current->Type == CmResourceTypePort);
				Config->u.type0.BaseAddresses[i]		= StartLow;
			}
			else
			{
				ASSERT(Current->Type == CmResourceTypeMemory);
				Config->u.type0.BaseAddresses[i]		= StartLow;

				ULONG MemoryType						= FlagOn(OldBaseAddress,PCI_ADDRESS_MEMORY_TYPE_MASK);
				if(MemoryType == PCI_TYPE_64BIT)
					Config->u.type0.BaseAddresses[++ i]	= Current->u.Memory.Start.HighPart;
				else if(MemoryType == PCI_TYPE_20BIT)
					ASSERT(!FlagOn(StartLow,0xfff00000));
			}
		}
	}
}