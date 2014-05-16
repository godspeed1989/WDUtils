
//**************************************************************************************
//	日期:	29:6:2004   
//	创建:	tiamo	
//	描述:	power
//**************************************************************************************

#include "stdafx.h"

extern "C"
{
	VOID RequestPowerCompletionRoutine(PDEVICE_OBJECT pDevice,UCHAR ucMinorFunction,POWER_STATE powerState,
									   PVOID pContext,PIO_STATUS_BLOCK pIoStatus);

	NTSTATUS DevicePowerUpCompletionRoutine(PDEVICE_OBJECT pDevice,PIRP pIrp,PVOID pContext);

	NTSTATUS SystemPowerUpCompletionRoutine(PDEVICE_OBJECT pDevice,PIRP pIrp,PVOID pContext);

	NTSTATUS SendSetDevicePowerState(PDEVICE_OBJECT pDevice,DEVICE_POWER_STATE devStateNew);
}

//#pragma alloc_text(PAGE,RequestPowerCompletionRoutine)
//#pragma alloc_text(PAGE,DevicePowerUpCompletionRoutine)
//#pragma alloc_text(PAGE,SystemPowerUpCompletionRoutine)
//#pragma alloc_text(PAGE,SendSetDevicePowerState)
//#pragma alloc_text(PAGE,DispatchPower)

// the bus fdo is the power policy owner
NTSTATUS DispatchPower(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	NTSTATUS status;
	PCommonExt pComExt = static_cast<PCommonExt>(pDevice->DeviceExtension);

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);
	POWER_STATE_TYPE powerType = pIoStack->Parameters.Power.Type;
	POWER_STATE powerState = pIoStack->Parameters.Power.State;

	if(pComExt->m_bFdo)
	{
		PFdoExt pFdoExt = static_cast<PFdoExt>(pComExt);

		IncIoCount(pFdoExt);

		switch(pIoStack->MinorFunction)
		{
			// set power
		case IRP_MN_SET_POWER:
			switch(powerType)
			{
				// set device power
			case DevicePowerState:
				{
					// get request device power state
					DEVICE_POWER_STATE stateNew = powerState.DeviceState;
					
					// need power up
					if(stateNew < pFdoExt->m_devPowerState)
					{
						devDebugPrint(DRIVER_NAME"*******fdo set device power up.first call down the devie stack\n");
						// first call down,then do things in the completion routine
						PoStartNextPowerIrp(pIrp);

						IoCopyCurrentIrpStackLocationToNext(pIrp);

						IoSetCompletionRoutine(pIrp,DevicePowerUpCompletionRoutine,NULL,TRUE,TRUE,TRUE);

						status = PoCallDriver(pFdoExt->m_pLowerDevice,pIrp);
					}
					else
					{
						// need power down
						if(stateNew > pFdoExt->m_devPowerState)
						{
							devDebugPrint(DRIVER_NAME"*******fdo set device power down.just call down the devie stack.\n");
							pFdoExt->m_devPowerState = stateNew;
						}
						else
							devDebugPrint(DRIVER_NAME"*******fdo set device power the same.just call down the devie stack.\n");

						PoSetPowerState(pDevice,DevicePowerState,powerState);

						// call down
						PoStartNextPowerIrp(pIrp);

						IoSkipCurrentIrpStackLocation(pIrp);
						
						status = PoCallDriver(pFdoExt->m_pLowerDevice,pIrp);
					}
				}
				break;

				// set system power state
			case SystemPowerState:
				{
					pFdoExt->m_sysPowerState = powerState.SystemState;

					// get request device power state
					DEVICE_POWER_STATE stateNew = (powerState.SystemState <= PowerSystemWorking?PowerDeviceD0 : PowerDeviceD3);

					// power up
					if(stateNew < pFdoExt->m_devPowerState)
					{
						devDebugPrint(DRIVER_NAME"*******fdo set system power up.first call down the devie stack.\n");
						// first call down
						PoStartNextPowerIrp(pIrp);
						IoCopyCurrentIrpStackLocationToNext(pIrp);
						IoSetCompletionRoutine(pIrp,SystemPowerUpCompletionRoutine,NULL,TRUE,TRUE,TRUE);
						status = PoCallDriver(pFdoExt->m_pLowerDevice,pIrp);
					}
					else 
					{
						// power down
						if(stateNew > pFdoExt->m_devPowerState)
						{
							devDebugPrint(DRIVER_NAME"*******fdo set system power down.set device power first then call down the devie stack.\n");
							status = SendSetDevicePowerState(pDevice,stateNew);
						}
						else
							devDebugPrint(DRIVER_NAME"*******fdo set system power the same.just call down the devie stack.\n");
						
						PoSetPowerState(pDevice,SystemPowerState,powerState);

						// call down
						PoStartNextPowerIrp(pIrp);

						IoSkipCurrentIrpStackLocation(pIrp);
						
						status = PoCallDriver(pFdoExt->m_pLowerDevice,pIrp);
					}
				}
				break;
			}
			break;

		case IRP_MN_QUERY_POWER:
			devDebugPrint(DRIVER_NAME"*******fdo query power state,just call down\n");
		default:
			PoStartNextPowerIrp(pIrp);
			IoSkipCurrentIrpStackLocation(pIrp);
			status = PoCallDriver(static_cast<PFdoExt>(pComExt)->m_pLowerDevice,pIrp);
			break;
		}

		DecIoCount(pFdoExt);
	}
	else
	{
		// power for pdo
		switch(pIoStack->MinorFunction)
		{
		case IRP_MN_SET_POWER:
			{
				switch(powerType)
				{
					// call PoSetPowerState only
				case DevicePowerState:
					devDebugPrint(DRIVER_NAME"*******pdo set device power state,just finish the irp\n");
					PoSetPowerState(pDevice,DevicePowerState,powerState);
					pComExt->m_devPowerState = powerState.DeviceState;
					status = STATUS_SUCCESS;
					break;

					// upper driver should do the whole thing,here just finish it
				case SystemPowerState:
					devDebugPrint(DRIVER_NAME"*******pdo set system power state,just finish the irp\n");
					PoSetPowerState(pDevice,SystemPowerState,powerState);
					pComExt->m_sysPowerState = powerState.SystemState;
					status = STATUS_SUCCESS;
					break;
				}
			}
			break;

		case IRP_MN_QUERY_POWER:
			devDebugPrint(DRIVER_NAME"*******pdo query power state,just finish the irp\n");
			status = STATUS_SUCCESS;
			break;

		default:
			status = pIrp->IoStatus.Status;
			break;
		}

		pIrp->IoStatus.Status = status;
		PoStartNextPowerIrp(pIrp);
		IoCompleteRequest(pIrp,IO_NO_INCREMENT);
	}

	return status;
}

