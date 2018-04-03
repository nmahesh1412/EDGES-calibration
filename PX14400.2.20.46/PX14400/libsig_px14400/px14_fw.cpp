/** @file px14_fw.cpp
*/
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "sigsvc_utility.h"		// Defines CAutoXmlDocPtr
#include "px14_xsvf_player.h"
#include "px14_util.h"
#include "px14_jtag.h"
#include "px14_private.h"
#include "px14_fwctx.h"
#include "px14_zip.h"

#define PP_VIRTUAL_DEVICE_USES_NEW_JTAG_CHAIN	1
#define PX14UFWF_SKIP_FW_UPLOAD				0x40000000
// This is way higher than actual device count; currently just using
//  this to prevent infinite looping when probing chain
#define PX14_MAX_JTAG_DEVICE_COUNT			64

#define PX14IRLEN_XC5VLX50T					10
#define PX14IRLEN_XC5VSX50T					10
#define PX14IRLEN_XC5VSX95T					10
#define PX14FW2_FW_NOTES_NAME				"fw_readme.txt"
#define JCIF_PATCH_XSVF_IDCODE_32P_TO_16P	0x0001
#define JCIF__DEFAULT						0

// Module-local data types ---------------------------------------------- //

typedef struct _PX14S_JTAG_CHAIN_ITEM_tag
{
   // - JTAG component info

   int				irLen;		// Instruction register length
   int				fw_chunk;	// Firmware chunk ID for comp or -1
   unsigned int	ver_have;	// Current version for this item
   unsigned short	skipf;		// Skip flags; used with PX14EA_FWINFO
   unsigned short	skipf_state;
   unsigned short	pre_rel_flag; // Pre-release bit @ PX14EA_PRE_RELEASE
   unsigned short  flags;		// JCIF_*

   // - Config EEPROM addresses for this version info

   int		ea_ver_low;
   int		ea_ver_hi;
   int		ea_prev_ver_low;
   int		ea_prev_ver_high;
   int		ea_cust_enum;

   // - Extra shift info

   int		lHir;		// Header instruction reg bit count
   int		lTir;		// Tail instruction reg bit count
   int		lHdr;		// Header data reg bit count
   int		lTdr;		// Tail data reg bit count
   int		lHdrFpga;	// Header FPGA info

   _PX14S_JTAG_CHAIN_ITEM_tag() : irLen(0), fw_chunk(-1), ver_have(0),
   skipf(0), skipf_state(0xFFFF), pre_rel_flag(0), flags(JCIF__DEFAULT),
   ea_ver_low(-1), ea_ver_hi(-1), ea_prev_ver_low(-1), ea_prev_ver_high(-1),
   ea_cust_enum(-1), lHir(0), lTir(0), lHdr(0), lTdr(0), lHdrFpga(0)
   {
   }

} PX14S_JTAG_CHAIN_ITEM;

typedef std::vector<PX14S_JTAG_CHAIN_ITEM> JtagChainVect;
typedef std::list<unsigned> IdCodeList;

int PatchXsvf_32p_to_16p (const char* pXsvfFile);

// Module-local function prototypes ------------------------------------- //

static int UploadFromFwUpdateFile (CMyXsvfPlayer& xp,
                                   const std::string& fwPath,
                                   unsigned int flags,
                                   unsigned int& out_flags);

static int UploadFromFwUpdateFileVer2(unsigned int flags,
                                      CMyXsvfPlayer& xp,
                                      const std::string& fwPath,
                                      const std::string& ctxPath,
                                      const std::string& tmpPath,
                                      unsigned int& out_flags);

static int VerifyFw2Compatibility (const CFwContextPX14& ctx, HPX14 hBrd);
static int ConstructJtagInfo (HPX14 hBrd, JtagChainVect& jcv);
static int ConstructJtagInfoDsNew (HPX14 hBrd, const IdCodeList& lstIdCodes, JtagChainVect& jcv);
static int SetupJtagShiftInfo (JtagChainVect& jcv);
static int PreLoadFwChunk (PX14S_JTAG_CHAIN_ITEM& jci,
                           CFwContextPX14& fwCtx,
                           CMyXsvfPlayer& xp,
                           const std::string& fwPath,
                           const std::string& tmpPath,
                           unsigned int flags);
static int DoFwChunk (PX14S_JTAG_CHAIN_ITEM& jci,
                      CFwContextPX14& fwCtx, CMyXsvfPlayer& xp,
                      const std::string& fwPath, const std::string& tmpPath,
                      unsigned int flags);
static int DoPostFwChunkUpload (HPX14 hBrd, PX14S_JTAG_CHAIN_ITEM& jci,
                                const CFwContextPX14::CFirmwareChunk& fwc);
static int PostFwUpload (HPX14 hBrd,
                         CFwContextPX14& fwCtx, unsigned int flags);
static int ReadChainSettings(HPX14 hBrd, IdCodeList& m_lstIdCodes);

static int DetectPciFwRequirements (HPX14 hBrd,
                                    int& fwc1, int& fwc2, int& fpgaIrLen);
static int DetectSabFwRequirements (HPX14 hBrd,
                                    int& fwc1, int& fwc2, int& fpgaIrLen);

static void MakeStrVer64 (const unsigned long long& ver, std::string& str);
static void MakeStrVer32 (unsigned ver, std::string& str);
static int IsJtagChainDsNew (HPX14 hBrd);

// PX14 library exports implementation --------------------------------- //

/** @brief Obtain PX14 firmware release notes from firmware file

  Call this function to extract firmware release notes into a temporary
  file. The user can then read/display the information in any manner they
  wish. The caller should delete the temporary file when done. (The
  temporary file should be generated in the system-designated area for
  temporary files so if the caller doesn't delete the file, the system
  should at some point.)

  @param fw_pathnamep
  A pointer to a NULL terminated string that defines the fully
  qualified pathname of the PX14 firmware update file to interrogate
  @param notes_pathpp
  Optional. A pointer to a char pointer that will receive the address
  of the library allocated buffer that contains the pathname of the
  extracted firmware release notes. It is up to the caller to free
  this buffer via the FreeMemoryPX14 function. If this parameter is
  NULL then the release notes will not be extracted. In the event that
  no release notes are embedded the function will specify an address
  of NULL.
  @param severityp
  Optional. A pointer to an int variable that will receive the
  severity of the firmware release notes. This value is interpreted
  as follows. 0 means no release notes are available. 1 means that
  firmware release notes are available. 2 means that the firmware
  release notes are available and they contain important information
  that the operator should read prior to uploading the firmware.

  @return
  Returns SIG_SUCCESS or one of the SIG_* error codes on error
  */
