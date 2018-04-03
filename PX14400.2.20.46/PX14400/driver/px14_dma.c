/** @file 	px14_dma.c
  @brief	DMA transfer/buffer releated routines
  */
#include "px14_drv.h"

/// Address is current user-space address
#define ADDR_USER	0
/// Address is kernel logical address
#define ADDR_KERN	1
/// Address is physical address
#define ADDR_PHYS	2
#define ADDRMAX	3 	// Not a valid ADDR_* setting


//////////////////////////////////////////////////////////////////////////////
// File static function prototypes.

static int PreDmaXferCheck (px14_device* devp, PX14S_DMA_XFER* ctxp);
static int GetDmaOpBusAddr (px14_device* devp, PX14S_DMA_XFER* ctxp,
                            dma_addr_t* p);

// Map a user-space address to kernel logical address
static int lookup_dma_kern_addr (px14_device* devp,
                                 u_long userAddr, struct mm_struct *asp,
                                 u_long *pAddrKern, int bAllowOffset,
                                 u_int *pBytesRemaining,
                                 struct dma_buf_desc **descpp);

/// Function used to select DMA buffers to free for various situations
typedef int (*pfdbFindFunc)(px14_device* devp, PX14S_FDB_CTX* fdbp,
                            PX14S_DMA_BUF_DESC** descpp);

// Find buffer by user-space virtual address
static int _fdbUserAddr(px14_device* devp, PX14S_FDB_CTX* fdbp,
                        PX14S_DMA_BUF_DESC** descpp);
// Find buffer by file (i.e. device handle)
static int _fdbFile    (px14_device* devp, PX14S_FDB_CTX* fdbp,
                        PX14S_DMA_BUF_DESC** descpp);
// Find all buffers; returns first descriptor in list
static int _fdbAll     (px14_device* devp, PX14S_FDB_CTX* fdbp,
                        PX14S_DMA_BUF_DESC** descpp);
// Find all user-alloc'd buffers; returns first user-alloc'd desc in list
static int _fdbAllUser (px14_device* devp, PX14S_FDB_CTX* fdbp,
                        PX14S_DMA_BUF_DESC** descpp);

// Find buffer by kernel virtual address
static int _fdbKernVirt (px14_device* devp, PX14S_FDB_CTX* fdbp,
                         PX14S_DMA_BUF_DESC** descpp);

//////////////////////////////////////////////////////////////////////////////
// Implementation

ssize_t px14_write (struct file *pFile, const char *pData,
                    size_t len, loff_t *pOffset)
{
   return 0;
}

ssize_t px14_read (struct file *pFile, char *pData, size_t len,
                   loff_t *pOffset)
{
   return 0;
}

int px14_mmap (struct file *filp, struct vm_area_struct *vmap)
{
   u_long kaddr, map_bytes, page_count, i;
   PX14S_DMA_BUF_DESC* descp;
   struct list_head* nodep;
   px14_device *devp;
   void* kern_virt;
   int res;
   unsigned long f;

   devp = (px14_device *)filp->private_data;
   kern_virt = (void*)(vmap->vm_pgoff << PAGE_SHIFT);
   map_bytes = vmap->vm_end - vmap->vm_start;

   // Find descriptor for this buffer
   descp = NULL;
   list_for_each(nodep, &devp->dma_buf_list) {
      descp = list_entry(nodep, PX14S_DMA_BUF_DESC, list);
      if (kern_virt == descp->pBufKern)
         break;
   }
   if (!descp || (descp->pBufKern != kern_virt))
      return -EINVAL;
   if (map_bytes > descp->buffer_bytes)
      return -EIO;
   if (descp->userAddr != 0)
      return -EINVAL;	// Already mapped!

   // Mark all pages as reserved
   page_count = map_bytes >> PAGE_SHIFT;
   if (map_bytes & (PAGE_SIZE - 1))
      page_count++;
   kaddr = (u_long)descp->pBufKern;
   for (i=0; i<page_count; i++,kaddr+=PAGE_SIZE)
      SetPageReserved(virt_to_page(kaddr));
   set_bit(bit_nr_of(PX14DBDF_PAGES_RESERVED), (void*)&descp->flags);

   res = remap_pfn_range(vmap, vmap->vm_start,
                         virt_to_phys(kern_virt) >> PAGE_SHIFT,
                         map_bytes, vmap->vm_page_prot);
   if (res)
      return res;

   PX14_LOCK_DEVICE(devp, f);
   {
      // Need to store user's address so we can validate DMA transfer
      //  requests later.
      descp->userAddr = (u_long)vmap->vm_start;
   }
   PX14_UNLOCK_DEVICE(devp, f);

   return 0;
}

