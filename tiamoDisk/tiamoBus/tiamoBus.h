
//**************************************************************************************
//	日期:	23:2:2004   
//	创建:	tiamo	
//	描述:	tiamoBus
//**************************************************************************************

#pragma once

extern "C"
{
	NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver,PUNICODE_STRING pRegPath);
	void DriverUnload(PDRIVER_OBJECT pDriver);
}
