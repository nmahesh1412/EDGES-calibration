/** @file   px14_drv.h
  @brief  Defines constants and types for PX14400 driver (sig_px14400) module

  This driver will work with PX14400(A), PX14400D, and PX12500 devices

  @author Mike DeKoker
  */
#ifndef px14_drv_by_Mike_DeKoker
#define px14_drv_by_Mike_DeKoker

/// This driver's version
#define MY_DRIVER_VER64 PX14_VER64(2,20,18,15)

/// Enabled: Verbose (lots of output) driver
//#define PX14_VERBOSE
/// Enabled: Verbose DMA buffer allocate/free events
//#define PX14PP_VERBOSE_DMA_BUFFER_EVENTS
/// Enabled: Enables various compile-time assertions
#define PX14PP_ALLOW_CT_ASSERTS

/// Enabled: tty_out not compiled
#define NO_TTY_OUT
/// Enabled: No /proc fs support
//#define NO_PROC_ENTRY

// If defined then the driver will not perform real IO on the PX14400
//#define PX14PP_NO_HARDWARE_ACCESS
// If defined then the driver will not touch configuration EEPROM
//#define PX14PP_NO_EEPROM_ACCESS

// Code does not use the allocator module for DMA buffer allocation
#define NO_RUBINI_ALLOCATOR

#include <asm/atomic.h>

#ifndef LINUX_VERSION_CODE
#  include <linux/version.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#error PX14400 is not supported on 2.4 kernels
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#  define PX14_KERNEL_2_6
#endif

#ifdef PRE25_KERN
#  ifndef _MAIN_FILE
#    if defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
#      include <linux/modversions.h>
#      define MODVERSIONS
#    endif
#    define __NO_VERSION__
#    include <linux/version.h>
#  endif
#  if defined(CONFIG_MODVERSIONS) && !defined(NO_RUBINI_ALLOCATOR)
#    include "../allocator/allocator.ver"
#  endif
# define iowrite32      writel
# define ioread32      readl
#else
#  ifndef NO_RUBINI_ALLOCATOR
#    include "../allocator/allocator.h"
#  endif
#  include <linux/dma-mapping.h>
#endif

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/pagemap.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/mm.h>

#ifndef NO_KERN_ASM_GENERIC_IOMAP
# include <asm-generic/iomap.h>
#endif
#include <asm/uaccess.h>
#include <asm/current.h>
#include <asm/signal.h>
#include <asm/types.h>
#include <asm/page.h>
#include <asm/io.h>

#define PX14PP_WANT_PX14S_JTAG_STREAM
#define PX14PP_NO_CLASS_DEFS
#include "../libsig_px14400/px14_private.h"
#include "../libsig_px14400/px14_plat_kern_linux.h"

// -- Kernel completion API not available until 2.6.11 but we need it for
//    DMA/acquisition complete notifications so we're using relevant code
//    from 2.6.11 implementation for older kernels. (Looking at you 2.6.9)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
#define PX14PP_NEED_COMPLETION_IMP
#include "px14_comp.h"

#define completion my_completion
#define wait_for_completion_interruptible \
   my_wait_for_completion_interruptible
#define wait_for_completion_timeout \
   my_wait_for_completion_timeout
#define wait_for_completion_interruptible_timeout \
   my_wait_for_completion_interruptible_timeout
#define complete_all    my_complete_all
#define init_completion my_init_completion
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
#define PX14PP_NO_REMAP_PFN_RANGE
#endif

#define PX14_TRUE    1
#define PX14_FALSE   0

/// Base PX14400 device name
#define PX14_DEVICE_NAME   "sig_px14400"
/// Largest character count for a single tty_out output
#define MAX_TTY_OUT        512

/// Default major number is 0; requests dynamic major number allocation
#define PX14_MAJOR_NUM_DEF   0

