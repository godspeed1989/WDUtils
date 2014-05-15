
//**************************************************************************************
//	日期:	30:3:2004   
//	创建:	tiamo	
//	描述:	control code
//**************************************************************************************

#pragma	once

#define IOCTL_REGISTER_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DEREGISTER_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)