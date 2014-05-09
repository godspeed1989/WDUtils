#pragma once

#include <Ntifs.h>
#include <Ntddk.h>
#include <Ntdddisk.h>
#include <Ntddvol.h>
#include <Wdm.h>
#include "Cache.h"

#define DF_DRIVER_EXTENSION_ID                  0
#define DF_DRIVER_EXTENSION_ID_UNICODE_BUFFER   1

// obsolete
#define MAX_PROTECTED_VOLUME                    16
#define MAX_CACHE_VOLUME                        16

typedef struct _DF_DRIVER_EXTENSION
{
    UNICODE_STRING  ServiceKeyName;
    PWCH            RegistryUnicodeBuffer;
    WCHAR           ProtectedVolumes[MAX_PROTECTED_VOLUME + 1];
    WCHAR           CacheVolumes[MAX_CACHE_VOLUME];
} DF_DRIVER_EXTENSION, *PDF_DRIVER_EXTENSION;

typedef struct _DF_DEVICE_EXTENSION
{
    PDEVICE_OBJECT  PhysicalDeviceObject;
    PDEVICE_OBJECT  LowerDeviceObject;
    BOOLEAN         bIsProtected;
    BOOLEAN         bIsStart;
    // Device Info
    LARGE_INTEGER   TotalSize;
    ULONG           SectorSize;
    ULONG           ClusterSize;
    ULONG           DiskNumber;
    ULONG           PartitionNumber;
    // Statictis
    ULONG32         ReadCount;
    ULONG32         WriteCount;
    // RW Thread
    LIST_ENTRY      RwList;
    KSPIN_LOCK      RwListSpinLock;
    PVOID           RwThreadObject;
    BOOLEAN         bTerminalRwThread;
    KEVENT          RwThreadStartEvent;
    KEVENT          RwThreadFinishEvent;
    // WB Thread
#ifdef WRITE_BACK_ENABLE
    PVOID           WbThreadObject;
    BOOLEAN         bTerminalWbThread;
#endif
    // Cache Pool
    CACHE_POOL      CachePool;
    // For Pnp
    volatile LONG   PagingCount;
    KEVENT          PagingCountEvent;
    BOOLEAN         CurrentPnpState;
} DF_DEVICE_EXTENSION, *PDF_DEVICE_EXTENSION;
