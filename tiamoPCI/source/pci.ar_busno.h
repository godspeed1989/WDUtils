//********************************************************************
//	created:	5:10:2008   6:32
//	file:		pci.ar_busno.h
//	author:		tiamo
//	purpose:	arbiter for bus number
//********************************************************************

#pragma once

//
// constructor
//
NTSTATUS arbusno_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// initializer
//
NTSTATUS arbusno_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// unpack requirement
//
NTSTATUS arbusno_UnpackRequirement(__in PIO_RESOURCE_DESCRIPTOR Desc,__out PULONGLONG Minimum,__out PULONGLONG Maximum,__out PULONG Length,__out PULONG Alignment);

//
// pack resource
//
NTSTATUS arbusno_PackResource(__in PIO_RESOURCE_DESCRIPTOR Requirement,__in ULONGLONG Start,__out PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor);

//
// unpack resource
//
NTSTATUS arbusno_UnpackResource(__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor,__out PULONGLONG Start,__out PULONG Length);

//
// score requirement
//
LONG arbusno_ScoreRequirement(__in PIO_RESOURCE_DESCRIPTOR Descriptor);