/** @brief Looks up the kernel address for a given user address.

  This routine takes the given user-space address and attempts to find it's
  equivalent kernel logical address (which may then easily be mapped to a
  bus address for DMA operations).

  @param devp
  Pointer to the state structure for the target device.
  @param userAddr
  The user-space address to look up.
  @param asp
  A pointer to the address space for which the user-space address exists.
  If this value is NULL, the first user-space address match will be used.
  @param pAddrKern
  A pointer to the variable that will receive the equivalent kernel
  address for the given user-space address. This value may be NULL if the
  address is not needed.
  @param bAllowOffset
  If this value is non-zero then the given user-address may occur
  anywhere within a valid DMA buffer. If this value is zero, the given
  user-address must equate to the beginning of a DMA buffer.
  @param pBytesRemaining
  A pointer to a variable that will receive the number of bytes
  remaining in the DMA buffer after the user-space address. This value
  may be NULL if this value is not needed.
  @param descpp
  A pointer to a struct p8a_dma_buf_desc pointer that will receive the
  address of the kernel allocated DMA buffer descriptor for the buffer
  mapped to the given user-space address. This value may be NULL if the
  address is not needed.

  @retval 0
  Operation was successful.
  @retval -EFAULT
  Given user-space address does not map to an allocated DMA buffer.

  @note Device semaphore should be acquired outside of this function.
  @note This function makes no assumptions about the state of the given
  device's mutex.
  @note This method may be called from the ISR or bottom half.
  */
int lookup_dma_kern_addr (px14_device *devp,
                          u_long userAddr,
                          struct mm_struct *asp,
                          u_long *pAddrKern,
                          int bAllowOffset,
                          u_int *pBytesRemaining,
                          struct dma_buf_desc **descpp)
{
   struct dma_buf_desc *descp;
   struct list_head *nodep;
   u_long endUserAddr;
   u_int offset;

   // For all allocated DMA buffers...
   list_for_each(nodep, &devp->dma_buf_list) {
      descp = list_entry(nodep, struct dma_buf_desc, list);

      // Only consider buffers allocated/mapped within same address space.
      if (asp && (asp != descp->pAddrSpace))
         continue;

      if (userAddr == descp->userAddr) {

         if (NULL != pAddrKern)
            *pAddrKern = (u_long)(descp->pBufKern);

         if (NULL != pBytesRemaining)
            *pBytesRemaining = descp->buffer_bytes;

         if (NULL != descpp)
            *descpp = descp;

         return 0;
      }

      if (bAllowOffset) {
         endUserAddr = descp->userAddr + descp->buffer_bytes;

         if ((userAddr >= descp->userAddr) && (userAddr < endUserAddr)) {
            offset = userAddr - descp->userAddr;

            if (NULL != pAddrKern)
               *pAddrKern = ((u_long)descp->pBufKern + offset);

            if (NULL != pBytesRemaining)
               *pBytesRemaining = descp->buffer_bytes - offset;

            if (NULL != descpp)
               *descpp = descp;

            return 0;
         }
      }
   }

   return -EFAULT;
}

int _fdbUserAddr (px14_device *devp,
                  struct px14_free_dma_buffer *fdbp,
                  struct dma_buf_desc **descpp)
{
   return lookup_dma_kern_addr(devp, fdbp->user_addr, current->mm, NULL, 0,
                               NULL, descpp) ? 1 : 0;
}

