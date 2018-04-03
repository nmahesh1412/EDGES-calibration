/** @file	px14_acquire.cpp
  @brief	PX14 data acquisition routines
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_virtual.h"

// PX14 library exports implementation --------------------------------- //

/** @brief Acquire data to PX14 RAM

  This function will perform a synchronous RAM acquisition using current
  data acquisition settings to the specified region of PX14 onboard RAM.

  This function will not return until the acquisition has completed
  or the optional timeout has elapsed. The invoking thread will sleep
  while the acquisition takes place. The Samples Complete interrupt will
  be used for end of acquisition notification.

  An acquisition may also be cancelled by putting the board into Standby
  operating mode from a secondary thread or process. A separate thread is
  required because the thread that invoked AcquireToBoardRamPX14 will be
  suspended waiting for the acquisition to finish (or timeout).

  This function can also start an asynchronous acquisition. By default,
  this function will not return until the data acquisition completes. If
  the bAsynchronous parameter is nonzero, then the function will start the
  data acquisition and return immediately without waiting for it to finish
  allowing the calling thread to continue working. The caller can use the
  IsAcquisitionInProgressPX14 function to determine if the acquisition is
  still in progress. To wait (sleep) for the acquisition to complete, call
  the WaitForAcquisitionCompletePX14 function.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param samp_start
  The PX14 RAM sample at which to put the first acquired sample
  @param samp_count
  The total number of samples to acquire. This is independent of
  active channel count and segment size.
  @param timeout_ms
  An optional timeout value in milliseconds. If this value is 0
  then the function will not timeout. This parameter is ignored if
  parameter bAsynchronous is nonzero.
  @param bAsynchronous
  If this parameter is nonzero then an asynchronous data transfer will
  be performed; see remarks
  */
PX14API AcquireToBoardRamPX14 (HPX14 hBrd,
                               unsigned int samp_start,
                               unsigned int samp_count,
                               unsigned int timeout_ms,
                               int bAsynchronous)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!InIdleModePX14(hBrd))
      return SIG_INVALID_MODE;

   // Make sure we're in standby mode
   res = SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   PX14_RETURN_ON_FAIL(res);

   // Setup active memory region
   res = SetStartSamplePX14(hBrd, samp_start);
   PX14_RETURN_ON_FAIL(res);
   res = SetSampleCountPX14(hBrd, samp_count);
   PX14_RETURN_ON_FAIL(res);

   // Arm the board for a RAM acquisition
   res = SetOperatingModePX14(hBrd, PX14MODE_ACQ_RAM);
   PX14_RETURN_ON_FAIL(res);

   // If this is a synchronous request, wait for acquisition to complete
   if (!bAsynchronous)
   {
      res = WaitForAcquisitionCompletePX14(hBrd, timeout_ms);
      SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   }

   return res;
}

/** @brief Synchronously acquire data to Signatec Auxiliary Bus (SAB)

  This function will perform a synchronous SAB acquisition using current
  data acquisition settings to the specified region of PX14 onboard RAM.

  This function will not return until the acquisition has completed
  or the optional timeout has elapsed. The invoking thread will sleep
  while the acquisition takes place. The Samples Complete interrupt will
  be used for end of acquisition notification.

  An acquisition may also be cancelled by putting the board into Standby
  operating mode from a secondary thread or process. A separate thread is
  required because the thread that invoked AcquireToSabPX14 will be
  suspended waiting for the acquisition to finish (or timeout).

  It is up to the caller to ensure that the SAB receiver is setup and
  ready to accept data.

  This function can also start an asynchronous acquisition. By default,
  this function will not return until the data acquisition completes. If
  the bAsynchronous parameter is nonzero, then the function will start the
  data acquisition and return immediately without waiting for it to finish
  allowing the calling thread to continue working. The caller can use the
  IsAcquisitionInProgressPX14 function to determine if the acquisition is
  still in progress. To wait (sleep) for the acquisition to complete, call
  the WaitForAcquisitionCompletePX14 function.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param samp_count
  The total number of samples to acquire. This is independent of
  active channel count and segment size. If this parameter is
  PX14_FREE_RUN, then a free-run (infinite) SAB acquisition will
  take place. In this case, the timeout is ignored and the function
  will return immediately after placing the board into acquisition
  mode. To stop a free-run acquisition place the board in Standby
  mode.
  @param bRamBuffered
  If this parameter is nonzero then the RAM-buffered SAB acquisition
  mode (PX14MODE_ACQ_SAB_BUF) will be used. If this parameter is zero
  then the normal SAB acquisition mode (PX14MODE_ACQ_SAB) will be used
  @param timeout_ms
  An optional timeout value in milliseconds. If this value is 0
  then the function will not timeout
  @param bAsynchronous
  If this parameter is nonzero then an asynchronous data transfer will
  be performed; see remarks
  */
