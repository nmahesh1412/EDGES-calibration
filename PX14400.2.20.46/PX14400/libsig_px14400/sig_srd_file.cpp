/** @file	sig_srd_file.cpp
  @brief	Internal API for Signatec Recorded Data files (*.srdc)
  */
#include "stdafx.h"
#include "sig_srd_file.h"
#include "sigsvc_utility.h"

#define SRDC_ROOT_NAME			"SignatecRecordedData"

// All error response strings are wrapped with this so we can easily
//  preprocess them out if desired
#define ERR_STR(e)				e

CSigRecDataFile::AcqDeviceParams::AcqDeviceParams (const AcqDeviceParams& cpy) :
   boardName(cpy.boardName), serialNum(cpy.serialNum),
   channelCount(cpy.channelCount), channelNum(cpy.channelNum),
   channelMask(cpy.channelMask),
   sampSizeBytes(cpy.sampSizeBytes), sampSizeBits(cpy.sampSizeBits),
   bSignedSamples(cpy.bSignedSamples), sampleRateMHz(cpy.sampleRateMHz),
   inputVoltRngPP(cpy.inputVoltRngPP), segment_size(cpy.segment_size),
   pretrig_samples(cpy.pretrig_samples),
   delaytrig_samples(cpy.delaytrig_samples)
{
}

CSigRecDataFile::AcqDeviceParams&
CSigRecDataFile::AcqDeviceParams::operator= (const AcqDeviceParams& cpy)
{
   boardName = cpy.boardName;
   serialNum = cpy.serialNum;
   channelCount = cpy.channelCount;
   channelNum = cpy.channelNum;
   channelMask = cpy.channelMask;
   sampSizeBytes = cpy.sampSizeBytes;
   sampSizeBits = cpy.sampSizeBits;
   bSignedSamples = cpy.bSignedSamples;
   sampleRateMHz = cpy.sampleRateMHz;
   inputVoltRngPP = cpy.inputVoltRngPP;
   segment_size = cpy.segment_size;
   pretrig_samples = cpy.pretrig_samples;
   delaytrig_samples = cpy.delaytrig_samples;

   return *this;
}


CSigRecDataFile::CSigRecDataFile()
: m_bModified(false)
{
}

CSigRecDataFile::~CSigRecDataFile()
{
}

bool CSigRecDataFile::_Failed(const char* reason, bool bAppendXmlErroInfo)
{
   if (reason)
      m_errText.assign(reason);
   else
      m_errText.clear();

   if (bAppendXmlErroInfo)
   {
      xmlErrorPtr errp;

      if (NULL != (errp = xmlGetLastError()))
      {
         int nremove;

         m_errText.append("(XML: ");
         m_errText.append(errp->message);

         // Remove trailing whitespace from error text; libxml will
         //  sometimes have newlines at the end of error text
         std::string::reverse_iterator r(m_errText.rbegin());
         for (nremove=0; r!=m_errText.rend() && isspace(*r); r++)
            nremove++;
         if (nremove)
            m_errText.erase(m_errText.length() - nremove);

         m_errText.append(1, ')');
      }
   }

   return false;
}

