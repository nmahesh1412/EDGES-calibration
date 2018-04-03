/** @file	px14_plat.cpp
  @brief	Platform-specific implementations
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"

static bool _ParseStrVerCmp (const std::string& str,
                             unsigned& a, unsigned& b,
                             unsigned& c, unsigned& d);

#ifdef _WIN32

///////////////////////////////////////////////
//                                           //
// Windows platform specific implementations //
//                                           //
///////////////////////////////////////////////

// Module-local function prototypes ------------------------------------- //

static unsigned GetVolumeSectorSize (const char* pathnamep);
static BOOL MyPathStripToRoot (char* szRoot, int bufBytes);

static bool UnbufferedIoOkay (const char* pathnamep,
                              UINT64 total_bytes,
                              const PX14S_FILE_WRITE_PARAMS* paramsp);

/// Main entry point for DLL
BOOL APIENTRY DllMain (HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
   return PX14_TRUE;
}

/// WIN: Obtain a handle to PX14 device
dev_handle_t GetDeviceHandle (unsigned int brdNum)
{
   std::ostringstream oss;
   oss << "\\\\.\\PX14_" << brdNum;
   std::string devName(oss.str());
   return CreateFileA(devName.c_str(), GENERIC_WRITE | GENERIC_READ,
                      FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
}

/// WIN: Release a device handle
void ReleaseDeviceHandle (dev_handle_t h)
{
   CloseHandle(h);
}

/// WIN: Send a request to the PX14 driver
int DriverRequest (HPX14 hBrd,
                   io_req_t req,
                   void* p,
                   size_t in_bytes,
                   size_t out_bytes)
{
   DWORD dwIgnored;
   BOOL bRes;

   bRes = DeviceIoControl(PX14_H2B(hBrd)->m_hDev, req,
                          in_bytes  ? p : NULL, static_cast<DWORD>(in_bytes),
                          out_bytes ? p : NULL, static_cast<DWORD>(out_bytes),
                          &dwIgnored, NULL);

   if (!bRes)
   {
      // Decode Windows driver error into SIG_* error
      switch (GetLastError())
      {
         case ERROR_INVALID_PARAMETER:		return SIG_INVALIDARG;
         case ERROR_ADDRESS_NOT_ASSOCIATED:	return SIG_PX14_INVALID_DMA_ADDR;
         case ERROR_INVALID_BLOCK_LENGTH:	return SIG_PX14_WOULD_OVERRUN_BUFFER;
         case ERROR_BUSY:
         case ERROR_TOKEN_ALREADY_IN_USE:	return SIG_PX14_BUSY;
         case ERROR_INVALID_DOMAIN_STATE:	return SIG_INVALID_MODE;
         case ERROR_INVALID_DOMAIN_ROLE:		return SIG_PX14_INVALID_MODE_CHANGE;
         case ERROR_OPERATION_ABORTED:		return SIG_CANCELLED;
         case ERROR_UNEXP_NET_ERR:			return SIG_PX14_INVALID_REGISTER;
         case ERROR_PRINT_CANCELLED:			return SIG_PX14_TIMED_OUT;
         case ERROR_STACK_OVERFLOW:			return SIG_PX14_FIFO_OVERFLOW;
         case ERROR_DEV_NOT_EXIST:			return SIG_PX14_DEVICE_REMOVED;
         case ERROR_ACCESS_DENIED:			return SIG_PX14_ACCESS_DENIED;
         case ERROR_FLOPPY_UNKNOWN_ERROR:	return SIG_PX14_PLL_LOCK_FAILED;
         case ERROR_FLOPPY_WRONG_CYLINDER:	return SIG_PX14_PLL_LOCK_FAILED_UNSTABLE;
         case ERROR_UNABLE_TO_UNLOAD_MEDIA:	return SIG_PX14_DCM_SYNC_FAILED;
         case ERROR_NOT_READY:				return SIG_PX14_DEVICE_NOT_READY;
         case ERROR_NO_BROWSER_SERVERS_FOUND:return SIG_DMABUFALLOCFAIL;
         case ERROR_BROKEN_PIPE:				return SIG_PX14_BUFFER_CHECKED_OUT;
         case ERROR_NO_TRUST_LSA_SECRET:		return SIG_PX14_BUFFER_NOT_ALLOCATED;
         default: break;
      }

      return SIG_ERROR;
   }

   return SIG_SUCCESS;
}

int ConvertWideCharToAscii (const wchar_t* wbufp, unsigned int str_chrs,
                            char* abufp)
{
   int res = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK,
                                 wbufp, str_chrs, abufp, str_chrs, NULL, NULL);

   return res ? SIG_SUCCESS : SIG_ERROR;
}

int ConvertAsciiToWideChar (const char* abufp, unsigned int str_chrs,
                            wchar_t* wbufp)
{
   int res = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
                                 abufp, str_chrs, wbufp, str_chrs);

   return res ? SIG_SUCCESS : SIG_ERROR;
}

sys_error_t GetSystemErrorVal()
{
   return GetLastError();
}

void SetSystemErrorVal(sys_error_t val)
{
   SetLastError(val);
}

bool GenSysErrorString (sys_error_t err_val, std::string& str)
{
   LPSTR lpMsgBuf;
   DWORD dwRes;

   // Have Windows turn err_val into a user-friendly string
   lpMsgBuf = NULL;
   dwRes = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_MAX_WIDTH_MASK,
                          NULL, err_val, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          reinterpret_cast<LPSTR>(&lpMsgBuf), 0, NULL );

   if (lpMsgBuf)
   {
      str.assign(lpMsgBuf);
      LocalFree(lpMsgBuf);
      return true;
   }

   return false;
}

int SysGetTemporaryFilesFolder (std::string& path)
{
   char tBuf[MAX_PATH];
   char* buf_allocp;
   DWORD dwRes;

   dwRes = GetTempPathA(MAX_PATH, tBuf);
   if (!dwRes)
      return SIG_ERROR;

   if (dwRes <= MAX_PATH)
      path.assign(tBuf);
   else
   {
      // Allocate buffer for the pathname
      try { buf_allocp = new char[(dwRes = GetTempPathA(0, NULL)) + 1]; }
      catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }
      GetTempPathA(dwRes, buf_allocp);
      path.assign(buf_allocp);
      delete buf_allocp;
   }

   return SIG_SUCCESS;
}

int SysDeleteFile (const char* filepathp)
{
   return DeleteFileA(filepathp) ? SIG_SUCCESS : SIG_ERROR;
}

int SysGetPX14SoftwareRelease (unsigned long long* verp)
{
   static const int verBufLen = 32;

   DWORD dwValType, dwBufLen;
   char verBuf[verBufLen];
   long lRes;
   HKEY hKey;

   // Query version info from registry
   lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Signatec\\PX14"),
                       0, KEY_READ, &hKey);
   if (ERROR_SUCCESS != lRes)
      return SIG_ERROR;
   dwBufLen = sizeof(verBuf);
   lRes = RegQueryValueExA(hKey, "Version", NULL, &dwValType,
                           reinterpret_cast<BYTE*>(verBuf), &dwBufLen);
   RegCloseKey(hKey);
   if (ERROR_SUCCESS != lRes)
      return SIG_ERROR;

   return ParseStrVer64(verBuf, *verp) ? SIG_SUCCESS : SIG_ERROR;
}

void SysSleep (unsigned int ms)
{
   Sleep(ms);
}
void SysMicroSleep (unsigned int us)
{
   /* do nothing; not used for now */
}

