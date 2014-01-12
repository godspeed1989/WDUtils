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

#define DISKFILTER_DRIVER_EXTENSION_ID					0
#define DISKFILTER_DRIVER_EXTENSION_ID_UNICODE_BUFFER	1

#define DISK_FILTER_TAG						'ksiD'

#define MAX_PROTECTED_VOLUME				10
#define MAX_CACHE_VOLUME					2

typedef struct _DISKFILTER_DRIVER_EXTENSION
{
	UNICODE_STRING	ServiceKeyName;
	PWCH			RegistryUnicodeBuffer;
	WCHAR			ProtectedVolumes[MAX_PROTECTED_VOLUME + 1];
	WCHAR			CacheVolumes[MAX_CACHE_VOLUME];
} DISKFILTER_DRIVER_EXTENSION, *PDISKFILTER_DRIVER_EXTENSION;

typedef struct _DISKFILTER_DEVICE_EXTENSION
{
	PDEVICE_OBJECT	PhysicalDeviceObject;
	PDEVICE_OBJECT	LowerDeviceObject;
	volatile LONG	PagingCount;
	KEVENT			PagingCountEvent;
	BOOLEAN			CurrentPnpState;
	BOOLEAN			bIsProtectedVolume;
	UNICODE_STRING	VolumeDosName;

	LARGE_INTEGER	TotalSize;		//	Total size of this volume in bytes.
	ULONG			ClusterSize;	//	Cluster size of this volume in bytes.
	ULONG			SectorSize;		//	Sector size of this volume in bytes.

	CACHE_POOL		CachePool;
} DISKFILTER_DEVICE_EXTENSION, *PDISKFILTER_DEVICE_EXTENSION;
