/** @file	px14_timestamp.cpp
  @brief	Implementation of PX14 timestamp routines
  */
#include "stdafx.h"
#include "px14_top.h"
#include "px14_timestamp.h"

// CTimestampMgrPX14 implementation  ------------------------------------- //

CTimestampMgrPX14::CTimestampMgrPX14() : m_ts_flags(0),
   m_hBrdMain(PX14_INVALID_HANDLE), m_imp_flags(TSMGRF__DEFAULT),
   m_bStopPlease(false), mt_hBrd(PX14_INVALID_HANDLE),
   mt_ts_result(SIG_SUCCESS), mt_err_preamble(NULL), mt_filp_bin(NULL),
   mt_ts_save_count(0), mt_bFifoOverflow(false),
   m_ts_status(PX14RECSTAT_IDLE)
{
   pthread_mutex_init(&m_mux, NULL);
}

CTimestampMgrPX14::~CTimestampMgrPX14()
{
   Cleanup();

   pthread_mutex_destroy(&m_mux);

   if (PX14_INVALID_HANDLE != mt_hBrd)
      DisconnectFromDevicePX14(mt_hBrd);
}

int CTimestampMgrPX14::GetThreadStatus()
{
   int res;

   pthread_mutex_lock(&m_mux);
   {
      res = m_ts_status;
   }
   pthread_mutex_unlock(&m_mux);

   return res;
}

int CTimestampMgrPX14::Init (HPX14 hBrd,
                             const char* ts_pathnamep,
                             unsigned int flags)
{
   int res;

   SIGASSERT (PX14_INVALID_HANDLE != hBrd);
   SIGASSERT_POINTER(ts_pathnamep, char);
   if ((!ts_pathnamep) || (PX14_INVALID_HANDLE == hBrd))
      return SIG_INVALIDARG;
   m_ts_filename.assign(ts_pathnamep);
   m_ts_flags = flags;
   m_hBrdMain = hBrd;

   m_sync_ts_quit.ClearEvent();
   m_sync_ts_start.ClearEvent();
   m_sync_ts_arm.ClearEvent();

   // Create and synchronize with timestamp thread. We'll still need to
   //  arm it before it'll start grabbing timestamps
   res = pthread_create(&m_thread_ts, NULL, raw_ts_thread_func, this);
   if (0 != res)
      return SIG_PX14_THREAD_CREATE_FAILURE;
   m_imp_flags |= TSMGRF_THREAD_CREATED;
   // Synch thread startup; condition set when thread ready for arming
   m_sync_ts_start.WaitEvent();
   m_sync_ts_start.ClearEvent();

   // Check to see that thread started up okay
   res = (GetThreadStatus() == PX14RECSTAT_IN_PROGRESS)
      ? SIG_SUCCESS : mt_ts_result;
   PX14_RETURN_ON_FAIL(res);

   // At this point, the timestamp thread has been created and initialized
   //  an is waiting for one last poke (what we're calling "arming" here)
   //  before it starts reading timestamps from the timestamp FIFO.

   return SIG_SUCCESS;
}

int CTimestampMgrPX14::ArmTimestampThread()
{
   SIGASSERT(0 != (m_imp_flags & TSMGRF_THREAD_CREATED));
   if (0 == (m_imp_flags & TSMGRF_THREAD_CREATED))
      return SIG_PX14_UNEXPECTED;

   if (0 == (m_imp_flags & TSMGRF_THREAD_ARMED))
   {
      // Arm the recording session
      m_sync_ts_arm.SetEvent();
      m_imp_flags |= TSMGRF_THREAD_ARMED;
   }

   return SIG_SUCCESS;
}

void CTimestampMgrPX14::StopTimestampThread()
{
   Cleanup();
}

