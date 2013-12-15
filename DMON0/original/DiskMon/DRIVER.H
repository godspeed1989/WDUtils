//--------------------------------------------------------------------
//
// DMon
//
// Copyright (C) 2000 Mark Russinovich
// Systems Internals - http://www.sysinternals.com
//
// driver.h: functions located in driver.
//
//--------------------------------------------------------------------


//
// Functions in driver.c
//
BOOL LoadDeviceDriver( const TCHAR * Name, const TCHAR * Path, 
					  HANDLE * lphDevice, PDWORD Error );
BOOL UnloadDeviceDriver( const TCHAR * Name );

BOOL OpenDevice( IN LPCTSTR DriverName, HANDLE * lphDevice );
