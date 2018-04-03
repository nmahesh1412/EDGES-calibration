/** @file px14_ioctl.c
  @brief Implements PX14400 driver ioctl interface
  */
#include "px14_drv.h"

//////////////////////////////////////
// Static helper function prototypes.

static int px14ioc_get_hw_config_ex (px14_device *devp, u_long arg);
static int px14ioc_get_device_state (px14_device *devp, u_long arg);
static int px14ioc_wait_acq_or_xfer (px14_device *devp, u_long arg);
static int px14ioc_driver_version (px14_device *devp, u_long arg);
static int px14ioc_microsec_delay (px14_device *devp, u_long arg);
static int px14ioc_need_dcm_reset (px14_device *devp, u_long arg);
static int px14ioc_get_device_id (px14_device *devp, u_long arg);
static int px14ioc_driver_stats (px14_device *devp, u_long arg);
static int px14ioc_fw_versions (px14_device *devp, u_long arg);
static int px14ioc_raw_reg_io (px14_device *devp, u_long arg);
static int px14ioc_hwcfg_refresh (px14_device *devp);
static int px14ioc_reset_dcms (px14_device *devp);


///////////////////////////////////////////////////////////////////////////
// File implementation

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
long px14_ioctl_unlocked (           struct file *filp, u_int cmd, u_long arg)
#else
int px14_ioctl (struct inode *pNode, struct file *filp, u_int cmd, u_long arg)
#endif
{
   px14_device *devp;
   int res;

   // Determine which device this is for
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
   devp = (px14_device*)filp->private_data;

#else
   devp = &g_pModPX14->pDev[MINOR(pNode->i_rdev)];
   UNUSED_PARAM (filp);
#endif

   // Check ioctl command fields.
   if ((_IOC_TYPE(cmd) != PX14IOC_MAGIC) || (_IOC_NR(cmd) >= PX14_IOCMAX))
      return -ENOTTY;

   // Check input/output parameters if necessary.
   if (_IOC_DIR(cmd)) {
      int err = 0;

      if (_IOC_DIR(cmd) & _IOC_READ)
         err = !access_ok (VERIFY_WRITE, (void*)arg, _IOC_SIZE(cmd));
      else if (_IOC_DIR(cmd) & _IOC_WRITE)
         err = !access_ok (VERIFY_READ, (void*)arg, _IOC_SIZE(cmd));

      if (err)
         return -EFAULT;
   }

   switch (cmd) {

      //
      // DMA transfers
      //
      case IOCTL_PX14_DMA_XFER:
         res = px14ioc_dma_xfer(filp, devp, arg); break;
      case IOCTL_PX14_DRIVER_BUFFERED_XFER:
         res = px14ioc_driver_buffered_xfer(filp, devp, arg); break;

         //
         // Device register IO
         //
      case IOCTL_PX14_DEVICE_REG_WRITE:
         res = px14ioc_device_reg_write(devp, arg); break;
      case IOCTL_PX14_DEVICE_REG_READ:
         res = px14ioc_device_reg_read(devp, arg); break;
      case IOCTL_PX14_RAW_REG_IO:
         res = px14ioc_raw_reg_io(devp, arg); break;
      case IOCTL_PX14_MODE_SET:
         res = px14ioc_mode_set(devp, arg); break;
      case IOCTL_PX14_READ_TIMESTAMPS:
         res = px14ioc_read_timestamps (devp, arg); break;

         //
         // DMA buffer requests
         //
      case IOCTL_PX14_DMA_BUFFER_ALLOC:
         res = px14ioc_dma_buffer_alloc (filp, devp, arg); break;
      case IOCTL_PX14_DMA_BUFFER_FREE:
         res = px14ioc_dma_buffer_free (devp, arg); break;
      case IOCTL_PX14_BOOTBUF_CTRL:
         // used by session recording; TBC !!!
         VERBOSE_LOG_ERR ("\n%s> IOCTL_PX14_BOOTBUF_CTRL called!\n", __func__);
         res = -ENOTTY;
         break;

         //
         // Misc requests
         //
      case IOCTL_PX14_WAIT_ACQ_OR_XFER:
         res = px14ioc_wait_acq_or_xfer(devp, arg); break;
      case IOCTL_PX14_GET_DEVICE_STATE:
         res = px14ioc_get_device_state(devp, arg); break;
      case IOCTL_PX14_RESET_DCMS:
         res = px14ioc_reset_dcms(devp); break;
      case IOCTL_PX14_NEED_DCM_RESET:
         res = px14ioc_need_dcm_reset(devp, arg); break;
      case IOCTL_PX14_GET_DEVICE_ID:
         res = px14ioc_get_device_id(devp,arg); break;
      case IOCTL_PX14_GET_HW_CONFIG_EX:
         res = px14ioc_get_hw_config_ex(devp, arg); break;
      case IOCTL_PX14_HWCFG_REFRESH:
         res = px14ioc_hwcfg_refresh(devp); break;
      case IOCTL_PX14_JTAG_IO:
         res = px14ioc_jtag_io(devp, arg, filp); break;
      case IOCTL_PX14_JTAG_STREAM:
         res = px14ioc_jtag_stream(devp, arg, filp); break;
      case IOCTL_PX14_DRIVER_STATS:
         res = px14ioc_driver_stats (devp, arg); break;
      case IOCTL_PX14_EEPROM_IO:
         res = px14ioc_eeprom_io (devp, arg); break;
      case IOCTL_PX14_US_DELAY:
         res = px14ioc_microsec_delay (devp, arg); break;
      case IOCTL_PX14_FW_VERSIONS:
         res = px14ioc_fw_versions(devp, arg); break;
      case IOCTL_PX14_DRIVER_VERSION:
         res = px14ioc_driver_version (devp, arg); break;

      case IOCTL_PX14_DBG_REPORT:
         // No debug reports currently implemented for PX14400
         res = -ENOTTY;
         break;

      default:
         VERBOSE_LOG("Unknown ioctl function: 0x%04X\n", (u_int)cmd);
         res = -ENOTTY;
   }

   return res;
}

