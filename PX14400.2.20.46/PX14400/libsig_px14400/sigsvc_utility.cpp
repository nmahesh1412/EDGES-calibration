/** @file	sigsvc_utility.cpp
  @brief	Implementation of global miscellaneous client/server routines

  The source in this module is meant to be shared across multiple clients
  and servers that use the SIGSRV client/server framework. It contains
  implementation of various miscellaneous routines used by both clients
  and servers. The intention is to share this file at the source level
  versus the binary level (i.e. a libray). The reason for this is so we
  can use client/server specific error codes.

  This module will use generic SIG_XXX_* error codes that should be
  defined in the dependent stdafx.h file. For instance, for a PDA16
  service client, the SIG_XXX_SOCKET_ERROR would be remapped to the
  PDA16 library specific equivalent:
  */
/* For example:
#define SIG_XXX_SOCKET_ERROR			SIG_P1K_SOCKET_ERROR
*/
#include "stdafx.h"
#include "sigsvc_utility.h"

/// Macro to return minimum of two values
#define MY_MIN(a,b)			((a)<(b)?(a):(b))
/// Macro to return maximum of two values
#define MY_MAX(a,b)			((a)>(b)?(a):(b))

#ifndef SIGASSERT_POINTER
#define SIGASSERT_POINTER(a,p)			((void)a)
#endif
#ifndef SIGASSERT_NULL_OR_POINTER
#define SIGASSERT_NULL_OR_POINTER(a,p)	((void)a)
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR					-1
#endif

/// Operation successful
#define SIG_SUCCESS						0
/// Generic error (GetLastError() will usually provide more info.)
#define SIG_ERROR						-1
/// Bad param or board not ready
#define SIG_INVALIDARG					-2
/// Argument is out of valid bounds
#define SIG_OUTOFBOUNDS					-3
/// Invalid board device
#define SIG_NODEV						-4
/// Error allocating memory
#define SIG_OUTOFMEMORY					-5
/// Error allocating a utility DMA buffer
#define SIG_DMABUFALLOCFAIL				-6
/// Board with given serial # not found
#define SIG_NOSUCHBOARD					-7
/// This feature is only available on NT
#define SIG_NT_ONLY						-8
/// Invalid operation for current operating mode
#define SIG_INVALID_MODE				-9
/// Operation was cancelled by user
#define SIG_CANCELLED					-10
/// Asynchronous operation is pending
#define SIG_PENDING						1

#ifndef SIGSVCPP_NO_SOCKET_FUNCTIONS

int my_SendLengthPrefixedData (SIGSVC_SOCKET s,
                               const void* datap,
                               unsigned int bytes)
{
   unsigned int bytes_n;
   int res;

   // Send response length
   bytes_n = htonl(bytes);
   res = my_socket_send(s,
                        reinterpret_cast<const char*>(&bytes_n), 4, 0);
   if (0 != res)
      return res;

   // Send response
   return my_socket_send(s,
                         reinterpret_cast<const char*>(datap), bytes, 0);
}

/// Wraps recv; will receive all data or error
int my_socket_recv (SIGSVC_SOCKET s, char* buf, int len, int flags)
{
   int bytes_got;

   while (len)
   {
      bytes_got = recv(s, buf, len, flags);
      if (!bytes_got)
         return SIG_XXX_USER_DISCONNECTED;

      if (SOCKET_ERROR == bytes_got)
         return SIG_XXX_SOCKET_ERROR;

      buf += bytes_got;
      len -= bytes_got;
   }

   return SIG_SUCCESS;
}

/// Wraps send; will send all data or error
int my_socket_send (SIGSVC_SOCKET s,
                    const char* buf,
                    int len,
                    int flags)
{
   int bytes_sent;

   while (len)
   {
      bytes_sent = send (s, buf, len, flags);
      if (!bytes_sent)
         return SIG_XXX_USER_DISCONNECTED;

      if (SOCKET_ERROR == bytes_sent)
         return SIG_XXX_SOCKET_ERROR;

      buf += bytes_sent;
      len -= bytes_sent;
   }

   return SIG_SUCCESS;
}

