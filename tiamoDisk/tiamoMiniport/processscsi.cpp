
//**************************************************************************************
//	日期:	1:7:2004   
//	创建:	tiamo	
//	描述:	miniport scsi process
//**************************************************************************************

#include "stdafx.h"

// convert cdb6 to 10 for read write
VOID ConvertCdb6To10(PSCSI_REQUEST_BLOCK pSrb)
{
	PCDB pCdb10 = reinterpret_cast<PCDB>(pSrb->Cdb);
	PCDB pCdb6 = reinterpret_cast<PCDB>(pSrb->Cdb);

	pCdb10->CDB10.Control = pCdb6->CDB6READWRITE.Control;
	pCdb10->CDB10.TransferBlocksMsb = 0;
	pCdb10->CDB10.TransferBlocksLsb = pCdb6->CDB6READWRITE.TransferBlocks;

	pCdb10->CDB10.LogicalBlockByte3 = pCdb6->CDB6READWRITE.LogicalBlockLsb;
	pCdb10->CDB10.LogicalBlockByte2 = pCdb6->CDB6READWRITE.LogicalBlockMsb0;
	pCdb10->CDB10.LogicalBlockByte1 = pCdb6->CDB6READWRITE.LogicalBlockMsb1;
	pCdb10->CDB10.LogicalBlockByte0 = 0;

	pCdb10->CDB10.LogicalUnitNumber = pCdb6->CDB6READWRITE.LogicalUnitNumber;
}

// swap big endian to little endian
VOID SwapReadWriteCBD10(PSCSI_REQUEST_BLOCK pSrb)
{
	PCDB pCdb10 = reinterpret_cast<PCDB>(pSrb->Cdb);

	UCHAR temp = pCdb10->CDB10.LogicalBlockByte0;
	pCdb10->CDB10.LogicalBlockByte0 = pCdb10->CDB10.LogicalBlockByte3;
	pCdb10->CDB10.LogicalBlockByte3 = temp;

	temp = pCdb10->CDB10.LogicalBlockByte1;
	pCdb10->CDB10.LogicalBlockByte1 = pCdb10->CDB10.LogicalBlockByte2;
	pCdb10->CDB10.LogicalBlockByte2 = temp;

	temp = pCdb10->CDB10.TransferBlocksLsb;
	pCdb10->CDB10.TransferBlocksLsb = pCdb10->CDB10.TransferBlocksMsb;
	pCdb10->CDB10.TransferBlocksMsb = temp;
}

// check bound
BOOLEAN CheckBound(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb,PULONG pulStart,PULONG pulLen)
{
	PCDB pCdb10 = reinterpret_cast<PCDB>(pSrb->Cdb);
	*pulStart = *reinterpret_cast<PULONG>(&pCdb10->CDB10.LogicalBlockByte0);
	*pulLen = *reinterpret_cast<PUSHORT>(&pCdb10->CDB10.TransferBlocksMsb);

	if(!*pulLen)
	{
		SetSrbSenseCode(pSrb,INVALID_SUB_REQUEST);
		return FALSE;
	}

	if(*pulStart + *pulLen > pExt->m_ulBlockCount[pSrb->TargetId])
	{
		SetSrbSenseCode(pSrb,OUT_BOUND_ACCESS);
		return FALSE;
	}

	return TRUE;
}

// test unit ready
BOOLEAN TestUnitReady(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	if(!pExt->m_hFileHandle[pSrb->TargetId])
	{
		devDebugPrint(DRIVER_NAME"*******TestUnitReady..no media in device\n");
		SetSrbSenseCode(pSrb,NO_MEDIA_IN_DEVICE);
	}

	return TRUE;
}

// rezero unit
BOOLEAN RezeroUnit(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)						
{
	return TRUE;
}

// invalid scsi cmd
BOOLEAN InvalidScsiCmd(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	SetSrbSenseCode(pSrb,INVALID_REQUEST);
	return TRUE;
}