void CTimestampMgrPX14::Cleanup()
{
   //bool bThreadLocked;
   int res;

   if (m_imp_flags & TSMGRF_THREAD_CREATED)
   {
      //bThreadLocked = false;

      // Access to this volatile member isn't explicitly synchronized;
      //  we're the only writer, timestamp thread is the only reader
      m_bStopPlease = true;

      // Ask timestamp thread to quit
      m_sync_ts_quit.SetEvent();

      if (0 == (m_imp_flags & TSMGRF_THREAD_ARMED))
         ArmTimestampThread();

      // We've asked nicely; wait a bit to see that thread ends cleanly
      res = m_sync_ts_start.WaitEvent(3000);
      if (SIG_PX14_TIMED_OUT == res)
      {
         // Something is really stuck now.
#ifdef _WIN32
         OutputDebugString(
                           _T("PX14400: Timestamp thread appears to have locked up\n"));
#endif
         pthread_cancel(m_thread_ts);
      }
      else
         pthread_join(m_thread_ts, NULL);

      m_imp_flags &= ~TSMGRF_THREAD_CREATED;
   }
}

void* CTimestampMgrPX14::raw_ts_thread_func(void* paramp)
{
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
   pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

   CTimestampMgrPX14* thisp = reinterpret_cast<CTimestampMgrPX14*>(paramp);
   SIGASSERT_POINTER(thisp, CTimestampMgrPX14);
   if (!thisp) return NULL;

   // Run real timestamp thread
   thisp->mt_ts_result = thisp->TimestampThread();

   pthread_mutex_lock(&thisp->m_mux);
   {
      if (thisp->m_ts_status == PX14RECSTAT_IN_PROGRESS)
      {
         if (SIG_SUCCESS == thisp->mt_ts_result)
            thisp->m_ts_status = PX14RECSTAT_COMPLETE;
         else
            thisp->m_ts_status = PX14RECSTAT_ERROR;
      }
      else if (thisp->m_ts_status == PX14RECSTAT_IDLE)
         thisp->m_ts_status = PX14RECSTAT_ERROR;
   }
   pthread_mutex_unlock(&thisp->m_mux);

   // Synchronize end of thread with main thread
   thisp->m_sync_ts_start.SetEvent();

   return NULL;
}