PX14API AcquireToSabPX14 (HPX14 hBrd,
                          unsigned int samp_count,
                          int bRamBuffered,
                          unsigned int timeout_ms,
                          int bAsynchronous)
{
   CStatePX14* statep;
   int res, op_mode;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!InIdleModePX14(hBrd))
      return SIG_INVALID_MODE;

   // Make sure we're in standby mode
   res = SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   PX14_RETURN_ON_FAIL(res);

   // Free-run acquisitions are always asynchronous
   if (PX14_FREE_RUN == samp_count)
      bAsynchronous = 1;

   // Setup active memory region
   res = SetSampleCountPX14(hBrd, samp_count);
   PX14_RETURN_ON_FAIL(res);

   // Arm the board for acquisition
   op_mode = bRamBuffered ? PX14MODE_ACQ_SAB_BUF : PX14MODE_ACQ_SAB;
   res = SetOperatingModePX14(hBrd, op_mode);
   PX14_RETURN_ON_FAIL(res);

   // If this is a synchronous request, wait for acquisition to complete
   if (!bAsynchronous)
   {
      res = WaitForAcquisitionCompletePX14(hBrd, timeout_ms);
      SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   }

   return res;
}

/** @brief Begin a buffered PCI acquisition
*/
PX14API BeginBufferedPciAcquisitionPX14 (HPX14 hBrd,
                                         unsigned int samp_count)
{
   CStatePX14* statep;
   int res;

   if (samp_count != PX14_FREE_RUN)
   {
      if (samp_count < PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES)
         return SIG_PX14_XFER_SIZE_TOO_SMALL;
      if (samp_count > PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES)
         return SIG_PX14_XFER_SIZE_TOO_LARGE;
   }
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // Make sure we're in standby mode
   res = SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   PX14_RETURN_ON_FAIL(res);

   // Setup active memory region
   res = SetStartSamplePX14(hBrd, 0);
   PX14_RETURN_ON_FAIL(res);
   res = SetSampleCountPX14(hBrd, samp_count);
   PX14_RETURN_ON_FAIL(res);

   // Enter the acquisition mode
   return SetOperatingModePX14(hBrd, PX14MODE_ACQ_PCI_BUF);
}

/** @brief Obtain fresh PCI acquisition data
*/
PX14API GetPciAcquisitionDataFastPX14 (HPX14 hBrd,
                                       unsigned int samples,
                                       px14_sample_t* dma_bufp,
                                       int bAsynchronous)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   //if (samples % PX14_ALIGN_SAMPLE_COUNT_BURST)
   //	return SIG_PX14_ALIGNMENT_ERROR; ??

   // Make sure we're in a PCI acquisition mode
   res = GetOperatingModePX14(hBrd, PX14_GET_FROM_CACHE);
   if (res < 0)
      return res;
   if ((res != PX14MODE_ACQ_PCI_BUF) && (res != PX14MODE_ACQ_PCI_SMALL_FIFO))
      return SIG_INVALID_MODE;

   if (statep->IsLocalVirtual())
      return GenerateVirtualDataPX14(hBrd, dma_bufp, samples, UINT_MAX);

   // Do the DMA transfer
   res = DmaTransferPX14(hBrd, dma_bufp,
                         samples * PX14_SAMPLE_SIZE_IN_BYTES, PX14_TRUE, bAsynchronous != 0);
   PX14_RETURN_ON_FAIL(res);

   return res;
}

