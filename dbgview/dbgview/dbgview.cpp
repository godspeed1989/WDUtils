
//**************************************************************************************
//	日期:	23:2:2004
//	创建:	tiamo
//	描述:   dbgview
//**************************************************************************************

#include "stdafx.h"
#include "dbgview.h"
#include "../controlcode.h"

#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(PAGE,DriverUnload)
#pragma alloc_text(PAGE,DeviceOnCreate)
#pragma alloc_text(PAGE,DeviceOnClose)
#pragma alloc_text(PAGE,DeviceOnRead)
#pragma alloc_text(PAGE,DeviceOnWrite)
#pragma alloc_text(PAGE,DeviceOnIoControl)

ULONG_PTR pOrgAddress = NULL;
PDeviceExt g_pDevExt = NULL;

typedef struct tagIDTENTRY
{
	USHORT OffsetLow;
	USHORT selector;
	UCHAR unused_lo;
	UCHAR unused_hi;
	USHORT OffsetHigh;
}IDTENTRY, *PIDTENTRY;

typedef struct tagIDTR
{
	USHORT IDTLimit;
	USHORT LowIDTbase;
	USHORT HiIDTbase;
}IDTR, *PIDTR;

BOOLEAN __declspec(naked) DisableInterrupt()
{
	__asm
	{
		pushfd
		pop     eax
		and     eax,0x200               ; (eax) = the interrupt bit
		shr     eax,9		            ; low bit of (eax) == interrupt bit
		cli
		ret
	}
}

VOID __declspec(naked) RestoreInterrupt(BOOLEAN bEnable)
{
	__asm
	{
		movzx   eax, byte ptr [esp + 4]
		shl     eax,9			            ; (eax) == the interrupt bit
		pushfd
		or      [esp],eax                   ; or EI into flags
		popfd
		ret
	}
}

VOID __declspec(naked) NewInterrupt()
{
	__asm
	{
		cmp		eax,BREAK_PRINT
		jnz		go_on

		pushad
		push	ecx									// ecx is the pointer
		call	BreakPrintInterrupt
		popad
go_on:
		jmp		[pOrgAddress]						// jmp to orignal handler
	}
}

// install new interrupt
VOID InstallNewInterrupt()
{
	IDTR idtr;

	BOOLEAN bEnabled = DisableInterrupt();
	//得到IDT基址（IDT：中断符号表）
	__asm sidt	fword ptr[idtr]

	ULONG_PTR ulEntry = static_cast<ULONG_PTR>(idtr.LowIDTbase + (idtr.HiIDTbase<<16)) + DGB_PRINT_INT * sizeof(IDTENTRY);
	PIDTENTRY pEntry = (PIDTENTRY)ulEntry;
	pOrgAddress = (pEntry->OffsetHigh << 16) + pEntry->OffsetLow;
	pEntry->OffsetHigh = static_cast<USHORT>((ULONG_PTR)(NewInterrupt) >> 16);
	pEntry->OffsetLow = static_cast<USHORT>((ULONG_PTR)(NewInterrupt));

	RestoreInterrupt(bEnabled);
}

// restore interrupt
VOID RestoreOrignalInterrupt()
{
	IDTR idtr;

	BOOLEAN bEnabled = DisableInterrupt();

	__asm sidt	fword ptr[idtr]

	ULONG_PTR ulEntry = static_cast<ULONG_PTR>(idtr.LowIDTbase + (idtr.HiIDTbase<<16)) + DGB_PRINT_INT * sizeof(IDTENTRY);
	PIDTENTRY pEntry = (PIDTENTRY)ulEntry;
	pEntry->OffsetHigh = static_cast<USHORT>(pOrgAddress >> 16);
	pEntry->OffsetLow = static_cast<USHORT>(pOrgAddress);

	RestoreInterrupt(bEnabled);
}

// interrupt handler for DbgPrint
VOID BreakPrintInterrupt(PCHAR pString)
{
	// safe to use xxx function
	if(KeGetCurrentIrql() <= DISPATCH_LEVEL)
	{
		BreakPrintDpcForIsr(NULL,NULL,NULL,pString);
	}
	else
	{
		// request dpc
		IoRequestDpc(g_pDevExt->pDevice,NULL,pString);
	}
}

