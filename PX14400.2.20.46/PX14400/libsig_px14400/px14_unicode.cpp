/** @file	px14_unicode.cpp
  @brief	UNICODE implementation of library routines
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"

#ifndef PX14PP_NO_UNICODE_SUPPORT

/**
  @overload
  PX14API GetErrorTextAPX14 (int res, char** bufpp, unsigned int flags)
  */
PX14API GetErrorTextWPX14 (int res,
                           wchar_t** bufpp,
                           unsigned int flags,
                           HPX14 hBrd)
{
   int my_res, str_len;
   char* tmp_bufp;

   SIGASSERT_POINTER(bufpp, wchar_t*);
   if (NULL == bufpp)
      return SIG_PX14_INVALID_ARG_2;

   // Grab the ASCII string
   my_res = GetErrorTextAPX14(res, &tmp_bufp, flags, hBrd);
   if (my_res != SIG_SUCCESS)
      return res;

   str_len = static_cast<int>(strlen(tmp_bufp));
   my_res = SIG_SUCCESS;

   *bufpp = reinterpret_cast<wchar_t*>(
                                       my_malloc((str_len + 1) * sizeof(wchar_t)));
   if (NULL != *bufpp)
      my_res = ConvertAsciiToWideChar(tmp_bufp, str_len+1, *bufpp);

   FreeMemoryPX14(tmp_bufp);

   return *bufpp ? my_res : SIG_OUTOFMEMORY;
}

/** @brief Save board settings to an XML file (UNICODE)
*/
PX14API SaveSettingsToFileXmlWPX14 (HPX14 hBrd, unsigned int flags,
                                    const wchar_t* pathnamep,
                                    const wchar_t* encodingp)
{
   return SaveSettingsToFileXmlAPX14(hBrd, flags,
                                     CAutoCharBuf(pathnamep), CAutoCharBuf (encodingp));
}

PX14API LoadSettingsFromFileXmlWPX14 (HPX14 hBrd, unsigned int flags,
                                      const wchar_t* pathnamep)
{
   CAutoCharBuf acb(pathnamep);

   if (!SysFileExists(acb))
      return SIG_PX14_SOURCE_FILE_OPEN_FAILED;

   return LoadSettingsFromFileXmlAPX14(hBrd, flags,acb);
}

/** @overload
  PX14API GetVersionTextAPX14 (unsigned int ver_type, char* bufp,
  unsigned int buf_sz, unsigned int flags)
  */
PX14API GetVersionTextWPX14 (HPX14 hBrd,
                             unsigned int ver_type,
                             wchar_t** bufpp,
                             unsigned int flags)
{
   int res, str_len;
   char* tmp_buf;

   SIGASSERT_POINTER(bufpp, wchar_t*);

   // Generate the ASCII string
   res = GetVersionTextAPX14(hBrd, ver_type, &tmp_buf, flags);
   PX14_RETURN_ON_FAIL(res);

   str_len = 1 + static_cast<int>(strlen(tmp_buf));
   *bufpp = reinterpret_cast<wchar_t*>(
                                       my_malloc(str_len * sizeof(wchar_t)));
   if (NULL != *bufpp)
      res = ConvertAsciiToWideChar(tmp_buf, str_len, *bufpp);

   FreeMemoryPX14(tmp_buf);
   return *bufpp ? res : SIG_OUTOFMEMORY;
}

PX14API UploadFirmwareWPX14 (HPX14 hBrd,
                             const wchar_t* fw_pathnamep,
                             unsigned int flags,
                             unsigned int* out_flagsp,
                             PX14_FW_UPLOAD_CALLBACK callbackp,
                             void* callback_ctx)
{
   return UploadFirmwareAPX14(hBrd, CAutoCharBuf(fw_pathnamep), flags,
                              out_flagsp, callbackp, callback_ctx);
}

/** @see ExtractFirmwareNotesAPX14 */
PX14API ExtractFirmwareNotesWPX14 (const wchar_t* fw_pathnamep,
                                   wchar_t** notes_pathpp,
                                   int* severityp)
{
   char* abufp = NULL;
   int res;

   SIGASSERT_NULL_OR_POINTER (notes_pathpp, wchar_t*);

   res = ExtractFirmwareNotesAPX14(CAutoCharBuf(fw_pathnamep),
                                   notes_pathpp ? &abufp : NULL, severityp);
   PX14_RETURN_ON_FAIL(res);

   // Allocate and convert ASCII -> UNICODE
   res = ConvertAsciiToWideCharAlloc(abufp, notes_pathpp);

   if (abufp)
      FreeMemoryPX14(abufp);

   return res;
}

/// Obtain user-friendly name for given board (UNICODE)
PX14API GetBoardNameWPX14 (HPX14 hBrd, wchar_t** bufpp, int flags)
{
   char* abufp = NULL;
   int res;

   SIGASSERT_POINTER(bufpp, wchar_t);
   if (NULL == bufpp)
      return SIG_PX14_INVALID_ARG_2;

   res = GetBoardNameAPX14(hBrd, &abufp, flags);
   PX14_RETURN_ON_FAIL(res);

   res = ConvertAsciiToWideCharAlloc(abufp, bufpp);
   if (abufp)
      FreeMemoryPX14(abufp);

   return res;
}

/// Obtain firmware version information (UNICODE)
PX14API QueryFirmwareVersionInfoWPX14 (const wchar_t* fw_pathnamep,
                                       PX14S_FW_VER_INFO* infop)
{
   return QueryFirmwareVersionInfoAPX14(CAutoCharBuf(fw_pathnamep), infop);
}