int _fdbKernVirt (px14_device* devp,
                  PX14S_FDB_CTX* fdbp,
                  PX14S_DMA_BUF_DESC** descpp)
{
   PX14S_DMA_BUF_DESC* descp;
   struct list_head* nodep;

   list_for_each(nodep, &devp->dma_buf_list) {
      descp = list_entry(nodep, PX14S_DMA_BUF_DESC, list);

      if (fdbp->kern_virt == descp->pBufKern) {
         *descpp = descp;
         return 0;
      }
   }

   return 1;	// Not found.
}

// Find buffer by file (i.e. device handle)
int _fdbFile (px14_device* devp,
              PX14S_FDB_CTX* fdbp,
              PX14S_DMA_BUF_DESC** descpp)
{
   PX14S_DMA_BUF_DESC* descp;
   struct list_head* nodep;

   list_for_each(nodep, &devp->dma_buf_list) {
      descp = list_entry(nodep, PX14S_DMA_BUF_DESC, list);

      if (descp->filp_owner == fdbp->filp) {
         *descpp = descp;
         return 0;
      }
   }

   return 1;	// Not found.
}

// Find all buffers; returns first descriptor in list
int _fdbAll (px14_device* devp,
             PX14S_FDB_CTX* fdbp,
             PX14S_DMA_BUF_DESC** descpp)
{
   struct list_head* nodep;

   list_for_each(nodep, &devp->dma_buf_list) {
      *descpp = list_entry(nodep, PX14S_DMA_BUF_DESC, list);
      return 0;
   }

   return 1;	// Not found.
}

int _fdbAllUser (px14_device* devp,
                 PX14S_FDB_CTX* fdbp,
                 PX14S_DMA_BUF_DESC** descpp)
{
   struct list_head* nodep;

   list_for_each(nodep, &devp->dma_buf_list) {
      *descpp = list_entry(nodep, PX14S_DMA_BUF_DESC, list);
      if ((*descpp)->flags & PX14DBDF_DBT_BUFFER)
         continue;	// Skip the buffer used for driver buffered transfers
      return 0;
   }

   return 1;	// Not found.
}

/**
  @note This function makes no assumptions about the device lock.
  */
void ResetDma_PX14 (px14_device* devp)
{
   WriteDmaReg(devp, PX14DMAREG_DCR_OFFSET, 1);
   WriteDmaReg(devp, PX14DMAREG_DCR_OFFSET, 0);
}

int DoPendingOpAbortWork_PX14 (px14_device* devp, int bTimedOut)
{
   int f, operating_mode;

   VERBOSE_LOG("Starting DoPendingOpAbortWork_PX14...\n");

   if ((devp->DeviceState == PX14STATE_DMA_XFER_FAST) ||
       (devp->DeviceState == PX14STATE_DMA_XFER_BUFFERED))
   {
      // Disable DMA transfer operations in general. This will abort the
      //  current transfer if there's one in progress. We'll re-enable DMA
      //  prior to next transfer.
      ResetDma_PX14(devp);
   }

   // Always put the board back into standby mode if we're cancelling
   //  or this is a PCI-based (i.e. DMA) operation.
   operating_mode = PX14_OP_MODE(devp);
   if (!bTimedOut || PX14_IS_PCI_MODE(operating_mode))
      SetOperatingMode_PX14(devp, PX14MODE_STANDBY);

   devp->DeviceState = PX14STATE_IDLE;
   devp->bOpCancelled = PX14_TRUE;

   // Wake anyone waiting for acquisition/transfer to complete
   complete_all(&devp->comp_acq_or_xfer);

   return 0;
}