unsigned int SysGetTickCount()
{
   return GetTickCount();
}

void SysRelativeMsToTimespec (unsigned int ms, struct timespec* ts)
{
   static const unsigned int NANOSEC_PER_MILLISEC = 1000000;

   struct _timeb currSysTime;

   // Current time
   _ftime64_s(&currSysTime);
   ts->tv_sec = static_cast<long>(currSysTime.time);
   ts->tv_nsec = NANOSEC_PER_MILLISEC * currSysTime.millitm;

   // Plus timeout
   ts->tv_sec += ms / 1000;
   ts->tv_nsec += NANOSEC_PER_MILLISEC * (ms % 1000);
}

/**
  Okay, so in trying to make code as portable as possible we've defaulted
  down to low-level file IO routines (read, write, open, close).

  Because the old open API does not have a way of specifying the Windows
  FILE_FLAG_NO_BUFFERING flag (to ensure optimal file IO performance) we
  must open files using the native CreateFile API. Fortunately, the
  Windows has a CRT function that will allow us to convert a file handle
  into a low-level file descriptor.

  In order to use FILE_FLAG_NO_BUFFERING we need to ensure that all read
  or write offsets and lengths are integer multiples of the underlying
  drive's sector size. This is why we've got all this extra crap as
  parameters.

  We use the SysOpenCookie as an optimization that prevents us from
  having to redetermine if we can use the FILE_FLAG_NO_BUFFERING for
  subsequent calls using the same parameters. (This will happen if we're
  spanning data out among multiple files.)

  The CRT implementation was used as a reference for this function. Note
  that we're assuming normal files here; pipes, sockets, devices, etc may
  not work correctly.
  */
