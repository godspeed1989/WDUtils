
//**************************************************************************************
//	日期:	1:7:2004   
//	创建:	tiamo	
//	描述:	miniport helper
//**************************************************************************************

#include "stdafx.h"
#include "../public.h"

// check finish timer call
BOOLEAN DoCheckFinish(PVOID pDeviceExt)
{
	PMiniportExt pExt = static_cast<PMiniportExt>(pDeviceExt);

	// remove entry
	PLIST_ENTRY pltEntry = ExInterlockedRemoveHeadList(&g_ltFinishHead,&g_ltFinishLock);

	BOOLEAN bRet = TRUE;

	while(pltEntry)
	{
		PThreadContext pContext = CONTAINING_RECORD(pltEntry,ThreadContext,m_ltEntry);
		
		switch(pContext->m_ulType)
		{
		case Normal:
			FinishRequest(pExt,pContext->m_pSrb);
			break;

		case Shutdown:
			devDebugPrint(DRIVER_NAME"*******DoCheckFinish meets a shutdown request,then return false.this will end the timer call.\n");
			bRet = FALSE;
			break;
		}
		
		// next entry
		pltEntry = ExInterlockedRemoveHeadList(&g_ltFinishHead,&g_ltFinishLock);
	}

	return bRet;
}

// complete the request
VOID FinishRequest(PVOID pDeviceExt,PSCSI_REQUEST_BLOCK pSrb)
{
	ScsiPortNotification(RequestComplete,pDeviceExt,pSrb);

	ScsiPortNotification(NextRequest,pDeviceExt,NULL);
}

// worker thread function
VOID MiniportRequestProcessorWorker(PVOID pContext)
{
	// set priority
	KeSetPriorityThread(KeGetCurrentThread(),LOW_REALTIME_PRIORITY);

	PVOID pObject[2] = {&g_evRequestArrival,&g_evShutdown};
	PMiniportExt pExt = NULL;
	
	for(;;)
	{
		BOOLEAN bBreak = FALSE;

		// wait any one of the 2 events to become signaled
		NTSTATUS status = KeWaitForMultipleObjects(2,pObject,WaitAny,Executive,KernelMode,FALSE,NULL,NULL);
	
		switch(status)
		{
			// request arrival
		case 0:
			{
				// remove entry
				PLIST_ENTRY pltEntry = ExInterlockedRemoveHeadList(&g_ltRequestHead,&g_ltRequestLock);

				while(pltEntry)
				{
					// get the action context
					PThreadContext pContext = CONTAINING_RECORD(pltEntry,ThreadContext,m_ltEntry);

					switch(pContext->m_ulType)
					{
						// normal srb
					case Normal:
						{
							PSCSI_REQUEST_BLOCK pSrb = pContext->m_pSrb;

							// process it
							if(!ProcessSrb(pExt,pSrb))
								devDebugPrint(DRIVER_NAME"*******MiniportRequestProcessorWorker...process return false\n");
						}
						break;

						// shut down request
					case Shutdown:
						bBreak = TRUE;
						break;

						// mount request
					case Mount:
						{
							// check fdo ext
							if(pExt)
								devDebugPrint(DRIVER_NAME"*******MiniportRequestProcessorWorker...this should not happen,"
														 "pExt is not NULL\n");

							pExt = pContext->m_pMiniportExt;

							// mount image
							pContext->m_retStatus = MountImage(pExt);

							// set finish event
							KeSetEvent(pContext->m_pFinishEvent,IO_NO_INCREMENT,FALSE);
						}
						break;
					}

					pltEntry = ExInterlockedRemoveHeadList(&g_ltRequestHead,&g_ltRequestLock);
				}
			}
			break;

			// shutdown event
		case 1:
			bBreak = TRUE;
			break;
		}

		// close file handle
		if(bBreak)
		{
			if(pExt)
			{
				// close the file
				ULONG i;
				for(i = 0;i < 4; i ++)
				{
					if(pExt->m_hFileHandle[i])
					{
						ZwClose(pExt->m_hFileHandle[i]);
						pExt->m_hFileHandle[i] = NULL;
					}
				}
			}
			break;
		}
	}

	devDebugPrint(DRIVER_NAME"*******MiniportRequestProcessorWorker...terminate worker thread\n");

	KeSetEvent(&g_evShutdownFinish,IO_NO_INCREMENT,FALSE);
	PsTerminateSystemThread(STATUS_SUCCESS);
}

// mount image file
NTSTATUS MountImage(PMiniportExt pExt)
{
	PDEVICE_OBJECT pBusFdo;
	PFILE_OBJECT pFileObj;

	UNICODE_STRING busFdoName;
	RtlInitUnicodeString(&busFdoName,BUS_FDO_NAME);

	// get bus fdo object
	NTSTATUS status = IoGetDeviceObjectPointer(&busFdoName,FILE_ALL_ACCESS,&pFileObj,&pBusFdo);

	if(NT_SUCCESS(status))
	{
		// add ref
		ObReferenceObject(pBusFdo);

		// dec file object's ref
		if(pFileObj)
			ObDereferenceObject(pFileObj);

		IO_STATUS_BLOCK ioStatus;
		KEVENT event;
		KeInitializeEvent(&event,NotificationEvent,FALSE);

		// build io control irp
		PIRP pIrp = IoBuildDeviceIoControlRequest(IOCTL_TIAMO_BUS_MINIPORT_GET_CONFIG,pBusFdo,&pExt->m_ulDevices,
												  sizeof(MiniportConfig),&pExt->m_ulDevices,sizeof(MiniportConfig),
												  FALSE,&event,&ioStatus);
		if(pIrp)
		{
			// send irp and wait
			status = IoCallDriver(pBusFdo,pIrp);
			if(status == STATUS_PENDING)
			{
				KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
				status = ioStatus.Status;
			}

			if(NT_SUCCESS(status))
			{
				// open those files
				ULONG i;
				for(i = 0; i < pExt->m_ulDevices; i ++)
				{
					UNICODE_STRING fileName;
					RtlInitUnicodeString(&fileName,pExt->m_szImageFileName[i]);
					OBJECT_ATTRIBUTES oa;
					InitializeObjectAttributes(&oa,&fileName,OBJ_CASE_INSENSITIVE,NULL,NULL);

					status = ZwCreateFile( &pExt->m_hFileHandle[i],SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,&oa,
										   &ioStatus,NULL,FILE_ATTRIBUTE_NORMAL,0,FILE_OPEN,
										   FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);

					if(NT_SUCCESS(status))
					{
						// query information
						FILE_STANDARD_INFORMATION info;
						status = ZwQueryInformationFile(pExt->m_hFileHandle[i],&ioStatus,&info,sizeof(info),FileStandardInformation);

						if(NT_SUCCESS(status))
						{
							pExt->m_ulBlockShift[i] = 9;
							pExt->m_ulBlockCount[i] = static_cast<ULONG>(info.EndOfFile.QuadPart / (1<<pExt->m_ulBlockShift[i]));
						}
						else
						{
							ZwClose(pExt->m_hFileHandle[i]);
							pExt->m_hFileHandle[i] = NULL;
						}
					}
				}
			}
		}

		// def bus fdo
		ObDereferenceObject(pBusFdo);
	}

	
	return status;
}