#ifdef _WIN32
bool CSigRecDataFile::BufferFileInMemory (const char* pathnamep,
                                          void** bufpp,
                                          unsigned* bytesp)
{
   LARGE_INTEGER liBytes;
   DWORD dwRead;
   HANDLE hTry;
   void* fbufp;
   bool bRes;

   bRes = false;

   SIGASSERT_POINTER(pathnamep, char);
   SIGASSERT_POINTER(bufpp, void*);
   SIGASSERT_POINTER(bytesp, unsigned);
   if (pathnamep && bufpp && bytesp)
   {
      hTry = CreateFileA(pathnamep, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
      if (INVALID_HANDLE_VALUE != hTry)
      {
         if (GetFileSizeEx(hTry, &liBytes) &&
             (liBytes.QuadPart > 0) && (liBytes.QuadPart < UINT_MAX))
         {
            fbufp = malloc(liBytes.LowPart);
            if (NULL != fbufp)
            {
               if (ReadFile(hTry, fbufp, liBytes.LowPart, &dwRead, NULL))
               {
                  *bytesp = liBytes.LowPart;
                  *bufpp = fbufp;
                  bRes = true;
               }
               else
                  free (fbufp);
            }
         }

         CloseHandle(hTry);
      }
   }

   return bRes;
}
#endif

bool CSigRecDataFile::LoadFrom (const char* pathnamep, bool bResetContent)
{
   xmlNodePtr rootp, childp;
   xmlChar* xcp;

   if (bResetContent)
      ResetContent();

   CAutoXmlDocPtr spDoc;

   // - Load the XML data
   bool bLoaded = false;
#ifdef _WIN32

   const char* pNoRoot;
   unsigned raw_bytes;
   void* rawp;

   // Only buffer if we have to; : in filename indicates AFS
   if (NULL == (pNoRoot = PathSkipRootA(pathnamep)))
      pNoRoot = pathnamep;
   if (pNoRoot && strchr(pNoRoot, ':'))
   {
      // Rather than loading directly from a file via xmlReadFile, we're
      //  reading from file into a temporary buffer and parsing from there.
      //  The reason for this is so we can embed SRDC data in an alternate
      //  file stream for NTFS systems. The xmlReadFile fails to read data
      //  from AFS. (We can write okay though.)
      if (BufferFileInMemory(pathnamep, &rawp, &raw_bytes) &&
          rawp && raw_bytes)
      {
         spDoc = xmlReadMemory(reinterpret_cast<const char*>(rawp),
                               raw_bytes, NULL, NULL, XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
         free(rawp);
         bLoaded = true;
      }
   }
#endif
   if (!bLoaded)
   {
      // Default to original implementation
      spDoc = xmlReadFile(pathnamep, NULL,
                          XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
   }

   if (!spDoc.Valid() || (NULL == (rootp = xmlDocGetRootElement(spDoc))))
      return _Failed(ERR_STR("Malformed XML"));

   // Root node must be <SignatecRecordedData> (==SRDC_ROOT_NAME)
   if (xmlStrcasecmp(rootp->name, BAD_CAST SRDC_ROOT_NAME))
      return _Failed(ERR_STR("Corrupt or invalid SRDC file"));

   std::string itemName;

   // Walk through and add items to our map
   for (childp=rootp->xmlChildrenNode; childp; childp=childp->next)
   {
      if (XML_ELEMENT_NODE != childp->type)
         continue;

      itemName.assign(reinterpret_cast<const char*>(childp->name));

      // Get node's content: the value that we're to set
      ItemValue iv;
      xcp = xmlNodeGetContent(childp);
      if (NULL != xcp)
      {
         iv.itemValue.assign(reinterpret_cast<const char*>(xcp));
         xmlFree(xcp);
      }

      m_valMap[itemName] = iv;
   }

   m_pathname.assign(pathnamep);

   return true;
}

bool CSigRecDataFile::SaveTo (const char* pathnamep)
{
   xmlSaveCtxtPtr ctxp;

   SIGASSERT_NULL_OR_POINTER(pathnamep, char);
   if (NULL == pathnamep)
   {
      // Use current pathname
      if (m_pathname.empty())
         return _Failed("No output pathname specified");

      pathnamep = m_pathname.c_str();
   }

   CAutoXmlDocPtr spDoc;
   if (!CreateOutputXml(spDoc))
      return false;

   ctxp = xmlSaveToFilename(pathnamep, "UTF-8", XML_SAVE_FORMAT);
   if (NULL == ctxp)
      return _Failed("Failed to write XML data: ", true);
   xmlSaveDoc(ctxp, spDoc);
   xmlSaveClose(ctxp);

   // Reset 'modified' bit for all items
   ValueMap::iterator iVal(m_valMap.begin());
   for (; iVal!=m_valMap.end(); iVal++)
      iVal->second.flags &= ~IVF_MODIFIED;

   m_pathname.assign(pathnamep);
   m_bModified = false;

   return true;
}

void CSigRecDataFile::ResetContent()
{
   m_pathname.clear();
   m_errText.clear();
   m_valMap.clear();
   m_bModified = false;
}

bool CSigRecDataFile::ItemExists(const char* itemName) const
{
   return m_valMap.find(itemName) != m_valMap.end();
}

bool CSigRecDataFile::GetItemValue (const char* itemName,
                                    std::string& itemValue) const
{
   SIGASSERT_POINTER(itemName, char);
   if (NULL != itemName)
   {
      ValueMap::const_iterator iValue(m_valMap.find(itemName));
      if (iValue != m_valMap.end())
      {
         itemValue.assign(iValue->second.itemValue);
         return true;
      }
   }

   return false;
}

/// Add or modify an item name/value pair to SRDC data
bool CSigRecDataFile::RefreshItem (const char* itemName,
                                   const char* itemValue)
{
   SIGASSERT_NULL_OR_POINTER(itemValue, char);
   SIGASSERT_POINTER(itemName, char);

   std::string strName(itemName);

   if (NULL == itemValue)
   {
      m_valMap.erase(strName);
      return true;
   }

   std::string strVal(itemValue);

   ValueMap::iterator iVal(m_valMap.find(strName));
   if (iVal != m_valMap.end())
   {
      if (iVal->second.itemValue.compare(strVal))
      {
         iVal->second.itemValue.assign(strVal);
         iVal->second.flags |= IVF_MODIFIED;
         m_bModified = true;
      }
   }
   else
   {
      ItemValue iv;
      iv.itemValue.assign(strVal);
      iv.flags |= IVF_MODIFIED;
      m_bModified = true;

      m_valMap[strName] = iv;
   }

   return true;
}

bool CSigRecDataFile::GetPredefinedParameters (AcqDeviceParams& adp)
{
   // Anyone using RefreshKnownParameters will need to override this
   return false;
}

bool CSigRecDataFile::RefreshKnownParameters()
{
   AcqDeviceParams adp;
   // Sub-class will implement this
   if (!GetPredefinedParameters(adp))
      return false;

   // NOTE: If you add or remove any of these, be sure to document changes
   //       in the product's Operator's Manual

   // -- Source board information
   RefreshItem("SourceBoard", adp.boardName.c_str());
   RefreshItem("SourceBoardSerialNum",
               my_ConvertToString(adp.serialNum).c_str());

   // -- Data sample information
   RefreshItem("ChannelCount",
               my_ConvertToString(adp.channelCount).c_str());
   RefreshItem("ChannelId",
               my_ConvertToString(adp.channelNum).c_str());
   RefreshItem("ChannelMask",
               my_ConvertToString(adp.channelMask).c_str());
   RefreshItem("SampleSizeBytes",
               my_ConvertToString(adp.sampSizeBytes).c_str());
   RefreshItem("SampleSizeBits",
               my_ConvertToString(adp.sampSizeBits).c_str());
   RefreshItem("SampleFormat",
               adp.bSignedSamples ? "Signed" : "Unsigned");

   // -- Data acquisition information
   RefreshItem("SamplingRateMHz",
               my_ConvertToString(adp.sampleRateMHz).c_str());
   RefreshItem("PeakToPeakInputVoltRange",
               my_ConvertToString(adp.inputVoltRngPP).c_str());
   RefreshItem("SegmentSize",
               my_ConvertToString(adp.segment_size).c_str());
   RefreshItem("PreTriggerSampleCount",
               my_ConvertToString(adp.pretrig_samples).c_str());
   RefreshItem("TriggerDelaySamples",
               my_ConvertToString(adp.delaytrig_samples).c_str());

   return true;
}

bool CSigRecDataFile::OutputItem (xmlNodePtr rootp, const char* itemName)
{
   SIGASSERT_POINTER(itemName, char);
   if (NULL == itemName)
      return false;

   ValueMap::iterator iItem(m_valMap.find(itemName));
   if (iItem != m_valMap.end())
   {
      std::string itemValue(iItem->second.itemValue);
      my_EscapeReservedXmlMarkupChars(itemValue);

      xmlNewChild(rootp, NULL,
                  BAD_CAST itemName,
                  BAD_CAST itemValue.c_str());

      iItem->second.flags |= IVF_SAVED;
   }

   return true;
}

bool CSigRecDataFile::CreateOutputXml(CAutoXmlDocPtr& doc)
{
   xmlNodePtr rootp, nodep;

   // Rather than just spin on the contents of our map, we go through a
   // little extra work to ensure that we have a semi-ordered output that
   // aggregates related, known items.

   // Create DOM document object
   doc = xmlNewDoc(BAD_CAST "1.0");
   if (!doc.Valid())
      return _Failed("Failed to create XML document", true);

   // Create our root node: <SignatecRecordedData> (==SRDC_ROOT_NAME)
   rootp = xmlNewNode(NULL, BAD_CAST SRDC_ROOT_NAME);
   if (NULL == rootp)
      return _Failed("Failed to create XML document", true);
   xmlDocSetRootElement(doc, rootp);

   // Reset 'saved' bit for all items
   ValueMap::iterator iVal(m_valMap.begin());
   for (; iVal!=m_valMap.end(); iVal++)
      iVal->second.flags &= ~IVF_SAVED;

   //// -- Source board information --
   if (ItemExists("SourceBoard"))
   {
      nodep = xmlNewComment(BAD_CAST "Source board information");
      if (nodep) xmlAddChild(rootp, nodep);

      OutputItem(rootp, "SourceBoard");
      OutputItem(rootp, "SourceBoardSerialNum");
   }

   // -- Data sample information --
   nodep = xmlNewComment(BAD_CAST "Data sample information");
   if (nodep) xmlAddChild(rootp, nodep);
   OutputItem(rootp, "ChannelCount");
   OutputItem(rootp, "ChannelId");
   OutputItem(rootp, "ChannelMask");
   OutputItem(rootp, "SampleSizeBytes");
   OutputItem(rootp, "SampleSizeBits");
   OutputItem(rootp, "SampleFormat");

   // -- Data acquisition information --
   nodep = xmlNewComment(BAD_CAST "Data acquisition information");
   if (nodep) xmlAddChild(rootp, nodep);
   OutputItem(rootp, "SamplingRateMHz");
   OutputItem(rootp, "PeakToPeakInputVoltRange");
   OutputItem(rootp, "SegmentSize");
   OutputItem(rootp, "PreTriggerSampleCount");
   OutputItem(rootp, "TriggerDelaySamples");

   // Now output unsaved items
   OutputUnsavedItems(rootp);

   return true;
}

bool CSigRecDataFile::OutputUnsavedItems(xmlNodePtr rootp)
{
   xmlNodePtr nodep;
   bool bFirstItem;

   // Output all items that do not have the IVF_SAVED bit set
   ValueMap::iterator iValue(m_valMap.begin());
   for (bFirstItem=true; iValue!=m_valMap.end(); iValue++)
   {
      if (0 == (iValue->second.flags & IVF_SAVED))
      {
         if (bFirstItem)
         {
            nodep = xmlNewComment(BAD_CAST "Other items");
            if (nodep) xmlAddChild(rootp, nodep);
            bFirstItem = false;
         }

         OutputItem(rootp, iValue->first.c_str());
      }
   }

   return true;
}

bool CSigRecDataFile::GetAcqDeviceParamsFromData(AcqDeviceParams& adp)
{
   std::string itemValue;

   // -- Set defaults for all values
   adp.boardName.empty();
   adp.serialNum = 0;

   adp.channelCount = 1;
   adp.channelNum = 1;
   adp.channelMask = 0;
   adp.sampSizeBytes = 2;
   adp.sampSizeBits = 16;
   adp.bSignedSamples = false;

   adp.sampleRateMHz = 0;
   adp.inputVoltRngPP = 0;

   adp.segment_size = 0;
   adp.pretrig_samples = 0;
   adp.delaytrig_samples = 0;

   if (GetItemValue("SourceBoard", itemValue))
      adp.boardName.assign(itemValue);
   if (GetItemValue("SourceBoardSerialNum", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.serialNum);
   if (GetItemValue("ChannelCount", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.channelCount);
   if (GetItemValue("ChannelId", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.channelNum);
   if (GetItemValue("ChannelMask", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.channelMask);
   if (GetItemValue("SampleSizeBytes", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.sampSizeBytes);
   if (GetItemValue("SampleSizeBits", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.sampSizeBits);
   if (GetItemValue("SampleFormat", itemValue))
   {
      std::string::iterator is(itemValue.begin());
      for (; is!=itemValue.end(); is++) *is = tolower(*is);

      if (0 == itemValue.compare("unsigned"))
         adp.bSignedSamples = false;
      else if (0 == itemValue.compare("signed"))
         adp.bSignedSamples = true;
      else
         my_ConvertFromString(itemValue.c_str(), adp.bSignedSamples);
   }
   if (GetItemValue("SamplingRateMHz", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.sampleRateMHz);
   if (GetItemValue("PeakToPeakInputVoltRange", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.inputVoltRngPP);
   if (GetItemValue("SegmentSize", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.segment_size);
   if (GetItemValue("PreTriggerSampleCount", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.pretrig_samples);
   if (GetItemValue("TriggerDelaySamples", itemValue))
      my_ConvertFromString(itemValue.c_str(), adp.delaytrig_samples);

   return true;
}

static int mycmp(const char** a, const char** b)
{
   return strcmp(*a, *b);
}
bool CSigRecDataFile::IsStdItem (const std::string& sItem) const
{
   // These items must be in increasing lexicographic order!
   static const char* std_set[] =
   {
      "ChannelCount",
      "ChannelId",
      "ChannelMask",
      "FileFormat",
      "HeaderBytes",
      "OperatorNotes",
      "PeakToPeakInputVoltRange",
      "PreTriggerSampleCount",
      "RecArmTimeSec",
      "RecArmTimeStr",
      "RecEndTimeSec",
      "RecEndTimeStr",
      "SampleFormat",
      "SampleRadix",
      "SampleSizeBits",
      "SampleSizeBytes",
      "SamplingRateMHz",
      "SegmentSize",
      "SourceBoard",
      "SourceBoardSerialNum",
      "TriggerDelaySamples",
   };

   const char* keyp = sItem.c_str();
   return NULL != bsearch(&keyp, std_set, sizeof(std_set)/sizeof(std_set[0]), sizeof(const char*),
                          (int (*)(const void*, const void*))mycmp);
}

bool CSigRecDataFile::EnumItems (std::string& str_enum, unsigned int flags)
{
   bool bAdded, bStd;

   std::ostringstream oss;

   // For all items...
   ValueMap::const_iterator iItem(m_valMap.begin());
   for (bAdded=false; iItem!=m_valMap.end(); iItem++)
   {
      bStd = IsStdItem(iItem->first);

      if ((SIGSRDCENUM_SKIP_USER_DEFINED & flags) && !bStd)
         continue;
      if ((SIGSRDCENUM_SKIP_STD & flags) && bStd)
         continue;
      if ((SIGSRDCENUM_MODIFIED_ONLY & flags) &&
          (0 == (iItem->second.flags & IVF_MODIFIED)))
      {
         continue;
      }

      oss << iItem->first.c_str() << " ";
      bAdded = true;
   }

   if (bAdded)
      str_enum.assign(oss.str());
   else
      str_enum.empty();

   return true;
}