PX14API ExtractFirmwareNotesAPX14 (const char* fw_pathnamep,
                                   char** notes_pathpp,
                                   int* severityp)
{
   int res;

   SIGASSERT_POINTER(fw_pathnamep, const char);
   SIGASSERT_NULL_OR_POINTER(notes_pathpp, char*);
   SIGASSERT_NULL_OR_POINTER(severityp, int);

   if (NULL == fw_pathnamep)
      return SIG_PX14_INVALID_ARG_1;

   std::string pathTemp;
   res = SysGetTemporaryFilesFolder(pathTemp);
   PX14_RETURN_ON_FAIL(res);

   // Extract out the XML context file that contains firmware details
   std::string pathCtx(pathTemp);
   pathCtx.append("PX14FWCTX.XML");
   res = UnzipFile(fw_pathnamep, "context_v2.xml", pathCtx.c_str());
   PX14_RETURN_ON_FAIL(res);

   // Parse the firmware context file
   CFwContextPX14 fwCtx;
   res = fwCtx.ParseFrom (pathCtx.c_str());
   SysDeleteFile (pathCtx.c_str());
   PX14_RETURN_ON_FAIL(res);

   if (severityp)
      *severityp = fwCtx.readme_sev;

   if (notes_pathpp)
   {
      std::string pathReadme(pathTemp);
      pathReadme.append("fw_readme.txt");

      // Unzip will fail if readme isn't there
      res = UnzipFile(fw_pathnamep, PX14FW2_FW_NOTES_NAME, pathReadme.c_str());
      if (SIG_SUCCESS == res)
      {
         res = AllocAndCopyString(pathReadme.c_str(), notes_pathpp);
         PX14_RETURN_ON_FAIL(res);
      }
      else
         *notes_pathpp = NULL;
   }

   return SIG_SUCCESS;
}

/** @brief Obtain firmware version information (ASCII)

  @param fw_pathnamep
  A pointer to a NULL terminated string that defines the fully
  qualified pathname of the PX14 firmware update file to interrogate
  @param info
  A pointer to a PX14S_FW_VER_INFO structure that the function will
  write. The caller should set the struct_size member prior to invoking
  the function.

  @return
  Returns SIG_SUCCESS or one of the SIG_* error codes on error
  */
PX14API QueryFirmwareVersionInfoAPX14 (const char* fw_pathnamep,
                                       PX14S_FW_VER_INFO* infop)
{
   int res;

   PX14_ENSURE_POINTER(PX14_INVALID_HANDLE, fw_pathnamep, char, NULL);
   PX14_ENSURE_POINTER(PX14_INVALID_HANDLE, infop, PX14S_FW_VER_INFO, NULL);
   PX14_ENSURE_STRUCT_SIZE(PX14_INVALID_HANDLE, infop, _PX14SO_FW_VER_INFO_V1, NULL);

   std::string pathTemp;
   res = SysGetTemporaryFilesFolder(pathTemp);
   PX14_RETURN_ON_FAIL(res);

   // Extract out the XML context file that contains firmware details
   std::string pathCtx(pathTemp);
   pathCtx.append("PX14FWCTX.XML");
   res = UnzipFile(fw_pathnamep, "context_v2.xml", pathCtx.c_str());
   PX14_RETURN_ON_FAIL(res);

   // Parse the firmware context file
   CFwContextPX14 fwCtx;
   res = fwCtx.ParseFrom (pathCtx.c_str());
   SysDeleteFile (pathCtx.c_str());
   PX14_RETURN_ON_FAIL(res);

   infop->fw_pkg_ver = fwCtx.fw_ver_pkg;
   infop->fw_pkg_cust_enum = fwCtx.pkg_cust_enum;
   infop->readme_sev = fwCtx.readme_sev;
   infop->extra_flags = fwCtx.fw_flags;

   return SIG_SUCCESS;
}

/// Upload PX14 firmware
PX14API UploadFirmwareAPX14 (HPX14 hBrd,
                             const char* fw_pathnamep,
                             unsigned int flags,
                             unsigned int* out_flagsp,
                             PX14_FW_UPLOAD_CALLBACK callbackp,
                             void* callback_ctx)
{
   unsigned xsvf_flags, out_flags;
   int res;

   SIGASSERT_NULL_OR_POINTER(out_flagsp, unsigned int);
   SIGASSERT_POINTER(fw_pathnamep, char);
   if (NULL == fw_pathnamep)
      return SIG_PX14_INVALID_ARG_2;

   // Local devices only
   if (!IsHandleValidPX14(hBrd))
      return SIG_PX14_INVALID_HANDLE;
   if (IsDeviceRemotePX14(hBrd) > 0)
      return SIG_PX14_REMOTE_CALL_NOT_AVAILABLE;

   // Grab given file's extension
   std::string fw_pathname(fw_pathnamep);
   TrimString(fw_pathname);
   std::string::size_type posExt(fw_pathname.rfind('.'));
   if (posExt == std::string::npos)
      return SIG_PX14_UNKNOWN_FW_FILE;
   std::string fw_ext(fw_pathname.substr(posExt+1));

   CMyXsvfPlayer xp(hBrd);
   if (callbackp)
      xp.SetCallback(callbackp, callback_ctx);
   xsvf_flags = PX14XSVFF__DEFAULT;
   if (IsDeviceVirtualPX14(hBrd))
      xsvf_flags |= PX14XSVFF_VIRTUAL;
   xp.SetFlags(xsvf_flags);

   out_flags = 0;

   // Is this a straight XSVF file?
   if (0 == strcmp_nocase(fw_ext.c_str(), "xsvf"))
   {
      // Given an XSVF file, the native format
      res = xp.xsvfExecute(fw_pathname.c_str());
      if (SIG_SUCCESS != res)
         return SIG_PX14_FIRMWARE_UPLOAD_FAILED;

      if (out_flagsp) *out_flagsp = 0;
      return SIG_SUCCESS;
   }

   res = UploadFromFwUpdateFile (xp, fw_pathname, flags, out_flags);
   if ((SIG_SUCCESS == res) && out_flagsp)
      *out_flagsp = out_flags;

   return res;
}

PX14API GetJtagIdCodesAPX14 (HPX14 hBrd, char** bufpp)
{
   int res;

   res = ValidateHandle(hBrd);
   PX14_RETURN_ON_FAIL(res);

   std::list<unsigned int> lstCodes;
   res = ReadChainSettings(hBrd, lstCodes);
   PX14_RETURN_ON_FAIL(res);

   std::ostringstream oss;
   std::list<unsigned int>::iterator iCode(lstCodes.begin());
   for (; iCode!=lstCodes.end(); iCode++)
      oss << *iCode << ' ';

   std::string s(oss.str());
   return AllocAndCopyString(s.c_str(), bufpp);
}


// Module private function implementation  ------------------------------ //

void MakeStrVer32 (unsigned ver, std::string& str)
{
   std::ostringstream oss;

   oss << ((ver >> 24) & 0xFF) << "."
      << ((ver >> 16) & 0xFF) << "."
      << ((ver >>  8) & 0xFF) << "."
      << ((ver >>  0) & 0xFF) << ".";
   str.assign(oss.str());
}

void MakeStrVer64 (const unsigned long long& ver, std::string& str)
{
   std::ostringstream oss;

   oss << ((ver >> 48) & 0xFFFF) << "."
      << ((ver >> 32) & 0xFFFF) << "."
      << ((ver >> 16) & 0xFFFF) << "."
      << ((ver >>  0) & 0xFFFF) << ".";
   str.assign(oss.str());
}

