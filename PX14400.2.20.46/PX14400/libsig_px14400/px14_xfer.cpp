/** @file	px14_xfer.cpp
  @brief	PX14 data transfer routines
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_virtual.h"
#include "px14_remote.h"

// Prototype removed from main header until it's working correctly in firmware
// Transfer data in a DMA buffer on the host PC to PX14400 RAM
PX14API WriteSampleRamFastPX14 (HPX14 hBrd,
                                unsigned int sample_start,
                                unsigned int sample_count,
                                const px14_sample_t* dma_bufp,
                                int bAsynchronous _PX14_DEF(0),
                                int ram_bank _PX14_DEF(0));


// PX14 library exports implementation --------------------------------- //

/** @brief Transfer data in PX14 RAM to a buffer on the host PC

  This function copies a section of PX14 signal RAM to a buffer on the
  host PC using a Direct Memory Access (DMA) transfer.

  This function can also start an asynchronous transfer. By default, this
  function will not return until the data transfer completes. If the
  bAsynchronous parameter is nonzero, then the function will start the
  data transfer and return immediately without waiting for it to finish
  allowing the calling thread to continue working. The caller can use the
  IsTransferInProgressPX14 function to determine if the transfer is still
  in progress. To wait (sleep) for the transfer to complete, call the
  WaitForTransferCompletePX14 function.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param sample_start
  The index of the first sample to copy. This parameter has the same
  restrictions defined in the SetStartSamplePX14 function.
  @param sample_count
  The total number of samples to copy. This parameter has the same
  restrictions defined in the SetSampleCountPX14 function and must be
  an integer multiple of 4096 samples.
  @param dma_bufp
  The address of the host PC buffer at which the sample data will be
  copied to. This buffer must be large enough to hold sample_count
  samples and must have been allocated for this device by the
  AllocateDmaBufferPX14 function.
  @param bAsynchronous
  If this parameter is nonzero then an asynchronous data transfer will
  be performed. See remarks

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.

  @see AllocateDmaBufferPX14
  */
PX14API ReadSampleRamFastPX14 (HPX14 hBrd,
                               unsigned int sample_start,
                               unsigned int sample_count,
                               px14_sample_t* dma_bufp,
                               int bAsynchronous)
{
   unsigned int total_samples;
   CStatePX14* statep;
   int res;

   if (sample_start >= PX14_RAM_SIZE_IN_SAMPLES)
      return SIG_PX14_INVALID_ARG_2;
   if (sample_start + sample_count > PX14_RAM_SIZE_IN_SAMPLES)
      return SIG_PX14_INVALID_ARG_3;
   if (NULL == dma_bufp)
      return SIG_PX14_INVALID_ARG_4;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (0 == sample_count)
      return SIG_SUCCESS;
   if (sample_count < PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES)
      return SIG_PX14_XFER_SIZE_TOO_SMALL;
   if (sample_count > PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES)
      return SIG_PX14_XFER_SIZE_TOO_LARGE;
   if (sample_count % PX14_ALIGN_SAMPLE_COUNT_BURST)
      return SIG_PX14_ALIGNMENT_ERROR;

   // Define active memory region
   res = SetStartSamplePX14(hBrd, sample_start);
   PX14_RETURN_ON_FAIL(res);
   total_samples = PX14_RAM_SIZE_IN_SAMPLES - (4 * PX14_ALIGN_SAMPLE_COUNT_BURST);
   if (total_samples < sample_count)
      total_samples = sample_count;
   res = SetSampleCountPX14(hBrd, total_samples);
   PX14_RETURN_ON_FAIL(res);

   // Enter the PCI read mode
   res = SetOperatingModePX14(hBrd, PX14MODE_RAM_READ_PCI);
   PX14_RETURN_ON_FAIL(res);

   // Do the DMA transfer
   res	= DmaTransferPX14(hBrd, dma_bufp,
                           sample_count * PX14_SAMPLE_SIZE_IN_BYTES, PX14_TRUE,
                           bAsynchronous != 0);
   if (SIG_SUCCESS != res)
   {
      SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
      return res;
   }

   if (!bAsynchronous)
   {
      // Return back to standby mode
      SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   }

   return res;
}

