/** @file px14_remote.h
	@brief	Private PX14400 client/server definitions
*/
#ifndef __px14_remote_header_defined
#define __px14_remote_header_defined

#include "sigsvc_utility_my_stub.h"
#include "sigsrv_imp.h"
#include "px14_xml.h"

#define PX14_SERVICE_NAME			"PX14400"
#define PX14_SERVICE_DESC			"Manages connections to PX14 family devices"


/// Maximum remote data transfer chunk size in samples
#define PX14_MAX_REMOTE_CHUNK_SAMPS	8192

int RemoteDeviceRequest (HPX14 hBrd, io_req_t req, 
						 void* ctxp, size_t in_bytes, size_t out_bytes);

int Remote_ConnectToService (HPX14* phDev, PX14S_REMOTE_CONNECT_CTXA* ctxp);
int Remote_DisconnectFromService (CStatePX14& state);

int rmc_RefreshLocalRegisterCache (HPX14 hBrd, int bFromHardware);

int rmc_ReadSampleRam (HPX14 hBrd, unsigned int sample_start,
					   unsigned int sample_count, px14_sample_t* bufp);

int rmc_CreateRecordingSession (HPX14 hBrd,
								PX14S_REC_SESSION_PARAMS* ctxp,
								HPX14RECORDING* handlep);
int rmc_RecSesHandleParamOnly (HPX14RECORDING hRec, const char* namep);
int rmc_AbortRecordingSession (HPX14RECORDING hRec);
int rmc_DeleteRecordingSession (HPX14RECORDING hRec);
int rmc_ArmRecordingSession (HPX14RECORDING hRec);
int rmc_GetRecordingSessionOutFlags (HPX14RECORDING hRec,
									 unsigned int* flagsp);
int rmc_GetRecordingSessionProgress (HPX14RECORDING hRec,
									 PX14S_REC_SESSION_PROG* progp,
									 unsigned flags);
int rmc_GetRecordingSnapshot (HPX14RECORDING hRec, px14_sample_t* bufp,
							  unsigned int samples,
							  unsigned int* samples_gotp,
							  unsigned int* ss_countp);

int rmc_SetPowerupDefaults (HPX14 hBrd);
int rmc_SetAdcClockSource (HPX14 hBrd, unsigned int val);

int rmc_PX14_NoParams (HPX14 hBrd, const char* method_namep);
int rmc_PX14_DRIVER_VERSION (HPX14 hBrd, PX14S_DRIVER_VER* ctxp);
int rmc_PX14_EEPROM_IO (HPX14 hBrd, PX14S_EEPROM_IO* ctxp);
int rmc_PX14_DBG_REPORT (HPX14 hBrd, PX14S_DBG_REPORT* ctxp);
int rmc_PX14_DEVICE_REG_WRITE (HPX14 hBrd, PX14S_DEV_REG_WRITE* ctxp);
int rmc_PX14_DEVICE_REG_READ (HPX14 hBrd, PX14S_DEV_REG_READ* ctxp);
int rmc_PX14_TEMP_REG_IO (HPX14 hBrd, PX14S_RAW_REG_IO* ctxp);
int rmc_PX14_US_DELAY (HPX14 hBrd, int* ctxp);
int rmc_PX14_DRIVER_STATS (HPX14 hBrd, PX14S_DRIVER_STATS* ctxp);
int rmc_PX14_MODE_SET (HPX14 hBrd, int* ctxp);
int rmc_PX14_GET_DEVICE_STATE (HPX14 hBrd, int* ctxp);
int rmc_PX14_WAIT_ACQ_OR_XFER (HPX14 hBrd, PX14S_WAIT_OP* ctxp);
int rmc_PX14_FW_VERSIONS (HPX14 hBrd, PX14S_FW_VERSIONS* ctxp);
int rmc_PX14_READ_TIMESTAMPS (HPX14 hBrd, PX14S_READ_TIMESTAMPS* ctxp);
int rmc_PX14_NEED_DCM_RESET (HPX14 hBrd, int* ctxp);
int rmc_PX14_GET_HW_CONFIG_EX (HPX14 hBrd, PX14S_HW_CONFIG_EX* ctxp);

struct _xmlDoc;

/// Server-side instance of PX14 service class
class CSigServiceClassPX14 : public ISigServiceClass
{
public:

	CSigServiceClassPX14();
	virtual ~CSigServiceClassPX14();

	// - ISigServiceClass implementation
	virtual void Release();
	virtual const char* GetServiceName();
	virtual const char* GetServiceDesc();
	virtual unsigned short GetPreferredPort();
	virtual int CreateServiceInstance (ISigServiceInstance** objpp);

	virtual int ServiceClassStartup();
};

/// Server-side instance of a PX14 service
class CServicePX14 : public ISigServiceInstance
{
public:

	static void StaticInitialize();

	// -- Construction

	CServicePX14();

	// -- Implementation

	virtual ~CServicePX14();

	// - ISigServiceInstance implementation
	virtual void Release();
	virtual int HandleRequest (SIGSVC_SOCKET s, void* reqp, int req_fmt);

	// Populate the s_MethodMap map
	static void sMethodMapInit();

protected:

	void CleanupResources();

	// Signature of a particular method implementor
	typedef int (CServicePX14::*pfnMethImp) (px14_socket_t s, xmlDocPtr docp);
	// Alias for our method implementor map
	typedef std::map<std::string, pfnMethImp> MethodMap_t;

	static pthread_once_t s_once;
	static MethodMap_t s_MethodMap;

	// Use this instead of s_MethodMap directly to ensure that we're 
	//  not modifying anything
	const MethodMap_t& GetMethodMap() const { return s_MethodMap; }

