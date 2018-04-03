/** @file	px14_srd_file.cpp
  @brief	Implementation of PX14 library support for .srdc files
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"
#include "px14_srd_file.h"
#include "sigsvc_utility.h"

// CSrdPX14 implementation ----------------------------------------------- //

CSrdPX14::CSrdPX14(HPX14 hBrd) : m_magic(s_magic), m_hBrd(hBrd)
{
}

CSrdPX14::~CSrdPX14()
{
}

int CSrdPX14::Validate (HPX14SRDC hFile, CSrdPX14** thispp)
{
   CSrdPX14* thisp;

   SIGASSERT_NULL_OR_POINTER(thispp, CSrdPX14*);

   thisp = reinterpret_cast<CSrdPX14*>(hFile);
   if ((NULL == thisp) || thisp->m_magic != s_magic)
      return SIG_PX14_INVALID_HANDLE;

   if (thispp)
      *thispp = thisp;
   return SIG_SUCCESS;
}

int CSrdPX14::SrdError (const char* preamblep)
{
   SIGASSERT_NULL_OR_POINTER(preamblep, char);
   if (NULL == preamblep)
      preamblep = "SRDC file error: ";

   std::string s(preamblep);
   if (!m_errText.empty())
      s.append(m_errText);

   SetErrorExtra(m_hBrd, s.c_str());

   return SIG_PX14_SRDC_FILE_ERROR;
}

bool CSrdPX14::GetPredefinedParameters (AcqDeviceParams& adp)
{
   int active_channels;
   unsigned sn, brd_rev;

   if (IsHandleValidPX14(m_hBrd) <= 0)
      return _Failed("No PX14 associated with SRDC handle");

   sn = 0;
   GetSerialNumberPX14(m_hBrd, &sn);
   brd_rev = PX14BRDREV_PX14400;
   GetBoardRevisionPX14(m_hBrd, &brd_rev, NULL);
   GetBoardRevisionStr(brd_rev, adp.boardName);
   adp.serialNum = sn;

   active_channels = GetActiveChannelsPX14(m_hBrd);

   adp.channelCount = (active_channels == PX14CHANNEL_DUAL) ? 2 : 1;
   adp.sampSizeBytes = PX14_SAMPLE_SIZE_IN_BYTES;
   adp.sampSizeBits = PX14_SAMPLE_SIZE_IN_BITS;
   adp.bSignedSamples = false;
   adp.channelNum = active_channels;	// Naturally okay 0,1,2 = mult/1/2

   switch (active_channels)
   {
      case PX14CHANNEL_DUAL: adp.channelMask = 0x3; break;
      case PX14CHANNEL_TWO:  adp.channelMask = 0x2; break;
      default:
      case PX14CHANNEL_ONE:  adp.channelMask = 0x1; break;
   }
   PX14_CT_ASSERT(PX14CHANNEL__COUNT == 3);

   adp.sampleRateMHz = 0;
   GetEffectiveAcqRatePX14(m_hBrd, &adp.sampleRateMHz);

   adp.inputVoltRngPP = 0;
   GetInputVoltageRangeVoltsCh1PX14(m_hBrd, &adp.inputVoltRngPP);

   adp.segment_size = (GetTriggerModePX14(m_hBrd) == PX14TRIGMODE_SEGMENTED)
      ? GetSegmentSizePX14(m_hBrd) : 0;
   adp.pretrig_samples = GetPreTriggerSamplesPX14(m_hBrd);
   adp.delaytrig_samples = (0 == adp.pretrig_samples)
      ? GetTriggerDelaySamplesPX14(m_hBrd) : 0;

   return true;
}

// PX14 library exports implementation --------------------------------- //

PX14API OpenSrdcFileWPX14 (HPX14 hBrd,
                           HPX14SRDC* handlep,
                           const wchar_t* pathnamep,
                           unsigned flags)
{
   return OpenSrdcFileAPX14(hBrd, handlep, CAutoCharBuf(pathnamep), flags);
}

PX14API SaveSrdcFileWPX14 (HPX14SRDC hFile, const wchar_t* pathnamep)
{
   return SaveSrdcFileAPX14(hFile, CAutoCharBuf(pathnamep));
}

/// Lookup SRDC item with given name (UNICODE)
PX14API GetSrdcItemWPX14 (HPX14SRDC hFile,
                          const wchar_t* namep,
                          wchar_t** valuepp)
{
   SIGASSERT_NULL_OR_POINTER(valuepp, wchar_t*);
   if (valuepp)
   {
      char* abufp;
      int res;

      res = GetSrdcItemAPX14(hFile, CAutoCharBuf(namep), &abufp);
      PX14_RETURN_ON_FAIL(res);

      res = ConvertAsciiToWideCharAlloc(abufp, valuepp);
      my_free(abufp);
      return res;
   }

   return GetSrdcItemAPX14(hFile, CAutoCharBuf(namep), NULL);
}

// Add/modify SRDC item with given name; name is case-sensitive (UNICODE)
PX14API SetSrdcItemWPX14 (HPX14SRDC hFile,
                          const wchar_t* namep,
                          const wchar_t* valuep)
{
   return SetSrdcItemAPX14(hFile, CAutoCharBuf(namep),
                           CAutoCharBuf(valuep));
}

/// Obtain information on data recorded to given file (UNICODE)
PX14API GetRecordedDataInfoWPX14 (const wchar_t* pathnamep,
                                  PX14S_RECORDED_DATA_INFO* infop,
                                  wchar_t** operator_notespp)
{
   int res;

   SIGASSERT_NULL_OR_POINTER(operator_notespp, wchar_t);

   if (NULL != operator_notespp)
   {
      char* notesp;

      res = GetRecordedDataInfoAPX14(CAutoCharBuf(pathnamep),
                                     infop, &notesp);
      if (SIG_SUCCESS == res)
      {
         if (notesp)
         {
            res = ConvertAsciiToWideCharAlloc(notesp, operator_notespp);
            my_free(notesp);
         }
         else
         {
            // No notes available
            *operator_notespp = NULL;
         }
      }
   }
   else
   {
      res = GetRecordedDataInfoAPX14(CAutoCharBuf(pathnamep),
                                     infop, NULL);
   }

   return res;
}

// Obtain enumeration of all SRDC items with given constraints (UNICODE)
PX14API EnumSrdcItemsWPX14 (HPX14SRDC hFile,
                            wchar_t** itemspp,
                            unsigned int flags)
{
   char* itemsap;
   int res;

   SIGASSERT_POINTER(itemspp, wchar_t);
   if (NULL == itemspp)
      return SIG_PX14_INVALID_ARG_2;

   res = EnumSrdcItemsAPX14(hFile, &itemsap, flags);
   PX14_RETURN_ON_FAIL(res);

   if (itemsap)
   {
      res = ConvertAsciiToWideCharAlloc(itemsap, itemspp);
      FreeMemoryPX14(itemsap);
      PX14_RETURN_ON_FAIL(res);
   }
   else
      *itemspp = NULL;

   return SIG_SUCCESS;
}

/** @brief Open a new or existing .srdc file


  @param hBrd
  A handle to the PX14 board. This handle is obtained by calling the
  ConnectToDevicePX14 function
  @param handlep
  A pointer to a HPX14SRDC variable that will receive a handle that
  identifies the SRDC file
  @param pathnamep
  A pointer to a NULL-terminated string that contains the pathname of
  the SRDC file.


*/
PX14API OpenSrdcFileAPX14 (HPX14 hBrd,
                           HPX14SRDC* handlep,
                           const char* pathnamep,
                           unsigned flags)
{
   int res;

   SIGASSERT_NULL_OR_POINTER(pathnamep, char);
   SIGASSERT_NULL_OR_POINTER(handlep, HPX14SRDC);
   if ((0 == (flags & PX14SRDCOF_QUICK_SET)) && (NULL == handlep))
      return SIG_PX14_INVALID_ARG_1;

   // It's okay to not specify a board handle, but if user tries to do
   //  anything that references PX14 settings (like calling
   //  RefreshSrdcParametersPX14) they'll get an error
   if (PX14_INVALID_HANDLE != hBrd)
   {
      res = ValidateHandle(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }

   if (flags & PX14SRDCOF_PATHNAME_IS_REC_DATA)
      flags |= PX14SRDCOF_OPEN_EXISTING;

   // "create new" and "open existing" are mutually exclusive
   if ((flags & PX14SRDCOF_OPEN_EXISTING) &&
       (flags & PX14SDRCOF_CREATE_NEW))
   {
      return SIG_PX14_INVALID_ARG_4;
   }

   if (pathnamep && (flags & PX14SRDCOF_PATHNAME_IS_REC_DATA))
   {
      // Caller is giving us the data file (.rd16) and we should try to
      //  find the corresponding SDRC data.

      std::string pathname(pathnamep), file_try;

      // First try auxiliary file: pathname.srdc
      file_try.assign(pathname);
      file_try.append(PX14_SRDC_DOT_EXTENSIONA);
      res = OpenSrdcFileAPX14(hBrd, handlep, file_try.c_str(),
                              flags & ~PX14SRDCOF_PATHNAME_IS_REC_DATA);
      if (SIG_SUCCESS == res)
         return res;

#ifdef _WIN32
      // Next try embedded AFS - Windows NTFS only: pathname:SRDC
      file_try.assign(pathname);
      file_try.append(":SRDC");
      res = OpenSrdcFileAPX14(hBrd, handlep, file_try.c_str(),
                              flags & ~PX14SRDCOF_PATHNAME_IS_REC_DATA);
      if (SIG_SUCCESS == res)
         return res;
#endif

      if (PX14_INVALID_HANDLE != hBrd)
      {
         pathname.insert(0, "Failed to load or find existing SRDC file: ");
         SetErrorExtra(hBrd, pathname.c_str());
      }
      return SIG_PX14_CANNOT_FIND_SRDC_DATA;
   }

   CSrdPX14* file_ctxp;
   try { file_ctxp = new CSrdPX14(hBrd); }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }
   // Auto-free in the event that something below fails
   std::auto_ptr<CSrdPX14> afCtx(file_ctxp);

   if (0 == (flags & PX14SDRCOF_CREATE_NEW))
   {
      // If a pathname is specified then we're opening an existing file;
      //  else we're creating a new file
      if (pathnamep && (!file_ctxp->LoadFrom(pathnamep)))
      {
         if (flags & PX14SRDCOF_OPEN_EXISTING)
            return file_ctxp->SrdError("Failed to load existing SRDC file: ");
      }
   }

   // Is this just a quickset operation?
   if (0 != (flags & PX14SRDCOF_QUICK_SET))
   {
      HPX14SRDC hUse = reinterpret_cast<HPX14SRDC>(afCtx.get());

      // Refresh parameters
      res = RefreshSrdcParametersPX14(hUse);
      PX14_RETURN_ON_FAIL(res);

      // Save data
      res = SaveSrdcFileAPX14(hUse, pathnamep);
      PX14_RETURN_ON_FAIL(res);

      return SIG_SUCCESS;
   }

   if (pathnamep)
      afCtx->m_pathname.assign(pathnamep);

   *handlep = reinterpret_cast<HPX14SRDC>(afCtx.release());
   return SIG_SUCCESS;
}

