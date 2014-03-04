
#include <ntddk.h>

VOID DriverUnload(PDRIVER_OBJECT driver)
{
	DbgPrint("timer: Our driver is unloading\r\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	KTIMER	timer;
	LARGE_INTEGER	tickcount, timeout;
	DbgPrint("timer: Time Increment %d", KeQueryTimeIncrement());
	DbgPrint("timer: The number of 100-nanosecond units per Tick\r\n");

	KeQueryTickCount(&tickcount);
	DbgPrint("timer: %I64d", tickcount.QuadPart);
#define microsecond 2000000
	timeout.QuadPart = microsecond * -10;
	KeInitializeTimerEx(&timer, NotificationTimer);
	KeSetTimer(&timer, timeout, NULL);
	KeWaitForSingleObject(&timer, Executive, KernelMode, FALSE, NULL);  

	KeQueryTickCount(&tickcount);
	DbgPrint("timer: %I64d", tickcount.QuadPart);

	driver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}
