#pragma once

#include "Cache.h"

BOOLEAN InitQueue (PQueue Queue, ULONG Size);

VOID DestroyQueue (PQueue Queue);

BOOLEAN QueueInsert (PQueue Queue, QUEUE_DAT_T Entry);

QUEUE_DAT_T QueueRemove (PQueue Queue);

BOOLEAN QueueIsFull (PQueue Queue);

BOOLEAN QueueIsEmpty (PQueue Queue);