// Module output (printk) constants.
//#define PX14_INFO      KERN_INFO    PX14_DEVICE_NAME " : "
#define PX14_INFO          KERN_ALERT   PX14_DEVICE_NAME " : "
#define PX14_ERROR         KERN_ALERT   PX14_DEVICE_NAME " : "
#define PX14_BUG           KERN_ALERT   PX14_DEVICE_NAME " : *BUG* "
#define PX14_KERN_WARNING  KERN_WARNING PX14_DEVICE_NAME " : WARN: "

/// PX14400 device state structure (struct px14_device) magic number
#define PX14_DEVICE_MAGIC   0x14000400

/// Size of DMA frame in PX14 samples
#define PX14_DMA_XFER_FRAME_SIZE_IN_SAMPLES \
   (PX14_DMA_XFER_FRAME_SIZE_IN_BYTES / PX14_SAMPLE_SIZE_IN_BYTES)

/// Size of driver's internal utility DMA buffer in bytes
//#define INT_DMA_BUF_BYTES   65536
/// Size of driver's internal utility DMA buffer in samples
//#define INT_DMA_BUF_SAMPLES
//  (INT_DMA_BUF_BYTES / PX14_SAMPLE_SIZE_IN_BYTES)

// - Code assumes PX14_DB_XFER_* are powers of two
/// Preferred DMA buffer size (in samples) for driver-buffered transfers
#define PX14_DB_XFER_MAX   (262144 / PX14_SAMPLE_SIZE_IN_BYTES)
/// Minimum DMA buffer size (in samples) for driver buffered transfers
#define PX14_DB_XFER_MIN   (65536  / PX14_SAMPLE_SIZE_IN_BYTES)


/* -- PX14400 device implementation flags (DEVF_*) */
/// Set if PCI device enabled
#define DEVF_ENABLED          0x00000001
/// Set if device started
#define DEVF_STARTED          0x00000002
/// Set if /proc entry created
#define DEVF_PFS_ENTRY        0x00000004
/// Set if DMA umapping needed
#define DEVF_DO_DMA_UNMAP     0x00000008
/// Set if we need to reset DCMs prior to next acquisition
#define DEVF_NEED_DCM_RESET   0x00000010
/// Initial device implementation flags value
#define DEVFINIT              0

// -- DMA buffer descriptor flags (PX14DBDF_*)
/// Buffer used for driver-buffered transfers; (Not a user-allocated buffer)
#define PX14DBDF_DBT_BUFFER      0x00000001
/// Pages for this buffer have been marked as reserved
#define PX14DBDF_PAGES_RESERVED  0x00000002
#define PX14DBDF__INIT           0

/// Length of buffer to hold a device's name. (i.e. "sig_px144000")
#define MAX_PX14_DEVNAME   16
#define MAX_PX14_PFS_NAME  24

/** @brief DMA buffer descriptor.

  Holds information for a single allocated DMA buffer. Each time a DMA
  buffer is allocated or requested from the special reserved RAM, one of
  these structures is allocated to track the buffer and its associated
  state.
  */
struct dma_buf_desc
{
   struct list_head  list;           /// Kernel linked-list link
   struct mm_struct* pAddrSpace;     /// Owner's address space
   struct file*      filp_owner;     /// File handle that did alloc
   u_short           flags;          /// PX14DBDF_*
   u_int             buffer_bytes;   /// Buffer size in bytes
   u_long            userAddr;       /// Mapped user-space virtual address
   u_long            physAddr;       /// Bus address for DMA operations
   void*             pBufKern;       /// Kernel-virtual address
};

typedef struct dma_buf_desc PX14S_DMA_BUF_DESC;

/* -- Contants that define which DMA buffer(s) to free */
/// By current user-space address
#define PX14FDB_USERSPACE_ADDR   0
/// Free all DMA buffers.
#define PX14FDB_ALL              1
/// By given file handle
#define PX14FDB_FILE             2
/// Free all user-allocated DMA buffers
#define PX14FDB_ALL_USER         3
/// Free buffer by kernel-virtual address
#define PX14FDB_KERNVIRT_ADDR    4
#define PX14FDBMAX               5   // Not a valid PX14FDB_* value