int UploadFromFwUpdateFile (CMyXsvfPlayer& xp,
                            const std::string& fwPath,
                            unsigned int flags,
                            unsigned int& out_flags)
{
   int res;

   std::string pathTemp;
   res = SysGetTemporaryFilesFolder(pathTemp);
   PX14_RETURN_ON_FAIL(res);

   // Extract out the XML context file that contains firmware details
   std::string pathCtx(pathTemp);
   pathCtx.append("PX14FWCTX.XML");
   res = UnzipFile(fwPath.c_str(), "context_v2.xml", pathCtx.c_str());
   PX14_RETURN_ON_FAIL(res);

   res = UploadFromFwUpdateFileVer2 (flags, xp, fwPath,
                                     pathCtx, pathTemp, out_flags);
   SysDeleteFile(pathCtx.c_str());

   return res;
}

int UploadFromFwUpdateFileVer2(unsigned int flags,
                               CMyXsvfPlayer& xp,
                               const std::string& fwPath,
                               const std::string& ctxPath,
                               const std::string& tmpPath,
                               unsigned int& out_flags)
{
   int res, fwchunks_uploaded, fwchunks_uptodate, file_count, file_cur;
   HPX14 hBrd;

   hBrd = xp.GetBoardHandle();

   // Parse the firmware context file
   CFwContextPX14 fwCtx;
   res = fwCtx.ParseFrom (ctxPath.c_str());
   PX14_RETURN_ON_FAIL(res);

   // Check that given firmware is compatible with board
   res = VerifyFw2Compatibility(fwCtx, hBrd);
   PX14_RETURN_ON_FAIL(res);

   out_flags = 0;

   if (flags & PX14UFWF_COMPAT_CHECK_ONLY)
      return SIG_SUCCESS;

#if defined(_DEBUG) && defined(_WIN32)
   if (!IsDeviceVirtualPX14(hBrd))
   {
      char* p;
      res = GetJtagIdCodesAPX14(hBrd, &p);
      if (SIG_SUCCESS == res)
      {
         OutputDebugStringA("PX14400 JTAG chain: ");
         OutputDebugStringA(p);
         OutputDebugStringA("\n");
         FreeMemoryPX14(p);
      }
   }
#endif

   // Determine our JTAG chain from hardware configuration
   // This also figures out which firmware we need to upload
   JtagChainVect jtagChain;
   res = ConstructJtagInfo (hBrd, jtagChain);
   PX14_RETURN_ON_FAIL(res);
   res = SetupJtagShiftInfo(jtagChain);
   PX14_RETURN_ON_FAIL(res);

   fwchunks_uptodate = 0;
   fwchunks_uploaded = 0;

   // Run through component list to get a total count of components
   JtagChainVect::iterator iComp(jtagChain.begin());
   for (file_count=0; iComp!=jtagChain.end(); iComp++)
   {
      if (iComp->fw_chunk == -1)
         continue;		// No firmware for this component
      CFwContextPX14::FwChunkMap::iterator iChunk(
                                                  fwCtx.fw_chunk_map.find(iComp->fw_chunk));
      if (iChunk == fwCtx.fw_chunk_map.end())
         continue;		// Firmware not in file

      file_count++;
   }
   file_cur = 1;

   // Walk components and upload firmware
   for (iComp=jtagChain.begin(); iComp!=jtagChain.end(); iComp++)
   {
      if (iComp->fw_chunk == -1)
         continue;		// No firmware for this component

      CFwContextPX14::FwChunkMap::iterator iChunk(
                                                  fwCtx.fw_chunk_map.find(iComp->fw_chunk));
      if (iChunk == fwCtx.fw_chunk_map.end())
         continue;		// Firmware not in file

      const CFwContextPX14::CFirmwareChunk& fwc =
         fwCtx.fw_chunk_map[iComp->fw_chunk];

      xp.SetFileCountInfo(file_cur++, file_count);

      res = DoFwChunk(*iComp, fwCtx, xp, fwPath, tmpPath, flags);
      if ((SIG_SUCCESS == res) || (SIG_PX14_FIRMWARE_IS_UP_TO_DATE == res))
      {
         if (SIG_PX14_FIRMWARE_IS_UP_TO_DATE != res)
         {
            if (0 == (flags & PX14UFWF_SKIP_FW_UPLOAD))
            {
               if (fwc.fwc_flags & PX14FWCF_REQUIRES_SHUTDOWN)
                  out_flags |= PX14UFWOUTF_SHUTDOWN_REQUIRED;
               if (fwc.fwc_flags & PX14FWCF_REQUIRES_REBOOT)
                  out_flags |= PX14UFWOUTF_REBOOT_REQUIRED;
            }

            fwchunks_uploaded++;
         }
         else
            fwchunks_uptodate++;
      }
      else if (SIG_PX14_REQUIRED_FW_NOT_FOUND != res)
         return res;
   }

   if (!fwchunks_uploaded && !fwchunks_uptodate)
   {
      // Nothing uploaded or up-to-date; firmware file does not contain
      //  anything relevant for this board.
      return SIG_PX14_REQUIRED_FW_NOT_FOUND;
   }

   if (fwCtx.fw_flags & PX14FWCTXF_VERIFY_FILE)
   {
      // Firmware verified
      out_flags |= PX14UFWOUTF_VERIFIED;
      return SIG_SUCCESS;
   }

   if (fwchunks_uptodate && !fwchunks_uploaded)
   {
      // Didn't upload anything; current firmware is up to date
      out_flags |= PX14UFWOUTF_FW_UP_TO_DATE;
   }

   if (fwchunks_uploaded > 0)
   {
      // Finalize EEPROM update
      return PostFwUpload(hBrd, fwCtx, flags);
   }

   return SIG_SUCCESS;
}