// request sense...because we use auto request sense ,so this is not needed
BOOLEAN RequestSense(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	return TRUE;
}

// format unit
BOOLEAN FormatUnit(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)						
{
	return TRUE;
}

// seek6...no need to seek
BOOLEAN Seek6(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)							
{
	return TRUE;
}

// inquiry
BOOLEAN Inquiry(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	if(pSrb->TargetId >= pExt->m_ulDevices || pSrb->Lun)
	{
		devDebugPrint("\tInquiry...(target-lun)=(%d,%d)..not supported\n",pSrb->TargetId,pSrb->Lun);
		pSrb->SrbStatus = SRB_STATUS_SELECTION_TIMEOUT;
		return TRUE;
	}

	if(pSrb->Cdb[1] & CDB_INQUIRY_EVPD)
	{
		devDebugPrint("\tInquiry..not support evpd\n");
		SetSrbSenseCode(pSrb,INVALID_SUB_REQUEST);
		return TRUE;
	}

	PINQUIRYDATA pInquiryData = (PINQUIRYDATA)pSrb->DataBuffer;
	
	pInquiryData->DeviceType = DIRECT_ACCESS_DEVICE;
	pInquiryData->DeviceTypeQualifier = DEVICE_CONNECTED;

	pInquiryData->DeviceTypeModifier = 0;
	pInquiryData->RemovableMedia = 0;
	pInquiryData->ResponseDataFormat = 2;
	pInquiryData->Versions = 0;
	pInquiryData->AdditionalLength = sizeof(INQUIRYDATA) - 5;

	RtlMoveMemory(pInquiryData->VendorId,"Tiamo   ",8);

	RtlMoveMemory(pInquiryData->ProductId,"Virtual DISK    ",16);

	RtlMoveMemory(pInquiryData->ProductRevisionLevel,"1010",4);

	devDebugPrint("\tInquiry...succeeded(target-lun)=(%d,%d)\n",pSrb->TargetId,pSrb->Lun);

	return TRUE;
}

// reserve
BOOLEAN Reserve(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)							
{
	return TRUE;
}

// release
BOOLEAN Release(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)							
{
	return TRUE;
}

// start stop
BOOLEAN StartStopUnit(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)					
{
	return TRUE;
}

// send diagnostic
BOOLEAN SendDiagnostic(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)					
{
	return TRUE;
}

// read caps
BOOLEAN ReadCaps(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	if(!pExt->m_hFileHandle[pSrb->TargetId])
	{
		devDebugPrint(DRIVER_NAME"*******ReadCaps..no media in device\n");
		SetSrbSenseCode(pSrb,NO_MEDIA_IN_DEVICE);
		return TRUE;
	}

	PREAD_CAPACITY_DATA pReadCapsData = (PREAD_CAPACITY_DATA)pSrb->DataBuffer;

	ULONG ulValue = pExt->m_ulBlockCount[pSrb->TargetId] - 1;
	REVERSE_LONG(&ulValue);
	pReadCapsData->LogicalBlockAddress = ulValue;

	ulValue = 1 << pExt->m_ulBlockShift[pSrb->TargetId];
	REVERSE_LONG(&ulValue);
	pReadCapsData->BytesPerBlock = ulValue;

	return TRUE;
}

// read 10
BOOLEAN Read10(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	if(!pExt->m_hFileHandle[pSrb->TargetId])
	{
		devDebugPrint(DRIVER_NAME"*******Read10..no media in device\n");
		SetSrbSenseCode(pSrb,NO_MEDIA_IN_DEVICE);
		return TRUE;
	}

	SwapReadWriteCBD10(pSrb);

	ULONG ulStart,ulLen;
	if(!CheckBound(pExt,pSrb,&ulStart,&ulLen))
	{
		devDebugPrint(DRIVER_NAME"*******Read10..check bound failed\n");
		return FALSE;
	}

	LARGE_INTEGER pos;
	pos.QuadPart = ulStart;
	pos.QuadPart <<= pExt->m_ulBlockShift[pSrb->TargetId];

	IO_STATUS_BLOCK ioStatus;
	NTSTATUS status = ZwReadFile(pExt->m_hFileHandle[pSrb->TargetId],NULL,NULL,NULL,&ioStatus,pSrb->DataBuffer,
								 pSrb->DataTransferLength,&pos,NULL);

	if(!NT_SUCCESS(status))
	{
		devDebugPrint(DRIVER_NAME"*******Read10..read image file failed\n");
		SetSrbSenseCode(pSrb,MEDIA_ERROR);
	}

	return TRUE;
}

