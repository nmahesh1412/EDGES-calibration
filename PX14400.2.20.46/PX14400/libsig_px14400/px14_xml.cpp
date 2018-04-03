/** @file	px14_xml.cpp
  @brief	Code involving XML usage
  */
/*
   Hardware setting get/set routines implemented here:

   SetActiveChannelsPX14
   SetAdcClockSourcePX14
   SetBoardProcessingEnablePX14
   SetInternalAdcClockReferencePX14
   SetDigitalIoEnablePX14
   SetDigitalIoModePX14
   SetExtClockDivider1PX14
   SetExtClockDivider2PX14
   SetMasterSlaveConfigurationPX14
   SetOperatingModePX14
   SetPostAdcClockDividerPX14
   SetPreTriggerSamplesPX14
   SetSabBoardNumberPX14
   SetSabClockPX14
   SetSabConfigurationPX14
   SetSampleCountPX14
   SetSegmentSizePX14
   SetStartSamplePX14
   SetTimestampCounterModePX14
   SetTimestampModePX14
   SetTriggerDelaySamplesPX14
   SetTriggerDirectionAPX14
   SetTriggerDirectionBPX14
   SetTriggerDirectionExtPX14
   SetTriggerLevelAPX14
   SetTriggerLevelBPX14
   SetTriggerModePX14
   SetTriggerSourcePX14

   SetExternalClockRatePX14
   SetInternalAdcClockRatePX14
   */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"
#include "sigsvc_utility.h"

#ifndef PX14_NO_XML_SUPPORT

#include "px14_xml.h"

#define NEW_CHILD(r,n,f)					\
   xmlNewChild(r, NULL, BAD_CAST n,		\
               BAD_CAST my_ConvertToString(f(hBrd)).c_str())

// Module-local function prototypes ------------------------------------- //

/// Workhorse function for saving board settings to XML
static int _SaveSettingsXml (HPX14 hBrd, unsigned int flags,
                             CAutoXmlDocPtr& doc);

/// Apply a setting from XML settings data
static int _ApplySettingXml (HPX14 hBrd, unsigned int flags,
                             const char* settingp, const char* valp);

/// Workhorse function for SaveSettingsToBufferXml*PX14 implementations
template <typename E>
static int SaveSettingsToBufferXmlWork (HPX14 hBrd, unsigned int flags,
                                        E** bufpp, int* buflenp,
                                        const char* encoding);

/// Workhorse function for LoadSettingsFromBufferXml*PX14 implementations
template <typename E>
static int LoadSettingsFromXmlWork (HPX14 hBrd, unsigned int flags,
                                    const E* bufp, int buflen,
                                    const char* encoding);

/// Find the library 'SetXXXPX14' function pointer for given setting name
static px14lib_set_func_t _FindSetFunc (const MySetUintCtxPair* table,
                                        int nItems, const char* settingp);
static px14lib_setd_func_t _FindSetDFunc (const MySetDblCtxPair* table,
                                          int nItems, const char* settingp);

/// Convert setting value from string to integral value
static bool _GetSettingValue (const char* valp, unsigned int flags,
                              unsigned int& value);
//
// PX14 library exports implementation
//

/** @brief Save board settings to a library allocated buffer (ASCII)
*/
PX14API SaveSettingsToBufferXmlAPX14 (HPX14 hBrd,
                                      unsigned int flags,
                                      char** bufpp,
                                      int* buflenp)
{
   int res = SaveSettingsToBufferXmlWork<char>(hBrd, flags, bufpp,
                                               buflenp, "ASCII");
   return res;
}

PX14API LoadSettingsFromBufferXmlAPX14 (HPX14 hBrd,
                                        unsigned int flags,
                                        const char* bufp)
{
   int res = LoadSettingsFromXmlWork<char>(hBrd, flags,
                                           bufp, (int)strlen(bufp), "ASCII");
   return res;
}

PX14API LoadSettingsFromFileXmlAPX14 (HPX14 hBrd,
                                      unsigned int flags,
                                      const char* pathnamep)
{
   int res;

   if (!SysFileExists(pathnamep))
      return SIG_PX14_SOURCE_FILE_OPEN_FAILED;

   res = LoadSettingsFromXmlWork<char>(hBrd, flags,
                                       pathnamep, 0, NULL);
   return res;
}

