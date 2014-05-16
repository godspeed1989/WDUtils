
//**************************************************************************************
//	日期:	23:2:2004   
//	创建:	tiamo	
//	描述:	stdafx
//**************************************************************************************

#pragma once

extern "C"
{
	#include <ntddk.h>
	#include <srb.h>
	#include <ntddscsi.h>
	#include <scsi.h>
	#include <stdarg.h>
}

#if DBG
#define devDebugPrint DbgPrint
#else
#define devDebugPrint __noop
#endif

#define MINIPORT_TIMER_PERIOD 100000
#define DRIVER_NAME "tiamominiport"

enum
{
	INVALID_REQUEST,
	DEVICE_NOT_READY,
	NO_MEDIA_IN_DEVICE,
	MEDIA_ERROR,
	INVALID_SUB_REQUEST,
	OUT_BOUND_ACCESS,
};

// miniport fdo extension
typedef struct __tagMiniportExt
{
	ULONG								m_ulDevices;				// how many targets
	WCHAR								m_szImageFileName[4][256];	// image file name

	HANDLE								m_hFileHandle[4];			// image file handle
	ULONG								m_ulBlockShift[4];			// block size
	ULONG								m_ulBlockCount[4];			// block count
}MiniportExt,*PMiniportExt;

// thread action
enum
{
	Mount,
	Normal,
	Shutdown,
};

// pass thread context
typedef struct __tagPassThreadContext
{
	LIST_ENTRY							m_ltEntry;					// linked list entry
	ULONG								m_ulType;					// action type
	union
	{
		// for nomal scsi request
		PSCSI_REQUEST_BLOCK				m_pSrb;

		// for mount image
		struct
		{
			PKEVENT						m_pFinishEvent;				// when we finish we must set the event
			NTSTATUS					m_retStatus;				// return status
			PMiniportExt				m_pMiniportExt;				// miniport fdo extension
		};
	};
}ThreadContext,SrbExt,*PThreadContext,*PSrbExt;

extern "C"
{
	ULONG DriverEntry(PDRIVER_OBJECT pDriver,PVOID pRegPath);

	VOID DriverUnload(PDRIVER_OBJECT pDriver);

	BOOLEAN MiniportInitialize(PVOID pDeviceExt);

	BOOLEAN MiniportStartIo(PVOID pDeviceExt,PSCSI_REQUEST_BLOCK pSrb);

	ULONG MiniportFindAdapter(PVOID pDeviceExt,PVOID pHwContext,PVOID pBusInformation,PCHAR pArgumentString,PPORT_CONFIGURATION_INFORMATION pConfigInfo,PBOOLEAN pAgain);

	BOOLEAN MiniportResetBus(PVOID pDeviceExtension,ULONG ulPathId);

	BOOLEAN MiniportAdapterState(PVOID pDeviceExt,PVOID pContext,BOOLEAN bSaveState);

	SCSI_ADAPTER_CONTROL_STATUS MiniportAdapterControl(PVOID pDeviceExt,SCSI_ADAPTER_CONTROL_TYPE ControlType,PVOID pParameters);

	BOOLEAN CreateWorkerThread(PVOID pDeviceExt);

	VOID MiniportCheckFinishTimer(PVOID pDeviceExt);

	BOOLEAN DoCheckFinish(PVOID pDeviceExt);

	VOID FinishRequest(PVOID pDeviceExt,PSCSI_REQUEST_BLOCK pSrb);

	VOID MiniportRequestProcessorWorker(PVOID pContext);

	VOID SetSrbSenseCode(PSCSI_REQUEST_BLOCK pSrb,ULONG ulErrorCode,...);

	BOOLEAN ProcessSrb(PMiniportExt pExt,PSCSI_REQUEST_BLOCK pSrb);

	NTSTATUS MountImage(PMiniportExt pExt);
}

extern KEVENT								g_evRequestArrival;
extern KEVENT								g_evShutdown;
extern KEVENT								g_evShutdownFinish;
extern HANDLE								g_hWorkerThread;
extern LIST_ENTRY							g_ltRequestHead;
extern KSPIN_LOCK							g_ltRequestLock;
extern LIST_ENTRY							g_ltFinishHead;
extern KSPIN_LOCK							g_ltFinishLock;