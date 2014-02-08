#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include "../DiskFilterIoctl.h"

#if _MSC_VER
#define snprintf _snprintf
#endif

HANDLE m_hCommDevice;
ULONG sysDiskNumber;
ULONG sysPartitionNumber;

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

struct command
{
	const char *cmd, *desp;
	DWORD IoControlCode;
};
struct command options[] =
{
	{"sa", "Start All Filters", IOCTL_DF_START_ALL},
	{"qa", "Stop All Filters", IOCTL_DF_STOP_ALL},
	{"start", "Start One Filter", IOCTL_DF_START},
	{"stop", "Stop One Filter", IOCTL_DF_STOP},
	{"stat", "Get Statistic on One Volume", IOCTL_DF_GET_STAT},
	{"clear", "Clear Statistic of One Volume", IOCTL_DF_CLEAR_STAT},
	{"quiet", "Quite All Output", IOCTL_DF_QUIET},
	{"verbose", "Verbose All Output", IOCTL_DF_VERBOSE},
	{"q", "Quit", 0},
	{0, 0, 0}
};

#define BUFLEN 32

DWORD GetSystemVolumeNum();

int main(int argc, char *argv[])
{
	DWORD dwOutBytes;
	ULONG32 iBuffer[BUFLEN] = {0};
	ULONG32 oBuffer[BUFLEN] = {0};
	int i;
	char istr[256];

	if (GetSystemVolumeNum())
		return -1;

	if (!OpenDF())
	{
		// Test Ioctl
		iBuffer[0] = rand() % 256 + 1;
		iBuffer[1] = rand() % 256 + 1;
		DeviceIoControl (
			m_hCommDevice,
			IOCTL_DF_TEST,
			iBuffer,
			2 * sizeof(ULONG32),
			oBuffer,
			sizeof(ULONG32),
			&dwOutBytes,
			NULL
		);
		iBuffer[0] += iBuffer[1];
		if (oBuffer[0] != iBuffer[0])
		{
			fprintf(stderr, "Test Ioctl Error\n");
			goto ERROUT;
		}
		// Print Help
		i = 0;
		while (options[i].cmd)
		{
			printf("%10s   %s\n", options[i].cmd, options[i].desp);
			i++;
		}
		// Get Input
		for (;;)
		{
			printf("Input you option:\n");
			scanf("%s", istr);

			i = 0;
			while (options[i].cmd)
			{
				if (0 == strcmp("q", istr))
					goto ERROUT;
				if (0 == strcmp(options[i].cmd, istr))
				{
					printf("%s\n", options[i].desp);
					if (strcmp(istr, "start")==0 || strcmp(istr, "stop")==0||
						strcmp(istr, "clear")==0 || strcmp(istr, "stat")==0)
					{
						printf("Input DiskNumber PartitionNumber\n");
						scanf("%d%d", &iBuffer[0], &iBuffer[1]);
						if (iBuffer[0] == sysDiskNumber && iBuffer[1] == sysPartitionNumber)
							printf("Highly recommend not cache system volume!\n");
						printf("%s disk(%d) partition(%d)\n", istr, iBuffer[0], iBuffer[1]);
					}
					DeviceIoControl (
						m_hCommDevice,
						options[i].IoControlCode,
						iBuffer,
						BUFLEN * sizeof(ULONG32),
						oBuffer,
						BUFLEN * sizeof(ULONG32),
						&dwOutBytes,
						NULL
					);
					if (strcmp(istr, "stat") == 0 && dwOutBytes >= 3*sizeof(ULONG32))
					{
						printf("CacheHit:  %10d\n", oBuffer[0]);
						printf("ReadCount: %10d\n", oBuffer[1]);
						printf("WriteCount:%10d\n", oBuffer[2]);
					}
					break;
				}
				i++;
			}
		}
	}
ERROUT:
	CloseDF();
	return 0;
}

DWORD GetSystemVolumeNum()
{
	HANDLE hDrv;
	BOOL bSuccess;
	DWORD lpBytesReturned;
	PARTITION_INFORMATION pinfo;
	VOLUME_DISK_EXTENTS VolumeDiskExt;
	TCHAR dir[MAX_PATH], letter;

	if (GetSystemDirectory(dir, MAX_PATH))
		letter = dir[0];
	else
	{
		printf("%s: Can not Get System Directory\n", __FUNCTION__);
		goto l_error;
	}
	snprintf(dir, MAX_PATH, "\\\\.\\%c:", letter);

	hDrv = CreateFile ( dir, GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING, 0, NULL );
	if(hDrv == INVALID_HANDLE_VALUE)
	{
		printf("%s: Can not open the Driver %c:\n", __FUNCTION__, letter);
		goto l_error;
	}

	bSuccess = DeviceIoControl (
		hDrv, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
		NULL, 0, (LPVOID) &VolumeDiskExt, (DWORD) sizeof(VOLUME_DISK_EXTENTS),
		(LPDWORD) &lpBytesReturned, (LPOVERLAPPED) NULL
	);
	if(!bSuccess)
	{
		printf("%s: ioctl error0 %lu\n", __FUNCTION__, GetLastError());
		goto l_error;
	}

	bSuccess = DeviceIoControl (
		hDrv, IOCTL_DISK_GET_PARTITION_INFO,
		NULL, 0, (LPVOID) &pinfo, (DWORD) sizeof(PARTITION_INFORMATION),
		(LPDWORD) &lpBytesReturned, (LPOVERLAPPED) NULL
	);
	if(!bSuccess)
	{
		printf("%s: ioctl error1 %lu\n", __FUNCTION__, GetLastError());
		goto l_error;
	}

	sysDiskNumber = VolumeDiskExt.Extents[0].DiskNumber;
	sysPartitionNumber = pinfo.PartitionNumber;

	//printf("%lu\n", sysDiskNumber);
	//printf("%lu\n", sysPartitionNumber);
	return 0;
l_error:
	CloseHandle(hDrv);
	return GetLastError();
}
