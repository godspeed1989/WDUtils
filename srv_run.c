#include <windows.h>
#include <stdio.h>
char ____DEVICE_BASENAME[128];

//
//	Get system error message string
//
PCSTR SystemMessage(
	DWORD			nError)
{
	static CHAR		msg[256];

	if (!FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, nError, 0, msg, sizeof(msg), NULL)) {

		_snprintf(msg, sizeof(msg),
			"Unknown system error %lu (0x%08x)\n", nError, nError);
	}
	return msg;
}

DWORD WINAPI StartDriver()
{
	SC_HANDLE		hScManager;			// Service Control Manager
	SC_HANDLE		hService;			// Service (= Driver)
	SERVICE_STATUS	stat;
	DWORD			ret = ERROR_SUCCESS;
	int				i;


	//	Connect to the Service Control Manager
	hScManager = OpenSCManager(NULL, NULL, 0);

	if (hScManager == NULL)
	{
		ret = GetLastError();
		fprintf(stderr, "failed to connect scm: %s\n", SystemMessage(ret));
		return ret;
	}

	//	Open the driver entry in the service database
	hService = OpenService(
		hScManager,						// Service control manager
		____DEVICE_BASENAME,			// service name
		SERVICE_START
		| SERVICE_QUERY_STATUS);		// service access mode
	if (hService == NULL)
	{
		ret = GetLastError();
		fprintf(stderr, "failed to open service: %s\n", SystemMessage(ret));
		goto cleanup;
	}

	//	Start the driver
	if (!StartService(hService, 0, NULL))
	{
		ret = GetLastError();
		fprintf(stderr, "failed to start service: %s\n", SystemMessage(ret));
		goto cleanup;
	}

	//	Wait until the driver is properly running
	for (i = 0; i < 5; ++i)
	{
		if (!QueryServiceStatus(hService, &stat))
		{
			ret = GetLastError();
			fprintf(stderr, "failed to query service: %s\n", SystemMessage(ret));
			break;
		}
		if (stat.dwCurrentState == SERVICE_RUNNING)
			break;
		Sleep(1000);
	}

	if (stat.dwCurrentState == SERVICE_RUNNING)
	{
		fprintf(stderr, "service is running now\n");
	}
	else
	{
		fprintf(stderr, "failed to run service\n");
		ret = ERROR_SERVICE_NOT_ACTIVE;
	}

cleanup:
	//	Close the service object handle
	if (hService) {
		CloseServiceHandle(hService);
	}
	//	Close handle to the service control manager.
	if (hScManager) {
		CloseServiceHandle(hScManager);
	}

	return ret;
}

DWORD WINAPI StopDriver()
{
	SC_HANDLE		hScManager;			// Service Control Manager
	SC_HANDLE		hService;			// Service (= Driver)
	SERVICE_STATUS	stat;
	DWORD			ret = ERROR_SUCCESS;
	int				i;


	//	Connect to the Service Control Manager
	hScManager = OpenSCManager(NULL, NULL, 0);
	if (hScManager == NULL)
	{
		ret = GetLastError();
		fprintf(stderr, "failed to connect scm: %s\n", SystemMessage(ret));
		return ret;
	}

	//	Open the VFD driver entry in the service database
	hService = OpenService(
		hScManager,						// Service control manager
		____DEVICE_BASENAME,			// service name
		SERVICE_STOP
		| SERVICE_QUERY_STATUS);		// service access mode
	if (hService == NULL)
	{
		ret = GetLastError();
		fprintf(stderr, "failed to open service: %s\n", SystemMessage(ret));
		goto cleanup;
	}

	//	Stop the driver
	if (!ControlService(hService, SERVICE_CONTROL_STOP, &stat))
	{
		ret = GetLastError();
		fprintf(stderr, "failed to stop service\n");
		goto cleanup;
	}

	//	Wait until the driver is stopped
	for (i = 0; i < 5; ++i)
	{
		Sleep(1000);
		if (!QueryServiceStatus(hService, &stat))
		{
			ret = GetLastError();
			fprintf(stderr, "failed to query service: %s\n", SystemMessage(ret));
			break;
		}
		if (stat.dwCurrentState != SERVICE_RUNNING)
			break;
	}

	if (stat.dwCurrentState != SERVICE_RUNNING)
	{
		fprintf(stderr, "service is stopped now\n");
	}
	else
	{
		fprintf(stderr, "failed to stop service: %s\n", SystemMessage(ret));
		ret = ERROR_SERVICE_NOT_ACTIVE;
	}

cleanup:
	//	Close the service object handle
	if (hService) {
		CloseServiceHandle(hService);
	}
	//	Close handle to the service control manager.
	if (hScManager) {
		CloseServiceHandle(hScManager);
	}

	return ret;
}

int main(int argc, const char* argv[])
{
	if(argc < 3)
		goto ret;

	strcpy(____DEVICE_BASENAME, argv[2]);
	printf("service_name=%s\n", ____DEVICE_BASENAME);

	switch(argv[1][0])
	{
	case 'r':
		if(StartDriver())
		{
			printf("install error\n");
			return 1;
		}
		break;
	case 's':
		if(StopDriver())
		{
			printf("remove error\n");
			return 1;
		}
		break;
	}
	return 0;
ret:
	fprintf(stderr, "Usage: r|s service_name\n");
	return 1;
}
