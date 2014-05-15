
//**************************************************************************************
//	日期:	23:2:2004
//	创建:	tiamo
//	描述:	stdafx
//**************************************************************************************

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
	#include "ntddk.h"
#ifdef __cplusplus
}
#endif

#if DBG
	#define devDebugPrint DbgPrint
#else
	#define devDebugPrint __noop
#endif
