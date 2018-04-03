/** @file   px14_versions.cpp
  @brief   Routines for obtaining version information; fw, hw, driver, etc
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_top.h"

#define MY_PX14_LIB_VERSION		PX14_VER64(2,20,18,15)

typedef struct _PX14S_VER_64tag
{
   unsigned short verPkg;
   unsigned short verSubMin;
   unsigned short verMin;
   unsigned short verMaj;

} PX14S_VER_64;

typedef union _PX4U_VER_64tag
{
   PX14S_VER_64            bits;
   unsigned long long      value;

} PX14U_VER_64;


// Module-local function prototypes ------------------------------------- //

static bool _TranslateCustomEnum (unsigned int ver_type,
                                  unsigned int cust_enum, std::string& s);

// Obtains the version of the previous PX14 SAB firmware
//static int GetPrevSabFirmwareVer (HPX14 hBrd, unsigned long long* verp);


// PX14 library exports implementation --------------------------------- //

/** @brief Obtains the version of the PX14 library

  The PX14 library version is a 64-bit value broken up into four 16-bit
   fields:
   - Major version     (Mask = 0xFFFF000000000000ULL)
   - Minor version     (Mask = 0x0000FFFF00000000ULL)
   - Sub-minor version (Mask = 0x00000000FFFF0000ULL)
   - Package number    (Mask = 0x000000000000FFFFULL)

@param verp
A pointer to the variable that will receive the version

@return
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.

@see GetDriverVersionPX14
*/
PX14API GetLibVersionPX14 (unsigned long long* verp)
{
   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
      return SIG_PX14_INVALID_ARG_2;

   *verp = MY_PX14_LIB_VERSION;
   return SIG_SUCCESS;
}

/** @brief Obtains the version of the PX14 firmware

  The PX14 firmware version is a 64-bit value broken up into four 16-bit
   fields:
   - Major version     (Mask = 0xFFFF000000000000ULL)
   - Minor version     (Mask = 0x0000FFFF00000000ULL)
   - Sub-minor version (Mask = 0x00000000FFFF0000ULL)
   - Package number    (Mask = 0x000000000000FFFFULL)

@param hBrd
A handle to a PX14400 device obtained by calling ConnectToDevicePX14
@param verp
A pointer to the variable that will receive the version

@return
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.
*/
#if 0
PX14API GetFirmwareVersionPX14 (HPX14 hBrd, unsigned long long* verp)
{
   CStatePX14* statep;
   PX14U_VER_64 ver64;
   int res;

   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
      return SIG_PX14_INVALID_ARG_2;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   ver64.bits.verMaj    = static_cast<unsigned short>( statep->m_fw_ver_pkg >> 24);
   ver64.bits.verMin    = static_cast<unsigned short>((statep->m_fw_ver_pkg & 0x00FF0000) >> 16);
   ver64.bits.verSubMin = static_cast<unsigned short>((statep->m_fw_ver_pkg & 0x0000FF00) >> 8);
   ver64.bits.verPkg    = static_cast<unsigned short>( statep->m_fw_ver_pkg & 0x000000FF);
   *verp  = ver64.value;

   return SIG_SUCCESS;
}
#endif

PX14API GetFirmwareVersionPX14 (HPX14 hBrd, unsigned long long* verp)
{
	return GetSysFirmwareVersionPX14(hBrd, verp);
}