struct px14_free_dma_buffer
{
   union {
      u_int        buffer_bytes; /// Out: Length freed
      u_long       user_addr;    /// In : User-space address
      void*        addr_space;   /// In : Address space
      void*        kern_virt;    /// In : Kernel-virtual addr
      struct file* filp;         /// In : File (device) handle
   };
   int how;                      /// P14FDB_*
};

typedef struct px14_free_dma_buffer PX14S_FDB_CTX;

/// Holds state information for a single PX14 device
typedef struct _px14_device_tag
{
   u_int                      magic;         /// Structure's magic number
   spinlock_t                 devLock;       /// Device spin lock
   struct semaphore           devMutex;      /// Device mutex
   struct pci_dev*            pOsDevice;     /// Pointer to system pci info
   struct device*             devicep;
   u_int                      dwFlags;       /// Implementation flags (DEVF_*)
   atomic_t                   refCount;      /// Device's reference count
   u_int                      serialNum;     /// Serial number of device
   int                        brdNum;        /// 0-based index of device in sys
   unsigned short             features;      /// Hw feature flags (PX14FEATURE_*)
   unsigned short             chanImps;      /// PX14CHANIMPF_*; ch2|ch1
   unsigned short             chanFilters;   /// PX14CHANFILTER_*; ch2|ch1
   unsigned short             fpgaTypeSys;   /// System FPGA type (PX14SYSFPGA_*)
   unsigned short             fpgaTypeSab;   /// SAB FPGA type (PX14SABFPGA_*)
   unsigned short             boardRev;      /// Brd revision (PX14BRDREV_*)
   unsigned short             boardRevSub;   /// Brd sub-rev (PX14BRDREVSUB_*)
   unsigned short             custFwPkg;     ///< Custom package firmware enumeration
   unsigned short             custFwPci;     ///< Custom PCI firmware enumeration
   unsigned short             custFwSab;     ///< Custom SAB firmware enumeration
   unsigned short             custHw;        ///< Custom hardware enumeration
   u_int                      fwVerPci;      /// PCI firmware version (private)
   u_int                      fwVerSab;      /// SAB firmware version (private)
   u_int                      fwVerPkg;      /// Firmware package version (public)
   u_int                      fwPreRelFlags; /// PX14PRERELEASE_*
   u_int                      ram_samples;   /// Total PX14400 RAM samples

   int                        irqNum;        /// Device IRQ number

   u_long                     MemBase  [PX14_MEMS_CNT];   /// Mem IO - base
   u_int                      MemCount [PX14_MEMS_CNT];   /// Mem IO - len
   void*                      MemMapped[PX14_MEMS_CNT];   /// kernel mapped

   struct file*               jtag_filp;     ///< File that "owns" JTAG access

   // -- DMA buffer stuff
   struct list_head           dma_buf_list;  /// DMA buffer description list

   // -- DMA transfer stuff
   struct file*               dma_filp;      /// File that "owns" current DMA
   dma_addr_t                 dmaAddr;       /// Bus addr of start of last DMA xfer
   u_int                      dmaBytes;      /// Length of last/cur DMA xfer in bytes
   int                        dmaDir;        /// Dir. of last DMA xfer (PCI_DMA_*)

   // -- Driver-buffered DMA transfer stuff
   PX14S_DMA_BUF_DESC*        db_dma_descp;  /// DMA buffer for driver buffered xfers
   PX14_SAMPLE_TYPE*          btIntBufCh1;   ///< Interleave buffer (ch1)
   PX14_SAMPLE_TYPE*          btIntBufCh2;   ///< Interleave buffer (ch2)

   // -- Current opertation (acq or DMA) stuff
   int                        DeviceState;      /// PX14STATE_*
   int                        bOpCancelled;     /// Last acq/xfer cancelled?
   int                        bSampComp;        /// Samples complete intr recv'd?
   struct completion          comp_acq_or_xfer; /// Completion event for acq or DMA

   // -- Driver stats
   atomic_t                   stat_isr_count;  /// ISR counter
   atomic_t                   stat_dma_start;  /// DMA transfers started
   atomic_t                   stat_dma_comp;   /// DMA transfers completed
   atomic_t                   stat_acq_start;  /// Acquisitions started
   atomic_t                   stat_acq_comp;   /// Acquisitions completed
   unsigned long              stat_dma_bytes;  /// Total bytes transferred via DMA
   atomic_t                   stat_jtag_ops;
   atomic_t                   stat_dcm_resets; /// Total DCM reset operations

   // Software registers (cached hardware register content)
   PX14U_DEVICE_REGISTER_SET  regDev;
   PX14U_DRIVER_REGISTER_SET  regDriver;   ///< Main device registers
   PX14U_CLKGEN_REGISTER_SET  regClkGen;   ///< Clock generator registers
   PX14U_CONFIG_REGISTER_SET  regCfg;      ///< JTAG/EEPROM registers

   /* Must be long enough to hold device name (ie: sig_pda160) */
   char devName[MAX_PX14_DEVNAME];     /// Device name
#ifndef NO_PROC_ENTRY
   char pfsName[MAX_PX14_PFS_NAME];    /// Proc FS name
#endif

} px14_device;