int px14ioc_dma_buffer_alloc (struct file* filp, px14_device* devp, u_long arg)
{
   PX14S_DMA_BUF_DESC* buf_newp;
   PX14S_DMA_BUFFER_ALLOC ctx;
   void* kern_bufp;
   unsigned long f;

   // Input is a PX14S_DMA_BUFFER_ALLOC
   if (__copy_from_user(&ctx, (void*)arg, sizeof(PX14S_DMA_BUFFER_ALLOC)))
      return -EFAULT;
   if (ctx.struct_size < sizeof(PX14S_DMA_BUFFER_ALLOC))
      return -SIG_INVALIDARG;

   // Always allocate a whole number of pages
   if (ctx.req_bytes & (PAGE_SIZE - 1))
      ctx.req_bytes = (ctx.req_bytes + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);

   // Actual DMA buffer allocation
   buf_newp = AllocateDmaBufferImp_PX14(devp, &ctx);
   if (NULL == buf_newp)
      return -SIG_DMABUFALLOCFAIL;

   PX14_LOCK_DEVICE(devp, f);
   {
      buf_newp->pAddrSpace = current->mm;
      buf_newp->filp_owner = filp;

      kern_bufp = buf_newp->pBufKern;
   }
   PX14_UNLOCK_DEVICE(devp, f);

   ctx.virt_addr   = (unsigned long long)(u_long)kern_bufp;

   if (__copy_to_user((void*)arg, &ctx, sizeof(PX14S_DMA_BUFFER_ALLOC)))
      return -EFAULT;

   return 0;
}

int px14ioc_dma_buffer_free (px14_device* devp, u_long arg)
{
   struct px14_free_dma_buffer fdb;
   struct list_head lstToFree;
   PX14S_DMA_BUFFER_FREE ctx;
   unsigned len_freed;
   unsigned long f;
   int res;

   // Input is a PX14S_DMA_BUFFER_FREE structure
   res = __copy_from_user(&ctx, (void*)arg, sizeof(PX14S_DMA_BUFFER_FREE));
   if (res != 0)
      return -EFAULT;

   INIT_LIST_HEAD(&lstToFree);

   // Translate PX14S_DMA_BUFFER_FREE into px14_free_dma_buffer
   if (ctx.free_all == PX14_FREE_ALL_DMA_BUFFERS)
      fdb.how = PX14FDB_ALL;
   else if (ctx.free_all == ~PX14_FREE_ALL_DMA_BUFFERS) {
      fdb.how = PX14FDB_KERNVIRT_ADDR;
      fdb.kern_virt = (void*)(u_long)ctx.virt_addr;
   }
   else {
      if (0 == ctx.virt_addr)
         return -SIG_PX14_INVALID_DMA_ADDR;
      fdb.how = PX14FDB_USERSPACE_ADDR;
      fdb.user_addr = (u_long)ctx.virt_addr;
   }

   PX14_LOCK_DEVICE(devp, f)
   {
      res = px14_free_dma_buffer_work (devp, &fdb, &len_freed, &lstToFree);
   }
   PX14_UNLOCK_DEVICE(devp, f);

   // Can't iounmap with spinlock held
   FreeDmaBufferList_PX14(devp, &lstToFree);

   // Output is number of bytes freed
   if (0 == res) {
      res = __copy_to_user ((void*)arg, &len_freed, 4)
         ? -EFAULT : 0;
   }

   return res;
}