PX14API GetPciAcquisitionDataBufPX14 (HPX14 hBrd,
                                      unsigned int samples,
                                      px14_sample_t* heap_bufp,
                                      int bAsynchronous)
{
   static const unsigned sample_align = 4096; // Assuming power of 2 below

   unsigned int max_read_samples, sample_chunk;
   int res;

   // Make sure we're in a PCI acq mode; this also validates handle
   res = GetOperatingModePX14(hBrd, PX14_GET_FROM_CACHE);
   if (res < 0)		// SIG_* error
      return res;
   //if ((res != PX14MODE_ACQ_PCI_BUF) && (res != PX14MODE_ACQ_PCI_SMALL_FIFO))
   if (res != PX14MODE_ACQ_PCI_BUF)
      return SIG_INVALID_MODE;
   if (samples & (sample_align - 1))
      return SIG_PX14_INVALID_ARG_2;
   if (NULL == heap_bufp)
      return SIG_PX14_INVALID_ARG_3;

   // Maximum read size (Windows plaform restriction; no biggie though)
   max_read_samples = PX14_MAX_DEVICE_READ_BYTES / sizeof(px14_sample_t);
   // Align to DMA transfer
   max_read_samples -= (max_read_samples & (sample_align-1));

   if (bAsynchronous && (samples > max_read_samples))
      return SIG_PX14_XFER_SIZE_TOO_LARGE;

   if (PX14_H2B(hBrd)->IsLocalVirtual())
      return GenerateVirtualDataPX14(hBrd, heap_bufp, samples, UINT_MAX);

   while (samples)
   {
      sample_chunk = PX14_MIN(samples, max_read_samples);

      PX14S_DRIVER_BUFFERED_XFER req;
      req.struct_size = sizeof(PX14S_DRIVER_BUFFERED_XFER);
      req.flags = PX14DBTF__MODE_MASK;
      if (bAsynchronous)
         req.flags |= PX14DBTF_ASYNCHRONOUS;
      req.sample_start = 0;
      req.sample_count = sample_chunk;
      req.user_virt_addr = (unsigned long long)(uintptr_t)heap_bufp;
      req.user_virt_addr2 = 0;

      res = DeviceRequest(hBrd,
                          IOCTL_PX14_DRIVER_BUFFERED_XFER, &req, req.struct_size);
      PX14_RETURN_ON_FAIL(res);

      if ((samples -= sample_chunk) > 0)
         heap_bufp += sample_chunk;
   }

   return res;
}

/** @brief End a buffered PCI acquisition
*/
PX14API EndBufferedPciAcquisitionPX14 (HPX14 hBrd)
{
	// Begin PATCH for problem with SW trigger
	//
	// If a Sw trigger has been sent before then the board will be in free triggered forever, unless we switch to RAM acquistion mode
	// then switch back to STANDBY
	// Switch to PX14MODE_ACQ_RAM then PX14MODE_STANDBY to removed the effect of SW trigger
   int res;
   res = SetOperatingModePX14(hBrd, PX14MODE_ACQ_RAM);
   PX14_RETURN_ON_FAIL(res);
   SysSleep(500);
	// End of PATCH for problem with SW trigger

   return SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
}

/** @brief Determine if RAM/SAB acquisition is currently in progress

  @note
  This function will always return 0 for virtual devices

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @retval
  Returns 1 if an acquisition is currently in progress, 0 if an
  acquisition is not in progress, or a (negative) SIG_* error code on
  error
  */
PX14API IsAcquisitionInProgressPX14 (HPX14 hBrd)
{
   int res, dev_state;
   CStatePX14* statep;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsVirtual())
      dev_state = PX14STATE_IDLE;
   else
   {
      // Ask driver if transfer is in progress
      res = DeviceRequest(hBrd, IOCTL_PX14_GET_DEVICE_STATE,
                          &dev_state, 0, sizeof(int));
      PX14_RETURN_ON_FAIL(res);
   }

   return (dev_state == PX14STATE_ACQ) ? 1 : 0;
}

/** @brief  Wait for a RAM/SAB data acquisition to complete with optional timeout

  Calling this function will put the calling thread to sleep until one
  of the following occur:
  - Acquisition completes (function returns SIG_SUCCESS)
  - Specified timeout elapses (function returns SIG_PX14_TIMED_OUT)
  - Acquisition is cancelled by entering Standby mode (function returns SIG_CANCELLED)
  */
PX14API WaitForAcquisitionCompletePX14 (HPX14 hBrd, unsigned int timeout_ms)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   PX14S_WAIT_OP ctx;
   ctx.struct_size = sizeof(PX14S_WAIT_OP);
   ctx.wait_op = PX14STATE_ACQ;
   ctx.timeout_ms = timeout_ms;
   ctx.dummy = 0;

   // Wait for acquisition to complete
   res = DeviceRequest(hBrd, IOCTL_PX14_WAIT_ACQ_OR_XFER,
                       &ctx, sizeof(PX14S_WAIT_OP), 0);

   if ((SIG_CANCELLED == res) || (SIG_SUCCESS == res))
   {
      // Acquisition was cancelled. The driver has put the card into
      //  Standby mode already. Do an explicit call here to ensure that
      //  our internal cached state coherant.
      if (PX14MODE_STANDBY != GetOperatingModePX14(hBrd))
         SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   }

   return res;
}