int GetLibXmlErrorText (std::string& sText)
{
   xmlErrorPtr errp;

   if (NULL == (errp = xmlGetLastError()))
      return SIG_ERROR;	// No error info available

   sText.assign(errp->message);
   TrimString(sText);
   return SIG_SUCCESS;
}

/** @brief Save board settings to an XML file (ASCII)
*/
PX14API SaveSettingsToFileXmlAPX14 (HPX14 hBrd,
                                    unsigned int flags,
                                    const char* pathnamep,
                                    const char* encodingp)
{
   xmlSaveCtxtPtr ctxp;
   int res, options;

   SIGASSERT_NULL_OR_POINTER(encodingp, char);
   SIGASSERT_POINTER(pathnamep, char);
   if (NULL == pathnamep)
      return SIG_PX14_INVALID_ARG_3;

   // Generate XML data
   CAutoXmlDocPtr spDoc;
   res = _SaveSettingsXml(hBrd, flags, spDoc);
   PX14_RETURN_ON_FAIL(res);

   options = 0;
   if (flags & PX14XMLSET_FORMAT_OUTPUT)
      options |= XML_SAVE_FORMAT;
   if (flags & PX14XMLSET_NODE_ONLY)
      options |= XML_SAVE_NO_DECL;

   ctxp = xmlSaveToFilename(pathnamep, encodingp, options);
   if (NULL == ctxp) return SIG_PX14_XML_GENERIC;
   xmlSaveDoc(ctxp, spDoc);
   xmlSaveClose(ctxp);
   return SIG_SUCCESS;
}

#ifndef PX14PP_NO_UNICODE_SUPPORT

PX14API LoadSettingsFromBufferXmlWPX14 (HPX14 hBrd,
                                        unsigned int flags,
                                        const wchar_t* bufp)
{
   int res = LoadSettingsFromXmlWork<wchar_t>(hBrd, flags,
                                              bufp, (int)wcslen(bufp), "UTF-16");
   return res;
}

PX14API SaveSettingsToBufferXmlWPX14 (HPX14 hBrd,
                                      unsigned int flags,
                                      wchar_t** bufpp,
                                      int* buflenp)
{
   int res = SaveSettingsToBufferXmlWork<wchar_t>(hBrd, flags, bufpp,
                                                  buflenp, "UTF-16");
   return res;
}

#else

#ifdef _WIN32
PX14API LoadSettingsFromBufferXmlWPX14 (HPX14 hBrd,
                                        unsigned int flags,
                                        const wchar_t* bufp)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}

PX14API SaveSettingsToBufferXmlWPX14 (HPX14 hBrd,
                                      unsigned int flags,
                                      wchar_t** bufpp,
                                      int* buflenp)
{
   return SIG_PX14_NOT_IMPLEMENTED;
}
#endif

#endif

//
// Module private function implementation
//

