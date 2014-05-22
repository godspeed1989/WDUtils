//********************************************************************
//	created:	23:7:2008   21:39
//	file:		pci.power.cpp
//	author:		tiamo
//	purpose:	power
//********************************************************************

#include "stdafx.h"

//
// fdo wait wake [checked]
//
NTSTATUS PciFdoWaitWake(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		PciAcquireLock(&FdoExt->Lock);

		//
		// child wait wake count must not be zero
		//
		if(!FdoExt->ChildWaitWakeCount)
			try_leave(Status = STATUS_DEVICE_BUSY;PciDebugPrintf(0x8000,"WaitWake (fdox %08x) Unexpected WaitWake IRP IGNORED.\n",FdoExt));

		//
		// if we already have a wait irp
		//
		if(FdoExt->PowerState.WaitWakeIrp)
			try_leave(Status = STATUS_DEVICE_BUSY;PciDebugPrintf(0x8000,"WaitWake: fdox %08x already waiting (%d waiters)\n",FdoExt,FdoExt->ChildWaitWakeCount));

		//
		// save this irp
		//
		FdoExt->PowerState.WaitWakeIrp					= Irp;
		PciDebugPrintf(0x8000,"WaitWake: fdox %08x is a now waiting for a wake event\n",FdoExt);

		//
		// set a complete routine
		//
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(Irp,&PciFdoWaitWakeCompletion,FdoExt,TRUE,TRUE,TRUE);

		Irp->IoStatus.Status							= STATUS_SUCCESS;
	}
	__finally
	{
		PciReleaseLock(&FdoExt->Lock);

		PoStartNextPowerIrp(Irp);

		if(NT_SUCCESS(Status))
		{
			Status										= PoCallDriver(FdoExt->AttachedDeviceObject,Irp);
		}
		else
		{
			Irp->IoStatus.Status						= Status;
			IoCompleteRequest(Irp,IO_NO_INCREMENT);
		}
	}

	return Status;
}

//
// fdo set power [checked]
//
NTSTATUS PciFdoSetPowerState(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	//
	// set device power state
	//
	if(IrpSp->Parameters.Power.Type == DevicePowerState)
	{
		FdoExt->PowerState.CurrentDeviceState			= IrpSp->Parameters.Power.State.DeviceState;
		return STATUS_SUCCESS;
	}

	//
	// we are not in started state
	//
	if(FdoExt->Common.DeviceState != PciStarted)
		return STATUS_NOT_SUPPORTED;

	//
	// set system power state
	//
	ASSERT(IrpSp->Parameters.Power.Type == SystemPowerState);

	SYSTEM_POWER_STATE NewState							= IrpSp->Parameters.Power.State.SystemState;

	//
	// reset
	//
	if(NewState == PowerSystemShutdown && IrpSp->Parameters.Power.ShutdownType == PowerActionShutdownReset)
		return STATUS_SUCCESS;

	ASSERT(NewState > PowerSystemUnspecified && NewState < PowerSystemMaximum);

	//
	// request a set device power irp
	//
	IoMarkIrpPending(Irp);
	POWER_STATE PowerState;
	PowerState.DeviceState								= FdoExt->PowerState.SystemStateMapping[NewState];
	PoRequestPowerIrp(FdoExt->FunctionalDeviceObject,IRP_MN_SET_POWER,PowerState,&PciFdoSetPowerStateCompletion,Irp,0);

	return STATUS_PENDING;
}

//
// fdo query power [checked]
//
NTSTATUS PciFdoIrpQueryPower(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_FDO_EXTENSION FdoExt)
{
	return STATUS_SUCCESS;
}

