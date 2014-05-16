
//**************************************************************************************
//	日期:	31:5:2004   
//	创建:	tiamo	
//	描述:	pnp
//**************************************************************************************

#include "stdafx.h"
#include "initguid.h"
#include "..\public.h"

#pragma alloc_text(PAGE,DoFdoPnP)
#pragma alloc_text(PAGE,DoPdoPnP)
#pragma alloc_text(PAGE,SendIrpToLowerDeviceSyn)

// fdo pnp
NTSTATUS DoFdoPnP(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PFdoExt pFdoExt = static_cast<PFdoExt>(pDevice->DeviceExtension);

	// nead call next driver
	BOOLEAN bCallNext = TRUE;

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);

	// inc io count
	IncIoCount(pFdoExt);

	// save minor code
	UCHAR uMinorCode = pIoStack->MinorFunction;

	switch(uMinorCode)
	{
	case IRP_MN_START_DEVICE:
		{
			// send down first
			status = SendIrpToLowerDeviceSyn(pFdoExt->m_pLowerDevice,pIrp);
			if(NT_SUCCESS(status))
			{
				// set device power state
				pFdoExt->m_devPowerState = PowerDeviceD0;
				POWER_STATE state;
				state.DeviceState = PowerDeviceD0;
				PoSetPowerState(pDevice,DevicePowerState,state);

				// set device interface state
				status = IoSetDeviceInterfaceState(&pFdoExt->m_symbolicName,TRUE);

				// set device pnp state
				SetNewPnpState(pFdoExt,IRP_MN_START_DEVICE);
			}

			// complete the irp
			pIrp->IoStatus.Status = status;
			IoCompleteRequest(pIrp,IO_NO_INCREMENT);

			// do not call down the device stack
			bCallNext = FALSE;
		}
		break;

		// set pnp state directly
	case IRP_MN_QUERY_REMOVE_DEVICE:
	case IRP_MN_QUERY_STOP_DEVICE:
		SetNewPnpState(pFdoExt,uMinorCode);
		break;

		// check for current pnp state,and restore it
	case IRP_MN_CANCEL_REMOVE_DEVICE:
		if(pFdoExt->m_ulCurrentPnpState == IRP_MN_QUERY_REMOVE_DEVICE)
			RestorePnpState(pFdoExt);

		break;

		// the same
	case IRP_MN_CANCEL_STOP_DEVICE:
		if(pFdoExt->m_ulCurrentPnpState == IRP_MN_QUERY_STOP_DEVICE)
			RestorePnpState(pFdoExt);

		break;

		// remove
	case IRP_MN_REMOVE_DEVICE:
		{
			// normal remove
			if(pFdoExt->m_ulCurrentPnpState != IRP_MN_SURPRISE_REMOVAL)
			{
				// just stop device interface
				if(pFdoExt->m_symbolicName.Buffer)
				{
					// set device interface false
					IoSetDeviceInterfaceState(&pFdoExt->m_symbolicName,FALSE);

					RtlFreeUnicodeString(&pFdoExt->m_symbolicName);
				}
			}

			// update pnp state
			SetNewPnpState(pFdoExt,IRP_MN_REMOVE_DEVICE);

			// dec outstandingio by 2
			DecIoCount(pFdoExt);

			DecIoCount(pFdoExt);

			// wait other irps finish
			KeWaitForSingleObject(&pFdoExt->m_evRemove,Executive,KernelMode,FALSE,NULL);

			// check pdo
			ExAcquireFastMutex(&pFdoExt->m_mutexEnumPdo);

			// if the pdo is present
			if(pFdoExt->m_pEnumPdo)
			{
				PPdoExt pPdoExt = static_cast<PPdoExt>(pFdoExt->m_pEnumPdo->DeviceExtension);

				// surprise removal.update those field
				if(pPdoExt->m_ulCurrentPnpState == IRP_MN_SURPRISE_REMOVAL)
				{
					pPdoExt->m_pParentFdo = NULL;
					pPdoExt->m_bReportMissing = TRUE;
					pFdoExt->m_pEnumPdo = NULL;
				}

				// delete the pdo device
				IoDeleteDevice(pFdoExt->m_pEnumPdo);
			}

			ExReleaseFastMutex(&pFdoExt->m_mutexEnumPdo);

			pIrp->IoStatus.Status = STATUS_SUCCESS;

			// call next driver,do not need to wait the finish
			IoSkipCurrentIrpStackLocation(pIrp);
			status = IoCallDriver(pFdoExt->m_pLowerDevice,pIrp);

			// first detach it from the device stack
			IoDetachDevice(pFdoExt->m_pLowerDevice);

			// then delete it,note that the device extension will become invalid,so dec need to check the minor code carefully
			IoDeleteDevice(pDevice);

			bCallNext = FALSE;
		}
		break;

		// stop
	case IRP_MN_STOP_DEVICE:
		DecIoCount(pFdoExt);

		KeWaitForSingleObject(&pFdoExt->m_evStop,Executive,KernelMode,FALSE,NULL);

		IncIoCount(pFdoExt);

		SetNewPnpState(pFdoExt,IRP_MN_STOP_DEVICE);
		break;
	
		// query bus relations
	case IRP_MN_QUERY_DEVICE_RELATIONS:
		{
			// only care bus relations
			if (BusRelations != pIoStack->Parameters.QueryDeviceRelations.Type) 
			{
				/*switch(pIoStack->Parameters.QueryDeviceRelations.Type)
				{
				case EjectionRelations:
					devDebugPrint("\tquery EjectionRelations\n");
					break;
				case PowerRelations:
					devDebugPrint("\tquery PowerRelations\n");
					break;
				case RemovalRelations:
					devDebugPrint("\tquery RemovalRelations\n");
					break;
				case TargetDeviceRelation:
					devDebugPrint("\tquery TargetDeviceRelation\n");
					break;
				case SingleBusRelations:
					devDebugPrint("\tquery SingleBusRelations\n");
					break;
				}*/
				
				break;
			}

			// old relations
			PDEVICE_RELATIONS pOldRel = static_cast<PDEVICE_RELATIONS>(ULongToPtr(pIrp->IoStatus.Information));
			ULONG ulNewCount = 0;

			ExAcquireFastMutex(&pFdoExt->m_mutexEnumPdo);

			// no pdo
			if(!pFdoExt->m_pEnumPdo)
			{
				devDebugPrint("\tBusRelations no device plugin \n");
                ExReleaseFastMutex(&pFdoExt->m_mutexEnumPdo);
				break;
			}
			
			PPdoExt pPdoExt = static_cast<PPdoExt>(pFdoExt->m_pEnumPdo->DeviceExtension);
			
			// if the pdo is not present
			if(!pPdoExt->m_bPresent)
			{
				// then we report it as missing
				pPdoExt->m_bReportMissing = TRUE;
			}
			else
			{
				// report the pdo
				ObReferenceObject(pFdoExt->m_pEnumPdo);
				ulNewCount ++;
			}

			// add the old count
			if(pOldRel)
				ulNewCount += pOldRel->Count;

			// allocate paged memory
			ULONG len = sizeof(DEVICE_RELATIONS) + sizeof(PDEVICE_OBJECT) * (ulNewCount - 1);
			PDEVICE_RELATIONS pRel = static_cast<PDEVICE_RELATIONS>(ExAllocatePoolWithTag(PagedPool,len,'suBT'));

			// do not set the status,we should continue with the orignal devices that the upper devices reported
			if(!pRel)
				break;

			// copy old value
			if(pOldRel)
			{
				RtlCopyMemory(pRel,pOldRel,len - sizeof(PDEVICE_OBJECT));
				if(pPdoExt->m_bPresent)
					pRel->Objects[pOldRel->Count] = pFdoExt->m_pEnumPdo;

				// free the previous buffer
				ExFreePool(pOldRel);
			}
			else
			{
				// the device is present
				if(pPdoExt->m_bPresent)
					pRel->Objects[0] = pFdoExt->m_pEnumPdo;
			}

			pRel->Count = ulNewCount;

			pIrp->IoStatus.Information = PtrToUlong(pRel);

			devDebugPrint("\tBusRelations pdo present %d,report %d\n",pPdoExt->m_bPresent,ulNewCount);

			ExReleaseFastMutex(&pFdoExt->m_mutexEnumPdo);
		}
		break;

		// surprise removal
	case IRP_MN_SURPRISE_REMOVAL:
		{
			// set pnp state
			SetNewPnpState(pFdoExt,IRP_MN_SURPRISE_REMOVAL);

			// stop the fdo
			if(pFdoExt->m_symbolicName.Buffer)
			{
				// set device interface
				IoSetDeviceInterfaceState(&pFdoExt->m_symbolicName,FALSE);

				RtlFreeUnicodeString(&pFdoExt->m_symbolicName);
			}

			// update pdo's field
			ExAcquireFastMutex(&pFdoExt->m_mutexEnumPdo);
			PPdoExt pPdoExt = static_cast<PPdoExt>(pFdoExt->m_pEnumPdo->DeviceExtension);
			pPdoExt->m_pParentFdo = NULL;
			pPdoExt->m_bReportMissing = TRUE;
			ExReleaseFastMutex(&pFdoExt->m_mutexEnumPdo);
		}
		break;

	default:
		status = pIrp->IoStatus.Status;
		break;
	}

	// nead call lower device
	if(bCallNext)
	{
		pIrp->IoStatus.Status = status;
		IoSkipCurrentIrpStackLocation(pIrp);
		status = IoCallDriver(pFdoExt->m_pLowerDevice,pIrp);
	}
	
	// specail check for remove irp
	if(uMinorCode != IRP_MN_REMOVE_DEVICE)
		DecIoCount(pFdoExt);

	return status;
}