int _SaveSettingsXml (HPX14 hBrd, unsigned int flags, CAutoXmlDocPtr& doc)
{
   xmlNodePtr rootp;
   CStatePX14* brdp;
   double dValue;
   int res;

   if (SIG_SUCCESS != (res = ValidateHandle(hBrd, &brdp)))
      return res;

   // Create DOM document object
   doc = xmlNewDoc(BAD_CAST "1.0");

   // Create our root node: <PX14_SETTINGS SerialNumber=######>
   rootp = xmlNewNode(NULL, BAD_CAST "PX14_SETTINGS");
   GetSerialNumberPX14(hBrd, reinterpret_cast<unsigned int*>(&res));
   xmlNewProp(rootp, BAD_CAST "SerialNumber", BAD_CAST my_ConvertToString(res).c_str());
   xmlDocSetRootElement(doc, rootp);

   // Create all settings nodes: <Setting>Value</Setting>
   NEW_CHILD(rootp, "SegmentSize",			GetSegmentSizePX14);
   NEW_CHILD(rootp, "SampleCount",			GetSampleCountPX14);
   NEW_CHILD(rootp, "StartSample",         GetStartSamplePX14);
   NEW_CHILD(rootp, "TriggerDelaySamples", GetTriggerDelaySamplesPX14);
   NEW_CHILD(rootp, "PreTriggerSamples",   GetPreTriggerSamplesPX14);
   NEW_CHILD(rootp, "TriggerLevelA",       GetTriggerLevelAPX14);
   NEW_CHILD(rootp, "TriggerLevelB",       GetTriggerLevelBPX14);
   NEW_CHILD(rootp, "TriggerMode",         GetTriggerModePX14);
   NEW_CHILD(rootp, "TriggerSource",       GetTriggerSourcePX14);
   NEW_CHILD(rootp, "TriggerDirectionA",   GetTriggerDirectionAPX14);
   NEW_CHILD(rootp, "TriggerDirectionB",   GetTriggerDirectionBPX14);
   NEW_CHILD(rootp, "TriggerDirectionExt", GetTriggerDirectionExtPX14);
   NEW_CHILD(rootp, "AdcInternalClockReference",   GetInternalAdcClockReferencePX14);
   NEW_CHILD(rootp, "AdcClockSource",      GetAdcClockSourcePX14);
   NEW_CHILD(rootp, "PostAdcClockDivider", GetPostAdcClockDividerPX14);
   NEW_CHILD(rootp, "MasterSlaveConfiguration",
             GetMasterSlaveConfigurationPX14);
   NEW_CHILD(rootp, "DigitalIoMode",       GetDigitalIoModePX14);
   NEW_CHILD(rootp, "DigitalIoEnable",     GetDigitalIoEnablePX14);
   NEW_CHILD(rootp, "ActiveChannels",      GetActiveChannelsPX14);
   NEW_CHILD(rootp, "PowerDownOverride",   GetPowerDownOverridePX14);
   NEW_CHILD(rootp, "SabBoardNumber",      GetSabBoardNumberPX14);
   NEW_CHILD(rootp, "SabConfiguration",    GetSabConfigurationPX14);
   NEW_CHILD(rootp, "SabClock",            GetSabClockPX14);
   NEW_CHILD(rootp, "ExtClockDivider1",    GetExtClockDivider1PX14);
   NEW_CHILD(rootp, "ExtClockDivider2",    GetExtClockDivider2PX14);
   NEW_CHILD(rootp, "TimestampMode",       GetTimestampModePX14);
   NEW_CHILD(rootp, "TimestampCounterMode",GetTimestampCounterModePX14);
   NEW_CHILD(rootp, "BoardProcessingEnable", GetBoardProcessingEnablePX14);
   NEW_CHILD(rootp, "InputVoltRangeCh1",   GetInputVoltRangeCh1PX14);
   NEW_CHILD(rootp, "InputVoltRangeCh2",   GetInputVoltRangeCh2PX14);
   NEW_CHILD(rootp, "DcOffsetCh1",         GetDcOffsetCh1PX14);
   NEW_CHILD(rootp, "DcOffsetCh2",         GetDcOffsetCh2PX14);
   NEW_CHILD(rootp, "FineDcOffsetCh1",     GetFineDcOffsetCh1PX14);
   NEW_CHILD(rootp, "FineDcOffsetCh2",     GetFineDcOffsetCh2PX14);
   NEW_CHILD(rootp, "InputGainLevelDc",    GetInputGainLevelDcPX14);

   // We've got a couple that don't fit scheme above
   GetInternalAdcClockRatePX14(hBrd, &dValue);
   xmlNewChild(rootp, NULL, BAD_CAST "InternalClockRate",
               BAD_CAST my_ConvertToString(dValue).c_str());
   GetExternalClockRatePX14(hBrd, &dValue);
   xmlNewChild(rootp, NULL, BAD_CAST "ExternalClockRate",
               BAD_CAST my_ConvertToString(dValue).c_str());

   // PX14HWSET

   return SIG_SUCCESS;
}

   template <typename E>