int px14ioc_get_device_id (px14_device *devp, u_long arg)
{
   PX14S_DEVICE_ID cfg;
   unsigned long f;

   PX14_CT_ASSERT(24 == sizeof(PX14S_DEVICE_ID));

   // Fill in the PDA14 configuration data.
   PX14_LOCK_DEVICE(devp, f)
   {
      cfg.struct_size   = sizeof(PX14S_DEVICE_ID);
      cfg.serial_num    = devp->serialNum;
      cfg.ord_num       = devp->brdNum;
      cfg.feature_flags = devp->features;
      cfg.chan_imps     = devp->chanImps;
   }
   PX14_UNLOCK_DEVICE(devp,f);

   if (__copy_to_user ((void*)arg, &cfg, sizeof(PX14S_DEVICE_ID)))
      return -EFAULT;

   return 0;
}

int px14ioc_microsec_delay (px14_device *devp, u_long arg)
{
   unsigned microseconds;
   int res;

   // Input is an unsigned int (microsecond count)
   res = __copy_from_user(&microseconds, (void*)arg, sizeof(unsigned));
   if (res != 0)
      return -EFAULT;

   if (microseconds > 1000) {
      msleep (microseconds / 1000);
      microseconds %= 1000;
   }

   if (microseconds)
      udelay(microseconds);

   return 0;
}

