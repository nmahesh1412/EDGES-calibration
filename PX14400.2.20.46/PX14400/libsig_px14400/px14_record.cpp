/** @file	px14_record.cpp
*/
#include "stdafx.h"
#include "px14_top.h"

// Module-local function prototypes ------------------------------------- //

static int DeepCopyRecParams (const PX14S_REC_SESSION_PARAMS& src,
                              PX14S_REC_SESSION_PARAMS& dst);

static int DeepCopyFileWriteParams (const PX14S_FILE_WRITE_PARAMS& src,
                                    PX14S_FILE_WRITE_PARAMS& dst);

// Module-local implementation ------------------------------------------ //

CPX14RecSession::CPX14RecSession()
: CPX14SessionBase(false), m_magic(_magic),
   m_flags(PX14RECIMPF__DEFAULT),
   m_hBrdMainThread(PX14_INVALID_HANDLE),
   m_rec_status(PX14RECSTAT_IDLE), m_ss_bufp(NULL), m_ss_buf_samps(0),
   mt_bSnapshots(false), mt_rec_result(SIG_SUCCESS), mt_err_preamble(NULL),
   mt_sys_err_code(0), mt_xbuf1p(NULL), mt_xbuf2p(NULL),
   mt_time_armed(0), mt_time_ended(0)
{
   memset (&m_fil_params, 0, sizeof(PX14S_FILE_WRITE_PARAMS));
   m_fil_params.struct_size = sizeof(PX14S_FILE_WRITE_PARAMS);

   pthread_mutex_init(&m_mux, NULL);
}

CPX14RecSession::~CPX14RecSession()
{
   pthread_mutex_destroy(&m_mux);

   if (PX14_INVALID_HANDLE != m_hBrd)
   {
      if (m_flags & PX14RECIMPF_FREE_DMA_BUFFER)
      {
         if (mt_xbuf1p) FreeDmaBufferPX14(m_hBrd, mt_xbuf1p);
         if (mt_xbuf2p) FreeDmaBufferPX14(m_hBrd, mt_xbuf2p);
      }

      DisconnectFromDevicePX14(m_hBrd);
   }

   SIGASSERT_NULL_OR_POINTER(m_ss_bufp, px14_sample_t);
   delete m_ss_bufp;
}

int CPX14RecSession::ValidateRecHandle (HPX14RECORDING hRec,
                                        CPX14RecSession** ctxpp)
{//static

   CPX14RecSession* ctx_rawp;

   if (INVALID_HPX14RECORDING_HANDLE == hRec)
      return SIG_PX14_INVALID_OBJECT_HANDLE;
   ctx_rawp = reinterpret_cast<CPX14RecSession*>(hRec);
   SIGASSERT_POINTER(ctx_rawp, CPX14RecSession);
   if (ctx_rawp->m_magic != CPX14RecSession::_magic)
      return SIG_PX14_INVALID_OBJECT_HANDLE;

   SIGASSERT_NULL_OR_POINTER(ctxpp, CPX14RecSession*);
   if (ctxpp)
      *ctxpp = ctx_rawp;

   return SIG_SUCCESS;
}

int CPX14RecSession::EnsureSnapshotBuf (unsigned int samples)
{
   if (!m_ss_bufp || (m_ss_buf_samps < samples))
   {
      m_ss_buf_samps = 0;
      delete m_ss_bufp;
      m_ss_bufp = NULL;
   }

   try { m_ss_bufp = new px14_sample_t[samples]; }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }

   m_ss_buf_samps = samples;

   return SIG_SUCCESS;
}

/// Determine and return size of DMA transfer (in samples)
unsigned int CPX14RecSession::DetermineRecXferSize (unsigned int hint)
{
   unsigned int xfer_samps;

   if (hint)
   {
      if (hint < PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES)
         xfer_samps = PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES;
      else if (hint > PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES)
         xfer_samps = PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES;
      else
      {
         // Align down to DMA transfer frame size
         xfer_samps = PX14_ALIGN_DOWN_FAST(hint, PX14_ALIGN_DMA_XFER_FAST);
      }
   }
   else
      xfer_samps = _def_xfer_samps;

   return xfer_samps;
}

