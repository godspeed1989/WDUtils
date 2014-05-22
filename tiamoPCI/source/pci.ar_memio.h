//********************************************************************
//	created:	5:10:2008   6:34
//	file:		pci.ar_memio.h
//	author:		tiamo
//	purpose:	arbiter for memory and port
//********************************************************************

#pragma once

//
// port io constructor
//
NTSTATUS ario_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// port initializer
//
NTSTATUS ario_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// memory constructor
//
NTSTATUS armem_Constructor(__in PPCI_COMMON_EXTENSION Ext,__in PPCI_INTERFACE PciIntf,__in PVOID Data,__in USHORT Version,__in USHORT Size,__in PINTERFACE Interface);

//
// memory initializer
//
NTSTATUS armem_Initializer(__in PPCI_ARBITER_INSTANCE Instance);

//
// unpack requirement
//
NTSTATUS armemio_UnpackRequirement(__in PIO_RESOURCE_DESCRIPTOR Desc,__out PULONGLONG Minimum,__out PULONGLONG Maximum,__out PULONG Length,__out PULONG Alignment);

//
// pack resource
//
NTSTATUS armemio_PackResource(__in PIO_RESOURCE_DESCRIPTOR Requirement,__in ULONGLONG Start,__out PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor);

//
// unpack resource
//
NTSTATUS armemio_UnpackResource(__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor,__out PULONGLONG Start,__out PULONG Length);

//
// score requirement
//
LONG armemio_ScoreRequirement(__in PIO_RESOURCE_DESCRIPTOR Descriptor);

//
// port find suitable range
//
BOOLEAN ario_FindSuitableRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// port proprocess entry
//
NTSTATUS ario_PreprocessEntry(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// port start arbiter
//
NTSTATUS ario_StartArbiter(__in PARBITER_INSTANCE Arbiter,__in PCM_RESOURCE_LIST StartResources);

//
// port get next allocation range
//
BOOLEAN ario_GetNextAllocationRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// port override conflict
//
BOOLEAN ario_OverrideConflict(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// port backtrack allocation
//
VOID ario_BacktrackAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// port add allocation
//
VOID ario_AddAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// memory find suitable range
//
BOOLEAN armem_FindSuitableRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// memory preprocess entry
//
NTSTATUS armem_PreprocessEntry(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// memory start arbiter
//
NTSTATUS armem_StartArbiter(__in PARBITER_INSTANCE Arbiter,__in PCM_RESOURCE_LIST StartResources);

//
// memory get next allocation range
//
BOOLEAN armem_GetNextAllocationRange(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// port get next alias
//
BOOLEAN ario_GetNextAlias(__in ULONG IoDescriptorFlags,__in ULONGLONG LastAlias,__out PULONGLONG NextAlias);

//
// port is aliased range available
//
BOOLEAN ario_IsAliasedRangeAvailable(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// apply broken video hack
//
VOID ario_ApplyBrokenVideoHack(__in PPCI_FDO_EXTENSION FdoExt);

//
// is bridge
//
BOOLEAN ario_IsBridge(__in PDEVICE_OBJECT Pdo);

//
// find window with isa bit
//
BOOLEAN ario_FindWindowWithIsaBit(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State);

//
// add/backtrack allocation
//
VOID ario_AddOrBacktrackAllocation(__in PARBITER_INSTANCE Arbiter,__in PARBITER_ALLOCATION_STATE State,__in PVOID AddOrBacktrack);