int px14ioc_wait_acq_or_xfer (px14_device *devp, u_long arg)
{
   int res, bWaitOk, bCheckFifo;
   PX14S_WAIT_OP ctx;
   long ms_left;
   unsigned long f;

   // Input is a PX14S_WAIT_OP structure
   res = __copy_from_user(&ctx, (void*)arg, sizeof(PX14S_WAIT_OP));
   if (res != 0)
      return -EFAULT;

   res = bCheckFifo = bWaitOk = 0;

   PX14_LOCK_DEVICE(devp,f)
   {
      switch (ctx.wait_op) {

         case PX14STATE_DMA_XFER_FAST:
         case PX14STATE_DMA_XFER_BUFFERED:
            // Are we currently in a DMA transfer?
            if ((devp->DeviceState == PX14STATE_DMA_XFER_FAST) ||
                (devp->DeviceState == PX14STATE_DMA_XFER_BUFFERED)) {
               bWaitOk = PX14_TRUE;
            }
            else if (devp->DeviceState != PX14STATE_IDLE)
               res = -SIG_PX14_BUSY;

            bCheckFifo = (PX14MODE_ACQ_PCI_BUF == PX14_OP_MODE(devp));
            break;

         case PX14STATE_ACQ:
            // Are we currently in an acquisition?
            if (devp->DeviceState == PX14STATE_ACQ)
               bWaitOk = PX14_TRUE;
            else if (devp->DeviceState != PX14STATE_IDLE)
               res = -SIG_PX14_BUSY;
            break;

         case PX14STATE_WRAM:
            if (devp->DeviceState != PX14STATE_IDLE)
               res = -SIG_PX14_BUSY;
            else if (PX14_OP_MODE(devp) != PX14MODE_RAM_WRITE_PCI)
               res = -SIG_INVALID_MODE;
            else {
               // Don't wait if we've already received interrupt
               if (!devp->bSampComp) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,12,0)
                  reinit_completion (&devp->comp_acq_or_xfer);
#else
                  init_completion (&devp->comp_acq_or_xfer);
#endif
                  devp->bOpCancelled = 0;

                  devp->DeviceState = PX14STATE_WRAM;
                  bWaitOk = PX14_TRUE;
               }
            }
            break;

         default:
            res = -EINVAL;
            break;
      }
   }
   PX14_UNLOCK_DEVICE(devp,f);

   if (bWaitOk) {

      if (ctx.timeout_ms) {

         // User doesn't want to wait forever
         ms_left =
            wait_for_completion_interruptible_timeout(&devp->comp_acq_or_xfer,
                                                      (ctx.timeout_ms * HZ) / 1000);

         if (0 == ms_left)
            res = -SIG_PX14_TIMED_OUT;
         else if (ms_left < 0)
            res = -ERESTARTSYS;	// Received a signal
         else
            res = 0;
      }
      else {

         // Wait as long as it takes
         res = wait_for_completion_interruptible(&devp->comp_acq_or_xfer);
         if (res != 0)
            res = -ERESTARTSYS;	// Received a signal
      }

      // We're done waiting; completion event received
      if (0 == res) {

         // See if we were cancelled
         PX14_LOCK_DEVICE(devp,f)
         {
            if (devp->bOpCancelled)
               res = -SIG_CANCELLED;
         }
         PX14_UNLOCK_DEVICE(devp,f);
      }
   }

   if (bCheckFifo && !res)
      res = CheckPciFifo_PX14(devp);

   return res;
}

// Checks state of PCI FIFO; returns 0 or -SIG_PX14_FIFO_OVERFLOW
int CheckPciFifo_PX14(px14_device* devp)
{
   // May be called from within spinlock!

   if (PX14REGMSK_FIFO_FULL & ReadDevReg(devp, 13))
      return -SIG_PX14_FIFO_OVERFLOW;

   return 0;
}

int px14ioc_reset_dcms (px14_device *devp)
{
   int res;

   // No input/output parameters

   PX14_LOCK_MUTEX(devp)
   {
      res = DoDcmResetWork(devp);
   }
   PX14_UNLOCK_MUTEX(devp);

   return res;
}

int px14ioc_get_device_state (px14_device *devp, u_long arg)
{
   int dev_state, res;
   unsigned long f;

   PX14_LOCK_DEVICE(devp,f)
   {
      dev_state = devp->DeviceState;
   }
   PX14_UNLOCK_DEVICE(devp,f);

   // Output is device state (int)
   if (__copy_to_user ((void*)arg, &dev_state, 4))
      return -EFAULT;

   return 0;
}

