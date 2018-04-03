/** @file	px14_xml.h
	@brief	PX14 XML definitions
*/
#ifndef px14_xml_header_defined_20070402
#define px14_xml_header_defined_20070402

#include <libxml/parser.h>
#include <libxml/xmlsave.h>
#include <libxml/xpath.h>

struct MySetUintCtxPair
{
	const char*			_attrib;
	px14lib_set_func_t	_funcp;
};

struct MySetDblCtxPair
{
	const char*			_attrib;
	px14lib_setd_func_t	_funcp;
};

#endif	// px14_xml_header_defined_20070402