// write 10
BOOLEAN Write10(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	if(!pExt->m_hFileHandle[pSrb->TargetId])
	{
		devDebugPrint(DRIVER_NAME"*******Write10..no media in device\n");
		SetSrbSenseCode(pSrb,NO_MEDIA_IN_DEVICE);
		return TRUE;
	}

	SwapReadWriteCBD10(pSrb);

	ULONG ulStart,ulLen;
	if(!CheckBound(pExt,pSrb,&ulStart,&ulLen))
	{
		devDebugPrint(DRIVER_NAME"*******Write10..check bound failed\n");
		return FALSE;
	}

	LARGE_INTEGER pos;
	pos.QuadPart = ulStart;
	pos.QuadPart <<= pExt->m_ulBlockShift[pSrb->TargetId];

	IO_STATUS_BLOCK ioStatus;
	NTSTATUS status = ZwWriteFile(pExt->m_hFileHandle[pSrb->TargetId],NULL,NULL,NULL,&ioStatus,pSrb->DataBuffer,
								  pSrb->DataTransferLength,&pos,NULL);

	if(!NT_SUCCESS(status))
	{
		devDebugPrint(DRIVER_NAME"*******Write10..read image file failed\n");
		SetSrbSenseCode(pSrb,MEDIA_ERROR);
	}

	return TRUE;
}

// seek
BOOLEAN Seek10(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)							
{
	return TRUE;
}

// write verify
BOOLEAN WriteVerify(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)						
{
	return TRUE;
}

// verify
BOOLEAN Verify(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)							
{
	return TRUE;
}

// synchronize cache
BOOLEAN SynchronizeCache(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)				
{
	return TRUE;
}

// read6
BOOLEAN Read6(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	// convert to 10
	ConvertCdb6To10(pSrb);

	// read 10
	return Read10(pExt,pSrb);
}

// the same
BOOLEAN Write6(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	ConvertCdb6To10(pSrb);
	return Write10(pExt,pSrb);
}

namespace
{
	typedef BOOLEAN (*fnScsiCmdHandler)(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb);