//
// fdo wait wake completion routine [checked]
//
NTSTATUS PciFdoWaitWakeCompletion(__in PDEVICE_OBJECT DeviceObject,__in PIRP Irp,__in PVOID Context)
{
	PPCI_FDO_EXTENSION FdoExt							= static_cast<PPCI_FDO_EXTENSION>(Context);

	ASSERT(FdoExt->Common.ExtensionType	== PciFdoExtensionType);

	PciDebugPrintf(0x8000,"WaitWake (fdox %08x) Completion routine, Irp %08x, IrpStatus = %08x\n",FdoExt,Irp,Irp->IoStatus.Status);

	NTSTATUS Status										= STATUS_SUCCESS;
	__try
	{
		PciAcquireLock(&FdoExt->Lock);

		if(Irp == FdoExt->PowerState.WaitWakeIrp || !FdoExt->PowerState.WaitWakeIrp)
		{
			FdoExt->PowerState.WaitWakeIrp				= 0;
			if(FdoExt->PowerState.CurrentDeviceState != PowerDeviceD0)
			{
				POWER_STATE PowerState;
				PowerState.DeviceState					= PowerDeviceD0;
				PoRequestPowerIrp(DeviceObject,IRP_MN_SET_POWER,PowerState,&PciFdoWaitWakeCallBack,Irp,0);
				Status									= STATUS_MORE_PROCESSING_REQUIRED;
			}
		}
	}
	__finally
	{
		PciReleaseLock(&FdoExt->Lock);
	}

	return Status;
}

//
// fdo wait wake callback [checked]
//
VOID PciFdoWaitWakeCallBack(__in PDEVICE_OBJECT DeviceObject,__in UCHAR MinorFunction,__in POWER_STATE PowerState,__in_opt PVOID Context,__in PIO_STATUS_BLOCK IoStatus)
{
	PIRP Irp											= static_cast<PIRP>(Context);

	PoStartNextPowerIrp(Irp);

	Irp->IoStatus.Status								= IoStatus->Status;

	IoCompleteRequest(Irp,IO_NO_INCREMENT);
}

//
// fdo set power state callback [checked]
//
VOID PciFdoSetPowerStateCompletion(__in PDEVICE_OBJECT DeviceObject,__in UCHAR MinorFunction,__in POWER_STATE PowerState,
								   __in_opt PVOID Context,__in PIO_STATUS_BLOCK IoStatus)
{
	ASSERT(NT_SUCCESS(IoStatus->Status));

	PIRP Irp											= static_cast<PIRP>(Context);
	PPCI_FDO_EXTENSION FdoExt							= static_cast<PPCI_FDO_EXTENSION>(DeviceObject->DeviceExtension);
	PIO_STACK_LOCATION IrpSp							= IoGetCurrentIrpStackLocation(Irp);

	ASSERT(FdoExt->Common.ExtensionType == PciFdoExtensionType);

	//
	// from hibernated to working
	//
	if(IrpSp->Parameters.Power.State.SystemState == PowerSystemWorking && FdoExt->Hibernated)
	{
		FdoExt->Hibernated								= FALSE;
		PciScanHibernatedBus(FdoExt);
	}

	if(IrpSp->Parameters.Power.ShutdownType == PowerActionHibernate && IrpSp->Parameters.Power.State.SystemState > PowerSystemWorking)
		FdoExt->Hibernated								= TRUE;

	//
	// pass the set system power down to device stack
	//
	Irp->IoStatus.Status								= STATUS_SUCCESS;
	PoStartNextPowerIrp(Irp);
	IoCopyCurrentIrpStackLocationToNext(Irp);
	PoCallDriver(FdoExt->AttachedDeviceObject,Irp);
}

