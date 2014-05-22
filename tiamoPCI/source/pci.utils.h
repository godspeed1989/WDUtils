//********************************************************************
//	created:	5:10:2008   6:56
//	file:		pci.utils.h
//	author:		tiamo
//	purpose:	utils
//********************************************************************

#pragma once

//
// read registry
//
NTSTATUS PciGetRegistryValue(__in PWCH Name,__in PWCH SubKeyName,__in HANDLE KeyHandle,__out PVOID* Data,__out PULONG Length);

//
// open registry
//
BOOLEAN	 PciOpenKey(__in PWCH SubKeyName,__in HANDLE KeyHandle,__out HANDLE* SubKeyHandle,__out NTSTATUS* Status);

//
// read device property
//
NTSTATUS PciGetDeviceProperty(__in PDEVICE_OBJECT PhysicalDeviceObject,__in DEVICE_REGISTRY_PROPERTY Property,__out PVOID* Data);

//
// read bios config
//
NTSTATUS PciGetBiosConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config);

//
// write bios config
//
NTSTATUS PciSaveBiosConfig(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config);

//
// build default exclusion lists
//
NTSTATUS PciBuildDefaultExclusionLists();

//
// find parent fdo extension
//
PPCI_FDO_EXTENSION PciFindParentPciFdoExtension(__in PDEVICE_OBJECT PhysicalDeviceObject,__in_opt PKEVENT Lock);

//
// find pdo by function
//
PPCI_PDO_EXTENSION PciFindPdoByFunction(__in PPCI_FDO_EXTENSION FdoExt,__in PCI_SLOT_NUMBER Slot,__in PPCI_COMMON_HEADER Config);

//
// invalidate resource info cache
//
VOID PciInvalidateResourceInfoCache(__in PPCI_PDO_EXTENSION);

//
// build range list from resource list
//
NTSTATUS PciRangeListFromResourceList(__in PPCI_FDO_EXTENSION FdoExt,__in PCM_RESOURCE_LIST CmResList,__in UCHAR Type,__in BOOLEAN ArbRes,__in PRTL_RANGE_LIST List);

//
// search resource descriptor with type
//
PCM_PARTIAL_RESOURCE_DESCRIPTOR PciFindDescriptorInCmResourceList(__in UCHAR Type,__in PCM_RESOURCE_LIST CmResList,__in PCM_PARTIAL_RESOURCE_DESCRIPTOR StartPoint);

//
// intialize partial list context
//
VOID PcipInitializePartialListContext(__in PPCI_PARTIAL_LIST_CONTEXT Context,__in PCM_PARTIAL_RESOURCE_LIST List,__in UCHAR Type);

//
// get next range
//
PCM_PARTIAL_RESOURCE_DESCRIPTOR PcipGetNextRangeFromList(__in PPCI_PARTIAL_LIST_CONTEXT Context);

//
// get next resource descriptor
//
PCM_PARTIAL_RESOURCE_DESCRIPTOR PciNextPartialDescriptor(__in PCM_PARTIAL_RESOURCE_DESCRIPTOR Current);

//
// insert at head
//
VOID PciInsertEntryAtHead(__in PSINGLE_LIST_ENTRY ListHead,__in PSINGLE_LIST_ENTRY ListEntry,__in PKEVENT Lock);

//
// insert at tail
//
VOID PciInsertEntryAtTail(__in PSINGLE_LIST_ENTRY ListHead,__in PSINGLE_LIST_ENTRY ListEntry,__in PKEVENT Lock);

//
// remove list entry
//
VOID PciRemoveEntryFromList(__in PSINGLE_LIST_ENTRY ListHead,__in PSINGLE_LIST_ENTRY ListEntry,__in PKEVENT Lock);

//
// link secondary extension
//
VOID PcipLinkSecondaryExtension(__in PSINGLE_LIST_ENTRY ListHead,__in PPCI_SECONDARY_EXTENSION SecondaryExtension,__in PKEVENT Lock,
								__in PCI_SIGNATURE Type,__in PCI_ARBITER_INSTANCE_DESTRUCTOR Destructor);

//
// destroy secondary extension
//
VOID PcipDestroySecondaryExtension(__in PSINGLE_LIST_ENTRY ListHead,__in PKEVENT Lock,__in PPCI_SECONDARY_EXTENSION SecondaryExtension);

//
// find next secondary extension
//
PPCI_SECONDARY_EXTENSION PciFindNextSecondaryExtension(__in PSINGLE_LIST_ENTRY FirstEntry,__in PCI_SIGNATURE Type);

