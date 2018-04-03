/** @file	px14_zip.cpp
  @brief	Implementation of PX14 zip/unzip routines

  @note
  This file contains no exported library functions and it only used by
  the Windows platform. We use the zlib library to generate PX14
  firmware files.
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_zip.h"

#ifdef _WIN32

// -- zlib includes (firmware decompression)
#define ZLIB_WINAPI
#include <unzip20\unzip.h>
#include <unzip20\zip.h>

#ifndef my_malloc
# define my_malloc		malloc
#endif

#ifndef my_free
# define my_free		free
#endif

static int AddFileToZipFile (zipFile zf,
                             HANDLE hFile,
                             const std::string& name);

// Find and return address of filename in given pathname
static const char* MyPathFindFileNameA (const char* pathnamep);


/** @brief Unzip a file from an archive and copy to destination

  We use the open source zlib for zip operations. Also, this
  implementation does assume the Windows platform. We use an alternate
  method for Linux systems.

  This function is a generic file extraction routine but returns error
  codes specific to PX14 firmware uploading.

  @param zipp
  A pointer to a NULL terminated string that contains the path of the
  zip (or PX14 logic update) file.
  @param srcp
  A pointer to a NULL terminated string that contains the name of the
  file to extract
  @param dstp
  A pointer to a NULL terminated string that contains the path for the
  output file. Will overwrite file if it exists
  */
int UnzipFile (const char* zipp, const char* srcp, const char *dstp)
{
   static const int chunkBytes = 16 * 1024;

   ULONG dwBytes, dwWrote;
   bool bCloseCurFile;
   LPVOID hZip, bufp;
   HANDLE hDest;
   int retVal;

   SIGASSERT_POINTER(zipp, char);
   SIGASSERT_POINTER(srcp, char);
   SIGASSERT_POINTER(dstp, char);
   if (!zipp || !srcp || !dstp)
      return SIG_INVALIDARG;

   hDest = INVALID_HANDLE_VALUE;
   bCloseCurFile = false;
   retVal = SIG_SUCCESS;
   bufp = NULL;
   hZip = NULL;

   do {
      // Open up zip file and find source file
      if ((NULL == (hZip = unzOpen(zipp))) ||
          (UNZ_OK != unzLocateFile(hZip, srcp, 2)) ||
          (UNZ_OK != unzOpenCurrentFile(hZip)))
      {
         retVal = SIG_PX14_INVALID_FW_FILE;
         break;
      }
      bCloseCurFile = true;

      // Open up destination file
      hDest = CreateFileA (dstp, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           OPEN_ALWAYS, 0, NULL);
      if (hDest == INVALID_HANDLE_VALUE)
      {
         retVal = SIG_PX14_DEST_FILE_OPEN_FAILED;
         break;
      }

      // Allocate chunk buffer
      bufp = my_malloc(chunkBytes);
      if (NULL == bufp)
      {
         retVal = SIG_OUTOFMEMORY;
         break;
      }

      // Extract the zip file data and copy to destination
      while (0 < (dwBytes = unzReadCurrentFile(hZip, bufp, chunkBytes)))
      {
         if (!WriteFile(hDest, bufp, dwBytes, &dwWrote, NULL))
         {
            retVal = SIG_PX14_FILE_IO_ERROR;
            break;
         }
      }

   } while (0);

   if (bCloseCurFile)
      unzCloseCurrentFile(hZip);

   SIGASSERT_NULL_OR_POINTER(bufp, char);
   if (bufp) my_free(bufp);

   if (INVALID_HANDLE_VALUE != hDest)
   {
      SetEndOfFile(hDest);
      CloseHandle(hDest);
   }

   if (hZip)
      unzClose(hZip);

   return retVal;
}

int CreateZip (const char* zipp, const ZipFilesList& files)
{
   zipFile zf;
   HANDLE h;
   int res;

   SIGASSERT_POINTER(zipp, const char);
   if (NULL == zipp)
      return SIG_PX14_INVALID_ARG_1;

   if (NULL == (zf = zipOpen(zipp, 0)))
      return SIG_ERROR;

   ZipFilesList::const_iterator iFile(files.begin());
   for (res=SIG_SUCCESS; iFile!=files.end(); iFile++)
   {
      std::string nameUse(iFile->second);
      if (nameUse.empty())
         nameUse.assign(MyPathFindFileNameA(iFile->first.c_str()));

      h = CreateFileA (iFile->first.c_str(), GENERIC_READ,
                       FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                       NULL);
      if (INVALID_HANDLE_VALUE == h)
      {
         res = SIG_ERROR;
         break;
      }

      res = AddFileToZipFile(zf, h, nameUse);

      CloseHandle(h);

      if (SIG_SUCCESS != res)
         break;
   }

   zipClose(zf, NULL);

   return res;
}

