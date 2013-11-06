#include <windows.h>
#include <stdio.h>
#include <ctype.h>
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

DWORD WINAPI GetDriverConfig(
	PSTR			sFileName,  // driver file name
	PDWORD			pStart)     // driver state
{
	SC_HANDLE		hScManager;				// Service Control Manager
	SC_HANDLE		hService;				// Service (= Driver)
	LPQUERY_SERVICE_CONFIG config = NULL;
	DWORD			result;
	DWORD			ret = ERROR_SUCCESS;

	if (sFileName)
		ZeroMemory(sFileName, MAX_PATH);
	if (pStart)
		*pStart = 0;

	//	Connect to the Service Control Manager
	hScManager = OpenSCManager(NULL, NULL, 0);

	if (hScManager == NULL) {
		ret = GetLastError();
		fprintf(stderr, "fail to connect scm: %s\n", SystemMessage(ret));
		return ret;
	}

	//	Open the driver entry in the service database
	hService = OpenService(
		hScManager,						// Service control manager
		____DEVICE_BASENAME,			// service name
		SERVICE_QUERY_CONFIG);			// service access mode
	if (hService == NULL) {
		ret = GetLastError();
		fprintf(stderr, "fail to open service: %s\n", SystemMessage(ret));
		goto cleanup;
	}

	//	Get the length of config information
	if (!QueryServiceConfig(hService, NULL, 0, &result))
	{
		ret = GetLastError();
		if (ret == ERROR_INSUFFICIENT_BUFFER)
			ret = ERROR_SUCCESS;
		else
		{
			fprintf(stderr, "fail to get config info: %s\n", SystemMessage(ret));
			goto cleanup;
		}
	}

	//	allocate a required buffer
	config = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, result);
	if (config == NULL)
	{
		ret = GetLastError();
		fprintf(stderr, "fail to alloc: %s\n", SystemMessage(ret));
		goto cleanup;
	}

	//	get the config information
	if (!QueryServiceConfig(hService, config, result, &result))
	{
		ret = GetLastError();
		fprintf(stderr, "fail to query config: %s\n", SystemMessage(ret));
		goto cleanup;
	}

	//	copy information to output buffer
	if (sFileName)
	{
		if (strncmp(config->lpBinaryPathName, "\\??\\", 4) == 0)
		{
			//	driver path is an absolute UNC path
			strncpy(
				sFileName,
				config->lpBinaryPathName + 4,
				MAX_PATH);
		}
		else if (config->lpBinaryPathName[0] == '\\' ||
			(isalpha(config->lpBinaryPathName[0]) &&
			config->lpBinaryPathName[1] == ':'))
		{
			//	driver path is an absolute path
			strncpy(sFileName,
				config->lpBinaryPathName,
				MAX_PATH);
		}
		else
		{
			//	driver path is relative to the SystemRoot
			DWORD len = GetWindowsDirectory(sFileName, MAX_PATH);
			if (len == 0 || len > MAX_PATH)
			{
				fprintf(stderr, ": %%SystemRoot%% is empty or too long.\n");
				ret = ERROR_BAD_ENVIRONMENT;
				goto cleanup;
			}
			sprintf((sFileName + len), "\\%s",
				config->lpBinaryPathName);
		}
	}
	
	if (pStart)
		*pStart = config->dwStartType;
cleanup:
	//	Free service config buffer
	if (config) {
		LocalFree(config);
	}
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
	CHAR		sFileName[MAX_PATH];
	DWORD       Start;
	if(argc < 2)
		return 1;
	strcpy(____DEVICE_BASENAME, argv[1]);
	GetDriverConfig(sFileName, &Start);
	printf("%s\n", sFileName);
	return 0;
}
