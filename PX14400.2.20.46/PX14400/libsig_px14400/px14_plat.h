/** @file   px14_plat.h
  @brief   Top-level include for PX14 library implementation
  */
#ifndef __px14_plat_header_defined
#define __px14_plat_header_defined

// If defined, exports that use UNICODE strings are not implemented
//#define PX14PP_NO_UNICODE_SUPPORT

// Note on <stdint.h>: This (C99) standard header file may not be defined
//  for all versions of Microsoft C compilers. In this case, there are
//  third-party versions of the header file that can be downloaded that are
//  compatible with the Windows API.
// Signatec uses: http://msinttypes.googlecode.com/svn/trunk/stdint.h

// C includes
#include <stdint.h>      // See note above
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

// C++ includes
#include <sstream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <new>
#include <set>

#include <pthread.h>

// Standard include on Linux; pthread-win32 implements on Windows
#include <semaphore.h>

// We are exporting DLL symbols
#define _PX14_IMPLEMENTING_DLL
// Define PX14S_JTAG_STREAM structure please
#define PX14PP_WANT_PX14S_JTAG_STREAM

#ifdef _WIN32

///////////////////////////////////////////
//                                       //
// Windows platform specific definitions //
//                                       //
///////////////////////////////////////////

#define WINVER         0x0501         // Windows XP
#define _WIN32_WINNT   0x0501         // Windows XP
#define _WIN32_IE      0x0600         // IE 6

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
// Don't try to link with PX14 import library
#define PX14PP_NO_LINK_LIBRARY

#ifdef _WIN64
#define PX14_LIB_64BIT_IMP
#endif

// -- Windows includes
#include <windows.h>    // Standard Windows definitions
#include <winioctl.h>   // Defines CTL_CODE for device IO
#include <tchar.h>      // Defines generic text mapping routines
#include <Winsock2.h>
#include <shlwapi.h>

// -- CRT includes
#include <crtdbg.h>      // C Runtime debugging
#include <io.h>          // Low-level IO
#include <fcntl.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/stat.h>

// -- Signatec includes
#include "px14_plat_kern_win32.h"

/// An invalid device handle
#define PX14_BAD_DEV_HANDLE      INVALID_HANDLE_VALUE
/// An invalid socket handle
#define PX14_INVALID_SOCKET      INVALID_SOCKET
/// A handle to a system device
#define dev_handle_t             HANDLE
/// A handle to a file
#define sys_file_t               HANDLE
/// A network socket
#define sys_socket_t             SOCKET
/// A device IO operation request
#define io_req_t                 DWORD
/// A system error type
#define sys_error_t              ULONG
/// System error name
#define PX14_SYS_ERR_NAME_A      "LastError"
/// Case insensitive string comparison
#define strcmp_nocase            _stricmp
// Close a network socket
#define SysCloseSocket           closesocket
/// More stupid Microsoft; can't defined _POSIX_ because it breaks ATL stuff
#define isfinite                 _finite
#define snprintf                 _snprintf
#define sys_file_open            _open
#define sys_file_read            _read
#define sys_file_write           _write
#define sys_file_close           _close
#define O_RDONLY                 _O_RDONLY
#define O_WRONLY                 _O_WRONLY
#define O_RDWR                   _O_RDWR
#define O_APPEND                 _O_APPEND
#define O_CREAT                  _O_CREAT
#define O_TRUNC                  _O_TRUNC
#define O_EXCL                   _O_EXCL
#define O_TEXT                   _O_TEXT
#define O_BINARY                 _O_BINARY
#define O_RAW                    _O_BINARY
#define O_TEMPORARY              _O_TEMPORARY
#define O_NOINHERIT              _O_NOINHERIT
#define O_SEQUENTIAL             _O_SEQUENTIAL
#define O_RANDOM                 _O_RANDOM

/// DEBUG: The mighty assertion macro.
#define SIGASSERT                _ASSERT
/// DEBUG: Asserts that the given pointer is valid.
#define SIGASSERT_POINTER(p,type)               \
   _ASSERT(_CrtIsValidPointer(((const void*)(p)),sizeof(type),TRUE))
/// DEBUG: Asserts that the given pointer is valid or NULL
#define SIGASSERT_NULL_OR_POINTER(p,type)            \
   _ASSERT((NULL==(p)) || (_CrtIsValidPointer(((const void*)(p)),sizeof(type),TRUE)))

