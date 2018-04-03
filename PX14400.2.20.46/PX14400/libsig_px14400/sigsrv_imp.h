/** @file	sigsrv_imp.h
	@brief	API and definitions for service implementors
*/
#ifndef sigsrv_hdlr_header_defined
#define sigsrv_hdlr_header_defined

#ifndef _WIN32
#define APIENTRY
#endif

// -- Service call request format types (SIGSRVREQFMT_*)
/// Request data is a pointer to a xmlDoc object; libxml2 DOM tree
#define SIGSRVREQFMT_LIBXML2_DOCP			0
#define SIGSRVREQFMT__COUNT					1

/// Represents a particular instance (connection) to a service
struct ISigServiceInstance
{
	/// Release object to delete itself
	virtual void Release() = 0;

	/** @brief Handle a request

		The SigService framework will invoke this routine to actually 
		fulfill a client request.

		@param s
			The socket on which the client is connected
		@param reqp
			A pointer to the request data. Interpretation of this is 
			dependent on the value of the @req_fmt parameter
		@param req_fmt
			One of the SIGSRVREQFMT_* values. Defines how the request data
			should be interpreted.

		@return This method should return the following:
			<0 : Error, request contained bad or unexpected data
			 0 : Request not handled
			>0 : The service implementation has handled all response
			     generation and delivery
	*/
	virtual int HandleRequest (SIGSVC_SOCKET s, void* reqp, int req_fmt) = 0;
};

/// Represents a particular service
struct ISigServiceClass
{
	/// Release object to delete itself
	virtual void Release() = 0;

	/// Return a user-friendly name for the service
	virtual const char* GetServiceName() = 0;
	/// Return a brief description of the service
	virtual const char* GetServiceDesc() = 0;

	/// Return the preferred port number for this service
	virtual unsigned short GetPreferredPort() = 0;

	/// Create a new service instance
	virtual int CreateServiceInstance (ISigServiceInstance** objpp) = 0;
};

/// Name of service count function
#define SIGSRV_IMP_SVC_COUNT_FNAME			"GetSigServiceCount"
/// Signature of service count function
typedef int (APIENTRY * GetSigServiceCount_t)();

/// Name of service class creator function
#define SIGSRV_IMP_CREATECLASS_FNAME		"CreateSigServiceClass"
/// Signature of service class creator function
typedef int (APIENTRY * CreateSigServiceClass_t) (int idx, ISigServiceClass** classpp);

/// Name of service class creator function (CDECL)
#define SIGSRV_IMP_CREATECLASS_FNAMEC		"CreateSigServiceClassC"
/// Signature of service class creator function
typedef int (* CreateSigServiceClassC_t) (int idx, ISigServiceClass** classpp);

#endif // sigsrv_hdlr_header_defined