// for pdo
NTSTATUS DoPdoPnP(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PPdoExt pPdoExt = static_cast<PPdoExt>(pDevice->DeviceExtension);

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);

	switch(pIoStack->MinorFunction)
	{
	case IRP_MN_START_DEVICE:
		// set power state
		pPdoExt->m_devPowerState = PowerDeviceD0;
		POWER_STATE state;
		state.DeviceState = PowerDeviceD0;
		PoSetPowerState(pDevice,DevicePowerState,state);

		// set pnp state directly
	case IRP_MN_STOP_DEVICE:
	case IRP_MN_QUERY_STOP_DEVICE:
	case IRP_MN_QUERY_REMOVE_DEVICE:
		SetNewPnpState(pPdoExt,pIoStack->MinorFunction);
		break;

		// check prev state
	case IRP_MN_CANCEL_REMOVE_DEVICE:
		if(pPdoExt->m_ulCurrentPnpState == IRP_MN_QUERY_REMOVE_DEVICE)
		{
			RestorePnpState(pPdoExt);
		}
		break;

		// the same
	case IRP_MN_CANCEL_STOP_DEVICE:
		if(pPdoExt->m_ulCurrentPnpState == IRP_MN_QUERY_STOP_DEVICE)
		{
			RestorePnpState(pPdoExt);
		}
		break;

		// remove
	case IRP_MN_REMOVE_DEVICE:
		{
			// delete only if we have reported the device physical removal
			if(pPdoExt->m_bReportMissing)
			{
				SetNewPnpState(pPdoExt,IRP_MN_REMOVE_DEVICE);

				PDEVICE_OBJECT pFdo = pPdoExt->m_pParentFdo;
				if(pFdo)
				{
					PFdoExt pFdoExt = static_cast<PFdoExt>(pFdo->DeviceExtension);

					// update fdo's pointer
					ExAcquireFastMutex(&pFdoExt->m_mutexEnumPdo);
					pFdoExt->m_pEnumPdo = NULL;
					ExReleaseFastMutex(&pFdoExt->m_mutexEnumPdo);
				}

				// delete device
				IoDeleteDevice(pDevice);
			}

			// if it's present
			if(pPdoExt->m_bPresent)
			{
				// set it as stopped
				SetNewPnpState(pPdoExt,IRP_MN_STOP_DEVICE);
			}
		}
		break;

		// query caps
	case IRP_MN_QUERY_CAPABILITIES:
		{
			PDEVICE_CAPABILITIES pCaps = pIoStack->Parameters.DeviceCapabilities.Capabilities;

			// version check
			if(pCaps->Version != 1 || pCaps->Size < sizeof(DEVICE_CAPABILITIES))
			{
				status = STATUS_UNSUCCESSFUL; 
				break;
			}

			IO_STATUS_BLOCK     ioStatus;
			KEVENT              pnpEvent;
			PDEVICE_OBJECT      pTargetObject;
			PIO_STACK_LOCATION  pIrpStack;
			PIRP                pPnpIrp;

			DEVICE_CAPABILITIES parentCaps;

			RtlZeroMemory(&parentCaps,sizeof(DEVICE_CAPABILITIES));
			parentCaps.Size = sizeof(DEVICE_CAPABILITIES);
			parentCaps.Version = 1;
			parentCaps.Address = -1;
			parentCaps.UINumber = -1;

			KeInitializeEvent(&pnpEvent,NotificationEvent,FALSE);

			pTargetObject = IoGetAttachedDeviceReference(
								static_cast<PFdoExt>(pPdoExt->m_pParentFdo->DeviceExtension)->m_pLowerDevice);

			// get parent fdo's caps
			pPnpIrp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP,pTargetObject,NULL,0,NULL,&pnpEvent,&ioStatus);
			if(pPnpIrp == NULL) 
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
			}
			else
			{
				pPnpIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
				pIrpStack = IoGetNextIrpStackLocation(pPnpIrp);

				RtlZeroMemory(pIrpStack,sizeof(IO_STACK_LOCATION));
				pIrpStack->MajorFunction = IRP_MJ_PNP;
				pIrpStack->MinorFunction = IRP_MN_QUERY_CAPABILITIES;
				pIrpStack->Parameters.DeviceCapabilities.Capabilities = pCaps;
				status = IoCallDriver(pTargetObject,pPnpIrp);
				if (status == STATUS_PENDING) 
				{
					KeWaitForSingleObject(&pnpEvent,Executive,KernelMode,FALSE,NULL);
					status = ioStatus.Status;
				}
			}

			// dec the ref of the fdo's stack upper device
			ObDereferenceObject(pTargetObject);

			// copy the device state
			RtlCopyMemory(pCaps->DeviceState,parentCaps.DeviceState,(PowerSystemShutdown + 1) * sizeof(DEVICE_POWER_STATE));

			// set our own supported device state
			pCaps->DeviceState[PowerSystemWorking] = PowerDeviceD0;

			if(pCaps->DeviceState[PowerSystemSleeping1] != PowerDeviceD0)
				pCaps->DeviceState[PowerSystemSleeping1] = PowerDeviceD3;

			if(pCaps->DeviceState[PowerSystemSleeping2] != PowerDeviceD0)
				pCaps->DeviceState[PowerSystemSleeping2] = PowerDeviceD3;

			if(pCaps->DeviceState[PowerSystemSleeping3] != PowerDeviceD0)
				pCaps->DeviceState[PowerSystemSleeping3] = PowerDeviceD3;

			// donot support d1 and d2
			pCaps->DeviceD1 = pCaps->DeviceD2 = FALSE;

			// no wake
			pCaps->DeviceWake = PowerDeviceUnspecified;
			pCaps->SystemWake = PowerSystemUnspecified;

			pCaps->WakeFromD0 = FALSE;
			pCaps->WakeFromD1 = FALSE;
			pCaps->WakeFromD2 = FALSE;
			pCaps->WakeFromD3 = FALSE;

			// no latency
			pCaps->D1Latency = 0;
			pCaps->D2Latency = 0;
			pCaps->D3Latency = 0;

			// can eject
			pCaps->EjectSupported = TRUE;

			// don't disable
			pCaps->HardwareDisabled = FALSE;

			// can be removed
			pCaps->Removable = TRUE;

			// don't display surprise remove warning dlg
			pCaps->SurpriseRemovalOK = TRUE;

			// no unique id
			pCaps->UniqueID = FALSE;

			// nead user action for install
			pCaps->SilentInstall = FALSE;

			// bus address
			pCaps->Address = 0;

			// ui display number
			pCaps->UINumber = 0;
		}
		break;

		// query pdo id
	case IRP_MN_QUERY_ID:
		{
			switch(pIoStack->Parameters.QueryId.IdType)
			{
			case BusQueryInstanceID:
				{
					PVOID buffer = ExAllocatePoolWithTag(PagedPool,10,'suBT');
					if(!buffer) 
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						break;
					}

					RtlStringCchPrintfW(static_cast<PWCHAR>(buffer),10,L"%04d",0);
					pIrp->IoStatus.Information = PtrToUlong(buffer);
					devDebugPrint("\tBusQueryInstanceID\n");
				}
				break;

			case BusQueryDeviceID:
				{
					PVOID buffer = ExAllocatePoolWithTag(PagedPool,PDO_DEVICE_ID_LENGTH,'suBT');
					if(!buffer) 
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						break;
					}
					RtlCopyMemory(buffer,PDO_DEVICE_ID,PDO_DEVICE_ID_LENGTH);
					pIrp->IoStatus.Information = PtrToUlong(buffer);
					devDebugPrint("\tBusQueryDeviceID\n");
				}
				break;

			case BusQueryHardwareIDs:
				{
					PVOID buffer = ExAllocatePoolWithTag(PagedPool,PDO_HARDWARE_IDS_LENGTH,'suBT');
					if(!buffer) 
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						break;
					}
					RtlCopyMemory(buffer,PDO_HARDWARE_IDS,PDO_HARDWARE_IDS_LENGTH);
					pIrp->IoStatus.Information = PtrToUlong(buffer);
					devDebugPrint("\tBusQueryHardwareIDs\n");
				}
				break;

			case BusQueryCompatibleIDs:
				{
					PVOID buffer = ExAllocatePoolWithTag(PagedPool,PDO_COMPATIBLE_IDS_LENGTH,'suBT');
					if(!buffer) 
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						break;
					}
					RtlCopyMemory(buffer,PDO_COMPATIBLE_IDS,PDO_COMPATIBLE_IDS_LENGTH);
					pIrp->IoStatus.Information = PtrToUlong(buffer);
					devDebugPrint("\tBusQueryCompatibleIDs\n");
				}
				break;
			}
		}
		break;

		// query text
	case IRP_MN_QUERY_DEVICE_TEXT:
		{
			switch (pIoStack->Parameters.QueryDeviceText.DeviceTextType) 
			{
			case DeviceTextDescription:
				if(!pIrp->IoStatus.Information) 
				{
					PVOID buffer = ExAllocatePoolWithTag (PagedPool,PDO_TEXT_LENGTH,'suBT');
					if(!buffer) 
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
						break;
					}

					RtlStringCchPrintfW(static_cast<PWCHAR>(buffer),PDO_TEXT_LENGTH,L"%ws",PDO_TEXT);
					
					pIrp->IoStatus.Information = PtrToUlong(buffer);
					devDebugPrint("\tDeviceTextDescription\n");
				}
				break;

			default:
				status = pIrp->IoStatus.Status;
				break;
			}
		}
		break;

		// boot resource
	case IRP_MN_QUERY_RESOURCES:
		{
			PCM_RESOURCE_LIST pResList = static_cast<PCM_RESOURCE_LIST>(ExAllocatePoolWithTag(PagedPool,
																			sizeof(CM_RESOURCE_LIST),'suBT'));

			if(pResList)
			{
				// shareed busnumber resource 
				RtlZeroMemory(pResList,sizeof(CM_RESOURCE_LIST));

				pResList->Count = 1;
				pResList->List[0].BusNumber = 0;
				pResList->List[0].InterfaceType = Internal;
				pResList->List[0].PartialResourceList.Count = 1;
				pResList->List[0].PartialResourceList.Revision = 1;
				pResList->List[0].PartialResourceList.Version = 1;
				pResList->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareShared;
				pResList->List[0].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypeBusNumber;
				pResList->List[0].PartialResourceList.PartialDescriptors[0].u.BusNumber.Length = 1;

				pIrp->IoStatus.Information = PtrToUlong(pResList);
			}
			else
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
			}
		}
		break;

		// resource requirements
	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
		{
			PIO_RESOURCE_REQUIREMENTS_LIST pList = static_cast<PIO_RESOURCE_REQUIREMENTS_LIST>(
										ExAllocatePoolWithTag(PagedPool,sizeof(IO_RESOURCE_REQUIREMENTS_LIST),'suBT'));

			if(pList)
			{
				RtlZeroMemory(pList,sizeof(IO_RESOURCE_REQUIREMENTS_LIST));

				pList->ListSize = sizeof(IO_RESOURCE_REQUIREMENTS_LIST);
				pList->AlternativeLists = 1;
				pList->InterfaceType = InterfaceTypeUndefined;
				pList->BusNumber = 0;
				pList->List[0].Version = 1;
				pList->List[0].Revision = 1;
				pList->List[0].Count = 1;
				pList->List[0].Descriptors[0].Option = IO_RESOURCE_PREFERRED;
				pList->List[0].Descriptors[0].ShareDisposition = CmResourceShareShared;

				pList->List[0].Descriptors[0].Type = CmResourceTypeBusNumber;
				pList->List[0].Descriptors[0].u.BusNumber.MaxBusNumber = 0x10;
				pList->List[0].Descriptors[0].u.BusNumber.Length = 1;

				pIrp->IoStatus.Information = PtrToUlong(pList);
			}
			else
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
			}
		}
		break;

		// bus info
	case IRP_MN_QUERY_BUS_INFORMATION:
		{
			PPNP_BUS_INFORMATION busInfo;

			busInfo = static_cast<PPNP_BUS_INFORMATION>(ExAllocatePoolWithTag(PagedPool, sizeof(PNP_BUS_INFORMATION),'suBT'));

			if (busInfo == NULL) 
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			busInfo->BusTypeGuid = GUID_TIAMO_BUS;

			busInfo->LegacyBusType = Internal;

			busInfo->BusNumber = 0;

			pIrp->IoStatus.Information = PtrToUlong(busInfo);
		}
		break;

		// usage
	case IRP_MN_DEVICE_USAGE_NOTIFICATION:
		status = STATUS_UNSUCCESSFUL;
		break;

	case IRP_MN_EJECT:
		{
			// device physical removed
			pPdoExt->m_bPresent = FALSE;
		}
		break;

	//case IRP_MN_QUERY_INTERFACE:
	//	break;

		// target relations
	case IRP_MN_QUERY_DEVICE_RELATIONS:
		{
			if(pIoStack->Parameters.QueryDeviceRelations.Type != TargetDeviceRelation)
			{
				/*switch(pIoStack->Parameters.QueryDeviceRelations.Type)
				{
				case EjectionRelations:
				devDebugPrint("\tquery EjectionRelations\n");
				break;
				case PowerRelations:
				devDebugPrint("\tquery PowerRelations\n");
				break;
				case RemovalRelations:
				devDebugPrint("\tquery RemovalRelations\n");
				break;
				case BusDeviceRelation:
				devDebugPrint("\tquery BusDeviceRelation\n");
				break;
				case SingleBusRelations:
				devDebugPrint("\tquery SingleBusRelations\n");
				break;
				}*/
				break;
			}

			PDEVICE_RELATIONS pRel = static_cast<PDEVICE_RELATIONS>(ExAllocatePoolWithTag(PagedPool,sizeof(DEVICE_RELATIONS),'suBT'));
			if(!pRel) 
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			pRel->Count = 1;
			pRel->Objects[0] = pDevice;
			ObReferenceObject(pDevice);

			status = STATUS_SUCCESS;
			pIrp->IoStatus.Information = PtrToUlong(pRel);
		}
		break;

	default:
		status = pIrp->IoStatus.Status;
		break;
	}

	// pdo should complete the irp
	pIrp->IoStatus.Status = status;
	IoCompleteRequest (pIrp, IO_NO_INCREMENT);

	return status;
}