int CPX14RecSession::InitRecordingBuffers()
{
   unsigned xfer_samps;
   HPX14 hBrd;
   int res;

   hBrd = m_hBrdMainThread;

   SIGASSERT(0 == (m_rec_params.rec_flags & PX14RECSESF_DEEP_BUFFERING));

   if (NULL == m_rec_params.dma_bufp)
   {
      // User has not given us a DMA buffer to use

      xfer_samps = m_rec_params.xfer_samples;

      if (m_rec_params.rec_flags & PX14RECSESF_USE_UTILITY_BUFFERS)
      {
         SIGASSERT(xfer_samps > 0);

         // Use our utility buffers. These will not be freed after
         //  recording. This allows applications to reuse DMA buffers
         //  for subsequent recordings.

         res = EnsureUtilityDmaBufferPX14(hBrd, xfer_samps);
         PX14_RETURN_ON_FAIL(res);
         res = EnsureUtilityDmaBuffer2PX14(hBrd, xfer_samps);
         PX14_RETURN_ON_FAIL(res);

         GetUtilityDmaBufferPX14 (hBrd, &mt_xbuf1p, NULL);
         GetUtilityDmaBuffer2PX14(hBrd, &mt_xbuf2p, NULL);
      }
      else
      {
         // Allocate temporary buffers that we'll free after recording

         res = AllocateDmaBufferPX14(hBrd, xfer_samps, &mt_xbuf1p);
         PX14_RETURN_ON_FAIL(res);
         res = AllocateDmaBufferPX14(hBrd, xfer_samps, &mt_xbuf2p);
         PX14_RETURN_ON_FAIL(res);

         m_flags |= PX14RECIMPF_FREE_DMA_BUFFER;
      }
   }
   else
   {
      // User has given us a DMA buffer to use. Our recording implementation
      //  uses two buffers for recording, split given buffer into two virtual
      //  buffers.

      xfer_samps = m_rec_params.xfer_samples;
      if (xfer_samps == 0)
         xfer_samps = m_rec_params.dma_buf_samples >> 1;
      xfer_samps = PX14_ALIGN_DOWN_FAST(xfer_samps, PX14_ALIGN_DMA_XFER_FAST);
      if (0 == xfer_samps)
         return SIG_INVALIDARG;
      m_rec_params.xfer_samples = xfer_samps;

      mt_xbuf1p = m_rec_params.dma_bufp;
      mt_xbuf2p = m_rec_params.dma_bufp + xfer_samps;
   }

   SIGASSERT_POINTER(mt_xbuf1p, px14_sample_t);
   SIGASSERT_POINTER(mt_xbuf2p, px14_sample_t);

   return SIG_SUCCESS;
}

int CPX14RecSession::CopyRecParams (PX14S_REC_SESSION_PARAMS* rec_paramsp)
{
   int res;

   // Make a deep copy of user's input parameters
   res = DeepCopyRecParams(*rec_paramsp, m_rec_params);
   PX14_RETURN_ON_FAIL(res);
   if (rec_paramsp->filwp)
   {
      res = DeepCopyFileWriteParams(*rec_paramsp->filwp, m_fil_params);
      PX14_RETURN_ON_FAIL(res);

      m_rec_params.filwp->flags_out = 0;

      // Move pathname pointers to known storage
      if (m_fil_params.pathname)
      {
         m_pathname.assign(m_fil_params.pathname);
         m_fil_params.pathname = m_pathname.c_str();
      }
      if (m_fil_params.pathname2)
      {
         m_pathname2.assign(m_fil_params.pathname2);
         m_fil_params.pathname2 = m_pathname2.c_str();
      }
   }

   return SIG_SUCCESS;
}

