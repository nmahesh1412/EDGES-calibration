/** @file	px14_remote_macros.h
	@brief	A few macros used in remote implementation
*/
#ifndef __px14_remote_macros_header_defined
#define __px14_remote_macros_header_defined

/// Obtain required, named method request parameter and copy value to v
#define MI_GET_PARM_REQ(n,v)						\
	do {											\
		res = my_GetMethodReqParamVal(docp, n, v);	\
		PX14_RETURN_ON_FAIL(res);					\
	} while (0)

/// Obtain optional, named method request parameter and copy value to v
#define MI_GET_PARM_OPT(n,v)						\
	do {											\
		res = my_GetMethodReqParamVal(docp, n, v);	\
	} while (0)

#define MI_RESPONSE_START(oss)						\
	oss << "<SigServiceResponse><ReturnData>";		\
	{
#define MI_REPONSE_END(oss)							\
	oss << "</ReturnData></SigServiceResponse>";	\
	}
#define MI_RESPONSE_ENTRY(oss,n,v)					\
	oss << "<" n ">" << v << "</" n ">";

/// Get mandatory named item value from response data; fails if not there
#define MC_GET_RETVAL_REQ(n,v)						\
	do {											\
	res = my_GetMethodRspReturnVal(af_docp, n, v);	\
	PX14_RETURN_ON_FAIL(res);						\
	} while (0)

/// Get optional named item value from response data
#define MC_GET_RETVAL_OPT(n,v)						\
	do {											\
	res = my_GetMethodRspReturnVal(af_docp, n, v);	\
	} while (0)

/// Add MethodReqParamList item to list
#define MC_MRPL_ADD(m, n, v)						\
	m.push_back(std::make_pair(n, my_ConvertToString(v)))

/// Add MethodReqParamList item to list if v is non-zero
#define MC_MRPL_ADD_NON_ZERO(m, n, v)				\
	do { if (v) MC_MRPL_ADD(m, n, v); } while (0)

#endif // __px14_remote_macros_header_defined


