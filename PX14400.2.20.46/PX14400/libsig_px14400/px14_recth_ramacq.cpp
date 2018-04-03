/** @file	px14_recth_ramacq.cpp
  @brief	Implementation of RAM acquisition recordings
  */
#include "stdafx.h"
#include "px14_top.h"

CPX14RecSes_RamAcq::CPX14RecSes_RamAcq()
{
}

int CPX14RecSes_RamAcq::th_RecordMain()
{
   unsigned int tick_start, tick_last_sync, tick_now, acq_samps;
   unsigned int samps_to_write, loop_counter;
   unsigned long long samples_recorded;
   int res, cancel_res;
   IIoSinkCtxPX14* pIoSink;
   bool bDone;
   HPX14 hBrd;

   acq_samps = m_rec_params.acq_samples;
   hBrd = m_hBrd;

   tick_last_sync = tick_start = 0;
   cancel_res = SIG_CANCELLED;
   samples_recorded = 0;
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

   // Signal main thread that we're created and ready
   m_sync_rec_thread.SetEvent();

   // Wait for main thread to tell us it's cool to arm
   m_sync_arm.WaitEvent();
   if (m_bStopRecPlease)
      return cancel_res;

   time(&mt_time_armed);
   while (!bDone)
   {
      // Do the RAM acquisition
      res = AcquireToBoardRamPX14(hBrd, 0, acq_samps);
      if (SIG_SUCCESS != res)
      {
         if (SIG_CANCELLED == res)
            break;

         return th_RuntimeError(res, "Error acquiring to RAM: ");
      }

      // The software can never know exactly when the board triggers, so
      //  we mark the start of our recording after the first data
      //  acquisition completes
      if (_PX14_UNLIKELY(0 == tick_start))
         tick_start = SysGetTickCount();

      // Are we being asked to quit?
      if (_PX14_UNLIKELY(m_bStopRecPlease))
         break;

      // Figure out how much more data to save
      samps_to_write = acq_samps;
      if (m_rec_params.rec_samples &&
          (samples_recorded + samps_to_write > m_rec_params.rec_samples))
      {
         samps_to_write =
            static_cast<unsigned int>(m_rec_params.rec_samples - samples_recorded);
      }

      // Save it to disk
      res = ReadSampleRamFileFastHaveSink(hBrd, 0,
                                          samps_to_write, mt_xbuf1p, m_rec_params.xfer_samples, pIoSink);
      if (SIG_SUCCESS != res)
      {
         if (SIG_CANCELLED == res)
            break;

         return th_RuntimeError(res, "Error saving RAM acquisition data: ");
      }
      // Grab snapshot if necessary
      if (mt_bSnapshots)
         th_SnapshotRamRec(hBrd);

      // Are we being asked to quit?
      if (_PX14_UNLIKELY(m_bStopRecPlease))
         break;

      samples_recorded += samps_to_write;
      loop_counter++;
      if (m_rec_params.rec_samples && (samples_recorded >= m_rec_params.rec_samples))
         bDone = true;

      // Periodically update progress stats
      tick_now = SysGetTickCount();
      if (bDone ||
          SysGetElapsedTicks(tick_last_sync, tick_now) >= s_update_prog_ticks)
      {
         tick_last_sync = tick_now;
         pthread_mutex_lock(&m_mux);
         {
            m_elapsed_time_ms = tick_now - tick_start;
            m_samps_recorded = samples_recorded;
            m_xfer_count = loop_counter;
         }
         pthread_mutex_unlock(&m_mux);
      }
   }

   pthread_mutex_lock(&m_mux);
   {
      m_rec_status = PX14RECSTAT_COMPLETE;
   }
   pthread_mutex_unlock(&m_mux);

   return SIG_SUCCESS;
}

bool CPX14RecSes_RamAcq::th_SnapshotRamRec (HPX14 hBrd)
{
   unsigned tick_now, new_ss_cur, samps_to_copy;
   bool bUpdateSnapshot;
   int res;

   bUpdateSnapshot = false;

   if (_PX14_LIKELY(m_rec_params.ss_period_ms))
   {
      // When mt_ss_by_xfers is not true, then mt_ss_cur is the
      //  tick count of the last data snapshot update

      // Update snapshot?
      tick_now = SysGetTickCount();
      if (tick_now - mt_ss_cur >= m_rec_params.ss_period_ms)
      {
         new_ss_cur = tick_now;
         bUpdateSnapshot = true;
      }
   }

   if (bUpdateSnapshot)
   {
      mt_ss_cur = new_ss_cur;

      pthread_mutex_lock(&m_mux);
      {
         samps_to_copy = PX14_MIN(m_rec_params.acq_samples, m_ss_buf_samps);

         res = ReadSampleRamBufPX14(hBrd, 0, samps_to_copy, m_ss_bufp);
         if (SIG_SUCCESS != res)
            bUpdateSnapshot = false;

         m_ss_buf_valid = samps_to_copy;
         m_ss_counter++;
      }
      pthread_mutex_unlock(&m_mux);
   }

   return bUpdateSnapshot;
}