/** @brief Obtains the version of the PX14 SAB firmware

  If the PX14 does not implement SAB, this value is undefined. The
  GetBoardFeaturesPX14 function may be used to determine if SAB is
  available for a particular device

  The PX14 firmware version is a 64-bit value broken up into four 16-bit
   fields:
   - Major version     (Mask = 0xFFFF000000000000ULL)
   - Minor version     (Mask = 0x0000FFFF00000000ULL)
   - Sub-minor version (Mask = 0x00000000FFFF0000ULL)
   - Package number    (Mask = 0x000000000000FFFFULL)

@param hBrd
A handle to a PX14400 device obtained by calling ConnectToDevicePX14
@param verp
A pointer to the variable that will receive the version

@return
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.
*/
PX14API GetSabFirmwareVersionPX14 (HPX14 hBrd, unsigned long long* verp)
{
   CStatePX14* statep;
   PX14U_VER_64 ver64;
   int res;

   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
      return SIG_PX14_INVALID_ARG_2;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   /*ver64.bits.verMaj    = static_cast<unsigned short>( statep->m_fw_ver_sab >> 24);
     ver64.bits.verMin    = static_cast<unsigned short>((statep->m_fw_ver_sab & 0x00FF0000) >> 16);
     ver64.bits.verSubMin = static_cast<unsigned short>((statep->m_fw_ver_sab & 0x0000FF00) >> 8);
     ver64.bits.verPkg    = static_cast<unsigned short>( statep->m_fw_ver_sab & 0x000000FF);
     */

   SysSleep(1);
   res = WriteDeviceRegPX14(hBrd, 0xA, 0xFFFFFFFF, 7);
   PX14_RETURN_ON_FAIL(res);
   SysSleep(1);
   res = WriteDeviceRegPX14(hBrd, 0xA, 0xFFFFFFFF, 7);
   PX14_RETURN_ON_FAIL(res);

   unsigned int readVal;
   SysSleep(1);
   res = ReadDeviceRegPX14(hBrd, 0xA, &readVal,PX14REGREAD_HARDWARE);
   PX14_RETURN_ON_FAIL(res);
   SysSleep(1);
   readVal = statep->m_regDev.fields.regA.bits.preg_val;

   ver64.bits.verMaj    = static_cast<unsigned short>(readVal >> 6);
   ver64.bits.verMin    = static_cast<unsigned short>(readVal & 0x0000003F);
   ver64.bits.verSubMin = 0;
   ver64.bits.verPkg    = 0;

   *verp  = ver64.value;

   return SIG_SUCCESS;
}

/** @brief Obtains the version of the PX14 PCI firmware

  The PX14 firmware version is a 64-bit value broken up into four 16-bit
   fields:
   - Major version     (Mask = 0xFFFF000000000000ULL)
   - Minor version     (Mask = 0x0000FFFF00000000ULL)
   - Sub-minor version (Mask = 0x00000000FFFF0000ULL)
   - Package number    (Mask = 0x000000000000FFFFULL)

@param hBrd
A handle to a PX14400 device obtained by calling ConnectToDevicePX14
@param verp
A pointer to the variable that will receive the version

@return
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.
*/
PX14API GetSysFirmwareVersionPX14 (HPX14 hBrd, unsigned long long* verp)
{
   CStatePX14* statep;
   PX14U_VER_64 ver64;
   int res;

   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
      return SIG_PX14_INVALID_ARG_2;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   /*ver64.bits.verMaj    = static_cast<unsigned short>( statep->m_fw_ver_pci >> 24);
     ver64.bits.verMin    = static_cast<unsigned short>((statep->m_fw_ver_pci & 0x00FF0000) >> 16);
     ver64.bits.verSubMin = static_cast<unsigned short>((statep->m_fw_ver_pci & 0x0000FF00) >> 8);
     ver64.bits.verPkg    = static_cast<unsigned short>( statep->m_fw_ver_pci & 0x000000FF);
     */
   ver64.bits.verMaj    = static_cast<unsigned short>(statep->m_fw_ver_pci >> 6);
   ver64.bits.verMin    = static_cast<unsigned short>(statep->m_fw_ver_pci & 0x0000003F);
   ver64.bits.verSubMin = 0;
   ver64.bits.verPkg    = 0;

   *verp  = ver64.value;

   return SIG_SUCCESS;
}

/** @brief Obtains the version of the previous PX14 firmware

  The PX14 firmware version is a 64-bit value broken up into four 16-bit
   fields:
   - Major version     (Mask = 0xFFFF000000000000ULL)
   - Minor version     (Mask = 0x0000FFFF00000000ULL)
   - Sub-minor version (Mask = 0x00000000FFFF0000ULL)
   - Package number    (Mask = 0x000000000000FFFFULL)

@param hBrd
A handle to a PX14400 device obtained by calling ConnectToDevicePX14
@param verp
A pointer to the variable that will receive the version

@return
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.
*/
#if 0
PX14API GetPrevPciFirmwareVerPX14 (HPX14 hBrd, unsigned long long* verp)
{
   unsigned short ver, sub;
   int res;

   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
   return SIG_PX14_INVALID_ARG_2;

   res = GetEepromDataPX14(hBrd, PX14EA_PREV_LOGIC_VER, &ver);
   PX14_RETURN_ON_FAIL(res);
   res = GetEepromDataPX14(hBrd, PX14EA_PREV_LOGIC_SUB_VER, &sub);
   PX14_RETURN_ON_FAIL(res);

   *verp  = sub & 0xFF;
   *verp |= static_cast<unsigned long long>(sub >> 8)   << 16;
   *verp |= static_cast<unsigned long long>(ver & 0xFF) << 32;
   *verp |= static_cast<unsigned long long>(ver >> 8)   << 48;

   return SIG_SUCCESS;
}
#endif

