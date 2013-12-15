/*****************************************************************************
*
*  DMon - Logon Session Monitor for Windows NT
*		
*  Copyright (c) 2000 Mark Russinovich 
*
*  See readme.txt for terms and conditions.
*
*  PROGRAM: DMon.h
*
*  PURPOSE: Communicates with the DMon driver to display 
*  token logon and logoff information.
*
******************************************************************************/

// delay for listview subitem tooltip
#define BALLOONDELAY		10

// toolbar height plus the borders
#define TOOLBARHEIGHT		28

// Number of buttons in the toolbar bitmap
#define NUMTOOLBUTTONS		13

// Number of columns in the listview
#define NUMCOLUMNS			6

// Application name
#define APPNAME				_T("DMon")

// Variables/definitions for the driver that performs the actual monitoring.
#define	SYS_FILE			_T("DMon.SYS")
#define	SYS_NAME			_T("DMon")

// Number of recent filters we keep
#define NUMRECENTFILTERS	5

// Offset for listview dots
#define DOTOFFSET			0

// Registry paths
#define TOKENMON_DRIVER_KEY				TEXT("System\\CurrentControlSet\\Services\\DMon")
#define TOKENMON_SETTINGS_KEY			TEXT("Software\\DMon")
#define TOKENMON_SETTINGS_VALUE			TEXT("Settings")
#define TOKENMON_RECENT_CONNECT_VALUE	TEXT("Recent")
#define TOKENMON_RECENT_INFILTER_VALUE	TEXT("InFilters")
#define TOKENMON_RECENT_EXFILTER_VALUE	TEXT("ExFilters")
#define TOKENMON_RECENT_HIFILTER_VALUE	TEXT("HiFilters")

// toolbar constants
#define ID_TOOLBAR			1

// defined for comtl32.dll version 4.7
#define TOOLBAR_FLAT		0x800

#define MAXITEMLENGTH		0x1000

// typedef for balloon popup
typedef struct {
	CHAR	itemText[1024];
	RECT	itemPosition;
} ITEM_CLICK, *PITEM_CLICK;

// this typedef, present in newer include files,
// supports the building tokenmon on older systems
typedef struct 
{
    DWORD cbSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformID;
} DLLVERSIONINFO_, *PDLLVERSIONINFO_;

// version number for position settings
#define POSITION_VERSION	200

// Position settings data structure 
typedef struct {
	int			posversion;
	int			left;
	int			top;
	int			width;
	int			height;
	DWORD		column[NUMCOLUMNS];
	DWORD		historydepth;
	BOOLEAN		maximized;
	BOOLEAN		clocktime;
	BOOLEAN		ontop;
	DWORD		highlightfg;
	DWORD		highlightbg;
	LOGFONT		font;
} POSITION_SETTINGS;