int CTimestampMgrPX14::TimestampThread()
{
   static const unsigned def_ts_buf_size = 4096;
   static const unsigned def_sleep_ms = 250;
   static const px14_timestamp_t ts_overflow_marker[2] =
   { 0xF1F0F1F0F1F0F1F0ULL, 0xF1F0F1F0F1F0F1F0ULL };

   pmfIoCleanup_t pmfIoCleanup;
   pmfIoInit_t pmfIoInit;
   pmfIoDump_t pmfIoDump;

   unsigned int ts_read_out_flags, ts_in_flags, ts_got, ts_buf_size;
   px14_timestamp_t* ts_bufp;
   int res;

   // Figure out size of our timestamp buffer. We want it to be at
   //  least as large as the onboard timestamp FIFO
   ts_buf_size = def_ts_buf_size;
   res = GetTimestampFifoDepthPX14(m_hBrdMain, &ts_in_flags);
   if ((SIG_SUCCESS == res) && (ts_in_flags > ts_buf_size))
      ts_buf_size = ts_in_flags;

   CAutoIoCleanup auto_io_cleanup(*this);

   pmfIoInit = NULL;
   pmfIoDump = NULL;
   ts_bufp = NULL;

   do
   {
      // Duplicate PX14 device handle; they're not thread safe
      res = DuplicateHandlePX14(m_hBrdMain, &mt_hBrd);
      if (SIG_SUCCESS != res)
      {
         mt_err_preamble = "Failed to duplicate PX14 handle: ";
         mt_ts_result = res;
         break;
      }

      // Determine how we're writing data (text/binary)
      if (m_ts_flags & PX14FILWF_TIMESTAMPS_AS_TEXT)
      {
         pmfIoInit    = &CTimestampMgrPX14::_IoInit_Text;
         pmfIoDump    = &CTimestampMgrPX14::_IoDump_Text;
         pmfIoCleanup = &CTimestampMgrPX14::_IoCleanup_Text;
      }
      else
      {
         pmfIoInit    = &CTimestampMgrPX14::_IoInit_Binary;
         pmfIoDump    = &CTimestampMgrPX14::_IoDump_Binary;
         pmfIoCleanup = &CTimestampMgrPX14::_IoCleanup_Binary;
      }

      // Ready output file
      res = (this->*pmfIoInit)();
      if (SIG_SUCCESS != res)
      {
         mt_err_preamble =
            "Failed to create/open timestamp output file: ";
         mt_ts_result = res;
         break;
      }
      auto_io_cleanup.ReInit(pmfIoCleanup);

      // Allocate our local timestamp buffer
      try { ts_bufp = new px14_timestamp_t[ts_buf_size]; }
      catch (std::bad_alloc)
      {
         mt_err_preamble = "Failed to allocate timestamp buffer: ";
         mt_ts_result = SIG_OUTOFMEMORY;
         break;
      }

      mt_ts_result = SIG_SUCCESS;

   } while (0);

   // Free timestamp buffer on exit
   std::auto_ptr<px14_timestamp_t> autoptr_ts_buf(ts_bufp);

   // Synchronize thread state now; main thread will check on things
   //  after we synch below
   pthread_mutex_lock(&m_mux);
   {
      m_ts_status = (SIG_SUCCESS != mt_ts_result)
         ? PX14RECSTAT_ERROR : PX14RECSTAT_IN_PROGRESS;
   }
   pthread_mutex_unlock(&m_mux);

   // Signal main thread that we're created and ready
   m_sync_ts_start.SetEvent();

   // Wait for main thread to tell us it's cool to arm. This
   //  synchronization is needed to prevent us from reading timestamp FIFO
   //  data too early (perhaps we haven't started an acquisition yet).
   m_sync_ts_arm.WaitEvent();

   ts_in_flags = 0;

   // Read timestamps until we're asked to quit
   while (!m_bStopPlease)
   {
      if (GetTimestampAvailabilityPX14(mt_hBrd) > 0)
      {
         res = ReadTimestampDataPX14(mt_hBrd, ts_bufp, ts_buf_size, &ts_got,
                                     ts_in_flags, 0, &ts_read_out_flags);
         if (SIG_SUCCESS != res)
         {
            mt_err_preamble = "Failed to read timestamp FIFO: ";
            return res;
         }

         // Did we get any timestamps?
         if (!ts_got)
         {
            // Was it because FIFO is full?
            if (ts_read_out_flags & PX14TSREAD_FIFO_OVERFLOW)
            {
               // Set flag such that next iteration will read from a
               //  known FULL timestamp FIFO.
               ts_in_flags |= PX14TSREAD_READ_FROM_FULL_FIFO;
               mt_bFifoOverflow = true;
               continue;
            }
         }
         else
         {
            mt_ts_save_count += ts_got;

            // Dump timestamps
            res = (this->*pmfIoDump)(ts_bufp, ts_got);
            if (SIG_SUCCESS != res)
            {
               mt_err_preamble = "Failed to write timestamp data: ";
               return res;
            }

            // If this flag is set then we just read from a full ts FIFO
            //  We intentionally size our timestamp buffer to be able to
            //  contain the full FIFO size, so we should have just read
            //  all valid timestamp info. At some point after the last
            //  element we may have missed a number of timestamps. For
            //  this situation, we have the option of writing a "FIFO
            //  overflow marker" into the timestamp stream so this can
            //  programmatically handled if desired.
            if (ts_in_flags & PX14TSREAD_READ_FROM_FULL_FIFO)
            {
               // Reset our 'read from full FIFO' bit since we've
               // just made room in the FIFO.
               ts_in_flags &= ~PX14TSREAD_READ_FROM_FULL_FIFO;

               // Insert timestamp FIFO overflow
               if (m_ts_flags & PX14FILWF_USE_TS_FIFO_OVFL_MARKER)
               {
                  res = (this->*pmfIoDump)(ts_overflow_marker, 2);
                  if (SIG_SUCCESS != res)
                  {
                     mt_err_preamble =
                        "Failed to write timestamp data: ";
                     return res;
                  }
               }
            }
         }

         if (ts_read_out_flags & PX14TSREAD_MORE_AVAILABLE)
            continue;
      }

      // We use a condition object to do waiting so we can promptly
      // respond to a quit request.
      res = m_sync_ts_quit.WaitEvent(def_sleep_ms);
      if (0 == res)
      {
         // Main thread is asking us to quit
         break;
      }
   }

   return SIG_SUCCESS;
}

/// Returns true if timestamp FIFO has overflowed at any point
bool CTimestampMgrPX14::TimestampFifoOverflowed()
{
   return mt_bFifoOverflow;
}

