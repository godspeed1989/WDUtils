//======================================================================
// 
// Ioctlcmd.h
//
// Copyright (C) 2009 diyhack 
//
// Common header file for device driver and GUI. Contains common
// data structure definitions and IOCTL command codes.
//
//======================================================================

//
// Define the various device type values.  Note that values used by Microsoft
// Corporation are in the range 0-32767, and 32768-65535 are reserved for use
// by customers.
//
#define FILE_DEVICE_DMON      0x00008554

//
// DISK Monitor version
//
#define DMONVERSION           100

//
// Commands that the GUI can send the device driver
//
#define IOCTL_DMON_VERSION             CTL_CODE( FILE_DEVICE_DMON, 0x00, METHOD_BUFFERED, FILE_READ_DATA )
#define IOCTL_DMON_ZEROSTATS           CTL_CODE( FILE_DEVICE_DMON, 0x01, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_DMON_GETSTATS	       CTL_CODE( FILE_DEVICE_DMON, 0x02, METHOD_NEITHER,  FILE_READ_DATA)
#define IOCTL_DMON_SETFILTER           CTL_CODE( FILE_DEVICE_DMON, 0x03, METHOD_BUFFERED, FILE_WRITE_DATA )
#define IOCTL_DMON_STOPFILTER          CTL_CODE( FILE_DEVICE_DMON, 0x04, METHOD_BUFFERED, FILE_WRITE_DATA )
#define IOCTL_DMON_STARTFILTER         CTL_CODE( FILE_DEVICE_DMON, 0x05, METHOD_BUFFERED, FILE_WRITE_DATA )
#define IOCTL_DMON_PASSTHROUGH         CTL_CODE( FILE_DEVICE_DMON, 0x06, METHOD_BUFFERED, FILE_WRITE_DATA )
#define IOCTL_DMON_NOPASSTHROUGH       CTL_CODE( FILE_DEVICE_DMON, 0x07, METHOD_BUFFERED, FILE_WRITE_DATA )
#define IOCTL_DMON_HOOK                CTL_CODE( FILE_DEVICE_DMON, 0x05, METHOD_BUFFERED, FILE_WRITE_DATA )
#define IOCTL_DMON_UNHOOK              CTL_CODE( FILE_DEVICE_DMON, 0x04, METHOD_BUFFERED, FILE_WRITE_DATA )

//
// Format of an output buffer entry
//
#pragma warning(disable:4200)
typedef struct _ENTRY {

	ULONG           seq;
	LARGE_INTEGER   time;
	ULONG			DiskNum;
	CHAR	        text[0];

} ENTRY, *PENTRY;
//
// Length of a filter definition string
//
#define MAXFILTERLEN 256

//
// Filter definition
//
typedef struct {
    CHAR     includefilter[MAXFILTERLEN];
	CHAR     excludefilter[MAXFILTERLEN];
} FILTER, *PFILTER;

//
// Define page size for use by GUI
//
#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000  // 4K
#endif

//
// An allocation unit size 
//
#define MAX_STORE	( PAGE_SIZE*16 - 2*sizeof(ULONG_PTR) )

//
// Max length of domain plus user name
//
#define MAX_NAME_LENGTH    1024




