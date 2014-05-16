
//**************************************************************************************
//	日期:	23:2:2004   
//	创建:	tiamo	
//	描述:	stdafx
//**************************************************************************************

#pragma once

extern "C"
{
	#include "ntddk.h"

	#define NTSTRSAFE_LIB
	#include <ntstrsafe.h>
}

#if DBG
	#define devDebugPrint DbgPrint
#else
	#define devDebugPrint __noop
#endif

// common device extension 
typedef struct __tagCommonExt
{
	ULONG						m_bFdo;							// indicate whether this is a fdo or pdo
	ULONG						m_ulCurrentPnpState;			// current pnp state
	ULONG						m_ulPrevPnpState;				// prev pnp state
	SYSTEM_POWER_STATE			m_sysPowerState;				// system power state
	DEVICE_POWER_STATE			m_devPowerState;				// device power state
}CommonExt,*PCommonExt;

// device extension for fdo
struct FdoExt : CommonExt
{
	PDEVICE_OBJECT				m_pPhysicalDevice;				// pdo
	PDEVICE_OBJECT				m_pLowerDevice;					// lower device in the device stack
	ULONG						m_ulOutstandingIO;				// remove lock
	KEVENT						m_evStop;						// stop event
	KEVENT						m_evRemove;						// remove event
	FAST_MUTEX					m_mutexEnumPdo;					// mutex for access child pdo
	UNICODE_STRING				m_symbolicName;					// symbolic name
	PDEVICE_OBJECT				m_pEnumPdo;						// the child pdo
};
typedef FdoExt* PFdoExt;

// device extension for pdo
struct PdoExt : CommonExt
{
	ULONG						m_bPresent;						// is the device present
	ULONG						m_bReportMissing;				// we had reported the device is missing to the pnp manager
	PDEVICE_OBJECT				m_pParentFdo;					// parent fdo
	ULONG						m_ulDevices;					// how many child device we have (0 - 4)
	WCHAR						m_szImageFileName[4][256];		// image file name unicode
};

typedef PdoExt*	PPdoExt;

// macro for pnp state transform
#define SetNewPnpState(pExt,state) {pExt->m_ulPrevPnpState = pExt->m_ulCurrentPnpState;\
									pExt->m_ulCurrentPnpState = state;}

#define RestorePnpState(pExt) {pExt->m_ulCurrentPnpState = pExt->m_ulPrevPnpState;}

// device id
#define PDO_DEVICE_ID L"PCI\\tiamoport\0\0"
#define PDO_DEVICE_ID_LENGTH sizeof(PDO_DEVICE_ID)

// hardware id
#define PDO_HARDWARE_IDS L"*tiamoport\0PCI\\tiamoport\0\0"
#define PDO_HARDWARE_IDS_LENGTH sizeof(PDO_HARDWARE_IDS)

// compatible id
#define PDO_COMPATIBLE_IDS L"GEN_SCSIADAPTER\0\0"
#define PDO_COMPATIBLE_IDS_LENGTH sizeof(PDO_COMPATIBLE_IDS)

// device text
#define PDO_TEXT L"tiamo_bus_pdo\0"
#define PDO_TEXT_LENGTH sizeof(PDO_TEXT)

// driver name
#define DRIVER_NAME "tiamobus.sys"

extern "C"
{
	NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver,PUNICODE_STRING pRegPath);
	void DriverUnload(PDRIVER_OBJECT pDriver);

	NTSTATUS AddDevice(PDRIVER_OBJECT pDriver,PDEVICE_OBJECT pPhysicalDeviceObject);

	NTSTATUS DispatchCreateClose(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DispatchIoControl(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DispatchPnP(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DispatchPower(PDEVICE_OBJECT pDevice,PIRP pIrp);

	NTSTATUS SendIrpToLowerDeviceSyn(PDEVICE_OBJECT pDevice,PIRP pIrp);

	NTSTATUS DoFdoPnP(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DoPdoPnP(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS CompletionRoutine(PDEVICE_OBJECT pDevice,PIRP pIrp,PVOID pContext);
	NTSTATUS SendIrpToLowerDeviceSyn(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DispatchCreateClose(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS AddDevice(PDRIVER_OBJECT pDriver,PDEVICE_OBJECT pPhysicalDeviceObject);

	void PrintPnpCall(PDEVICE_OBJECT pDevice,PIRP pIrp);
	void DecIoCount(PFdoExt pExt);
	void IncIoCount(PFdoExt pExt);
}