int px14ioc_hwcfg_refresh (px14_device *devp)
{
   unsigned long f;
   int res;

   PX14_LOCK_DEVICE(devp,f)
   {
      res = DoHwCfgRefreshWork_PX14(devp, 0);
   }
   PX14_UNLOCK_DEVICE(devp,f);

   return res;
}

int DoHwCfgRefreshWork_PX14 (px14_device* devp, int bDefaultsOnly)
{
   unsigned short magic_num;

   // At some point we may have this info in the EEPROM...
   devp->ram_samples = PX14_RAM_SIZE_IN_SAMPLES;

   if (bDefaultsOnly) {
      magic_num = ~PX14_EEPROM_MAGIC_NUMBER;
      devp->serialNum = 0xFFFFFFFF;
   }
   else {
      devp->serialNum = ReadEepromVal32_PX14 (devp, PX14EA_SERIALNUM_LOWER,
                                              PX14EA_SERIALNUM_UPPER);
      magic_num = ReadEepromVal_PX14 (devp, PX14EA_MAGIC_NUM);
   }

   if ((devp->serialNum == 0xFFFFFFFF) || (magic_num != PX14_EEPROM_MAGIC_NUMBER)) {
      // EEPROM is likely unconfigured; use a default serial number of 0
      //  since there may be some user-mode code that uses serial number
      //  of 0xFFFFFFFF as a sentry.
      devp->serialNum = 0;

      devp->features      = 0xFFFF & ~PX14FEATURE_NO_INTERNAL_CLOCK;
      devp->chanImps      = 0;
      devp->chanFilters   = 0;
      devp->fpgaTypeSys   = 0;
      devp->fpgaTypeSab   = 0;
      devp->boardRev      = PX14BRDREV_PX14400;
      devp->boardRevSub   = PX14BRDREVSUB_1_SP;
      devp->fwVerPci      = PX14FWV_PCI_MIN_SPLIT_INT_CLEAR;
      devp->fwVerSab      = 0;
      devp->fwVerPkg      = 0;
      devp->fwPreRelFlags = 0;
      devp->custFwPkg     = 0;
      devp->custFwPci     = 0;
      devp->custFwSab     = 0;
      devp->custHw        = 0x20;

   }
   else {
      devp->features      = ReadEepromVal_PX14(devp,   PX14EA_FEATURES);
      devp->chanImps      = ReadEepromVal_PX14(devp,   PX14EA_CHANNEL_IMP);
      devp->chanFilters   = ReadEepromVal_PX14(devp,   PX14EA_CHANFILTERTYPES);
      devp->fpgaTypeSys   = ReadEepromVal_PX14(devp,   PX14EA_SYS_FPGA_TYPE);
      devp->fpgaTypeSab   = ReadEepromVal_PX14(devp,   PX14EA_SAB_FPGA_TYPE);
      devp->boardRev      = ReadEepromVal_PX14(devp,   PX14EA_BOARD_REV);
      devp->boardRevSub   = ReadEepromVal_PX14(devp,   PX14EA_BOARD_REV_SUB);
      /* devp->fwVerPci      = ReadEepromVal32_PX14(devp, PX14EA_LOGIC_SUB_VER, PX14EA_LOGIC_VER); */
      devp->fwVerPci      = ReadHwRegFpgaVersion(devp);
      devp->fwVerSab      = ReadEepromVal32_PX14(devp, PX14EA_SAB_LOGIC_SUB_VER, PX14EA_SAB_LOGIC_VER);
      devp->fwVerPkg      = ReadEepromVal32_PX14(devp, PX14EA_FWPKG_VER_LOW, PX14EA_FWPKG_VER_HIGH);
      devp->fwPreRelFlags = ReadEepromVal_PX14(devp,   PX14EA_PRE_RELEASE);
      devp->custFwPkg     = ReadEepromVal_PX14(devp,   PX14EA_CUST_FWPKG_ENUM);
      devp->custFwPci     = ReadEepromVal_PX14(devp,   PX14EA_CUSTOM_LOGIC_ENUM);
      devp->custFwSab     = ReadEepromVal_PX14(devp,   PX14EA_CUSTOM_SAB_LOGIC_ENUM);
      devp->custHw        = ReadEepromVal_PX14(devp,   PX14EA_CUSTOM_HW_ENUM);
   }

   return 0;
}