	fnScsiCmdHandler g_pfnScsiCmdHandler[] = 
	{
		TestUnitReady,									// test unit ready = 0
		RezeroUnit,										// rezero unit = 1
		InvalidScsiCmd,									// vendor specific = 2
		RequestSense,									// request sense = 3
		FormatUnit,										// format unit = 4
		InvalidScsiCmd,									// read block limits = 5
		InvalidScsiCmd,									// vendor specific = 6
		InvalidScsiCmd,									// initialize element status = 7
		Read6,											// read6 = 8
		InvalidScsiCmd,									// vendor specific = 9
		Write6,											// write6 = A
		Seek6,											// seek6 = B
		InvalidScsiCmd,									// vendor specific = C
		InvalidScsiCmd,									// vendor specific = D
		InvalidScsiCmd,									// vendor specific = E
		InvalidScsiCmd,									// vendor specific = F

		InvalidScsiCmd,									// synchronize buffer = 10
		InvalidScsiCmd,									// space = 11
		Inquiry,										// inquiry = 12
		InvalidScsiCmd,									// verify = 13
		InvalidScsiCmd,									// recover buffered data = 14
		InvalidScsiCmd,									// mode select6 = 15
		Reserve,										// reserve = 16
		Release,										// release = 17
		InvalidScsiCmd,									// copy = 18
		InvalidScsiCmd,									// erase = 19
		InvalidScsiCmd,									// mode sense6 = 1A
		StartStopUnit,									// stop start unit = 1B
		InvalidScsiCmd,									// receive diagnostic results = 1c
		SendDiagnostic,									// send diagnostic = 1d
		InvalidScsiCmd,									// prevent allow medium removal = 1e
		InvalidScsiCmd,									// 1f

		InvalidScsiCmd,									// vendor specific = 20
		InvalidScsiCmd,									// vendor specific = 21
		InvalidScsiCmd,									// vendor specific = 22
		InvalidScsiCmd,									// vendor specific = 23
		InvalidScsiCmd,									// set window = 24
		ReadCaps,										// read caps = 25
		InvalidScsiCmd,									// vendor specific = 26
		InvalidScsiCmd,									// vendor specific = 27
		Read10,											// read10 = 28
		InvalidScsiCmd,									// read generation = 29
		Write10,										// write10 = 2A
		Seek10,											// seek10 = 2B
		InvalidScsiCmd,									// erase10 = 2C
		InvalidScsiCmd,									// read updated block = 2D
		WriteVerify,									// write and verify = 2E
		Verify,											// verify = 2F
		
		InvalidScsiCmd,									// search data high = 30
		InvalidScsiCmd,									// search data equal = 31
		InvalidScsiCmd,									// search data low = 32
		InvalidScsiCmd,									// set limits = 33
		InvalidScsiCmd,									// prefetch = 34
		SynchronizeCache,								// synchronize cache = 35
		InvalidScsiCmd,									// lock unlock cache = 36
		InvalidScsiCmd,									// read defect data = 37
		InvalidScsiCmd,									// medium scan = 38
		InvalidScsiCmd,									// compare = 39
		InvalidScsiCmd,									// copy and verify = 3A
		InvalidScsiCmd,									// write buffer = 3B
		InvalidScsiCmd,									// read buffer = 3C
		InvalidScsiCmd,									// update block = 3D
		InvalidScsiCmd,									// read long = 3E
		InvalidScsiCmd,									// write long = 3F

		InvalidScsiCmd,									// change definition = 40
		InvalidScsiCmd,									// write same = 41
		InvalidScsiCmd,									// read sub channel = 42
		InvalidScsiCmd,									// read toc = 43
		InvalidScsiCmd,									// read header = 44
		InvalidScsiCmd,									// read audio10 = 45
		InvalidScsiCmd,									// 46
		InvalidScsiCmd,									// play audio smf = 47
		InvalidScsiCmd,									// play audio track index = 48
		InvalidScsiCmd,									// play audio relative10 = 49
		InvalidScsiCmd,									// 4A
		InvalidScsiCmd,									// pause resume = 4B
		InvalidScsiCmd,									// log select = 4C
		InvalidScsiCmd,									// log sense = 4D
		InvalidScsiCmd,									// 4E
		InvalidScsiCmd,									// 4F
	
		InvalidScsiCmd,									// 50
		InvalidScsiCmd,									// 51
		InvalidScsiCmd,									// 52
		InvalidScsiCmd,									// 53
		InvalidScsiCmd,									// 54
		InvalidScsiCmd,									// mode select10 = 55
		InvalidScsiCmd,									// 56
		InvalidScsiCmd,									// 57
		InvalidScsiCmd,									// 58
		InvalidScsiCmd,									// 59
		InvalidScsiCmd,									// mode sense10 = 5A
		InvalidScsiCmd,									// 5B
		InvalidScsiCmd,									// 5C
		InvalidScsiCmd,									// 5D
		InvalidScsiCmd,									// 5E
		InvalidScsiCmd,									// 5F
	};

	#define MAX_SCSI_CMD (sizeof(g_pfnScsiCmdHandler)/sizeof(fnScsiCmdHandler))