//
// pdo wait wake [checked]
//
NTSTATUS PciPdoWaitWake(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	PoStartNextPowerIrp(Irp);

	ASSERT(PdoExt->PowerState.CurrentDeviceState < PowerDeviceMaximum);

	NTSTATUS Status										= STATUS_SUCCESS;
	//
	// can we wait?
	//
	if(PdoExt->PowerState.CurrentDeviceState <= PdoExt->PowerState.DeviceWakeLevel && PdoExt->PowerState.DeviceWakeLevel != PowerDeviceUnspecified)
	{
		LONG WaitWakeCount								= 0;

		__try
		{
			PciAcquireLock(&PdoExt->Lock);
			//
			// already has a wait irp?
			//
			if(PdoExt->PowerState.WaitWakeIrp)
				try_leave(Status = STATUS_DEVICE_BUSY;PciDebugPrintf(0x8000,"WaitWake: pdox %08x is already waiting\n",PdoExt));

			//
			// get pme information
			//
			BOOLEAN HasPowerMgrCaps						= FALSE;
			PciPmeGetInformation(PdoExt->PhysicalDeviceObject,&HasPowerMgrCaps,0,0);
			if(!HasPowerMgrCaps)
				try_leave(Status = STATUS_INVALID_DEVICE_REQUEST;PciDebugPrintf(0x8000,"WaitWake: pdox %08x does not support PM\n",PdoExt));

			//
			// fake parent?
			//
			PPCI_FDO_EXTENSION ParentFdoExt				= PdoExt->ParentFdoExtension;
			ASSERT(ParentFdoExt->Common.ExtensionType == PciFdoExtensionType);
			if(ParentFdoExt->Fake)
				try_leave(Status = STATUS_PENDING);

		
			PciDebugPrintf(0x8000,"WaitWake: pdox %08x setting PMEEnable.\n",PdoExt);

			//
			// save this wake irp
			//
			PdoExt->PowerState.WaitWakeIrp				= Irp;
			IoMarkIrpPending(Irp);
			PdoExt->PowerState.SavedCancelRoutine		= IoSetCancelRoutine(Irp,&PciPdoWaitWakeCancelRoutine);
			ASSERT(!PdoExt->PowerState.SavedCancelRoutine);

			//
			// set pme enable
			//
			PciPdoAdjustPmeEnable(PdoExt,TRUE);

			//
			// increase parent's wait wake count
			//
			WaitWakeCount								= InterlockedIncrement(&ParentFdoExt->ChildWaitWakeCount);

			//
			// return pending
			//
			Status										= STATUS_PENDING;
		}
		__finally
		{
			PciReleaseLock(&PdoExt->Lock);
		}

		//
		// if wait wake count became to 1,request a wait wake irp
		//
		if(WaitWakeCount == 1)
		{
			POWER_STATE State;
			State.SystemState							= IrpSp->Parameters.WaitWake.PowerState;
			PoRequestPowerIrp(PdoExt->ParentFdoExtension->FunctionalDeviceObject,IRP_MN_WAIT_WAKE,State,&PciPdoWaitWakeCallBack,PdoExt->ParentFdoExtension,0);
		}
	}
	else
	{
		PciDebugPrintf(0x8000,"WaitWake: pdox %08x current state (%d) not valid for waiting\n",PdoExt,PdoExt->PowerState.CurrentDeviceState);
		Status											= STATUS_INVALID_DEVICE_STATE;
	}

	if(Status != STATUS_PENDING)
	{
		Irp->IoStatus.Status							= Status;
		IoCompleteRequest(Irp,IO_NO_INCREMENT);
	}

	return Status;
}

//
// pdo set power [checked]
//
NTSTATUS PciPdoSetPowerState(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	if(IrpSp->Parameters.Power.Type == SystemPowerState)
		return STATUS_SUCCESS;

	if(IrpSp->Parameters.Power.Type == DevicePowerState)
	{
		DEVICE_POWER_STATE NewState						= IrpSp->Parameters.Power.State.DeviceState;

		if(NewState != PowerDeviceD0)
		{
			//
			// invalid power state check
			//
			if(NewState < PowerDeviceD0 || NewState > PowerDeviceD3)
				return STATUS_INVALID_PARAMETER;

			//
			// notify debugger system
			//
			if(PdoExt->OnDebugPath)
				KdPowerTransition(NewState);

			//
			// save command enables
			//
			if(PdoExt->PowerState.CurrentDeviceState == PowerDeviceD0)
				PciReadDeviceConfig(PdoExt,&PdoExt->CommandEnables,FIELD_OFFSET(PCI_COMMON_HEADER,Command),sizeof(PdoExt->CommandEnables));

			//
			// set new state
			//
			PdoExt->PowerState.CurrentDeviceState		= NewState;

			//
			// disable power down hack
			//
			if(PdoExt->DisablePowerDown)
			{
				PciDebugPrintf(0x7fffffff,"PCI power down of PDOx %08x, disabled, ignored.\n",PdoExt);
				return STATUS_SUCCESS;
			}

			//
			// do not power down the hibernate and paging device if we are hibernate the system
			//
			POWER_ACTION ShutdownType					= IrpSp->Parameters.Power.ShutdownType;
			if(ShutdownType == PowerActionHibernate && (PdoExt->PowerState.Hibernate || PdoExt->PowerState.Paging))
				return STATUS_SUCCESS;

			//
			// going to d3 state for shutdown reset
			//
			if(NewState == PowerDeviceD3 && (ShutdownType == PowerActionShutdown || ShutdownType == PowerActionShutdownOff || ShutdownType == PowerActionShutdownReset))
			{
				//
				// if we are on the vga path,ignore it
				//
				if(PciIsOnVGAPath(PdoExt))
					return STATUS_SUCCESS;
			}

			//
			// if we are on debug path
			//
			if(PdoExt->OnDebugPath)
				return STATUS_SUCCESS;
		}
		else
		{
			//
			// already in d0 state
			//
			if(NewState == PdoExt->PowerState.CurrentDeviceState)
				return STATUS_SUCCESS;

			//
			// device changed?
			//
			if(!PciIsSameDevice(PdoExt))
				return STATUS_NO_SUCH_DEVICE;
		}

		//
		// set power state
		//
		NTSTATUS Status									= PciSetPowerManagedDevicePowerState(PdoExt,NewState,TRUE);

		//
		// we are powering down
		//
		if(NewState != PowerDeviceD0)
		{
			//
			// notify power system the new device power state
			//
			PoSetPowerState(PdoExt->PhysicalDeviceObject,DevicePowerState,IrpSp->Parameters.Power.State);

			//
			// disable decode
			//
			PciDecodeEnable(PdoExt,FALSE,0);

			return STATUS_SUCCESS;
		}

		//
		// failed to set device power
		//
		if(!NT_SUCCESS(Status))
			return Status;

		//
		// update new power state
		//
		PdoExt->PowerState.CurrentDeviceState			= PowerDeviceD0;
		PoSetPowerState(PdoExt->PhysicalDeviceObject,DevicePowerState,IrpSp->Parameters.Power.State);

		//
		// notify debugger system
		//
		if(PdoExt->OnDebugPath)
			KdPowerTransition(PowerDeviceD0);

		return Status;
	}

	return STATUS_NOT_SUPPORTED;
}