// Troubleshooting function that obtains IDCODES of all components in the
//  JTAG chain
int ReadChainSettings(HPX14 hBrd, IdCodeList& m_lstIdCodes)
{
   unsigned int dwData, dwIdCode, deviceCount;
   int i, bAccepted, res, retVal;

   if (IsDeviceVirtualPX14(hBrd))
   {
#if PP_VIRTUAL_DEVICE_USES_NEW_JTAG_CHAIN

      m_lstIdCodes.push_back(0xE5058093);	// xcf16p
      m_lstIdCodes.push_back(0xC2A96093); // V5LX5t

      unsigned int brd_subrev = PX14BRDREVSUB_1_DR;
      GetBoardRevisionPX14(hBrd, NULL, &brd_subrev);
      if (brd_subrev == PX14BRDREVSUB_1_SP)
      {
         if (PX14_H2B(hBrd)->m_fpgaTypeSab == PX14SABFPGA_V5_SX50T)
         {
            m_lstIdCodes.push_back(0xF5059093); // xcf32p
            m_lstIdCodes.push_back(0x52E9A093); // V5SX50t
         }
         else
         {
            m_lstIdCodes.push_back(0xE5058093); // xcf16p
            m_lstIdCodes.push_back(0xF5059093); // xcf32p
            m_lstIdCodes.push_back(0x52E9A093); // V5SX50t (would be 95 but this is virtual so whatever)
         }
      }
#else
      // Assume chain settings for virtual devices
      m_lstIdCodes.push_back(0xE5058093);	// xcf16p
      m_lstIdCodes.push_back(0xF5059093);	// cxf32p
      m_lstIdCodes.push_back(0xC2A96093); // V5LX5t
      m_lstIdCodes.push_back(0xE5058093); // xcf16p
      m_lstIdCodes.push_back(0xF5059093); // xcf32p
      m_lstIdCodes.push_back(0x52E9A093); // V5SX50t
#endif

      return SIG_SUCCESS;
   }

   retVal = SIG_SUCCESS;

   // Enable JTAG operation
   res = JtagSessionCtrlPX14(hBrd, PX14_TRUE, &bAccepted);
   if ((SIG_SUCCESS != res) || !bAccepted)
      return -1;

   // Enable JTAG
   res = WriteJtagPX14(hBrd, PX14JIO_CTRL, PX14JIO_CTRL);
   if (SIG_SUCCESS != res)
      return res;

   // Go to Test-Logic-Reset by clocking TMS high 5 times
   WriteJtagPX14(hBrd, PX14JIO_TMS, PX14JIO_TMS | PX14JIO_TDI);
   for (i=0; i<5; i++)
      WriteJtagPX14(hBrd, 0, 0, PX14JIOF_PULSE_TCK);

   // -- Go to Shift-DR; keeping TDI high
   WriteJtagPX14 (hBrd, PX14JIO_TDI, PX14JIO_TDI);
   // TLR->RTI
   WriteJtagPX14 (hBrd, 0,           PX14JIO_TMS, PX14JIOF_PULSE_TCK);
   // RTI->SelDR
   WriteJtagPX14 (hBrd, PX14JIO_TMS, PX14JIO_TMS, PX14JIOF_PULSE_TCK);
   // SelDR->CDR
   WriteJtagPX14 (hBrd, 0,           PX14JIO_TMS, PX14JIOF_PULSE_TCK);
   // CDR->SDR
   WriteJtagPX14 (hBrd, 0,           PX14JIO_TMS, PX14JIOF_PULSE_TCK);

   for (deviceCount=0; deviceCount<PX14_MAX_JTAG_DEVICE_COUNT; deviceCount++)
   {
      ReadJtagPX14  (hBrd, &dwData, PX14JIOF_PULSE_TCK);

      // LSB of ID code must be 1.
      if (0 == (dwData & PX14JIO_TDO))
      {
         m_lstIdCodes.push_front(0);
         continue;
      }

      dwIdCode = 0x00000001;

      for (i=1; i<32; i++)
      {
         ReadJtagPX14  (hBrd, &dwData, PX14JIOF_PULSE_TCK);
         if (dwData & PX14JIO_TDO)
            dwIdCode |= (1 << i);
      }

      if (0xFFFFFFFF == dwIdCode)
         break;

      m_lstIdCodes.push_front(dwIdCode);
   }

   if (deviceCount >= PX14_MAX_JTAG_DEVICE_COUNT)
      retVal = SIG_PX14_JTAG_IO_ERROR;

   // Go back to Test-Logic-Reset by clocking TMS high 5 times
   WriteJtagPX14(hBrd, PX14JIO_TMS, PX14JIO_TMS | PX14JIO_TDI);
   for (i=0; i<5; i++)
      WriteJtagPX14(hBrd, 0, 0, PX14JIOF_PULSE_TCK);

   WriteJtagPX14(hBrd, PX14JIO_TCK, PX14JIO_TCK);
   WriteJtagPX14(hBrd, 0, PX14JIO_CTRL);

   // Disable JTAG control
   JtagSessionCtrlPX14(hBrd, PX14_FALSE, NULL);

   return retVal;
}

