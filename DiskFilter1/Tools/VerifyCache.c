#include "../Cache.h"
#include <stdio.h>
#include <time.h>

#define		MAX_SECTOR	20000
#define		TEST_LOOPS	10000
#pragma pack(1)
typedef union _SECTOR
{
	int			i;
	UCHAR		d[SECTOR_SIZE];
}SECTOR, *PSECTOR;
typedef struct _VDISK
{
	ULONG		Sectors;
	PSECTOR		Content;
	PSECTOR		Backup;
}VDISK, *PVDISK;
#pragma pack()
static VDISK VDisk;
static CACHE_POOL CachePool;
void InitVDisk (PVDISK VDisk);
void SimulateReadWrite (PVDISK VDisk, PCACHE_POOL CachePool);
void FlushBackPoolData (PVDISK VDisk, PCACHE_POOL CachePool);
void VerifyDiskContest (PVDISK VDisk);

int main()
{
	srand((unsigned int)time(NULL));
	InitVDisk(&VDisk);
	InitCachePool(&CachePool);
	SimulateReadWrite(&VDisk, &CachePool);
	FlushBackPoolData(&VDisk, &CachePool);
	VerifyDiskContest(&VDisk);
	DestroyCachePool(&CachePool);
	return 0;
}

void InitVDisk (PVDISK VDisk)
{
	ULONG i;
	printf("Init Virtual Disk ...\n");
	VDisk->Sectors = MAX_SECTOR;
	VDisk->Content = (PSECTOR)malloc(MAX_SECTOR * sizeof(SECTOR));
	VDisk->Backup = (PSECTOR)malloc(MAX_SECTOR * sizeof(SECTOR));
	assert(VDisk->Content);
	assert(VDisk->Backup);
	for (i = 0; i < MAX_SECTOR; i++)
		VDisk->Content[i].i = rand();
	memcpy(VDisk->Backup, VDisk->Content, MAX_SECTOR * sizeof(SECTOR));
}

void SimulateReadWrite (PVDISK VDisk, PCACHE_POOL CachePool)
{
	ULONG i, Length;
	LARGE_INTEGER Offset;
	PVOID Buffer, DiskPtr;
	printf("Simulate R/W ...\n");
	for (i = 0; i < TEST_LOOPS; i++)
	{
		Offset.QuadPart = rand() % MAX_SECTOR;
		Length = 1 + (rand() % (MAX_SECTOR - Offset.QuadPart));
		Offset.QuadPart *= SECTOR_SIZE;
		Length *= SECTOR_SIZE;

		Buffer = malloc(Length);
		assert(Buffer);
		if(rand() % 2)
		{
			if (QueryAndCopyFromCachePool(
					CachePool, Buffer, Offset, Length))
			{
				DiskPtr = (PUCHAR)(VDisk->Backup) + Offset.QuadPart;
				// completely from cache pool
				if (memcmp(DiskPtr, Buffer, Length) != 0)
				{
					printf("Not Matched!\n");
					break;
				}
			}
			else
			{
				// read from disk
				DiskPtr = (PUCHAR)(VDisk->Content) + Offset.QuadPart;
				memcpy(Buffer, DiskPtr, Length);
				// update pool
				UpdataCachePool(
					CachePool, Buffer, Offset, Length, _READ_);
			}
		}
		else //WRITE
		{
			// generate write data
			for (i = 0; i < Length/SECTOR_SIZE; i++)
				((PSECTOR)((PUCHAR)Buffer + i*SECTOR_SIZE))->i = rand();
			//TODO: sync disk, this should be done by cache algorithm
			DiskPtr = (PUCHAR)(VDisk->Content) + Offset.QuadPart;
			memcpy(DiskPtr, Buffer, Length);
			// sync backup disk
			DiskPtr = (PUCHAR)(VDisk->Backup) + Offset.QuadPart;
			memcpy(DiskPtr, Buffer, Length);
			// update pool
			UpdataCachePool(
					CachePool, Buffer, Offset, Length, _WRITE_);
		}
		free(Buffer);
	}
}

void FlushBackPoolData (PVDISK VDisk, PCACHE_POOL CachePool)
{}

void VerifyDiskContest (PVDISK VDisk)
{
	if (memcmp(VDisk->Content, VDisk->Backup, MAX_SECTOR * sizeof(SECTOR)) != 0)
	{
		printf("Disk verify Error!\n");
	}
	printf("Disk verify Passed!\n");
}

