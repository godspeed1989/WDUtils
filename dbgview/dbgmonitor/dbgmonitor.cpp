// dbgmonitor.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "../controlcode.h"

BOOL ReadDebugString(HANDLE hDevice)
{
	CHAR buffer[513];
	DWORD dwRead = 0;
	if(ReadFile(hDevice,buffer,sizeof(buffer),&dwRead,NULL))
	{
		buffer[512] = 0;
		if(dwRead)
		{
			printf("dbg-> %s",buffer);
		}
	}

	return dwRead != 0;
}

BOOL g_bLoop = TRUE;

UINT WINAPI TestThread(LPVOID lpParam)
{
	HANDLE hDevice = static_cast<HANDLE>(lpParam);

	int i = 0;
	while(g_bLoop)
	{
		DWORD dwWritten;
		WriteFile(hDevice,&i,sizeof(i),&dwWritten,NULL);
		i ++;
		Sleep(500);
	}

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hDevice = CreateFile("\\\\.\\DBGLOG",GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hDevice != INVALID_HANDLE_VALUE)
	{
		HANDLE hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
		if(hEvent)
		{
			DWORD dummy;
			BOOL bRet = DeviceIoControl(hDevice,IOCTL_REGISTER_EVENT,&hEvent,sizeof(hEvent),NULL,NULL,&dummy,NULL);

			printf("device opened.waiting for output...and press any key to abort.\n");

			UINT uThreadID;
			HANDLE hThread = (HANDLE)_beginthreadex(NULL,0,TestThread,hDevice,0,&uThreadID);
			if(hThread)
			{
				while(!kbhit())
				{
					// wait for event
					if(WaitForSingleObject(hEvent,100) == WAIT_OBJECT_0)
					{
						while(ReadDebugString(hDevice));
					}
				}

				g_bLoop = FALSE;

				WaitForSingleObject(hThread,INFINITE);

				CloseHandle(hThread);
			}

			CloseHandle(hEvent);
		}

		CloseHandle(hDevice);
	}
	return 0;
}