/// Returns total number of timestamps processed; not threadsafe
unsigned CTimestampMgrPX14::GetTimestampCount()
{
   return mt_ts_save_count;
}

int CTimestampMgrPX14::_IoInit_Binary()
{
   const char* fmodep;

   SIGASSERT(0 == (m_ts_flags & PX14FILWF_TIMESTAMPS_AS_TEXT));

   fmodep = (m_ts_flags & PX14FILWF_APPEND) ? "a" : "w";
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
   errno_t eres;
   eres = fopen_s(&mt_filp_bin, m_ts_filename.c_str(), fmodep);
   if (0 != eres)
      return SIG_PX14_FILE_IO_ERROR;
#else
   mt_filp_bin = fopen(m_ts_filename.c_str(), fmodep);
   if (NULL == mt_filp_bin)
      return SIG_PX14_FILE_IO_ERROR;
#endif

   return SIG_SUCCESS;
}

int CTimestampMgrPX14::_IoDump_Binary (const px14_timestamp_t* bufp,
                                       unsigned nItems)
{
   size_t items_written;

   items_written = fwrite(bufp, sizeof(px14_timestamp_t), nItems,
                          mt_filp_bin);

   return (items_written == nItems) ? SIG_SUCCESS : SIG_PX14_DISK_FULL;
}

int CTimestampMgrPX14::_IoCleanup_Binary()
{
   if (NULL != mt_filp_bin)
   {
      fclose(mt_filp_bin);
      mt_filp_bin = NULL;
   }

   return SIG_SUCCESS;
}

int CTimestampMgrPX14::_IoInit_Text()
{
   std::ios_base::openmode om;

   SIGASSERT(0 != (m_ts_flags & PX14FILWF_TIMESTAMPS_AS_TEXT));

   om = (m_ts_flags & PX14FILWF_APPEND)
      ? std::ios_base::app : std::ios_base::trunc;
   mt_file_txt.open(m_ts_filename.c_str(), std::ios_base::out | om);

   if (!mt_file_txt.is_open())
      return SIG_PX14_FILE_IO_ERROR;

   return SIG_SUCCESS;
}

int CTimestampMgrPX14::_IoDump_Text (const px14_timestamp_t* bufp,
                                     unsigned nItems)
{
   unsigned i;

   for (i=0; (i<nItems) && mt_file_txt; i++,bufp++)
      mt_file_txt << *bufp << std::endl;

   return mt_file_txt ? SIG_SUCCESS : SIG_PX14_FILE_IO_ERROR;
}

int CTimestampMgrPX14::_IoCleanup_Text()
{
   if (mt_file_txt.is_open())
      mt_file_txt.close();

   return SIG_SUCCESS;
}

// PX14 library exports implementation --------------------------------- //

/** @brief Manually reset timestamp counter

  The PX14 will automatically reset the timestamp counter at the start of
  each new acquisition (on Standby to acquisition operating mode change).
  */
PX14API ResetTimestampCounterPX14 (HPX14 hBrd)
{
   int res;

   // Pulse TSRES bit
   PX14U_REG_DEV_06 r6;
   r6.bits.TSRES = 1;
   res = WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_TS_RESET, r6.val);
   PX14_RETURN_ON_FAIL(res);

   r6.bits.TSRES = 0;
   return WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_TS_RESET, r6.val);
}

/** @brief Read Timestamp Overflow Flag from PX14 hardware
*/
PX14API GetTimestampOverflowFlagPX14 (HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xD, NULL, PX14REGREAD_HARDWARE);
   PX14_RETURN_ON_FAIL(res);

   return PX14_H2Bref(hBrd).m_regDev.fields.regD.bits.TSFIFO_OVFL ? 1 : 0;
}

/// Determine if any timestamps are available in timestamp FIFO
PX14API GetTimestampAvailabilityPX14 (HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xD, NULL, PX14REGREAD_HARDWARE);
   PX14_RETURN_ON_FAIL(res);

   return PX14_H2Bref(hBrd).m_regDev.fields.regD.bits.TSFIFO_EMPTY ? 0 : 1;
}