/// Ensures that proposed firmware update file is compatible with target device
int VerifyFw2Compatibility (const CFwContextPX14& ctx, HPX14 hBrd)
{
   unsigned int ver32;
   unsigned short eeprom_data;
   unsigned long long ver64;
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // PX14 software version
   res = GetSoftwareReleaseVersionPX14(&ver64);
   if ((SIG_SUCCESS == res) && (ver64 < ctx.min_sw_ver))
   {
      std::string svMin, svHave;
      std::ostringstream oss;

      MakeStrVer64(ctx.min_sw_ver, svMin);
      MakeStrVer64(ver64, svHave);
      oss << "Firmware requires a minimum PX14400 software release "
         << "version of " << svMin.c_str() << ". ";
      oss << "Current software release version is " << svHave.c_str();
      SetErrorExtra(hBrd, oss.str().c_str());
      return SIG_PX14_INCOMPATIBLE_FIRMWARE;
   }

   // Board revision - PX14400, PX12500, etc
   if (statep->m_boardRev != ctx.brd_rev)
   {
      std::ostringstream oss;
      std::string sHave, sNeed;
      GetBoardRevisionStr(statep->m_boardRev, sHave);
      GetBoardRevisionStr(ctx.brd_rev, sNeed);
      oss << "Board revision mismatch (Requires: " << sNeed.c_str();
      oss << ", Have: " << sHave.c_str() << ")";
      SetErrorExtra(hBrd, oss.str().c_str());
      return SIG_PX14_INCOMPATIBLE_FIRMWARE;
   }

   // Custom hardware
   if ((ctx.cust_hw != 0) && (ctx.cust_hw != 0xFFFF) &&
       (statep->m_custHw != ctx.cust_hw))
   {
      std::ostringstream oss;
      oss << "Custom hardware required (Requires: " << ctx.cust_hw;
      oss << ", Have: " << statep->m_custHw << ")";
      SetErrorExtra(hBrd, oss.str().c_str());
      return SIG_PX14_INCOMPATIBLE_FIRMWARE;
   }

   if (!statep->IsVirtual())
   {
      // Read current hardware revision from EEPROM
      res = GetEepromDataPX14(hBrd, PX14EA_HW_REV, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      ver32 = static_cast<unsigned>(eeprom_data) << 16;

      // Minimum hardware revision
      if (ctx.min_hw_rev && (ver32 < ctx.min_hw_rev))
      {
         std::string svMin, svHave;
         std::ostringstream oss;

         MakeStrVer32(ctx.min_hw_rev, svMin);
         MakeStrVer32(ver32, svHave);
         oss << "Firmware requires a minimum hardware revision of "
            << svMin.c_str() << ". ";
         oss << "Current hardware revision is " << svHave.c_str();
         SetErrorExtra(hBrd, oss.str().c_str());
         return SIG_PX14_INCOMPATIBLE_FIRMWARE;
      }

      // Maximum hardware revision
      if (ctx.max_hw_rev && (ver32 > ctx.max_hw_rev))
      {
         std::string svMax, svHave;
         std::ostringstream oss;

         MakeStrVer32(ctx.max_hw_rev, svMax);
         MakeStrVer32(ver32, svHave);
         oss << "Firmware requires a maximum hardware revision of "
            << svMax.c_str() << ". ";
         oss << "Current hardware revision is " << svHave.c_str();
         SetErrorExtra(hBrd, oss.str().c_str());
         return SIG_PX14_INCOMPATIBLE_FIRMWARE;
      }
   }

   return SIG_SUCCESS;
}

int ConstructJtagInfo (HPX14 hBrd, JtagChainVect& jcv)
{
   int sys_fwc1, sys_fwc2, sys_fpga_ir_len;
   int sab_fwc1, sab_fwc2, sab_fpga_ir_len;
   int res, jtag_device_count;
   bool bShortChain;

   // PX14400 devices can have different JTAG chains; DR devices do not
   IdCodeList lstIdCodes;
   res = ReadChainSettings(hBrd, lstIdCodes);
   PX14_RETURN_ON_FAIL(res);

   res = IsJtagChainDsNew(hBrd);
   PX14_RETURN_ON_FAIL(res);
   if (res > 0)
      return ConstructJtagInfoDsNew(hBrd, lstIdCodes, jcv);

   jtag_device_count = static_cast<int>(lstIdCodes.size());
   if ((jtag_device_count != 3) && (jtag_device_count != 6))
   {
      SetErrorExtra(hBrd, "Number of JTAG devices does not match any known configurations");
      return SIG_PX14_UNKNOWN_JTAG_CHAIN;
   }
   bShortChain = (3 == lstIdCodes.size());

   res = DetectPciFwRequirements(hBrd, sys_fwc1, sys_fwc2,
                                 sys_fpga_ir_len);
   PX14_RETURN_ON_FAIL(res);

   if (!bShortChain)
   {
      res = DetectSabFwRequirements(hBrd, sab_fwc1, sab_fwc2,
                                    sab_fpga_ir_len);
      PX14_RETURN_ON_FAIL(res);
   }
   else
      sab_fwc1 = sab_fwc2 = -1;

   // Long chain: 0-5, Short chain: 0-2
   //  [0] System logic EEPROM #2 (xcf16p IDCODE:0x*5058093)
   //  [1] System logic EEPROM #1 (xcf32p IDCODE:0x*5059093)
   //  [2] System FPGA (xc5vlx50t IDCODE: 0x*2A96093)
   //  [3] SAB logic EEPROM #2 (xcf16p IDCODE: 0x*5058093)
   //  [4] SAB logic EEPROM #1 (xcf32p IDCODE: IDCODE:0x*5059093)
   //  [5] SAB FPGA (xc5vsx50t IDCODE: 0x*2E9A093)
   try { jcv.resize(bShortChain ? 3 : 6); }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }

   // Note that when we upload firmware, we're actually uploading firmware
   //  to an EEPROM which will be used to load the FPGA during system start
   //  up. We do not actually touch the FPGAs here. They are in the JTAG
   //  chain however so we need to be aware of them.

   // System logic EEPROM #2 (xcf16p) -- SECONDARY (BLANK)
   jcv[0].fw_chunk         = sys_fwc2;
   jcv[0].irLen            = 16;
   jcv[0].skipf            = PX14FWIF_PCIE_EEPROM_2_BLANK;

   // System logic EEPROM #1 (xcf32p) -- PRIMARY
   jcv[1].fw_chunk         = sys_fwc1;
   jcv[1].irLen            = 16;
   jcv[1].ea_ver_low       = PX14EA_LOGIC_SUB_VER;
   jcv[1].ea_ver_hi        = PX14EA_LOGIC_VER;
   jcv[1].ea_prev_ver_low  = PX14EA_PREV_LOGIC_SUB_VER;
   jcv[1].ea_prev_ver_high = PX14EA_PREV_LOGIC_VER;
   jcv[1].ea_cust_enum     = PX14EA_CUSTOM_LOGIC_ENUM;
   jcv[1].pre_rel_flag     = PX14PRERELEASE_SYS_FW;

   // System FPGA
   jcv[2].irLen            = sys_fpga_ir_len;

   if (!bShortChain)
   {
      // SAB logic EEPROM #2 (xcf16p) -- SECONDARY (BLANK)
      jcv[3].fw_chunk         = sab_fwc2;
      jcv[3].irLen            = 16;
      jcv[3].skipf            = PX14FWIF_SAB_EEPROM_2_BLANK;

      // SAB logic EEPROM #1 (xcf32p) -- PRIMARY
      jcv[4].fw_chunk         = sab_fwc1;
      jcv[4].irLen            = 16;
      jcv[4].ea_ver_low       = PX14EA_SAB_LOGIC_SUB_VER;
      jcv[4].ea_ver_hi        = PX14EA_SAB_LOGIC_VER;
      jcv[4].ea_prev_ver_low  = PX14EA_PREV_SAB_LOGIC_SUB_VER;
      jcv[4].ea_prev_ver_high = PX14EA_PREV_SAB_LOGIC_VER;
      jcv[4].ea_cust_enum     = PX14EA_CUSTOM_SAB_LOGIC_ENUM;
      jcv[4].pre_rel_flag     = PX14PRERELEASE_SAB_FW;

      // SAB FPGA
      jcv[5].irLen            = sab_fpga_ir_len;
   }

   return SIG_SUCCESS;
}

/**
  This function is used to setup some per-JTAG item shift information
  that allows us to use XSVF files generated for a single device chain.
  */
int SetupJtagShiftInfo (JtagChainVect& jcv)
{
   int il_cur, il_total, comp_cur, comp_total;

   JtagChainVect::iterator iComp(jcv.begin());
   for (il_total=comp_total=0; iComp!=jcv.end(); iComp++)
   {
      il_total += iComp->irLen;
      comp_total++;
   }

   il_cur = comp_cur = 0;
   for (iComp=jcv.begin(); iComp!=jcv.end(); iComp++,comp_cur++)
   {
      iComp->lHir = il_total - il_cur - iComp->irLen;
      iComp->lTir = il_cur;
      iComp->lHdr = comp_total - 1 - comp_cur;
      iComp->lTdr = comp_cur;
      iComp->lHdrFpga = (32 - comp_cur) % 32;

      il_cur += iComp->irLen;
   }

   return SIG_SUCCESS;
}

