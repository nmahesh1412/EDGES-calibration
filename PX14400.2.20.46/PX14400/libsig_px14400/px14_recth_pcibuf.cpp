/** @file	px14_recth_pcibuf.cpp
  @brief	Implementation of RAM-buffered PCIe recording (double-buffer imp)
  */
#include "stdafx.h"
#include "px14_top.h"

// Defining these turns on some internal file IO performance tracking and is
//  only of interest to Signatec
//#define TRACK_MAX_FILE_WRITES			// Windows only
//#define TRACK_EST_FIFO_DEPTH			// Windows only
//#define TRACK_DATA_THROUGHPUT

#if !defined(_DEBUG) && (defined(TRACK_MAX_FILE_WRITES) || defined(TRACK_EST_FIFO_DEPTH) || defined(TRACK_DATA_THROUGHPUT))
#error Comment out tracking for release code please!
#endif

   CPX14RecSes_PciBuf::CPX14RecSes_PciBuf()
: m_bCheckIn1(false), m_bCheckIn2(false)
{
}

int CPX14RecSes_PciBuf::InitRecordingBuffers()
{
   int res;

   if ((NULL == m_rec_params.dma_bufp) && (m_rec_params.rec_flags & PX14RECSESF_BOOT_BUFFERS_OKAY))
   {
      res = CheckForBootBuffers();
      if (SIG_SUCCESS == res)
         return SIG_SUCCESS;
   }

   return CPX14RecSession::InitRecordingBuffers();
}

int CPX14RecSes_PciBuf::CheckForBootBuffers()
{
   unsigned int buf_samples, xfer_samps;
   px14_sample_t *dma_buf1p, *dma_buf2p;
   int i, max_bufs, res, checked_out;
   unsigned short buf_idx;

   max_bufs = BootBufGetMaxCountPX14(m_hBrdMainThread);
   xfer_samps = m_rec_params.xfer_samples;

   // First look for one buffer that's large enough for both transfers
   for (i=0; i<max_bufs; i++)
   {
      buf_idx = static_cast<unsigned short>(i);
      res = BootBufQueryPX14(m_hBrdMainThread, buf_idx, &checked_out, &buf_samples);
      if ((SIG_SUCCESS == res) && (buf_samples >= (xfer_samps << 1)) && !checked_out)
      {
         res = BootBufCheckOutPX14(m_hBrdMainThread, buf_idx, &dma_buf1p, NULL);
         if (SIG_SUCCESS == res)
         {
            mt_xbuf1p = dma_buf1p;
            mt_xbuf2p = dma_buf1p + xfer_samps;
            m_bCheckIn2 = false;
            m_bCheckIn1 = true;
            return SIG_SUCCESS;
         }
      }
   }

   // Next try two individual buffers
   dma_buf1p = dma_buf2p = NULL;
   for (i=0; i<max_bufs; i++)
   {
      buf_idx = static_cast<unsigned short>(i);
      res = BootBufQueryPX14(m_hBrdMainThread, buf_idx, &checked_out, &buf_samples);
      if ((SIG_SUCCESS == res) && (buf_samples >= xfer_samps) && !checked_out)
      {
         res = BootBufCheckOutPX14(m_hBrdMainThread, buf_idx,
                                   dma_buf1p ? &dma_buf2p : &dma_buf1p, NULL);
         if (SIG_SUCCESS == res)
         {
            if (dma_buf2p)
            {
               mt_xbuf2p = dma_buf2p;
               m_bCheckIn2 = true;
               return SIG_SUCCESS;
            }
            else
            {
               mt_xbuf1p = dma_buf1p;
               m_bCheckIn1 = true;
            }
         }
      }
   }

   if (m_bCheckIn1)
   {
      BootBufCheckInPX14(m_hBrdMainThread, mt_xbuf1p);
      m_bCheckIn1 = false;
   }

   return -1;
}

void CPX14RecSes_PciBuf::PostThreadRun()
{
   if (m_bCheckIn1 && mt_xbuf1p)
      BootBufCheckInPX14(m_hBrdMainThread, mt_xbuf1p);
   if (m_bCheckIn2 && mt_xbuf2p)
      BootBufCheckInPX14(m_hBrdMainThread, mt_xbuf2p);

   m_bCheckIn1 = m_bCheckIn2 = false;

   CPX14RecSession::PostThreadRun();
}