/// Holds state information for a sig_px14400 module
struct px14400_module_state
{
   struct class*  classp;     /// Manages /dev file generation
   struct cdev    cdev;       /// Char device state
   px14_device*   pDev;       /// PX14400 device state objects
   dev_t          majorID;
   int            nDevices;   /// Number of PX14400 devices present
   volatile u_int bh_bmp;     /// Bitmap of devices to run ISR bh
};

///////////////////
// Helper macros

////
//
// Hardware access macros
//

#ifdef PX14PP_NO_HARDWARE_ACCESS
//
// Macros **DO NOT** access hardware
//

/// Macro used to write a device register with register cache data
#define WriteDevReg(pDev, n)        (void)(0)
/// Macro used to write a device register with specifc data. CACHE NOT UPDATED
#define WriteDevRegVal(pDev,n,v)    (void)(0)
/// Macro used to read a device register from hardware
#define ReadDevReg(pDev, n)         (pDev->regDev.values[n])

/// Macro used to write a device register with register cache data
#define WriteCfgReg(pDev, n)        (void)(0)
/// Macro used to read a device register from hardware
#define ReadCfgReg(pDev, n)         ((pDev)->regCfg.values[n])

/// Macro used to write a DMA device register
#define WriteDmaReg(pDev, n, v)     (void)(0)
/// Macro used to read a DMA device register
#define ReadDmaReg(pDev, n)         0

#else
//
// Macros **DO** access hardware
//

/// Macro used to write a device register with register cache data
#define WriteCfgReg(pDev, n) \
   iowrite32((pDev)->regCfg.values[n], \
             ((u32*)(pDev)->MemMapped[PX14_BARIDX_CONFIG])+(n))

/// Macro used to read a device register from hardware
#define ReadCfgReg(pDev, n) \
   ioread32(((u32*)(pDev)->MemMapped[PX14_BARIDX_CONFIG])+(n))

/// Macro used to write a device register with register cache data
#define WriteDevReg(pDev, n) \
   iowrite32((pDev)->regDev.values[n], \
             ((u32*)(pDev)->MemMapped[PX14_BARIDX_DEVICE])+(n))

/// Macro used to write a device register with specifc data. CACHE NOT UPDATED
#define WriteDevRegVal(pDev,n,v) \
   iowrite32(v, ((u32*)(pDev)->MemMapped[PX14_BARIDX_DEVICE])+(n))