PX14API WriteSampleRamFastPX14 (HPX14 hBrd,
                                unsigned int sample_start,
                                unsigned int sample_count,
                                const px14_sample_t* dma_bufp,
                                int bAsynchronous,
                                int ram_bank)
{
   //CStatePX14* statep;
   //int res;

   //if (sample_start >= PX14_RAM_SIZE_IN_SAMPLES)
   //	return SIG_PX14_INVALID_ARG_2;
   //if (sample_start + sample_count > PX14_RAM_SIZE_IN_SAMPLES)
   //	return SIG_PX14_INVALID_ARG_3;
   //if (NULL == dma_bufp)
   //	return SIG_PX14_INVALID_ARG_4;
   //if ((ram_bank != 0) && (ram_bank != 1))
   //	return SIG_PX14_INVALID_ARG_6;
   //res = ValidateHandle(hBrd, &statep);
   //PX14_RETURN_ON_FAIL(res);

   //if (0 == sample_count)
   //	return SIG_SUCCESS;
   //if (sample_count < PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES)
   //	return SIG_PX14_XFER_SIZE_TOO_SMALL;
   //if (sample_count > PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES)
   //	return SIG_PX14_XFER_SIZE_TOO_LARGE;
   //if (sample_count % PX14_ALIGN_SAMPLE_COUNT_BURST)
   //	return SIG_PX14_ALIGNMENT_ERROR;

   //if (statep->IsDriverVerLessThan(PX14_VER64(1,5,2,0)))
   //	return SIG_PX14_NOT_IMPLEMENTED_IN_DRIVER;
   //res = CheckFirmwareVerPX14(hBrd,
   //	PX14_VER32(1,1,1,0), PX14FWTYPE_PCI, PX14VCT_GTOE);
   //PX14_RETURN_ON_FAIL(res);
   //PX14_CT_ASSERT(PX14_BOARD_TYPE == 0x31);

   //// Define active memory region
   //res = SetStartSamplePX14(hBrd, sample_start);
   //PX14_RETURN_ON_FAIL(res);
   //res = SetSampleCountPX14(hBrd, sample_count);
   //PX14_RETURN_ON_FAIL(res);

   //// Enter the PCI read mode
   //// TODO_WRAM : Mode will depend on ram_bank!
   //res = SetOperatingModePX14(hBrd, PX14MODE_RAM_WRITE_PCI);
   //PX14_RETURN_ON_FAIL(res);

   //// Do the DMA transfer
   //res	= DmaTransferPX14(hBrd, const_cast<px14_sample_t*>(dma_bufp),
   //	sample_count * PX14_SAMPLE_SIZE_IN_BYTES, PX14_FALSE,
   //	bAsynchronous != 0);
   //if (SIG_SUCCESS != res)
   //{
   //	SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   //	return res;
   //}
   //
   //if (!bAsynchronous)
   //{
   //	res = WaitForRamWriteCompletionPX14(hBrd, 0);
   //	// Return back to standby mode; driver will do this but explicit
   //	//  call here will keep our register cache coherent
   //	SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   //}

   //return res;

   // This feature is mostly working, but we are sometimes missing interrupts
   return SIG_PX14_NOT_IMPLEMENTED;
}


/** @brief Determine if transfer is currently in progress

  @note
  This function will always return 0 for virtual devices

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @retval
  Returns 1 if a transfer is currently in progress, 0 if a transfer
  is not in progress, or a (negative) SIG_* error code on error
  */
PX14API IsTransferInProgressPX14 (HPX14 hBrd)
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

   if ((dev_state == PX14STATE_DMA_XFER_FAST) ||
       (dev_state == PX14STATE_DMA_XFER_BUFFERED))
   {
      return 1;
   }

   return 0;
}