// dpc for isr
VOID BreakPrintDpcForIsr(PKDPC Dpc,PDEVICE_OBJECT DeviceObject,PIRP Irp,PCHAR pString)
{
	// if Dpc is not null,it is DISPATCH_LEVEL
	if(Dpc)
	{
		KeAcquireSpinLockAtDpcLevel(&g_pDevExt->spinLock);

		// adjust read pointer
		if(g_pDevExt->ulCurrentWrite == g_pDevExt->ulCurrentRead - 1)
		{
			g_pDevExt->ulCurrentRead ++;
			if(g_pDevExt->ulCurrentRead == BUFFER_COUNT)
				g_pDevExt->ulCurrentRead = 0;
		}

		strncpy(g_pDevExt->ltBuffer[g_pDevExt->ulCurrentWrite],pString,sizeof(StringBuffer));

		g_pDevExt->ulCurrentWrite ++;

		if(g_pDevExt->ulCurrentWrite == BUFFER_COUNT)
			g_pDevExt->ulCurrentWrite = 0;

		// set event
		if(g_pDevExt->pKEvent)
			KeSetEvent(g_pDevExt->pKEvent,IO_NO_INCREMENT,FALSE);

		KeReleaseSpinLock(&g_pDevExt->spinLock,DISPATCH_LEVEL);
	}
	else
	{
		KIRQL oldIRQL;
		KeAcquireSpinLock(&g_pDevExt->spinLock,&oldIRQL);

		if(g_pDevExt->ulCurrentWrite == g_pDevExt->ulCurrentRead - 1)
		{
			g_pDevExt->ulCurrentRead ++;
			if(g_pDevExt->ulCurrentRead == BUFFER_COUNT)
				g_pDevExt->ulCurrentRead = 0;
		}

		strncpy(g_pDevExt->ltBuffer[g_pDevExt->ulCurrentWrite],pString,sizeof(StringBuffer));

		g_pDevExt->ulCurrentWrite ++;

		if(g_pDevExt->ulCurrentWrite == BUFFER_COUNT)
			g_pDevExt->ulCurrentWrite = 0;

		if(g_pDevExt->pKEvent)
			KeSetEvent(g_pDevExt->pKEvent,IO_NO_INCREMENT,FALSE);

		KeReleaseSpinLock(&g_pDevExt->spinLock,oldIRQL);
	}
}

VOID DriverUnload(PDRIVER_OBJECT pDriver)
{
	PDEVICE_OBJECT pDev = pDriver->DeviceObject;
	while(pDev)
	{
		PDeviceExt pExt = static_cast<PDeviceExt>(pDev->DeviceExtension);

		UNICODE_STRING devSymName;
		RtlInitUnicodeString(&devSymName,DEV_SYM_NAME);
		IoDeleteSymbolicLink(&devSymName);

		pDev = pDev->NextDevice;
		IoDeleteDevice(pExt->pDevice);
	}
	return;
}

// create file....
NTSTATUS DeviceOnCreate(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	PDeviceExt pExt = static_cast<PDeviceExt>(pDevice->DeviceExtension);
	InstallNewInterrupt();
	return CompleteRequest(pIrp,STATUS_SUCCESS,0);
}

// close handler
NTSTATUS DeviceOnClose(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	PDeviceExt pExt = static_cast<PDeviceExt>(pDevice->DeviceExtension);
	RestoreOrignalInterrupt();

	if(g_pDevExt->pKEvent)
	{
		// deref event
		ObDereferenceObject(g_pDevExt->pKEvent);
		g_pDevExt->pKEvent = NULL;
	}

	return CompleteRequest(pIrp,STATUS_SUCCESS,0);
}

// read file
NTSTATUS DeviceOnRead(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	KIRQL oldIRQL;
	KeAcquireSpinLock(&g_pDevExt->spinLock,&oldIRQL);
	ULONG ulSize = 0;

	// if there are strings....
	if(g_pDevExt->ulCurrentRead != g_pDevExt->ulCurrentWrite)
	{
		PIO_STACK_LOCATION pCurStack = IoGetCurrentIrpStackLocation(pIrp);

		ulSize = pCurStack->Parameters.Read.Length;
		StringBuffer *pCurrentBuffer = &g_pDevExt->ltBuffer[g_pDevExt->ulCurrentRead];
		if( ulSize > sizeof(StringBuffer))
			ulSize = sizeof(StringBuffer);

		if(ulSize)
			RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,pCurrentBuffer,ulSize);

		g_pDevExt->ulCurrentRead ++;

		if(g_pDevExt->ulCurrentRead == BUFFER_COUNT)
			g_pDevExt->ulCurrentRead = 0;
	}
	else
	{
		// clear event
		if(g_pDevExt->pKEvent)
			KeClearEvent(g_pDevExt->pKEvent);
	}
	KeReleaseSpinLock(&g_pDevExt->spinLock,oldIRQL);

	return CompleteRequest(pIrp,STATUS_SUCCESS,ulSize);
}