/// Macro used to read a device register from hardware
#define ReadDevReg(pDev, n) \
   ioread32(((u32*)(pDev)->MemMapped[PX14_BARIDX_DEVICE])+(n))

/// Macro used to write a DMA device register
#define WriteDmaReg(pDev, n, v) \
   iowrite32(v, ((u32*)(pDev)->MemMapped[PX14_BARIDX_DMA])+(n))

/// Macro used to read a DMA device register
#define ReadDmaReg(pDev, n) \
   ioread32(((u32*)(pDev)->MemMapped[PX14_BARIDX_DMA])+(n))

#endif

/// Macro used to write EEPROM control register
#define WriteHwRegCfgEeprom(pDev)   WriteCfgReg(pDev, 0)
/// Macro used to read EEPROM control register
#define ReadHwRegCfgEeprom(pDev)    ReadCfgReg(pDev, 0)

/// Macro used to write JTAG register
#define WriteHwRegJtagStatus(pDev)  WriteCfgReg(pDev, 1)
/// Macro used to read JTAG register
#define ReadHwRegJtagStatus(pDev)   ReadCfgReg(pDev, 1)

/// Macro used to write FPGA version register
#define WriteHwRegFpgaVersion(pDev) WriteCfgReg(pDev, 4)
/// Macro used to read FPGA version register
#define ReadHwRegFpgaVersion(pDev)  ReadCfgReg(pDev, 4)

/// Returns nonzero if given device register index is a serial (SAB) reg
#define PX14_IS_SERIAL_DEVICE_REG(r) \
   (((r) >= 8) && ((r) <= 11))

/// Force flush of possible pending writes by doing a read
#define PX14_FLUSH_BUS_WRITES(devp) ReadDevReg(devp, 13)

/// Returns nonzero if given mode is Standby mode
#define PX14_IS_STANDBY(m)          ((m) == PX14MODE_STANDBY)

/// Macro to return current operating mode; lock ignored
#define PX14_OP_MODE(p)             ((p)->regDev.fields.regB.bits.OM)

/// Macro that evaluates to minimum of given values
#define PX14_MIN(a,b)               ((a)<(b)?(a):(b))

/// Acquire device lock and disable interrupts
#define PX14_LOCK_DEVICE(d,f)       spin_lock_irqsave(&(d)->devLock,f);{
   /// Release device lock and restore interrupt state
#define PX14_UNLOCK_DEVICE(d,f) \
}spin_unlock_irqrestore(&(d)->devLock,f)
/// Release device lock and restore interrupt state from within a lock grou
#define PX14_UNLOCK_DEVICE_BREAK(d,f) \
   spin_unlock_irqrestore(&(d)->devLock,f)

