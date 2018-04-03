/** @file  px14_main.c
  @brief PX14400 driver module init/cleanup
  */
#include "px14_drv.h"
#include <linux/init.h>

// This module only supports a single PCI device, so we need kernel support
// for PCI.
#ifndef CONFIG_PCI
#error sig_px14400 : Compiling PCI device driver without kernel PCI support.
#endif

///////////////////////
// Module prototypes.

// Module initialization function. Returns 0 on success.
static int px14_init_module (void);

// Module cleanup function.
static void px14_cleanup_module (void);

// Finds and initializes all PX14400 devices. Returns 0
//  on success.
static int px14_init_all_devices (struct px14400_module_state *pMod);

// Frees all P14 devices and resources. This routine un-does what the
//  find_all_px14_devices function does.
static void px14_free_all_devices (struct px14400_module_state *pMod);

// Initializes a single PX14400 device. If this function fails, free_px14_device
//  should be called to free any resources that were allocated.
static int px14_init_sw_device (int nDev,
                                struct pci_dev *pDevPci,
                                px14_device *pDevP14);

// Initializes a single P14's hardware. This function assumes that all device
//  initialization has completed. Returns 0 on success.
static int px14_init_hardware (px14_device *pDev);

// Frees all resources associated with the given P14 device.
static void px14_free_device (int nDev,
                              px14_device *pDevP14);

// Returns a count of physical PX14400 devices in the system.
static int px14_physical_count(void);

// Proc filesystem read function.
#ifndef NO_PROC_ENTRY
#  if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
   static ssize_t px14_read_proc (char *page, char **start, off_t offset, int count,
                              int *eof, void *datap);
#  else
   static struct file_operations g_popsPX14;
   static ssize_t px14_read_proc (struct file *filp, char *buf, size_t count, loff_t *offp);
#  endif
#endif

/* -- Module globals -- */

/** PX14400 file operation structure. (Kernel interface to module.) */
static struct file_operations g_fopsPX14;
/** PX14400 VM operations structure. (Used in DMA buffer mapping.) */
struct vm_operations_struct g_vmOpsPX14;
/** PX14400 module state information. */
struct px14400_module_state *g_pModPX14 = NULL;

/* -- Exported module globals. */
/// PX14400 major number
int px14_major = PX14_MAJOR_NUM_DEF;

module_param(px14_major, int, S_IRUGO);

// Define module initialization and cleanup functions.
module_init(px14_init_module);
module_exit(px14_cleanup_module);

// Define module attributes.
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Signatec");
MODULE_DESCRIPTION("Signatec PX14400 device driver module");
MODULE_SUPPORTED_DEVICE(PX14_DEVICE_NAME);

///////////////////////////////////////////////////////////////////////////
// Module/device initialization routines