int AddFileToZipFile (zipFile zf, HANDLE hFile, const std::string& name)
{
   static const unsigned chunk_bytes = 16 * 1024;		// Bytes

   int res, zres;
   DWORD dwRead;
   LPVOID pData;

   SIGASSERT (!name.empty());

   // Open the destination zipped file stream
   res = zipOpenNewFileInZip(zf, name.c_str(), NULL, NULL, 0, NULL, 0, NULL,
                             Z_DEFLATED, Z_DEFAULT_COMPRESSION);
   if (ZIP_OK != res)
      return SIG_ERROR;

   // Allocate our chunk buffer
   if (NULL == (pData = my_malloc (chunk_bytes)))
   {
      zipCloseFileInZip(zf);
      return SIG_OUTOFMEMORY;
   }

   res = SIG_SUCCESS;

   do
   {
      if (!ReadFile(hFile, pData, chunk_bytes, &dwRead, NULL))
      {
         res = SIG_PX14_FILE_IO_ERROR;
         break;
      }
      if (!dwRead)
         break;

      if (ZIP_OK != zipWriteInFileInZip(zf, pData, dwRead))
      {
         res = SIG_ERROR;
         break;
      }

   } while (1);

   my_free(pData);

   zres = zipCloseFileInZip(zf);

   return (res == SIG_SUCCESS) ?
      (ZIP_OK == res ? SIG_SUCCESS : SIG_ERROR) : res;
}

static int EnumZipFiles (zipFile zf, std::list<std::string>& files)
{
   unz_file_info ufi;
   int res;

   res = unzGoToFirstFile(zf);
   if (UNZ_OK != res)
      return SIG_ERROR;

   std::string s;

   while (UNZ_OK == res)
   {
      res = unzGetCurrentFileInfo(zf, &ufi, NULL, 0, NULL, 0, NULL, 0);
      if (UNZ_OK != res) break;

      try { s.resize(ufi.size_filename); }
      catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }

      res = unzGetCurrentFileInfo(zf, &ufi, &s[0], ufi.size_filename,
                                  NULL, 0, NULL, 0);
      if (UNZ_OK == res)
      {
         if (!s.empty())
            files.push_back(s.c_str());

         res = unzGoToNextFile(zf);
         if (UNZ_END_OF_LIST_OF_FILE == res)
         {
            res = UNZ_OK;
            break;
         }
      }
   }

   return (res == UNZ_OK) ? SIG_SUCCESS : SIG_ERROR;
}

int EnumZipFiles (const char* zipp, std::list<std::string>& files)
{
   zipFile zf;
   int res;

   if (NULL == (zf = unzOpen(zipp)))
      return SIG_PX14_INVALID_FW_FILE;

   res = EnumZipFiles(zf, files);

   unzClose(zf);

   return res;
}

const char* MyPathFindFileNameA (const char* pathnamep)
{
   const char* at;

   // Walk string backwards looking for last '\'
   at = pathnamep + strlen(pathnamep);
   for (; at!=pathnamep; at--)
   {
      if (*at == '\\')
      {
         at++;
         break;
      }
   }

   return at;
}

#endif // #ifdef _WIN32

#ifdef __linux__

/** @brief Unzip a file from an archive and copy to destination

  This function is a generic file extraction routine but returns error
  codes specific to PX14400 firmware uploading.

  In Linux we're using a 'system' call to unzip for file extraction.

  @param zipp
  A pointer to a NULL terminated string that contains the path of the
  zip (or PX14400 logic update) file.
  @param srcp
  A pointer to a NULL terminated string that contains the name of the
  file to extract
  @param dstp
  A pointer to a NULL terminated string that contains the path for the
  output file. Will overwrite file if it exists. LINUX: We are assuming
  that this is a pathname and that the last item after / is the desired
  file name.
  */
int UnzipFile (const char* zipp, const char* srcp, const char* dstp)
{
   int res;

   SIGASSERT_NULL_OR_POINTER(dstp, char);
   SIGASSERT_NULL_OR_POINTER(srcp, char);
   SIGASSERT_POINTER(zipp, char);
   if (NULL == zipp)
      return SIG_INVALIDARG;

   // We're using unzip to handle our unzipping needs. There's one slight
   //  snag here in that we cannot specify an output pathname for a zip file
   //  member like we can on Windows. (That is, we can't extract "A" to
   //  "/some/path/B" for file name B).We can specify an alternate path, but
   //  not an alternate name.

   // Our solution is to unzip to a redirected stdout.

   // -qq super quiet
   // -p extract to stdout
   std::string sCmd("unzip -qq -p \"");
   sCmd.append(zipp);
   sCmd.append("\" ");

   if (srcp) {
      sCmd.append(1,'"');
      sCmd.append(srcp);
      sCmd.append("\" ");
   }

   if (dstp) {
      sCmd.append (" > ");
      sCmd.append(dstp);
   }

   res = system(sCmd.c_str());
   if (res < 0)
      return SIG_ERROR;
   if (0==res || 1==res)
      return SIG_SUCCESS;

   return (res>=4 && res<=7) ? SIG_OUTOFMEMORY : SIG_PX14_INVALID_FW_FILE;
}

int EnumZipFiles (const char* zipp, std::list<std::string>& files)
{
   return SIG_PX14_FILE_IO_ERROR;
}

#endif // __linux__


