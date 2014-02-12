#include <ntddk.h>

#define BITMAP_TAG 'tbmp'
typedef struct _Pool
{
	RTL_BITMAP	Bitmap;
}Pool;

VOID test_bitmap()
{
	Pool		pool;
	ULONG		Bitmap_Size, Index;
	PULONG		Bitmap_Buffer;

	// Initialize
	Bitmap_Size = 13; // Number of bits
	Bitmap_Buffer = ExAllocatePoolWithTag (
		NonPagedPool,
		(ULONG)(((Bitmap_Size/8+1)/sizeof(ULONG) + 1)* sizeof(ULONG)),
		BITMAP_TAG
	);
	RtlInitializeBitMap(
		&pool.Bitmap, 
		(PULONG)(Bitmap_Buffer),
		(ULONG)(Bitmap_Size)
	);
	RtlClearAllBits(&pool.Bitmap);

	for (Index = 0; Index < 10; Index++)
		RtlSetBit(&pool.Bitmap, Index);
	if (RtlAreBitsSet(&pool.Bitmap, 0, 10) == TRUE)
		DbgPrint("bitmap: bit[0..9] is set\r\n");

	if (RtlCheckBit(&pool.Bitmap, 10))
		DbgPrint("bitmap: bit[10] is set\r\n");
	if (RtlCheckBit(&pool.Bitmap, 1024)) //Warning! Return 1 here
		DbgPrint("bitmap: bit[1024] is set\r\n");

	Index = 0;
	do
	{
		Index = RtlFindClearBitsAndSet (
			&pool.Bitmap,
			1, //NumberToFind
			Index //HintIndex
		);
		DbgPrint("%d\n", Index);
	}while (Index != -1);

	// Free
	ExFreePoolWithTag(pool.Bitmap.Buffer, BITMAP_TAG);
}

VOID DriverUnload(PDRIVER_OBJECT driver)
{
	DbgPrint("bitmap: Our driver is unloading\r\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	DbgPrint("bitmap: Hello, my salary!\r\n");
	driver->DriverUnload = DriverUnload;

	test_bitmap();

	return STATUS_SUCCESS;
}
