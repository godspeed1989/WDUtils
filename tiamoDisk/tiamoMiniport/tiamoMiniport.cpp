
//**************************************************************************************
//	日期:	23:2:2004   
//	创建:	tiamo	
//	描述:   exminiport
//**************************************************************************************

#include "stdafx.h"

#pragma alloc_text(INIT,DriverEntry)

KEVENT								g_evRequestArrival;
KEVENT								g_evShutdown;
KEVENT								g_evShutdownFinish;
HANDLE								g_hWorkerThread;
LIST_ENTRY							g_ltRequestHead;
KSPIN_LOCK							g_ltRequestLock;
LIST_ENTRY							g_ltFinishHead;
KSPIN_LOCK							g_ltFinishLock;

VOID (*pOldDriverUnload)(PDRIVER_OBJECT pDriver);

ULONG DriverEntry(PDRIVER_OBJECT pDriver,PVOID pRegPath)
{
	HW_INITIALIZATION_DATA hwInitData = {0};

	// Internal interface
	hwInitData.AdapterInterfaceType = Internal;

	// auto request sense
	hwInitData.AutoRequestSense = TRUE;

	// device extension
	hwInitData.DeviceExtensionSize = sizeof(MiniportExt);

	// adapter control
	hwInitData.HwAdapterControl = MiniportAdapterControl;

	// adapter state
	hwInitData.HwAdapterState = MiniportAdapterState;

	// find adapter
	hwInitData.HwFindAdapter = MiniportFindAdapter;

	// sizeof this struct
	hwInitData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

	// initialize
	hwInitData.HwInitialize = MiniportInitialize;

	// reset bus
	hwInitData.HwResetBus = MiniportResetBus;

	// start io
	hwInitData.HwStartIo = MiniportStartIo;

	// need virtual address
	hwInitData.MapBuffers = TRUE;

	// no multi request
	hwInitData.MultipleRequestPerLu = FALSE;

	// do not use dma
	hwInitData.NeedPhysicalAddresses = FALSE;
	hwInitData.NumberOfAccessRanges = 0;
	hwInitData.ReceiveEvent = FALSE;
	hwInitData.SpecificLuExtensionSize = 0;
	hwInitData.SrbExtensionSize = sizeof(SrbExt);

	// do no support tagged queuing
	hwInitData.TaggedQueuing = FALSE;

	// initialize notification event
	KeInitializeEvent(&g_evShutdown,NotificationEvent,FALSE);
	KeInitializeEvent(&g_evShutdownFinish,NotificationEvent,FALSE);

	KeInitializeEvent(&g_evRequestArrival,SynchronizationEvent,FALSE);

	InitializeListHead(&g_ltRequestHead);
	KeInitializeSpinLock(&g_ltRequestLock);

	InitializeListHead(&g_ltFinishHead);
	KeInitializeSpinLock(&g_ltFinishLock);
	
	// create system thread
	devDebugPrint(DRIVER_NAME"*******DriverEntry...create system thread\n");
	NTSTATUS status = PsCreateSystemThread( &g_hWorkerThread,THREAD_ALL_ACCESS,NULL,NULL,NULL,
											MiniportRequestProcessorWorker,NULL);

	if(NT_SUCCESS(status))
	{
		devDebugPrint(DRIVER_NAME"*******DriverEntry...call ScsiPortInitialize\n");
		status = ScsiPortInitialize(pDriver,pRegPath,&hwInitData,NULL);

		devDebugPrint(DRIVER_NAME"*******DriverEntry..setup driver unload function pointer\n");
		pOldDriverUnload = pDriver->DriverUnload;
		pDriver->DriverUnload = DriverUnload;
	}

	return status;
}