/** @brief Obtains the revision of the PX14 hardware

  The PX14 hardware revision is a 64-bit value broken up into four 16-bit
   fields:
   - Major version     (Mask = 0xFFFF000000000000ULL)
   - Minor version     (Mask = 0x0000FFFF00000000ULL)
   - Sub-minor version (Mask = 0x00000000FFFF0000ULL)
   - Package number    (Mask = 0x000000000000FFFFULL)

@param hBrd
A handle to a PX14400 device obtained by calling ConnectToDevicePX14
@param verp
A pointer to the variable that will receive the version

@return
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.
*/
PX14API GetHardwareRevisionPX14 (HPX14 hBrd, unsigned long long* verp)
{
   unsigned short rev;
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
      return SIG_PX14_INVALID_ARG_2;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->m_hw_revision)
      *verp = statep->m_hw_revision;
   else
   {
      // Just go to EEPROM the first time and cache for future
      res = GetEepromDataPX14(hBrd, PX14EA_HW_REV, &rev);
      PX14_RETURN_ON_FAIL(res);

      *verp  = static_cast<unsigned long long>(rev & 0xFF) << 32;
      *verp |= static_cast<unsigned long long>(rev >> 8)   << 48;
      statep->m_hw_revision = *verp;
   }

   return SIG_SUCCESS;
}

/** @brief Obtains the version of the PX14 software release

  The PX14 software release version is a 64-bit value broken up into four
  16-bit fields:
  - Major version     (Mask = 0xFFFF000000000000ULL)
  - Minor version     (Mask = 0x0000FFFF00000000ULL)
  - Sub-minor version (Mask = 0x00000000FFFF0000ULL)
  - Package number    (Mask = 0x000000000000FFFFULL)

  @param verp
  A pointer to the variable that will receive the version

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.
  */
PX14API GetSoftwareReleaseVersionPX14 (unsigned long long* verp)
{
   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
      return SIG_PX14_INVALID_ARG_2;

   return SysGetPX14SoftwareRelease(verp);
}

// Ensures that desired firmware matches desired constraint
#if 0
PX14API CheckFirmwareVerPX14 (HPX14 hBrd,
                              unsigned int fw_ver,
                              int fw_type,
                              int constraint)
{
   CStatePX14* statep;
   unsigned ver_have;
   int res;

   SIGASSERT(fw_type < PX14FWTYPE__COUNT);
   SIGASSERT(constraint < PX14VCT__COUNT);
   if ((fw_type < 0) || (fw_type >= PX14FWTYPE__COUNT))
      return SIG_INVALIDARG;
   if ((constraint < 0) || (constraint >= PX14VCT__COUNT))
      return SIG_INVALIDARG;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsVirtual())
      return SIG_SUCCESS;

   if (_PX14_LIKELY(PX14FWTYPE_PCI == fw_type))
      ver_have = statep->m_fw_ver_pci;
   else if (PX14FWTYPE_SAB == fw_type)
      ver_have = statep->m_fw_ver_sab;
   else
      ver_have = statep->m_fw_ver_pkg;

   if (_PX14_LIKELY(constraint == PX14VCT_GTOE))
      res = ver_have >= fw_ver;
   else
   {
      switch(constraint)
      {
         case PX14VCT_LTOE: res = ver_have <= fw_ver; break;
         case PX14VCT_GT:   res = ver_have >  fw_ver; break;
         case PX14VCT_LT:   res = ver_have <  fw_ver; break;
         case PX14VCT_E:    res = ver_have == fw_ver; break;
         case PX14VCT_NE:   res = ver_have != fw_ver; break;
         default:
                            SIGASSERT(0);
                            return SIG_PX14_UNEXPECTED;
      }
   }

   return res ? SIG_SUCCESS : SIG_PX14_NOT_IMPLEMENTED_IN_FIRMWARE;
}
#endif

/** @brief Obtains an ASCII string describing the version of an item
  @todo Document me
  */