// Module initialization function. This function is called when
//  a module is being loaded. The "__init" declaration marks this
//  function as being able to unloaded after it has been run.
int __init px14_init_module (void)
{
   int res;
   struct class* classp;

   VERBOSE_LOG("Initializing module.\n");

   // Make sure there are physical PX14400 devices present.
   if (0 == px14_physical_count()) {
      VERBOSE_LOG_ERR("No PX14400 devices present.\n");
      return -ENODEV;
   }

   // Allocate and initialize our module state structure.
   g_pModPX14 = kmalloc(sizeof(struct px14400_module_state), GFP_KERNEL);
   if (NULL == g_pModPX14) {
      VERBOSE_LOG_ERR("Failed to allocate module state structure.\n");
      return -ENOMEM;
   }
   memset(g_pModPX14, 0, sizeof(struct px14400_module_state));

   // Initialize file operations structure.
   memset (&g_fopsPX14, 0, sizeof(struct file_operations));
   g_fopsPX14.read    = px14_read;
   g_fopsPX14.write   = px14_write;
   g_fopsPX14.mmap    = px14_mmap;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
   g_fopsPX14.unlocked_ioctl = px14_ioctl_unlocked;
#else
   g_fopsPX14.ioctl   = px14_ioctl;
#endif
   g_fopsPX14.open    = px14_open;
   g_fopsPX14.release = px14_release;
   cdev_init (&g_pModPX14->cdev, &g_fopsPX14);
   g_pModPX14->cdev.owner = THIS_MODULE;

#ifndef NO_PROC_ENTRY
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
   memset (&g_popsPX14, 0, sizeof(struct file_operations));
   g_popsPX14.read = px14_read_proc;
#  endif
#endif

   // Allocate char device numbers
   res = alloc_chrdev_region (&g_pModPX14->majorID, 0,
                              PX14_MAX_LOCAL_DEVICE_COUNT, PX14_DEVICE_NAME);
   if (res < 0) {
      VERBOSE_LOG_ERR("Failed to allocate device numbers: %d\n", res);
      goto emi_out1;
   }

   // Register the device
   res = cdev_add (&g_pModPX14->cdev, g_pModPX14->majorID,
                   PX14_MAX_LOCAL_DEVICE_COUNT);
   if (res < 0) {
      VERBOSE_LOG_ERR("Failed to register driver: %d\n", res);
      goto emi_out2;
   }

   // Create a class_simple object that we can use to manage the creation of
   //  device nodes that user-space will use for device handles
   classp = class_create (THIS_MODULE, PX14_DEVICE_NAME);
   if (IS_ERR(classp)) {
      VERBOSE_LOG_ERR ("Failed to create class object (%ld)\n", PTR_ERR(classp));
      goto emi_out2;
   }
   else
      g_pModPX14->classp = classp;

   // Find and initialize all PX14400 devices
   if (0 != (res = px14_init_all_devices (g_pModPX14))) {
      VERBOSE_LOG_ERR ("Failed to init all devices: %d\n", res);
      goto emi_out3;
   }

   return 0;

emi_out3:
   px14_free_all_devices (g_pModPX14);
   if (g_pModPX14->classp) class_destroy (g_pModPX14->classp);
   cdev_del (&g_pModPX14->cdev);
emi_out2:
   unregister_chrdev_region (g_pModPX14->majorID, PX14_MAX_LOCAL_DEVICE_COUNT);
emi_out1:
   SAFE_KFREE (g_pModPX14);
   g_pModPX14 = NULL;
   return res;
}

int __init px14_init_all_devices (struct px14400_module_state *pMod)
{
   int i, nDevice, res, nDevicesStarted;
   unsigned int device_id;
   struct pci_dev* pPciDev;
   const char* pDevStr;

   // Get number of PX14400 devices installed in the system.
   if (0 == (nDevice = px14_physical_count()))
      return -ENODEV;

   VERBOSE_LOG("Found %d PX14400-family device(s).\n", nDevice);

   // Allocate space for device state.
   pMod->pDev = kmalloc (nDevice * sizeof(px14_device), GFP_KERNEL);
   if (NULL == pMod->pDev) {
      VERBOSE_LOG_ERR("Failed to allocate device state buffer.\n");
      return -ENOMEM;
   }
   memset (pMod->pDev, 0, nDevice * sizeof(px14_device));

   // Cache device count.
   pMod->nDevices = nDevice;

   nDevicesStarted = 0;
   nDevice = 0;

   for (i=0; i<2; i++) {

      if (0 == i) {
         device_id = PX14_DEVICE_ID;
         pDevStr   = PX14_REV1_NAMEA;

      }
      else {
         device_id = PX14_DEVICE_ID_PX12500;
         pDevStr   = PX14_REV2_NAMEA;
      }

      // Now walk each device, initializing as we go...
      for (pPciDev=NULL;
           NULL!=(pPciDev=pci_get_device(PX14_VENDOR_ID,device_id,pPciDev));)
      {
         // Initialize the device. This initializes the device with respect to the
         //  operating system. No device-specific IO is done yet.
         res = px14_init_sw_device (nDevice, pPciDev, &pMod->pDev[nDevice]);
         if (0 != res) {
            VERBOSE_LOG_ERR ("Failed to initialize %s device %d. Err=%d\n",
                             pDevStr, nDevice, res);
            px14_free_device (nDevice, &pMod->pDev[nDevice]);
         }
         else {
            // We've initialized the device, now we need to initialize the hardware.
            if (0 != (res = px14_init_hardware (&pMod->pDev[nDevice]))) {
               VERBOSE_LOG_ERR ("Failed to initialize %s hardware for "
                                "device %d. Err=%d\n", pDevStr, nDevice, res);
               // Release any resources that were allocated.
               px14_free_device (nDevice, &pMod->pDev[nDevice]);
            }
            else
               nDevicesStarted++;
         }
         nDevice++;
      }
   }

   // Did we fail to start all devices in the system?
   if (0 == nDevicesStarted) {
      pMod->nDevices = 0;
      SAFE_KFREE(pMod->pDev);

      return -ENODEV;
   }

   return 0;
}

