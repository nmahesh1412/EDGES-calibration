/** @file px14_isr
  @brief PX14400 interrupt handling
  */
#include "px14_drv.h"

// PX14400 interrupt handler bottom half; services all PX14400 devices
static void _px14_bh_tasklet (u_long unused);
// Does actual bottom-half processing for a specific PX14400 device
static void _do_px14_device_bh (px14_device* devp);

// Declare our BH tasklet.
DECLARE_TASKLET(px14_bh, _px14_bh_tasklet, 0);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
irqreturn_t px14_isr (int irq, void *dev_id, struct pt_regs *regsp)
#else
irqreturn_t px14_isr (int irq, void *dev_id)
#endif
{
   int bOurIntDma, bOurIntSampComp, bWantDpc;
   px14_device* devp;
   u_int int_status;

   bOurIntDma = bOurIntSampComp = bWantDpc = 0;

   devp = (px14_device*)dev_id;
   if (unlikely(NULL == devp))
      return IRQ_NONE;

   atomic_inc (&devp->stat_isr_count);

   // Read interrupt status.
   int_status = ReadDmaReg(devp, PX14DMAREG_DCSR_OFFSET);

   // Has a DMA (read or write) completed?
   if (int_status & 0x01000100) {

      atomic_inc (&devp->stat_dma_comp);
      devp->stat_dma_bytes += devp->dmaBytes;
      devp->dma_filp = NULL;

      bOurIntDma = 1;
      bWantDpc = 1;
   }

   // Has an acquisition completed?
   if (int_status & 0x00000200) {

      if (PX14_IS_ACQ_MODE(PX14_OP_MODE(devp)))
         atomic_inc(&devp->stat_acq_comp);

      // Remember that we received samples complete interrupt. User
      //  may be wanting to wait on this in a short while
      devp->bSampComp = 1;

      // We only care about this interrupt if we're waiting for it
      if ((devp->DeviceState == PX14STATE_ACQ) ||
          (devp->DeviceState == PX14STATE_WRAM)) {

         bWantDpc = 1;
      }

      bOurIntSampComp = 1;
   }

   if (bOurIntDma || bOurIntSampComp) {
      if (devp->fwVerPci && (devp->fwVerPci<PX14FWV_PCI_MIN_SPLIT_INT_CLEAR)){
         // Clear interrupt the old way; this way clears both interrupts
         WriteDmaReg(devp, PX14DMAREG_CLEAR_INTERRUPT, 1);
      }
      else {
         if (bOurIntDma)
            WriteDmaReg(devp, PX14DMAREG_CLEAR_INTERRUPT, 1);

         if (bOurIntSampComp)
            WriteDmaReg(devp, PX14DMAREG_CLEAR_SAMP_COMP_INTERRUPT, 1);
      }
   }

   if (bWantDpc) {
      // We'll finish up processing in the bottom half

      // Mark this device as interrupting so the bottom half can handle it.
      // Note that set_bit is an atomic operation so we don't need to worry
      //  about synchronizing with a possibly currently preempted running
      //  bottom half.
      set_bit(devp->brdNum, (void*)&g_pModPX14->bh_bmp);

      // Schedule our bottom half
      tasklet_schedule(&px14_bh);
   }

   return (bOurIntDma || bOurIntSampComp) ? IRQ_HANDLED : IRQ_NONE;
}

/** @brief PX14400 interrupt handler bottom half; services all PX14400 devices

  This function is called as a result of a request by a PX14400 device
  interrupt handler. This function runs in an interrupt context so it must
  behave as such (no sleeping, scheduling, etc...).

  Unlike most other functions in the module, this function isn't explicitly
  coupled to a specific device. It is possible that multiple PX14400 devices
  have interrupted (multiple times) before this function has been called.
  */
void _px14_bh_tasklet (u_long unused)
{
   int curDev;

   UNUSED_PARAM(unused);

   // While there are devices to manage...
   while (g_pModPX14->bh_bmp) {

      // For all possible PMP1000 devices...
      for (curDev=0; curDev<g_pModPX14->nDevices; curDev++) {

         // Did the current device interrupt at some point?
         // Note that the test and clear is an atomic operation so we don't
         //  need to worry about synchronization with the interrupt handler.
         if (test_and_clear_bit(curDev, (void*)&g_pModPX14->bh_bmp)) {

            // Do the actual event handling for the device.
            _do_px14_device_bh (g_pModPX14->pDev + curDev);
         }
      }
   }
}

/// Does actual bottom-half processing for a specific PX14400 device
void _do_px14_device_bh (px14_device* devp)
{
   int bWakeAcqOrDmaWaiters, next_device_state;
   unsigned long f;

   next_device_state = PX14STATE_IDLE;
   bWakeAcqOrDmaWaiters = 0;

   PX14_LOCK_DEVICE(devp,f)
   {
      switch (devp->DeviceState) {
         case PX14STATE_DMA_XFER_FAST:
            bWakeAcqOrDmaWaiters = PX14_TRUE;

            // Unmap the DMA registers if necessary.
            if (devp->dwFlags & DEVF_DO_DMA_UNMAP) {
               pci_unmap_single (devp->pOsDevice, devp->dmaAddr, devp->dmaBytes,
                                 devp->dmaDir);

               devp->dwFlags &= ~DEVF_DO_DMA_UNMAP;
            }

            break;

         case PX14STATE_DMA_XFER_BUFFERED:
            next_device_state = PX14STATE_DMA_XFER_BUFFERED; // Still working
            bWakeAcqOrDmaWaiters = PX14_TRUE;
            break;

         case PX14STATE_ACQ:
         case PX14STATE_WRAM:
            SetOperatingMode_PX14(devp, PX14MODE_STANDBY);
            bWakeAcqOrDmaWaiters = PX14_TRUE;
            break;

         default:
            break;
      }

      devp->DeviceState = next_device_state;
   }
   PX14_UNLOCK_DEVICE(devp,f);

   if (bWakeAcqOrDmaWaiters) {
      // Wake anyone waiting for acquisition/transfer to complete
      complete_all(&devp->comp_acq_or_xfer);
   }
}