PX14API GetVersionTextAPX14 (HPX14 hBrd,
                             unsigned int ver_type,
                             char** bufpp,
                             unsigned int flags)
{
   bool bPrerelease, bShowSubMin, bShowPkg, bZeroPad;
   unsigned int maj, min, submin, package;
   unsigned long long ver;
   unsigned short wVal;
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(bufpp, char*);

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // Get the version that we're generating the text fors
   switch (ver_type) {
      /*case PX14VERID_FIRMWARE:
        res = GetFirmwareVersionPX14(hBrd, &ver);
        break;*/
      case PX14VERID_HARDWARE:
         res = GetHardwareRevisionPX14(hBrd, &ver);
         break;
      case PX14VERID_DRIVER:
         res = GetDriverVersionPX14(hBrd, &ver);
         break;
      case PX14VERID_LIBRARY:
         res = GetLibVersionPX14(&ver);
         break;
      case PX14VERID_PX14_SOFTWARE:
         res = GetSoftwareReleaseVersionPX14(&ver);
         break;
      case PX14VERID_PCI_FIRMWARE:
         res = GetSysFirmwareVersionPX14(hBrd, &ver);
         break;
      /*case PX14VERID_PREV_PCI_FIRMWARE:
        res = GetPrevPciFirmwareVerPX14(hBrd, &ver);
        break;*/
      case PX14VERID_SAB_FIRMWARE:
         res = GetSabFirmwareVersionPX14(hBrd, &ver);
         break;
      /*case PX14VERID_PREV_SAB_FIRMWARE:
        res = GetPrevSabFirmwareVer(hBrd, &ver);
        break;*/
      default:
         res = SIG_PX14_INVALID_ARG_2;
         break;
   }
   PX14_RETURN_ON_FAIL(res);

   std::ostringstream oss;

   // Grab component parts of version
   maj     = static_cast<unsigned int>(ver >> 48);
   min     = static_cast<unsigned int>(ver >> 32 & 0xFF);
   submin  = static_cast<unsigned int>(ver >> 16 & 0xFF);
   package = static_cast<unsigned int>(ver & 0xFF);

   bZeroPad = 0 != (flags & PX14VERF_ZERO_PAD_SINGLE_DIGIT_VER);

   // Determine pre-release state
   bPrerelease = false;
   if (0 == (flags & PX14VERF_NO_PREREL)) {
      switch (ver_type) {
         case PX14VERID_PCI_FIRMWARE:
            bPrerelease = 0 != (statep->m_fw_pre_rel & PX14PRERELEASE_SYS_FW);
            break;
         case PX14VERID_SAB_FIRMWARE:
            bPrerelease = 0 != (statep->m_fw_pre_rel & PX14PRERELEASE_SAB_FW);
            break;
         case PX14VERID_HARDWARE:
            bPrerelease = 0 != (statep->m_fw_pre_rel & PX14PRERELEASE_HW);
            break;
         default:
            bPrerelease = false;
      }
   }
   if (bPrerelease)
      oss << "Pre-release ";

   bool bHaveText = false;
   if (flags & PX14VERF_ALLOW_ALIASES) {
      // Mixed firmware - When someone uploads firmware for a single
      // component (such as with custom firmware development) we use a
      // package version of 99.0.0.0 to identify that the card has 'mixed'
      // firmware. It's mixed in that the internal firmware component
      // versions may not correlate to a particular known firmware
      // package. In other words, we know that firmware package version A
      // has internal firmware versions C and D. If someone comes along
      // and uploads internal firmware version E, overriding C or D, we no
      // longer have package version A anymore, we have mixed firmware.
      if ((ver_type == PX14VERID_FIRMWARE) && (ver == PX14_VER64(99,0,0,0))) {
         oss << "Mixed firmware ";
         bHaveText = true;
      }

      if (IsDeviceVirtualPX14(hBrd)) {
         if ((ver_type == PX14VERID_FIRMWARE) ||
             (ver_type == PX14VERID_HARDWARE) ||
             (ver_type == PX14VERID_DRIVER) ||
             (ver_type == PX14VERID_PCI_FIRMWARE) ||
             (ver_type == PX14VERID_PREV_PCI_FIRMWARE) ||
             (ver_type == PX14VERID_SAB_FIRMWARE) ||
             (ver_type == PX14VERID_PREV_SAB_FIRMWARE)) {
            oss << "<Virtual device> ";
            bHaveText = true;
         }
      }
   }

   if (!bHaveText) {
      // Always display major and minor versions
      oss << maj << ".";
      if (bZeroPad && min < 10) oss << "0";
      oss << min;

      // Determine if we're displaying sub-minor info
      bShowSubMin = (0 == (flags & PX14VERF_NO_SUBMIN)) &&
         (submin || (0 == (flags & PX14VERF_COMPACT_VER)));
      if (bShowSubMin) {
         oss << ".";
         if (bZeroPad && submin < 10) oss << "0";
         oss << submin;
      }

      // Determine if we're displaying package info
      bShowPkg = bShowSubMin && (0 == (flags & PX14VERF_NO_PACKAGE)) &&
         (package || (0== (flags & PX14VERF_COMPACT_VER)));
      if (bShowPkg) {
         oss << ".";
         if (bZeroPad && package < 10) oss << "0";
         oss << package;
      }
   }

   // Determine if we're displaying custom enumeration
   if (0 == (flags & PX14VERF_NO_CUSTOM)) {
      wVal = 0;

      switch (ver_type) {
         case PX14VERID_PCI_FIRMWARE:
            wVal = statep->m_custFwPci;
            break;
         case PX14VERID_SAB_FIRMWARE:
            {
               //wVal = statep->m_custFwSab;
               SysSleep(1);
               res = WriteDeviceRegPX14(hBrd, 0xA, 0xFFFFFFFF, 8);
               PX14_RETURN_ON_FAIL(res);
               SysSleep(1);
               res = WriteDeviceRegPX14(hBrd, 0xA, 0xFFFFFFFF, 8);
               PX14_RETURN_ON_FAIL(res);
               SysSleep(1);
               unsigned int readVal = 0;
               res = ReadDeviceRegPX14(hBrd, 0xA, &readVal,PX14REGREAD_HARDWARE);
               PX14_RETURN_ON_FAIL(res);
               SysSleep(1);
               wVal = statep->m_regDev.fields.regA.bits.preg_val;
            }
            break;
         case PX14VERID_HARDWARE:
            wVal = statep->m_custHw;
            break;
         default:
            break;
      }
      if (wVal > 0) {
         oss << " (Custom: ";

         if (PX14CUSTHW_ONESHOT == wVal)
            oss << " Non-specific";
         else {
            // Map known custom enumerations to nicer, user-friendly strings
            std::string sCust;
            if (_TranslateCustomEnum(ver_type, wVal, sCust) && !sCust.empty())
               oss << sCust.c_str();
            else
               oss << wVal;
         }
         oss << ")";
      }
   }

   std::string str(oss.str());
   return AllocAndCopyString(str.c_str(), bufpp);
}