int my_ReadResponse (SIGSVC_SOCKET s,
                     char* bufp,
                     int buf_bytes,
                     xmlDocPtr* docpp)
{
   int res, resp_bytes, bytes_left, bytes_got, bytes_try;
   xmlParserCtxtPtr xml_ctxp;

   SIGASSERT_POINTER(docpp, xmlDocPtr);
   if (NULL == docpp)
      return SIG_INVALIDARG;

   // Receive response length in bytes
   res = my_socket_recv(s, reinterpret_cast<char*>(&resp_bytes), 4, 0);
   if (SIG_SUCCESS != res) return res;
   resp_bytes = ntohl(resp_bytes);

   // Read request data and parse it
   bytes_left = resp_bytes;
   xml_ctxp = NULL;
   while (bytes_left)
   {
      bytes_try = MY_MIN(bytes_left, buf_bytes);
      bytes_got = recv(s, bufp, bytes_try, 0);
      if (!bytes_got)
         return SIG_XXX_SOCKET_ERROR;

      if (NULL == xml_ctxp)
      {
         xml_ctxp = xmlCreatePushParserCtxt(NULL, NULL,
                                            bufp, bytes_got, NULL);
         if (NULL == xml_ctxp)
            return SIG_XXX_XML_GENERIC;
      }
      else
      {
         res = xmlParseChunk(xml_ctxp, bufp, bytes_got, 0);
         if (res != 0)
            return SIG_XXX_XML_GENERIC;
      }

      bytes_left -= bytes_got;
   }

   xmlParseChunk(xml_ctxp, bufp, 0, 1);		// We're done parsing

   *docpp = xml_ctxp->myDoc;
   res = xml_ctxp->wellFormed;
   xmlFreeParserCtxt(xml_ctxp);

   return res ? SIG_SUCCESS : SIG_XXX_INVALID_SERVER_RESPONSE;
}

/// Send a canned response to client
int my_SendCannedResponse (SIGSVC_SOCKET s,
                           bool bSuccess,
                           const char* err_textp)
{
   int res;

   std::string sr;
   res = my_GenerateCannedResponse(sr, bSuccess, err_textp);
   if (SIG_SUCCESS != res) return res;

   return my_SendLengthPrefixedData(s, sr.c_str(),
                                    static_cast<unsigned int>(sr.length()));
}

#endif

#ifndef SIGSVCPP_NO_REMOTE_FUNCTIONS

/**

Note: this is supposed to be XML so replace '{' '}' with '<' and '>'
<pre>
{SigServiceRequest}
{Call method="ConnectToDevice"}
{Parameters}
{!--Method-specific stuff goes here--}
{BoardNum}1{/BoardNum}
{/Parameters}
{/Call}
{/SigServiceRequest}
</pre>
*/
int my_GenerateMethodRequest (const char* methodp,
                              const MethodReqParamList* paramsp,
                              std::string& strReq)
{
   SIGASSERT_NULL_OR_POINTER(methodp, char);
   SIGASSERT_NULL_OR_POINTER(paramsp, MethodReqParamList);
   if (NULL == methodp)
      methodp = "ping";

   strReq.clear();
   strReq.reserve(128);

   strReq.assign(
                 "<SigServiceRequest>"
                 "<Call method=\"");
   strReq.append(methodp);
   strReq.append("\">");

   if (paramsp)
   {
      strReq.append("<Parameters>");

      MethodReqParamList::const_iterator iParam(paramsp->begin());
      for (; iParam!=paramsp->end(); iParam++)
      {
         strReq.append(1, '<');
         strReq.append(iParam->first);
         strReq.append(1, '>');

         strReq.append(iParam->second);

         strReq.append("</");
         strReq.append(iParam->first);
         strReq.append(1, '>');
      }

      strReq.append("</Parameters>");
   }

   strReq.append(
                 "</Call>"
                 "</SigServiceRequest>");

   return SIG_SUCCESS;
}

int my_GenerateMethodRequestOneParam (const char* methodp,
                                      const char* param_namep,
                                      const char* param_valp,
                                      std::string& strReq)
{
   MethodReqParamList mrpl;
   mrpl.push_back(std::make_pair(param_namep, param_valp));
   return my_GenerateMethodRequest(methodp, &mrpl, strReq);
}

