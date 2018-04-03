/** @file	px14_plat_kern_win32.h
	@brief	Platform specific user/kernel shared definitions for Windows
*/
#ifndef __px14_plat_kern_win32_header_defined
#define __px14_plat_kern_win32_header_defined

/// Driver version at which driver has 'need DCM reset' bit
//#define PX14DRV_HAVE_DRIVER_DCM_BIT				0x0001000100010000ull

// -- Device IO Control codes

/// Type of device (with respect to Windows). (In user-defined range.)
#define SIGDEV_TYPE					40000

#define PX14_IOCTL(n)	\
	CTL_CODE(SIGDEV_TYPE, (n), METHOD_BUFFERED, FILE_ANY_ACCESS)

// IN/OUT: PX14S_DEVICE_ID
#define IOCTL_PX14_GET_DEVICE_ID				PX14_IOCTL(2054)
// OUT: PX14S_DRIVER_VER
#define IOCTL_PX14_DRIVER_VERSION				PX14_IOCTL(2055)
// IN/OUT: PX14S_DMA_BUFFER_ALLOC
#define IOCTL_PX14_DMA_BUFFER_ALLOC				PX14_IOCTL(2056)
// IN: PX14S_DMA_BUFFER_FREE
#define IOCTL_PX14_DMA_BUFFER_FREE				PX14_IOCTL(2057)
// IN/OUT: PX14S_EEPROM_IO
#define IOCTL_PX14_EEPROM_IO					PX14_IOCTL(2058)
// IN: PX14S_DBG_REPORT
#define IOCTL_PX14_DBG_REPORT					PX14_IOCTL(2059)
// IN: PX14S_DEV_REG_WRITE
#define IOCTL_PX14_DEVICE_REG_WRITE				PX14_IOCTL(2060)
// IN/OUT: PX14S_DEV_REG_READ (OUT=4 bytes)
#define IOCTL_PX14_DEVICE_REG_READ				PX14_IOCTL(2061)
// No parameters
#define IOCTL_PX14_RESET_DCMS					PX14_IOCTL(2062)
// IN: PX14S_DMA_XFER
#define IOCTL_PX14_DMA_XFER						PX14_IOCTL(2063)
// No parameters
#define IOCTL_PX14_HWCFG_REFRESH				PX14_IOCTL(2064)
// IN: PX14S_RAW_REG_IO
#define IOCTL_PX14_RAW_REG_IO					PX14_IOCTL(2065)
// IN: PX14S_DRIVER_BUFFERED_XFER
#define IOCTL_PX14_DRIVER_BUFFERED_XFER			PX14_IOCTL(2066)
// IN/OUT: PX14S_JTAGIO
#define IOCTL_PX14_JTAG_IO						PX14_IOCTL(2067)
// IN: unsigned
#define IOCTL_PX14_US_DELAY						PX14_IOCTL(2068)
// IN/OUT: PX14S_JTAG_STREAM
#define IOCTL_PX14_JTAG_STREAM					PX14_IOCTL(2069)
// IN: int, OUT: PX14S_DRIVER_STATS
#define IOCTL_PX14_DRIVER_STATS					PX14_IOCTL(2070)
// IN: int (PX14MODE_*)
#define IOCTL_PX14_MODE_SET						PX14_IOCTL(2071)
// OUT: int (PX14STATE_*)
#define IOCTL_PX14_GET_DEVICE_STATE				PX14_IOCTL(2072)
// IN: PX14S_WAIT_OP
#define IOCTL_PX14_WAIT_ACQ_OR_XFER				PX14_IOCTL(2073)
// IN: unsigned
#define IOCTL_PX14_NEED_DCM_RESET				PX14_IOCTL(2074)
// IN/OUT: PX14S_READ_TIMESTAMPS
#define IOCTL_PX14_READ_TIMESTAMPS				PX14_IOCTL(2075)
// IN/OUT: PX14S_FW_VERSIONS
#define IOCTL_PX14_FW_VERSIONS					PX14_IOCTL(2076)

// -- Added in driver version 1.3.1

// IN/OUT: PX14S_HW_CONFIG_EX
#define IOCTL_PX14_GET_HW_CONFIG_EX				PX14_IOCTL(2077)

// -- Added in driver version 1.21.1

// IN/OUT: PX14S_BOOTBUF_CTRL
#define IOCTL_PX14_BOOTBUF_CTRL					PX14_IOCTL(2078)

#endif	// __px14_plat_kern_win32_header_defined