	UCHAR g_ucSenseCodes[][5] = 
	{
		/*INVALID_REQUEST*/		SRB_STATUS_ERROR+SRB_STATUS_AUTOSENSE_VALID,SCSISTAT_CHECK_CONDITION,SCSI_SENSE_ILLEGAL_REQUEST,SCSI_ADSENSE_ILLEGAL_COMMAND,	0,
		/*DEVICE_NOT_READY*/	SRB_STATUS_ERROR+SRB_STATUS_AUTOSENSE_VALID,SCSISTAT_CHECK_CONDITION,SCSI_SENSE_NOT_READY,		SCSI_ADSENSE_LUN_NOT_READY,		SCSI_SENSEQ_MANUAL_INTERVENTION_REQUIRED,
		/*NO_MEDIA_IN_DEVICE*/	SRB_STATUS_ERROR+SRB_STATUS_AUTOSENSE_VALID,SCSISTAT_CHECK_CONDITION,SCSI_SENSE_NOT_READY,		SCSI_ADSENSE_NO_MEDIA_IN_DEVICE,0,
		/*MEDIA_ERROR*/			SRB_STATUS_ERROR+SRB_STATUS_AUTOSENSE_VALID,SCSISTAT_CHECK_CONDITION,SCSI_SENSE_MEDIUM_ERROR,	0x10,							0,
		/*INVALID_SUB_REQUEST*/	SRB_STATUS_ERROR+SRB_STATUS_AUTOSENSE_VALID,SCSISTAT_CHECK_CONDITION,SCSI_SENSE_ILLEGAL_REQUEST,SCSI_ADSENSE_INVALID_CDB,		0,
		/*OUT_BOUND_ACCESS*/	SRB_STATUS_ERROR+SRB_STATUS_AUTOSENSE_VALID,SCSISTAT_CHECK_CONDITION,SCSI_SENSE_ILLEGAL_REQUEST,SCSI_ADSENSE_ILLEGAL_BLOCK,		0,
	};
}

// process io control
BOOLEAN DoIoControl(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	PSRB_IO_CONTROL pIoCtl = static_cast<PSRB_IO_CONTROL>(pSrb->DataBuffer);

	//ULONG ulCode = pIoCtl->ControlCode;

	//pSrb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

	//if(RtlCompareMemory(pIoCtl->Signature,MINIPORTSIGN,sizeof(pIoCtl->Signature)) == sizeof(pIoCtl->Signature))
	//{
	//	switch(ulCode)
	//	{
	//		// check present
	//	case IOCTL_MINIPORT_GET_VERSION:
	//		if(pSrb->DataTransferLength >= sizeof(SRB_IO_CONTROL) + sizeof(USHORT))
	//		{
	//			*reinterpret_cast<PUSHORT>(pIoCtl + 1) = 0x1010;
	//			pSrb->SrbStatus = SRB_STATUS_SUCCESS;
	//		}
	//		else
	//		{
	//			pSrb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
	//		}
	//		break;

	//	case IOCTL_MINIPORT_MOUNT_IMAGE:
	//		{
	//			if(pExt->m_hFileHandle)
	//				pSrb->SrbStatus = SRB_STATUS_BUSY;
	//			else
	//			{
	//				if(MountImage(pExt,L"\\DosDevices\\c:\\pal.iso"))
	//					pSrb->SrbStatus = SRB_STATUS_SUCCESS;
	//				else
	//					pSrb->SrbStatus = SRB_STATUS_ERROR;
	//			}
	//		}
	//		break;

	//	case IOCTL_MINIPORT_UNMOUNT_IMAGE:
	//		{
	//			if(!pExt->m_hFileHandle)
	//				pSrb->SrbStatus = SRB_STATUS_ABORTED;
	//			else
	//			{
	//				pExt->m_bEjectIrpForUnmount = TRUE;

	//				// remove entry
	//				PLIST_ENTRY pltEntry = ExInterlockedRemoveHeadList(&pExt->m_ltRequestHead,&pExt->m_ltRequestLock);

	//				while(pltEntry)
	//				{
	//					// get srb pointer
	//					PSCSI_REQUEST_BLOCK pSrb = (CONTAINING_RECORD(pltEntry,SrbExt,m_ltEntry))->m_pSrb;

	//					// process it
	//					ProcessSrb(pExt,pSrb);

	//					// next entry
	//					pltEntry = ExInterlockedRemoveHeadList(&pExt->m_ltRequestHead,&pExt->m_ltRequestLock);
	//				}

	//				ZwClose(pExt->m_hFileHandle);
	//				pExt->m_hFileHandle = NULL;

	//				pExt->m_bEjectIrpForUnmount = FALSE;
	//			}
	//		}
	//		break;

	//	case IOCTL_MINIPORT_IS_MOUNTED:
	//		if(pSrb->DataTransferLength >= sizeof(SRB_IO_CONTROL) + sizeof(ULONG))
	//		{
	//			*reinterpret_cast<PULONG>(pIoCtl + 1) = pExt->m_hFileHandle != NULL;
	//			pSrb->SrbStatus = SRB_STATUS_SUCCESS;
	//		}
	//		else
	//		{
	//			pSrb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
	//		}
	//		break;
	//	}
	//}

	devDebugPrint(DRIVER_NAME"*******DeviceIoControl,code 0x%x\n",pIoCtl->ControlCode);

	return TRUE;
}