/**
  This function generates a connect request, which is XML data.

Note: this is supposed to be XML so replace '{' '}' with '<' and '>'
<pre>
{?xml version="1.0" encoding="utf-8"?}
{SigServiceRequest}
{Connect service="PDA16">
{ClientApp}PDA16 Scope Application</ClientApp}  {!-- Optional --}
{SubServices}                                   {!-- Optional --}
{SubService>ServiceName{SubService}
{/SubServices}
{/Connect}
{/SigServiceRequest}
</pre>

@param serv_namep
A pointer to a string containing the name of the service. This will
will be validated on the server end and if it does not match the
name of the service running on the port the connection will fail.
@param bufpp
A pointer to a void* that will receive the address of the library
allocated buffer that contains the request. This memory must be
freed via xmlFree
@param buf_bytesp
A pointer to an integer variable that will receive the number of
bytes allocated for *bufpp. Pass NULL if this information is not
needed
@param app_namep
An optional parameter the specified the name of the client
application. This may be NULL
@param sub_servicesp
An optional list of sub-services that will be loaded along with the
main service. The format of this string is not yet defined.
@param encodingp
The encoding that will be used for the generated XML request data.
If this parameter is NULL then UTF-8 encoding will be used
*/
int my_GenerateConnectRequest (const char* serv_namep,
                               void** bufpp,
                               int* buf_bytesp,
                               const char* app_namep,
                               const char* sub_servicesp,
                               const char* encodingp)
{
   xmlNodePtr rootp, connectp;
   xmlChar *xmlbuff;
   int buffersize;

   SIGASSERT_NULL_OR_POINTER(buf_bytesp, int);
   SIGASSERT_POINTER(bufpp, void*);
   if (NULL == bufpp)
      return SIG_INVALIDARG;
   SIGASSERT_NULL_OR_POINTER(encodingp, char);
   if (NULL == encodingp)
      encodingp = "UTF-8";

   // Create DOM document object
   CAutoXmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
   if (!doc) return SIG_XXX_XML_GENERIC;

   // Create root node
   rootp = xmlNewNode(NULL, BAD_CAST "SigServiceRequest");
   if (!rootp) return SIG_OUTOFMEMORY;
   xmlDocSetRootElement(doc, rootp);

   // Create Connect node
   connectp = xmlNewChild(rootp, NULL, BAD_CAST "Connect", NULL);
   if (!connectp) return SIG_OUTOFMEMORY;
   if (serv_namep)
      xmlNewProp(connectp, BAD_CAST "service", BAD_CAST serv_namep);

   if (app_namep)
   {
      xmlNewChild(connectp, NULL, BAD_CAST "ClientApp",
                  BAD_CAST app_namep);
   }

   if (sub_servicesp)
   {
      xmlNewChild(connectp, NULL, BAD_CAST "SubServices",
                  BAD_CAST sub_servicesp);
   }

   // Generate XML
   buffersize = 0;
   xmlbuff = NULL;
   xmlDocDumpFormatMemoryEnc(doc, &xmlbuff, &buffersize, encodingp, 1);
   if (!buffersize || !xmlbuff)
      return SIG_XXX_XML_GENERIC;

   if (buf_bytesp) *buf_bytesp = buffersize;
   *bufpp = xmlbuff;
   return SIG_SUCCESS;
}

int my_GenerateCannedResponse (std::string& sr,
                               bool bSuccess,
                               const char* err_textp)
{
   static const char* canned_success =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<SigServiceResponse error=\"false\">"
      "</SigServiceResponse>";

   if (bSuccess && !err_textp)
      sr.assign(canned_success);
   else
   {
      // We'll just manually generate the XML here

      sr.assign(
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                "<SigServiceResponse error=\"");
      sr.append(bSuccess ? "false" : "true");
      sr.append("\" >");

      if (err_textp)
      {
         std::string strErr(err_textp);
         my_EscapeReservedXmlMarkupChars(strErr);

         sr.append("<ErrorText>");
         sr.append(strErr);
         sr.append("</ErrorText>");
      }

      sr.append("</SigServiceResponse>");
   }

   return 0;
}

// Obtain text of named Parameter item
int my_GetMethodReqParam (xmlDocPtr docp,
                          const char* paramp,
                          std::string& t)
{
   xmlNodePtr nodep;

   SIGASSERT_POINTER (paramp, char);

   std::string xpath("/SigServiceRequest/Call/Parameters/");
   if (paramp)
      xpath.append(paramp);

   nodep = my_xmlFindSingleNamedNode(docp, BAD_CAST xpath.c_str());
   if (NULL == nodep)
      return SIG_XXX_INVALID_CLIENT_REQUEST;

   if (!my_xmlNodeGetContent(nodep, t))
      return SIG_XXX_INVALID_CLIENT_REQUEST;

   return SIG_SUCCESS;
}

