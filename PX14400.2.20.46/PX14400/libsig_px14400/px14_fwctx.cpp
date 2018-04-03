/** @file px14_fwctx.cpp

  @note
  This file is used by both the PX14 library and PX14 Firmware
  Generation Utility
  */
#include "stdafx.h"
#include <fstream>
#include "px14.h"
#include "sigsvc_utility.h"		// Defines CAutoXmlDocPtr
#include "px14_fwctx.h"

#ifndef PX14_RETURN_ON_FAIL
#define PX14_RETURN_ON_FAIL(e)	if (SIG_SUCCESS!=(e)) return e
#endif

#define MY_GET_ELEMENT_REL(d,e,m,n)			\
   my_FindAndConvertNodeTextRel(d, "./" e, m, n);
#define MY_GET_ELEMENT(d,e,m)				\
   my_FindAndConvertNodeTextRel(d, "/PX14_Firmware/" e, m, NULL);
#define MY_SET_ELEMENT(s,e,v)				\
   s << " <" e ">" << v << "</" e ">" << std::endl

// Module-local function prototypes ------------------------------------- //

static std::string MakeStrVer (unsigned ver);
static std::string MakeStrVer (const unsigned long long& ver);

#ifndef _PX14_IMPLEMENTING_DLL

// -- PX14 library implements thes already

static bool _ParseStrVerCmp (const std::string& str,
                             unsigned& a, unsigned& b,
                             unsigned& c, unsigned& d);

static std::string& _TrimString (std::string& s,
                                 bool bLeading = true,
                                 bool bTrailing = true);

int my_EscapeReservedXmlMarkupChars (std::string& s);

#endif

// CFwContextPX14::CFirmwareChunk implementation ------------------------ //

CFwContextPX14::CFirmwareChunk::CFirmwareChunk() : fwc_version(0),
   fwc_cust_enum(0), fwc_flags(PX14FWCF__DEFAULT), fwc_file_count(0)
{
}

CFwContextPX14::CFirmwareChunk::CFirmwareChunk(const CFirmwareChunk& cpy) :
   fwc_version(cpy.fwc_version), fwc_cust_enum(cpy.fwc_cust_enum),
   fwc_flags(cpy.fwc_flags), fwc_file_count(cpy.fwc_file_count),
   fwc_file_list(cpy.fwc_file_list)
{
}

CFwContextPX14::CFirmwareChunk::~CFirmwareChunk()
{
}

   CFwContextPX14::CFirmwareChunk&
CFwContextPX14::CFirmwareChunk::operator= (const CFirmwareChunk& cpy)
{
   fwc_version = cpy.fwc_version;
   fwc_cust_enum = cpy.fwc_cust_enum;
   fwc_flags = cpy.fwc_flags;
   fwc_file_count = cpy.fwc_file_count;
   fwc_file_list = cpy.fwc_file_list;

   return *this;
}

// CFwContextPX14 implementation ---------------------------------------- //

CFwContextPX14::CFwContextPX14() : fw_ver_pkg(0), pkg_cust_enum(0),
   ucd(0), fw_flags(PX14FWCTXF__DEFAULT), brd_rev(PX14BRDREV_1),
   cust_hw(0), max_hw_rev(0), min_hw_rev(0), min_sw_ver(0),
   readme_sev(PX14FWNOTESEV_NONE), aux_file_count(0)
{
}

CFwContextPX14::CFwContextPX14(const CFwContextPX14& cpy) :
   fw_ver_pkg(cpy.fw_ver_pkg), pkg_cust_enum(cpy.pkg_cust_enum),
   ucd(cpy.ucd), rel_date(cpy.rel_date),
   fw_flags(cpy.fw_flags), fw_chunk_map(cpy.fw_chunk_map),
   brd_rev(cpy.brd_rev), cust_hw(cpy.cust_hw), max_hw_rev(cpy.max_hw_rev),
   min_hw_rev(cpy.min_hw_rev), min_sw_ver(cpy.min_sw_ver),
   readme_sev(cpy.readme_sev), aux_file_count(cpy.aux_file_count),
   aux_file_list(cpy.aux_file_list), userDataLst(cpy.userDataLst)
{
}