//
// pdo query power [checked]
//
NTSTATUS PciPdoIrpQueryPower(__in PIRP Irp,__in PIO_STACK_LOCATION IrpSp,__in PPCI_PDO_EXTENSION PdoExt)
{
	return STATUS_SUCCESS;
}

//
// pdo wait wake cancel [checked]
//
VOID PciPdoWaitWakeCancelRoutine(__in PDEVICE_OBJECT DeviceObject,__in PIRP Irp)
{
	PPCI_PDO_EXTENSION PdoExt								= static_cast<PPCI_PDO_EXTENSION>(DeviceObject->DeviceExtension);

	PciDebugPrintf(0x8000,"WaitWake (pdox %08x) Cancel routine, Irp %08x.\n",PdoExt,Irp);

	ASSERT(PdoExt->Common.ExtensionType == PciPdoExtensionType);

	IoReleaseCancelSpinLock(Irp->CancelIrql);

	PciAcquireLock(&PdoExt->Lock);

	if(PdoExt->PowerState.WaitWakeIrp)
	{
		PdoExt->PowerState.WaitWakeIrp						= 0;

		//
		// disable pme
		//
		PciPdoAdjustPmeEnable(PdoExt,FALSE);

		PPCI_FDO_EXTENSION ParentFdoExt						= PdoExt->ParentFdoExtension;
		ASSERT(ParentFdoExt->Common.ExtensionType == PciFdoExtensionType);

		//
		// decrease parent's child wait count
		//
		LONG WaitCount										= InterlockedDecrement(&ParentFdoExt->ChildWaitWakeCount);

		PciReleaseLock(&PdoExt->Lock);

		//
		// if the is the last child wait irp,we should cancel parent's wait irp
		//
		if(!WaitCount)
		{
			PciAcquireLock(&ParentFdoExt->Lock);
			PIRP ParentIrp									= ParentFdoExt->PowerState.WaitWakeIrp;
			ParentFdoExt->PowerState.WaitWakeIrp			= 0;
			PciReleaseLock(&ParentFdoExt->Lock);

			if(ParentIrp)
			{
				PciDebugPrintf(0x8000,"WaitWake (pdox %08x) zero waiters remain on parent, cancelling parent wait\n",PdoExt);
				IoCancelIrp(ParentIrp);
			}
		}

		//
		// complete this irp
		//
		Irp->IoStatus.Information							= 0;
		PoStartNextPowerIrp(Irp);
		Irp->IoStatus.Status								= STATUS_CANCELLED;
		IoCompleteRequest(Irp,IO_NO_INCREMENT);
	}
	else
	{
		PciReleaseLock(&PdoExt->Lock);
	}
}