PX14S_DMA_BUF_DESC* AllocateDmaBufferImp_PX14(px14_device* devp,
                                              PX14S_DMA_BUFFER_ALLOC* ctxp)
{
   PX14S_DMA_BUF_DESC* buf_newp;
   dma_addr_t bus_addr;
   void* kernel_bufp;
   unsigned long f;

   // Request the actual allocation
   kernel_bufp = dma_alloc_coherent(&devp->pOsDevice->dev, ctxp->req_bytes,
                                    &bus_addr, GFP_KERNEL);
   if (NULL == kernel_bufp)
      return NULL;

   // Allocate and init buffer descriptor
   buf_newp = kmalloc(sizeof(PX14S_DMA_BUF_DESC), GFP_KERNEL);
   if (NULL == buf_newp){
      dma_free_coherent(&devp->pOsDevice->dev, ctxp->req_bytes,
                        kernel_bufp, bus_addr);
      return NULL;
   }
   memset (buf_newp, 0, sizeof(PX14S_DMA_BUF_DESC));
   INIT_LIST_HEAD(&buf_newp->list);
   buf_newp->buffer_bytes = ctxp->req_bytes;
   buf_newp->physAddr = bus_addr;
   buf_newp->pBufKern = kernel_bufp;
   buf_newp->flags = PX14DBDF__INIT;

   // Add descriptor to device's list and allocate a buffer ID
   PX14_LOCK_DEVICE(devp, f);
   {
      list_add (&buf_newp->list, &devp->dma_buf_list);
   }
   PX14_UNLOCK_DEVICE(devp, f);

#ifdef PX14PP_VERBOSE_DMA_BUFFER_EVENTS
   VERBOSE_LOG("DMA buffer alloc: %u bytes\n", ctxp->req_bytes);
   VERBOSE_LOG("        KernAddr:  %p\n", kernel_bufp);
   VERBOSE_LOG("         BusAddr:  %p\n",(void*)(u_long)bus_addr);
#endif

   return buf_newp;

}

void FreeDmaBufferList_PX14 (px14_device* devp, struct list_head* lstToFree)
{
   PX14S_DMA_BUF_DESC* descp;
   struct list_head *nodep, *nextp;

   list_for_each_safe(nodep, nextp, lstToFree) {
      descp = list_entry(nodep, PX14S_DMA_BUF_DESC, list);

#ifdef PX14PP_VERBOSE_DMA_BUFFER_EVENTS
      VERBOSE_LOG("DMA buffer free:  %u bytes\n", descp->buffer_bytes);
      VERBOSE_LOG("       KernAddr:  %p\n", descp->pBufKern);
      VERBOSE_LOG("        BusAddr:  %p\n",(void*)(u_long)descp->physAddr);
      VERBOSE_LOG("  User VirtAddr:  %p\n", (void*)descp->userAddr);
#endif

      if (descp->flags & PX14DBDF_PAGES_RESERVED) {
         // We've marked the pages of this buffer as reserved so we could
         //  map them into user space. Remove reserved status on all pages

         u_long i, kaddr, page_count;
         kaddr = (u_long)descp->pBufKern;
         page_count = descp->buffer_bytes >> PAGE_SHIFT;
         if (descp->buffer_bytes & (PAGE_SIZE - 1))
            page_count++;
         for (i=0; i<page_count; i++,kaddr+=PAGE_SIZE)
            ClearPageReserved(virt_to_page(kaddr));
      }

      dma_free_coherent(&devp->pOsDevice->dev, descp->buffer_bytes,
                        descp->pBufKern, descp->physAddr);

      list_del(nodep);
      kfree(descp);
   }
}

int px14_free_dma_buffer_work (px14_device* devp,
                               struct px14_free_dma_buffer* fdbp,
                               unsigned* lenp,
                               struct list_head* lstToFree)
{
   int res, bSingleton;
   struct dma_buf_desc* descp;
   pfdbFindFunc pFindFunc;

   bSingleton = 0;

   // Determine the method in which we will use to determine which
   //  buffer(s) to free.
   switch(fdbp->how) {
      case PX14FDB_USERSPACE_ADDR: pFindFunc = _fdbUserAddr; bSingleton = 1; break;
      case PX14FDB_FILE:           pFindFunc = _fdbFile;     bSingleton = 0; break;
      case PX14FDB_ALL:            pFindFunc = _fdbAll;      bSingleton = 0; break;
      case PX14FDB_ALL_USER:       pFindFunc = _fdbAllUser;  bSingleton = 0; break;
      case PX14FDB_KERNVIRT_ADDR:  pFindFunc = _fdbKernVirt; bSingleton = 1; break;
      default:                     return -EINVAL;
   }

   if (NULL == pFindFunc)
      return -EINVAL;

