/** @file	sigsvc_utility_my_stub.h

	This file defines some application-specific stuff for the sigsvf_utility
	source file.
*/
#ifndef __px14_sigsvc_utility_my_stub_header_defined
#define __px14_sigsvc_utility_my_stub_header_defined

#include "px14.h"

#ifdef _WIN32
#define SIGSVC_SOCKET						SOCKET
#endif

#ifdef _PX14PP_LINUX_PLATFORM
#define SIGSVC_SOCKET						int
#endif

// -- Define our app-specific error code values for sigsvc_utility.cpp
#define SIG_XXX_USER_DISCONNECTED			SIG_PX14_SERVER_DISCONNECTED
#define SIG_XXX_SOCKET_ERROR				SIG_PX14_SOCKET_ERROR
#define SIG_XXX_XML_GENERIC					SIG_PX14_XML_GENERIC
#define SIG_XXX_INVALID_SERVER_RESPONSE		SIG_PX14_INVALID_SERVER_RESPONSE
#define SIG_XXX_INVALID_CLIENT_REQUEST		SIG_PX14_INVALID_CLIENT_REQUEST
#define SIG_XXX_XML_MALFORMED				SIG_PX14_XML_MALFORMED
#define SIG_XXX_UNKNOWN_REMOTE_METHOD		SIG_PX14_UNKNOWN_REMOTE_METHOD
#define SIG_XXX_REMOTE_CALL_RETURNED_ERROR	SIG_PX14_REMOTE_CALL_RETURNED_ERROR

#endif	// __px14_sigsvc_utility_my_stub_header_defined