// driver unload
VOID DriverUnload(PDRIVER_OBJECT pDriver)
{
	if(pOldDriverUnload)
	{
		devDebugPrint(DRIVER_NAME"*******DriverUnload...call ScsiportUnload.\n");
		pOldDriverUnload(pDriver);
	}
	else
	{
		devDebugPrint(DRIVER_NAME"*******DriverUnload...why old unload routine is null?\n");
	}

	devDebugPrint(DRIVER_NAME"*******DriverUnload...set shut down event\n");

	KeSetEvent(&g_evShutdown,IO_NO_INCREMENT,FALSE);

	devDebugPrint(DRIVER_NAME"*******DriverUnload...wait for the work thread terminate.\n");

	KeWaitForSingleObject(&g_evShutdownFinish,Executive,KernelMode,FALSE,NULL);

	devDebugPrint(DRIVER_NAME"*******DriverUnload...the work thread terminated.then driver image can be safe unload.\n");
}

// miniport adapter state
BOOLEAN MiniportAdapterState(PVOID pDeviceExt,PVOID pContext,BOOLEAN bSaveState)
{
	devDebugPrint(DRIVER_NAME"*******MiniportAdapterState...just return TRUE\n");
	return TRUE;
}

// miniport initialize
BOOLEAN MiniportInitialize(PVOID pDeviceExt)
{
	devDebugPrint(DRIVER_NAME"*******MiniportInitialize...just set mount event,and wait for 5 seconds to let work thread do all the thing.\n");

	PMiniportExt pExt = static_cast<PMiniportExt>(pDeviceExt);

	LARGE_INTEGER timeout;
	timeout.QuadPart = -50000000;

	KEVENT evFinish;
	KeInitializeEvent(&evFinish,NotificationEvent,FALSE);

	ThreadContext context;
	context.m_pFinishEvent = &evFinish;
	context.m_pMiniportExt = static_cast<PMiniportExt>(pDeviceExt);
	context.m_ulType = Mount;
	ExInterlockedInsertTailList(&g_ltRequestHead,&context.m_ltEntry,&g_ltRequestLock);

	KeSetEvent(&g_evRequestArrival,IO_NO_INCREMENT,FALSE);

	NTSTATUS status = KeWaitForSingleObject(&evFinish,Executive,KernelMode,FALSE,&timeout);

	if(status == STATUS_SUCCESS && context.m_retStatus == STATUS_SUCCESS)
	{
		devDebugPrint(DRIVER_NAME"*******MiniportInitialize...work thread finished successfully,then set timer,and return true.\n");
		ScsiPortNotification(RequestTimerCall,pDeviceExt,MiniportCheckFinishTimer,MINIPORT_TIMER_PERIOD);
		return TRUE;
	}

	devDebugPrint(DRIVER_NAME"*******MiniportInitialize...work thread met an error %d,so return true.\n",status);

	return FALSE;
}

// start io
BOOLEAN MiniportStartIo(PVOID pDeviceExt,PSCSI_REQUEST_BLOCK pSrb)
{
	pSrb->SrbStatus = SRB_STATUS_PENDING;
	PMiniportExt pExt = static_cast<PMiniportExt>(pDeviceExt);

	if(KeGetCurrentIrql() > DISPATCH_LEVEL)
	{
		devDebugPrint(DRIVER_NAME"*******MiniportStartIo...this should not happen.\n");
		SetSrbSenseCode(pSrb,DEVICE_NOT_READY);
	}
	else
	{
		PSrbExt pSrbExt = static_cast<PSrbExt>(pSrb->SrbExtension);
		pSrbExt->m_pSrb = pSrb;
		pSrbExt->m_ulType = Normal;

		ExInterlockedInsertTailList(&g_ltRequestHead,&pSrbExt->m_ltEntry,&g_ltRequestLock);

		KeSetEvent(&g_evRequestArrival,IO_NO_INCREMENT,FALSE);
	}

	return TRUE;
}

namespace
{
	BOOLEAN g_bFound = FALSE;
}