/// Reset the timestamp FIFO; all content lost
PX14API ResetTimestampFifoPX14 (HPX14 hBrd)
{
   int res;

   // Pulse TSFIFO_RES
   PX14U_REG_DEV_06 r6;
   r6.bits.TSFIFO_RES = 1;
   res = WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_TSFIFO_RESET, r6.val);
   PX14_RETURN_ON_FAIL(res);

   r6.bits.TSFIFO_RES = 0;
   return WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_TSFIFO_RESET, r6.val);
}

/// Set the timestamp mode; determines how timestamps are recorded
PX14API SetTimestampModePX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14TSMODE__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_06 r6;
   r6.bits.TS_MODE = val;
   return WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_TS_MODE, r6.val);
}

/// Get the timestamp mode; determines how timestamps are recorded
PX14API GetTimestampModePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 6);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg6.bits.TS_MODE);
}

// Set timestamp counter mode; determines how timestamp counter increments
PX14API SetTimestampCounterModePX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14TSCNTMODE__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_06 r6;
   r6.bits.TS_CNT_MODE = val;
   return WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_TS_CNT_MODE, r6.val);
}

// Get timestamp counter mode; determines how timestamp counter increments
PX14API GetTimestampCounterModePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 6);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg6.bits.TS_CNT_MODE);
}

PX14API GetTimestampFifoDepthPX14 (HPX14 hBrd, unsigned int* ts_elementsp)
{
   SIGASSERT_POINTER(ts_elementsp, unsigned int);
   if (NULL == ts_elementsp)
      return SIG_PX14_INVALID_ARG_2;

   // For now, all PX14s have the same timestamp FIFO depth, 2K elements
   *ts_elementsp = 2048;
   return SIG_SUCCESS;
}

/** @brief Read timestamp data from PX14 timestamp FIFO

  @param hBrd
  A handle to the PX14 board. This handle is obtained by calling the
  ConnectToDevicePX14 function.
  @param bufp
  A pointer to the buffer that will receive the timestamp values
  @param ts_count
  The size of the buffer pointed to by the bufp parameter, in timestamp
  elements
  @param ts_readp
  A pointer to an unsigned int variable that will receive the number of
  timestamps read from the timestamp FIFO. This parameter may not be
  NULL
  @param flags

*/
PX14API ReadTimestampDataPX14 (HPX14 hBrd,
                               px14_timestamp_t* bufp,
                               unsigned int ts_count,
                               unsigned int* ts_readp,
                               unsigned int flags,
                               unsigned int timeout_ms,
                               unsigned int* flags_outp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(flags_outp, unsigned int);
   SIGASSERT_POINTER(bufp, px14_timestamp_t);
   SIGASSERT_POINTER(ts_readp, unsigned int);
   if (NULL == bufp)
      return SIG_PX14_INVALID_ARG_2;
   if (0 == ts_count)
      return SIG_PX14_INVALID_ARG_3;
   if (NULL == ts_readp)
      return SIG_PX14_INVALID_ARG_4;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   PX14S_READ_TIMESTAMPS ctx;
   memset (&ctx, 0, sizeof(PX14S_READ_TIMESTAMPS));
   ctx.struct_size = sizeof(PX14S_READ_TIMESTAMPS);
   ctx.buffer_items = ts_count;
   ctx.flags = flags;
   ctx.timeout_ms = timeout_ms;
   ctx.user_virt_addr =
      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(bufp));

   if (statep->IsVirtual())
   {
      unsigned int i;

      // Just write some counting values; just want to test all of given
      //  buffer
      for (i=0; i<ts_count; i++,bufp++)
         *bufp = i << 3;
      *ts_readp = ts_count;

      if (flags_outp)
         *flags_outp = 0;

      return SIG_SUCCESS;
   }

   // Ask driver to read timestamps
   res = DeviceRequest(hBrd, IOCTL_PX14_READ_TIMESTAMPS, &ctx,
                       sizeof(PX14S_READ_TIMESTAMPS), sizeof(PX14S_READ_TIMESTAMPS));
   PX14_RETURN_ON_FAIL(res);

   *ts_readp = ctx.buffer_items;

   if (flags_outp)
   {
      *flags_outp &= ~PX14TSREAD__OUTPUT_FLAG_MASK;
      *flags_outp |= ctx.flags & PX14TSREAD__OUTPUT_FLAG_MASK;
   }

   return SIG_SUCCESS;
}