int __init px14_init_sw_device (int nDev,
                                struct pci_dev* pDevPci,
                                px14_device* devp)
{
   u_long dwStart, dwEnd, dwFlags, dwLen, dma_mask;
   int res, i, curPort, curMem;

   // Zero out the current content
   memset (devp, 0, sizeof(px14_device));

   // Initialize what we can...
   snprintf(devp->devName, MAX_PX14_DEVNAME, PX14_DEVICE_NAME "%d", nDev);
   INIT_LIST_HEAD(&devp->dma_buf_list);// Init DMA buffer descriptor list.
   init_completion(&devp->comp_acq_or_xfer);
   devp->magic = PX14_DEVICE_MAGIC;    // Magic number for device state.
   spin_lock_init(&devp->devLock);     // Initialize device spinlock.
   sema_init(&devp->devMutex, 1);      // Init device semaphore as mutex
   atomic_set(&devp->refCount,0);
   devp->pOsDevice = pDevPci;          // Cache the OS PCI struct
   devp->dwFlags = DEVFINIT;           // Initialize implementation flags.
   devp->brdNum = nDev;                // (0-biased) ordinal board number.
   devp->DeviceState = PX14STATE_IDLE;

   // Init stats
   atomic_set(&devp->stat_isr_count,  0);
   atomic_set(&devp->stat_dma_start,  0);
   atomic_set(&devp->stat_dma_comp,   0);
   atomic_set(&devp->stat_acq_start,  0);
   atomic_set(&devp->stat_acq_comp,   0);
   atomic_set(&devp->stat_jtag_ops,   0);
   atomic_set(&devp->stat_dcm_resets, 0);

   // Enable (wake up) the PCI device. This MUST be done before any
   // board access.
   if (0 != (res = pci_enable_device(pDevPci))) {
      VERBOSE_LOG_ERR("Failed to enable raw device %d. (%d)\n", nDev, res);
      return -ENODEV;
   }
   devp->dwFlags |= DEVF_ENABLED;

   // Cache device's IRQ from system PCI device structure.
   devp->irqNum = (int)pDevPci->irq;

   // Grab our hardware resources
   for (i=0,curPort=0,curMem=0; i<6; i++) {

      // Get start of resource's address
      dwStart = pci_resource_start(pDevPci, i);
      if (0 == dwStart)
         break;   // No more resources

      dwEnd   = pci_resource_end  (pDevPci, i);
      dwFlags = pci_resource_flags(pDevPci, i);
      dwLen   = 1 + dwEnd - dwStart;

      if (dwFlags & IORESOURCE_MEM) {

         if (curMem < PX14_MEMS_CNT) {

            // Allocate region
            if (!request_mem_region (dwStart, dwLen, devp->devName)) {
               VERBOSE_LOG("Allocated memory region stolen! "
                           "(Start: 0x%04lX, Len: 0x%04lX)\n", dwStart, dwLen);
               return -EIO;
            }

            devp->MemMapped[curMem] = ioremap(dwStart, dwLen);
            if (NULL == devp->MemMapped[curMem]) {
               release_mem_region(dwStart, dwLen);
               return -EIO;
            }

            devp->MemBase [curMem] = dwStart;
            devp->MemCount[curMem] = (u_int)dwLen;

            VERBOSE_LOG("Memory IO region %d -> Start: 0x%04lX, Len: 0x%04X\n",
                        curMem, devp->MemBase[curMem], devp->MemCount[curMem]);
            curMem++;
         }
         else {
            VERBOSE_LOG_ERR("Too many memory IO regions allocated!\n");
            BUG();
            return -ENOMEM;
         }
      }
   }

   // Enable bus-mastering
   pci_set_master(pDevPci);

   // Set DMA mask to use for our device
   dma_mask = 0x00000000FFFFFFFFULL; // 32-bit DMA addresses only
   if (pci_set_dma_mask(pDevPci, dma_mask)) {
      printk (PX14_KERN_WARNING "No suitable DMA available.\n");
      return -ENOMEM;
   }

   // Allocate a small DMA buffer that the driver will use for driver-buffered
   //  transfers. These are device data transfers that do not require the user
   //  to explicitly allocate a DMA buffer.
   if (AllocateDriverDmaBuffer_PX14 (devp)) {
      VERBOSE_LOG_ERR ("Failed to allocate DMA buffer for " \
                       "driver-buffered xfers\n");
   }

   /* Apr 29, 2013, Kiong:
    * access method for g_pModPX14 (file-scoped) is not consistent;
    * sometimes it was passed, sometimes not.
    * i'm kinda lazy today :-) */
   if (g_pModPX14->classp) {
      dev_t d = MKDEV(MAJOR(g_pModPX14->majorID), nDev);
      devp->devicep = device_create (g_pModPX14->classp, NULL, d, devp,
                                     "sig_px14400%d", nDev);
      if (IS_ERR (devp->devicep)) {
         VERBOSE_LOG_ERR ("Failed to create class device: %ld", PTR_ERR(devp->devicep));
      }
   }

#ifndef NO_PROC_ENTRY
   // Create the proc file system name and create the entry.
   strcpy (devp->pfsName, "driver/");
   strcat (devp->pfsName, devp->devName);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
   if (0 == create_proc_read_entry (devp->pfsName, 0, NULL, px14_read_proc, devp)) {
#else
   if (0 == proc_create_data (devp->pfsName, 0, NULL, &g_popsPX14, devp)) {
#endif
      printk (PX14_ERROR "Failed to create proc filesystem entry "
              "for device: %s\n", devp->devName);
   }
   else
      devp->dwFlags |= DEVF_PFS_ENTRY;
#endif

   devp->dwFlags |= DEVF_STARTED;

   return 0;
}

///////////////////////////////////////////////////////////////////////////
// Module/device cleanup routines

// Module cleanup function. This function is called when the driver
//  is being unloaded. It should undo what the module initialization
//  function did. The "__exit" declaration marks this routine as
//  unloadable after it has been executed.
void __exit px14_cleanup_module (void)
{
   int res;

   VERBOSE_LOG("Cleaning up module.\n");

   if (NULL != g_pModPX14) {
      // Unregister our driver.
      /* unregister_chrdev(g_pModPX14->uMajor, PX14_DEVICE_NAME); */
      // Release all P14 devices' resources.
      px14_free_all_devices (g_pModPX14);
      // Release class, devicea and major nos.
      if (g_pModPX14->classp)
         class_destroy (g_pModPX14->classp);
      cdev_del (&g_pModPX14->cdev);
      unregister_chrdev_region (g_pModPX14->majorID, PX14_MAX_LOCAL_DEVICE_COUNT);
      // Free the module state.
      SAFE_KFREE (g_pModPX14);
      g_pModPX14 = 0;
   }
}

void px14_free_all_devices (struct px14400_module_state *pMod)
{
   int i;

   // Free all initialized devices...
   for (i=0; i<pMod->nDevices; i++)
      px14_free_device(i, &pMod->pDev[i]);
}

void px14_free_device (int nDev,
                       px14_device *devp)
{
   struct px14_free_dma_buffer fdb;
   struct list_head lstToFree;
   int i, res;

   VERBOSE_LOG("Freeing device %d\n", devp->brdNum);

   INIT_LIST_HEAD(&lstToFree);

#ifndef NO_PROC_ENTRY
   // Remove /proc entry if necessary.
   if (devp->dwFlags & DEVF_PFS_ENTRY) {
      remove_proc_entry(devp->pfsName, NULL);
      devp->dwFlags &= ~DEVF_PFS_ENTRY;
   }
#endif

   if (devp->dwFlags & DEVF_ENABLED) {
      // Release resources

      if (devp->btIntBufCh1) {
         kfree(devp->btIntBufCh1);
         devp->btIntBufCh1 = NULL;
      }

      if (devp->btIntBufCh2) {
         kfree(devp->btIntBufCh2);
         devp->btIntBufCh2 = NULL;
      }

      // Free all DMA buffers.
      fdb.how = PX14FDB_ALL;
      if (0 != (res = px14_free_dma_buffer_work(devp, &fdb, NULL, &lstToFree))){
         printk (PX14_KERN_WARNING
                 "Failed to free all renegade DMA buffers: %d\n", res);
      }
      FreeDmaBufferList_PX14(devp, &lstToFree);

      // Release and free memory regions.
      for (i=0; i<PX14_MEMS_CNT; i++) {
         if (devp->MemMapped[i]) {
            // Unmap from kernel
            iounmap(devp->MemMapped[i]);
            devp->MemMapped[i] = 0;
            // Free resource
            release_mem_region(devp->MemBase[i], devp->MemCount[i]);
            devp->MemCount[i] = devp->MemBase[i] = 0;
         }
      }

      // Remove device class
      if (devp->devicep) {
         dev_t d = MKDEV(MAJOR(g_pModPX14->majorID), nDev);
         device_destroy (g_pModPX14->classp, d);
         devp->devicep = NULL;
      }

      devp->dwFlags &= ~DEVF_STARTED;

      // Disable the device.
      if (NULL != devp->pOsDevice)
         pci_disable_device(devp->pOsDevice);

      devp->dwFlags &= ~DEVF_ENABLED;
   }
   else {
      VERBOSE_LOG ("Freeing disabled device.\n");
   }
}

///////////////////////////////////////////////////////////////////////////
// Miscellanious routines

/** Returns the number of physical PX14400 devices present in the system. */
int __init px14_physical_count(void)
{
   struct pci_dev* pPciDev = NULL;
   int nDevice = 0;

   // PX14400 devices
   while(NULL!=(pPciDev=pci_get_device(PX14_VENDOR_ID,PX14_DEVICE_ID,pPciDev)))
      nDevice++;

   // PX12500 devices
   while(NULL!=(pPciDev=pci_get_device(PX14_VENDOR_ID,PX14_DEVICE_ID_PX12500,pPciDev)))
      nDevice++;

   return nDevice;
}

int __init px14_init_hardware (px14_device *devp)
{
   int res;

   res = CheckEepromIoPX14(devp);
   if (res != 0) {
      PX14_DMESG("Warning: EEPROM access test failed\n");
   }
   // Refresh our hardware configuration data; if EEPROM access failed we'll
   //  used assumed defaults
   DoHwCfgRefreshWork_PX14(devp, res);

   PX14_DMESG("PX14400[%d] SN: %u\n", devp->brdNum, devp->serialNum);

   // Put all hardware settings into a known state
   InitializeHardwareSettingsPX14(devp);

   return 0;
}

///////////////////////////////////////////////////////////////////////////
// A few module file routines

int px14_open (struct inode *pNode, struct file *pFile)
{
   unsigned long irq_flags = 0;
   px14_device *devp;
   u_int dwFlags;
   int minor, res, refCount;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
   // SA_* are deprecated around this point
   irq_flags = SA_SHIRQ;
#else
   irq_flags = IRQF_SHARED;
#endif

   // Make sure that the minor number is okay.
   minor = MINOR(pNode->i_rdev);
   if (minor >= g_pModPX14->nDevices) {
      VERBOSE_LOG("Open failed: Invalid minor number. (Minor:%d)\n", minor);
      return -ENODEV;
   }

   // Get a pointer to the device that is being opened.
   devp = &g_pModPX14->pDev[minor];

   // Cache this device's state data address in the file's private_data
   //  member for quick access later.
   pFile->private_data = devp;

   PX14_LOCK_MUTEX(devp) {

      // Make sure that the device was started properly.
      dwFlags = DEVF_ENABLED | DEVF_STARTED;
      if (dwFlags != (devp->dwFlags & dwFlags)) {
         PX14_UNLOCK_MUTEX_BREAK(devp);
         VERBOSE_LOG("Open failed: Device %d has not been started!\n", minor);
         return -ENODEV;
      }

      refCount = atomic_read(&devp->refCount);

      // Enable interrupt on first instantiation
      if (!refCount && devp->irqNum) {
         res = request_irq(devp->irqNum, px14_isr, irq_flags,
                           devp->devName, devp);
         if (0 != res) {
            PX14_UNLOCK_MUTEX_BREAK(devp);
            VERBOSE_LOG_ERR("Failed to get IRQ for interrupt. (%d)\n", res);
            return res;
         }
      }

      // Increment the device's reference count
      atomic_inc(&devp->refCount);

   } PX14_UNLOCK_MUTEX(devp);

   VERBOSE_LOG("Opening PX14400 device #%d\n", minor);

   return 0;
}

int px14_release (struct inode *pNode, struct file *filp)
{
   struct px14_free_dma_buffer fdb;
   struct list_head lstToFree;
   px14_device *devp;
   int f, refCount;

   VERBOSE_LOG("Releasing P14 device #%d\n", (int)MINOR(pNode->i_rdev));

   INIT_LIST_HEAD(&lstToFree);

   // File's private_data member contains a pointer to the device
   //  that this file references.
   devp = (px14_device *)filp->private_data;

   if (devp->magic != PX14_DEVICE_MAGIC) {
      VERBOSE_LOG("Magic number (0x%08X) is not expected (0x%08X)",
                  devp->magic, PX14_DEVICE_MAGIC);
   }

   PX14_LOCK_MUTEX(devp) {

      // Decrement the device's refernece count.
      atomic_dec(&devp->refCount);
      refCount = atomic_read(&devp->refCount);

      // A little sanity checking...
      if (refCount < 0) {
         printk (PX14_ERROR "Device %d has a negative reference count "
                 "after releasing!\n", devp->brdNum);
         atomic_set(&devp->refCount, 0);
         refCount = 0;
      }

      // If we have no more handles to this device, or if the closing
      //  handle started a DMA transfer, then we'll disable DMA and put
      //  the board back into standby mode. The former is for safety,
      //  the latter is required since we're going to free the DMA buffer
      //  to which the DMA is transferring

      if (!refCount || ((devp->DeviceState == PX14STATE_DMA_XFER_FAST) &&
                        devp->dma_filp == filp)){
         ResetDma_PX14(devp);
         SetOperatingMode_PX14(devp, PX14MODE_STANDBY);
         devp->DeviceState = PX14STATE_IDLE;
      }

      if (0 == refCount) {

         // No more handles, free all DMA buffers
         fdb.how = PX14FDB_ALL_USER;
         px14_free_dma_buffer_work(devp, &fdb, NULL, &lstToFree);

         // Release the IRQ, no more handles to this device exist.
         if (devp->irqNum)
            free_irq (devp->irqNum, devp);

         // Can't have a JTAG owner if no connections
         devp->jtag_filp = NULL;
         devp->dma_filp = NULL;
      }
      else {
         // Still have some handles to this device; free all buffers allocated
         //  by closing handle
         fdb.how = PX14FDB_FILE;
         fdb.filp = filp;
         px14_free_dma_buffer_work(devp, &fdb, NULL, &lstToFree);
      }

   } PX14_UNLOCK_MUTEX(devp);

   // Can't iounmap with spinlock held
   FreeDmaBufferList_PX14(devp, &lstToFree);

   UNUSED_PARAM(pNode);

   return 0;
}

void DoDriverStall_PX14 (u_int microsecs, int bNeverSleep)
{
   u_int milliseconds;

   if (microsecs) {

      if ((microsecs > 5 * 1000) && !bNeverSleep) {
         // Sleep (does not use CPU)
         milliseconds = (microsecs + 999) / 1000; // Round up to nearest ms
         msleep(milliseconds);
      }
      else {
         // Stall (uses CPU)
         milliseconds = (microsecs / 1000);
         if (milliseconds) {
            mdelay(milliseconds);
            microsecs -= (milliseconds * 1000);
         }
         if (microsecs)
            udelay(microsecs);
      }
   }
}

#ifndef NO_PROC_ENTRY

static const char* ModeStr(int om)
{
   const char* s;

   switch (om) {
      case PX14MODE_STANDBY:            s = "Standby";         break;
      case PX14MODE_ACQ_RAM:            s = "RAM Acq";         break;
      case PX14MODE_ACQ_PCI_BUF:        s = "Pci Acq (buf)";   break;
      case PX14MODE_ACQ_SAB:            s = "SAB Acq";         break;
      case PX14MODE_RAM_READ_PCI:       s = "PCI Xfer (to host)"; break;
      case PX14MODE_RAM_READ_SAB:       s = "SAB Xfer";           break;
      case PX14MODE_RAM_WRITE_PCI:      s = "PCI Xfer (to dev)";  break;
      case PX14MODE_ACQ_SAB_BUF:        s = "SAB Acq (buf)";   break;
      case PX14MODE_ACQ_PCI_SMALL_FIFO: s = "Pci Acq (old)";   break;
      case PX14MODE_OFF:                s = "Off";             break;
      default:                          s = "<unknown>";       break;
   }
   return s;
}

// A VERY implementation specific macro used to make this function's code
// look pretty. Prettier, anyway.
#define PX14_PROC_ENTRY(fmt,args...)   \
   res=snprintf(page+wrote,count,fmt,## args);wrote+=res;\
   if(0>=(count-=res))break

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
ssize_t px14_read_proc (char *page, char **start, off_t offset, int count,
                        int *eof, void *datap)
{
#else
ssize_t px14_read_proc (struct file *filp, char *buf, size_t count, loff_t *offp)
{
   char page[512];// = kmalloc (1024, GFP_KERNEL);
   void *datap = filp->private_data;

   //if (NULL == page)
   //   return -ENOMEM;
#endif

   px14_device *devp;
   int wrote, res, i;
   unsigned long f;

   // To simplify things, we're going to assume that all output will fit in
   // first attempt.

   devp = (px14_device*)datap;
   wrote = 0;

   if (devp->magic != PX14_DEVICE_MAGIC)
      return 0;

   PX14_LOCK_DEVICE(devp,f)
   do {
      PX14_PROC_ENTRY("SN: %u\n", devp->serialNum);
      PX14_PROC_ENTRY("BrdNum: %d\n", devp->brdNum+1);
      if (devp->boardRev == PX14BRDREV_1)
      {
         PX14_PROC_ENTRY("Revision: PX14400\n");
      }
      else if (devp->boardRev == PX14BRDREV_PX12500)
      {
         PX14_PROC_ENTRY("Revision: PX12500\n");
      }
      else
      {
         PX14_PROC_ENTRY("Revision: UNKNOWN\n");
      }
      PX14_PROC_ENTRY("RefCnt: %d\n", atomic_read(&devp->refCount));
      PX14_PROC_ENTRY("Flags: 0x%08X\n", devp->dwFlags);
      PX14_PROC_ENTRY("DeviceState: %d\n", devp->DeviceState);
      PX14_PROC_ENTRY("IRQ: %d\n", devp->irqNum);
      PX14_PROC_ENTRY("OM: %s\n", ModeStr(PX14_OP_MODE(devp)));

      PX14_PROC_ENTRY("-Stats---\n");
      PX14_PROC_ENTRY(" ISR counter  :  %d\n",
                      atomic_read(&devp->stat_isr_count));
      PX14_PROC_ENTRY(" DMAs started :  %d\n",
                      atomic_read(&devp->stat_dma_start));
      PX14_PROC_ENTRY(" DMAs finished:  %d\n",
                      atomic_read(&devp->stat_dma_comp));
      PX14_PROC_ENTRY(" Acqs started :  %d\n",
                      atomic_read(&devp->stat_acq_start));
      PX14_PROC_ENTRY(" Acqs finished:  %d\n",
                      atomic_read(&devp->stat_acq_comp));
      PX14_PROC_ENTRY(" JTAG ops     :  %d\n",
                      atomic_read(&devp->stat_jtag_ops));
      PX14_PROC_ENTRY(" DCM resets   :  %d\n",
                      atomic_read(&devp->stat_dcm_resets));

      //PX14_PROC_ENTRY("-DMA state---\n");
      //PX14_PROC_ENTRY(" Bus addr:  0x%08lX\n", (u_long)devp->dmaAddr);
      //PX14_PROC_ENTRY(" Length: 0x%08X\n", devp->dmaLen);
      //PX14_PROC_ENTRY(" Dir: %d\n", devp->dmaDir);

   } while(0);
   PX14_UNLOCK_DEVICE(devp,f);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
   *eof = 1;   // No more data to write.
   return wrote;
#else
   res = 0;
   if (copy_to_user (buf, page, wrote))
      res = -EFAULT;
   //kfree (page);
   return res; // 0, if OK; which is equivalent to eof==1
#endif
}
#endif

#ifndef NO_TTY_OUT
static void tty_out (const char *format, ...)
{
   struct tty_struct *my_tty;
   char buf[MAX_TTY_OUT];
   va_list ap;
   int res;

   // Is there a TTY attached to the current process?
   if (NULL != (my_tty = current->tty)) {
      va_start(ap, format);
      res = vsnprintf(buf, MAX_TTY_OUT, format, ap);
      va_end(ap);

      // Is there any output?
      if (res > 0) {
         // Display output.
         (*(my_tty->driver).write)(my_tty, 0, buf, strlen(buf));
         // Add a newline.
         (*(my_tty->driver).write)(my_tty, 0, "\015\012", 2);
      }
   }
}
#endif