// Wait (sleep) for a data transfer to complete with optional timeout
PX14API WaitForTransferCompletePX14 (HPX14 hBrd, unsigned int timeout_ms)
{
   int res;

   PX14S_WAIT_OP ctx;
   ctx.struct_size = sizeof(PX14S_WAIT_OP);
   ctx.wait_op = PX14STATE_DMA_XFER_FAST;
   ctx.timeout_ms = timeout_ms;
   ctx.dummy = 0;

   res = DeviceRequest(hBrd, IOCTL_PX14_WAIT_ACQ_OR_XFER,
                       &ctx, sizeof(PX14S_WAIT_OP));

   if (SIG_CANCELLED == res)
   {
      // Transfer was cancelled. The driver has put the card into
      //  Standby mode already. Do an explicit call here to ensure that
      //  our internal cached state coherant.
      if (PX14MODE_STANDBY != GetOperatingModePX14(hBrd))
         SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   }

   return res;
}

/** @brief
  Xfer data in PX14 RAM to a buffer on the PC; any boundary or
  alignment

  Transfers the data from the given region of the given board to the given
  buffer. Note that this buffer need not be a DMA buffer. This function
  exists as a convenience in situations where getting data off of the
  board is not speed-critical. This function does not run as fast as the
  ReadSampleRamFastPX14 function but has a few advantages:
  - No DMA buffer needs to be allocated by the caller
  - Starting sample and transfer length needn't be on any particular
  boundary
  - The transfer size is not bound by the minimum or maximum DMA
  transfer size

  This function can also start an asynchronous transfer. By default, this
  function will not return until the data transfer completes. If the
  bAsynchronous parameter is nonzero, then the function will start the
  data transfer and return immediately without waiting for it to finish
  allowing the calling thread to continue working. The caller can use the
  IsTransferInProgressPX14 function to determine if the transfer is still
  in progress. To wait (sleep) for the transfer to complete, call the
  WaitForTransferCompletePX14 function.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param sample_start
  The starting sample in PX14 RAM at which to begin transferring.
  This value does not need to be on any particular boundary.
  @param sample_count
  The total number of samples to transfer
  @param bufp
  A pointer to the buffer that will receive the PX14 data. This
  buffer does not have to be a DMA buffer. (If you do have a DMA
  buffer you should use ReadSampleRamFastPX14 since it's much
  faster due to not having to use intermediate buffering.)
  @param bAsynchronous
  If this parameter is nonzero then an asynchronous data transfer will
  be performed. See remarks

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.

  @see ReadSampleRamFastPX14
  */
PX14API ReadSampleRamBufPX14 (HPX14 hBrd,
                              unsigned int sample_start,
                              unsigned int sample_count,
                              px14_sample_t* bufp,
                              int bAsynchronous)
{
   unsigned int ram_samples, sample_chunk, max_read_samples;
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(bufp, px14_sample_t);
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (SIG_SUCCESS != GetSampleRamSizePX14(hBrd, &ram_samples))
      ram_samples = PX14_RAM_SIZE_IN_SAMPLES;

   if (sample_start > PX14_MAX_START_SAMPLE)
      return SIG_PX14_INVALID_ARG_2;
   if ((sample_count > PX14_MAX_SAMPLE_COUNT) ||
       (sample_start + sample_count > ram_samples))
   {
      return SIG_PX14_INVALID_ARG_3;
   }
   if (!sample_count)
      return SIG_SUCCESS;
   if (NULL == bufp)
      return SIG_PX14_INVALID_ARG_4;

   if (statep->IsRemote())
      return rmc_ReadSampleRam(hBrd, sample_start, sample_count, bufp);

   if (statep->IsVirtual())
   {
      return GenerateVirtualDataPX14(hBrd, bufp, sample_count,
                                     sample_start);
   }

   max_read_samples = PX14_MAX_DEVICE_READ_BYTES / sizeof(px14_sample_t);

   // If we're going to do an asynchronous transfers, all user data will
   //  need to be mapped ahead of time.
   if (bAsynchronous && (sample_count > max_read_samples))
      return SIG_PX14_XFER_SIZE_TOO_LARGE;

   while (sample_count)
   {
      sample_chunk = PX14_MIN(sample_count, max_read_samples);

      PX14S_DRIVER_BUFFERED_XFER req;
      req.struct_size = sizeof(PX14S_DRIVER_BUFFERED_XFER);
      req.flags = PX14MODE_RAM_READ_PCI;
      if (bAsynchronous)
         req.flags |= PX14DBTF_ASYNCHRONOUS;
      req.sample_start = sample_start;
      req.sample_count = sample_chunk;
      req.user_virt_addr = (unsigned long long)(uintptr_t)bufp;
      req.user_virt_addr2 = 0;

      res = DeviceRequest(hBrd, IOCTL_PX14_DRIVER_BUFFERED_XFER,
                          &req, req.struct_size);
      PX14_RETURN_ON_FAIL(res);

      if ((sample_count -= sample_chunk) > 0)
      {
         sample_start += sample_chunk;
         bufp += sample_chunk;
      }
   }

   return SIG_SUCCESS;
}