int my_GetMethodRspReturnValStr (xmlDocPtr docp,
                                 const char* namep,
                                 std::string& t)
{
   xmlNodePtr nodep;

   SIGASSERT_POINTER (namep, char);

   std::string xpath("/SigServiceResponse/ReturnData/");
   if (namep)
      xpath.append(namep);

   nodep = my_xmlFindSingleNamedNode(docp, BAD_CAST xpath.c_str());
   if (NULL == nodep)
      return SIG_XXX_INVALID_SERVER_RESPONSE;

   if (!my_xmlNodeGetContent(nodep, t))
      return SIG_XXX_INVALID_SERVER_RESPONSE;

   return SIG_SUCCESS;
}

int my_ValidateResponse(xmlDocPtr docp, std::string* res_strp)
{
   xmlNodePtr nodep;

   SIGASSERT_NULL_OR_POINTER(res_strp, std::string);
   SIGASSERT_POINTER(docp, xmlDoc);
   if (NULL == docp)
      return SIG_INVALIDARG;

   if (res_strp)
   {
      nodep = my_xmlFindSingleNamedNode(docp,
                                        BAD_CAST "/SigServiceResponse/Result");
      my_xmlNodeGetContent(nodep, *res_strp);
   }

   return SIG_SUCCESS;
}

/// Quickly verify a response without knowing too much
int my_VerifyGenericResponse(xmlDocPtr docp, std::string* err_strp)
{
   xmlNodePtr rootp, nodep;
   bool bError, bHandled;
   xmlChar* err_valp;

   SIGASSERT_NULL_OR_POINTER(err_strp, std::string);
   SIGASSERT_POINTER(docp, xmlDoc);
   if (NULL == docp)
      return SIG_INVALIDARG;

   // Root item must be SigServiceRequest
   rootp = xmlDocGetRootElement(docp);
   if (NULL == rootp)
      return SIG_XXX_XML_MALFORMED;
   if (xmlStrcasecmp(rootp->name, BAD_CAST "SigServiceResponse"))
      return SIG_XXX_INVALID_SERVER_RESPONSE;

   // Request not handled (unknown request) if root items contains an
   //  "handled" property whose value is "false"
   err_valp = xmlGetProp(rootp, BAD_CAST "handled");
   if (err_valp)
   {
      bHandled = (0 != xmlStrcasecmp(err_valp, BAD_CAST "false"));
      xmlFree(err_valp);

      if (!bHandled)
         return SIG_XXX_UNKNOWN_REMOTE_METHOD;
   }

   // Request failed if root item (SigServiceResponse) contains an "error"
   //  propery whose value is not "false"
   if (NULL != (err_valp = xmlGetProp(rootp, BAD_CAST "error")))
   {
      bError = (0 != xmlStrcasecmp(err_valp, BAD_CAST "false"));
      xmlFree(err_valp);

      if (bError)
      {
         if (err_strp)
         {
            // Grab error text
            nodep = my_xmlFindSingleNamedNode(docp,
                                              BAD_CAST "/SigServiceResponse/ErrorText");
            my_xmlNodeGetContent(nodep, *err_strp);
         }

         return SIG_XXX_REMOTE_CALL_RETURNED_ERROR;
      }
   }

   return SIG_SUCCESS;
}

#endif	// !defined SIGSVCPP_NO_REMOTE_FUNCTIONS

#ifndef SIGSVCPP_NO_XML_ROUTINES

/** @brief Return the node of item with the given (XPath) path

  In the event that multiple nodes are present, one will be returned.
  This may or may not be the first; it's up to the libxml implementation.

*/
xmlNodePtr my_xmlFindSingleNamedNode (xmlDocPtr docp,
                                      const xmlChar* pathp,
                                      xmlNodePtr pRelativeTo)
{
   xmlXPathContextPtr ctxp;
   xmlXPathObjectPtr resp;
   xmlNodePtr nodep;

   nodep = NULL;

   if (docp && pathp && (NULL != (ctxp = xmlXPathNewContext(docp))))
   {
      ctxp->node = pRelativeTo;

      resp = xmlXPathEvalExpression(pathp, ctxp);
      if (resp)
      {
         if (!xmlXPathNodeSetIsEmpty(resp->nodesetval))
            nodep = resp->nodesetval->nodeTab[0];

         xmlXPathFreeObject(resp);
      }

      xmlXPathFreeContext(ctxp);
   }

   return nodep;
}