int CPX14RecSes_PciBuf::th_RecordMain()
{
   unsigned int loop_counter, tick_start, tick_last_sync, tick_now, samps_to_proc;
   unsigned long long samples_recorded;//, samples_left;
   px14_sample_t *cur_chunkp, *prev_chunkp;
   IIoSinkCtxPX14* pIoSink;
   int res, cancel_res;
   bool bDone;

   //samples_left = m_rec_params.rec_samples;
   tick_last_sync = tick_start = 0;
   cancel_res = SIG_CANCELLED;
   samples_recorded = 0;
   prev_chunkp = NULL;
   loop_counter = 0;
   bDone = false;

   pthread_mutex_lock(&m_mux);
   {
      m_rec_status = PX14RECSTAT_IN_PROGRESS;
   }
   pthread_mutex_unlock(&m_mux);

   // Create and init the IO sink. This is the object that handles
   //  the processing (e.g. writing to disk, etc) of acquisition data
   pIoSink = NULL;
   res = th_CreateIoSink(&pIoSink);
   CAutoIoSinkCtx auto_sink(pIoSink);	// Auto release on destruction

   // Synchronize state now since main thread will check on our progress
   //  after we synch thread start below.
   if (SIG_SUCCESS != res)
   {
      // Don't bail out yet; need to sync with main thread
      cancel_res = th_RuntimeError(res, mt_err_preamble);
   }

   // Setup active memory region for a free-run acquisition
   SetStartSamplePX14(m_hBrd, 0);
   SetSampleCountPX14(m_hBrd, PX14_FREE_RUN);

   // Signal main thread that we're created and ready. This will be
   //  followed by an initial progress check.
   m_sync_rec_thread.SetEvent();

   // Wait for main thread to tell us it's cool to arm. This
   //  synchronization is needed for master/slave recordings
   m_sync_arm.WaitEvent();
   if (_PX14_UNLIKELY(m_bStopRecPlease))
      return cancel_res;

#ifdef TRACK_MAX_FILE_WRITES
   std::ofstream of_max_write("C:\\Temp\\PX14_file_write_max.txt");
   LARGE_INTEGER mw_start, mw_end, mw_freq;
   double mw_max_secs = 0, mw_cur_secs;
   QueryPerformanceFrequency(&mw_freq);
#endif

#ifdef TRACK_DATA_THROUGHPUT
   static const unsigned int dt_period_ms = 200;
   std::ofstream of_data_throughput("C:\\Temp\\PX14_data_throughput.txt");
   DWORD dt_tick_last = SysGetTickCount();
#endif

#ifdef TRACK_EST_FIFO_DEPTH
   static const unsigned s_tf_period_ms = 200;
   HANDLE hFifoDepth = CreateFileA("C:\\Temp\\PX14_fifo_depth.rd16", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
   unsigned int tf_tick_start, tf_tick_last_update=0, tf_tick_now;
   unsigned long long tf_samps_acquired = 0;
   int tf_chan_count = (PX14CHANNEL_DUAL == GetActiveChannelsPX14(m_hBrd)) ? 2 : 1;
   double tf_dAcqRateMHz;
   GetEffectiveAcqRatePX14(m_hBrd, &tf_dAcqRateMHz);
   tf_tick_start = SysGetTickCount();
#endif

   time(&mt_time_armed);
   res = SetOperatingModePX14(m_hBrd, PX14MODE_ACQ_PCI_BUF);
   if (_PX14_UNLIKELY(SIG_SUCCESS != res))
      return th_RuntimeError(res, "Failed to enter acquisition mode: ");
   CAutoStandbyModePX14 autoStandby(m_hBrd);

   // Main recording loop
   while (!bDone)
   {
      if (loop_counter & 1)
         cur_chunkp = mt_xbuf2p;
      else
         cur_chunkp = mt_xbuf1p;

      // Start asynchronous DMA transfer of new data
      res = GetPciAcquisitionDataFastPX14(m_hBrd,
                                          m_rec_params.xfer_samples, cur_chunkp, PX14_TRUE);
      if (SIG_SUCCESS != res)
         return th_RuntimeError(res, "Failed to get PCI acquisition data: ");

      // Process previous chunk of data while new transfer is in progress
      if (_PX14_LIKELY(NULL != prev_chunkp))
      {
         // Figure out how much more data to process
         samps_to_proc = m_rec_params.xfer_samples;
         if (m_rec_params.rec_samples &&
             samples_recorded + samps_to_proc > m_rec_params.rec_samples)
         {
            samps_to_proc =
               static_cast<unsigned int>(m_rec_params.rec_samples - samples_recorded);
         }

         // Process (e.g. write) previous chunk
#ifdef TRACK_MAX_FILE_WRITES
         QueryPerformanceCounter(&mw_start);
#endif
         res = pIoSink->Write(prev_chunkp, samps_to_proc);

#ifdef TRACK_MAX_FILE_WRITES
         QueryPerformanceCounter(&mw_end);
         mw_cur_secs = ((double)(mw_end.QuadPart - mw_start.QuadPart)) / mw_freq.QuadPart;
         if (mw_cur_secs > mw_max_secs)
         {
            if (of_max_write.is_open())
               of_max_write << "Max: Loop: " << loop_counter << ", secs: " << mw_cur_secs << std::endl;
            mw_max_secs = mw_cur_secs;
         }
         if (mw_cur_secs > 0.100)
         {
            if (of_max_write.is_open())
               of_max_write << "Big: Loop: " << loop_counter << ", secs: " << mw_cur_secs << std::endl;
         }
#endif
         if (_PX14_UNLIKELY(SIG_SUCCESS != res))
            return th_RuntimeError(res, "Error processing acquisition data: ");
         // Grab snapshot if necessary
         if (mt_bSnapshots)
            th_Snapshot(prev_chunkp, m_rec_params.xfer_samples);

         samples_recorded += samps_to_proc;
         if (m_rec_params.rec_samples && (samples_recorded >= m_rec_params.rec_samples))
            bDone = true;
      }

      // Wait for the asynchronous DMA transfer to complete. This thread
      //  will sleep until the transfer completes
      res = WaitForTransferCompletePX14(m_hBrd, 0);
      if (_PX14_UNLIKELY(SIG_SUCCESS != res))
      {
         if (SIG_CANCELLED != res)
         {
            return th_RuntimeError(res,
                                   "An error occurred while waiting for transfer of " \
                                   "acquisition data: ");
         }

         bDone = true;
      }
#ifdef TRACK_EST_FIFO_DEPTH
      tf_samps_acquired += m_rec_params.xfer_samples;
      tf_tick_now = SysGetTickCount();
      if (SysGetElapsedTicks(tf_tick_last_update, tf_tick_now) >= s_tf_period_ms)
      {
         unsigned tf_total_ms;
         double dFifoSamples;
         DWORD dwBytes;

         tf_tick_last_update = tf_tick_now;
         tf_total_ms = SysGetElapsedTicks(tf_tick_start, tf_tick_now);
         dFifoSamples = (tf_total_ms / 1000.0) * (tf_dAcqRateMHz * 1000000.0 * tf_chan_count);
         dFifoSamples -= tf_samps_acquired;
         unsigned short fifo_mb = static_cast<unsigned short>(dFifoSamples / _1mebi);
         if (INVALID_HANDLE_VALUE != hFifoDepth)
            WriteFile(hFifoDepth, &fifo_mb, 2, &dwBytes, NULL);
      }
#endif

      // Are we being asked to quit?
      if (_PX14_UNLIKELY(m_bStopRecPlease))
         bDone = true;

      // The software can never know exactly when the board triggers, so
      //  we mark the start of our recording after the first data transfer
      //  completes
      if (_PX14_UNLIKELY(0 == tick_start))
         tick_start = SysGetTickCount();

      // Update counters
      loop_counter++;
      prev_chunkp = cur_chunkp;

      // Periodically update progress stats
      tick_now = SysGetTickCount();
      if (bDone ||
          SysGetElapsedTicks(tick_last_sync, tick_now) >= s_update_prog_ticks)
      {
         tick_last_sync = tick_now;
         pthread_mutex_lock(&m_mux);
         {
            m_elapsed_time_ms = SysGetElapsedTicks(tick_start, tick_now);
            m_samps_recorded  = samples_recorded;
            m_xfer_count      = loop_counter;
         }
         pthread_mutex_unlock(&m_mux);
      }
#ifdef TRACK_DATA_THROUGHPUT
      if (of_data_throughput.is_open() &&
          SysGetElapsedTicks(dt_tick_last, tick_now) >= dt_period_ms)
      {
         // Rate in MB/s
         double dt_rate =
            (samples_recorded * PX14_SAMPLE_SIZE_IN_BYTES / 1000000.0) /  (SysGetElapsedTicks(tick_start, tick_now) / 1000.0);
         of_data_throughput << dt_rate << std::endl;
      }
#endif
   }

   EndBufferedPciAcquisitionPX14(m_hBrd);

   pthread_mutex_lock(&m_mux);
   {
      m_rec_status = PX14RECSTAT_COMPLETE;
   }
   pthread_mutex_unlock(&m_mux);

#ifdef TRACK_EST_FIFO_DEPTH
   if (INVALID_HANDLE_VALUE != hFifoDepth)
      CloseHandle(hFifoDepth);
#endif

   return SIG_SUCCESS;
}