int CPX14RecSession::CreateSession (HPX14 hBrd,
                                    PX14S_REC_SESSION_PARAMS* rec_paramsp)
{
   int res;

   mt_rec_result = SIG_SUCCESS;
   mt_err_preamble = NULL;
   m_bStopRecPlease = false;
   m_elapsed_time_ms = 0;
   m_samps_recorded = 0;
   m_xfer_count = 0;
   m_ss_buf_valid = 0;
   m_ss_counter = 0;

   // Use a different device handle for the recording thread
   res = DuplicateHandlePX14(hBrd, &m_hBrd);
   PX14_RETURN_ON_FAIL(res);
   m_hBrdMainThread = hBrd;
   // Copy recording parameters
   res = CopyRecParams(rec_paramsp);
   PX14_RETURN_ON_FAIL(res);

   // Allocate snapshot buffer if necessary
   mt_bSnapshots = (0 != (m_rec_params.rec_flags & PX14RECSESF_DO_SNAPSHOTS));
   if (mt_bSnapshots)
   {
      res = EnsureSnapshotBuf(m_rec_params.ss_len_samples);
      PX14_RETURN_ON_FAIL(res);

      mt_ss_by_xfers = (0 != m_rec_params.ss_period_xfer);
      mt_ss_cur = 0;
   }
   // Figure out what our transfer size should be
   m_rec_params.xfer_samples = DetermineRecXferSize(rec_paramsp->xfer_samples);
   if ((m_rec_params.xfer_samples < PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES) ||
       (m_rec_params.xfer_samples > PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES))
   {
      SIGASSERT(PX14_FALSE);
      return SIG_PX14_UNEXPECTED;
   }
   // Initialize DMA transfer buffer(s)
   res = InitRecordingBuffers();
   PX14_RETURN_ON_FAIL(res);

   m_sync_rec_thread.ClearEvent();
   m_sync_arm.ClearEvent();

   res = PreThreadCreate();
   PX14_RETURN_ON_FAIL(res);

   // Create and sync with recording thread. This mutex is held for the
   //  duration of the recording thread. (Except when waiting for cond)
   res = pthread_create(&m_thread_rec, NULL, th_raw, this);
   if (res != 0)
      return SIG_PX14_THREAD_CREATE_FAILURE;
   m_flags |= PX14RECIMPF_REC_THREAD_CREATED;

   // Synchronize with thread startup; condition set when thread ready for arming
   m_sync_rec_thread.WaitEvent();

   // We'll reuse m_sync_rec_thread for end-of-thread sync
   m_sync_rec_thread.ClearEvent();

   res = DoInitialProgressCheck();
   PX14_RETURN_ON_FAIL(res);

   // Arm recording if necessary
   if (0 == (m_rec_params.rec_flags & PX14RECSESF_DO_NOT_ARM))
   {
      res = Arm();
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

int CPX14RecSession::PreThreadCreate()
{
   // Intended for subclasses
   return SIG_SUCCESS;
}

int CPX14RecSession::Arm()
{
   if ((0 == (m_flags & PX14RECIMPF_REC_THREAD_CREATED)) ||
       (0 != (m_flags & PX14RECIMPF_REC_THREAD_ARMED)))
   {
      return SIG_PX14_REC_SESSION_CANNOT_ARM;
   }

   // Arm the recording session
   m_sync_arm.SetEvent();
   m_flags |= PX14RECIMPF_REC_THREAD_ARMED;

   return SIG_SUCCESS;
}

int CPX14RecSession::Abort()
{
   bool bThreadLocked;
   int res;

   if (0 == (m_flags & PX14RECIMPF_REC_THREAD_CREATED))
      return SIG_SUCCESS;		// Recording threads not running

   // Ask thread nicely
   m_bStopRecPlease = true;
   bThreadLocked = false;

   // Thread will need to be armed to respond
   if (0 == (m_flags & PX14RECIMPF_REC_THREAD_ARMED))
      Arm();

   res = m_sync_rec_thread.WaitEvent(_rec_timeout_ms);
   if (SIG_PX14_TIMED_OUT == res)
   {
      // Wait timed out. Board may be waiting for a trigger or a DMA
      //  transfer may be hung up. We'll abort the current operation
      //  by putting the board back into standby mode
      SetOperatingModePX14(m_hBrdMainThread, PX14MODE_STANDBY);

      // Now try again.
      res = m_sync_rec_thread.WaitEvent(_rec_timeout_ms);
      if (SIG_PX14_TIMED_OUT == res)
      {
         // Something is really stuck now
#ifdef _WIN32
         OutputDebugString(_T("PX14400: Recording thread has locked up\n"));
#endif
         pthread_cancel(m_thread_rec);
         bThreadLocked = true;
      }
   }

   if (!bThreadLocked)
      pthread_join(m_thread_rec, NULL);

   m_flags &= ~(PX14RECIMPF_REC_THREAD_ARMED | PX14RECIMPF_REC_THREAD_CREATED);

   return SIG_SUCCESS;
}

int CPX14RecSession::Progress (PX14S_REC_SESSION_PROG* progp, unsigned flags)
{
   pthread_mutex_lock(&m_mux);
   {
      progp->status = m_rec_status;
      if (progp->status == PX14RECSTAT_ERROR)
      {
         if (flags & PX14RECPROGF_NO_ERROR_TEXT)
            progp->err_textp = NULL;
         else
         {
            std::string errStr;
            if (mt_sys_err_code)
               SetSystemErrorVal(mt_sys_err_code);
            GetErrorTextStringPX14(mt_rec_result, errStr, 0, m_hBrd);
            if (mt_err_preamble)
               errStr.insert(0, mt_err_preamble);
            AllocAndCopyString(errStr.c_str(), &progp->err_textp);
         }
         progp->err_res = mt_rec_result;
      }

      progp->elapsed_time_ms  = m_elapsed_time_ms;
      progp->samps_recorded   = m_samps_recorded;
      progp->samps_to_record  = m_rec_params.rec_samples;
      progp->xfer_samples     = m_rec_params.xfer_samples;
      progp->xfer_count       = m_xfer_count;
      progp->snapshot_counter = m_ss_counter;
   }
   pthread_mutex_unlock(&m_mux);

   return SIG_SUCCESS;
}

int CPX14RecSession::Snapshot (px14_sample_t* bufp,
                               unsigned int samples,
                               unsigned int* samples_gotp,
                               unsigned int* ss_countp)
{
   unsigned int samples_got;

   if (0 == (m_rec_params.rec_flags & PX14RECSESF_DO_SNAPSHOTS))
      return SIG_PX14_SNAPSHOTS_NOT_ENABLED;

   pthread_mutex_lock(&m_mux);
   {
      if (bufp && samples && m_ss_bufp && m_ss_buf_samps)
      {
         samples_got = PX14_MIN(samples, m_ss_buf_samps);
         memcpy(bufp, m_ss_bufp, samples_got * sizeof(px14_sample_t));
      }
      else
         samples_got = 0;

      if (samples_gotp)
         *samples_gotp = samples_got;
      if (ss_countp)
         *ss_countp = m_ss_counter;
   }
   pthread_mutex_unlock(&m_mux);

   return SIG_SUCCESS;
}

int CPX14RecSession::DoInitialProgressCheck()
{
   PX14S_REC_SESSION_PROG rec_prog;

   // Do a progress check to see that the recording thread initialized
   //  okay and that it's ready to go.
   rec_prog.struct_size = sizeof(PX14S_REC_SESSION_PROG);
   Progress(&rec_prog, 0);
   if (rec_prog.status != PX14RECSTAT_ERROR)
      return SIG_SUCCESS;

   // There was an error during initialization
   Abort();

   SetErrorExtra(m_hBrdMainThread, rec_prog.err_textp);
   return SIG_PX14_REC_SESSION_ERROR;
}

// Library-export function implementation ------------------------------- //

/** @brief Arm device for recording
*/
PX14API ArmRecordingSessionPX14 (HPX14RECORDING hRec)
{
   CPX14RecSession* ctxp;
   int res;

   if (CPX14SessionBase::IsRemoteSessionHandle(hRec))
      return rmc_ArmRecordingSession(hRec);

   res = CPX14RecSession::ValidateRecHandle(hRec, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   return ctxp->Arm();
}

/** @brief Abort the current recording session
*/
PX14API AbortRecordingSessionPX14 (HPX14RECORDING hRec)
{
   CPX14RecSession* ctxp;
   int res;

   if (CPX14SessionBase::IsRemoteSessionHandle(hRec))
      return rmc_AbortRecordingSession(hRec);

   res = CPX14RecSession::ValidateRecHandle(hRec, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   return ctxp->Abort();
}

PX14API DeleteRecordingSessionPX14 (HPX14RECORDING hRec)
{
   CPX14RecSession* ctxp;
   int res;

   if (CPX14SessionBase::IsRemoteSessionHandle(hRec))
      return rmc_DeleteRecordingSession(hRec);

   res = CPX14RecSession::ValidateRecHandle(hRec, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   // Stop current recording if necessary
   ctxp->Abort();

   delete ctxp;
   return SIG_SUCCESS;
}

/** @brief Create a PX14 acquisition recording session

  When the session is no longer needed, the DeleteRecordingSessionPX14
  function should be called to free resources allocated for the recording
  session.

  @param hBrd
      A handle to the PX14 device that will be recorded. This handle is
      obtained by calling ConnectToDevicePX14
  @param rec_paramsp
      A pointer to a PX14S_REC_SESSION_PARAMS structure that define the
      parameters of the data recording session
  @param handlep
      A pointer to a HPX14RECORDING variable that will receive a handle
      that identifies the recording session.
  */
PX14API CreateRecordingSessionPX14 (HPX14 hBrd,
                                    PX14S_REC_SESSION_PARAMS* rec_paramsp,
                                    HPX14RECORDING* handlep)
{
   CPX14RecSession* ses_ctxp;
   CStatePX14* brd_ctxp;
   int res, rec_type;

   PX14_ENSURE_POINTER(hBrd, rec_paramsp, PX14S_REC_SESSION_PARAMS, "CreateRecordingSessionPX14");
   PX14_ENSURE_POINTER(hBrd, handlep, HPX14RECORDING, "CreateRecordingSessionPX14");
   PX14_ENSURE_STRUCT_SIZE(hBrd, rec_paramsp, _PX14SO_REC_SESSION_PARAMS_V1, "PX14S_REC_SESSION_PARAMS");

   res = ValidateHandle(hBrd, &brd_ctxp);
   PX14_RETURN_ON_FAIL(res);

   if (IsDeviceRemotePX14(hBrd))
      return rmc_CreateRecordingSession(hBrd, rec_paramsp, handlep);

   try
   {
      rec_type = rec_paramsp->rec_flags & PX14RECSESF_REC__MASK;
      if (rec_type == PX14RECSESF_REC_RAM_ACQ)
      {
         ses_ctxp = new CPX14RecSes_RamAcq();
      }
      else if (rec_type == PX14RECSESF_REC_PCI_ACQ)
      {
         if (rec_paramsp->rec_flags & PX14RECSESF_DEEP_BUFFERING)
            ses_ctxp = new CPX14RecSes_PciBufChained();
         else
            ses_ctxp = new CPX14RecSes_PciBuf();
      }
      else
         ses_ctxp = NULL;
   }
   catch (std::bad_alloc)
   {
      return SIG_OUTOFMEMORY;
   }
   if (NULL == ses_ctxp)
      return SIG_INVALIDARG;

   res = ses_ctxp->CreateSession(hBrd, rec_paramsp);
   if (SIG_SUCCESS != res)
   {
      delete ses_ctxp;
      return res;
   }

   *handlep = reinterpret_cast<HPX14RECORDING>(ses_ctxp);
   return SIG_SUCCESS;
}

// Obtain recording session output flags; only valid after recording stopped
PX14API GetRecordingSessionOutFlagsPX14 (HPX14RECORDING hRec,
                                         unsigned int* flagsp)
{
   CPX14RecSession* ctxp;
   int res;

   SIGASSERT_POINTER(flagsp, unsigned int);
   if (NULL == flagsp)
      return SIG_PX14_INVALID_ARG_2;

   if (CPX14SessionBase::IsRemoteSessionHandle(hRec))
      return rmc_GetRecordingSessionOutFlags(hRec, flagsp);

   res = CPX14RecSession::ValidateRecHandle(hRec, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   *flagsp = ctxp->GetOutFlags();

   return SIG_SUCCESS;
}


/** @brief Obtain progress/status for current recording session

  @param hRec
  A handle to a PX14 recording session. This handle is obtained by
  calling CreateRecordingSessionPX14
  @param progp
  A pointer to a PX14S_REC_SESSION_PROG structure that will receive
  the current progress/status of the PX14 recording session. The
  caller should initialize the struct_size field of this structure
  before calling this function.
  @param flags
  A set of flags (PX14RECPROGF_*) that control function behavior

*/
PX14API GetRecordingSessionProgressPX14 (HPX14RECORDING hRec,
                                         PX14S_REC_SESSION_PROG* progp,
                                         unsigned flags)
{
   CPX14RecSession* ctxp;
   int res;

   PX14_ENSURE_POINTER(PX14_INVALID_HANDLE, progp, PX14S_REC_SESSION_PROG, NULL);
   PX14_ENSURE_STRUCT_SIZE(PX14_INVALID_HANDLE, progp, _PX14SO_REC_SESSION_PROG_V1, NULL);

   if (CPX14SessionBase::IsRemoteSessionHandle(hRec))
      return rmc_GetRecordingSessionProgress(hRec, progp, flags);
   res = CPX14RecSession::ValidateRecHandle(hRec, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   return ctxp->Progress(progp, flags);
}

/** @brief Obtain data snapshot from current recording

  @param hRec
  A handle to a PX14 recording session. This handle is obtained by
  calling CreateRecordingSessionPX14
  @param bufp
  A pointer to a buffer that will receive the recording snapshot data
  @param samples
  The size, in samples, of the buffer pointed to by the bufp parameter
  @param samples_gotp
  The address of an unsigned int variable that will receive the number
  of samples copied into the snapshot buffer. Pass NULL if this
  information is not needed
  @param ss_countp
  The address of an unsigned int variable that will receive the
  snapshot counter value. Each data snapshot taken by the recording
  is given a unique counter value. This allows client software to
  differentiate between unique data snapshots. Pass NULL if this
  information is not needed
  */
PX14API GetRecordingSnapshotPX14 (HPX14RECORDING hRec,
                                  px14_sample_t* bufp,
                                  unsigned int samples,
                                  unsigned int* samples_gotp,
                                  unsigned int* ss_countp)
{
   CPX14RecSession* ctxp;
   int res;

   SIGASSERT_NULL_OR_POINTER(samples_gotp, unsigned int);
   SIGASSERT_NULL_OR_POINTER(ss_countp, unsigned int);
   SIGASSERT_NULL_OR_POINTER(bufp, px14_sample_t);

   if (CPX14SessionBase::IsRemoteSessionHandle(hRec))
   {
      return rmc_GetRecordingSnapshot(hRec, bufp, samples,
                                      samples_gotp, ss_countp);
   }
   res = CPX14RecSession::ValidateRecHandle(hRec, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   return ctxp->Snapshot(bufp, samples, samples_gotp, ss_countp);
}

// Module-local function implementation --------------------------------- //

int DeepCopyRecParams (const PX14S_REC_SESSION_PARAMS& src,
                       PX14S_REC_SESSION_PARAMS& dst)
{
   size_t ss;

   PX14_ENSURE_STRUCT_SIZE(PX14_INVALID_HANDLE, &src, _PX14SO_REC_SESSION_PARAMS_V1, NULL);

   ss = PX14_MIN(sizeof(PX14S_REC_SESSION_PARAMS), src.struct_size);
   memcpy (&dst, &src, ss);
   dst.struct_size = static_cast<unsigned int>(ss);
   return SIG_SUCCESS;
}

int DeepCopyFileWriteParams (const PX14S_FILE_WRITE_PARAMS& src,
                             PX14S_FILE_WRITE_PARAMS& dst)
{
   size_t ss;

   PX14_ENSURE_STRUCT_SIZE(PX14_INVALID_HANDLE, &src, _PX14SO_FILE_WRITE_PARAMS_V1, NULL);

   ss = PX14_MIN(sizeof(PX14S_FILE_WRITE_PARAMS), src.struct_size);
   memcpy (&dst, &src, ss);
   dst.struct_size = static_cast<unsigned int>(ss);
   return SIG_SUCCESS;
}