   do {

      res = (*pFindFunc)(devp, fdbp, &descp);
      if (0 == res) {

         // Swap lists
         list_del_init(&descp->list);
         list_add_tail(&descp->list, lstToFree);
      }

   } while ((0 == res) && !bSingleton);


   return 0;
}

void BeginDmaTransfer_PX14 (px14_device* devp,
                            dma_addr_t pa,
                            u_int sample_count,
                            int bXferToDevice)
{
   u_int dwBytes, tlp_size_reg, tlp_cnt_reg;

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,13,0)
   reinit_completion (&devp->comp_acq_or_xfer);
#else
   init_completion (&devp->comp_acq_or_xfer);
#endif
   devp->bOpCancelled = 0;

   dwBytes = sample_count * PX14_SAMPLE_SIZE_IN_BYTES;

   atomic_inc(&devp->stat_dma_start);
   devp->dmaBytes = dwBytes;
   devp->dmaAddr = pa;
   devp->dmaDir = bXferToDevice ? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE;

   // Define transfer size and transfer address size
   tlp_size_reg = (PX14_DMA_TLP_BYTES / 4) & 0x1FFF;
   tlp_cnt_reg = dwBytes / PX14_DMA_TLP_BYTES;

   // Reset initiator
   ResetDma_PX14(devp);

   if (bXferToDevice) {
      // Host -> Device (a READ from the device's POV)

      // Setup transfer size
      WriteDmaReg(devp, PX14DMAREG_READ_SIZE_OFFSET, tlp_size_reg);
      WriteDmaReg(devp, PX14DMAREG_READ_COUNT_OFFSET, tlp_cnt_reg);

      // Setup DMA address
      WriteDmaReg(devp, PX14DMAREG_READ_ADDR_OFFSET, (u32)pa);

      wmb();

      // Start the DMA transfer
      WriteDmaReg(devp, PX14DMAREG_DCSR_OFFSET, 0x00010000);
   }
   else {
      // Device -> Host (a WRITE from the device's POV)

      // Setup transfer size
      WriteDmaReg(devp, PX14DMAREG_WRITE_SIZE_OFFSET, tlp_size_reg);
      WriteDmaReg(devp, PX14DMAREG_WRITE_COUNT_OFFSET, tlp_cnt_reg);

      // Setup DMA address
      WriteDmaReg(devp, PX14DMAREG_WRITE_ADDR_OFFSET, (u32)pa);

      wmb();

      // Start the DMA transfer
      WriteDmaReg(devp, PX14DMAREG_DCSR_OFFSET, 0x00000001);
   }
}

int PreDmaXferCheck (px14_device* devp, PX14S_DMA_XFER* ctxp)
{
   int opMode;

   // Can only start a DMA transfer when we (the driver) are idle
   if (devp->DeviceState != PX14STATE_IDLE)
      return -SIG_PX14_BUSY;

   opMode = PX14_OP_MODE(devp);
   if (ctxp->bRead) {
      // PX14400 -> PC
      if ((opMode != PX14MODE_ACQ_PCI_BUF) &&
          (opMode != PX14MODE_RAM_READ_PCI) &&
          (opMode != PX14MODE_ACQ_PCI_SMALL_FIFO)) {
         return -SIG_INVALID_MODE;
      }
   }
   else {
      // PC -> PX14400
      if (opMode != PX14MODE_RAM_WRITE_PCI)
         return -SIG_INVALID_MODE;
   }

   return 0;
}

int GetDmaOpBusAddr (px14_device* devp, PX14S_DMA_XFER* ctxp, dma_addr_t* p)
{
   PX14S_DMA_BUF_DESC* buf_descp;
   u_int bytes_left;
   u_long uaddr;
   int res;

   // Get bus address for user's virtual-space address. It must be a
   //  pre-allocated DMA buffer
   uaddr = (u_long)ctxp->virt_addr;
   res = lookup_dma_kern_addr(devp, uaddr, current->mm,
                              NULL, 1, &bytes_left, &buf_descp);
   if (res)
      return res;
   if (bytes_left < ctxp->xfer_bytes)
      return -SIG_PX14_BUFFER_TOO_SMALL;

   *p = buf_descp->physAddr + (uaddr - buf_descp->userAddr);
   return 0;
}