// dispatch pnp
NTSTATUS DispatchPnP(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	NTSTATUS status;
	PCommonExt pCommonExt = static_cast<PCommonExt>(pDevice->DeviceExtension);

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);

	// delete pendind
	if( pCommonExt->m_ulCurrentPnpState == IRP_MN_REMOVE_DEVICE)
	{
		pIrp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE ;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return status;
	}

#ifdef DBG
	PrintPnpCall(pDevice,pIrp);
#endif

	if(pCommonExt->m_bFdo)
		status = DoFdoPnP(pDevice,pIrp);
	else
		status = DoPdoPnP(pDevice,pIrp);

	return status;
}

// completion routine
NTSTATUS CompletionRoutine(PDEVICE_OBJECT pDevice,PIRP pIrp,PVOID pContext)
{
	if(pIrp->PendingReturned == TRUE) 
	{
		KeSetEvent(static_cast<PKEVENT>(pContext),IO_NO_INCREMENT,FALSE);
	}

	// irp will complete later
	return STATUS_MORE_PROCESSING_REQUIRED;
}

// send irp down synchronously
NTSTATUS SendIrpToLowerDeviceSyn(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	KEVENT   event;
	NTSTATUS status;

	KeInitializeEvent(&event,NotificationEvent,FALSE);

	IoCopyCurrentIrpStackLocationToNext(pIrp);

	IoSetCompletionRoutine(pIrp,CompletionRoutine,&event,TRUE,TRUE,TRUE);

	status = IoCallDriver(pDevice,pIrp);

	if (status == STATUS_PENDING) 
	{
		KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
		status = pIrp->IoStatus.Status;
	}

	return status;
}

#ifdef DBG
void PrintPnpCall(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	static PCHAR szDevice[] = {"pdo","fdo"};
	static PCHAR szMinor[] = 
	{
		"Start",
		"QueryRemove",
		"Remove",
		"CancelRemove",
		"Stop",
		"QueryStop",
		"CancelStop",
		"QueryRelations",
		"QueryInterface",
		"QueryCaps",
		"QueryResource",
		"QueryResourceRequirements",
		"QueryText",
		"FilterResourceRequirements",
		"0x0E",
		"ReadConfig",
		"WriteConfig",
		"Eject",
		"SetLock",
		"QueryID",
		"QueryPnpState",
		"QueryBusInfo",
		"DeviceUsageNotification",
		"SurpriseRemove",
		"QueryLegacyBusInfo",
	};

	PCommonExt pCommonExt = static_cast<PCommonExt>(pDevice->DeviceExtension);
	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);

	devDebugPrint(DRIVER_NAME"*******IRP_MJ_PNP - %s - %s \n",szMinor[pIoStack->MinorFunction],szDevice[pCommonExt->m_bFdo]);
}
#endif