/** @overload
  PX14API ConnectToRemoteDeviceAPX14 (HPX14* phDev,
  PX14S_REMOTE_CONNECT_CTXA* ctxp)
  */
PX14API ConnectToRemoteDeviceWPX14 (HPX14* phDev,
                                    unsigned int brdNum,
                                    PX14S_REMOTE_CONNECT_CTXW* ctxp)
{
   PX14S_REMOTE_CONNECT_CTXA ctxa;

   SIGASSERT_POINTER(ctxp, PX14S_REMOTE_CONNECT_CTXW);
   if (NULL == ctxp)
      return SIG_PX14_INVALID_ARG_3;

   CAutoCharBuf abServerAddress(ctxp->pServerAddress);
   CAutoCharBuf abAppName(ctxp->pApplicationName);
   CAutoCharBuf abSubServices(ctxp->pSubServices);

   ctxa.struct_size = ctxp->struct_size;
   ctxa.flags = ctxp->flags;
   ctxa.port = ctxp->port;
   ctxa.pServerAddress = abServerAddress;
   ctxa.pApplicationName = abAppName;
   ctxa.pSubServices = abSubServices;

   return ConnectToRemoteDeviceAPX14(phDev, brdNum, &ctxa);
}

PX14API GetRemoteDeviceCountWPX14 (const wchar_t* server_addrp,
                                   unsigned short port,
                                   unsigned int** sn_bufpp)
{
   SIGASSERT_POINTER(server_addrp, wchar_t);
   if (NULL == server_addrp)
      return SIG_PX14_INVALID_ARG_1;

   return GetRemoteDeviceCountAPX14(CAutoCharBuf(server_addrp),
                                    port, sn_bufpp);
}

// Obtain a handle to a PX14400 residing on another computer (UNICODE)
PX14API ConnectToRemoteVirtualDeviceWPX14 (HPX14* phDev,
                                           unsigned int brdNum,
                                           unsigned int ordNum,
                                           PX14S_REMOTE_CONNECT_CTXW* ctxp)
{
   PX14S_REMOTE_CONNECT_CTXA ctxa;

   SIGASSERT_POINTER(ctxp, PX14S_REMOTE_CONNECT_CTXW);
   if (NULL == ctxp)
      return SIG_PX14_INVALID_ARG_3;

   CAutoCharBuf abServerAddress(ctxp->pServerAddress);
   CAutoCharBuf abAppName(ctxp->pApplicationName);
   CAutoCharBuf abSubServices(ctxp->pSubServices);

   ctxa.struct_size = ctxp->struct_size;
   ctxa.flags = ctxp->flags;
   ctxa.port = ctxp->port;
   ctxa.pServerAddress = abServerAddress;
   ctxa.pApplicationName = abAppName;
   ctxa.pSubServices = abSubServices;

   return ConnectToRemoteVirtualDeviceAPX14(phDev, brdNum, ordNum, &ctxa);
}

// Dump library error description to standard file or stream (UNICODE)
PX14API DumpLibErrorWPX14 (int res,
                           const wchar_t* pPreamble,
                           HPX14 hBrd,
                           FILE* filp)
{
   CAutoCharBuf acb_preamble(pPreamble);

   return DumpLibErrorAPX14(res, acb_preamble, hBrd, filp);
}

#else

#ifdef _WIN32
PX14API GetErrorTextWPX14 (int res,
                           wchar_t** bufpp,
                           unsigned int flags,
                           HPX14 hBrd)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

PX14API SaveSettingsToFileXmlWPX14 (HPX14 hBrd, unsigned int flags,
                                    const wchar_t* pathnamep,
                                    const wchar_t* encodingp)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

PX14API LoadSettingsFromFileXmlWPX14 (HPX14 hBrd, unsigned int flags,
                                      const wchar_t* pathnamep)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

PX14API GetVersionTextWPX14 (HPX14 hBrd,
                             unsigned int ver_type,
                             wchar_t** bufpp,
                             unsigned int flags)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

PX14API UploadFirmwareWPX14 (HPX14 hBrd,
                             const wchar_t* fw_pathnamep,
                             unsigned int flags,
                             PX14_FW_UPLOAD_CALLBACK callbackp,
                             void* callback_ctx)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

/** @see ExtractFirmwareNotesAPX14 */
PX14API ExtractFirmwareNotesWPX14 (const wchar_t* fw_pathnamep,
                                   wchar_t** notes_pathpp,
                                   int* severityp)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

/// Obtain firmware version information (UNICODE)
PX14API QueryFirmwareVersionInfoWPX14 (const wchar_t* fw_pathnamep,
                                       PX14S_FW_VER_INFO* infop)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

PX14API ConnectToRemoteDeviceWPX14 (HPX14* phDev,
                                    unsigned int brdNum,
                                    PX14S_REMOTE_CONNECT_CTXW* ctxp)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

PX14API GetRemoteDeviceCountWPX14 (const wchar_t* server_addrp,
                                   unsigned short port,
                                   unsigned int** sn_bufpp)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

// Obtain a handle to a PX14400 residing on another computer (UNICODE)
PX14API ConnectToRemoteVirtualDeviceWPX14 (HPX14* phDev,
                                           unsigned int brdNum,
                                           unsigned int ordNum,
                                           PX14S_REMOTE_CONNECT_CTXW* ctxp)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}
#endif

#endif

