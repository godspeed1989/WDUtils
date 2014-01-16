#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include "../DiskFilterIoctl.h"

HANDLE m_hCommDevice;

int OpenDF()
{
	m_hCommDevice = CreateFile (
				"\\\\.\\Diskfilter", 
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

void CloseDF()
{
	CloseHandle(m_hCommDevice);
}

int main(int argc, char *argv[])
{
	DWORD dwOutBytes;
	if (!OpenDF())
	{
		DeviceIoControl (
			m_hCommDevice, 
			IOCTL_DF_TEST, 
			NULL,
			0, 
			NULL, 
			0,
			&dwOutBytes,
			NULL
		);
		DeviceIoControl (
			m_hCommDevice, 
			IOCTL_DF_QUERY_DISK_INFO, 
			NULL,
			0, 
			NULL, 
			0,
			&dwOutBytes,
			NULL
		);
	}
	CloseDF();
	return 0;
}
