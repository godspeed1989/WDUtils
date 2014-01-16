#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <Ntifs.h>
#include <Ntddk.h>
#include <Ntdddisk.h.>
#include <Ntddvol.h>
#include <Wdm.h>
#include "Cache.h"

#ifdef __cplusplus
}
#endif

#define DF_DRIVER_EXTENSION_ID					0
#define DF_DRIVER_EXTENSION_ID_UNICODE_BUFFER	1

#define MAX_PARTITIONS_PER_DISK					104 //26*4
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
	// For Pnp
	volatile LONG	PagingCount;
	KEVENT			PagingCountEvent;
	BOOLEAN			CurrentPnpState;

	BOOLEAN			bIsProtectedVolume;
	UNICODE_STRING	VolumeDosName;

	LARGE_INTEGER	TotalSize;		//	Total size of this volume in bytes
	//ULONG			ClusterSize;	//	Cluster size of this volume in bytes
	ULONG			SectorSize;		//	Sector size of this volume in bytes
	ULONG			DiskNumber;
	ULONG			PartitionNumber;

	/*
		typedef struct _DRIVE_LAYOUT_INFORMATION {
			ULONG  PartitionCount;
			ULONG  Signature;
			PARTITION_INFORMATION  PartitionEntry[1];
		} DRIVE_LAYOUT_INFORMATION, *PDRIVE_LAYOUT_INFORMATION;
		typedef struct _PARTITION_INFORMATION {
			LARGE_INTEGER  StartingOffset;
			LARGE_INTEGER  PartitionLength;
			DWORD  HiddenSectors;
			DWORD  PartitionNumber;
			BYTE  PartitionType;
			BOOLEAN  BootIndicator;
			BOOLEAN  RecognizedPartition;
			BOOLEAN  RewritePartition;
		} PARTITION_INFORMATION, *PPARTITION_INFORMATION;
	*/
	PDRIVE_LAYOUT_INFORMATION	DiskLayout;

	CACHE_POOL					CachePool;
} DF_DEVICE_EXTENSION, *PDF_DEVICE_EXTENSION;