//
// pdo wait wake callback [checked]
//
VOID PciPdoWaitWakeCallBack(__in PDEVICE_OBJECT DeviceObject,__in UCHAR MinorFunction,__in POWER_STATE PowerState,__in_opt PVOID Context,__in PIO_STATUS_BLOCK IoStatus)
{
	PPCI_FDO_EXTENSION ParentFdoExt							= static_cast<PPCI_FDO_EXTENSION>(Context);

	//
	// if the parent fdo has a pending wait wake irp,do nothing
	//
	PciAcquireLock(&ParentFdoExt->Lock);
	BOOLEAN ParentHasWaitWakeIrp							= ParentFdoExt->PowerState.WaitWakeIrp != 0;
	PciReleaseLock(&ParentFdoExt->Lock);

	if(ParentHasWaitWakeIrp)
		return;

	//
	// go through child list and complete child pdo's wait wake irp
	//
	PPCI_PDO_EXTENSION PdoExt								= CONTAINING_RECORD(ParentFdoExt->ChildPdoList.Next,PCI_PDO_EXTENSION,Common.ListEntry);
	while(PdoExt && ParentFdoExt->ChildWaitWakeCount)
	{
		PciAcquireLock(&PdoExt->Lock);

		if(PdoExt->PowerState.WaitWakeIrp)
		{
			BOOLEAN PmeAssert								= FALSE;
			PciPmeGetInformation(PdoExt->PhysicalDeviceObject,0,&PmeAssert,0);

			BOOLEAN CompleteWaitWake						= TRUE;

			//
			// if the pdo is the device that is signalling a PME or the wait irp is failed,we complete the wait wake irp in the pdo
			//
			if(PmeAssert)
				PciDebugPrintf(0x8000,"PCI - pdox %08x is signalling a PME\n",PdoExt);
			else if(!NT_SUCCESS(IoStatus->Status))
				PciDebugPrintf(0x8000,"PCI - waking pdox %08x because fdo wait failed %0x.\n",PdoExt);
			else
				CompleteWaitWake							= FALSE;

			if(CompleteWaitWake)
			{
				PciPdoAdjustPmeEnable(PdoExt,FALSE);
				PIRP PdoWaitWakeIrp							= PdoExt->PowerState.WaitWakeIrp;
				PdoExt->PowerState.WaitWakeIrp				= 0;

				//
				// clear cancel routine
				//
				IoSetCancelRoutine(PdoWaitWakeIrp,0);

				//
				// complete it
				//
				PoStartNextPowerIrp(PdoWaitWakeIrp);
				PdoWaitWakeIrp->IoStatus.Status				= IoStatus->Status;
				IoCompleteRequest(PdoWaitWakeIrp,IO_NO_INCREMENT);

				//
				// decrease parent's child wait count
				//
				ASSERT(ParentFdoExt->ChildWaitWakeCount > 0);
				InterlockedDecrement(&ParentFdoExt->ChildWaitWakeCount);
			}
		}

		PciReleaseLock(&PdoExt->Lock);

		PdoExt												= CONTAINING_RECORD(PdoExt->Common.ListEntry.Next,PCI_PDO_EXTENSION,Common.ListEntry);
	}

	if(!NT_SUCCESS(IoStatus->Status))
	{
		PciDebugPrintf(0x8000,"WaitWake (fdox %08x) - WaitWake Irp Failed %08x\n",ParentFdoExt,IoStatus->Status);
		return;
	}

	//
	// check we should request another wake irp
	//
	if(!ParentFdoExt->ChildWaitWakeCount)
	{
		PciDebugPrintf(0x8000,"WaitWake (fdox %08x) - WaitWake Irp Finished\n",ParentFdoExt);
	}
	else
	{
		PciDebugPrintf(0x8000,"WaitWake (fdox %08x) - WaitWake Irp restarted - count = %x\n",ParentFdoExt,ParentFdoExt->ChildWaitWakeCount);

		PoRequestPowerIrp(DeviceObject,IRP_MN_WAIT_WAKE,PowerState,&PciPdoWaitWakeCallBack,Context,0);
	}
}