// write .... here only for test...no use
NTSTATUS DeviceOnWrite(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	DbgPrint("test write : %d\n",*(UCHAR*)pIrp->AssociatedIrp.SystemBuffer);
	return CompleteRequest(pIrp,STATUS_SUCCESS,1);
}

// used for register event
NTSTATUS DeviceOnIoControl(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	PIO_STACK_LOCATION pCurStack = IoGetCurrentIrpStackLocation(pIrp);

	ULONG code = pCurStack->Parameters.DeviceIoControl.IoControlCode;

	switch(code)
	{
	case IOCTL_REGISTER_EVENT:
		if(g_pDevExt->pKEvent)
			ObDereferenceObject(g_pDevExt->pKEvent);

		ObReferenceObjectByHandle(*(HANDLE*)pIrp->AssociatedIrp.SystemBuffer,EVENT_MODIFY_STATE,*ExEventObjectType,
			pIrp->RequestorMode,(PVOID *)(&g_pDevExt->pKEvent),NULL);
		break;

	case IOCTL_DEREGISTER_EVENT:
		if(g_pDevExt->pKEvent)
		{
			ObDereferenceObject(g_pDevExt->pKEvent);
			g_pDevExt->pKEvent = NULL;
		}

		break;
	}

	return CompleteRequest(pIrp,STATUS_SUCCESS,0);
}

// helper
NTSTATUS CompleteRequest(PIRP pIrp,NTSTATUS	status,ULONG_PTR pInfo)
{
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = pInfo;
	IoCompleteRequest(pIrp,IO_NO_INCREMENT);
	return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver,PUNICODE_STRING pRegPath)
{
	PDEVICE_OBJECT pDev = NULL;
	__try
	{
		UNICODE_STRING devName;
		RtlInitUnicodeString(&devName,DEV_NAME);
		NTSTATUS status = IoCreateDevice(pDriver,sizeof(DeviceExt),&devName,FILE_DEVICE_UNKNOWN,0,FALSE,&pDev);
		if(!NT_SUCCESS(status))
		{
			devDebugPrint("IoCreateDevice failed : %d\n",status);
			ExRaiseStatus(status);
		}

		g_pDevExt = static_cast<PDeviceExt>(pDev->DeviceExtension);

		g_pDevExt->pDevice = pDev;

		KeInitializeSpinLock(&g_pDevExt->spinLock);
		IoInitializeDpcRequest(g_pDevExt->pDevice,(PIO_DPC_ROUTINE)BreakPrintDpcForIsr);// PKDEFERRED_ROUTINE
		RtlZeroMemory(g_pDevExt->ltBuffer,sizeof(g_pDevExt->ltBuffer));
		g_pDevExt->ulCurrentRead = g_pDevExt->ulCurrentWrite = 0;
		g_pDevExt->pKEvent = NULL;

		UNICODE_STRING devSymName;
		RtlInitUnicodeString(&devSymName,DEV_SYM_NAME);
		status = IoCreateSymbolicLink(&devSymName,&devName);
		if(!NT_SUCCESS(status))
		{
			devDebugPrint("IoCreateSymbolicLink failed : %d\n",status);
			ExRaiseStatus(status);
		}

		pDev->Flags |= DO_BUFFERED_IO;

		pDev->Flags &= ~DO_DEVICE_INITIALIZING;

		pDriver->DriverUnload = DriverUnload;
		pDriver->MajorFunction[IRP_MJ_CREATE] = DeviceOnCreate;
		pDriver->MajorFunction[IRP_MJ_CLOSE] = DeviceOnClose;
		pDriver->MajorFunction[IRP_MJ_READ] = DeviceOnRead;
		pDriver->MajorFunction[IRP_MJ_WRITE] = DeviceOnWrite;
		pDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceOnIoControl;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		if(pDev)
		{
			UNICODE_STRING devSymName;
			RtlInitUnicodeString(&devSymName,DEV_SYM_NAME);
			IoDeleteSymbolicLink(&devSymName);

			IoDeleteDevice(pDev);

		}
		return GetExceptionCode();
	}
	return STATUS_SUCCESS;
}