int SysOpenFile (const char* pathname,
                 int oflag,
                 unsigned long long total_bytes_to_move,
                 const _PX14S_FILE_WRITE_PARAMS_tag* paramsp,
                 int& fd,
                 int& SysOpenCookie)
{
   DWORD fileaccess, fileshare, filecreate, fileattrib;
   HANDLE hFile;
   int fd_try;

   if (oflag & O_TEXT)
   {
      // Just use native implementation; our special casing is for
      // unbuffered IO, which we don't use for slower text file IO
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
      int fres = _sopen_s(&fd_try, pathname, oflag, _SH_DENYWR, _S_IWRITE);
      if (fres != 0)
         return SIG_PX14_FILE_IO_ERROR;
#else
      fd_try = sys_file_open(pathname, oflag, _S_IWRITE);
      if (-1 == fd_try)
         return SIG_PX14_FILE_IO_ERROR;
#endif
      fd = fd_try;
      return SIG_SUCCESS;
   }

   // Decode the access flags
   switch (oflag & (_O_RDONLY | _O_WRONLY | _O_RDWR))
   {
      case _O_RDONLY:         // read access
         fileaccess = GENERIC_READ;
         break;
      case _O_WRONLY:         // write access
         fileaccess = GENERIC_WRITE;
         break;
      case _O_RDWR:           /* read and write access */
         fileaccess = GENERIC_READ | GENERIC_WRITE;
         break;
      default:
         return SIG_INVALIDARG;
   }

   // Allow others to peek
   fileshare = FILE_SHARE_READ;

   // Decode open/create method flags
   switch (oflag & (_O_CREAT | _O_EXCL | _O_TRUNC))
   {
      case 0:
      case _O_EXCL:                   // ignore EXCL w/o CREAT
         filecreate = OPEN_EXISTING;
         break;

      case _O_CREAT:
         filecreate = OPEN_ALWAYS;
         break;

      case _O_CREAT | _O_EXCL:
      case _O_CREAT | _O_TRUNC | _O_EXCL:
         filecreate = CREATE_NEW;
         break;

      case _O_TRUNC:
      case _O_TRUNC | _O_EXCL:        // ignore EXCL w/o CREAT
         filecreate = TRUNCATE_EXISTING;
         break;

      case _O_CREAT | _O_TRUNC:
         filecreate = CREATE_ALWAYS;
         break;

      default:
         return SIG_INVALIDARG;
   }

   // Decode file attribute flags if _O_CREAT was specified

   fileattrib = FILE_ATTRIBUTE_NORMAL;     // default

   //if (oflag & _O_CREAT)
   //{
   //	if ( !((pmode & ~_umaskval) & _S_IWRITE) )
   //		fileattrib = FILE_ATTRIBUTE_READONLY;
   //}

   // Set temporary file (delete-on-close) attribute if requested.
   if (oflag & _O_TEMPORARY)
   {
      fileattrib |= FILE_FLAG_DELETE_ON_CLOSE;
      fileaccess |= DELETE;
      fileshare  |= FILE_SHARE_DELETE;
   }

   // Set temporary file (delay-flush-to-disk) attribute if requested.
   if (oflag & _O_SHORT_LIVED)
      fileattrib |= FILE_ATTRIBUTE_TEMPORARY;

   // Set sequential or random access attribute if requested.
   if ( oflag & _O_SEQUENTIAL )
      fileattrib |= FILE_FLAG_SEQUENTIAL_SCAN;
   else if ( oflag & _O_RANDOM )
      fileattrib |= FILE_FLAG_RANDOM_ACCESS;

   if (0 == (paramsp->flags & PX14FILWF_NO_UNBUFFERED_IO))
   {
      if (SysOpenCookie == PX14_SYS_OPEN_COOKIE_INIT)
      {
         SysOpenCookie = UnbufferedIoOkay(pathname,
                                          total_bytes_to_move, paramsp) ? 1 : 0;
      }

      if (SysOpenCookie > 0)
         fileattrib |= FILE_FLAG_NO_BUFFERING;
   }

   // Create file using native Windows API
   hFile = CreateFileA (pathname, fileaccess, fileshare, NULL,
                        filecreate, fileattrib, NULL);
   if (INVALID_HANDLE_VALUE == hFile)
      return SIG_PX14_FILE_IO_ERROR;
   // Associate native file handle with a file descriptor.
   fd_try = _open_osfhandle(reinterpret_cast<intptr_t>(hFile),
                            oflag & (_O_APPEND | _O_RDONLY | _O_TEXT));
   if (-1 == fd_try)
   {
      CloseHandle(hFile);
      return SIG_PX14_FILE_IO_ERROR;
   }

   fd = fd_try;
   return SIG_SUCCESS;
}

bool UnbufferedIoOkay (const char* pathnamep,
                       UINT64 total_bytes,
                       const PX14S_FILE_WRITE_PARAMS* paramsp)
{
   unsigned sector_size;

   if (0 == (sector_size = GetVolumeSectorSize(pathnamep)))
      return false;

   if (total_bytes % sector_size)
      return false;

   if (paramsp)
   {
      if (paramsp->init_bytes_skip % sector_size)
         return false;
      if (paramsp->max_file_seg % sector_size)
         return false;
   }

   return true;
}