PX14API SaveSrdcFileAPX14 (HPX14SRDC hFile, const char* pathnamep)
{
   CSrdPX14* ctxp;
   int res;

   SIGASSERT_NULL_OR_POINTER(pathnamep, char);

   res = CSrdPX14::Validate(hFile, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   if (!ctxp->SaveTo(pathnamep))
      return ctxp->SrdError("Failed to save SRDC file: ");

   return SIG_SUCCESS;
}

/// Close given SRDC file without updating contents
PX14API CloseSrdcFilePX14 (HPX14SRDC hFile)
{
   CSrdPX14* ctxp;
   int res;

   res = CSrdPX14::Validate(hFile, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   delete ctxp;
   return SIG_SUCCESS;
}

/// Refresh SRDC with current board settings
PX14API RefreshSrdcParametersPX14 (HPX14SRDC hFile, unsigned flags)
{
   CSrdPX14* ctxp;
   int res;

   // flags currently unused

   res = CSrdPX14::Validate(hFile, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   // Need a device attached
   if (ctxp->m_hBrd == PX14_INVALID_HANDLE)
      return SIG_PX14_INVALID_HANDLE;

   if (!ctxp->RefreshKnownParameters())
      return ctxp->SrdError("Failed to update SRDC parameters: ");

   return SIG_SUCCESS;
}

/// Lookup SRDC item with given name; name is case-sensitive
PX14API GetSrdcItemAPX14 (HPX14SRDC hFile,
                          const char* namep,
                          char** valuepp)
{
   CSrdPX14* ctxp;
   int res;

   SIGASSERT_NULL_OR_POINTER(valuepp, char*);
   SIGASSERT_POINTER(namep, char);
   if (NULL == namep)
      return SIG_PX14_INVALID_ARG_2;

   res = CSrdPX14::Validate(hFile, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   std::string itemValue;
   if (ctxp->GetItemValue(namep, itemValue))
      return AllocAndCopyString(itemValue.c_str(), valuepp);

   SetErrorExtra(ctxp->m_hBrd, namep);
   return SIG_PX14_NAMED_ITEM_NOT_FOUND;
}

/** @brief Add/modify SRDC item with given name

  This function will modify the in-memory item with the given name.
  Changes are not written to file until the SaveSrdFilePX14 function
  is called.

  @param valuep
  If this parameter is NULL, the named item will be removed from
  the SRDC file
  */
PX14API SetSrdcItemAPX14 (HPX14SRDC hFile,
                          const char* namep,
                          const char* valuep)
{
   CSrdPX14* ctxp;
   int res;

   SIGASSERT_NULL_OR_POINTER(valuep, char);
   SIGASSERT_POINTER(namep, char);
   if (NULL == namep)
      return SIG_PX14_INVALID_ARG_2;

   res = CSrdPX14::Validate(hFile, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   if (!ctxp->RefreshItem(namep, valuep))
      return ctxp->SrdError("Failed to update SRDC item: ");

   return SIG_SUCCESS;
}


/** @brief Obtain information on data recorded to given file

  This function works by interrograting Signatec Recorded Data (SRDC) that
  can optionally be generated when recording acquisition data or saving
  PX14 RAM data to disk.

  Currently this SRDC data can reside in one of two places: either as a
  named stream in the actual acquisition data, or in an secondary data
  file assumed to be the full pathname of the acquisition data file
  appended with the SRDC file extension (.srdc). (NOTE: Only the Windows
  PX14 library supports embedding data in named streams.)

  In both cases this SRDC data is a small XML file that contains
  information on the acquired data. This includes things like channel
  count, sample size and format, source board information, and segment
  size. The file may also contain user-defined data.

  The function first checks to see if SRDC data is available as an
  external file. If the external file is not present, the function checks
  for a named stream (the stream name is assumed to be SRDC). If embedded
  information is not present, the given file itself is checked. (This
  allows the caller to obtain information directly from a .srdc file.)
  */
PX14API GetRecordedDataInfoAPX14 (const char* pathnamep,
                                  PX14S_RECORDED_DATA_INFO* infop,
                                  char** operator_notespp)
{
   HPX14SRDC hSrd;
   int res;

   SIGASSERT_NULL_OR_POINTER(operator_notespp, char);

   PX14_ENSURE_POINTER(PX14_INVALID_HANDLE, infop, PX14S_RECORDED_DATA_INFO, NULL);
   PX14_ENSURE_POINTER(PX14_INVALID_HANDLE, pathnamep, char, NULL);
   PX14_ENSURE_STRUCT_SIZE(PX14_INVALID_HANDLE, infop, _PX14SO_RECORDED_DATA_INFO_V1, NULL);

   std::string file_try, acqdata_pathname(pathnamep);
   TrimString(acqdata_pathname);

   res = OpenSrdcFileAPX14(PX14_INVALID_HANDLE, &hSrd,
                           acqdata_pathname.c_str(),
                           PX14SRDCOF_OPEN_EXISTING | PX14SRDCOF_PATHNAME_IS_REC_DATA);
   if (SIG_SUCCESS != res)
      return res;
   SIGASSERT(PX14_INVALID_HPX14SRDC != hSrd);
   // Object will ensure SRDC handle is closed
   CAutoSrdcHandle ash(hSrd);

   CSrdPX14* ctxp = reinterpret_cast<CSrdPX14*>(hSrd);
   SIGASSERT_POINTER(ctxp, CSrdPX14);

   CSigRecDataFile::AcqDeviceParams adp;
   if (!ctxp->GetAcqDeviceParamsFromData(adp))
      return ctxp->SrdError("Failed to interrogate SRDC data: ");

   // - Most stuff is available via AcqDeviceParams
   if (adp.boardName.empty())
      infop->boardName[0] = 0;
   else
   {
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
      strncpy_s(infop->boardName, adp.boardName.c_str(), 15);
#else
      strncpy(infop->boardName, adp.boardName.c_str(), 15);
#endif
      infop->boardName[15] = 0;
   }
   infop->boardSerialNum = adp.serialNum;
   infop->channelCount = adp.channelCount;
   infop->channelNum = adp.channelNum;
   infop->sampSizeBytes = adp.sampSizeBytes;
   infop->sampSizeBits = adp.sampSizeBits;
   infop->bSignedSamples = adp.bSignedSamples ? 1 : 0;
   infop->sampleRateMHz = adp.sampleRateMHz;
   infop->inputVoltRngPP = adp.inputVoltRngPP;
   infop->segment_size = adp.segment_size;


   std::string itemValue;

   infop->header_bytes = 0;
   if (ctxp->GetItemValue("HeaderBytes", itemValue))
      my_ConvertFromString(itemValue.c_str(), infop->header_bytes);

   infop->bTextData = 0;
   if (ctxp->GetItemValue("FileFormat", itemValue))
   {
      if (0 == strcmp_nocase(itemValue.c_str(), "Text"))
      {
         infop->bTextData = 1;

         infop->textRadix = 10;
         if (ctxp->GetItemValue("SampleRadix", itemValue))
            my_ConvertFromString(itemValue.c_str(), infop->textRadix);
      }
   }

   if (operator_notespp)
   {
      if (ctxp->GetItemValue("OperatorNotes", itemValue))
      {
         res = AllocAndCopyString(itemValue.c_str(), operator_notespp);
         PX14_RETURN_ON_FAIL(res);
      }
      else
      {
         // No notes found
         *operator_notespp = NULL;
      }
   }

   return SIG_SUCCESS;
}

PX14API InitRecordedDataInfoPX14 (HPX14 hBrd, PX14S_RECORDED_DATA_INFO* infop)
{
   int res, chan_sel;
   unsigned brd_rev;

   PX14_ENSURE_POINTER(hBrd, infop, PX14S_RECORDED_DATA_INFO, "InitRecordedDataInfoPX14");
   PX14_ENSURE_STRUCT_SIZE(hBrd, infop, _PX14SO_RECORDED_DATA_INFO_V1, "InitRecordedDataInfoPX14");

   res = ValidateHandle(hBrd);
   PX14_RETURN_ON_FAIL(res);

   brd_rev = PX14BRDREV_PX14400;
   GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
   std::string strRev;
   GetBoardRevisionStr(brd_rev, strRev);

#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
   strncpy_s (infop->boardName, strRev.c_str(), 16);
#else
   strncpy (infop->boardName, strRev.c_str(), 16);
#endif

   infop->boardSerialNum = 0;
   GetSerialNumberPX14(hBrd, &infop->boardSerialNum);

   chan_sel = GetActiveChannelsPX14(hBrd);
   infop->channelCount = chan_sel == PX14CHANNEL_DUAL ? 2 : 1;
   if (1 == infop->channelCount)
      infop->channelNum = 1 + chan_sel - PX14CHANNEL_ONE;
   else
      infop->channelNum = 0;

   infop->sampSizeBytes = PX14_SAMPLE_SIZE_IN_BYTES;
   infop->sampSizeBits  = PX14_SAMPLE_SIZE_IN_BITS;
   infop->bSignedSamples = PX14_FALSE;

   infop->sampleRateMHz = 0;
   GetEffectiveAcqRatePX14(hBrd, &infop->sampleRateMHz);

   infop->inputVoltRngPP = 0;
   if (brd_rev == PX14BRDREV_PX14400D)
   {
      switch (GetInputGainLevelDcPX14(hBrd))
      {
         default:
         case PX14DGAIN_LOW:	 infop->inputVoltRngPP = 1.2; break;
         case PX14DGAIN_HIGH: infop->inputVoltRngPP = 0.4; break;
      }
   }
   else
   {
      switch (infop->channelNum)
      {
         default:
         case 1:
            GetInputVoltageRangeVoltsCh1PX14(hBrd, &infop->inputVoltRngPP);
            break;
         case 2:
            GetInputVoltageRangeVoltsCh2PX14(hBrd, &infop->inputVoltRngPP);
            break;
      }
   }

   if (PX14TRIGMODE_SEGMENTED == GetTriggerModePX14(hBrd))
      infop->segment_size = GetSegmentSizePX14(hBrd);
   else
      infop->segment_size = 0;		// Non-segmented

   // We don't touch: header_bytes, bTextData, and textRadix
   //  They're used when the struct is filled in from SRDC data

   if (infop->struct_size >= _PX14SO_RECORDED_DATA_INFO_V2)
   {
      if ((res = GetPreTriggerSamplesPX14(hBrd)) > 0)
      {
         infop->trig_offset = res;
         infop->bPreTrigger = PX14_TRUE;
      }
      else if ((res = GetTriggerDelaySamplesPX14(hBrd)) > 0)
      {
         infop->trig_offset = res;
         infop->bPreTrigger = PX14_FALSE;
      }
      else
      {
         infop->trig_offset = 0;
         infop->bPreTrigger = PX14_FALSE;
      }
   }

   return SIG_SUCCESS;
}

PX14API EnumSrdcItemsAPX14 (HPX14SRDC hFile,
                            char** itemspp,
                            unsigned int flags)
{
   CSrdPX14* ctxp;
   int res;

   SIGASSERT_POINTER(itemspp, char*);
   if (NULL == itemspp)
      return SIG_PX14_INVALID_ARG_2;

   res = CSrdPX14::Validate(hFile, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   std::string str_enum;
   if (!ctxp->EnumItems (str_enum, flags))
      return ctxp->SrdError("Failed to enumerate SRDC items: ");

   if (str_enum.empty())
      *itemspp = NULL;
   else
   {
      res = AllocAndCopyString(str_enum.c_str(), itemspp);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

PX14API IsSrdcFileModifiedPX14 (HPX14SRDC hFile)
{
   CSrdPX14* ctxp;
   int res;

   res = CSrdPX14::Validate(hFile, &ctxp);
   PX14_RETURN_ON_FAIL(res);

   return ctxp->m_bModified ? 1 : 0;
}