int PreLoadFwChunk (PX14S_JTAG_CHAIN_ITEM& jci,
                    CFwContextPX14& fwCtx,
                    CMyXsvfPlayer& xp,
                    const std::string& fwPath,
                    const std::string& tmpPath,
                    unsigned int flags)
{
   bool bFwFileIsBlank, bEepromBlankNow, bVerifyOp;
   unsigned int ver_now, cust_enum_now;
   unsigned short eeprom_data;
   HPX14 hBrd;
   int res;

   const CFwContextPX14::CFirmwareChunk& fwc =
      fwCtx.fw_chunk_map[jci.fw_chunk];

   bVerifyOp = 0 != (fwCtx.fw_flags & PX14FWCTXF_VERIFY_FILE);

   hBrd = xp.GetBoardHandle();

   // Optimization: We upload device firmware by writing firmware logic
   //  to configuration EEPROMs. Prior to writing the EEPROM, it must
   //  be erased and this can take some time; minutes in some cases.
   //  If we know a particular EEPROM is already blank, then there's no
   //  need to erase it again. (Set PX14UFWF_FORCE_EEPROM_ERASE to
   //  override this behavior)
   if (!bVerifyOp && jci.skipf && fwc.fwc_file_count)
   {
      // Framework is setup to handle the possibility of multiple
      //  firmware files per chunk; but (at least for the case we're
      //  checking) there will only ever be a single file
      std::string fname(*fwc.fwc_file_list.begin());

      // Is this a 'erase' firmware file?
      bFwFileIsBlank = (0 == fname.compare(0,
                                           strlen(PX14_BLANK_FW_PREFIX), PX14_BLANK_FW_PREFIX));
      jci.skipf_state = bFwFileIsBlank ? jci.skipf : 0;

      // Is this EEPROM already marked as blank?
      eeprom_data = 0;
      GetEepromDataPX14(hBrd, PX14EA_FWINFO, &eeprom_data);
      bEepromBlankNow = 0 != (eeprom_data & jci.skipf);

      if (bFwFileIsBlank && bEepromBlankNow &&
          (0 == (flags & PX14UFWF_FORCE_EEPROM_ERASE)))
      {
         // Skip this component, it's already blank
         return SIG_PX14_FIRMWARE_IS_UP_TO_DATE;
      }
   }

   // If refresh only flag is set, we skip redundant firmware loads
   if (!bVerifyOp && !IsDeviceVirtualPX14(hBrd) &&
       ((flags & PX14UFWF_REFRESH_ONLY) && (-1 != jci.ea_ver_low)))
   {
      res = GetEepromDataPX14(hBrd,
                              static_cast<unsigned short>(jci.ea_ver_hi), &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      ver_now = static_cast<unsigned int>(eeprom_data) << 16;
      res = GetEepromDataPX14(hBrd,
                              static_cast<unsigned short>(jci.ea_ver_low), &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      ver_now |= eeprom_data;

      if (-1 != jci.ea_cust_enum)
      {
         res = GetEepromDataPX14(hBrd,
                                 static_cast<unsigned short>(jci.ea_cust_enum),
                                 &eeprom_data);
         PX14_RETURN_ON_FAIL(res);
         cust_enum_now = eeprom_data;
      }
      else
         cust_enum_now = jci.ea_cust_enum;

      // Is firmware in file the same as what we've alreay got?
      if ((ver_now == fwc.fwc_version) &&
          (cust_enum_now == fwc.fwc_cust_enum))
      {
         return SIG_PX14_FIRMWARE_IS_UP_TO_DATE;
      }
   }

   return SIG_SUCCESS;
}

int DoFwChunk (PX14S_JTAG_CHAIN_ITEM& jci,
               CFwContextPX14& fwCtx,
               CMyXsvfPlayer& xp,
               const std::string& fwPath,
               const std::string& tmpPath,
               unsigned int flags)
{
   int res, cur_file;
   bool bFailure;

   // Lookup firmware chunk details in firmware file context data
   CFwContextPX14::FwChunkMap::const_iterator iFwc(
                                                   fwCtx.fw_chunk_map.find(jci.fw_chunk));
   if (iFwc == fwCtx.fw_chunk_map.end())
      return SIG_PX14_REQUIRED_FW_NOT_FOUND;
   const CFwContextPX14::CFirmwareChunk& fwc = iFwc->second;

   // We may be able to skip this chunk
   res = PreLoadFwChunk(jci, fwCtx, xp, fwPath, tmpPath, flags);
   PX14_RETURN_ON_FAIL(res);

   if (0 == (flags & PX14UFWF_SKIP_FW_UPLOAD))
   {
      // Keep track of extracted files so we can delete them when we're done
      std::list<std::string> tmpFileList;

      // Extract all firmware files
      CFwContextPX14::FileList::const_iterator iFile =
         fwc.fwc_file_list.begin();
      for (; iFile!=fwc.fwc_file_list.end(); iFile++)
      {
         std::string fwFilePath(tmpPath);
         fwFilePath.append(*iFile);
         res = UnzipFile(fwPath.c_str(), iFile->c_str(), fwFilePath.c_str());
         PX14_RETURN_ON_FAIL(res);
         tmpFileList.push_back(fwFilePath);

         if (jci.flags & JCIF_PATCH_XSVF_IDCODE_32P_TO_16P)
         {
            res = PatchXsvf_32p_to_16p(fwFilePath.c_str());
            PX14_RETURN_ON_FAIL(res);
         }
      }

      // For each file to upload...
      std::list<std::string>::iterator iFwFile(tmpFileList.begin());
      bFailure = false;
      for (cur_file=1; iFwFile!=tmpFileList.end(); iFwFile++,cur_file++)
      {
         // Do the actual firmware upload
         res = xp.xsvfExecute(iFwFile->c_str(),
                              jci.lHir, jci.lTir, jci.lHdr, jci.lTdr, jci.lHdrFpga);
         if (0 != res)
         {
            bFailure = true;
            break;
         }
      }

      // Remove temporary files
      for (iFwFile=tmpFileList.begin(); iFwFile!=tmpFileList.end(); iFwFile++)
         SysDeleteFile(iFwFile->c_str());

      if (bFailure)
         return SIG_PX14_FIRMWARE_UPLOAD_FAILED;

      // This is just a verify operation; we're done now
      if (fwCtx.fw_flags & PX14FWCTXF_VERIFY_FILE)
         return SIG_SUCCESS;
   }

   // Update EEPROM with new version information
   return DoPostFwChunkUpload(xp.GetBoardHandle(), jci, fwc);
}

/** @brief Called after a firmware chunk has been uploaded

  This function updates configuration EEPROM data (version numbers and
  such) for the given firmware chunk
  */
int DoPostFwChunkUpload (HPX14 hBrd,
                         PX14S_JTAG_CHAIN_ITEM& jci,
                         const CFwContextPX14::CFirmwareChunk& fwc)
{
   unsigned short eeprom_data, new_eeprom_data;
   bool bWroteEeprom;
   int res;

   bWroteEeprom = false;

   // Have we changed firmware version?
   if (jci.ver_have != fwc.fwc_version)
   {
      // Update 'previous firmware version' item in EEPROM
      if (-1 != jci.ea_prev_ver_low)
      {
         res = SetEepromDataPX14(hBrd,
                                 jci.ea_prev_ver_low, jci.ver_have);
         PX14_RETURN_ON_FAIL(res);
         res = SetEepromDataPX14(hBrd,
                                 jci.ea_prev_ver_high, jci.ver_have >> 16);
         PX14_RETURN_ON_FAIL(res);
         bWroteEeprom = true;
      }

      // Update new version number
      if (-1 != jci.ea_ver_low)
      {
         res = SetEepromDataPX14(hBrd,
                                 jci.ea_ver_low, fwc.fwc_version);
         PX14_RETURN_ON_FAIL(res);
         res = SetEepromDataPX14(hBrd,
                                 jci.ea_ver_hi, fwc.fwc_version >> 16);
         PX14_RETURN_ON_FAIL(res);
         bWroteEeprom = true;
      }
   }

   // Update custom firmware enumeration if necessary
   if (-1 != jci.ea_cust_enum)
   {
      res = GetEepromDataPX14(hBrd, jci.ea_cust_enum, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);

      if (eeprom_data != fwc.fwc_cust_enum)
      {
         res = SetEepromDataPX14(hBrd, jci.ea_cust_enum,
                                 fwc.fwc_cust_enum);
         PX14_RETURN_ON_FAIL(res);
         bWroteEeprom = true;
      }
   }

   // Update pre-release bit if necessary
   if (jci.pre_rel_flag)
   {
      res = GetEepromDataPX14(hBrd, PX14EA_PRE_RELEASE, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);

      new_eeprom_data = eeprom_data;
      if (fwc.fwc_flags & PX14FWCF_PRERELEASE)
         new_eeprom_data |=  jci.pre_rel_flag;
      else
         new_eeprom_data &= ~jci.pre_rel_flag;
      if (new_eeprom_data != eeprom_data)
      {
         res = SetEepromDataPX14(hBrd, PX14EA_PRE_RELEASE,
                                 new_eeprom_data);
         PX14_RETURN_ON_FAIL(res);
         bWroteEeprom = true;
      }
   }

   if (jci.skipf && (jci.skipf_state != 0xFFFF))
   {
      unsigned short new_fwinfo, fwinfo;

      GetEepromDataPX14(hBrd, PX14EA_FWINFO, &fwinfo);
      new_fwinfo = fwinfo;
      if (jci.skipf & jci.skipf_state)
         new_fwinfo |=  jci.skipf;
      else
         new_fwinfo &= ~jci.skipf;
      if (new_fwinfo != fwinfo)
      {
         res = SetEepromDataPX14(hBrd, PX14EA_FWINFO, new_fwinfo);
         PX14_RETURN_ON_FAIL(res);
         bWroteEeprom = true;
      }
   }

   // Update EEPROM checksum if we wrote anything
   if (bWroteEeprom)
   {
      res = ResetEepromChecksumPX14(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

/** @brief Called after all firmware chunks have been uploaded

  This function updates configuration EEPROM data (version numbers and
  such) for overall firmware upload
  */
int PostFwUpload (HPX14 hBrd, CFwContextPX14& fwCtx, unsigned int flags)
{
   int res;

   // Update 'firmware package version' item in EEPROM
   res = SetEepromDataPX14(hBrd, PX14EA_FWPKG_VER_LOW,
                           static_cast<unsigned short>(fwCtx.fw_ver_pkg));
   PX14_RETURN_ON_FAIL(res);
   res = SetEepromDataPX14(hBrd, PX14EA_FWPKG_VER_HIGH,
                           static_cast<unsigned short>(fwCtx.fw_ver_pkg >> 16));
   PX14_RETURN_ON_FAIL(res);

   // Update firmware package custom enumeration
   res = SetEepromDataPX14(hBrd, PX14EA_CUST_FWPKG_ENUM,
                           static_cast<unsigned short>(fwCtx.pkg_cust_enum));
   PX14_RETURN_ON_FAIL(res);

   // Refresh EEPROM checksum
   return ResetEepromChecksumPX14(hBrd);
}

int DetectPciFwRequirements (HPX14 hBrd,
                             int& fwc1,
                             int& fwc2,
                             int& fpgaIrLen)
{
   unsigned short eeprom_data;
   int res;

   // Consult configuration EEPROM to determine what kind of FPGA we have
   res = GetEepromDataPX14(hBrd, PX14EA_SYS_FPGA_TYPE, &eeprom_data);
   PX14_RETURN_ON_FAIL(res);
   if (IsDeviceVirtualPX14(hBrd))
      eeprom_data = PX14SYSFPGA_V5_LX50T;

   if (eeprom_data == PX14SYSFPGA_V5_LX50T)
   {
      // Virtex 5 LX50T
      fwc1 = PX14FWCHUNKID_SYS_V5LX50T_1;
      fwc2 = PX14FWCHUNKID_SYS_V5LX50T_2;
      fpgaIrLen = PX14IRLEN_XC5VLX50T;
      return SIG_SUCCESS;
   }

   return SIG_PX14_CANNOT_DETERMINE_FW_REQ;
}

int DetectSabFwRequirements (HPX14 hBrd,
                             int& fwc1,
                             int& fwc2,
                             int& fpgaIrLen)
{
   unsigned short eeprom_data;
   int res;

   // Consult configuration EEPROM to determine what kind of FPGA we have
   res = GetEepromDataPX14(hBrd, PX14EA_SAB_FPGA_TYPE, &eeprom_data);
   PX14_RETURN_ON_FAIL(res);
   if (IsDeviceVirtualPX14(hBrd))
      eeprom_data = PX14SABFPGA_V5_SX95T;

   if (eeprom_data == PX14SABFPGA_V5_SX50T)
   {
      // Virtex 5 SX50T
      fwc1 = PX14FWCHUNKID_SAB_V5SX50T_1;
      fwc2 = PX14FWCHUNKID_SAB_V5SX50T_2;
      fpgaIrLen = PX14IRLEN_XC5VSX50T;
      return SIG_SUCCESS;
   }

   if (eeprom_data == PX14SABFPGA_V5_SX95T)
   {
      // Virtex 5 SX95T
      fwc1 = PX14FWCHUNKID_SAB_V5SX95T_1;
      fwc2 = PX14FWCHUNKID_SAB_V5SX95T_2;
      fpgaIrLen = PX14IRLEN_XC5VSX95T;
      return SIG_SUCCESS;
   }

   return SIG_PX14_CANNOT_DETERMINE_FW_REQ;
}

/** @brief Determine if board uses alternate JTAG chain configuration found on newer cards

  @return
  Returns >0 if board has newer JTAG chain configuration.
  Returns  0 if board has original JTAG chain configuration.
  Returns <0 (SIG_*) on error
  */
int IsJtagChainDsNew (HPX14 hBrd)
{
   unsigned int brd_rev;
   int res, bNewCfg;
   UINT64 hw_rev;

   if (IsDeviceVirtualPX14(hBrd))
      return PP_VIRTUAL_DEVICE_USES_NEW_JTAG_CHAIN;

   res = GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
   PX14_RETURN_ON_FAIL(res);
   res = GetHardwareRevisionPX14(hBrd, &hw_rev);
   PX14_RETURN_ON_FAIL(res);

   PX14_CT_ASSERT(PX14BRDREV__COUNT == 4);
   switch (brd_rev)
   {
      default:
      case PX14BRDREV_PX14400:   bNewCfg = (hw_rev >= PX14_VER64(3,0,0,0)); break;
      case PX14BRDREV_PX12500:   bNewCfg = (hw_rev >= PX14_VER64(3,0,0,0)); break;
      case PX14BRDREV_PX14400D:  bNewCfg = (hw_rev >= PX14_VER64(2,1,0,0)); break;
      case PX14BRDREV_PX14400D2: bNewCfg = (hw_rev >= PX14_VER64(2,1,0,0)); break;
   }

   return bNewCfg;
}

int ConstructJtagInfoDsNew (HPX14 hBrd, const IdCodeList& lstIdCodes, JtagChainVect& jcv)
{
   int res, jtag_device_count;
   unsigned int brd_subrev;
   CStatePX14* statep;
   PX14_CT_ASSERT(PX14BRDREV__COUNT == 4);

   res = GetBoardRevisionPX14(hBrd, NULL, &brd_subrev);
   PX14_RETURN_ON_FAIL(res);

   statep = PX14_H2B(hBrd);

   jtag_device_count = static_cast<int>(lstIdCodes.size());
   try { jcv.resize(jtag_device_count); }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }

   // Make sure our SYS1 FPGA type is an LX50T
   if (statep->m_fpgaTypeSys != PX14SYSFPGA_V5_LX50T)
   {
      SetErrorExtra(hBrd, "Unknown SYS1 FPGA type in configuration data");
      return SIG_PX14_CANNOT_DETERMINE_FW_REQ;
   }

   // First two components are the same for all configurations:
   //  [0] System logic EEPROM (xcf16p IDCODE:0x*5058093)
   //  [1] System FPGA (xc5vlx50t IDCODE: 0x*2A96093)

   // [0] System logic EEPROM (xcf16p IDCODE:0x*5058093)
   jcv[0].fw_chunk         = PX14FWCHUNKID_SYS_V5LX50T_1;
   jcv[0].irLen            = 16;
   jcv[0].ea_ver_low       = PX14EA_LOGIC_SUB_VER;
   jcv[0].ea_ver_hi        = PX14EA_LOGIC_VER;
   jcv[0].ea_prev_ver_low  = PX14EA_PREV_LOGIC_SUB_VER;
   jcv[0].ea_prev_ver_high = PX14EA_PREV_LOGIC_VER;
   jcv[0].ea_cust_enum     = PX14EA_CUSTOM_LOGIC_ENUM;
   jcv[0].pre_rel_flag     = PX14PRERELEASE_SYS_FW;
   // For JTAG chain configurations on earlier revision hardware, the main
   //  system logic existed on a 32p EEPROM and the 16p EEPROM was unused.
   //  Rather than generating a duplicate XSVF file for the newer boards,
   //  we instead patch the 32p XSVF data so that it will run on a 16p
   jcv[0].flags           |= JCIF_PATCH_XSVF_IDCODE_32P_TO_16P;

   // [1] System FPGA (xc5vlx50t IDCODE: 0x*2A96093)
   jcv[1].irLen            = PX14IRLEN_XC5VLX50T;

   PX14_CT_ASSERT(PX14BRDREVSUB_1__COUNT == 2);
   if (brd_subrev == PX14BRDREVSUB_1_SP)
   {
      if (statep->m_fpgaTypeSab == PX14SABFPGA_V5_SX95T)
      {
         // JTAG chain length should be 5: two FPGAs and three EEPROMs
         if (jtag_device_count != 5)
         {
            SetErrorExtra(hBrd, "-SP95 card does not have 5 components in JTAG chain");
            return SIG_PX14_UNKNOWN_JTAG_CHAIN;
         }

         // -SP95 JTAG chain
         //  [2] SAB logic EEPROM #2 (xcf16p IDCODE: 0x*5058093)
         //  [3] SAB logic EEPROM #1 (xcf32p IDCODE: IDCODE:0x*5059093)
         //  [4] SAB FPGA (xc5vsx95t)

         // SAB logic EEPROM #2 (xcf16p)
         jcv[2].fw_chunk         = PX14FWCHUNKID_SAB_V5SX95T_2;
         jcv[2].irLen            = 16;
         jcv[2].skipf            = PX14FWIF_SAB_EEPROM_2_BLANK;

         // SAB logic EEPROM #1 (xcf32p) -- PRIMARY
         jcv[3].fw_chunk         = PX14FWCHUNKID_SAB_V5SX95T_1;
         jcv[3].irLen            = 16;
         jcv[3].ea_ver_low       = PX14EA_SAB_LOGIC_SUB_VER;
         jcv[3].ea_ver_hi        = PX14EA_SAB_LOGIC_VER;
         jcv[3].ea_prev_ver_low  = PX14EA_PREV_SAB_LOGIC_SUB_VER;
         jcv[3].ea_prev_ver_high = PX14EA_PREV_SAB_LOGIC_VER;
         jcv[3].ea_cust_enum     = PX14EA_CUSTOM_SAB_LOGIC_ENUM;
         jcv[3].pre_rel_flag     = PX14PRERELEASE_SAB_FW;

         // SAB FPGA
         jcv[4].irLen            = PX14IRLEN_XC5VSX95T;
      }
      else if (statep->m_fpgaTypeSab == PX14SABFPGA_V5_SX50T)
      {
         // JTAG chain length should be 4: two FPGAs and two EEPROMs
         if (jtag_device_count != 4)
         {
            SetErrorExtra(hBrd, "-SP50 card does not have 4 components in JTAG chain");
            return SIG_PX14_UNKNOWN_JTAG_CHAIN;
         }

         // -SP50 JTAG chain
         // [2] SAB logic EEPROM #1 (xcf32p IDCODE: IDCODE:0x*5059093)
         // [3] SAB FPGA (xc5vsx50t)

         // SAB logic EEPROM #1 (xcf32p) -- PRIMARY
         jcv[2].fw_chunk         = PX14FWCHUNKID_SAB_V5SX50T_1;
         jcv[2].irLen            = 16;
         jcv[2].ea_ver_low       = PX14EA_SAB_LOGIC_SUB_VER;
         jcv[2].ea_ver_hi        = PX14EA_SAB_LOGIC_VER;
         jcv[2].ea_prev_ver_low  = PX14EA_PREV_SAB_LOGIC_SUB_VER;
         jcv[2].ea_prev_ver_high = PX14EA_PREV_SAB_LOGIC_VER;
         jcv[2].ea_cust_enum     = PX14EA_CUSTOM_SAB_LOGIC_ENUM;
         jcv[2].pre_rel_flag     = PX14PRERELEASE_SAB_FW;

         // SAB FPGA
         jcv[3].irLen            = PX14IRLEN_XC5VSX50T;
      }
      else
      {
         SetErrorExtra(hBrd, "Unknown SYS2 FPGA type in configuration data");
         return SIG_PX14_CANNOT_DETERMINE_FW_REQ;
      }
   }
   else
   {
      // JTAG chain length should be 2: EEPROM and FPGA
      if (jtag_device_count != 2)
      {
         SetErrorExtra(hBrd, "-DR card does not have 2 components in JTAG chain");
         return SIG_PX14_UNKNOWN_JTAG_CHAIN;
      }
   }

   return SIG_SUCCESS;
}
