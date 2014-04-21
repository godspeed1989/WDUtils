#include <ntddk.h>
#include "Lock.h"

static LONG lock;

void init_spin_lock (PLONG lock)
{
	*lock = 0;
}

void spin_lock (PLONG lock)
{
	/* Try to acquire the lock */
	while (InterlockedCompareExchange(lock, 1, 0))
	{
		/* It's locked... spin until it's unlocked */
		while (*(PLONG)lock & 1)
		{
			;/* Yield and keep looping */
		}
	}
}

void spin_unlock (PLONG lock)
{
	InterlockedAnd(lock, 0);
}
