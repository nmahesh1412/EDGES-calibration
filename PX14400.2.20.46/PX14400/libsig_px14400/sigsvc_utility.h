/** @file	sigsvc_utility.h
	@brief	Global miscellaneous client/server routines

	Interface to a module that implements some miscellaneous routines used
	by both clients and servers in the SigServ framework

	Usage of these routines assumes that the libxml2 library is present

	If the SIGSVCPP_NO_SOCKET_FUNCTIONS symbol is defined then all code
	involving socket IO will not be built.

	If the SIGSVCPP_NO_REMOTE_FUNCTIONS symbol is defined then all code
	involving SIGSVC client/server will not be built.
*/
#ifndef sigsvc_utility_header_defined
#define sigsvc_utility_header_defined

#include "sigsvc_utility_my_stub.h"		// Application-specific defines

#include <sstream>
#include <string>

#ifndef SIGSVCPP_NO_XML_ROUTINES

#include <libxml/parser.h>
#include <libxml/xpath.h>

// Return the node of item with the given (XPath) path
xmlNodePtr my_xmlFindSingleNamedNode (xmlDocPtr docp,
									  const xmlChar* pathp,
									  xmlNodePtr pRelativeTo = NULL);

// Obtain all nodes that match (XPath) path
int my_xmlFindNamedNodes (xmlDocPtr docp,
						  const char* pathp,
						  std::list<xmlNodePtr>& nodeList);

// Get node's text content (includes children)
bool my_xmlNodeGetContent (xmlNodePtr nodep, std::string& text);

// Obtain the text of a node's attribute; returns true if it exists
bool my_xmlGetProp (xmlNodePtr nodep, const char* prop_namep, 
                    std::string& text);

// Replace all occurrences of reserved XML markup chars with escaped equiv
int my_EscapeReservedXmlMarkupChars (std::string& s);

#endif	// !defined SIGSVCPP_NO_XML_ROUTINES

#ifndef SIGSVCPP_NO_SOCKET_FUNCTIONS

// Send data to socket, with initial 32-bit byte count sent first
int my_SendLengthPrefixedData (SIGSVC_SOCKET s, const void* datap, 
                               unsigned int bytes);

// Wraps recv; will receive all data or error
int my_socket_recv (SIGSVC_SOCKET s, char* buf, int len, int flags);
// Wraps send; will send all data or error
int my_socket_send (SIGSVC_SOCKET s, const char* buf, int len, int flags);

// Read and parse a server response
int my_ReadResponse (SIGSVC_SOCKET s,
					 char* bufp,
					 int buf_bytes,
					 xmlDocPtr* docpp);

// Send a canned response to client
int my_SendCannedResponse (SIGSVC_SOCKET s, 
						   bool bSuccess, 
						   const char* err_textp = NULL);

#endif	// !defined SIGSVCPP_NO_SOCKET_FUNCTIONS

#ifndef SIGSVCPP_NO_REMOTE_FUNCTIONS

/// Alias for a list of <paramName, paramVal> items
typedef std::list<std::pair<std::string,std::string> > MethodReqParamList;

int my_GenerateMethodRequest (const char* methodp,
                              const MethodReqParamList* paramsp,
                              std::string& strReq);

int my_GenerateMethodRequestOneParam (const char* methodp,
                                      const char* param_namep,
                                      const char* param_valp,
                                      std::string& strReq);

int my_GenerateConnectRequest (const char* serv_namep,
                               void** bufpp, 
                               int* buf_bytesp = NULL,
                               const char* app_namep = NULL,
                               const char* sub_servicesp = NULL,
                               const char* encodingp = "UTF-8");

// Quickly verify a response without knowing too much
int my_VerifyGenericResponse(xmlDocPtr docp, 
							 std::string* err_strp = NULL);

int my_ValidateResponse(xmlDocPtr docp, 
						std::string* res_strp = NULL);

// Generate a canned response
int my_GenerateCannedResponse (std::string& sr, 
							   bool bSuccess,
							   const char* err_textp = NULL);

#endif	// !defined SIGSVCPP_NO_REMOTE_FUNCTIONS

#ifndef SIGSVCPP_NO_XML_ROUTINES

/// Quick and dirty class to auto-free a xmlDocPtr object.
class CAutoXmlDocPtr
{
	xmlDocPtr	m_docp;

	public:

	CAutoXmlDocPtr (xmlDocPtr docp = NULL) : m_docp(docp) {}
	~CAutoXmlDocPtr() { Destruct(); }