/** @brief
  Transfer and deinterleave data in brd RAM to host PC; any
  boundary or alignment

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param sample_start
  The starting sample number at which to begin reading. Note this is
  sample number, not sample address. Dual channel data is sample
  interleaved in PX14 RAM. For this reason, when interpreting RAM as
  dual channel, channel 1 sample N sits at address 2*N.
  */
PX14API ReadSampleRamDualChannelBufPX14 (HPX14 hBrd,
                                         unsigned int sample_start,
                                         unsigned int sample_count,
                                         px14_sample_t* buf_ch1p,
                                         px14_sample_t* buf_ch2p,
                                         int bAsynchronous)
{
   unsigned int ram_samples, sample_chunk, max_read_samples;
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(buf_ch1p, px14_sample_t);
   SIGASSERT_NULL_OR_POINTER(buf_ch2p, px14_sample_t);

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (SIG_SUCCESS != GetSampleRamSizePX14(hBrd, &ram_samples))
      ram_samples = PX14_RAM_SIZE_IN_SAMPLES;

   // If we're assuming that RAM contains dual channel data then a sample's
   //  address is (2 * sample number) since channel data is sample
   //  interleaved
   sample_start <<= 1;

   if (sample_start > PX14_MAX_START_SAMPLE)
      return SIG_PX14_INVALID_ARG_2;
   if ((sample_count > PX14_MAX_SAMPLE_COUNT) ||
       (sample_start + sample_count > ram_samples))
   {
      return SIG_PX14_INVALID_ARG_3;
   }
   if (!sample_count)
      return SIG_SUCCESS;
   if ((NULL == buf_ch1p) && (NULL == buf_ch2p))
      return SIG_PX14_INVALID_ARG_5;

   if (statep->IsRemote())
      return SIG_PX14_REMOTE_CALL_NOT_AVAILABLE;	// Maybe later...

   if (statep->IsVirtual())
   {
      if (buf_ch1p)
      {
         res = GenerateVirtualDataPX14(hBrd, buf_ch1p, sample_count >> 1,
                                       sample_start, PX14CHANNEL_ONE);
         PX14_RETURN_ON_FAIL(res);
      }

      if (buf_ch2p)
      {
         res = GenerateVirtualDataPX14(hBrd, buf_ch2p, sample_count >> 1,
                                       sample_start, PX14CHANNEL_TWO);
         PX14_RETURN_ON_FAIL(res);
      }

      return SIG_SUCCESS;
   }

   max_read_samples = PX14_MAX_DEVICE_READ_BYTES / sizeof(px14_sample_t);

   if (bAsynchronous && (sample_count > max_read_samples))
      return SIG_PX14_XFER_SIZE_TOO_LARGE;

   while (sample_count)
   {
      sample_chunk = PX14_MIN(sample_count, max_read_samples);

      PX14S_DRIVER_BUFFERED_XFER req;
      req.struct_size = sizeof(PX14S_DRIVER_BUFFERED_XFER);
      req.flags = PX14MODE_RAM_READ_PCI | PX14DBTF_DEINTERLEAVE;
      if (bAsynchronous)
         req.flags |= PX14DBTF_ASYNCHRONOUS;
      req.sample_start = sample_start;
      req.sample_count = sample_chunk;
      req.user_virt_addr =  (unsigned long long)(uintptr_t)buf_ch1p;
      req.user_virt_addr2 = (unsigned long long)(uintptr_t)buf_ch2p;

      res = DeviceRequest(hBrd, IOCTL_PX14_DRIVER_BUFFERED_XFER,
                          &req, req.struct_size);
      PX14_RETURN_ON_FAIL(res);

      if ((sample_count -= sample_chunk) > 0)
      {
         sample_start += sample_chunk >> 1;
         if (buf_ch1p) buf_ch1p += sample_chunk >> 1;
         if (buf_ch2p) buf_ch2p += sample_chunk >> 1;
      }
   }

   return SIG_SUCCESS;
}

