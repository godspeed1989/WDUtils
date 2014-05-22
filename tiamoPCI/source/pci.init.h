//********************************************************************
//	created:	5:10:2008   6:51
//	file:		pci.init.h
//	author:		tiamo
//	purpose:	init
//********************************************************************

#pragma once

//
// driver entry
//
NTSTATUS DriverEntry(__in PDRIVER_OBJECT DriverObject,__in PUNICODE_STRING RegPath);

//
// build hack table
//
NTSTATUS PciBuildHackTable(__in HANDLE KeyHandle);

//
// get irq routing table
//
NTSTATUS PciGetIrqRoutingTableFromRegistry(__out PPCI_IRQ_ROUTING_TABLE_HEAD* Table);

//
// get debug ports
//
NTSTATUS PciGetDebugPorts(__in HANDLE KeyHandle);

//
// driver unload
//
VOID PciDriverUnload(__in PDRIVER_OBJECT DriverObject);