BOOL MyPathStripToRoot (char* szRoot, int bufBytes)
{
   std::string::size_type pos_start, p, to_copy;
   bool bUnc;

   SIGASSERT_POINTER(szRoot, char);
   if (NULL == szRoot)
      return FALSE;
   std::string s(szRoot);

   bUnc = false;
   pos_start = 0;

   // Long (> MAX_PATH) UNC path? (\\?\UNC\)
   if (0 == s.compare(0, 8, "\\\\?\\UNC\\"))
   {
      bUnc = true;
      pos_start = 8;
   }
   // Long (> MAX_PATH) normal path? (\\?\)
   else if (0 == s.compare(0, 4, "\\\\?\\"))
   {
      pos_start = 4;
   }
   // Normal UNC path?
   else if (0 == s.compare(0, 2, "\\\\"))
   {
      pos_start = 2;
      bUnc = true;
   }

   if (!bUnc)
   {
      // Normal, non UNC path. Just find ":\"
      if (std::string::npos == (p = s.find(":\\", pos_start)))
         return FALSE;	// Bad or relative path

      if (0 == pos_start)
      {
         // Quick optimization, just write a 0 to shorten string
         szRoot[p+2] = '\0';
      }
      else
      {
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
         strncpy_s (szRoot, bufBytes, &s[pos_start], p - pos_start + 2);
#else
         strncpy(szRoot, &s[pos_start], p - pos_start + 2);
#endif
         szRoot[p - pos_start + 2] = '\0';
      }
   }
   else
   {
      // Dealing with a UNC path (\\serverName\shareName)
      //  s[pos_start] is first char of server name

      // Find end of server
      if ((std::string::npos == (p = s.find("\\", pos_start))) ||
          (p == s.length() - 1))
      {
         return FALSE;	// Bad UNC path
      }

      if (std::string::npos == (p = s.find("\\", p+1)))
         to_copy = s.length() - pos_start;
      else
         to_copy = p - pos_start;

      if (pos_start > 2)
      {
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
         strncpy_s(szRoot + 0, bufBytes, "\\\\", 2);
         strncpy_s(szRoot + 2, bufBytes, &s[pos_start], to_copy);
#else
         strncpy(szRoot + 0, "\\\\", 2);
         strncpy(szRoot + 2, &s[pos_start], to_copy);
#endif
      }
      szRoot[2 + to_copy] = '\0';
   }

   return TRUE;
}

unsigned GetVolumeSectorSize (const char* pathnamep)
{
   DWORD dwBytesPerSector;

   char tBuf[MAX_PATH+1];

   SIGASSERT_POINTER(pathnamep, char);
   if (NULL != pathnamep)
   {
      // We only want the root path
      strncpy_s(tBuf, pathnamep, MAX_PATH);
      if (MyPathStripToRoot(tBuf, MAX_PATH+1) &&
          GetDiskFreeSpaceA(tBuf, NULL, &dwBytesPerSector, NULL, NULL))
      {
         return dwBytesPerSector;
      }
   }

   // Safe default
   return 4096;
}

int SysSocketsInit()
{
   static const WORD wSockVer = MAKEWORD(2,2);		// We'll ask for Winsock 2.2

   WSADATA wsaData;
   int res;

   res = WSAStartup(wSockVer, &wsaData);
   if (NO_ERROR == res)
      return SIG_SUCCESS;

   switch (res)
   {
      case WSASYSNOTREADY:	return SIG_PX14_NETWORK_NOT_READY;
      case WSAEPROCLIM:		return SIG_PX14_SOCKETS_TOO_MANY_TASKS;
      default: break;
   }

   return SIG_PX14_SOCKETS_INIT_ERROR;
}