CFwContextPX14::~CFwContextPX14()
{
}

CFwContextPX14& CFwContextPX14::operator= (const CFwContextPX14& cpy)
{
   fw_ver_pkg = cpy.fw_ver_pkg;
   pkg_cust_enum = cpy.pkg_cust_enum;
   ucd = cpy.ucd;
   rel_date = cpy.rel_date;
   fw_flags = cpy.fw_flags;

   fw_chunk_map = cpy.fw_chunk_map;

   brd_rev = cpy.brd_rev;
   cust_hw = cpy.cust_hw;
   max_hw_rev = cpy.max_hw_rev;
   min_hw_rev = cpy.min_hw_rev;
   min_sw_ver = cpy.min_sw_ver;

   readme_sev = cpy.readme_sev;

   aux_file_count = cpy.aux_file_count;
   aux_file_list = cpy.aux_file_list;

   userDataLst = cpy.userDataLst;

   return *this;
}

void CFwContextPX14::ResetState()
{
   fw_ver_pkg = 0;
   pkg_cust_enum = 0;
   ucd = 0;
   rel_date.clear();
   fw_flags = PX14FWCTXF__DEFAULT;

   fw_chunk_map.clear();

   brd_rev = PX14BRDREV_1;
   cust_hw = 0;
   max_hw_rev = 0;
   min_hw_rev = 0;
   min_sw_ver = 0;

   readme_sev = PX14FWNOTESEV_NONE;

   aux_file_count = 0;
   aux_file_list.clear();

   userDataLst.clear();
}

int CFwContextPX14::ParseUserData (void* xml_doc_ptr)
{
   xmlNodePtr nodep, curp;
   xmlDocPtr docp;

   docp = reinterpret_cast<xmlDocPtr>(xml_doc_ptr);
   SIGASSERT_POINTER(docp, xmlDoc);

   nodep = my_xmlFindSingleNamedNode(docp,
                                     BAD_CAST "/PX14_Firmware/UserDefined");
   if (NULL == nodep) return SIG_SUCCESS;		// No user data

   std::string sName, sValue;

   for (curp=nodep->children; curp; curp=curp->next)
   {
      if (curp->type != XML_ELEMENT_NODE)
         continue;

      sName.assign(reinterpret_cast<const char*>(curp->name));
      my_xmlNodeGetContent(curp, sValue);
      userDataLst.push_back(UserCtxPair(sName, sValue));
   }

   return SIG_SUCCESS;
}