//
// set power state [checked]
//
NTSTATUS PciSetPowerManagedDevicePowerState(__in PPCI_PDO_EXTENSION PdoExt,__in DEVICE_POWER_STATE PowerState,__in BOOLEAN SetResource)
{
	//
	// unable to disable device
	//
	if(!PciCanDisableDecodes(PdoExt,0,0,0,TRUE) && PowerState != PowerDeviceD0)
		return STATUS_SUCCESS;

	NTSTATUS Status										= STATUS_SUCCESS;

	__try
	{
		if(FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_PME_CAPS))
			try_leave(NOTHING);

		//
		// read pm caps
		//
		PCI_PM_CAPABILITY Caps;
		UCHAR PmCapsOffset								= PciReadDeviceCapability(PdoExt,PdoExt->CapabilitiesPtr,PCI_CAPABILITY_ID_POWER_MANAGEMENT,&Caps,sizeof(Caps));
		if(!PmCapsOffset)
			try_leave(Status = STATUS_INVALID_DEVICE_REQUEST;ASSERT(PmCapsOffset));

		//
		// setup new caps
		//
		if(PowerState == PowerDeviceUnspecified)
			try_leave(Status = STATUS_INVALID_PARAMETER;ASSERT(PowerState != PowerDeviceUnspecified));

		Caps.PMCSR.ControlStatus.PowerState				= PowerState - 1;

		if(PowerState == PowerDeviceD0 && Caps.PMC.Capabilities.Support.PMED3Cold)
			Caps.PMCSR.ControlStatus.PMEStatus			= TRUE;

		//
		// write it
		//
		ASSERT(PmCapsOffset);
		PciWriteDeviceConfig(PdoExt,&Caps.PMCSR,PmCapsOffset + FIELD_OFFSET(PCI_PM_CAPABILITY,PMCSR),sizeof(Caps.PMCSR));

		//
		// wait delay
		//
		Status											= PciStallForPowerChange(PdoExt,PowerState,PmCapsOffset);
	}
	__finally
	{
		if(NT_SUCCESS(Status) && SetResource && PowerState < PdoExt->PowerState.CurrentDeviceState)
			Status										= PciSetResources(PdoExt,TRUE,FALSE);
	}

	return Status;
}

//
// stall for power change [checked]
//
NTSTATUS PciStallForPowerChange(__in PPCI_PDO_EXTENSION PdoExt,__in DEVICE_POWER_STATE PowerState,__in UCHAR PmCapsOffset)
{
	ASSERT(PowerState > PowerDeviceUnspecified && PowerState < PowerDeviceMaximum);
	ASSERT(PdoExt->PowerState.CurrentDeviceState > PowerDeviceUnspecified && PdoExt->PowerState.CurrentDeviceState < PowerDeviceMaximum);
	ASSERT(!FlagOn(PdoExt->HackFlags.LowPart,PCI_HACK_FLAGS_LOW_NO_PME_CAPS));

	static ULONG PciPowerDelayTable[4][4] =
	{
		{0,		0,		200,	10000},
		{0,		0,		200,	10000},
		{200,	200,	0,		10000},
		{10000,	10000,	10000,	0},
	};

	ULONG Delay											= PciPowerDelayTable[PdoExt->PowerState.CurrentDeviceState - 1][PowerState - 1];

	for(ULONG i = 0; i < 100; i ++)
	{
		if(Delay)
		{
			if(KeGetCurrentIrql() >= DISPATCH_LEVEL)
			{
				KeStallExecutionProcessor(Delay);
			}
			else
			{
				LARGE_INTEGER Interval;
				Interval.QuadPart						= static_cast<LONGLONG>(Delay) * -10 - (KeQueryTimeIncrement() - 1);
				KeDelayExecutionThread(KernelMode,FALSE,&Interval);
			}
		}

		PCI_PMCSR PMCSR;
		PciReadDeviceConfig(PdoExt,&PMCSR,PmCapsOffset + FIELD_OFFSET(PCI_PM_CAPABILITY,PMCSR),sizeof(PMCSR));

		if(PMCSR.PowerState == PowerState - 1)
			return STATUS_SUCCESS;

		Delay											= 1000;
	}

	PVERIFIER_FAILURE_DATA Data							= PciVerifierRetrieveFailureData(2);
	ASSERT(Data);
	ULONG Value											= PowerState;
	VfFailDeviceNode(PdoExt->PhysicalDeviceObject,0xf6,2,Data->Offset4,&Data->Offset8,Data->FailureMessage,"%DevObj%Ulong",PdoExt->PhysicalDeviceObject,Value);

	return STATUS_DEVICE_PROTOCOL_ERROR;
}