int GetSocketsErrorText (std::string& sText)
{
   int wsaError = WSAGetLastError();

   std::ostringstream oss;

   switch (wsaError)
   {
      case 0:
         oss << "No error"; break;

      case WSAEINTR:
         oss << "Interrupted function call"; break;
      case WSAEACCES:
         oss << "Permission denied"; break;
      case WSAEFAULT:
         oss << "Bad address"; break;
      case WSAEINVAL:
         oss << "Invalid argument"; break;
      case WSAEMFILE:
         oss << "Too many open files"; break;
      case WSAEWOULDBLOCK:
         oss << "Resource temporarily unavailable"; break;
      case WSAEINPROGRESS:
         oss << "Operation now in progress"; break;
      case WSAEALREADY:
         oss << "Operation already in progress"; break;
      case WSAENOTSOCK:
         oss << "Socket operation on nonsocket"; break;
      case WSAEDESTADDRREQ:
         oss << "Destination address required"; break;
      case WSAEMSGSIZE:
         oss << "Message too long"; break;
      case WSAEPROTOTYPE:
         oss << "Protocol wrong type for socket"; break;
      case WSAENOPROTOOPT:
         oss << "Bad protocol option"; break;
      case WSAEPROTONOSUPPORT:
         oss << "Protocol not supported"; break;
      case WSAESOCKTNOSUPPORT:
         oss << "Socket type not supported"; break;
      case WSAEOPNOTSUPP:
         oss << "Operation not supported"; break;
      case WSAEPFNOSUPPORT:
         oss << "Protocol family not supported"; break;
      case WSAEAFNOSUPPORT:
         oss << "Address family not supported by protocol family"; break;
      case WSAEADDRINUSE:
         oss << "Address already in use"; break;
      case WSAEADDRNOTAVAIL:
         oss << "Cannot assign requested address"; break;
      case WSAENETDOWN:
         oss << "Network is down"; break;
      case WSAENETUNREACH:
         oss << "Network is unreachable"; break;
      case WSAENETRESET:
         oss << "Network dropped connection on reset"; break;
      case WSAECONNABORTED:
         oss << "Software caused connection abort"; break;
      case WSAECONNRESET:
         oss << "Connection reset by peer"; break;
      case WSAENOBUFS:
         oss << "No buffer space available"; break;
      case WSAEISCONN:
         oss << "Socket is already connected"; break;
      case WSAENOTCONN:
         oss << "Socket is not connected"; break;
      case WSAESHUTDOWN:
         oss << "Cannot send after socket shutdown"; break;
      case WSAETIMEDOUT:
         oss << "Connection timed out"; break;
      case WSAECONNREFUSED:
         oss << "Connection refused"; break;
      case WSAEHOSTDOWN:
         oss << "Host is down"; break;
      case WSAEHOSTUNREACH:
         oss << "No route to host"; break;
      case WSAEPROCLIM:
         oss << "Too many processes"; break;
      case WSASYSNOTREADY:
         oss << "Network subsystem is unavailable"; break;
      case WSAVERNOTSUPPORTED:
         oss << "Winsock.dll version out of range"; break;
      case WSANOTINITIALISED:
         oss << "Successful WSAStartup (or SocketsInitPX14) not yet performed"; break;
      case WSAEDISCON:
         oss << "Graceful shutdown in progress"; break;
      case WSATYPE_NOT_FOUND:
         oss << "Class type not found"; break;
      case WSAHOST_NOT_FOUND:
         oss << "Host not found"; break;
      case WSATRY_AGAIN:
         oss << "Nonauthoritative host not found"; break;
      case WSANO_RECOVERY:
         oss << "This is a nonrecoverable error"; break;
      case WSANO_DATA:
         oss << "Valid name, no data record of requested type"; break;
      case WSA_INVALID_HANDLE:
         oss << "Specified event object handle is invalid"; break;
      case WSA_INVALID_PARAMETER:
         oss << "One or more parameters are invalid"; break;
      case WSA_IO_INCOMPLETE:
         oss << "Overlapped I/O event object not in signaled state"; break;
      case WSA_IO_PENDING:
         oss << "Overlapped operations will complete later"; break;
      case WSA_NOT_ENOUGH_MEMORY:
         oss << "Insufficient memory available"; break;
      case WSA_OPERATION_ABORTED:
         oss << "Overlapped operation aborted"; break;
         //case WSAINVALIDPROCTABLE:
         //	oss << "Invalid procedure table from service provider"; break;
         //case WSAINVALIDPROVIDER:
         //	oss << "Invalid service provider version number"; break;
         //case WSAPROVIDERFAILEDINIT:
         //	oss << "Unable to initialize a service provider"; break;
      case WSASYSCALLFAILURE:
         oss << "System call failure"; break;

      default:
         oss << "Unknown error (" << wsaError << ")"; break;
   }

   sText = oss.str();
   return SIG_SUCCESS;
}

int SysNetworkGetLocalAddress (std::string& hostName)
{
   static const int max_hostname_len = 255;

   char host_name[max_hostname_len+1];
   hostent* pe;

   memset (host_name, 0, sizeof(host_name));
   if ((gethostname(host_name, max_hostname_len)) ||
       (NULL == (pe = gethostbyname(host_name))))
   {
      // Failed to get host address; we'll fall back on a loopback
      // address
      hostName.assign("127.0.0.0");
   }
   else
      hostName.assign(inet_ntoa(*((struct in_addr *)pe->h_addr)));

   return SIG_SUCCESS;
}

// Establish a TCP/IP connection to the given server and port
int SysNetworkConnectTcp (sys_socket_t* sockp,
                          const char* server,
                          unsigned short port)
{
   sys_socket_t sock_use;
   int res;

   sock_use = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (sock_use == PX14_INVALID_SOCKET)
      return SIG_PX14_SOCKET_ERROR;

   sockaddr_in service;
   service.sin_family = AF_INET;
   service.sin_addr.s_addr = inet_addr(server);
   service.sin_port = htons(port);
   res = connect(sock_use, reinterpret_cast<SOCKADDR*>(&service),
                 sizeof(service));
   if (NO_ERROR != res)
   {
      SysCloseSocket(sock_use);
      return SIG_PX14_CONNECTION_REFUSED;
   }

   *sockp = sock_use;
   return SIG_SUCCESS;
}

int SysSocketsCleanup()
{
   return WSACleanup();
}

bool SysFileExists (const char* pathname, bool bDirOkay)
{
   unsigned long file_attribs;

   file_attribs = GetFileAttributesA(pathname);
   if (INVALID_FILE_ATTRIBUTES == file_attribs)
      return false;

   if (!bDirOkay && (file_attribs & FILE_ATTRIBUTE_DIRECTORY))
      return false;

   return true;
}

