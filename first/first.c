
#include <ntddk.h>

VOID DriverUnload(PDRIVER_OBJECT driver)
{
	DbgPrint("first: Our driver is unloading¡­\r\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	DbgPrint("first: Hello, my salary!");

	driver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}