// process scsi cmd
BOOLEAN DoScsiCmd(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	if(pSrb->Cdb[0] > MAX_SCSI_CMD)
	{
		devDebugPrint(DRIVER_NAME"*******ScsiCmd : unknown(0x%x)\n",pSrb->Cdb[0]);
		return InvalidScsiCmd(pExt,pSrb);
	}

#ifdef DBG
	static PCHAR szScsiCmd[] = 
	{
		"test unit ready = 0",
		"rezero unit = 1",
		"vendor specific = 2",
		"request sense = 3",
		"format unit = 4",
		"read block limits = 5",
		"vendor specific = 6",
		"initialize element status = 7",
		NULL,//"read6 = 8",
		"vendor specific = 9",
		NULL,//"write6 = A",
		NULL,//"seek6 = B",
		"vendor specific = C",
		"vendor specific = D",
		"vendor specific = E",
		"vendor specific = F",

		"synchronize buffer = 10",
		"space = 11",
		"inquiry = 12",
		"verify = 13",
		"recover buffered data = 14",
		"mode select6 = 15",
		"reserve = 16",
		"release = 17",
		"copy = 18",
		"erase = 19",
		"mode sense6 = 1A",
		"stop start unit = 1B",
		"receive diagnostic results = 1C",
		"send diagnostic = 1D",
		"prevent allow medium removal = 1E",
		"1F",
		
		"vendor specific = 20",
		"vendor specific = 21",
		"vendor specific = 22",
		"vendor specific = 23",
		"set window = 24",
		"read caps = 25",
		"vendor specific = 26",
		"vendor specific = 27",
		NULL,//"read10 = 28",
		"read generation = 29",
		NULL,//"write10 = 2A",
		NULL,//"seek10 = 2B",
		"erase10 = 2C",
		"read updated block = 2D",
		"write and verify = 2E",
		"verify = 2F",

		"search data high = 30",
		"search data equal = 31",
		"search data low = 32",
		"set limits = 33",
		"prefetch = 34",
		"synchronize cache = 35",
		"lock unlock cache = 36",
		"read defect data = 37",
		"medium scan = 38",
		"compare = 39",
		"copy and verify = 3A",
		"write buffer = 3B",
		"read buffer = 3C",
		"update block = 3D",
		"read long = 3E",
		"write long = 3F",

		"change definition = 40",
		"write same = 41",
		"read sub channel = 42",
		"read toc = 43",
		"read header = 44",
		"read audio10 = 45",
		"46",
		"play audio smf = 47",
		"play audio track index = 48",
		"play audio relative10 = 49",
		"4A",
		"pause resume = 4B",
		"log select = 4C",
		"log sense = 4D",
		"4E",
		"4F",

		"50",
		"51",
		"52",
		"53",
		"54",
		"mode select10 = 55",
		"56",
		"57",
		"58",
		"59",
		"mode sense10 = 5A",
		"5B",
		"5C",
		"5D",
		"5E",
		"5F",
	};

	//if(g_pfnScsiCmdHandler[pSrb->Cdb[0]] == InvalidScsiCmd)
	if(szScsiCmd[pSrb->Cdb[0]])
		devDebugPrint(DRIVER_NAME"*******ScsiCmd : %s\n",szScsiCmd[pSrb->Cdb[0]]);	
#endif

	return g_pfnScsiCmdHandler[pSrb->Cdb[0]](pExt,pSrb);
}