#ifdef _DEBUG
typedef struct tagTHREADNAME_INFO
{
   DWORD  dwType;			// must be 0x1000
   LPCSTR szName;			// pointer to name (in user addr space)
   DWORD  dwThreadID;		// thread ID (-1=caller thread)
   DWORD  dwFlags;			// reserved for future use, must be zero

} THREADNAME_INFO;

/// Sets name of calling thread; useful for debugging
int SysSetThreadName (const char* namep)
{
   if (namep)
   {
      THREADNAME_INFO tni;
      tni.dwType     = 0x1000;
      tni.dwThreadID = -1;
      tni.dwFlags    = 0;
      tni.szName     = namep;

      __try { RaiseException(0x406D1388, 0, sizeof(tni)/sizeof(DWORD), (DWORD_PTR*)&tni); }
      __except (EXCEPTION_CONTINUE_EXECUTION) { }
   }

   return SIG_SUCCESS;
}
#endif

#endif	// end Windows platform stuff

#ifdef _PX14PP_LINUX_PLATFORM

/////////////////////////////////////////////
//                                         //
// Linux platform specific implementations //
//                                         //
/////////////////////////////////////////////

sys_error_t GetSystemErrorVal()
{
   return errno;
}

void SetSystemErrorVal(sys_error_t val)
{
   errno = val;
}

bool GenSysErrorString (sys_error_t err, std::string& str)
{
   char* err_strp = strerror(err);
   if (err_strp) {
      str.assign(err_strp);
      return true;
   }

   return false;
}

int GetSocketsErrorText (std::string& sText)
{
   // No support for socket error text in Linux
   return SIG_PX14_NOT_IMPLEMENTED;	// Linux
}

int SysSocketsInit()
{
   // Nothing to init on Linux platforms
   return SIG_SUCCESS;
}

int SysSocketsCleanup()
{
   // Nothing to init on Linux platforms
   return SIG_SUCCESS;
}

int SysGetTemporaryFilesFolder (std::string& path)
{
   path.assign("/tmp/");
   return 0;
}

int SysDeleteFile (const char* filepathp)
{
   if (filepathp) {

      std::ostringstream oss;
      oss << "rm -f " << filepathp;
      if (-1 == system(oss.str().c_str()))
         return SIG_ERROR;
   }

   return 0;
}

int SysGetPX14SoftwareRelease (unsigned long long* verp)
{
   // We have ver.dat in top-level directory but it doesn't look like it's being used
   //  at the moment. Check PMP1000/PDAC impl.

   // LTODO: Implement SysGetPX14SoftwareRelease
   return SIG_PX14_NOT_IMPLEMENTED;	// Linux
}

int SysOpenFile (const char* pathname,
                 int oflag,
                 unsigned long long total_bytes_to_move,
                 const _PX14S_FILE_WRITE_PARAMS_tag* params,
                 int& fd, int& SysOpenCookie)
{
   static const mode_t m = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
   int res;

   // Most of the parameters can be ignore here; they're used on Windows
   //  platforms to determine if fast, unbuffered IO may be used

   res = open(pathname, oflag, m);
   if (-1 == res)
      return SIG_PX14_FILE_IO_ERROR;

   fd = res;
   return SIG_SUCCESS;
}

void SysSleep (unsigned int ms)
{
   usleep(ms * 1000);
}
/* Higher resolution delay that's used in firmware uploading;
 * using SysSleep would have made the process 'unbearable' */
void SysMicroSleep (unsigned int us)
{
   usleep (us);
}

/**
NOTE: Do not use this value for absolute time management; use it as a
relative count
*/
unsigned int SysGetTickCount()
{
   struct timeval tv;
   if (gettimeofday(&tv, (struct timezone *)0))
      return 0;
   return (unsigned)((tv.tv_sec & 0x003FFFFF) * 1000L + tv.tv_usec / 1000L);

}

void SysRelativeMsToTimespec (unsigned int ms, struct timespec* ts)
{
   unsigned secs = ms / 1000;
   if (ms % 1000) secs++;

   clock_gettime(CLOCK_REALTIME, ts);
   ts->tv_sec += secs;
}

int SysNetworkGetLocalAddress (std::string& hostName)
{
   // LTODO : Implement SysNetworkGetLocalAddress
   return SIG_PX14_NOT_IMPLEMENTED;		// Linux
}

bool SysFileExists (const char* pathname, bool bDirOkay)
{
	struct stat s;

	if (!stat (pathname, &s)) {
      if (bDirOkay && S_ISDIR(s.st_mode))
         return true;
      if (S_ISREG(s.st_mode))
         return true;
   }
   return false;
}

int ConvertWideCharToAscii (const wchar_t* wbufp,
                            unsigned int str_chrs,
                            char* abufp)
{
   size_t conv_res, in_bytes, out_bytes;
   char* in_bufp;
   iconv_t cd;
   int res;

   cd = iconv_open("", "wchar_t");
   if ((iconv_t)-1 == cd)
      return SIG_PX14_TEXT_CONV_ERROR;

   in_bytes  = str_chrs * sizeof(wchar_t);
   out_bytes = str_chrs * sizeof(char);
   in_bufp = reinterpret_cast<char*>(const_cast<wchar_t*>(wbufp));

   conv_res = iconv(cd, &in_bufp, &in_bytes, &abufp, &out_bytes);
   res = (conv_res == (size_t)(-1)) ? SIG_PX14_TEXT_CONV_ERROR : SIG_SUCCESS;

   iconv_close(cd);

   return res;
}