int px14ioc_dma_xfer (struct file* filp, px14_device* devp, u_long arg)
{

   dma_addr_t busAddr;
   PX14S_DMA_XFER ctx;
   u_int xfer_samples;
   unsigned long f;
   int res;

   // Input is a PX14S_DMA_XFER structure
   if (__copy_from_user(&ctx, (void*)arg, sizeof(PX14S_DMA_XFER)))
      return -EFAULT;
   if (ctx.struct_size < sizeof(PX14S_DMA_XFER))
      return -SIG_INVALIDARG;

   PX14_LOCK_MUTEX(devp)
   {
      PX14_LOCK_DEVICE(devp, f)
      {
         // Validate device state and operating mode
         res = PreDmaXferCheck(devp, &ctx);
         if (!res) {
            // Validate user-space address
            res = GetDmaOpBusAddr(devp, &ctx, &busAddr);
            if (!res) {
               // Start the DMA transfer
               xfer_samples = ctx.xfer_bytes / PX14_SAMPLE_SIZE_IN_BYTES;
               devp->DeviceState = PX14STATE_DMA_XFER_FAST;
               devp->dma_filp = filp;
               BeginDmaTransfer_PX14(devp, busAddr,xfer_samples, !ctx.bRead);
            }
         }
      }
      PX14_UNLOCK_DEVICE(devp, f);
   }
   PX14_UNLOCK_MUTEX(devp);

   // Wait for transfer to complete for synchronous DMA transfers
   if ((0 == res) && !ctx.bAsynch)
      res = WaitForDmaTransferToComplete_PX14(devp);

   return res;
}

int AllocateDriverDmaBuffer_PX14 (px14_device* devp)
{
   PX14S_DMA_BUFFER_ALLOC ctx;
   u_int buf_samps;

   ctx.struct_size = sizeof(PX14S_DMA_BUFFER_ALLOC);
   ctx.virt_addr  = 0;

   VERBOSE_LOG("Allocating buffer for driver buffered transfers...\n");

   for (buf_samps=PX14_DB_XFER_MAX; buf_samps>=PX14_DB_XFER_MIN; buf_samps>>=1) {
      ctx.req_bytes = buf_samps * PX14_SAMPLE_SIZE_IN_BYTES;
      devp->db_dma_descp = AllocateDmaBufferImp_PX14(devp, &ctx);
      if (NULL != devp->db_dma_descp)
         break;
   }

   if (NULL == devp->db_dma_descp)
      return -SIG_DMABUFALLOCFAIL;

   devp->db_dma_descp->flags |= PX14DBDF_DBT_BUFFER;

   return 0;
}

int WaitForDmaTransferToComplete_PX14 (px14_device* devp)
{
   int res, bCheckFifo;
   unsigned long f;

   bCheckFifo = 0;

   // Wait for DMA to complete
   res = wait_for_completion_interruptible(&devp->comp_acq_or_xfer);
   if (res != 0) {
      // Received a signal
      PX14_LOCK_DEVICE(devp, f)
      {
         // Abort DMA transfer
         DoPendingOpAbortWork_PX14(devp, PX14_FALSE);
      }
      PX14_UNLOCK_DEVICE(devp, f);
      res = -SIG_CANCELLED;
   }
   else {
      PX14_LOCK_DEVICE(devp,f)
      {
         if (devp->bOpCancelled)
            res = -SIG_CANCELLED;

         // Check PCI FIFO full flag if we're in PCI acq mode
         bCheckFifo =
            (PX14MODE_ACQ_PCI_BUF == PX14_OP_MODE(devp));
      }
      PX14_UNLOCK_DEVICE(devp,f);
   }

   if (bCheckFifo && !res)
      res = CheckPciFifo_PX14(devp);

   return res;
}
