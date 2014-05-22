//********************************************************************
//	created:	14:5:2008   2:42
//	file: 		stdafx.h
//	author:		tiamo
//	purpose:	stdafx
//********************************************************************

#pragma once

extern "C"
{
	#include <ntddk.h>
	#include <ntstrsafe.h>
	#include <acpiioct.h>
	#include <wdmguid.h>

	#include "pci.nt.h"
	#include "pci.arbiter.h"
	#include "pci.const.h"
	#include "pci.struct.h"
	#include "pci.data.h"
	#include "pci.agpintrf.h"
	#include "pci.ar_busno.h"
	#include "pci.ar_memio.h"
	#include "pci.arb_comm.h"
	#include "pci.busintrf.h"
	#include "pci.busno.h"
	#include "pci.cardbus.h"
	#include "pci.config.h"
	#include "pci.debug.h"
	#include "pci.devhere.h"
	#include "pci.device.h"
	#include "pci.dispatch.h"
	#include "pci.enum.h"
	#include "pci.fdo.h"
	#include "pci.hookhal.h"
	#include "pci.id.h"
	#include "pci.ideintrf.h"
	#include "pci.init.h"
	#include "pci.lddintrf.h"
	#include "pci.pdo.h"
	#include "pci.pmeintf.h"
	#include "pci.power.h"
	#include "pci.ppbridge.h"
	#include "pci.romimage.h"
	#include "pci.routintf.h"
	#include "pci.state.h"
	#include "pci.tr_irq.h"
	#include "pci.usage.h"
	#include "pci.utils.h"
	#include "pci.verifier.h"
};