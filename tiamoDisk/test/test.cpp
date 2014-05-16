
#include "stdafx.h"

#include <setupapi.h>
#pragma comment(lib,"Setupapi.lib")

#include "initguid.h"
#include "../public.h"

HANDLE FindDevice()
{
	HANDLE hDevice;

	HDEVINFO                    hardwareDeviceInfo;
	SP_DEVICE_INTERFACE_DATA    deviceInterfaceData;

	hardwareDeviceInfo = SetupDiGetClassDevs(&GUID_TIAMO_BUS,NULL,NULL,(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

	deviceInterfaceData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

	if(SetupDiEnumDeviceInterfaces(hardwareDeviceInfo,0,&GUID_TIAMO_BUS,0,&deviceInterfaceData))
	{
		PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
		ULONG                               predictedLength = 0;
		ULONG                               requiredLength = 0;

		SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo,&deviceInterfaceData,NULL,0,&requiredLength,NULL);

		predictedLength = requiredLength;

		deviceInterfaceDetailData = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(HeapAlloc(GetProcessHeap(),0,predictedLength));

		if(deviceInterfaceDetailData) 
		{
			deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		} 

		SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo,&deviceInterfaceData,deviceInterfaceDetailData,predictedLength,
										&requiredLength,NULL);

		hDevice = CreateFile(deviceInterfaceDetailData->DevicePath,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);

		HeapFree(GetProcessHeap(),0,deviceInterfaceDetailData);
	}

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);

	return hDevice;
}

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hDevice = FindDevice();
	if(hDevice != INVALID_HANDLE_VALUE)
	{
		MiniportConfig cfg;
		cfg.m_ulDevices = 1;
		wcscpy(cfg.m_szImageFileName[0],L"\\DosDevices\\c:\\pal.iso");

		DWORD dwReturn;
		BOOL bRet = DeviceIoControl(hDevice,IOCTL_TIAMO_BUS_PLUGIN,&cfg,sizeof(cfg),&cfg,sizeof(cfg),
									&dwReturn,NULL);

		CloseHandle(hDevice);
	}

	return 0;
}
