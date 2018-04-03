/** @file	px14_plat_kern_linux.h
    @brief	Platform specific user/kernel shared definitions for Linux

	This file should only be included on Linux platform
*/
#ifndef __px14_plat_kern_linux_header_defined
#define __px14_plat_kern_linux_header_defined

#include <linux/ioctl.h>

#if !defined(__KERNEL__) && defined(_IOC_TYPECHECK)

// We need to redefine a few macros for user-mode code. Newer kernel versions
//  include an error-checking mechanism (_IOC_TYPECHECK) in the _IO* macros
//  that are used to catch drivers that use the macros incorrectly. This is
//  good. The problem is that the method that they used to do it prevents us
//  from using the macros in a switch statement. When the test fails, an
//  undefined variable is referenced which triggers a build error. This
//  reference makes the expanded value non-constant. This has since been
//  corrected by only doing the test for kernel mode builds. Ubuntu Hardy
//  does not have this fix.

#undef _IOR
#undef _IOW
#undef _IOWR

#define _IOR(type,nr,size)	_IOC(_IOC_READ,(type),(nr),(sizeof(size)))
#define _IOW(type,nr,size)	_IOC(_IOC_WRITE,(type),(nr),(sizeof(size)))
#define _IOWR(type,nr,size)	_IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(sizeof(size)))

#endif	// ifndef __KERNEL__

// -- Device IO Control codes

/** Magic number for ioctl commands. */
#define PX14IOC_MAGIC       0x14

#define PX14_IOCTL(n)       (n)

// IN/OUT: PX14S_DEVICE_ID
#define IOCTL_PX14_GET_DEVICE_ID    _IOR  (PX14IOC_MAGIC, 0,  PX14S_DEVICE_ID)
// OUT: PX14S_DRIVER_VER
#define IOCTL_PX14_DRIVER_VERSION   _IOR  (PX14IOC_MAGIC, 1,  PX14S_DRIVER_VER)
// IN/OUT: PX14S_DMA_BUFFER_ALLOC
#define IOCTL_PX14_DMA_BUFFER_ALLOC _IOWR (PX14IOC_MAGIC, 2,  PX14S_DMA_BUFFER_ALLOC)
// IN: PX14S_DMA_BUFFER_FREE
#define IOCTL_PX14_DMA_BUFFER_FREE  _IOWR (PX14IOC_MAGIC, 3,  PX14S_DMA_BUFFER_FREE)
// IN/OUT: PX14S_EEPROM_IO
#define IOCTL_PX14_EEPROM_IO        _IOWR (PX14IOC_MAGIC, 4,  PX14S_EEPROM_IO)
// IN: PX14S_DBG_REPORT
#define IOCTL_PX14_DBG_REPORT       _IOR  (PX14IOC_MAGIC, 5,  PX14S_DBG_REPORT)
// IN: PX14S_DEV_REG_WRITE
#define IOCTL_PX14_DEVICE_REG_WRITE _IOWR (PX14IOC_MAGIC, 6,  PX14S_DEV_REG_WRITE)
// IN/OUT: PX14S_DEV_REG_READ (OUT=4 bytes)
#define IOCTL_PX14_DEVICE_REG_READ  _IOR  (PX14IOC_MAGIC, 7,  PX14S_DEV_REG_READ)
// No parameters
#define IOCTL_PX14_RESET_DCMS       _IO   (PX14IOC_MAGIC, 8)
// IN: PX14S_DMA_XFER
#define IOCTL_PX14_DMA_XFER         _IOWR (PX14IOC_MAGIC, 9,  PX14S_DMA_XFER)
// No parameters
#define IOCTL_PX14_HWCFG_REFRESH    _IO   (PX14IOC_MAGIC, 10)
// IN: PX14S_RAW_REG_IO
#define IOCTL_PX14_RAW_REG_IO       _IOWR (PX14IOC_MAGIC, 11,  PX14S_RAW_REG_IO)
// IN: PX14S_DRIVER_BUFFERED_XFER
#define IOCTL_PX14_DRIVER_BUFFERED_XFER _IOWR (PX14IOC_MAGIC, 12,  PX14S_DRIVER_BUFFERED_XFER)
// IN/OUT: PX14S_JTAGIO
#define IOCTL_PX14_JTAG_IO          _IOWR (PX14IOC_MAGIC, 13,  PX14S_JTAGIO)
// IN: unsigned
#define IOCTL_PX14_US_DELAY         _IOWR (PX14IOC_MAGIC, 14,  unsigned int)
// IN/OUT: PX14S_JTAG_STREAM
#define IOCTL_PX14_JTAG_STREAM      _IOWR (PX14IOC_MAGIC, 15,  PX14S_JTAG_STREAM)
// IN: int, OUT: PX14S_DRIVER_STATS
#define IOCTL_PX14_DRIVER_STATS     _IOR  (PX14IOC_MAGIC, 16,  PX14S_DRIVER_STATS)
// IN: int (PX14MODE_*)
#define IOCTL_PX14_MODE_SET         _IOWR (PX14IOC_MAGIC, 17,  int)
// OUT: int (PX14STATE_*)
#define IOCTL_PX14_GET_DEVICE_STATE _IOR  (PX14IOC_MAGIC, 18,  int)
// IN: PX14S_WAIT_OP
#define IOCTL_PX14_WAIT_ACQ_OR_XFER _IOWR (PX14IOC_MAGIC, 19,  PX14S_WAIT_OP)
// IN: unsigned
#define IOCTL_PX14_NEED_DCM_RESET   _IOWR (PX14IOC_MAGIC, 20,  unsigned int)
// IN/OUT: PX14S_READ_TIMESTAMPS
#define IOCTL_PX14_READ_TIMESTAMPS  _IOR  (PX14IOC_MAGIC, 21,  PX14S_READ_TIMESTAMPS)
// IN/OUT: PX14S_FW_VERSIONS
#define IOCTL_PX14_FW_VERSIONS      _IOR  (PX14IOC_MAGIC, 22,  PX14S_FW_VERSIONS)
// IN/OUT: PX14S_HW_CONFIG_EX
#define IOCTL_PX14_GET_HW_CONFIG_EX _IOR  (PX14IOC_MAGIC, 23,  PX14S_HW_CONFIG_EX)
// IN/OUT: PX14S_BOOTBUF_CTRL
#define IOCTL_PX14_BOOTBUF_CTRL	   _IOR  (PX14IOC_MAGIC, 24,  PX14S_BOOTBUF_CTRL)
#define PX14_IOCMAX                                       25

#endif	// __px14_plat_kern_linux_header_defined