// process srb
BOOLEAN ProcessSrb(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb)
{
	BOOLEAN bRet = TRUE;
	pSrb->SrbStatus = SRB_STATUS_SUCCESS;
	pSrb->ScsiStatus = SCSISTAT_GOOD;
	switch(pSrb->Function)
	{
	case SRB_FUNCTION_SHUTDOWN:
		devDebugPrint(DRIVER_NAME"*******SRB_FUNCTION_SHUTDOWN\n");
		break;

	case SRB_FUNCTION_FLUSH:
		devDebugPrint(DRIVER_NAME"*******SRB_FUNCTION_FLUSH\n");
		break;

	case SRB_FUNCTION_ABORT_COMMAND:
		pSrb->SrbStatus = SRB_STATUS_ABORT_FAILED;
		devDebugPrint(DRIVER_NAME"*******SRB_FUNCTION_ABORT_COMMAND\n");
		break;

	case SRB_FUNCTION_RESET_BUS:
		devDebugPrint(DRIVER_NAME"*******SRB_FUNCTION_RESET_BUS\n");
		break;

	case SRB_FUNCTION_EXECUTE_SCSI:
		bRet = DoScsiCmd(pExt,pSrb);
		break;

	case SRB_FUNCTION_IO_CONTROL:
		bRet = DoIoControl(pExt,pSrb);
		break;
	default:
		pSrb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		devDebugPrint(DRIVER_NAME"*******SRB_FUNCTION_unknow %d\n",pSrb->Function);
		break;
	}

	PSrbExt pSrbExt = static_cast<PSrbExt>(pSrb->SrbExtension);
	ExInterlockedInsertTailList(&g_ltFinishHead,&pSrbExt->m_ltEntry,&g_ltFinishLock);

	return bRet;
}

// set sense code
VOID SetSrbSenseCode(PSCSI_REQUEST_BLOCK pSrb,ULONG ulErrorCode,...)
{
	PUCHAR pucSense = &g_ucSenseCodes[ulErrorCode][0];
	UCHAR  ucLen;
	va_list list;

	va_start(list, ulErrorCode);

	ucLen = pSrb->SenseInfoBufferLength;

	pSrb->SrbStatus  = pucSense[0];
	pSrb->ScsiStatus = pucSense[1];

	PSENSE_DATA pSenseData = static_cast<PSENSE_DATA>(pSrb->SenseInfoBuffer);

	if(ucLen > 0)
	{
		RtlZeroMemory(pSenseData, ucLen);

		pSenseData->ErrorCode = 0x70;

		if(ucLen >  2)
			pSenseData->SenseKey = pucSense[2];

		if(ucLen >  7)
			pSenseData->AdditionalSenseLength = 6;

		if(ucLen > 12)
			pSenseData->AdditionalSenseCode = pucSense[3];

		if(ucLen > 13)
			pSenseData->AdditionalSenseCodeQualifier = pucSense[4];
	};

	va_end(list);
}