#ifdef _DEBUG
/// DEBUG: Using debug versions of malloc/free
# define my_malloc(size)   _malloc_dbg(size, _NORMAL_BLOCK, __FILE__, __LINE__)
/// DEBUG: Using debug versions of malloc/free
# define my_free(p)        _free_dbg(p, _NORMAL_BLOCK)
#else
# define my_malloc(size)   malloc(size)
# define my_free(p)        free(p)
#endif

#ifdef _DEBUG
// Disable "conditional expression is constant" warning (for PX14_CT_ASSERT)
# pragma warning(disable: 4127)
#endif
// Disable "A structure or union contains an array with zero size" warning
#pragma warning(disable : 4200)

// To whom it may concern, please link with these import libraries:
#pragma comment (lib, "Ws2_32.lib")       // Winsock
#pragma comment (lib, "shlwapi.lib")      // Shell lightweight API

// Olde
//#pragma comment (lib, "pthreadVC2.lib")         // pthreads
//#pragma comment (lib, "libxml2.lib")            // libxml2
//#pragma comment (lib, "zlibwapi.lib")           // zlib

#ifdef _WIN64
#pragma comment (lib, "libxml2_sig_vc08_64.lib")    // libxml2
#pragma comment (lib, "pthreadVC2_sig_vc08_64.lib") // pthreads
#pragma comment (lib, "zlibwapi_sig_vc08_64.lib")   // zlib
#else
#pragma comment (lib, "libxml2_sig_vc08_32.lib")    // libxml2
#pragma comment (lib, "pthreadVC2_sig_vc08_32.lib") // pthreads
#pragma comment (lib, "zlibwapi_sig_vc08_32.lib")   // zlib
#endif

#endif      // end Windows platform stuff

#ifdef __linux__
# define _PX14PP_LINUX_PLATFORM
#endif

#ifdef _PX14PP_LINUX_PLATFORM

/////////////////////////////////////////
//                                     //
// Linux platform specific definitions //
//                                     //
/////////////////////////////////////////

// Linux includes
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>   // htonl, et al
#include <sys/timeb.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <iconv.h>

#ifdef _LP64
#define PX14_LIB_64BIT_IMP
#endif

// -- Signatec includes
#include "px14_plat_kern_linux.h"

#define _PX14_LIKELY(e)             __builtin_expect((e),1)
#define _PX14_UNLIKELY(e)           __builtin_expect((e),0)

/// An invalid device handle
#define PX14_BAD_DEV_HANDLE        (-1)
/// An invalid socket handle
#define PX14_INVALID_SOCKET        (-1)
/// A handle to a system device
#define dev_handle_t               int
/// A handle to a file
#define sys_file_t                 int
/// A network socket
#define sys_socket_t               int
/// A device IO operation request
#define io_req_t                   int
/// A system error type
#define sys_error_t                int
/// System error name
#define PX14_SYS_ERR_NAME_A        "errno"
/// Case insensitive string comparison
#define strcmp_nocase              strcasecmp
// Close a network socket
#define SysCloseSocket             close
#define sys_file_open              open
#define sys_file_read              read
#define sys_file_write             write
#define sys_file_close             close

#define O_BINARY                   0
#define O_SEQUENTIAL               0

/// DEBUG: The mighty assertion macro.
#define SIGASSERT                  assert
/// DEBUG: Asserts that the given pointer is valid.
#define SIGASSERT_POINTER(p,type)  assert(p)
/// DEBUG: Asserts that the given pointer is valid or NULL
#define SIGASSERT_NULL_OR_POINTER(p,type) ((void)p)

#define my_malloc(size)            malloc(size)
#define my_free(p)                 free(p)

#endif      // end Linux platform stuff

#ifdef _DEBUG
/// DEBUG: Compile-time assertion
# define PX14_CT_ASSERT(e)      do{int _[(e)?1:-1];_[0]=0;}while(0)
#else
# define PX14_CT_ASSERT(e)
#endif

// _PX14_LIKELY and _PX14_UNLIKELY are for platforms that support branch
//  prediction hints
#ifndef _PX14_LIKELY
#define _PX14_LIKELY(e)            e
#endif
#ifndef _PX14_UNLIKELY
#define _PX14_UNLIKELY(e)          e
#endif

// If given parameter is not SIG_SUCCESS it is returned as function return
//  value. Do *NOT* pass function call as argument as it might be called
//  twice.
#define PX14_RETURN_ON_FAIL(res)   if (_PX14_UNLIKELY((res)<0)) return (res)

/// Macro to return minimum of two values
#define PX14_MIN(a,b)         ((a)<(b)?(a):(b))
/// Macro to return maximum of two values
#define PX14_MAX(a,b)         ((a)>(b)?(a):(b))