int px14ioc_driver_stats (px14_device *devp, u_long arg)
{
   int res, bResetCounters;
   PX14S_DRIVER_STATS ctx;
   unsigned long f;

   // Input is a PX14S_DRIVER_STATS structure
   res = __copy_from_user(&ctx, (void*)arg, sizeof(PX14S_DRIVER_STATS));
   if (res != 0)
      return -EFAULT;

   bResetCounters = ctx.nConnections;

   PX14_LOCK_DEVICE(devp,f)
   {
      ctx.nConnections = atomic_read(&devp->refCount);
      ctx.isr_cnt = atomic_read(&devp->stat_isr_count);
      ctx.dma_finished_cnt = atomic_read(&devp->stat_dma_comp);
      ctx.dma_started_cnt = atomic_read(&devp->stat_dma_start);
      ctx.acq_started_cnt = atomic_read(&devp->stat_acq_start);
      ctx.acq_finished_cnt = atomic_read(&devp->stat_acq_comp);
      ctx.dcm_reset_cnt = atomic_read(&devp->stat_dcm_resets);
      ctx.dma_bytes = devp->stat_dma_bytes;

      if (bResetCounters) {
         atomic_set(&devp->stat_isr_count,  0);
         atomic_set(&devp->stat_dma_start,  0);
         atomic_set(&devp->stat_dma_comp,   0);
         atomic_set(&devp->stat_acq_start,  0);
         atomic_set(&devp->stat_acq_comp,   0);
         atomic_set(&devp->stat_dcm_resets, 0);
         devp->stat_dma_bytes = 0;
      }
   }
   PX14_UNLOCK_DEVICE(devp,f);

   // Output is a PX14S_DRIVER_STATS structure
   res = __copy_to_user ((void*)arg, &ctx, sizeof(PX14S_DRIVER_STATS))
      ? -EFAULT : 0;

   return res;
}

int px14ioc_driver_version (px14_device *devp, u_long arg)
{
   PX14S_DRIVER_VER ctx;
   int res;

   PX14_CT_ASSERT(16 == sizeof(PX14S_DRIVER_VER));

   // Output is a PX14S_DRIVER_VER structure
   ctx.struct_size = sizeof(PX14S_DRIVER_VER);
   ctx.version = MY_DRIVER_VER64;

   if (__copy_to_user ((void*)arg, &ctx, sizeof(PX14S_DRIVER_VER)))
      return -EFAULT;

   return 0;
}

int px14ioc_need_dcm_reset (px14_device *devp, u_long arg)
{
   unsigned long f;
   unsigned ctx;

   // Input is a unsigned
   if (__copy_from_user(&ctx, (void*)arg, sizeof(unsigned)))
      return -EFAULT;

   PX14_LOCK_DEVICE(devp,f)
   {
      if (ctx)
         devp->dwFlags |=  DEVF_NEED_DCM_RESET;
      else
         devp->dwFlags &= ~DEVF_NEED_DCM_RESET;
   }
   PX14_UNLOCK_DEVICE(devp,f);

   return 0;
}

