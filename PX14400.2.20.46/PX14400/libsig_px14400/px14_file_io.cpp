/** @file	px14_file_io.cpp
  @brief	Code involving loading/saving data from/to storage
  */
#include "stdafx.h"
#include "px14_top.h"

// Module private function implementation  ------------------------------ //

#pragma region CIoSinkCtx_Base

   CIoSinkCtx_Base::CIoSinkCtx_Base()
: m_paramsp(NULL),  m_hBrd(PX14_INVALID_HANDLE),
   m_samps_to_move(0), m_samps_moved(0), m_pTsMgr(NULL),
   m_pSrdcCallback(NULL), m_srdcCallbackCtx(NULL)
{
}

CIoSinkCtx_Base::~CIoSinkCtx_Base()
{
   SIGASSERT_NULL_OR_POINTER (m_pTsMgr, CTimestampMgrPX14);
   delete m_pTsMgr;
}

int CIoSinkCtx_Base::Init (HPX14 hBrd,
                           unsigned long long total_samples_to_move,
                           PX14S_FILE_WRITE_PARAMS& params)
{
   int res;

   res = ValidateHandle(hBrd, NULL);
   PX14_RETURN_ON_FAIL(res);
   m_hBrd = hBrd;

   m_samps_to_move = total_samples_to_move;
   m_paramsp = &params;

   if ((params.struct_size > 0) &&
       (params.flags & PX14FILWF_SAVE_TIMESTAMPS))
   {
      res = ReadyTimestampState();
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

int CIoSinkCtx_Base::Write (px14_sample_t* bufp, unsigned int samples)
{
   int res;

   SIGASSERT_POINTER(bufp, px14_sample_t);
   if (NULL == bufp)
      return SIG_INVALIDARG;

   if (NULL != m_pTsMgr)
   {
      // Arm timestamp thread on our first time through here
      if (0 == m_samps_moved)
         m_pTsMgr->ArmTimestampThread();
      else
      {
         if (m_paramsp->flags & PX14FILWF_ABORT_OP_ON_TS_OVFL)
         {
            if (m_pTsMgr->TimestampFifoOverflowed())
               return SIG_PX14_TIMESTAMP_FIFO_OVERFLOW;
         }
      }
   }

   // Convert data to signed if necessary
   if (m_paramsp->flags & PX14FILWF_CONVERT_TO_SIGNED)
   {
      px14_sample_t* curp = bufp;
      register unsigned int i;

      for (i=0; i<samples; i++)
         *curp++ ^= 0x8000;
   }

   // Do user callback if necessary
   if (m_paramsp->pfnCallback)
   {
      res = (*m_paramsp->pfnCallback)(m_hBrd,
                                      m_paramsp->callbackCtx,
                                      bufp, samples,
                                      GetCurrentFilePath(),
                                      GetCurrentFileSamples(),
                                      m_samps_moved, m_samps_to_move);
      if (SIG_SUCCESS != res)
      {
         // User wants us to abort. This error value should be sent back
         // to top-level.
         return res;
      }
   }

   m_samps_moved += samples;
   return SIG_SUCCESS;
}

void CIoSinkCtx_Base::Release()
{
   if (m_pTsMgr)
   {
      m_pTsMgr->StopTimestampThread();
      if (m_pTsMgr->TimestampFifoOverflowed())
         m_paramsp->flags_out |= PX14FILWOUTF_TIMESTAMP_FIFO_OVERFLOW;
      if (m_pTsMgr->GetTimestampCount() == 0)
         m_paramsp->flags_out |= PX14FILWOUTF_NO_TIMESTAMP_DATA;
   }

   delete this;
}

const char* CIoSinkCtx_Base::GetCurrentFilePath()
{
   return m_paramsp ? m_paramsp->pathname : NULL;
}

unsigned long long CIoSinkCtx_Base::GetCurrentFileSamples()
{
   return m_samps_moved;
}

void CIoSinkCtx_Base::SetSrdcGenCallback (SrdcGenCallback_t pFunc, void* ctxp)
{
   m_pSrdcCallback = pFunc;
   m_srdcCallbackCtx = ctxp;
}

/** @brief Call any time a new PX14 data file is created
*/
int CIoSinkCtx_Base::OnNewOutputFileOpened (const char* pathnamep,
                                            int active_channels)
{
   int res;

   SIGASSERT_POINTER(pathnamep, char);
   if (NULL == pathnamep)
      return SIG_INVALIDARG;

   if (m_paramsp->flags & PX14FILWF_GENERATE_SRDC_FILE)
   {
      res = GenerateSrdFile(pathnamep, active_channels);
      PX14_RETURN_ON_FAIL(res);
   }
   if (m_paramsp->flags & PX14FILWF_EMBED_SRDC_AS_AFS)
   {
      res = GenerateSrdFile(pathnamep, active_channels, true);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

int CIoSinkCtx_Base::GenerateSrdFile (const char* pathnamep,
                                      int active_channels,
                                      bool bEmbedAsAfs)
{
   bool bVal;
   int res;
   HPX14SRDC hFile;

#ifndef _WIN32
   if (bEmbedAsAfs)
   {
      // Only Windows supports embedding SRDC data as AFS
      return SIG_PX14_NOT_IMPLEMENTED;
   }
#endif

   std::string srdPathname(pathnamep);

   if (bEmbedAsAfs)
      srdPathname.append(":SRDC");
   else
      srdPathname.append(PX14_SRDC_DOT_EXTENSIONA);

   // Open the SRDC file
   res = OpenSrdcFileAPX14(m_hBrd, &hFile, srdPathname.c_str());
   PX14_RETURN_ON_FAIL(res);
   CAutoSrdcHandle ash(hFile);		// Auto close SRDC file handle on exit

   // Refresh known parameters: channel counts, etc.
   res = RefreshSrdcParametersPX14(hFile);
   PX14_RETURN_ON_FAIL(res);

   // The RefreshSrdcParametersPX14 uses current board settings to set item
   //  values. Since we might be breaking things up (or saving as text) we
   //  may have to tweak some things. Also note that we might be
   //  overwriting an older file so don't assume that default values are
   //  present

   // Signed/unsigned
   bVal = (0 != (m_paramsp->flags & PX14FILWF_CONVERT_TO_SIGNED));
   res = SetSrdcItemAPX14(hFile, "SampleFormat",
                          bVal ? "Signed" : "Unsigned");

   if (m_paramsp->flags & PX14FILWF_ASSUME_DUAL_CHANNEL)
      active_channels = PX14CHANNEL_DUAL;
   else if (active_channels == -1)
      active_channels = GetActiveChannelsPX14(m_hBrd);
   SetSrdcItemAPX14(hFile, "ChannelId",
                    my_ConvertToString(active_channels).c_str());

   // If we're deinterleaving, then we'll have two one-channel files
   if (m_paramsp->flags & PX14FILWF_DEINTERLEAVE)
      SetSrdcItemAPX14(hFile, "ChannelCount", "1");

   // File format
   if (m_paramsp->flags & PX14FILWF_SAVE_AS_TEXT)
   {
      SetSrdcItemAPX14(hFile, "FileFormat", "Text");
      SetSrdcItemAPX14(hFile, "SampleRadix",
                       (m_paramsp->flags & PX14FILWF_HEX_OUTPUT)
                       ? "16" : "10");

      if (m_paramsp->flags & PX14FILWF_ASSUME_DUAL_CHANNEL)
         SetSrdcItemAPX14(hFile, "ChannelCount", "2");
   }
   else
   {
      // Saving as binary; remove any text-mode items
      SetSrdcItemAPX14(hFile, "FileFormat", NULL);
      SetSrdcItemAPX14(hFile, "SampleRadix", NULL);
   }

   // File header
   if (m_paramsp->init_bytes_skip)
   {
      SetSrdcItemAPX14(hFile, "HeaderBytes",
                       my_ConvertToString(m_paramsp->init_bytes_skip).c_str());
   }
   else
      SetSrdcItemAPX14(hFile, "HeaderBytes", NULL);

   // Operator notes
   std::string operator_notes;
   SIGASSERT_NULL_OR_POINTER(m_paramsp->operator_notes, char);
   if ((NULL != m_paramsp->operator_notes) &&
       (0    != m_paramsp->operator_notes[0]))
   {
      operator_notes.assign(m_paramsp->operator_notes);
   }
   SetSrdcItemAPX14(hFile, "OperatorNotes",
                    operator_notes.empty() ? NULL : operator_notes.c_str());

   // Give callback a stab at modifying or augmenting SRDC content
   if (m_pSrdcCallback)
   {
      res = (*m_pSrdcCallback)(hFile, m_srdcCallbackCtx);
      PX14_RETURN_ON_FAIL(res);
   }

   res = SaveSrdcFileAPX14(hFile, srdPathname.c_str());
   PX14_RETURN_ON_FAIL(res);

   // CAutoSrdcHandle will free handle
   return SIG_SUCCESS;
}

int CIoSinkCtx_Base::ReadyTimestampState()
{
   bool bSaveAsText;
   int res;

   SIGASSERT (0 != (m_paramsp->flags & PX14FILWF_SAVE_TIMESTAMPS));
   SIGASSERT (NULL == m_pTsMgr);

   bSaveAsText = 0 != (m_paramsp->flags & PX14FILWF_TIMESTAMPS_AS_TEXT);

   // Determine timestamp pathname
   std::string ts_pathname;
   if (m_paramsp->ts_filenamep)
      ts_pathname.assign(m_paramsp->ts_filenamep);
   TrimString(ts_pathname);
   if (ts_pathname.empty())
   {
      // No override specified, construct from given data output filenames
      if (m_paramsp->pathname && m_paramsp->pathname[0])
         ts_pathname.assign(m_paramsp->pathname);
      else if (m_paramsp->pathname2 && m_paramsp->pathname2[0])
         ts_pathname.assign(m_paramsp->pathname2);
      else
      {
         return SIG_INVALIDARG;
      }

      ts_pathname.append(PX14_TIMESTAMP_FILE_DOT_EXTENSIONA);
      if (bSaveAsText)
         ts_pathname.append(".txt");
   }

   // Create and initialize timestamp manager
   try { m_pTsMgr = new CTimestampMgrPX14; }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }
   res = m_pTsMgr->Init(m_hBrd, ts_pathname.c_str(), m_paramsp->flags);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

#pragma endregion CIoSinkCtx_Base implementation

#pragma region CIoSinkCtx_BinarySingleChannel

   CIoSinkCtx_BinarySingleChannel::CIoSinkCtx_BinarySingleChannel()
: m_sysOpenCookie(PX14_SYS_OPEN_COOKIE_INIT), m_fd(-1)
{
}

CIoSinkCtx_BinarySingleChannel::~CIoSinkCtx_BinarySingleChannel()
{
}

int CIoSinkCtx_BinarySingleChannel::Init (HPX14 hBrd,
                                          unsigned long long total_samples_to_move,
                                          PX14S_FILE_WRITE_PARAMS& params)
{
   int res, oflag;

   res = CIoSinkCtx_Base::Init(hBrd, total_samples_to_move, params);
   PX14_RETURN_ON_FAIL(res);

   if (params.pathname && params.pathname[0])
   {
      oflag = O_BINARY | O_SEQUENTIAL | O_WRONLY | O_CREAT | O_TRUNC;
      res = SysOpenFile(params.pathname, oflag, total_samples_to_move,
                        &params, m_fd, m_sysOpenCookie);
      PX14_RETURN_ON_FAIL(res);
      SIGASSERT(m_fd != -1);
      if (-1 == m_fd) return SIG_PX14_UNEXPECTED;

      res = OnNewOutputFileOpened(params.pathname, -1);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

int CIoSinkCtx_BinarySingleChannel::Write (px14_sample_t* bufp,
                                           unsigned int samples)
{
   int res;

   // Always call base class first
   res = CIoSinkCtx_Base::Write(bufp, samples);
   PX14_RETURN_ON_FAIL(res);

   if (-1 != m_fd)
   {
      res = sys_file_write(m_fd, bufp, samples * sizeof(px14_sample_t));
      if (-1 == res)
         return SIG_PX14_FILE_IO_ERROR;
      if (res != static_cast<int>(samples * sizeof(px14_sample_t)))
         return SIG_PX14_DISK_FULL;
   }

   return SIG_SUCCESS;
}

void CIoSinkCtx_BinarySingleChannel::Release()
{
   // Close files, etc.
   if (m_fd != -1)
   {
      sys_file_close(m_fd);
      m_fd = -1;
   }

   // Finish up with base class implementation which will delete this
   CIoSinkCtx_Base::Release();
}

#pragma endregion CIoSinkCtx_BinarySingleChannel implementation

#pragma region CIoSinkCtx_BinSingle_Seg

CIoSinkCtx_BinSingle_Seg::CIoSinkCtx_BinSingle_Seg() :
   m_samples_in_file(0), m_file_index(0),
   m_sysOpenCookie(PX14_SYS_OPEN_COOKIE_INIT), m_fd(-1)
{
}

CIoSinkCtx_BinSingle_Seg::~CIoSinkCtx_BinSingle_Seg()
{
}

int CIoSinkCtx_BinSingle_Seg::Init (HPX14 hBrd,
                                    unsigned long long total_samples_to_move,
                                    PX14S_FILE_WRITE_PARAMS& params)
{
   std::string::size_type pos;
   int res;

   res = CIoSinkCtx_Base::Init(hBrd, total_samples_to_move, params);
   PX14_RETURN_ON_FAIL(res);

   if (!params.pathname || !params.pathname[0])
   {
      SIGASSERT(PX14_FALSE);
      return SIG_PX14_UNEXPECTED;
   }

   m_sysOpenCookie = PX14_SYS_OPEN_COOKIE_INIT;
   m_file_index = 0;

   // Break given file pathname into a couple component pieces so we can
   //  quickly generate new, indexed filenames as necessary
   std::string pathname_base(params.pathname);
   pos = pathname_base.rfind('.');
   if (pos == std::string::npos)
   {
      m_pathname_no_ext = pathname_base;
      m_filename_ext.clear();
   }
   else
   {
      m_pathname_no_ext = pathname_base.substr(0, pos);
      m_filename_ext = pathname_base.substr(pos);
   }
   m_pathname_no_ext.append(1, '_');

   // Open first file. This will also setup m_sysOpenCookie cookie
   //  so we don't need to recalc file flags each time we create a
   //  file.
   return StartNextFile();
}

void CIoSinkCtx_BinSingle_Seg::NextFileName(std::string& p)
{
   p.assign(m_pathname_no_ext);
   p.append(my_ConvertToString(m_file_index));
   if (!m_filename_ext.empty())
      p.append(m_filename_ext);
}

int CIoSinkCtx_BinSingle_Seg::StartNextFile()
{
   unsigned long long samples_left_to_move;
   int res, oflag;

   // Close current file if necessary
   if (-1 != m_fd)
   {
      sys_file_close(m_fd);
      m_fd = -1;
   }

   // Generate next file name
   std::string pathname_new;
   NextFileName(pathname_new);
   m_file_index++;

   samples_left_to_move = (m_samps_to_move)
      ? m_samps_to_move - m_samps_moved : 0;

   // Open next file
   oflag = O_BINARY | O_SEQUENTIAL | O_WRONLY | O_CREAT | O_TRUNC;
   res = SysOpenFile(pathname_new.c_str(), oflag, samples_left_to_move,
                     m_paramsp, m_fd, m_sysOpenCookie);
   PX14_RETURN_ON_FAIL(res);

   res = OnNewOutputFileOpened(pathname_new.c_str(), false);
   PX14_RETURN_ON_FAIL(res);

   m_samples_in_file = 0;

   return SIG_SUCCESS;
}

int CIoSinkCtx_BinSingle_Seg::Write (px14_sample_t* bufp,
                                     unsigned int samples)
{
   unsigned int samples_to_write;
   int res;

   res = CIoSinkCtx_Base::Write(bufp, samples);
   PX14_RETURN_ON_FAIL(res);

   while (samples)
   {
      // How much should we write?
      samples_to_write = samples;
      if (m_samples_in_file + samples > m_paramsp->max_file_seg)
      {
         samples_to_write = static_cast<unsigned int>(
                                                      m_paramsp->max_file_seg - m_samples_in_file);
      }

      // Write it
      res = sys_file_write(m_fd, bufp,
                           samples_to_write * sizeof(px14_sample_t));
      if (-1 == res)
         return SIG_PX14_FILE_IO_ERROR;
      if (res != static_cast<int>(
                                  samples_to_write * sizeof(px14_sample_t)))
      {
         return SIG_PX14_DISK_FULL;
      }

      samples -= samples_to_write;

      // Start next file if necessary
      m_samples_in_file += samples_to_write;
      if ((m_samples_in_file >= m_paramsp->max_file_seg) &&
          (!m_samps_to_move || (m_samps_moved < m_samps_to_move)))
      {
         res = StartNextFile();
         PX14_RETURN_ON_FAIL(res);
      }
   }

   return SIG_SUCCESS;
}

void CIoSinkCtx_BinSingle_Seg::Release()
{
   // Close files, etc.
   if (m_fd != -1)
   {
      sys_file_close(m_fd);
      m_fd = -1;
   }

   CIoSinkCtx_Base::Release();
}

#pragma endregion CIoSinkCtx_BinSingle_Seg implementation

#pragma region CIoSinkCtx_BinaryDualFile

   CIoSinkCtx_BinaryDualFile::CIoSinkCtx_BinaryDualFile()
: m_sysOpenCookieCh1(PX14_SYS_OPEN_COOKIE_INIT), m_fdCh1(-1),
   m_sysOpenCookieCh2(PX14_SYS_OPEN_COOKIE_INIT), m_fdCh2(-1)
{
}

CIoSinkCtx_BinaryDualFile::~CIoSinkCtx_BinaryDualFile()
{
}

int CIoSinkCtx_BinaryDualFile::Init (HPX14 hBrd,
                                     unsigned long long total_samples_to_move,
                                     PX14S_FILE_WRITE_PARAMS& params)
{
   int res, oflag;

   res = CIoSinkCtx_Base::Init(hBrd, total_samples_to_move, params);
   PX14_RETURN_ON_FAIL(res);

   if (params.pathname && params.pathname[0])
   {
      oflag = O_BINARY | O_SEQUENTIAL | O_WRONLY | O_CREAT | O_TRUNC;
      res = SysOpenFile(params.pathname, oflag, total_samples_to_move / 2,
                        &params, m_fdCh1, m_sysOpenCookieCh1);
      PX14_RETURN_ON_FAIL(res);
      SIGASSERT(m_fdCh1 != -1);
      if (-1 == m_fdCh1) return SIG_PX14_UNEXPECTED;

      res = OnNewOutputFileOpened(params.pathname, PX14CHANNEL_ONE);
      PX14_RETURN_ON_FAIL(res);
   }

   if (params.pathname2 && params.pathname2[0])
   {
      oflag = O_BINARY | O_SEQUENTIAL | O_WRONLY | O_CREAT | O_TRUNC;
      res = SysOpenFile(params.pathname2, oflag, total_samples_to_move / 2,
                        &params, m_fdCh2, m_sysOpenCookieCh2);
      PX14_RETURN_ON_FAIL(res);
      SIGASSERT(m_fdCh2 != -1);
      if (-1 == m_fdCh2) return SIG_PX14_UNEXPECTED;

      res = OnNewOutputFileOpened(params.pathname2, PX14CHANNEL_TWO);
      PX14_RETURN_ON_FAIL(res);
   }

   // Allocate a small buffer to use for deinterleaving data
   m_spBufCh1 = _IntBuf(new px14_sample_t[s_int_buf_samps]);
   m_spBufCh2 = _IntBuf(new px14_sample_t[s_int_buf_samps]);

   return SIG_SUCCESS;
}

int CIoSinkCtx_BinaryDualFile::Write (px14_sample_t* bufp,
                                      unsigned int samples)
{
   unsigned this_chunk, this_chunk_chan;
   px14_sample_t *ch1p, *ch2p;
   int res;

   // Always call base class first
   res = CIoSinkCtx_Base::Write(bufp, samples);
   PX14_RETURN_ON_FAIL(res);

   while (samples)
   {
      this_chunk = PX14_MIN(samples, s_int_buf_samps << 1);
      this_chunk_chan = this_chunk >> 1;
      ch1p = m_spBufCh1.get();
      ch2p = m_spBufCh2.get();

      res = DeInterleaveDataPX14(bufp, this_chunk, ch1p, ch2p);
      PX14_RETURN_ON_FAIL(res);

      if (-1 != m_fdCh1)
      {
         res = sys_file_write(m_fdCh1, ch1p,
                              this_chunk_chan * sizeof(px14_sample_t));
         if (-1 == res)
            return SIG_PX14_FILE_IO_ERROR;
         if (res != static_cast<int>(
                                     this_chunk_chan * sizeof(px14_sample_t)))
         {
            return SIG_PX14_DISK_FULL;
         }
      }

      if (-1 != m_fdCh2)
      {
         res = sys_file_write(m_fdCh2, ch2p,
                              this_chunk_chan * sizeof(px14_sample_t));
         if (-1 == res)
            return SIG_PX14_FILE_IO_ERROR;
         if (res != static_cast<int>(
                                     this_chunk_chan * sizeof(px14_sample_t)))
         {
            return SIG_PX14_DISK_FULL;
         }
      }

      samples -= this_chunk;
      bufp += this_chunk;
   }

   return SIG_SUCCESS;
}

void CIoSinkCtx_BinaryDualFile::Release()
{
   // Close files, etc.
   if (m_fdCh1 != -1)
   {
      sys_file_close(m_fdCh1);
      m_fdCh1 = -1;
   }

   if (m_fdCh2 != -1)
   {
      sys_file_close(m_fdCh2);
      m_fdCh2 = -1;
   }

   // Finish up with base class implementation which will delete this
   CIoSinkCtx_Base::Release();
}

#pragma endregion CIoSinkCtx_BinaryDualFile implementation

   CIoSinkCtx_BinDual_Seg::CIoSinkCtx_BinDual_Seg()
: m_sysOpenCookieCh1(PX14_SYS_OPEN_COOKIE_INIT), m_fdCh1(-1),
   m_sysOpenCookieCh2(PX14_SYS_OPEN_COOKIE_INIT), m_fdCh2(-1)
{
}

CIoSinkCtx_BinDual_Seg::~CIoSinkCtx_BinDual_Seg()
{
}

bool CIoSinkCtx_BinDual_Seg::GetFilenameParts (const char* wholep,
                                               std::string& pathname_no_ext,
                                               std::string& filename_ext)
{
   std::string::size_type pos;

   // Break given file pathname into a couple component pieces so we can
   //  quickly generate new, indexed filenames as necessary

   if (!wholep || !wholep[0])
   {
      pathname_no_ext.empty();
      filename_ext.empty();
      return false;
   }

   std::string pathname_base(wholep);
   pos = pathname_base.rfind('.');
   if (pos == std::string::npos)
   {
      pathname_no_ext = pathname_base;
      filename_ext.clear();
   }
   else
   {
      pathname_no_ext = pathname_base.substr(0, pos);
      filename_ext = pathname_base.substr(pos);
   }
   pathname_no_ext.append(1, '_');

   return true;
}

void CIoSinkCtx_BinDual_Seg::NextFileName(std::string& p, bool bCh1)
{
   if (bCh1)
   {
      p.assign(m_pathname1_no_ext);
      p.append(my_ConvertToString(m_file_index));
      if (!m_filename1_ext.empty())
         p.append(m_filename1_ext);
   }
   else
   {
      p.assign(m_pathname2_no_ext);
      p.append(my_ConvertToString(m_file_index));
      if (!m_filename2_ext.empty())
         p.append(m_filename2_ext);
   }
}

int CIoSinkCtx_BinDual_Seg::StartNextFiles()
{
   unsigned long long samples_left_to_move;
   int res, oflag;

   samples_left_to_move = (m_samps_to_move)
      ? m_samps_to_move - m_samps_moved : 0;

   std::string pathname_new;

   if (m_bUsingCh1)
   {
      if (-1 != m_fdCh1)
      {
         sys_file_close(m_fdCh1);
         m_fdCh1 = -1;
      }

      // Generate next file name
      NextFileName(pathname_new, true);

      // Open next file
      oflag = O_BINARY | O_SEQUENTIAL | O_WRONLY | O_CREAT | O_TRUNC;
      res = SysOpenFile(pathname_new.c_str(), oflag,
                        samples_left_to_move / 2, m_paramsp,
                        m_fdCh1, m_sysOpenCookieCh1);
      PX14_RETURN_ON_FAIL(res);

      res = OnNewOutputFileOpened(pathname_new.c_str(), PX14CHANNEL_ONE);
      PX14_RETURN_ON_FAIL(res);
   }

   if (m_bUsingCh2)
   {
      if (-1 != m_fdCh2)
      {
         sys_file_close(m_fdCh2);
         m_fdCh2 = -1;
      }

      // Generate next file name
      NextFileName(pathname_new, false);

      // Open next file
      oflag = O_BINARY | O_SEQUENTIAL | O_WRONLY | O_CREAT | O_TRUNC;
      res = SysOpenFile(pathname_new.c_str(), oflag,
                        samples_left_to_move / 2, m_paramsp,
                        m_fdCh2, m_sysOpenCookieCh2);
      PX14_RETURN_ON_FAIL(res);

      res = OnNewOutputFileOpened(pathname_new.c_str(), PX14CHANNEL_TWO);
      PX14_RETURN_ON_FAIL(res);
   }

   m_file_index++;
   m_samples_in_file = 0;

   return SIG_SUCCESS;
}

int CIoSinkCtx_BinDual_Seg::Init (HPX14 hBrd,
                                  unsigned long long total_samples_to_move,
                                  PX14S_FILE_WRITE_PARAMS& params)
{
   int res;

   res = CIoSinkCtx_Base::Init(hBrd, total_samples_to_move, params);
   PX14_RETURN_ON_FAIL(res);

   m_sysOpenCookieCh1 = PX14_SYS_OPEN_COOKIE_INIT;
   m_sysOpenCookieCh2 = PX14_SYS_OPEN_COOKIE_INIT;
   m_file_index = 0;

   m_bUsingCh1 = GetFilenameParts(params.pathname,
                                  m_pathname1_no_ext, m_filename1_ext);
   m_bUsingCh2 = GetFilenameParts(params.pathname2,
                                  m_pathname2_no_ext, m_filename2_ext);

   // Allocate a small buffer to use for deinterleaving data
   m_spBufCh1 = _IntBuf(new px14_sample_t[s_int_buf_samps]);
   m_spBufCh2 = _IntBuf(new px14_sample_t[s_int_buf_samps]);

   return StartNextFiles();
}

void CIoSinkCtx_BinDual_Seg::Release()
{
   if (m_fdCh1 != -1)
   {
      sys_file_close(m_fdCh1);
      m_fdCh1 = -1;
   }

   if (m_fdCh2 != -1)
   {
      sys_file_close(m_fdCh2);
      m_fdCh2 = -1;
   }

   CIoSinkCtx_Base::Release();
}

int CIoSinkCtx_BinDual_Seg::Write (px14_sample_t* bufp,
                                   unsigned int samples)
{
   unsigned this_chunk, this_chunk_chan;
   px14_sample_t *ch1p, *ch2p;
   int res;

   res = CIoSinkCtx_Base::Write(bufp, samples);
   PX14_RETURN_ON_FAIL(res);

   while (samples)
   {
      // How much should we write?
      this_chunk = PX14_MIN(samples, s_int_buf_samps << 1);
      this_chunk_chan = this_chunk >> 1;
      if (m_samples_in_file + this_chunk_chan > m_paramsp->max_file_seg)
      {
         this_chunk_chan = static_cast<unsigned int>(
                                                     m_paramsp->max_file_seg - m_samples_in_file);
         this_chunk = this_chunk_chan << 1;
      }

      ch1p = m_spBufCh1.get();
      ch2p = m_spBufCh2.get();
      res = DeInterleaveDataPX14(bufp, this_chunk, ch1p, ch2p);
      PX14_RETURN_ON_FAIL(res);

      if (m_bUsingCh1)
      {
         res = sys_file_write(m_fdCh1, ch1p,
                              this_chunk_chan * sizeof(px14_sample_t));
         if (-1 == res)
            return SIG_PX14_FILE_IO_ERROR;
         if (res != static_cast<int>(
                                     this_chunk_chan * sizeof(px14_sample_t)))
         {
            return SIG_PX14_DISK_FULL;
         }
      }

      if (m_bUsingCh2)
      {
         res = sys_file_write(m_fdCh2, ch2p,
                              this_chunk_chan * sizeof(px14_sample_t));
         if (-1 == res)
            return SIG_PX14_FILE_IO_ERROR;
         if (res != static_cast<int>(
                                     this_chunk_chan * sizeof(px14_sample_t)))
         {
            return SIG_PX14_DISK_FULL;
         }
      }

      samples -= this_chunk;
      bufp += this_chunk;

      // Start next file if necessary
      m_samples_in_file += this_chunk_chan;
      if ((m_samples_in_file >= m_paramsp->max_file_seg) &&
          (!m_samps_to_move || (m_samps_moved < m_samps_to_move)))
      {
         res = StartNextFiles();
         PX14_RETURN_ON_FAIL(res);
      }
   }

   return SIG_SUCCESS;
}

#pragma region CIoSinkCtx_TextSingleFile

CIoSinkCtx_TextSingleFile::CIoSinkCtx_TextSingleFile(bool bDualChannel) :
   m_bDualChannel(bDualChannel)
{
}

CIoSinkCtx_TextSingleFile::~CIoSinkCtx_TextSingleFile()
{
}

int CIoSinkCtx_TextSingleFile::Init (HPX14 hBrd,
                                     unsigned long long total_samples_to_move,
                                     PX14S_FILE_WRITE_PARAMS& params)
{
   int res;

   res = CIoSinkCtx_Base::Init(hBrd, total_samples_to_move, params);
   PX14_RETURN_ON_FAIL(res);

   if (params.pathname && params.pathname[0])
   {
      m_of.open(params.pathname);
      if (m_of.fail())
         return SIG_PX14_FILE_IO_ERROR;

      if (params.flags & PX14FILWF_HEX_OUTPUT)
         m_of.setf(std::ios_base::hex);

      OnNewOutputFileOpened(params.pathname, -1);
   }

   return SIG_SUCCESS;
}

int CIoSinkCtx_TextSingleFile::Write (px14_sample_t* bufp,
                                      unsigned int samples)
{
   unsigned i;
   int res;

   // Always call base class first
   res = CIoSinkCtx_Base::Write(bufp, samples);
   PX14_RETURN_ON_FAIL(res);

   if (m_of.is_open())
   {
      if (m_paramsp->flags & PX14FILWF_CONVERT_TO_SIGNED)
      {
         // Saving signed data

         PX14_SAMPLE_TYPE_SIGNED* signed_bufp =
            reinterpret_cast<PX14_SAMPLE_TYPE_SIGNED*>(bufp);

         if (m_bDualChannel)
         {
            samples >>= 1;
            for (i=0; i<samples && m_of; i++,signed_bufp+=2)
            {
               m_of << *signed_bufp << "\t" << *(signed_bufp+1)
                  << std::endl;
            }
         }
         else
         {
            for (i=0; i<samples && m_of; i++,signed_bufp++)
               m_of << *signed_bufp << std::endl;
         }
      }
      else
      {
         // Saving unsigned data; default

         if (m_bDualChannel)
         {
            samples >>= 1;
            for (i=0; i<samples && m_of; i++,bufp+=2)
               m_of << *bufp << "\t" << *(bufp+1) << std::endl;
         }
         else
         {
            for (i=0; i<samples && m_of; i++,bufp++)
               m_of << *bufp << std::endl;
         }
      }
   }

   return m_of ? SIG_SUCCESS : SIG_PX14_FILE_IO_ERROR;
}

#pragma endregion CIoSinkCtx_TextSingleFile implementation

#pragma region CIoSinkCtx_TextDualFile

CIoSinkCtx_TextDualFile::CIoSinkCtx_TextDualFile()
{
}

CIoSinkCtx_TextDualFile::~CIoSinkCtx_TextDualFile()
{
}

int CIoSinkCtx_TextDualFile::Init (HPX14 hBrd,
                                   unsigned long long total_samples_to_move,
                                   PX14S_FILE_WRITE_PARAMS& params)
{
   int res;

   res = CIoSinkCtx_Base::Init(hBrd, total_samples_to_move, params);
   PX14_RETURN_ON_FAIL(res);

   if (params.pathname && params.pathname[0])
   {
      m_ofCh1.open(params.pathname);
      if (m_ofCh1.fail())
         return SIG_PX14_FILE_IO_ERROR;

      if (params.flags & PX14FILWF_HEX_OUTPUT)
         m_ofCh1.setf(std::ios_base::hex);

      OnNewOutputFileOpened(params.pathname, PX14CHANNEL_ONE);
   }

   if (params.pathname2 && params.pathname2[0])
   {
      m_ofCh2.open(params.pathname2);
      if (m_ofCh2.fail())
         return SIG_PX14_FILE_IO_ERROR;

      if (params.flags & PX14FILWF_HEX_OUTPUT)
         m_ofCh2.setf(std::ios_base::hex);

      OnNewOutputFileOpened(params.pathname2, PX14CHANNEL_TWO);
   }

   // Allocate a small buffer to use for deinterleaving data
   m_spBufCh1 = _IntBuf(new px14_sample_t[s_int_buf_samps]);
   m_spBufCh2 = _IntBuf(new px14_sample_t[s_int_buf_samps]);

   return SIG_SUCCESS;
}

int CIoSinkCtx_TextDualFile::Write (px14_sample_t* bufp,
                                    unsigned int samples)
{
   px14_sample_t *ch1p, *ch2p;
   unsigned i, n, this_chunk;
   int res;

   // Always call base class first
   res = CIoSinkCtx_Base::Write(bufp, samples);
   PX14_RETURN_ON_FAIL(res);

   while (samples)
   {
      this_chunk = PX14_MIN(samples, s_int_buf_samps << 1);
      ch1p = m_spBufCh1.get();
      ch2p = m_spBufCh2.get();

      res = DeInterleaveDataPX14(bufp, this_chunk, ch1p, ch2p);
      PX14_RETURN_ON_FAIL(res);

      n = this_chunk >> 1;

      if (m_paramsp->flags & PX14FILWF_CONVERT_TO_SIGNED)
      {
         // Saving signed data

         PX14_SAMPLE_TYPE_SIGNED* signed_ch1p =
            reinterpret_cast<PX14_SAMPLE_TYPE_SIGNED*>(ch1p);
         PX14_SAMPLE_TYPE_SIGNED* signed_ch2p =
            reinterpret_cast<PX14_SAMPLE_TYPE_SIGNED*>(ch2p);

         for (i=0; i<n && m_ofCh1 && m_ofCh2; i++)
         {
            if (m_ofCh1.is_open()) m_ofCh1 << *signed_ch1p << std::endl;
            if (m_ofCh2.is_open()) m_ofCh2 << *signed_ch2p << std::endl;

            signed_ch1p++;
            signed_ch2p++;
         }
      }
      else
      {
         // Savin unsigned data; default

         for (i=0; i<n && m_ofCh1 && m_ofCh2; i++,ch1p++,ch2p++)
         {
            if (m_ofCh1.is_open()) m_ofCh1 << *ch1p << std::endl;
            if (m_ofCh2.is_open()) m_ofCh2 << *ch2p << std::endl;
         }
      }

      samples -= this_chunk;
   }

   return (m_ofCh1 && m_ofCh2) ? SIG_SUCCESS : SIG_PX14_FILE_IO_ERROR;
}

#pragma endregion CIoSinkCtx_TextDualFile implementation

int CreateIoSinkCtxPX14 (HPX14 hBrd,
                         PX14S_FILE_WRITE_PARAMS* paramsp,
                         IIoSinkCtxPX14** sinkpp)
{
   PX14_ENSURE_POINTER(hBrd, paramsp, PX14S_FILE_WRITE_PARAMS, "paramsp");
   PX14_ENSURE_POINTER(hBrd, sinkpp,  IIoSinkCtxPX14,          "sinkpp");

   PX14_ENSURE_STRUCT_SIZE(hBrd, paramsp, _PX14SO_FILE_WRITE_PARAMS_V1, "paramsp");

   // Use a NULL device if no pathname specified
   SIGASSERT_NULL_OR_POINTER(paramsp->pathname2, char);
   SIGASSERT_NULL_OR_POINTER(paramsp->pathname, char);
   if (NULL == paramsp->pathname)
   {
      *sinkpp = new CIoSinkCtx_Base();
      return SIG_SUCCESS;
   }

   // -- Creating text files -- //

   if (paramsp->flags & PX14FILWF_SAVE_AS_TEXT)
   {
      if (paramsp->flags & PX14FILWF_DEINTERLEAVE)
      {
         *sinkpp = new CIoSinkCtx_TextDualFile();
         return SIG_SUCCESS;
      }

      *sinkpp = new CIoSinkCtx_TextSingleFile(
                                              (paramsp->flags & PX14FILWF_ASSUME_DUAL_CHANNEL) != 0);
      return SIG_SUCCESS;
   }

   // -- Creating binary files -- //

   if (paramsp->max_file_seg)
   {
      if (paramsp->flags & PX14FILWF_DEINTERLEAVE)
      {
         if ((paramsp->pathname  && paramsp->pathname[0]) ||
             (paramsp->pathname2 && paramsp->pathname2[0]))
         {
            *sinkpp = new CIoSinkCtx_BinDual_Seg();
            return SIG_SUCCESS;
         }
         // else fall through; no file writing taking place
      }
      else
      {
         if (paramsp->pathname && paramsp->pathname[0])
         {
            *sinkpp = new CIoSinkCtx_BinSingle_Seg();
            return SIG_SUCCESS;
         }
         // else fall through; no file writing taking place
      }
   }

   if (paramsp->flags & PX14FILWF_DEINTERLEAVE)
   {
      // Caller wants to deinterleave data into separate files
      *sinkpp = new CIoSinkCtx_BinaryDualFile();
      return SIG_SUCCESS;
   }

   // Single-channel binary file dump
   *sinkpp = new CIoSinkCtx_BinarySingleChannel();
   return SIG_SUCCESS;
}

int ReadSampleRamFileFastHaveSink (HPX14 hBrd,
                                   unsigned int sample_start,
                                   unsigned int sample_count,
                                   px14_sample_t* dma_bufp,
                                   unsigned int dma_buf_samples,
                                   IIoSinkCtxPX14* sinkp)
{
   unsigned max_chunk_samples, chunk_cur, chunk_prev;
   px14_sample_t *buf_curp, *buf_prevp;
   bool bVirtual;
   int res, i;

   SIGASSERT_POINTER(dma_bufp, px14_sample_t);
   if (NULL == dma_bufp)
      return SIG_PX14_INVALID_ARG_4;
   SIGASSERT_POINTER(sinkp, IIoSinkCtxPX14);
   if (NULL == sinkp)
      return SIG_PX14_INVALID_ARG_6;

   max_chunk_samples = dma_buf_samples >> 1;
   if (max_chunk_samples > PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES)
      max_chunk_samples = PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES;
   if (max_chunk_samples < PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES)
      return SIG_PX14_BUFFER_TOO_SMALL;

   // Setup active memory region
   res = SetStartSamplePX14(hBrd, sample_start);
   PX14_RETURN_ON_FAIL(res);
   res = SetSampleCountPX14(hBrd, sample_count);
   PX14_RETURN_ON_FAIL(res);
   // Function may have had to realign sample count
   sample_count = GetSampleCountPX14(hBrd, PX14_GET_FROM_CACHE);

   bVirtual = IsDeviceVirtualPX14(hBrd) > 0;

   // Enter PCI transfer operating mode
   res = SetOperatingModePX14(hBrd, PX14MODE_RAM_READ_PCI);
   PX14_RETURN_ON_FAIL(res);
   // Ensures that we always go back to standby mode
   CAutoStandbyModePX14 autoStandby(hBrd);

   buf_prevp = NULL;
   chunk_prev = 0;

   // Main loop
   for (i=0; sample_count; i++)
   {
      // Ping-pong between buffer halves
      buf_curp = dma_bufp + ((i & 1) ? max_chunk_samples : 0);
      chunk_cur = PX14_MIN(sample_count, max_chunk_samples);

      if (bVirtual)
      {
         res = GenerateVirtualDataPX14(hBrd, buf_curp,
                                       chunk_cur, sample_start);
         sample_start += chunk_cur;
      }
      else
      {
         // Start asynchronous transfer of data
         res = DmaTransferPX14(hBrd, buf_curp,
                               chunk_cur * PX14_SAMPLE_SIZE_IN_BYTES, PX14_TRUE, PX14_TRUE);
      }
      if ((SIG_SUCCESS != res) && (SIG_PENDING != res))
         return res;

      // Write previous data while we're transferring new data
      if (buf_prevp)
      {
         res = sinkp->Write(buf_prevp, chunk_prev);
         PX14_RETURN_ON_FAIL(res);
      }

      // Wait for asynchronous DMA to finish
      res = WaitForTransferCompletePX14(hBrd);
      if (SIG_SUCCESS != res)
      {
         SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
         return res;
      }

      // Update pointers/counters
      sample_count -= chunk_cur;
      chunk_prev = chunk_cur;
      buf_prevp = buf_curp;
   }

   // Handle last chunk
   if (buf_prevp)
   {
      res = sinkp->Write(buf_prevp, chunk_prev);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}


// PX14 library exports implementation --------------------------------- //

/** @brief Transfer data in PX14 RAM to a file on the host PC

  @param hBrd
  A handle to the PX14 board. This handle is obtained by calling the
  ConnectToDevicePX14 function.
  @param sample_start
  The index of the first sample to copy. This parameter has the same
  restrictions defined in the SetStartSamplePX14 function.
  @param sample_count
  The total number of samples to copy. This parameter has the same
  restrictions defined in the SetSampleCountPX14 function and must be
  an integer multiple of 1024 samples.
  @param dma_bufp
  The address of a DMA buffer previously allocated by the
  AllocateDmaBufferPX14 function. This DMA buffer is used for the data
  transfer between the PX14 and host PC. This buffer need not be as
  large as the total amount of data to transfer but must be at least
  2 * PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES, regardless of amount of data
  to write. 1 mebi-sample (1048576 samples) is ideal
  @param dma_buf_samples
  The size, in samples, of the DMA buffer pointed to by dma_bufp
  @param pathnamep
  A pointer to a NULL-terminated string that contains the full
  pathname of the file to save the data to.
  @param init_bytes_skip
  The number of initial bytes to skip when writing to file.
  @param flags
  A set of flags (PX14FILWF_*) that affect how the file is written

*/
PX14API ReadSampleRamFileFastPX14 (HPX14 hBrd,
                                   unsigned int sample_start,
                                   unsigned int sample_count,
                                   px14_sample_t* dma_bufp,
                                   unsigned int dma_buf_samples,
                                   PX14S_FILE_WRITE_PARAMS* paramsp)
{
   IIoSinkCtxPX14* sinkp;
   int res;

   SIGASSERT_POINTER(dma_bufp, px14_sample_t);
   if (NULL == dma_bufp)
      return SIG_PX14_INVALID_ARG_4;
   SIGASSERT_POINTER(paramsp, PX14S_FILE_WRITE_PARAMS);
   if (NULL == paramsp)
      return SIG_PX14_INVALID_ARG_6;

   // Create IO sink object; handles all writing to file(s)
   try { res = CreateIoSinkCtxPX14(hBrd, paramsp, &sinkp); }
   catch (std::bad_alloc) { res = SIG_OUTOFMEMORY; }
   PX14_RETURN_ON_FAIL(res);
   CAutoIoSinkCtx autoRelease(sinkp);		// Cleans up sink on destruction

   // Initialize the sink object; ready files, etc
   try { res = sinkp->Init(hBrd, sample_count, *paramsp); }
   catch (std::bad_alloc) { res = SIG_OUTOFMEMORY; }
   PX14_RETURN_ON_FAIL(res);

   return ReadSampleRamFileFastHaveSink(hBrd, sample_start, sample_count,
                                        dma_bufp, dma_buf_samples, sinkp);
}

PX14API ReadSampleRamFileBufPX14 (HPX14 hBrd,
                                  unsigned int sample_start,
                                  unsigned int sample_count,
                                  PX14S_FILE_WRITE_PARAMS* paramsp)
{
   static const unsigned chunk_samples = 16384;

   px14_sample_t* chunk_bufp;
   IIoSinkCtxPX14* sinkp;
   unsigned cur_chunk;
   int res;

   // Allocate a temporary buffer to shuttle data
   try { chunk_bufp = new px14_sample_t[chunk_samples]; }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }
   std::auto_ptr<px14_sample_t> spBuf(chunk_bufp);	// free on exit

   // Create IO sink object; handles all writing to file(s)
   try { res = CreateIoSinkCtxPX14(hBrd, paramsp, &sinkp); }
   catch (std::bad_alloc) { res = SIG_OUTOFMEMORY; }
   PX14_RETURN_ON_FAIL(res);
   CAutoIoSinkCtx autoRelease(sinkp);		// Cleans up sink on destruction

   // Initialize the sink object; ready files, etc
   try { res = sinkp->Init(hBrd, sample_count, *paramsp); }
   catch (std::bad_alloc) { res = SIG_OUTOFMEMORY; }
   PX14_RETURN_ON_FAIL(res);

   while (sample_count)
   {
      // Read data from PX14
      cur_chunk = PX14_MIN(sample_count, chunk_samples);
      res = ReadSampleRamBufPX14(hBrd, sample_start, cur_chunk, chunk_bufp);
      PX14_RETURN_ON_FAIL(res);

      // Write to file(s)
      res = sinkp->Write(chunk_bufp, cur_chunk);
      PX14_RETURN_ON_FAIL(res);

      sample_count -= cur_chunk;
      sample_start += cur_chunk;
   }

   return SIG_SUCCESS;
}

PX14API _DumpRawDataPX14 (HPX14 hBrd,
                          px14_sample_t* bufp,
                          unsigned samples,
                          PX14S_FILE_WRITE_PARAMS* paramsp)
{
   IIoSinkCtxPX14* sinkp;
   int res;

   SIGASSERT_POINTER(bufp, px14_sample_t);
   if (NULL == bufp)
      return SIG_PX14_INVALID_ARG_1;

   // Create IO sink object; handles all writing to file(s)
   try { res = CreateIoSinkCtxPX14(hBrd, paramsp, &sinkp); }
   catch (std::bad_alloc) { res = SIG_OUTOFMEMORY; }
   PX14_RETURN_ON_FAIL(res);
   CAutoIoSinkCtx autoRelease(sinkp);		// Cleans up sink on destruction

   // Initialize the sink object; ready files, etc
   try { res = sinkp->Init(hBrd, samples, *paramsp); }
   catch (std::bad_alloc) { res = SIG_OUTOFMEMORY; }
   PX14_RETURN_ON_FAIL(res);

   return sinkp->Write(bufp, samples);
}