	bool Valid() { return NULL != m_docp; }

	void Destruct() 
	{
		if (m_docp) {
			xmlFreeDoc(m_docp);
			m_docp = NULL;
		}
	}

	CAutoXmlDocPtr& operator= (const xmlDocPtr& p) 
	{
		Destruct(); m_docp = p;
		return *this;
	}

	xmlDocPtr detach()
	{
		xmlDocPtr ret = m_docp;
		m_docp = NULL;
		return ret;
	}

	operator xmlDocPtr() { return m_docp; }
	xmlDocPtr operator->() { return m_docp; }
};

template <typename T>
bool my_ConvertFromString (const char* textp, T& t)
{
	if (NULL == textp) return false;
	std::istringstream iss(textp);
	return (iss >> t) != NULL;
}

template<>
bool my_ConvertFromString (const char* textp, std::string& t);

/// Return a string representation of the given integeral type
template <typename T>
std::string my_ConvertToString(const T& val)
{
	// Optimize for most common value
	if (!val) return std::string("0");

	std::ostringstream oss;
	oss << val;
	return oss.str();
}

template <>
std::string my_ConvertToString<std::string> (const std::string& val);

template <typename T>
bool my_xmlNodeTextConvert (xmlNodePtr nodep, T& t)
{
	std::string node_text;
	if (!my_xmlNodeGetContent(nodep, node_text))
		return false;

	if (0 != node_text.compare("0"))
	{
		std::istringstream iss(node_text);
		return (iss >> t) ? true : false;
	}

	t = 0;
	return true;
}

template <typename T>
int my_FindAndConvertNodeTextRel (xmlDocPtr docp,
								  const char* node_path,
								  T& t,
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

	if (!my_xmlNodeTextConvert(nodep, t))
		return SIG_XXX_INVALID_CLIENT_REQUEST;

	return SIG_SUCCESS;
}

template <typename T>
int my_FindAndConvertNodeText (xmlDocPtr docp,
							   const char* node_path,
							   T& t)
{
	return my_FindAndConvertNodeTextRel(docp, node_path, t, NULL);
}

template <>
int my_FindAndConvertNodeText<std::string> (xmlDocPtr docp, 
											const char* node_path, 
											std::string& t);

template <>
int my_FindAndConvertNodeTextRel<std::string> (xmlDocPtr docp, 
											   const char* node_path, 
											   std::string& t,
											   xmlNodePtr pRelativeTo);

template <typename T>
bool my_xmlGetNodePropVal (xmlNode* nodep, const char* attrib_name, T& val)
{
	xmlChar* prop_valp;
	bool bRes;

	prop_valp = xmlGetProp(nodep, BAD_CAST attrib_name);
	if (NULL == prop_valp)
		return false;

	bRes = my_ConvertFromString(reinterpret_cast<const char*>(prop_valp), val);
	xmlFree(prop_valp);

	return bRes;
}

#endif		// !defined SIGSVCPP_NO_XML_ROUTINES

#ifndef SIGSVCPP_NO_REMOTE_FUNCTIONS

// Obtain text of named Parameter item and convert it to type T
template <typename T>
int my_GetMethodReqParamVal (xmlDocPtr docp, const char* paramp, T& t)
{
	SIGASSERT_POINTER (paramp, char);

	std::string xpath("/SigServiceRequest/Call/Parameters/");
	if (paramp)
		xpath.append(paramp);

	return my_FindAndConvertNodeText(docp, xpath.c_str(), t);
}

// Obtain text of named ReturnData item and convert it to type T
template <typename T>
int my_GetMethodRspReturnVal (xmlDocPtr docp, const char* namep, T& t)
{
	int res;

	SIGASSERT_POINTER (namep, char);

	std::string xpath("/SigServiceResponse/ReturnData/");
	if (namep)
		xpath.append(namep);

	res = my_FindAndConvertNodeText(docp, xpath.c_str(), t);
	if (res == SIG_XXX_INVALID_CLIENT_REQUEST)
		res = SIG_XXX_INVALID_SERVER_RESPONSE;

	return res;
}

// Obtain text of named Parameter item 
int my_GetMethodReqParam (xmlDocPtr docp,
						  const char* paramp,
						  std::string& t);
// Obtain text of named ReturnData item
int my_GetMethodRspReturnValStr (xmlDocPtr docp,
								 const char* namep,
								 std::string& t);

#endif	// !defined SIGSVCPP_NO_REMOTE_FUNCTIONS

#endif // sigsvc_utility_header_defined


