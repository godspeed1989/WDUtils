#pragma once
#include <Ntifs.h>

NTSTATUS ForwardIrpSynchronously (
        PDEVICE_OBJECT  DeviceObject,
        PIRP            Irp
    );
NTSTATUS IoDoRWRequestSync (
        ULONG           MajorFunction,
        PDEVICE_OBJECT  DeviceObject,
        PVOID           Buffer,
        ULONG           Length,
        PLARGE_INTEGER  StartingOffset,
        ULONG           tryTimes
    );
NTSTATUS IoDoRWRequestAsync (
        ULONG           MajorFunction,
        PDEVICE_OBJECT  DeviceObject,
        PVOID           Buffer,
        ULONG           Length,
        PLARGE_INTEGER  StartingOffset
    );
NTSTATUS IoDoIoctl (
        ULONG           IoControlCode,
        PDEVICE_OBJECT  DeviceObject,
        PVOID           InputBuffer,
        ULONG           InputBufferLength,
        PVOID           OutputBuffer,
        IN ULONG        OutputBufferLength
    );
NTSTATUS DF_QueryDeviceInfo (
        PDEVICE_OBJECT DeviceObject
    );
NTSTATUS DF_GetDiskDeviceObjectPointer (
        ULONG           DiskIndex,
        ULONG           PartitionIndex,
        PFILE_OBJECT    *FileObject,
        PDEVICE_OBJECT  *DeviceObject
    );
ULONG QuerySystemTimeuSec (
    );
VOID DF_CalMD5 (
        PVOID buf,
        ULONG len,
        UCHAR digest[16]
    );
VOID StartDevice (PDEVICE_OBJECT DeviceObject);
VOID StopDevice (PDEVICE_OBJECT DeviceObject);

#pragma pack(1)
typedef struct _DP_FAT16_BOOT_SECTOR
{
    UCHAR       JMPInstruction[3];
    UCHAR       OEM[8];
    USHORT      BytesPerSector;
    UCHAR       SectorsPerCluster;
    USHORT      ReservedSectors;
    UCHAR       NumberOfFATs;
    USHORT      RootEntries;
    USHORT      Sectors;
    UCHAR       MediaDescriptor;
    USHORT      SectorsPerFAT;
    USHORT      SectorsPerTrack;
    USHORT      Heads;
    UINT32      HiddenSectors;
    UINT32      LargeSectors;
    UCHAR       PhysicalDriveNumber;
    UCHAR       CurrentHead;
} DP_FAT16_BOOT_SECTOR, *PDP_FAT16_BOOT_SECTOR;

typedef struct _DP_FAT32_BOOT_SECTOR
{
    UCHAR       JMPInstruction[3];
    UCHAR       OEM[8];
    USHORT      BytesPerSector;
    UCHAR       SectorsPerCluster;
    USHORT      ReservedSectors;
    UCHAR       NumberOfFATs;
    USHORT      RootEntries;
    USHORT      Sectors;
    UCHAR       MediaDescriptor;
    USHORT      SectorsPerFAT;
    USHORT      SectorsPerTrack;
    USHORT      Heads;
    UINT32      HiddenSectors;
    UINT32      LargeSectors;
    UINT32      LargeSectorsPerFAT;
    UCHAR       Data[24];
    UCHAR       PhysicalDriveNumber;
    UCHAR       CurrentHead;
} DP_FAT32_BOOT_SECTOR, *PDP_FAT32_BOOT_SECTOR;

typedef struct _DP_NTFS_BOOT_SECTOR
{
    UCHAR       Jump[3];                    //0
    UCHAR       FSID[8];                    //3
    USHORT      BytesPerSector;             //11
    UCHAR       SectorsPerCluster;          //13
    USHORT      ReservedSectors;            //14
    UCHAR       Mbz1;                       //16
    USHORT      Mbz2;                       //17
    USHORT      Reserved1;                  //19
    UCHAR       MediaDesc;                  //21
    USHORT      Mbz3;                       //22
    USHORT      SectorsPerTrack;            //24
    USHORT      Heads;                      //26
    UINT32      HiddenSectors;              //28
    UINT32      Reserved2[2];               //32
    ULONGLONG   TotalSectors;               //40
    ULONGLONG   MftStartLcn;                //48
    ULONGLONG   Mft2StartLcn;               //56
}DP_NTFS_BOOT_SECTOR, *PDP_NTFS_BOOT_SECTOR;
#pragma pack()
