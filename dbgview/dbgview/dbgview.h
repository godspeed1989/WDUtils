
//**************************************************************************************
//	日期:	23:2:2004
//	创建:	tiamo
//	描述:	dbgview
//**************************************************************************************

#pragma once

#define DEV_NAME							L"\\Device\\DBGLOG"
#define DEV_SYM_NAME						L"\\??\\DBGLOG"
#define BUFFER_COUNT						10
#define BREAK_PRINT							1
#define DGB_PRINT_INT						0x2d

extern "C"
{
	NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver,PUNICODE_STRING pRegPath);
	VOID DriverUnload(PDRIVER_OBJECT pDriver);
	NTSTATUS DeviceOnCreate(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DeviceOnClose(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DeviceOnRead(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DeviceOnWrite(PDEVICE_OBJECT pDevice,PIRP pIrp);
	NTSTATUS DeviceOnIoControl(PDEVICE_OBJECT pDevice,PIRP pIrp);
	VOID BreakPrintInterrupt(PCHAR pString);
	VOID BreakPrintDpcForIsr(PKDPC Dpc,PDEVICE_OBJECT DeviceObject,PIRP Irp,PCHAR pString);
}

typedef CHAR StringBuffer[512];

typedef struct __tagDeviceExt
{
	PDEVICE_OBJECT pDevice;
	KSPIN_LOCK spinLock;
	PKEVENT pKEvent;
	StringBuffer ltBuffer[BUFFER_COUNT];
	ULONG ulCurrentWrite;
	ULONG ulCurrentRead;
}DeviceExt,*PDeviceExt;

NTSTATUS CompleteRequest(PIRP pIrp,NTSTATUS	status,ULONG_PTR pInfo);