// find adapter
ULONG MiniportFindAdapter(PVOID pDeviceExt,PVOID pHwContext,PVOID pBusInformation,PCHAR pArgumentString,PPORT_CONFIGURATION_INFORMATION pConfigInfo,PBOOLEAN pAgain)
{
	*pAgain = FALSE;

	if(g_bFound)
	{
		devDebugPrint(DRIVER_NAME"*******MiniportFindAdapter...already has an adapter..this call failed.\n");
		return SP_RETURN_NOT_FOUND;
	}

	g_bFound = TRUE;

	pConfigInfo->AlignmentMask = 0x00000003;
	pConfigInfo->AutoRequestSense = TRUE;
	pConfigInfo->BufferAccessScsiPortControlled = FALSE;
	pConfigInfo->BusInterruptLevel = 0;
	pConfigInfo->BusInterruptVector = 0;
	pConfigInfo->Dma32BitAddresses = TRUE;
	pConfigInfo->Master = FALSE;
	pConfigInfo->CachesData = TRUE;
	pConfigInfo->NumberOfBuses = 1;
	pConfigInfo->MaximumNumberOfTargets = 4;
	pConfigInfo->MaximumTransferLength = 0x10000;
	pConfigInfo->MultipleRequestPerLu = FALSE;
	pConfigInfo->NumberOfPhysicalBreaks = 8;
	pConfigInfo->ScatterGather = TRUE;
	pConfigInfo->TaggedQueuing = FALSE;

	devDebugPrint(DRIVER_NAME"*******MiniportFindAdapter...found an adapter\n");
	return SP_RETURN_FOUND;
}

// reset bus
BOOLEAN MiniportResetBus(PVOID pDeviceExtension,ULONG ulPathId)
{
	return TRUE;
}

// miniport adapter control
SCSI_ADAPTER_CONTROL_STATUS MiniportAdapterControl(PVOID pDeviceExt,SCSI_ADAPTER_CONTROL_TYPE ctlType,PVOID pParameters)
{
	SCSI_ADAPTER_CONTROL_STATUS status = ScsiAdapterControlSuccess;
	PMiniportExt pExt = static_cast<PMiniportExt>(pDeviceExt);

	switch(ctlType)
	{
	case ScsiQuerySupportedControlTypes:
		{
			PSCSI_SUPPORTED_CONTROL_TYPE_LIST pList = static_cast<PSCSI_SUPPORTED_CONTROL_TYPE_LIST>(pParameters);
			pList->SupportedTypeList[ScsiStopAdapter] = TRUE;
			pList->SupportedTypeList[ScsiRestartAdapter] = TRUE;
			pList->SupportedTypeList[ScsiQuerySupportedControlTypes] = TRUE;
			devDebugPrint(DRIVER_NAME"*******MiniportAdapterControl...ScsiQuerySupportedControlTypes(stop,restart,query)\n");
		}
		break;

	case ScsiStopAdapter:
		devDebugPrint(DRIVER_NAME"*******MiniportAdapterControl...ScsiStopAdapter,do nothing\n");
		break;

	case ScsiRestartAdapter:
		devDebugPrint(DRIVER_NAME"*******MiniportAdapterControl...ScsiRestartAdapter,must set timer\n");
		ScsiPortNotification(RequestTimerCall,pDeviceExt,MiniportCheckFinishTimer,MINIPORT_TIMER_PERIOD);
		break;

	default:
		status = ScsiAdapterControlUnsuccessful;
		break;
	}

	return status;
}

// check finish timer
VOID MiniportCheckFinishTimer(PVOID pDeviceExt)
{
	if(DoCheckFinish(pDeviceExt))
	{
		ScsiPortNotification(RequestTimerCall,pDeviceExt,MiniportCheckFinishTimer,MINIPORT_TIMER_PERIOD);
	}
	else
	{
		devDebugPrint(DRIVER_NAME"*******MiniportCheckFinishTimer...DoCheckFinish return false,do not request another timer call\n");
	}
}