// Module private function implementation ------------------------------- //

/** @brief
  Translate the given custom enumeration to a user-friendly
  description

  The idea here is that at some point during a product's lifetime, we
  usually need to spin off some kind of customization. This is usually
  custom firmware and/or hardware. This function will attempt to map
  the given custom info to a user-friendly string. The implementation
  for this is not yet defined, but I'm thinking we can have an XML
  file in the root product installation folder that defines these
  values.
  */
bool _TranslateCustomEnum (unsigned int ver_type,
                           unsigned int cust_enum,
                           std::string& s)
{
   //if ((cust_enum == 1) &&
   //   ((ver_type == PX14VERID_PCI_FIRMWARE) || (ver_type == PX14VERID_HARDWARE)))
   //{
   //   s.assign("PCI");
   //   return true;
   //}

   if ((ver_type == PX14VERID_FIRMWARE) ||
       (ver_type == PX14VERID_SAB_FIRMWARE))
   {
      switch (cust_enum)
      {
         case PX14FLP_C2_DECIMATION:
            s.assign("Decimation");
            return true;

         case PX14FLP_C4_FFT:
            s.assign("FFT");
            return true;

         case PX14FLP_C5_FIRFILT_SINGLE_CHAN:
            s.assign("FIR Filter");
            return true;

         case PX14FLP_C6_FIRFILT_DUAL_CHAN:
            s.assign("FIR Filter (Dual chan)");
            return true;

         default:break;
      }
   }

   return false;
}

/// Obtains the version of the previous PX14 SAB firmware
#if 0
int GetPrevSabFirmwareVer (HPX14 hBrd, unsigned long long* verp)
{
   unsigned short ver, sub;
   int res;

   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
      return SIG_PX14_INVALID_ARG_2;

   res = GetEepromDataPX14(hBrd, PX14EA_PREV_SAB_LOGIC_VER, &ver);
   PX14_RETURN_ON_FAIL(res);
   res = GetEepromDataPX14(hBrd, PX14EA_PREV_SAB_LOGIC_SUB_VER, &sub);
   PX14_RETURN_ON_FAIL(res);

   *verp  = sub & 0xFF;
   *verp |= static_cast<unsigned long long>(sub >> 8)   << 16;
   *verp |= static_cast<unsigned long long>(ver & 0xFF) << 32;
   *verp |= static_cast<unsigned long long>(ver >> 8)   << 48;

   return SIG_SUCCESS;
}
#endif