/// Lock device mutex
#define PX14_LOCK_MUTEX(d) \
   do {if (0!=down_interruptible(&(d)->devMutex)) return -EINTR;} while(0);{
      /// Unlock device mutex
#define PX14_UNLOCK_MUTEX(d)        } up(&(d)->devMutex)
/// Unlock device mutex from within a lock group
#define PX14_UNLOCK_MUTEX_BREAK(d)  up(&(d)->devMutex)

/// Marks a function parameter as unused
#define UNUSED_PARAM(p)          ((void)(p))
/// Module assert macro
#define PX14_ASSERT(c)           do{if(!(c))BUG();}while(0)
/// Does a kfree on the given pointer and then invalidates it
#define SAFE_KFREE(p)            do{kfree(p); p=NULL;}while(0)

#ifdef PX14PP_ALLOW_CT_ASSERTS
/// Compile time assertion macro
# define PX14_CT_ASSERT(e)       do{int _[(e)?1:-1];_[0]=0;}while(0)
#else
# define PX14_CT_ASSERT(e)
#endif

#ifdef PX14_VERBOSE
/// Verbose module logging
# define VERBOSE_LOG(fmt,args...)      printk(PX14_INFO fmt, ## args)
/// Verbose module error logging
# define VERBOSE_LOG_ERR(fmt,args...)  printk(PX14_ERROR fmt, ## args)
#else
/// Place holder for verbose module logging
# define VERBOSE_LOG(fmt,args...)      ((void)(0))
/// Place holder for verbose module error logging
# define VERBOSE_LOG_ERR(fmt,args...)  ((void)(0))
#endif

/// Module debug tracing
#define PX14_TRACE(fmt, args...)    printk(PX14_INFO fmt, ## args)

/// Normal, driver diagnostic/info dump to system ring buffer
#define PX14_DMESG(fmt,args...)     printk(PX14_INFO fmt, ## args)

/// Mike's debugging trace call
#define MIKE_TRACE(fmt,args...)     printk(PX14_INFO fmt, ## args)

////////////////////////
// Function prototypes

// PX14400 interrupt handler
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
extern irqreturn_t px14_isr (int irq, void *dev_id, struct pt_regs *regsp);
#else
extern irqreturn_t px14_isr (int irq, void *dev_id);
#endif

// -- Driver module file operation functions

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
extern long px14_ioctl_unlocked (struct file *filp,
                                 u_int cmd, u_long arg);
#else
extern int px14_ioctl (struct inode *pNode, struct file *filp,
                       u_int cmd, u_long arg);
#endif
extern ssize_t px14_write (struct file *pFile, const char *pData,
                           size_t len, loff_t *pOffset);
extern ssize_t px14_read (struct file *pFile, char *pData, size_t len,
                          loff_t *pOffset);
extern int px14_mmap (struct file *filp, struct vm_area_struct *vmap);
extern int px14_release (struct inode *pNode, struct file *filp);
extern int px14_open (struct inode *pNode, struct file *pFile);

// This routine is called by the OS when a user tries to access a page
//  of a mapped DMA buffer for the first time. The job of the function
//  is to return the correct page structure to use.
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
extern struct page* px14_vm_nopage (struct vm_area_struct *vmap,
                                    u_long addr, int *typep);
#else
extern int px14_vm_fault (struct vm_area_struct *vma, struct vm_fault *vmf);
#endif

// -- ioctl request implementors
extern int px14ioc_jtag_io (px14_device* devp, u_long arg, struct file* filp);
extern int px14ioc_jtag_stream(px14_device* devp,u_long arg, struct file*filp);
extern int px14ioc_driver_buffered_xfer (struct file* filp, px14_device* devp,
                                         u_long arg);
extern int px14ioc_device_reg_write (px14_device* devp, u_long arg);
extern int px14ioc_device_reg_read (px14_device* devp, u_long arg);
extern int px14ioc_dma_buffer_alloc (struct file* filp, px14_device* devp,
                                     u_long arg);
extern int px14ioc_dma_buffer_free (px14_device* devp, u_long arg);
extern int px14ioc_dma_xfer (struct file* filp, px14_device* devp, u_long arg);
extern int px14ioc_read_timestamps (px14_device* devp, u_long arg);
extern int px14ioc_eeprom_io (px14_device* devp, u_long arg);
extern int px14ioc_mode_set (px14_device* devp, u_long arg);

// -- Register access functions

extern int WriteClockGenReg_PX14 (px14_device* devp, PX14S_DEV_REG_WRITE* ctxp,
                                  int bUpdateRegs);
extern u_int ReadClockGenReg_PX14 (px14_device* devp, PX14S_DEV_REG_READ* ctxp);

extern int WriteDeviceReg_PX14 (px14_device* devp, u_int reg_idx, u_int val,
                                u_int mask);
extern u_int ReadDeviceReg_PX14 (px14_device* devp, u_int reg_idx,
                                 u_int read_how);

extern void InitializeHardwareSettingsPX14 (px14_device* devp);

// Change PX14400 operating mode
extern void SetOperatingMode_PX14 (px14_device* devp, int mode);
// Get current PX14400 operating mode from driver's register cache
extern int GetOperatingModeSwr_PX14 (px14_device* devp);

// Setup the PX14400's active memory region; defines acq/xfer length
extern void SetActiveMemoryRegion_PX14 (px14_device* devp, u_int start_sample,
                                        u_int sample_count);

/// Cache info from config EEPROM (serial number, firmware versions, etc)
extern int DoHwCfgRefreshWork_PX14 (px14_device* devp, int bDefaultsOnly);

/// Reset firmware DCMs
extern int DoDcmResetWork (px14_device* devp);

extern void SetTriggerSelection_PX14 (px14_device* devp, int selection);

extern void SetTriggerHysteresis_PX14 (px14_device* devp, int hyst);

// -- DMA transfer/buffer routines

// Allocate a DMA buffer implementation
PX14S_DMA_BUF_DESC* AllocateDmaBufferImp_PX14(px14_device* devp,
                                              PX14S_DMA_BUFFER_ALLOC* ctxp);

/// Allocate driver's internal DMA buffer
int AllocateDriverDmaBuffer_PX14 (px14_device* devp);
/// Free driver's internal DMA buffer
int FreeDriverDmaBuffer_PX14 (px14_device* devp);

// Workhorse routine for freeing DMA buffer(s). If pnRemoved is non-NULL, it
//  will point to the number of DMA buffers freed.
// NOTE: Device semaphore should be acquired outside of this function
int px14_free_dma_buffer_work (px14_device* devp,
                               struct px14_free_dma_buffer* fdbp,
                               unsigned* lenp,
                               struct list_head* lstToFree);

void FreeDmaBufferList_PX14 (px14_device* devp, struct list_head* lstToFree);

extern void BeginDmaTransfer_PX14 (px14_device* devp,
                                   dma_addr_t pa,
                                   u_int sample_count,
                                   int bXferToDevice);


// Reset DMA logic; this will cancel any pending DMA transfers in hardware
extern void ResetDma_PX14 (px14_device* devp);


// -- Miscellaneous routines

// Stall or sleep for a bit
void DoDriverStall_PX14 (u_int microsecs, int bNeverSleep);

/// Returns 0 if configuration EEPROM access is okay
int CheckEepromIoPX14 (px14_device* devp);

// Read serial number from configuration EEPROM
u_int ReadSerialNum_PX14 (px14_device* devp);
// Read a value from the configuration EEPROM
u_int ReadEepromVal_PX14 (px14_device* devp, u_int eeprom_addr);
// Read 32-bit value from adjacent EEPROM addresses
u_int ReadEepromVal32_PX14 (px14_device* devp, u_int addrLow, u_int addrHigh);

// Wait for current DMA transfer to complete
int WaitForDmaTransferToComplete_PX14 (px14_device* devp);

// Handles cancellation of acq/xfer waiters
int DoPendingOpAbortWork_PX14 (px14_device* devp, int bTimedOut);

// Checks state of PCI FIFO; returns 0 or -SIG_PX14_FIFO_OVERFLOW
int CheckPciFifo_PX14(px14_device* devp);

#ifndef NO_TTY_OUT
// Does a printf-styled output to the current terminal. This should work for
//  any terminal; X11, telenet or plain-vanilla. Total length of output
//  string is limited to MAX_TTY_OUT characters. If MAX_TTY_OUT is extended,
//  keep in mind that the buffer lives on the stack.
extern void tty_out (const char *format, ...);
#endif

/////////////////////
// Global variables

extern struct px14400_module_state* g_pModPX14;

// Returns bit index of set (lsb) bit
static inline u_int bit_nr_of(u_int f)
{
   u_int nr;

   PX14_ASSERT(f);
   for (nr=0; !(f&1); f>>=1,nr++);

   return nr;
}


#endif // px14_drv_by_Mike_DeKoker

