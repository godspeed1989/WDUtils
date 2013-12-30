#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include "IOCTL.h"

HANDLE m_hCommDevice;

int OpenDMon()
{
	m_hCommDevice = CreateFile (
				"\\\\.\\DMon", 
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ, NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL
			);
	if(m_hCommDevice == INVALID_HANDLE_VALUE)
	{
		int error = GetLastError();
		fprintf(stderr, "Please install the device driver and start it firstly!\n");
		return error;
	}
	else
	{
		fprintf(stderr, "Successfully opened the device\n");
	}
	return 0;
}

void CloseDMon()
{
	CloseHandle(m_hCommDevice);
}

int main(int argc, char *argv[])
{
	DWORD dwOutBytes;
	if (argc > 1 && !OpenDMon())
	{
		if (strcmp("start", argv[1]) == 0)
		{
			printf("IOCTL_DMON_STARTFILTER\n");
			DeviceIoControl (
				m_hCommDevice, 
				IOCTL_DMON_STARTFILTER, 
				NULL,
				0, 
				NULL, 
				0,
				&dwOutBytes,
				NULL
			);
		}
		if (strcmp("stop", argv[1]) == 0)
		{
			printf("IOCTL_DMON_STOPFILTER\n");
			DeviceIoControl (
				m_hCommDevice, 
				IOCTL_DMON_STOPFILTER, 
				NULL,
				0, 
				NULL, 
				0,
				&dwOutBytes,
				NULL
			);
		}
	}
	CloseDMon();
	return 0;
}