//
// string to USHORT
//
BOOLEAN PciStringToUSHORT(__in PWCH String,__out PUSHORT UShort);

//
// send io ctrl
//
NTSTATUS PciSendIoctl(__in PDEVICE_OBJECT DeviceObject,__in ULONG IoCode,__in PVOID Input,__in ULONG InputLength,__in PVOID Output,__in ULONG OutputLength);

//
// get hack flags
//
ULONGLONG PciGetHackFlags(__in USHORT VendorId,__in USHORT DeviceId,__in USHORT SubVendorId,__in USHORT SubSystemId,__in UCHAR RevisionId);

//
// read device caps
//
UCHAR PciReadDeviceCapability(__in PPCI_PDO_EXTENSION PdoExt,__in UCHAR StartOffset,__in UCHAR SearchId,__in PVOID Buffer,__in ULONG Length);

//
// is device on debug path
//
BOOLEAN PciIsDeviceOnDebugPath(__in PPCI_PDO_EXTENSION PdoExt);

//
// does acpi method exist
//
BOOLEAN PciIsSlotPresentInParentMethod(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG Method);

//
// can disable decodes
//
BOOLEAN PciCanDisableDecodes(__in PPCI_PDO_EXTENSION PdoExt,__in PPCI_COMMON_HEADER Config,__in ULONG HackFlagsLow,__in ULONG HackFlagsHi,__in BOOLEAN DefaultRet);

//
// enable/disable decodes
//
VOID PciDecodeEnable(__in PPCI_PDO_EXTENSION PdoExt,__in BOOLEAN Enable,__in PUSHORT Command);

//
// query interface
//
NTSTATUS PciQueryInterface(__in PPCI_COMMON_EXTENSION Ext,__in GUID const* Type,__in USHORT Size,__in USHORT Ver,__in PVOID Data,__in PINTERFACE Intrf,__in BOOLEAN Last);

//
// query legacy bus info
//
NTSTATUS PciQueryLegacyBusInformation(__in PPCI_FDO_EXTENSION FdoExt,__out PLEGACY_BUS_INFORMATION* Info);

//
// query bus info
//
NTSTATUS PciQueryBusInformation(__in PPCI_PDO_EXTENSION PdoExt,__in PPNP_BUS_INFORMATION* Info);

//
// is on vga path
//
BOOLEAN PciIsOnVGAPath(__in PPCI_PDO_EXTENSION PdoExt);

//
// is the same device
//
BOOLEAN PciIsSameDevice(__in PPCI_PDO_EXTENSION PdoExt);

//
// get device type
//
ULONG PciClassifyDeviceType(__in PPCI_PDO_EXTENSION PdoExt);

//
// read device space
//
NTSTATUS PciReadDeviceSpace(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG WhichSpace,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length,__out PULONG ReadLength);

//
// write device space
//
NTSTATUS PciWriteDeviceSpace(__in PPCI_PDO_EXTENSION PdoExt,__in ULONG WhichSpace,__in PVOID Buffer,__in ULONG Offset,__in ULONG Length,__out PULONG WritenLength);

//
// query caps
//
NTSTATUS PciQueryCapabilities(__in PPCI_PDO_EXTENSION PdoExt,__in PDEVICE_CAPABILITIES DeviceCaps);

//
// determine slot number
//
NTSTATUS PciDetermineSlotNumber(__in PPCI_PDO_EXTENSION PdoExt,__out PULONG SlotNumber);

//
// query power caps
//
NTSTATUS PciQueryPowerCapabilities(__in PPCI_PDO_EXTENSION PdoExt,__in PDEVICE_CAPABILITIES DeviceCaps);

//
// get interrupt assignment
//
NTSTATUS PciGetInterruptAssignment(__in PPCI_PDO_EXTENSION PdoExt,__out PUCHAR MinVector,__out PUCHAR MaxVector);

//
// get device caps
//
NTSTATUS PciGetDeviceCapabilities(__in PDEVICE_OBJECT DeviceObject,__in PDEVICE_CAPABILITIES DeviceCaps);

//
// get length from bar
//
ULONG PciGetLengthFromBar(__in ULONG BaseAddress);

//
// create io descriptor from bar
//
BOOLEAN PciCreateIoDescriptorFromBarLimit(__in PIO_RESOURCE_DESCRIPTOR IoResDesc,__in PULONG BaseAddress,__in BOOLEAN RomAddress);

//
// exclude ranges from window
//
NTSTATUS PciExcludeRangesFromWindow(__in ULONGLONG Start,__in ULONGLONG End,__in PRTL_RANGE_LIST FromList,__in PRTL_RANGE_LIST RemoveList);