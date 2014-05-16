// bus guid
// {473DB41C-CC0E-4ce7-89FE-1E980922806C}
DEFINE_GUID(GUID_TIAMO_BUS, 
			0x473db41c, 0xcc0e, 0x4ce7, 0x89, 0xfe, 0x1e, 0x98, 0x9, 0x22, 0x80, 0x6c);

// ioctol for bus
#define IOCTL_TIAMO_BUS_PLUGIN CTL_CODE(FILE_DEVICE_BUS_EXTENDER,0,METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_TIAMO_BUS_MINIPORT_GET_CONFIG CTL_CODE(FILE_DEVICE_BUS_EXTENDER,1,METHOD_BUFFERED, FILE_ANY_ACCESS)

// miniport configuration
typedef struct __tagMiniportConfig
{
	ULONG						m_ulDevices;
	WCHAR						m_szImageFileName[4][256];
}MiniportConfig,*PMiniportConfig;

// bus fdo name
#define BUS_FDO_NAME L"\\Device\\tiamobus"