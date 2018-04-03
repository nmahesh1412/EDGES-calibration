/** @file	px14_recth_imp.cpp
  @brief	Recording thread routines; mainly framework and utility
  */
#include "stdafx.h"
#include "px14_top.h"

void* CPX14RecSession::th_raw (void* paramp)
{//static

   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
   pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

   reinterpret_cast<CPX14RecSession*>(paramp)->th_main();
   return NULL;
}

void CPX14RecSession::th_main()
{
   mt_rec_result = th_RecordMain();

   PostThreadRun();

   // Synchronize end of rec thread with main thread
   m_sync_rec_thread.SetEvent();
}

int CPX14RecSession::th_RuntimeError (int res, const char* descp)
{
   mt_sys_err_code = GetSystemErrorVal();
   mt_err_preamble = descp;
   mt_rec_result = res;

   pthread_mutex_lock(&m_mux);
   {
      m_rec_status = PX14RECSTAT_ERROR;
   }
   pthread_mutex_unlock(&m_mux);

   return res;
}

void CPX14RecSession::PostThreadRun()
{
   time(&mt_time_ended);

   th_PostRecordUpdateAllSrdcData();

   // Wipe this out here since this thread allocated content
   mt_srdc_list.clear();
}

int CPX14RecSession::th_CreateIoSink(IIoSinkCtxPX14** sinkpp)
{
   IIoSinkCtxPX14* sinkp;
   int res;

   PX14_ENSURE_POINTER(PX14_INVALID_HANDLE, sinkpp, IIoSinkCtxPX14*, NULL);

   // Create IO sink object. This object will be responsible for
   //  dumping recorded data to destination file(s).
   res = CreateIoSinkCtxPX14(m_hBrd, &m_fil_params, &sinkp);
   if (SIG_SUCCESS != res)
   {
      mt_err_preamble = "Failed to create IO sink object: ";
      return res;
   }

   // Initialize IO sink object
   sinkp->SetSrdcGenCallback(th_MyRecSrdcGenCallback, this);
   res = sinkp->Init(m_hBrd, m_rec_params.rec_samples, m_fil_params);
   if (SIG_SUCCESS != res)
   {
      sinkp->Release();
      mt_err_preamble = "Failed to initialize IO sink object: ";
      return res;
   }

   *sinkpp = sinkp;

   return SIG_SUCCESS;
}

int CPX14RecSession::th_MyRecSrdcGenCallback (HPX14SRDC hSrdc, void* callback_ctxp)
{
   CSigRecDataFile* pSrdcFile;
   CPX14RecSession* ctxp;

   ctxp = reinterpret_cast<CPX14RecSession*>(callback_ctxp);
   SIGASSERT_POINTER(ctxp, CPX14RecSession);
   SIGASSERT(ctxp->_ses_magic == CPX14SessionBase::_ses_magic);

   pSrdcFile = reinterpret_cast<CSigRecDataFile*>(hSrdc);
   SIGASSERT_POINTER(pSrdcFile, CSigRecDataFile);

   // Cache all generated SRDC files so we can go back and update
   //  them after recording completes.
   ctxp->mt_srdc_list.push_back(pSrdcFile->m_pathname);

   return SIG_SUCCESS;
}

int CPX14RecSession::th_PostRecordUpdateAllSrdcData ()
{
   HPX14SRDC hFile;
   int res;

   CPX14RecSession::SrdcFileList::iterator fl(mt_srdc_list.begin());
   for (; fl!=mt_srdc_list.end(); fl++)
   {
      res = OpenSrdcFileAPX14(m_hBrd, &hFile, fl->c_str(), PX14SRDCOF_OPEN_EXISTING);
      if (SIG_SUCCESS == res)
      {
         CAutoSrdcHandle ash(hFile);		// Auto close SRDC file handle on exit
         res = th_PostRecUpdateSrdcFile(hFile);
         if (SIG_SUCCESS == res)
            SaveSrdcFileAPX14(hFile, NULL);
      }

   }

   return SIG_SUCCESS;
}

int CPX14RecSession::th_PostRecUpdateSrdcFile (HPX14SRDC hSrdc)
{
   int res;

   std::string s;

   SetSrdcItemAPX14(hSrdc, "RecArmTimeStr", NULL);
   SetSrdcItemAPX14(hSrdc, "RecArmTimeSec", NULL);
   if (mt_time_armed)
   {
      // We'll outout two items:
      //  - RecArmTimeStr - A full string representation of date and time
      //  - RecArmTimeSec - Time in seconds since midnight 1/1/1970

      s = my_ConvertToString(mt_time_armed);
      SetSrdcItemAPX14(hSrdc, "RecArmTimeSec", s.c_str());

      res = MakeTimeString(mt_time_armed, s);
      if (SIG_SUCCESS == res)
         SetSrdcItemAPX14(hSrdc, "RecArmTimeStr", s.c_str());
   }

   SetSrdcItemAPX14(hSrdc, "RecEndTimeStr", NULL);
   SetSrdcItemAPX14(hSrdc, "RecEndTimeSec", NULL);
   if (mt_time_ended)
   {
      // We'll outout two items:
      //  - RecEndTimeStr - A full string representation of date and time
      //  - RecEndTimeSec - Time in seconds since midnight 1/1/1970

      s = my_ConvertToString(mt_time_ended);
      SetSrdcItemAPX14(hSrdc, "RecEndTimeSec", s.c_str());

      res = MakeTimeString(mt_time_ended, s);
      if (SIG_SUCCESS == res)
         SetSrdcItemAPX14(hSrdc, "RecEndTimeStr", s.c_str());
   }

   return SIG_SUCCESS;
}

bool CPX14RecSession::th_Snapshot (const px14_sample_t* rec_data,
                                   unsigned int rec_data_samples)
{
   unsigned tick_now, new_ss_cur, samps_to_copy;
   bool bUpdateSnapshot;

   bUpdateSnapshot = false;

   if (mt_ss_by_xfers)
   {
      // When mt_ss_by_xfers is true, then mt_ss_cur is the number
      //  of DMA transfer left before we should do a snapshot update

      // Update snapshot?
      if (!mt_ss_cur)
      {
         new_ss_cur = m_rec_params.ss_period_xfer;
         bUpdateSnapshot = true;
      }
      else
      {
         // Count down for transfer counter
         mt_ss_cur--;
      }
   }
   else
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
         samps_to_copy = PX14_MIN(rec_data_samples, m_ss_buf_samps);
         memcpy(m_ss_bufp, rec_data, samps_to_copy * sizeof(px14_sample_t));
         m_ss_buf_valid = samps_to_copy;
         m_ss_counter++;
      }
      pthread_mutex_unlock(&m_mux);
   }

   return bUpdateSnapshot;
}

