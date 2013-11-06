#include <ntddk.h>

VOID WorkThread(IN PVOID pContext)
{ 
	int i;
	for(i=0; i<5; i++)
		DbgPrint("thread!I am %d\r\n", i+1);
	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,IN PUNICODE_STRING RegistryPath)
{
	HANDLE hThread;
	PsCreateSystemThread(
		&hThread,                 // handle
		(ACCESS_MASK)0,
		NULL,
		(HANDLE)0,
		NULL,
		WorkThread,               // functon ptr
		NULL); 

	return STATUS_SUCCESS;
}