/** @brief Transfer data in PX14 RAM to the SAB bus

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param sample_start
  The PX14 RAM sample index at which to begin copying data
  @param sample_count
  The total number of samples to transfer. This parameter may not be
  PX14_FREE_RUN.
  @param timeout_ms
  An optional timeout value in milliseconds. If this value is 0
  then the function will not timeout
  @param bAsynchronous
  If this parameter is true then an asynchronous SAB transfer will be
  performed.

  @return
  Returns SIG_SUCCESS (0) on success or one of the SIG_* error values
  on error
  */
PX14API TransferSampleRamToSabPX14 (HPX14 hBrd,
                                    unsigned int sample_start,
                                    unsigned int sample_count,
                                    unsigned int timeout_ms,
                                    int bAsynchronous)
{
   CStatePX14* statep;
   int res;

   if ((PX14_FREE_RUN == sample_count) ||
       (sample_count > PX14_MAX_SAMPLE_COUNT))
   {
      return SIG_PX14_INVALID_ARG_3;
   }
   if (sample_start > PX14_MAX_START_SAMPLE)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!InIdleModePX14(hBrd))
      return SIG_INVALID_MODE;

   // Make sure we're in standby mode
   res = SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   PX14_RETURN_ON_FAIL(res);

   // Setup active memory region
   res = SetSampleCountPX14(hBrd, sample_start);
   PX14_RETURN_ON_FAIL(res);
   res = SetSampleCountPX14(hBrd, sample_count);
   PX14_RETURN_ON_FAIL(res);

   // Start the SAB transfer
   res = SetOperatingModePX14(hBrd, PX14MODE_RAM_READ_SAB);
   PX14_RETURN_ON_FAIL(res);

   // If this is a synchronous request, wait for acquisition to complete
   if (!bAsynchronous)
   {
      res = WaitForAcquisitionCompletePX14(hBrd, timeout_ms);
      SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   }

   return res;
}

PX14API WaitForRamWriteCompletionPX14 (HPX14 hBrd, unsigned int timeout_ms)
{
   //// TODO_WRAM : Document write RAM functions: WriteSampleRamFastPX14, WaitForRamWriteCompletionPX14

   //PX14S_WAIT_OP ctx;
   //ctx.struct_size = sizeof(PX14S_WAIT_OP);
   //ctx.wait_op = PX14STATE_WRAM;
   //ctx.timeout_ms = timeout_ms;
   //ctx.dummy = 0;

   //return DeviceRequest(hBrd, IOCTL_PX14_WAIT_ACQ_OR_XFER,
   //	&ctx, sizeof(PX14S_WAIT_OP));

   return SIG_PX14_NOT_IMPLEMENTED;
}