int SaveSettingsToBufferXmlWork (HPX14 hBrd,
                                 unsigned int flags,
                                 E** bufpp,
                                 int* buflenp,
                                 const char* encoding)
{
   int buffersize, res, format;
   xmlChar *xmlbuff;

   SIGASSERT_POINTER(bufpp, E*);
   SIGASSERT_POINTER(buflenp, int);
   if (NULL == bufpp)
      return SIG_PX14_INVALID_ARG_3;
   if (NULL == buflenp)
      return SIG_PX14_INVALID_ARG_4;

   // Generate XML data
   CAutoXmlDocPtr spDoc;
   res = _SaveSettingsXml(hBrd, flags, spDoc);
   PX14_RETURN_ON_FAIL(res);

   format = (flags & PX14XMLSET_FORMAT_OUTPUT) ? 1 : 0;
   buffersize = 0; xmlbuff = NULL;

   if (flags & PX14XMLSET_NODE_ONLY)
   {
      // Caller only wants root node (no XML header info)

      xmlCharEncodingHandlerPtr conv_hdlr;
      xmlOutputBufferPtr out_buf;

      conv_hdlr = encoding ? xmlFindCharEncodingHandler(encoding) : NULL;
      if (NULL == (out_buf = xmlAllocOutputBuffer(conv_hdlr)))
         return SIG_ERROR;

      xmlNodeDumpOutput(out_buf, spDoc, xmlDocGetRootElement(spDoc), 0, format, encoding);
      xmlOutputBufferFlush(out_buf);
#if defined(_WIN32) || LIBXML_VERSION<20900
      if (out_buf->conv != NULL)
      {
         buffersize = out_buf->conv->use;
         xmlbuff = xmlStrndup(out_buf->conv->content, buffersize);
      }
      else
      {
         buffersize = out_buf->buffer->use;
         xmlbuff = xmlStrndup(out_buf->buffer->content, buffersize);
      }
      xmlOutputBufferClose(out_buf);
#else
      buffersize = xmlOutputBufferGetSize (out_buf);
      xmlbuff = xmlStrndup (xmlOutputBufferGetContent (out_buf), buffersize);
#endif

   }
   else
   {
      // Caller wants full XML dump
      xmlDocDumpFormatMemoryEnc(spDoc, &xmlbuff, &buffersize, encoding, format);
   }
   // bytes to elements
   buffersize /= sizeof(E);

   if (buffersize && xmlbuff)
   {
      // Move from the libxml buffer to our own
      *bufpp = reinterpret_cast<E*>(my_malloc((buffersize+1) * sizeof(E)));
      *buflenp = buffersize;
      memcpy (*bufpp, xmlbuff, buffersize * sizeof(E));
      (*bufpp)[buffersize] = E(0);
      xmlFree(xmlbuff);

      return SIG_SUCCESS;
   }

   return SIG_ERROR;
}

   template <typename E>