/// Returns lower 16-bit word of (l)
#define PX14_LOWORD(l)                  \
   ((unsigned short)(((unsigned int)(l))         & 0xFFFF))
/// Returns upper 16-bit word of (l)
#define PX14_HIWORD(l)                  \
   ((unsigned short)((((unsigned int)(l)) >> 16) & 0xFFFF))

// -- Common size prefixes
#define _1kibi                  1024
#define _1mebi                  1048576               // 1024 * 1024
#define _1gibi                  1073741824            // 1024 * 1024 * 1024
#define _1tebi_I64              1099511627776ULL      // 1024 * 1024 * 1024 * 1024 (64-bit)
#define _1kilo                  1000
#define _1mega                  1000000
#define _1giga                  1000000000
#define _1tera_I64              1000000000000ULL      // 64-bit

//
// Non-exported platform-independent function prototypes
//

// -- Device management

// Obtain a handle to PX14 device
dev_handle_t GetDeviceHandle (unsigned int brdNum);
// Release a device handle
void ReleaseDeviceHandle (dev_handle_t h);

// -- Error management

// Get platform-specific system error value
sys_error_t GetSystemErrorVal();

// Set platform-specifuc system error value
void SetSystemErrorVal(sys_error_t val);

// Generate a user-friendly string for given system error
bool GenSysErrorString (sys_error_t err_val, std::string& str);

#ifndef PX14PP_NO_XML_SUPPORT

int GetLibXmlErrorText (std::string& sText);

#endif

// -- File management

// Obtain path to use for temporary files
int SysGetTemporaryFilesFolder (std::string& path);
// Permenantly delete file
int SysDeleteFile (const char* filepathp);

#define PX14_SYS_OPEN_COOKIE_INIT      (-1)
struct _PX14S_FILE_WRITE_PARAMS_tag;
// Open a file
int SysOpenFile (const char* pathname,
                 int oflag,
                 unsigned long long total_bytes_to_move,
                 const _PX14S_FILE_WRITE_PARAMS_tag* params,
                 int& fd, int& SysOpenCookie);

// Checks to see if file exists; directories okay unless !bDirOkay
bool SysFileExists (const char* pathname, bool bDirOkay = false);

// -- Time management

/// Put calling thread to sleep for ms milliseconds
void SysSleep (unsigned int ms);
void SysMicroSleep (unsigned int us);
/// Obtain ms tick count; use for relative measurements only
unsigned int SysGetTickCount();
/// Returns elapsed time between two tick counts
unsigned int SysGetElapsedTicks (unsigned int tick0, unsigned int tick1);

/// Convert given absolute millisecond count to a timespec
void SysRelativeMsToTimespec (unsigned int ms, struct timespec* ts);

// -- Thread management

#ifdef _DEBUG
// Sets name of calling thread; useful for debugging
int SysSetThreadName (const char* namep);
#endif

// -- Character conversion

// Convert a UNICODE string to equivalent ASCII string
int ConvertWideCharToAscii (const wchar_t* wbufp, unsigned int str_chrs,
                            char* abufp);
// Convert an ASCII string to equivalent UNICODE string
int ConvertAsciiToWideChar (const char* abufp, unsigned int str_chrs,
                            wchar_t* wbufp);

// Convert an ASCII string to equivalent UNICODE string; allocates buffer
int ConvertAsciiToWideCharAlloc (const char* abufp, wchar_t** bufpp,
                                 unsigned int str_chrs = UINT_MAX);

// -- String management

// Allocate and copy given NULL terminated string
int AllocAndCopyString (const char* srcp, char** dstpp);

// Remove leading and/or trailing whitespace from the given string
std::string& TrimString (std::string& s,
                         bool bLeading = true,
                         bool bTrailing = true);

// -- Version number management

// Obtain current PX14 general software release
int SysGetPX14SoftwareRelease (unsigned long long* verp);

// Parse string into 32-bit version a.b.c.d
bool ParseStrVer32 (const std::string& str, unsigned& ver);
// Parse string into 64-bit version a.b.c.d
bool ParseStrVer64 (const std::string& str, unsigned long long& ver);

// -- Networking routines

int GetSocketsErrorText (std::string& sText);

// Initialize local sockets implementation
int SysSocketsInit();
// Cleanup local sockets implementation
int SysSocketsCleanup();

// Obtain local host address
int SysNetworkGetLocalAddress (std::string& hostName);

// Obtain textual description of last sockets error
int GetSocketsErrorText (std::string& sText);

// Establish a TCP/IP connection to the given server and port
int SysNetworkConnectTcp (sys_socket_t* sockp,
                          const char* server, unsigned short port);


#endif // __px14_plat_header_defined