int px14ioc_fw_versions (px14_device *devp, u_long arg)
{
   PX14S_FW_VERSIONS ctx;
   int res;

   PX14_CT_ASSERT(24 == sizeof(PX14S_FW_VERSIONS));

   memset (&ctx, 0, sizeof(PX14S_FW_VERSIONS));
   ctx.struct_size   = sizeof(PX14S_FW_VERSIONS);
   ctx.fw_ver_pci    = devp->fwVerPci;
   ctx.fw_ver_sab    = devp->fwVerSab;
   ctx.fw_ver_pkg    = devp->fwVerPkg;
   ctx.pre_rel_flags = devp->fwPreRelFlags;

   // Output is a PX14S_FW_VERSIONS structure
   if (__copy_to_user ((void*)arg, &ctx, sizeof(PX14S_FW_VERSIONS)))
      return -EFAULT;

   return 0;
}

int px14ioc_raw_reg_io (px14_device *devp, u_long arg)
{
   PX14S_RAW_REG_IO ctx;
   size_t szOff;
   u32* basep;
   int res;

   // Input is a PX14S_RAW_REG_IO structure
   if (__copy_from_user(&ctx, (void*)arg, sizeof(PX14S_RAW_REG_IO)))
      return -EFAULT;

   if ((ctx.nRegion < 0) || (ctx.nRegion >= PX14_MEMS_CNT))
      return -SIG_INVALIDARG;

   basep = (u32*)devp->MemMapped[ctx.nRegion];
   if (NULL == basep)
      return -SIG_INVALIDARG;

   if (ctx.bRead)
      ctx.regVal = ioread32(basep + ctx.nRegister);
   else
      iowrite32 ((u32)ctx.regVal, basep + ctx.nRegister);

   // Copy result
   if (ctx.bRead) {
      szOff = offsetof(PX14S_RAW_REG_IO, regVal);
      if (__copy_to_user ((void*)(arg + szOff), &ctx.regVal, sizeof(int)))
         return -EFAULT;
   }

   return 0;
}

int px14ioc_get_hw_config_ex (px14_device *devp, u_long arg)
{
   PX14S_HW_CONFIG_EX ctx;
   unsigned long f;

   // Get size of input struct so we know how much data to copy
   if (__copy_from_user(&ctx, (void*)arg, sizeof(unsigned int)))
      return -EFAULT;

   if (ctx.struct_size < _PX14SO_PX14S_HW_CONFIG_EX_V1)
      return -SIG_INVALIDARG;
   if ((ctx.struct_size > _PX14SO_PX14S_HW_CONFIG_EX_V1) &&
       (ctx.struct_size < _PX14SO_PX14S_HW_CONFIG_EX_V2)) {
      return -SIG_INVALIDARG;
   }
   // Don't copy more than what we know
   if (ctx.struct_size > sizeof(PX14S_HW_CONFIG_EX))
      ctx.struct_size = sizeof(PX14S_HW_CONFIG_EX);

   PX14_LOCK_DEVICE(devp,f)
   {
      ctx.chan_imps =     devp->chanImps;
      ctx.chan_filters =  devp->chanFilters;
      ctx.sys_fpga_type = devp->fpgaTypeSys;
      ctx.sab_fpga_type = devp->fpgaTypeSab;
      ctx.board_rev =     devp->boardRev;
      ctx.board_rev_sub = devp->boardRevSub;

      if (ctx.struct_size > _PX14SO_PX14S_HW_CONFIG_EX_V1)
      {
         ctx.struct_size = _PX14SO_PX14S_HW_CONFIG_EX_V2;
         ctx.custFwPkg   = devp->custFwPkg;
         ctx.custFwPci   = devp->custFwPci;
         ctx.custFwSab   = devp->custFwSab;
         ctx.custHw      = devp->custHw;

         //if (> V2) {
         //  future. ALSO UPDATE ASSIGNMENT TO struct_size WILL NEED TO CHANGE OR
         //  this code will NEVER run!!!
         //  Ultimately, we just want to assign struct_size (on output) to the
         //  size of the structure that we know here in teh driver.
      }
      }
      PX14_UNLOCK_DEVICE(devp,f);

      if (__copy_to_user((void*)arg, &ctx, ctx.struct_size))
         return -EFAULT;

      return 0;
   }
