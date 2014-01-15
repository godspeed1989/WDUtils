#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <Ntifs.h>
#include <Ntddk.h>
#include <Ntddvol.h>
#include <Wdm.h>
#include "Cache.h"

#ifdef __cplusplus
}
#endif

#define DF_DRIVER_EXTENSION_ID					0
#define DF_DRIVER_EXTENSION_ID_UNICODE_BUFFER	1

#define MAX_PROTECTED_VOLUME					10
#define MAX_CACHE_VOLUME						2

typedef struct _DF_DRIVER_EXTENSION
{
	UNICODE_STRING	ServiceKeyName;
	PWCH			RegistryUnicodeBuffer;
	WCHAR			ProtectedVolumes[MAX_PROTECTED_VOLUME + 1];
	WCHAR			CacheVolumes[MAX_CACHE_VOLUME];
} DF_DRIVER_EXTENSION, *PDF_DRIVER_EXTENSION;

typedef struct _DF_DEVICE_EXTENSION
{
	PDEVICE_OBJECT	PhysicalDeviceObject;
	PDEVICE_OBJECT	LowerDeviceObject;
	volatile LONG	PagingCount;
	KEVENT			PagingCountEvent;
	BOOLEAN			CurrentPnpState;
	BOOLEAN			bIsProtectedVolume;
	UNICODE_STRING	VolumeDosName;

	LARGE_INTEGER	TotalSize;		//	Total size of this volume in bytes
	ULONG			ClusterSize;	//	Cluster size of this volume in bytes
	ULONG			SectorSize;		//	Sector size of this volume in bytes
	ULONG			DiskNumber;
	ULONG			PartitionNumber;

	CACHE_POOL		CachePool;
} DF_DEVICE_EXTENSION, *PDF_DEVICE_EXTENSION;