// lower device finish device power up
NTSTATUS DevicePowerUpCompletionRoutine(PDEVICE_OBJECT pDevice,PIRP pIrp,PVOID pContext)
{
	devDebugPrint(DRIVER_NAME"*******fdo set device power up.call down the devie stack finished,then set myself power state\n");
	if(pIrp->PendingReturned)
		IoMarkIrpPending(pIrp);

	if(!NT_SUCCESS(pIrp->IoStatus.Status))
		return pIrp->IoStatus.Status;

	PFdoExt pFdoExt = static_cast<PFdoExt>(pDevice->DeviceExtension);

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);

	pFdoExt->m_devPowerState = pIoStack->Parameters.Power.State.DeviceState;
	PoSetPowerState(pDevice,DevicePowerState,pIoStack->Parameters.Power.State);

	PoStartNextPowerIrp(pIrp);

	return STATUS_SUCCESS;
}

// lower driver finish system power up
NTSTATUS SystemPowerUpCompletionRoutine(PDEVICE_OBJECT pDevice,PIRP pIrp,PVOID pContext)
{
	devDebugPrint(DRIVER_NAME"*******fdo set system power up.call down the devie stack finished.then request the device power irp to power up the device itself.\n");
	if(pIrp->PendingReturned)
		IoMarkIrpPending(pIrp);

	if(!NT_SUCCESS(pIrp->IoStatus.Status))
		return pIrp->IoStatus.Status;

	PFdoExt pFdoExt = static_cast<PFdoExt>(pDevice->DeviceExtension);

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);

	POWER_STATE powerState = pIoStack->Parameters.Power.State;

	// get request device power state
	DEVICE_POWER_STATE stateNew = (powerState.SystemState <= PowerSystemWorking?PowerDeviceD0 : PowerDeviceD3);

	PoStartNextPowerIrp(pIrp);

	// power up
	if(stateNew < pFdoExt->m_devPowerState)
	{
		SendSetDevicePowerState(pDevice,stateNew);
	}

	PoSetPowerState(pDevice,SystemPowerState,powerState);

	return STATUS_SUCCESS;
}

typedef struct __tagRequestPowerConext
{
	KEVENT						m_event;
	NTSTATUS					m_status;
}RequestPowerConext,*PRequestPowerConext;

// send set power irp
NTSTATUS SendSetDevicePowerState(PDEVICE_OBJECT pDevice,DEVICE_POWER_STATE devStateNew)
{
	POWER_STATE stateNew;
	stateNew.DeviceState = devStateNew;

	RequestPowerConext context;
	KeInitializeEvent(&context.m_event,NotificationEvent,FALSE);
	context.m_status = STATUS_SUCCESS;

	PFdoExt pFdoExt = static_cast<PFdoExt>(pDevice->DeviceExtension);

	devDebugPrint(DRIVER_NAME"*******call PoRequestPowerIrp to set device power state because of the set system state irp.\n");

	NTSTATUS status = PoRequestPowerIrp(pFdoExt->m_pPhysicalDevice,IRP_MN_SET_POWER,stateNew,RequestPowerCompletionRoutine,
										&context,NULL);

	if(status == STATUS_PENDING)
	{
		KeWaitForSingleObject(&context.m_event,Executive,KernelMode,FALSE,NULL);
		status = context.m_status;
	}

	if(NT_SUCCESS(status) && pFdoExt->m_devPowerState != devStateNew)
	{
		pFdoExt->m_devPowerState = devStateNew;
		PoSetPowerState(pDevice,DevicePowerState,stateNew);
	}

	return status;
}

VOID RequestPowerCompletionRoutine(PDEVICE_OBJECT pDevice,UCHAR ucMinorFunction,POWER_STATE powerState,
								   PVOID pContext,PIO_STATUS_BLOCK pIoStatus)
{
	devDebugPrint(DRIVER_NAME"*******PoRequestPowerIrp completion routine just set the event object.\n");

	PRequestPowerConext pRequestPowerContext = static_cast<PRequestPowerConext>(pContext);
	pRequestPowerContext->m_status = pIoStatus->Status;

	KeSetEvent(&pRequestPowerContext->m_event,IO_NO_INCREMENT,FALSE);
}