int ConvertAsciiToWideChar (const char* abufp,
                            unsigned int str_chrs,
                            wchar_t* wbufp)
{
   size_t conv_res, in_bytes, out_bytes;
   char *in_bufp, *out_bufp;
   iconv_t cd;
   int res;

   cd = iconv_open("wchar_t", "");
   if ((iconv_t)-1 == cd)
      return SIG_PX14_TEXT_CONV_ERROR;

   in_bytes  = str_chrs * sizeof(char);
   out_bytes = str_chrs * sizeof(wchar_t);
   in_bufp = const_cast<char*>(abufp);
   out_bufp = reinterpret_cast<char*>(wbufp);

   conv_res = iconv(cd, &in_bufp, &in_bytes, &out_bufp, &out_bytes);
   res = (conv_res == (size_t)(-1)) ? SIG_PX14_TEXT_CONV_ERROR : SIG_SUCCESS;

   iconv_close(cd);

   return res;
}

/// Linux: Obtain a handle to PX14400 device
dev_handle_t GetDeviceHandle (unsigned int brdNum)
{
   char nameBuf[64];

   // Create the device's name.
   memset (nameBuf, 0, sizeof(nameBuf));
   if (-1 == snprintf(nameBuf, 64, "/dev/sig_px14400%d", brdNum))
      return -1;

   // Open the device.
   return open(nameBuf, O_RDWR);
}

/// WIN: Release a device handle
void ReleaseDeviceHandle (dev_handle_t h)
{
   close(h);
}

/// Linux: Send a request to the PX14400 driver
int DriverRequest (HPX14 hBrd,
                   io_req_t req,
                   void* p,
                   size_t in_bytes,
                   size_t out_bytes)
{
   int res;

   res = ioctl(PX14_H2B(hBrd)->m_hDev, req, p);

   if (res == 0)
      return SIG_SUCCESS;
   if (res == -1)
      return SIG_ERROR;

   return -res;
}

// Establish a TCP/IP connection to the given server and port
int SysNetworkConnectTcp (sys_socket_t* sockp,
                          const char* server,
                          unsigned short port)
{
   sys_socket_t sock_use;
   int res;

   sock_use = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (sock_use == PX14_INVALID_SOCKET)
      return SIG_PX14_SOCKET_ERROR;

   struct sockaddr_in service;
   service.sin_family = AF_INET;
   service.sin_addr.s_addr = inet_addr(server);
   service.sin_port = htons(port);
   res = connect(sock_use, reinterpret_cast<struct sockaddr*>(&service), sizeof(service));
   if (0 != res)
   {
      SysCloseSocket(sock_use);
      return SIG_PX14_CONNECTION_REFUSED;
   }

   *sockp = sock_use;
   return SIG_SUCCESS;
}

/// Sets name of calling thread; useful for debugging
int SysSetThreadName (const char* namep)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

#ifndef PX14PP_NO_LINUX_CONSOLE_HELP_FUNCS

#include <termios.h>

PX14API getch_PX14()
{
   struct termios oldt, newt;
   int ch;

   tcgetattr (STDIN_FILENO, &oldt);
   newt = oldt;
   newt.c_lflag &= ~(ICANON | ECHO);
   tcsetattr( STDIN_FILENO, TCSANOW, &newt );
   ch = getchar();
   tcsetattr( STDIN_FILENO, TCSANOW, &oldt );

   return ch;
}

// Equivalent of old DOS kbhit function
PX14API kbhit_PX14()
{
   fd_set set;
   struct timeval tv;
   struct termios t;

   memset(&tv, 0, sizeof(tv));

   tcgetattr(0, &t);
   t.c_lflag &= ~ICANON;
   tcsetattr(0, TCSANOW, &t);

   FD_ZERO(&set);
   FD_SET(0, &set);

   select(1, &set, 0, 0, &tv);

   if (FD_ISSET(0, &set))
      return 1;

   return 0;
}

#endif // ifndef PX14PP_NO_LINUX_CONSOLE_HELP_FUNCS

#endif	// end Linux platform stuff

///////////////////////////////////////////////
//                                           //
// Platform generic implementations          //
//                                           //
///////////////////////////////////////////////

bool _ParseStrVerCmp (const std::string& str,
                      unsigned& a,
                      unsigned& b,
                      unsigned& c,
                      unsigned& d)
{
   std::string::size_type idx, pos;
   unsigned int vc[4] = {0,0,0,0};
   char* endp;
   int i;

   std::string ss, s(str);
   TrimString(s);

   for (idx=pos=0,i=0; (i<4)&&(idx!=std::string::npos); i++)
   {
      idx = s.find('.', pos);
      ss = s.substr(pos,
                    (idx == std::string::npos) ? std::string::npos : idx - pos);
      pos += ss.length() + 1;

      vc[i] = strtol(ss.c_str(), &endp, 10);
      if (*endp) return false;
   }

   a = vc[0];
   b = vc[1];
   c = vc[2];
   d = vc[3];

   return true;
}