	void* GetScratchBuf() { return m_scratch_bufp; }

	bool EnsureScratchBuffer (unsigned total_bytes);
	int SendErrorResponse (px14_socket_t s, int res);
	
	// - Method implementors (rmi = remote method implementor)

	int rmi_NoParams (px14_socket_t s, xmlDocPtr docp, io_req_t req);

	typedef int (_PX14LIBCALLCONV* FuncRecSesHandleOnly_t) (HPX14RECORDING hRec);
	int rmi_RecSesHandleOnly (px14_socket_t s, xmlDocPtr docp, 
		FuncRecSesHandleOnly_t funcp, HPX14RECORDING* handlep = NULL);

	int rmi_ConnectToDevice (px14_socket_t s, xmlDocPtr docp);
	int rmi_RefreshLocalRegisterCache (px14_socket_t s, xmlDocPtr docp);
	int rmi_GetDeviceCount (px14_socket_t s, xmlDocPtr docp);
	int rmi_ReadSampleRam (px14_socket_t s, xmlDocPtr docp);

	int rmi_CreateRecordingSession (px14_socket_t s, xmlDocPtr docp);
	int rmi_AbortRecordingSession (px14_socket_t s, xmlDocPtr docp);
	int rmi_DeleteRecordingSession (px14_socket_t s, xmlDocPtr docp);
	int rmi_ArmRecordingSession (px14_socket_t s, xmlDocPtr docp);
	int rmi_GetRecordingSessionOutFlags (px14_socket_t s, xmlDocPtr docp);
	int rmi_GetRecordingSessionProgress (px14_socket_t s, xmlDocPtr docp);
	int rmi_GetRecordingSnapshot (px14_socket_t s, xmlDocPtr docp);

	int rmi_PX14_DRIVER_VERSION (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_EEPROM_IO (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_DBG_REPORT (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_DEVICE_REG_WRITE (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_DEVICE_REG_READ (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_RESET_DCMS (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_HWCFG_REFRESH (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_TEMP_REG_IO (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_US_DELAY (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_DRIVER_STATS (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_MODE_SET (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_GET_DEVICE_STATE (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_WAIT_ACQ_OR_XFER (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_FW_VERSIONS (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_READ_TIMESTAMPS (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_NEED_DCM_RESET (px14_socket_t s, xmlDocPtr docp);
	int rmi_PX14_GET_HW_CONFIG_EX (px14_socket_t s, xmlDocPtr docp);
	int rmi_SPUD (px14_socket_t s, xmlDocPtr docp);
	int rmi_SetAdcClockSource (px14_socket_t s, xmlDocPtr docp);

	// -- Public members

	typedef std::set<HPX14RECORDING> RecSet;

	HPX14		m_hBrd;
	RecSet		m_recSet;

	void*		m_scratch_bufp;
	unsigned	m_scratch_bytes;
};

class CAutoFreeServiceResponse
{
public:

	CAutoFreeServiceResponse() : m_docp(NULL)
		{}

	virtual ~CAutoFreeServiceResponse()
		{ clean_up(); }

	//struct _xmlDoc* get() 
	//	{ return m_docp; }

	//struct _xmlDoc* release()
	//{
	//	struct _xmlDoc* ret = m_docp;
	//	m_docp = NULL;
	//	return ret;
	//}

	operator struct _xmlDoc*() { return m_docp; }
	operator const struct _xmlDoc*() const { return m_docp; }
	operator void**() { return &m_voidp; }

protected:

	void clean_up()
	{
		if (m_docp)
		{
			SIGASSERT_NULL_OR_POINTER(m_docp, char);
			FreeServiceResponsePX14(m_docp);
			m_docp = NULL;
		}
	}

private:

	// Using union here to avoid type-punning warnings on Linux
	union {
		struct _xmlDoc*	m_docp;
		void*			m_voidp;
	};
};

class CAutoDisconnectPX14
{
	HPX14	m_hBrd;

public:

	CAutoDisconnectPX14 (HPX14 hBrd) : m_hBrd(hBrd) 
	{}

	virtual ~CAutoDisconnectPX14()
	{
		if (m_hBrd != PX14_INVALID_HANDLE)
			DisconnectFromDevicePX14(m_hBrd);
	}

	HPX14 get() 
		{ return m_hBrd; }

	HPX14 detach()
	{
		HPX14 ret = m_hBrd;
		m_hBrd = PX14_INVALID_HANDLE;
		return ret;
	}
};

// Small wrapper around PX14S_FILE_WRITE_PARAMS to include string storage
class CFileParamsCnt
{
public:

	CFileParamsCnt()
	{
		memset (&m_fwp, 0, sizeof(PX14S_FILE_WRITE_PARAMS));
		m_fwp.struct_size = sizeof(PX14S_FILE_WRITE_PARAMS);
	}

	~CFileParamsCnt()
	{
	}

	void AssignStrings()
	{
		m_fwp.pathname  = m_strPath1.empty() ? NULL : m_strPath1.c_str();
		m_fwp.pathname2 = m_strPath2.empty() ? NULL : m_strPath2.c_str();
		m_fwp.operator_notes =
			m_strOpNotes.empty() ? NULL : m_strOpNotes.c_str();
		m_fwp.ts_filenamep =
			m_strTsFile.empty() ? NULL : m_strTsFile.c_str();
	}

	PX14S_FILE_WRITE_PARAMS	m_fwp;
	std::string				m_strPath1;
	std::string				m_strPath2;
	std::string				m_strOpNotes;
	std::string				m_strTsFile;
};

#endif	// __px14_remote_header_defined


