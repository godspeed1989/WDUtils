#include <ntddk.h>

static LONG lock;

void init_spin_lock (PLONG lock)
{
	*lock = 0;
}

void spin_lock (PLONG lock)
{
	/* Try to acquire the lock */
	while (InterlockedCompareExchange(lock, 1, 0))
	{
		/* It's locked... spin until it's unlocked */
		while (*(volatile PLONG)lock & 1)
		{
			;/* Yield and keep looping */
		}
	}
}

void spin_unlock (PLONG lock)
{
	InterlockedAnd(lock, 0);
}

VOID WorkThread1(IN PVOID pContext)
{
	int i;
	for(i=0; i<5; i++)
	{
		spin_lock(&lock);
		DbgPrint("%s\n", __FUNCTION__);
		spin_unlock(&lock);
		KeStallExecutionProcessor(500*1000);
	}
	PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID WorkThread2(IN PVOID pContext)
{
	int i;
	for(i=0; i<5; i++)
	{
		spin_lock(&lock);
		DbgPrint("%s\n", __FUNCTION__);
		spin_unlock(&lock);
		KeStallExecutionProcessor(500*1000);
	}
	PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID DriverUnload(PDRIVER_OBJECT driver)
{
	DbgPrint("%s: unloading\n", __FUNCTION__);
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	HANDLE hThread;

	init_spin_lock(&lock);

	PsCreateSystemThread(
		&hThread,				  // handle
		(ACCESS_MASK)0,
		NULL,
		(HANDLE)0,
		NULL,
		WorkThread1,			   // functon ptr
		NULL);
	PsCreateSystemThread(
		&hThread,				  // handle
		(ACCESS_MASK)0,
		NULL,
		(HANDLE)0,
		NULL,
		WorkThread2,			   // functon ptr
		NULL);
	DriverObject->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}
