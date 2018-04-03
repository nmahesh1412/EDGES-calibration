/** @file	px14_recth_pcibuf_chained.cpp
  @brief	Implementation of RAM-buffered PCIe recording (DMA buffer chain)
  */
#include "stdafx.h"
#include "px14_top.h"

   CPX14RecSes_PciBufChained::CPX14RecSes_PciBufChained()
: m_thp_stop_please(false), m_bufList(NULL), m_thd_err_str(NULL),
   m_thd_lastError(0), m_thd_res(0), m_thd_bufIdxStop(-1),
   m_thp_thread_is_alive(false)
{
}

CPX14RecSes_PciBufChained::~CPX14RecSes_PciBufChained()
{
}

int CPX14RecSes_PciBufChained::InitRecordingBuffers()
{
   unsigned int chain_samples;
   int res, chain_flags;

   SIGASSERT(0 != (m_rec_params.rec_flags & PX14RECSESF_DEEP_BUFFERING));

   chain_samples = m_rec_params.dma_buf_samples;
   if (0 == chain_samples)
      chain_samples = _def_chain_samps;
   chain_flags = PX14DMACHAINF_UTILITY_CHAIN;

   res = AllocateDmaBufferChainPX14(m_hBrdMainThread, chain_samples,
                                    NULL, chain_flags, NULL);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

int CPX14RecSes_PciBufChained::PreThreadCreate()
{
   unsigned int chain_samples;
   PX14S_BUFNODE* node_curp;
   int buffer_count, res;

   // Make sure we have a buffer chain allocated
   GetUtilityDmaBufferChainPX14(m_hBrdMainThread, &m_bufList, &chain_samples);
   if ((NULL == m_bufList) || (chain_samples < s_min_chain_samples))
      return SIG_PX14_BUFFER_TOO_SMALL;
   node_curp = m_bufList;
   for (buffer_count=0; node_curp->bufp; node_curp++, buffer_count++);
   if (buffer_count < s_min_buf_count)
      return SIG_PX14_BUFFER_TOO_SMALL;

   // Create semaphore objects - used to synchronize access to DMA buffers
   //  between the threads
   res = m_semDma.Init (buffer_count, buffer_count);
   PX14_RETURN_ON_FAIL(res);
   res = m_semProc.Init(0, buffer_count);
   PX14_RETURN_ON_FAIL(res);

   m_sync_proc_start.ClearEvent();
   m_sync_proc_end.ClearEvent();

   // Create the data processing (consumer) thread
   res = pthread_create(&m_thp_thread, NULL, th2_raw, this);
   if (res != 0)
      return SIG_PX14_THREAD_CREATE_FAILURE;
   m_thp_thread_is_alive = true;

   // Synchronize with thread; condition set when thread init complete
   m_sync_proc_start.WaitEvent();

   return SIG_SUCCESS;
}

void CPX14RecSes_PciBufChained::PostThreadRun()
{
   // Don't do anything; we'll handle this when the processing thread finishes
}

int CPX14RecSes_PciBufChained::Abort()
{
   bool bThreadLocked;
   int res;

   if (m_thp_thread_is_alive)
   {
      bThreadLocked = false;

      // Ask nicely
      m_thp_stop_please = true;
      m_semProc.Release();

      res = m_sync_proc_end.WaitEvent(_rec_timeout_ms);
      if (SIG_SUCCESS != res)
      {
         if (res == SIG_PX14_TIMED_OUT)
         {
            // Thread is locked up for some reason
            pthread_cancel(m_thp_thread);
            bThreadLocked = true;
         }
         else
            return res;
      }

      if (!bThreadLocked)
         pthread_join(m_thp_thread, NULL);

      m_thp_thread_is_alive = false;
   }

   return CPX14RecSession::Abort();
}

void CPX14RecSes_PciBufChained::thp_RuntimeError (int res, const char* descp)
{
   th_RuntimeError(res, descp);
}

void CPX14RecSes_PciBufChained::thd_RuntimeError (int res, const char* descp)
{
   m_thd_lastError = GetSystemErrorVal();
   m_thd_err_str   = descp;
   m_thd_res       = res;
}

int CPX14RecSes_PciBufChained::th_RecordMain()
{
   return thd_DmaThread();
}

/// Data transfer (producer) thread
int CPX14RecSes_PciBufChained::thd_DmaThread()
{
   unsigned int xfer_samples_cur, xfer_samples_total;
   unsigned long long samps_xfered;
   int cancel_res, res, bufIdx;
   px14_sample_t* buf_pos;
   PX14S_BUFNODE* pBufCur;
   HPX14 hBrd;
   bool bDone;

#ifdef _DEBUG
   SysSetThreadName("RecChained_Dma");
#endif

   cancel_res = SIG_CANCELLED;
   samps_xfered = 0;
   pBufCur = m_bufList;
   bufIdx = 0;
   bDone = false;
   hBrd = m_hBrd;

   // Setup active memory region for a free-run acquisition
   SetStartSamplePX14(hBrd, 0);
   SetSampleCountPX14(hBrd, PX14_FREE_RUN);

   // Signal main thread that we're created and ready. This will be
   //  followed by an initial progress check.
   m_sync_rec_thread.SetEvent();

   // Wait for request to arm
   m_sync_arm.WaitEvent();
   if (_PX14_UNLIKELY(m_bStopRecPlease))
      return cancel_res;

   // Arm card for recording
   time(&mt_time_armed);
   res = SetOperatingModePX14(hBrd, PX14MODE_ACQ_PCI_BUF);
   if (_PX14_UNLIKELY(SIG_SUCCESS != res))
   {
      thd_RuntimeError(res, "Failed to enter acquisition mode: ");
      return res;
   }
   CAutoStandbyModePX14 autoStandby(hBrd);

   // Main thread loop
   while (!bDone)
   {
      // Wait for an available DMA buffer
      m_semDma.Acquire();
      if (_PX14_UNLIKELY(m_bStopRecPlease))
         break;

      // We could potentially have multiple DMA transfers per buffer if
      //  buffer is large enough
      xfer_samples_total = pBufCur->buf_samples;
      buf_pos            = pBufCur->bufp;
      while (xfer_samples_total)
      {
         xfer_samples_cur = xfer_samples_total;
         if (xfer_samples_cur > PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES)
            xfer_samples_cur = PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES;

         // Grab fresh acquisition data
         res = GetPciAcquisitionDataFastPX14(hBrd, xfer_samples_cur, buf_pos, PX14_FALSE);
         if (SIG_SUCCESS != res)
         {
            thd_RuntimeError(res, "Failed to transfer data: ");
            m_thd_bufIdxStop = bufIdx;
            m_semProc.Release();
            break;
         }

         xfer_samples_total -= xfer_samples_cur;
         buf_pos            += xfer_samples_cur;
      }
      if (xfer_samples_total)
         break;

      samps_xfered += pBufCur->buf_samples;
      if (m_rec_params.rec_samples &&
          (samps_xfered >= m_rec_params.rec_samples))
      {
         m_thd_res = 1;
         bDone = true;
      }

      m_semProc.Release();

      // Move to next buffer in the chain
      pBufCur++;
      bufIdx++;
      if (pBufCur->bufp == NULL)
      {
         pBufCur = m_bufList;
         bufIdx = 0;
      }
   }

   EndBufferedPciAcquisitionPX14(hBrd);

   return SIG_SUCCESS;
}

void* CPX14RecSes_PciBufChained::th2_raw(void* paramp)
{//static

   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
   pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

   CPX14RecSes_PciBufChained* thisp =
      reinterpret_cast<CPX14RecSes_PciBufChained*>(paramp);

   thisp->thp_ProcThread();

   // Synchronize end of proc thread with main thread
   thisp->m_sync_proc_end.SetEvent();

   return NULL;
}

/// Data processing (consumer) thread
int CPX14RecSes_PciBufChained::thp_ProcThread()
{
   unsigned int tick_start, tick_last_sync, tick_now;
   unsigned int samps_to_proc, loop_counter;
   unsigned long long samples_processed;
   IIoSinkCtxPX14* pIoSink;
   PX14S_BUFNODE* pBufCur;
   int res, bufIdx;
   bool bDone;

#ifdef _DEBUG
   SysSetThreadName("RecChained_Proc");
#endif

   tick_last_sync = tick_start = 0;
   samples_processed = 0;
   loop_counter = 0;
   pBufCur = m_bufList;
   bufIdx = 0;
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
   if (SIG_SUCCESS != res)
      thp_RuntimeError(res, mt_err_preamble);

   // Signal main thread that we're created and ready
   m_sync_proc_start.SetEvent();

   if (SIG_SUCCESS != res)
      return res;

   while (!bDone)
   {
      // Wait for a DMA buffer to be available
      m_semProc.Acquire();
      if ((_PX14_UNLIKELY(m_thp_stop_please)) || (bufIdx == m_thd_bufIdxStop))
         break;
      loop_counter++;

      // The software can never know exactly when the board triggers, so
      //  we mark the start of our recording after the first data transfer
      //  completes
      if (_PX14_UNLIKELY(0 == tick_start))
         tick_start = SysGetTickCount();

      // Figure out how much data to process
      samps_to_proc = pBufCur->buf_samples;
      if (m_rec_params.rec_samples &&
          (samples_processed + samps_to_proc > m_rec_params.rec_samples))
      {
         samps_to_proc =
            static_cast<unsigned int>(m_rec_params.rec_samples - samples_processed);
      }

      // Process the data
      res = pIoSink->Write(pBufCur->bufp, pBufCur->buf_samples);
      if (SIG_SUCCESS != res)
      {
         thp_RuntimeError(res, "Error processing acquisition data: ");
         bDone = true;
      }
      if (mt_bSnapshots)
         th_Snapshot(pBufCur->bufp, pBufCur->buf_samples);

      // Update counters
      samples_processed += samps_to_proc;
      if (m_rec_params.rec_samples && (samples_processed >= m_rec_params.rec_samples))
         bDone = true;

      // Indicate that we're done with this buffer; allows DMA transfer
      //  thread to use this buffer again.
      m_semDma.Release();

      // Periodically update progress stats
      tick_now = SysGetTickCount();
      if (bDone ||
          SysGetElapsedTicks(tick_last_sync, tick_now) >= s_update_prog_ticks)
      {
         tick_last_sync = tick_now;
         pthread_mutex_lock(&m_mux);
         {
            m_elapsed_time_ms = SysGetElapsedTicks(tick_start, tick_now);
            m_samps_recorded  = samples_processed;
            m_xfer_count      = loop_counter;
         }
         pthread_mutex_unlock(&m_mux);
      }

      // Move to next buffer in the chain
      pBufCur++;
      bufIdx++;
      if (pBufCur->bufp == NULL)
      {
         pBufCur = m_bufList;
         bufIdx = 0;
      }
   }

   // We can use base class implementation
   CPX14RecSession::PostThreadRun();

   // Did DMA thread result in error?
   if (m_thd_res < 0)
   {
      // Most likely a FIFO overflow error
      th_RuntimeError(m_thd_res, m_thd_err_str);
      mt_sys_err_code = m_thd_lastError;
   }
   else
   {
      pthread_mutex_lock(&m_mux);
      {
         m_rec_status = PX14RECSTAT_COMPLETE;
      }
      pthread_mutex_unlock(&m_mux);
   }

   return SIG_SUCCESS;
}


