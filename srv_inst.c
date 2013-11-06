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

DWORD WINAPI InstallDriver(
	PSTR			inst_path,
	DWORD			nStart)
{
	DWORD			ret = ERROR_SUCCESS;
	SC_HANDLE		hScManager;				// Service Control Manager
	SC_HANDLE		hService = NULL;		// Service (= Driver)
	
	// Connect to the Service Control Manager
	hScManager = OpenSCManager(
		NULL,							// local machine
		NULL,							// local database
		SC_MANAGER_CREATE_SERVICE);		// access required
	if (hScManager == NULL)
	{
		ret = GetLastError();
		fprintf(stderr, "failed to connect scm: %s\n", SystemMessage(ret));
		goto cleanup;
	}

	// Create a new service object
	hService = CreateService(
		hScManager,						// service control manager
		____DEVICE_BASENAME,			// internal service name
		____DEVICE_BASENAME,			// display name
		SERVICE_ALL_ACCESS,				// access mode
		SERVICE_KERNEL_DRIVER,			// service type
		nStart,							// service start type
		SERVICE_ERROR_NORMAL,			// start error sevirity
		inst_path,						// service image file path
		NULL,							// service group
		NULL,							// service tag
		NULL,							// service dependency
		NULL,							// use LocalSystem account
		NULL							// password for the account
	);
	if (!hService)
	{
		// Failed to create a service object
		ret = GetLastError();
		fprintf(stderr, "failed to create service: %s\n", SystemMessage(ret));
		goto cleanup;
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

DWORD WINAPI RemoveDriver()
{
	SC_HANDLE		hScManager;				// Service Control Manager
	SC_HANDLE		hService;				// Service (= Driver)
	DWORD			ret = ERROR_SUCCESS;

	//	Connect to the Service Control Manager
	hScManager = OpenSCManager(NULL, NULL, 0);
	if (hScManager == NULL)
	{
		ret = GetLastError();
		fprintf(stderr, "failed to open scm: %s\n", SystemMessage(ret));
		return ret;
	}

	//	Open the VFD driver entry in the service database
	hService = OpenService(
		hScManager,						// Service control manager
		____DEVICE_BASENAME,			// service name
		DELETE);						// service access mode

	if (hService == NULL)
	{
		ret = GetLastError();
		fprintf(stderr, "failed to open service: %s\n", SystemMessage(ret));
		goto cleanup;
	}

	//	Remove driver entry from registry
	if (!DeleteService(hService))
	{
		ret = GetLastError();
		fprintf(stderr, "failed to delete service: %s\n", SystemMessage(ret));
		goto cleanup;
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

int main(int argc, char* argv[])
{
	CHAR system_dir[MAX_PATH];
	GetWindowsDirectory(system_dir, sizeof(system_dir));
	fprintf(stderr, "system dir: %s\n", system_dir);

	if(argc < 3) goto ret;

	strcpy(____DEVICE_BASENAME, argv[2]);
	printf("service_name=%s\n", ____DEVICE_BASENAME);

	switch(argv[1][0])
	{
	case 'i':
		if(argc < 4) goto ret;
		printf("file_path=%s\n", argv[3]);
		if(InstallDriver(argv[3], SERVICE_DEMAND_START))
		{
			printf("install error\n");
			return 1;
		}
		break;
	case 'r':
		if(RemoveDriver())
		{
			printf("remove error\n");
			return 1;
		}
		break;
	}
	return 0;
ret:
	fprintf(stderr, "Usage: i|r service_name [.sys] \n");
	return 1;
}