int LoadSettingsFromXmlWork (HPX14 hBrd,
                             unsigned int flags,
                             const E* bufp,
                             int buflen,
                             const char* encoding)
{
   int res, nSuccesses, nFailures, first_failure, xml_parse_opts;
   xmlNodePtr rootp, childp;
   CStatePX14* brdp;
   xmlChar* xcp;

   SIGASSERT_POINTER(bufp, char);
   SIGASSERT_NULL_OR_POINTER(encoding, char);
   if (NULL == bufp)
      return SIG_PX14_INVALID_ARG_3;

   if (SIG_SUCCESS != (res = ValidateHandle(hBrd, &brdp)))
      return res;

   // Don't dump XML warnings/errors to screen
   xml_parse_opts = XML_PARSE_NOERROR | XML_PARSE_NOWARNING;

   // Load the XML data
   CAutoXmlDocPtr spDoc;
   if (!buflen)
   {
      // Grab XML data from file
      spDoc = xmlReadFile(reinterpret_cast<const char*>(bufp),
                          encoding, xml_parse_opts);
   }
   else
   {
      // Grab XML data from buffer
      spDoc = xmlReadMemory(reinterpret_cast<const char*>(bufp),
                            buflen * sizeof(E), NULL, encoding, xml_parse_opts);
   }
   if (!spDoc.Valid())
      return SIG_PX14_XML_MALFORMED;

   // Grab root node: <PX14_SETTINGS SerialNumber=######>
   if (NULL == (rootp = xmlDocGetRootElement(spDoc)))
      return SIG_PX14_XML_MALFORMED;
   if (xmlStrcasecmp(rootp->name, BAD_CAST "PX14_SETTINGS"))
      return SIG_PX14_XML_INVALID;

   // Set power up defaults
   if (0 == (flags & PX14XMLSET_NO_PRELOAD_DEFAULTS))
   {
      res = SetPowerupDefaultsPX14(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }

   // Defer the checking of the PLL lock; we're going to be changing PLL
   // parameters several times. Without this deferral we'll be waiting
   // after each change when we really only need to wait after all
   // settings have been applied.
   CAutoPllLockCheckDeferral auto_pcd(hBrd);

   first_failure = SIG_SUCCESS;
   nSuccesses = nFailures = 0;

   // For all settings...
   for (childp=rootp->xmlChildrenNode; childp; childp=childp->next)
   {
      if (XML_ELEMENT_NODE != childp->type)
         continue;

      // Get node's content: the value that we're to set
      xcp = xmlNodeGetContent(childp);

      // Apply setting to the board
      res = _ApplySettingXml(hBrd, flags,
                             reinterpret_cast<const char*>(childp->name),
                             reinterpret_cast<const char*>(xcp));
      if (xcp)
         xmlFree(xcp);

      if (SIG_SUCCESS == res)
         nSuccesses++;
      else
      {
         if (res != SIG_PX14_INVALID_OP_FOR_BRD_CONFIG)
         {
            if (first_failure == SIG_SUCCESS)
               first_failure = res;

            nFailures++;
         }
      }
   }

   // Explicitly release so we can check result
   res = auto_pcd.Release();
   PX14_RETURN_ON_FAIL(res);

   return nFailures ? (nSuccesses ? SIG_PX14_QUASI_SUCCESSFUL : first_failure) : SIG_SUCCESS;
}

/**
  At some point, we might add the facility to include PX14 library
  constants as attribute values to improve readability. This would
  bloat the library and slow things down a bit though so we'll see.
  */
bool _GetSettingValue (const char* valp, unsigned int /*flags*/,
                       unsigned int& value)
{
   unsigned long val_conv;
   char* endp;

   // For now, just assuming integral data; octal and hexadecimal okay
   val_conv = strtoul(valp, &endp, 0);
   if (*endp)
      return false;

   if (val_conv > UINT_MAX)
      val_conv = UINT_MAX;

   value = static_cast<unsigned int>(val_conv);
   return true;
}

px14lib_set_func_t _FindSetFunc (const MySetUintCtxPair* table,
                                 int nItems,
                                 const char* settingp)
{
   int low, high, cur, cmp;

   low = 0;
   high = nItems - 1;
   cur = low + ((high - low) >> 1);

   while (low <= high)
   {
      cmp = strcmp_nocase(table[cur]._attrib, settingp);

      if (cmp > 0)
         high = cur - 1;
      else if (cmp < 0)
         low =  cur + 1;
      else
         return table[cur]._funcp;

      cur = low + ((high - low) > 1);
   }

   return NULL;
}

px14lib_setd_func_t _FindSetDFunc (const MySetDblCtxPair* table,
                                   int nItems,
                                   const char* settingp)
{
   int low, high, cur, cmp;

   low = 0;
   high = nItems - 1;
   cur = low + ((high - low) >> 1);

   while (low <= high)
   {
      cmp = strcmp_nocase(table[cur]._attrib, settingp);

      if (cmp > 0)
         high = cur - 1;
      else if (cmp < 0)
         low =  cur + 1;
      else
         return table[cur]._funcp;

      cur = low + ((high - low) > 1);
   }

   return NULL;
}

int _ApplySettingXml (HPX14 hBrd, unsigned int flags,
                      const char* settingp, const char* valp)
{
   SIGASSERT_POINTER(settingp, char);
   SIGASSERT_POINTER(valp, char);

   // PX14HWSET

   static const MySetUintCtxPair setFuncs[] =
   {
      {"activechannels",           SetActiveChannelsPX14},
      {"adcclocksource",           SetAdcClockSourcePX14},
      {"adcinternalclockreference",
         SetInternalAdcClockReferencePX14},
      {"boardprocessingenable",    SetBoardProcessingEnablePX14},
      {"dcoffsetch1",              SetDcOffsetCh1PX14},
      {"dcoffsetch2",              SetDcOffsetCh2PX14},
      {"digitalioenable",          SetDigitalIoEnablePX14},
      {"digitaliomode",            SetDigitalIoModePX14},
      {"extclockdivider1",         SetExtClockDivider1PX14},
      {"extclockdivider2",         SetExtClockDivider2PX14},
      {"finedcoffsetch1",          SetFineDcOffsetCh1PX14},
      {"finedcoffsetch2",          SetFineDcOffsetCh2PX14},
      {"inputgainleveldc",        SetInputGainLevelDcPX14},
      {"inputvoltrangech1",        SetInputVoltRangeCh1PX14},
      {"inputvoltrangech2",        SetInputVoltRangeCh2PX14},
      {"masterslaveconfiguration", SetMasterSlaveConfigurationPX14},
      {"postadcclockdivider",      SetPostAdcClockDividerPX14},
      {"powerdownoverride",        SetPowerDownOverridePX14},
      {"pretriggersamples",        SetPreTriggerSamplesPX14},
      {"sabboardnumber",           SetSabBoardNumberPX14},
      {"sabclock",                 SetSabClockPX14},
      {"sabconfiguration",         SetSabConfigurationPX14},
      {"samplecount",              SetSampleCountPX14},
      {"segmentsize",              SetSegmentSizePX14},
      {"startsample",              SetStartSamplePX14},
      {"timestampcountermode",     SetTimestampCounterModePX14},
      {"timestampmode",            SetTimestampModePX14},
      {"triggerdelaysamples",      SetTriggerDelaySamplesPX14},
      {"triggerdirectiona",        SetTriggerDirectionAPX14},
      {"triggerdirectionb",        SetTriggerDirectionBPX14},
      {"triggerdirectionext",      SetTriggerDirectionExtPX14},
      {"triggerlevela",            SetTriggerLevelAPX14},
      {"triggerlevelb",            SetTriggerLevelBPX14},
      {"triggermode",              SetTriggerModePX14},
      {"triggersource",            SetTriggerSourcePX14},
   };
   static const int table_size =
      static_cast<int>(sizeof(setFuncs)/sizeof(setFuncs[0]));

   static const MySetDblCtxPair setDFuncs[] =
   {
      {"externalclockrate",        SetExternalClockRatePX14},
      {"internalclockrate",        SetInternalAdcClockRatePX14}
   };
   static const int table_d_size =
      static_cast<int>(sizeof(setDFuncs)/sizeof(setDFuncs[0]));

#ifdef _DEBUG
   // Since we're using a binary search to find table elements we'll run
   //  a quick check to see that all items are in ascending order. This
   //  only happens in DEBUG builds of the library the first time through.
   //  There's a slight race condition here, but the worst that will happen
   //  is an extra check of the data.
   static volatile bool sbChecked = false;
   if (!sbChecked)
   {
      sbChecked = true;
      for (int i=1; i<table_size; i++)
         SIGASSERT(strcmp(setFuncs[i-1]._attrib, setFuncs[i]._attrib) < 0);
      for (int i=1; i<table_d_size; i++)
         SIGASSERT(strcmp(setDFuncs[i-1]._attrib, setDFuncs[i]._attrib) < 0);
   }
#endif

   px14lib_set_func_t funcp;
   px14lib_setd_func_t funcdp;
   unsigned int val;
   double dValue;

   funcp = _FindSetFunc(setFuncs, table_size, settingp);
   if (funcp)
   {
      if (!_GetSettingValue(valp, flags, val))
         return SIG_PX14_XML_INVALID;

      // Make the actual setting
      return (*funcp)(hBrd, val);
   }

   funcdp = _FindSetDFunc(setDFuncs, table_d_size, settingp);
   if (funcdp)
   {
      if (!my_ConvertFromString(valp, dValue))
         return SIG_PX14_XML_INVALID;

      // Make the actual setting
      return (*funcdp)(hBrd, dValue);
   }

   return 1;		// Function not found
}

#endif	// PX14_NO_XML_SUPPORT