int my_xmlFindNamedNodes (xmlDocPtr docp,
                          const char* pathp,
                          std::list<xmlNodePtr>& nodeList)
{
   xmlXPathContextPtr ctxp;
   xmlXPathObjectPtr resp;
   int i;

   if (docp && pathp && (NULL != (ctxp = xmlXPathNewContext(docp))))
   {
      resp = xmlXPathEvalExpression(BAD_CAST pathp, ctxp);
      if (resp)
      {
         if (!xmlXPathNodeSetIsEmpty(resp->nodesetval))
         {
            for (i=0; i<resp->nodesetval->nodeNr; i++)
               nodeList.push_back(resp->nodesetval->nodeTab[i]);
         }

         xmlXPathFreeObject(resp);
      }

      xmlXPathFreeContext(ctxp);
   }

   return SIG_SUCCESS;
}

int my_EscapeReservedXmlMarkupChars (std::string& s)
{
   // First char is char to replace, rest of string is replacement
   static const char* reps[] =
   {
      "&&amp;",			// Do this first!
      "<&lt;",
      ">&gt;",
      "'&apos;",
      "\x92&apos;",		// Fancy-pants apostraphe
      "\"&quot;"
   };
   static const int nreps = sizeof(reps) / sizeof(reps[0]);

   std::string::size_type szPos;
   size_t net_added;
   int i, nReps;

   nReps = 0;

   for (i=0; i<nreps; i++)
   {
      // Net number of chars added for this rep
      net_added = strlen(reps[i]) - 1;

      szPos = 0;
      while (std::string::npos != (szPos = s.find(reps[i][0], szPos)))
      {
         s.replace(szPos, 1, &(reps[i][1]));
         szPos += net_added;
         nReps++;
      }
   }

   return nReps;
}

bool my_xmlNodeGetContent (xmlNodePtr nodep, std::string& text)
{
   xmlChar* strp;
   bool bRes;

   if (nodep && (NULL != (strp = xmlNodeGetContent(nodep))))
   {
      text.assign(reinterpret_cast<const char*>(strp));
      xmlFree(strp);
      bRes = true;
   }
   else
   {
      text.clear();
      bRes = false;
   }

   return bRes;
}

/// Obtain the text of a node's attribute; returns true if it exists
bool my_xmlGetProp (xmlNodePtr nodep,
                    const char* prop_namep,
                    std::string& text)
{
   xmlChar* valp;

   if (nodep && prop_namep &&
       (NULL != (valp = xmlGetProp(nodep, BAD_CAST prop_namep))))
   {
      text.assign(reinterpret_cast<const char*>(valp));
      xmlFree(valp);
      return true;
   }

   return false;
}

// Specialization for strings
   template <>
int my_FindAndConvertNodeText<std::string> (xmlDocPtr docp,
                                            const char* node_path,
                                            std::string& t)
{
   return my_FindAndConvertNodeTextRel(docp, node_path, t, NULL);
}

   template <>
int my_FindAndConvertNodeTextRel<std::string> (xmlDocPtr docp,
                                               const char* node_path,
                                               std::string& t,
                                               xmlNodePtr pRelativeTo)
{
   xmlNodePtr nodep;

   SIGASSERT_POINTER(node_path, char);
   SIGASSERT_POINTER(docp, xmlDoc);
   if ((NULL == docp) || (NULL == node_path))
      return SIG_INVALIDARG;

   nodep = my_xmlFindSingleNamedNode(docp,
                                     BAD_CAST node_path, pRelativeTo);
   if (NULL == nodep)
      return SIG_XXX_INVALID_CLIENT_REQUEST;

   if (!my_xmlNodeGetContent(nodep, t))
      return SIG_XXX_INVALID_CLIENT_REQUEST;

   return SIG_SUCCESS;
}

   template <>
std::string my_ConvertToString<std::string> (const std::string& val)
{
   return val;
}

   template<>
bool my_ConvertFromString (const char* textp, std::string& t)
{
   if (textp)
      t.assign(textp);
   else
      t.clear();

   return true;
}

#endif // !defined SIGSVCPP_NO_XML_ROUTINES

