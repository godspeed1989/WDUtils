#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <Ntifs.h>
#include <Ntddk.h>
#include <Ntddvol.h>
#include <Wdm.h>

#ifdef __cplusplus
}
#endif

#define DISKFILTER_DRIVER_EXTENSION_ID					0
#define DISKFILTER_DRIVER_EXTENSION_ID_UNICODE_BUFFER	1

#define MAX_PROTECTED_VOLUME				10
#define MAX_CACHE_VOLUME					2

#define DISK_FILTER_TAG						'ksiD'

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

	LARGE_INTEGER	TotalSize;		//	Total size of this volume in bytes.
	ULONG			ClusterSize;	//	Cluster size of this volume in bytes.
	ULONG			SectorSize;		//	Sector size of this volume in bytes.
	RTL_BITMAP		Bitmap;

	LIST_ENTRY		RwList;
	PVOID			RwThreadObject;
	BOOLEAN			bTerminalThread;
	KEVENT			RwThreadEvent;
	KSPIN_LOCK		RwSpinLock;
} DISKFILTER_DEVICE_EXTENSION, *PDISKFILTER_DEVICE_EXTENSION;


#ifdef __cplusplus
extern "C"
{
#endif

DRIVER_INITIALIZE DriverEntry;

DRIVER_DISPATCH DiskFilter_DispatchDefault;

DRIVER_DISPATCH DiskFilter_DispatchReadWrite;

DRIVER_DISPATCH DiskFilter_DispatchPnp;

DRIVER_DISPATCH DiskFilter_DispatchPower;

DRIVER_DISPATCH	DiskFilter_DispatchControl;

DRIVER_ADD_DEVICE DiskFilter_AddDevice;

DRIVER_UNLOAD	DiskFilter_DriverUnload;

DRIVER_REINITIALIZE	DiskFilter_DriverReinitializeRoutine;

KSTART_ROUTINE DiskFilter_ReadWriteThread;

IO_COMPLETION_ROUTINE DiskFilter_QueryVolumeCompletion;

#ifdef __cplusplus
}
#endif

#pragma pack(1)
typedef struct _DP_FAT16_BOOT_SECTOR
{
	UCHAR		JMPInstruction[3];
	UCHAR		OEM[8];
	USHORT		BytesPerSector;
	UCHAR		SectorsPerCluster;
	USHORT		ReservedSectors;
	UCHAR		NumberOfFATs;
	USHORT		RootEntries;
	USHORT		Sectors;
	UCHAR		MediaDescriptor;
	USHORT		SectorsPerFAT;
	USHORT		SectorsPerTrack;
	USHORT		Heads;
	UINT32		HiddenSectors;
	UINT32		LargeSectors;
	UCHAR		PhysicalDriveNumber;
	UCHAR		CurrentHead;
} DP_FAT16_BOOT_SECTOR, *PDP_FAT16_BOOT_SECTOR;

typedef struct _DP_FAT32_BOOT_SECTOR
{
	UCHAR		JMPInstruction[3];
	UCHAR		OEM[8];
	USHORT		BytesPerSector;
	UCHAR		SectorsPerCluster;
	USHORT		ReservedSectors;
	UCHAR		NumberOfFATs;
	USHORT		RootEntries;
	USHORT		Sectors;
	UCHAR		MediaDescriptor;
	USHORT		SectorsPerFAT;
	USHORT		SectorsPerTrack;
	USHORT		Heads;
	UINT32		HiddenSectors;
	UINT32		LargeSectors;
	UINT32		LargeSectorsPerFAT;
	UCHAR		Data[24];
	UCHAR		PhysicalDriveNumber;
	UCHAR		CurrentHead;
} DP_FAT32_BOOT_SECTOR, *PDP_FAT32_BOOT_SECTOR;

typedef struct _DP_NTFS_BOOT_SECTOR
{
	UCHAR		Jump[3];					//0
	UCHAR		FSID[8];					//3
	USHORT		BytesPerSector;				//11
	UCHAR		SectorsPerCluster;			//13
	USHORT		ReservedSectors;			//14
	UCHAR		Mbz1;						//16
	USHORT		Mbz2;						//17
	USHORT		Reserved1;					//19
	UCHAR		MediaDesc;					//21
	USHORT		Mbz3;						//22
	USHORT		SectorsPerTrack;			//24
	USHORT		Heads;						//26
	UINT32		HiddenSectors;				//28
	UINT32		Reserved2[2];				//32
	ULONGLONG	TotalSectors;				//40
	ULONGLONG	MftStartLcn;				//48
	ULONGLONG	Mft2StartLcn;				//56
}DP_NTFS_BOOT_SECTOR, *PDP_NTFS_BOOT_SECTOR;
#pragma pack()

NTSTATUS
	DiskFilter_QueryConfig (
		PWCHAR ProtectedVolume,
		PWCHAR CacheVolume,
		PUNICODE_STRING RegistryPath
	);

NTSTATUS
	DiskFilter_QueryVolumeInfo(
		PDEVICE_OBJECT DeviceObject
	);

NTSTATUS
	DiskFilter_InitBitMapAndCreateThread(
		PDISKFILTER_DEVICE_EXTENSION DevExt
	);

VOID
	DiskFilter_ReadWriteThread(PVOID Context);

#pragma alloc_text("INIT",  DriverEntry)
#pragma alloc_text("PAGED", DiskFilter_DispatchDefault)
#pragma alloc_text("PAGED", DiskFilter_DispatchReadWrite)
#pragma alloc_text("PAGED", DiskFilter_DispatchPnp)
#pragma alloc_text("PAGED", DiskFilter_DispatchPower)
#pragma alloc_text("PAGED", DiskFilter_DispatchControl)
#pragma alloc_text("PAGED", DiskFilter_AddDevice)
#pragma alloc_text("PAGED", DiskFilter_DriverUnload)
