#include "minifilter.h"

//  Context definitions we currently care about.
CONST FLT_CONTEXT_REGISTRATION ContextNotifications[] = {

     { FLT_VOLUME_CONTEXT,
       0,
       NPCleanupVolumeContext,
       sizeof(VOLUME_CONTEXT),
       CONTEXT_TAG },

     { FLT_CONTEXT_END }
};

//  operation registration
const FLT_OPERATION_REGISTRATION Callbacks[] =
{
    { IRP_MJ_CREATE,
      0,
      NPPreCreate,
      NPPostCreate
    },
    { IRP_MJ_READ,
      0,
      NPPreRead,
      NPPostRead
    },
    { IRP_MJ_OPERATION_END }
};

//  This defines what we want to filter with FltMgr
const FLT_REGISTRATION FilterRegistration =
{
    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    ContextNotifications,               //  Context
    Callbacks,                          //  Operation callbacks

    NPUnload,                           //  MiniFilterUnload

    NPInstanceSetup,                    //  InstanceSetup
    NPInstanceQueryTeardown,            //  InstanceQueryTeardown
    NPInstanceTeardownStart,            //  InstanceTeardownStart
    NPInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent
};
