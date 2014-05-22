//********************************************************************
//	created:	5:10:2008   7:27
//	file:		pci.tr_irq.h
//	author:		tiamo
//	purpose:	translate irq
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS tranirq_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntrf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS tranirq_Initializer(__in PPCI_ARBITER_INSTANCE Instance);