int CFwContextPX14::ParseFrom (const char* ctx_pathp)
{
   xmlNodePtr rootp;
   int res, i, xml_opts;

   ResetState();

   xml_opts = XML_PARSE_NOWARNING | XML_PARSE_NOERROR;

   // Parse XML
   CAutoXmlDocPtr spDoc;
   spDoc = xmlReadFile(ctx_pathp, NULL, xml_opts);
   if (!spDoc.Valid())
      return SIG_PX14_INVALID_FW_FILE;

   // Root must be "PX14_Firmware"
   if (NULL == (rootp = xmlDocGetRootElement(spDoc)))
      return SIG_PX14_INVALID_FW_FILE;
   if (xmlStrcasecmp(rootp->name, BAD_CAST "PX14_Firmware"))
      return SIG_PX14_INVALID_FW_FILE;

   std::string s;

   // Mandatory items
   res = MY_GET_ELEMENT(spDoc, "version_pkg", s);
   if ((SIG_SUCCESS != res) || (!ParseStrVer32(s, fw_ver_pkg)))
      return res;

   // Optional items
   MY_GET_ELEMENT(spDoc, "cust_enum_pkg", pkg_cust_enum);
   MY_GET_ELEMENT(spDoc, "ucd", ucd);
   MY_GET_ELEMENT(spDoc, "rel_date", rel_date);
   MY_GET_ELEMENT(spDoc, "fw_flags", fw_flags);

   MY_GET_ELEMENT(spDoc, "req_boardrevision", brd_rev);
   MY_GET_ELEMENT(spDoc, "req_customhwenum", cust_hw);
   res = MY_GET_ELEMENT(spDoc, "req_maxhwrevision", s);
   if ((SIG_SUCCESS == res) && !ParseStrVer32(s, max_hw_rev))
      return SIG_PX14_INVALID_FW_FILE;
   res = MY_GET_ELEMENT(spDoc, "req_minhwrevision", s);
   if ((SIG_SUCCESS == res) && !ParseStrVer32(s, min_hw_rev))
      return SIG_PX14_INVALID_FW_FILE;
   res = MY_GET_ELEMENT(spDoc, "req_minswversion", s);
   if ((SIG_SUCCESS == res) && !ParseStrVer64(s, min_sw_ver))
      return SIG_PX14_INVALID_FW_FILE;

   MY_GET_ELEMENT(spDoc, "readmesev", readme_sev);
   // We're assuming readme file to be named "firmware_notes.txt"

   res = MY_GET_ELEMENT(spDoc, "auxfilecount", aux_file_count);
   if ((SIG_SUCCESS == res) && (aux_file_count > 0))
   {
      for (i=0; i<aux_file_count; i++)
      {
         std::ostringstream oss;
         oss << "/PX14_Firmware/auxfile" << i+1;

         std::string elementName(oss.str());
         res = my_FindAndConvertNodeText(spDoc, elementName.c_str(), s);
         if (SIG_SUCCESS == res)
            aux_file_list.push_back(s);
      }
   }

   // Parse user-defined context stuff
   res = ParseUserData(spDoc);
   PX14_RETURN_ON_FAIL(res);

   // Parse out firmware chunks
   std::list<xmlNodePtr> nodeList;
   res = my_xmlFindNamedNodes(spDoc,
                              "/PX14_Firmware/firmware_chunk", nodeList);
   PX14_RETURN_ON_FAIL(res);
   std::list<xmlNodePtr>::iterator iNode(nodeList.begin());
   for (; iNode!=nodeList.end(); iNode++)
   {
      res = ParseFirmwareChunk((void*)(xmlDocPtr)spDoc, *iNode);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

int CFwContextPX14::ParseFirmwareChunk (void* xml_doc_ptr,
                                        void* xml_node_ptr)
{
   int res, fwc_id, i;
   xmlNodePtr nodep;
   xmlChar* idTextp;
   xmlDocPtr docp;
   bool bRes;

   nodep = reinterpret_cast<xmlNodePtr>(xml_node_ptr);
   docp = reinterpret_cast<xmlDocPtr>(xml_doc_ptr);

   // Must have an id parameter; defines firmware chunk type
   // Parse out firmware chunk ID (PX14FWCHUNKID_*)
   idTextp = xmlGetProp(nodep, BAD_CAST "id");
   if (NULL == idTextp)
      return SIG_PX14_INVALID_FW_FILE;
   bRes = my_ConvertFromString(
                               reinterpret_cast<const char*>(idTextp), fwc_id);
   xmlFree(idTextp);
   if (!bRes)
      return SIG_PX14_INVALID_FW_FILE;

   CFwContextPX14::CFirmwareChunk fwc;
   std::string s;

   // -- Mandatory items

   // Firmware chunk version
   res = MY_GET_ELEMENT_REL(docp, "fwc_version", s, nodep);
   if ((SIG_SUCCESS != res) || !ParseStrVer32(s, fwc.fwc_version))
      return SIG_PX14_INVALID_FW_FILE;
   // Firmware file count for this firmware chunk
   res = MY_GET_ELEMENT_REL(docp, "fwc_file_count",
                            fwc.fwc_file_count, nodep);
   if (SIG_SUCCESS != res)
      return SIG_PX14_INVALID_FW_FILE;
   // Firmware file names
   for (i=0; i<fwc.fwc_file_count; i++)
   {
      std::ostringstream oss;
      oss << "fwc_file" << i+1;

      std::string elementName(oss.str());
      res = my_FindAndConvertNodeTextRel(docp,
                                         elementName.c_str(), s, nodep);
      if (SIG_SUCCESS != res)
         return SIG_PX14_INVALID_FW_FILE;
      fwc.fwc_file_list.push_back(s);
   }

   // -- Optional items
   MY_GET_ELEMENT_REL(docp, "fwc_flags", fwc.fwc_flags, nodep);
   MY_GET_ELEMENT_REL(docp, "fwc_cust_enum", fwc.fwc_cust_enum, nodep);

   fw_chunk_map[fwc_id] = fwc;
   return SIG_SUCCESS;
}

int CFwContextPX14::WriteTo (const char* out_pathp)
{
   using std::endl;

   int i, res;

   std::ofstream out(out_pathp);
   if (!out)
      return SIG_PX14_DEST_FILE_OPEN_FAILED;

   std::string s;

   out << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
      << endl;
   out << "<PX14_Firmware>" << endl;
   {
      out << "<!--Firmware package information-->" << endl;
      MY_SET_ELEMENT(out, "version_pkg", MakeStrVer(fw_ver_pkg).c_str());
      MY_SET_ELEMENT(out, "cust_enum_pkg", pkg_cust_enum);
      MY_SET_ELEMENT(out, "ucd", ucd);
      if (!rel_date.empty())
         MY_SET_ELEMENT(out, "rel_date", rel_date.c_str());
      MY_SET_ELEMENT(out, "fw_flags", fw_flags);

      out << endl << "<!-- Firmware requirements -->" << endl;
      MY_SET_ELEMENT(out, "req_boardrevision", brd_rev);
      MY_SET_ELEMENT(out, "req_customhwenum", cust_hw);
      MY_SET_ELEMENT(out, "req_maxhwrevision",
                     MakeStrVer(max_hw_rev).c_str());
      MY_SET_ELEMENT(out, "req_minhwrevision",
                     MakeStrVer(min_hw_rev).c_str());
      MY_SET_ELEMENT(out, "req_minswversion",
                     MakeStrVer(min_sw_ver).c_str());

      out << endl << "<!-- Firmware notes -->" << endl;
      MY_SET_ELEMENT(out, "readmesev", readme_sev);

      out << endl << "<!-- Auxiliary file info -->" << endl;
      MY_SET_ELEMENT(out, "auxfilecount", aux_file_count);
      FileList::iterator iFile(aux_file_list.begin());
      for (i=1; iFile!=aux_file_list.end(); iFile++,i++)
      {
         my_EscapeReservedXmlMarkupChars(s.assign(*iFile));

         out << "  <auxfile" << i << ">";
         out << s.c_str();
         out << "</auxfile" << i << ">" << endl;
      }

      if (!userDataLst.empty())
      {
         res = DumpUserData(out);
         PX14_RETURN_ON_FAIL(res);
      }

      if (!fw_chunk_map.empty())
      {
         res = DumpFwChunks(out);
         PX14_RETURN_ON_FAIL(res);
      }
   }
   out << endl << "</PX14_Firmware>" << endl;

   return SIG_SUCCESS;
}

int CFwContextPX14::DumpUserData (std::ostream& out)
{
   using std::endl;

   std::string s;

   out << endl << "<!-- User defined items -->" << endl;
   out << " <UserDefined>" << endl;
   {
      UserDataList::const_iterator iData(userDataLst.begin());
      for (; iData!=userDataLst.end(); iData++)
      {
         s.assign(iData->second);
         my_EscapeReservedXmlMarkupChars(s);

         out << "    <" << iData->first.c_str() << ">";
         out << s.c_str();
         out << "  </" << iData->first.c_str() << ">" << endl;
      }
   }
   out << "</UserDefined>" << endl;

   return SIG_SUCCESS;
}

int CFwContextPX14::DumpFwChunks (std::ostream& out)
{
   using std::endl;

   int i;

   std::string s;

   out << endl << "<!-- Per-firmware chunk information -->" << endl;

   FwChunkMap::iterator iChunk(fw_chunk_map.begin());
   for (; iChunk!=fw_chunk_map.end(); iChunk++)
   {
      CFwContextPX14::CFirmwareChunk& fwc = iChunk->second;

      out << endl << "<!-- Firmware chunk: " << iChunk->first << " = ";
      switch(iChunk->first)
      {
         case PX14FWCHUNKID_SYS_V5LX50T_1:
            out << "SysV5LX50_1 (xcf32p)"; break;
         case PX14FWCHUNKID_SYS_V5LX50T_2:
            out << "SysV5LX50_2 (xcf16p)"; break;
         case PX14FWCHUNKID_SAB_V5SX50T_1:
            out << "SabV5SX50_1 (xcf32p)"; break;
         case PX14FWCHUNKID_SAB_V5SX50T_2:
            out << "SabV5SX50_2 (xcf16p)"; break;
         case PX14FWCHUNKID_SAB_V5SX95T_1:
            out << "SabV5SX95T_1 (xcf32p)"; break;
         case PX14FWCHUNKID_SAB_V5SX95T_2:
            out << "SabV5SX95T_2 (xcf16p)"; break;
         default:
            out << "(Unknown firmware chunk type)";
      }
      out << "-->" << endl;
      out << " <firmware_chunk id=\"" << iChunk->first << "\">" << endl;
      {
         out << "  <fwc_version>";
         out << MakeStrVer(iChunk->second.fwc_version).c_str();
         out << "</fwc_version>" << endl;

         out << "  <fwc_cust_enum>";
         out << iChunk->second.fwc_cust_enum;
         out << "</fwc_cust_enum>" << endl;

         out << "  <fwc_flags>";
         out << iChunk->second.fwc_flags;
         out << "</fwc_flags>" << endl;

         if (!fwc.fwc_file_count)
         {
            fwc.fwc_file_count =
               static_cast<int>(fwc.fwc_file_list.size());
         }

         out << "  <fwc_file_count>";
         out << iChunk->second.fwc_file_count;
         out << "</fwc_file_count>" << endl;

         FileList::const_iterator iFile(fwc.fwc_file_list.begin());
         for (i=1; iFile!=fwc.fwc_file_list.end(); iFile++,i++)
         {
            out << "   <fwc_file" << i << ">";
            my_EscapeReservedXmlMarkupChars(s.assign(*iFile));
            out << s.c_str();
            out << "</fwc_file" << i << ">" << endl;
         }
      }
      out << " </firmware_chunk>" << endl << endl;
   }

   return SIG_SUCCESS;
}

// CCustomLogicProviderPX14 implementation

CCustomLogicProviderPX14::CCustomLogicProviderPX14() : m_custFwEnum(0)
{
}

CCustomLogicProviderPX14::CCustomLogicProviderPX14(const CCustomLogicProviderPX14& cpy) :
   m_guidStr(cpy.m_guidStr), m_clientName(cpy.m_clientName),
   m_shortName(cpy.m_shortName), m_custFwEnum(cpy.m_custFwEnum)
{
}

CCustomLogicProviderPX14& CCustomLogicProviderPX14::operator= (const CCustomLogicProviderPX14& cpy)
{
   m_guidStr = cpy.m_guidStr;
   m_clientName = cpy.m_clientName;
   m_shortName = cpy.m_shortName;
   m_custFwEnum = cpy.m_custFwEnum;
   return *this;
}

CCustomLogicProviderPX14::~CCustomLogicProviderPX14()
{
}

void CCustomLogicProviderPX14::ResetState()
{
   m_guidStr.clear();
   m_clientName.clear();
   m_shortName.clear();
   m_custFwEnum = 0;
}

int CCustomLogicProviderPX14::ParseFrom (const char* pathnamep)
{
   xmlNodePtr rootp;
   int res;

   // Parse XML
   CAutoXmlDocPtr spDoc;
   spDoc = xmlReadFile(pathnamep, NULL,
                       XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
   if (!spDoc.Valid())
      return SIG_PX14_XML_MALFORMED;

   // Root must be "SigFpgaDev_PX14400"
   if (NULL == (rootp = xmlDocGetRootElement(spDoc)))
      return SIG_PX14_XML_INVALID;
   if (xmlStrcasecmp(rootp->name, BAD_CAST "SigFpgaDev_PX14400"))
      return SIG_PX14_XML_INVALID;

   ResetState();

   // Mandatory items

   res = MY_GET_ELEMENT_REL(spDoc, "GUID", m_guidStr, rootp);
   PX14_RETURN_ON_FAIL(res);
   res = MY_GET_ELEMENT_REL(spDoc, "CustomFwEnum", m_custFwEnum, rootp);
   PX14_RETURN_ON_FAIL(res);

   // Optional items
   MY_GET_ELEMENT_REL(spDoc, "ClientName", m_clientName, rootp);
   MY_GET_ELEMENT_REL(spDoc, "ShortName", m_shortName, rootp);

   return SIG_SUCCESS;
}

// Module private function implementation  ------------------------------ //

std::string MakeStrVer (unsigned ver)
{
   std::ostringstream oss;
   oss << ((ver >> 24) & 0xFF) << "."
      << ((ver >> 16) & 0xFF) << "."
      << ((ver >>  8) & 0xFF) << "."
      << ((ver >>  0) & 0xFF);
   return oss.str();
}

std::string MakeStrVer (const unsigned long long& ver)
{
   std::ostringstream oss;
   oss << ((ver >> 48) & 0xFFFF) << "."
      << ((ver >> 32) & 0xFFFF) << "."
      << ((ver >> 16) & 0xFFFF) << "."
      << ((ver >>  0) & 0xFFFF);
   return oss.str();
}

#ifndef _PX14_IMPLEMENTING_DLL

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

std::string& _TrimString (std::string& s, bool bLeading, bool bTrailing)
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

bool _ParseStrVerCmp (const std::string& str,
                      unsigned& a, unsigned& b,
                      unsigned& c, unsigned& d)
{
   std::string::size_type idx, pos;
   unsigned int vc[4] = {0,0,0,0};
   char* endp;
   int i;

   std::string ss, s(str);
   _TrimString(s);

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

// Already defined in sigsvc_utility.cpp
//int my_EscapeReservedXmlMarkupChars (std::string& s)
//{
//	// First char is char to replace, rest of string is replacement
//	static const char* reps[] =
//	{
//		"&&amp;",			// Do this first!
//		"<&lt;",
//		">&gt;",
//		"'&apos;",
//		"\x92&apos;",		// Fancy-pants apostraphe
//		"\"&quot;"
//	};
//	static const int nreps = sizeof(reps) / sizeof(reps[0]);
//
//	std::string::size_type szPos;
//	size_t net_added;
//	int i, nReps;
//
//	nReps = 0;
//
//	for (i=0; i<nreps; i++)
//	{
//		// Net number of chars added for this rep
//		net_added = strlen(reps[i]) - 1;
//
//		szPos = 0;
//		while (std::string::npos != (szPos = s.find(reps[i][0], szPos)))
//		{
//			s.replace(szPos, 1, &(reps[i][1]));
//			szPos += net_added;
//			nReps++;
//		}
//	}
//
//	return nReps;
//}

#endif