bool ParseStrVer32 (const std::string& str, unsigned& ver)
{
   unsigned a, b, c, d;

   if (!_ParseStrVerCmp(str, a, b, c, d))
      return false;
   if ((a > 0xFF) || (b > 0xFF) || (c > 0xFF) || (d > 0xFF))
      return false;

   ver = (a << 24) | (b << 16) | (c << 8) | d;
   return true;
}

bool ParseStrVer64 (const std::string& str, unsigned long long& ver)
{
   unsigned a, b, c, d;

   if (!_ParseStrVerCmp(str, a, b, c, d))
      return false;
   if ((a > 0xFFFF) || (b > 0xFFFF) || (c > 0xFFFF) || (d > 0xFFFF))
      return false;

   ver =
      ((unsigned long long)a << 48) |
      ((unsigned long long)b << 32) |
      ((unsigned long long)c << 16) |
      (unsigned long long)d;
   return true;
}

std::string& TrimString (std::string& s, bool bLeading, bool bTrailing)
{
   int nremove;

   if (bLeading)
   {
      std::string::iterator i(s.begin());
      for (nremove=0; i!=s.end() && isspace(*i); i++)
         nremove++;
      if (nremove)
         s.erase(0, nremove);
   }

   if (bTrailing)
   {
      std::string::reverse_iterator r(s.rbegin());
      for (nremove=0; r!=s.rend() && isspace(*r); r++)
         nremove++;
      if (nremove)
         s.erase(s.length() - nremove);
   }

   return s;
}

int ConvertAsciiToWideCharAlloc (const char* abufp,
                                 wchar_t** bufpp,
                                 unsigned int str_chrs)
{
   wchar_t* tryp;
   int res;

   SIGASSERT_POINTER(bufpp, wchar_t*);
   if (NULL == bufpp)
      return SIG_INVALIDARG;

   if (!abufp)
      tryp = NULL;
   else
   {
      if (str_chrs == UINT_MAX)
         str_chrs = static_cast<unsigned int>(1 + strlen(abufp));

      // Allocate the wide char buffer
      tryp = reinterpret_cast<wchar_t*>(
                                        my_malloc(str_chrs * sizeof(wchar_t)));
      if (NULL == tryp)
         return SIG_OUTOFMEMORY;

      res = ConvertAsciiToWideChar(abufp, str_chrs, tryp);
      if (SIG_SUCCESS != res)
      {
         my_free(tryp);
         return res;
      }
   }

   *bufpp = tryp;
   return SIG_SUCCESS;
}

int AllocAndCopyString (const char* srcp, char** dstpp)
{
   int str_len;
   char* newp;

   SIGASSERT_NULL_OR_POINTER(srcp, char);
   SIGASSERT_POINTER(dstpp, char);
   if (NULL == dstpp)
      return SIG_INVALIDARG;

   if (NULL == srcp)
      *dstpp = NULL;
   else
   {
      str_len = 1 + static_cast<int>(strlen(srcp));
      if (NULL == (newp = my_malloc_arrayT<char>(str_len)))
         return SIG_OUTOFMEMORY;

#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
      strncpy_s (newp, str_len, srcp, str_len - 1);
#else
      strncpy (newp, srcp, str_len);
#endif
      *dstpp = newp;
   }

   return SIG_SUCCESS;
}

int MakeTimeString (const time_t& t, std::string& s)
{
   try
   {
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
      struct tm tms;
      if (0 == localtime_s(&tms, &t))
      {
         char sBuf[64];
         if (0 == asctime_s(sBuf, &tms))
         {
            s.assign(sBuf);
            TrimString(s);
            return SIG_SUCCESS;
         }
      }
#else
      struct tm* tmr = localtime(&t);
      if (NULL != tmr)
      {
         char* bufp = asctime(tmr);
         if (NULL != bufp)
         {
            s.assign(bufp);
            TrimString(s);
            return SIG_SUCCESS;
         }
      }
#endif
   }
   catch (std::bad_alloc)
   {
      return SIG_OUTOFMEMORY;
   }

   return SIG_ERROR;
}

/** @brief Returns elapsed time between two tick counts

  SysGetTickCount can be used to obtain the current tick count, which
  should only be used to track time differences and not absolute time

  This function will handle overflow of tick count

  @param tick0
  Tick count at beginning of duration
  @param tick1
  Tick count at end of duration
  */
unsigned int SysGetElapsedTicks (unsigned int tick0, unsigned int tick1)
{
   unsigned elapsed_ticks;

   // General case, tick0 < tick1 = no roll over
   if (tick0 <= tick1)
      elapsed_ticks = tick1 - tick0;
   else
      elapsed_ticks = tick1 + (0xFFFFFFFF - tick0);

   return elapsed_ticks;
}




