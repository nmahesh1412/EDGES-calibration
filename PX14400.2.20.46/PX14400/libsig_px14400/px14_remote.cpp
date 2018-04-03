/** @file	px14_remote.cpp
  @brief	PX14400 server implementation
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"

#include "sigsvc_utility.h"
#include "px14_xml.h"
#include "px14_remote.h"
#include "px14_remote_macros.h"
#include "px14_ses_base.h"

//#ifndef PX14PP_NO_REMOTE_IMP

// If defined, timeouts will be ignored for all SendServiceRequestPX14 calls
//#define PX14PP_NO_REMOTE_CALL_TIMEOUTS

// Module-local function prototypes ------------------------------------ //

static int _ConnectToRemoteDeviceWork (HPX14* phDev,
                                       unsigned int brdNum,
                                       unsigned int ordNum,
                                       bool bVirtual,
                                       PX14S_REMOTE_CONNECT_CTXA* ctxp);

static int _SendDevConnectRequest (CStatePX14& state,
                                   unsigned int brdNum,
                                   unsigned int ordNum,
                                   bool bVirtual);

static int _SendServiceRequestWork (CStatePX14& state,
                                    const void* svc_reqp,
                                    int req_bytes,
                                    void** responsepp,
                                    unsigned int timeoutMs,
                                    unsigned int flags);

static void _AppendMrpl_FilwParms (MethodReqParamList& mrpl,
                                   PX14S_FILE_WRITE_PARAMS* ctxp);
static int _ExtractMiParm_FilewParms (xmlDoc* docp,
                                      CFileParamsCnt& filwp);

// SigService Implementor Implementation ------------------------------- //

/** This is an entry point for a Signatec Service Handler library and is
  used to obtain the number of Signatec Services that this library
  implements.

  The PX14400 library only implements a single service: a PX14400 device
  manager

  @retval Returns the number of services that this library provides
  */
PX14API GetSigServiceCountC()
{
   return 1;
}

/**
  This is an entry point for a Signatec Service Handler library.

  This function is used to create an instance of an object that defines
  information for a particular service.
  */
PX14API CreateSigServiceClassC (int idx, ISigServiceClass** classpp)
{
   CSigServiceClassPX14* my_classp;
   int res;

   if (idx || NULL==classpp)
      return SIG_INVALIDARG;

   try { my_classp = new CSigServiceClassPX14(); }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }
   res = my_classp->ServiceClassStartup();
   if (SIG_SUCCESS != res)
      return res;

   *classpp = my_classp;
   return SIG_SUCCESS;
}

// CSigServiceClassPX14 implementation ----------------------------------- //

CSigServiceClassPX14::CSigServiceClassPX14()
{
}

CSigServiceClassPX14::~CSigServiceClassPX14()
{
}

/// Release object to delete itself
void CSigServiceClassPX14::Release()
{
   delete this;
}

/// Return a user-friendly name for the service
const char* CSigServiceClassPX14::GetServiceName()
{
   return PX14_SERVICE_NAME;
}

/// Return a brief description of the service
const char* CSigServiceClassPX14::GetServiceDesc()
{
   return PX14_SERVICE_DESC;
}

/// Return the preferred port number for this service
unsigned short CSigServiceClassPX14::GetPreferredPort()
{
   return PX14_SERVER_PREFERRED_PORT;
}

/// Create a new service instance
int CSigServiceClassPX14::CreateServiceInstance (ISigServiceInstance** objpp)
{
   if (NULL == objpp)
      return SIG_INVALIDARG;

   *objpp = new CServicePX14();
   if (NULL == *objpp)
      return SIG_OUTOFMEMORY;

   return SIG_SUCCESS;
}

int CSigServiceClassPX14::ServiceClassStartup()
{
   // Called when service class is started and before any service instances
   //  have been created.

   // Initialize service's static data (implementor map)
   try { CServicePX14::StaticInitialize(); }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }

   return SIG_SUCCESS;
}

// CServicePX14 implementation ------------------------------- //

pthread_once_t CServicePX14::s_once = PTHREAD_ONCE_INIT;
CServicePX14::MethodMap_t CServicePX14::s_MethodMap;

CServicePX14::CServicePX14() :
   m_hBrd(PX14_INVALID_HANDLE),
   m_scratch_bufp(NULL), m_scratch_bytes(0)
{
}

CServicePX14::~CServicePX14()
{
   CleanupResources();
}

void CServicePX14::Release()
{
   delete this;
}

void CServicePX14::CleanupResources()
{
   // Free any outstanding recordings
   RecSet::iterator iRec(m_recSet.begin());
   for (; iRec!=m_recSet.end(); iRec++)
      DeleteRecordingSessionPX14(*iRec);
   m_recSet.clear();

   if (PX14_INVALID_HANDLE != m_hBrd)
      DisconnectFromDevicePX14(m_hBrd);

   SIGASSERT_NULL_OR_POINTER(m_scratch_bufp, char);
   my_free(m_scratch_bufp);
}

void CServicePX14::StaticInitialize()
{//static

   // Call this method once and only once
   pthread_once(&s_once, CServicePX14::sMethodMapInit);
}

/**
  This method should be invoked once, and *only* once. (This should be
  mechanized with pthread_once)

  This method will populate a map that we can use to map requests to the
  functions that implement those requests. This map is static so there's
  only a single instance among all CServicePX14 instances. Since this is a
  write once, read many times situation we do not synchronoze access to
  this map.
  */
void CServicePX14::sMethodMapInit()
{
   s_MethodMap.clear();

   s_MethodMap["PX14_CONNECT"] =
      &CServicePX14::rmi_ConnectToDevice;
   s_MethodMap["PX14_REG_CACHE"] =
      &CServicePX14::rmi_RefreshLocalRegisterCache;
   s_MethodMap["PX14_DEVCNT"] =
      &CServicePX14::rmi_GetDeviceCount;
   s_MethodMap["PX14_RAM_R"] =
      &CServicePX14::rmi_ReadSampleRam;
   s_MethodMap["PX14_REC_NEW"] =
      &CServicePX14::rmi_CreateRecordingSession;
   s_MethodMap["PX14_REC_ABORT"] =
      &CServicePX14::rmi_AbortRecordingSession;
   s_MethodMap["PX14_REC_DEL"] =
      &CServicePX14::rmi_DeleteRecordingSession;
   s_MethodMap["PX14_REC_ARM"] =
      &CServicePX14::rmi_ArmRecordingSession;
   s_MethodMap["PX14_REC_OFLAGS"] =
      &CServicePX14::rmi_GetRecordingSessionOutFlags;
   s_MethodMap["PX14_REC_PROG"] =
      &CServicePX14::rmi_GetRecordingSessionProgress;
   s_MethodMap["PX14_REC_SS"] =
      &CServicePX14::rmi_GetRecordingSnapshot;

   s_MethodMap["PX14_DVER"] =
      &CServicePX14::rmi_PX14_DRIVER_VERSION;
   s_MethodMap["PX14_EEPROM_IO"] =
      &CServicePX14::rmi_PX14_EEPROM_IO;
   s_MethodMap["PX14_DBG_REPORT"] =
      &CServicePX14::rmi_PX14_DBG_REPORT;
   s_MethodMap["PX14_REG_W"] =
      &CServicePX14::rmi_PX14_DEVICE_REG_WRITE;
   s_MethodMap["PX14_REG_R"] =
      &CServicePX14::rmi_PX14_DEVICE_REG_READ;
   s_MethodMap["PX14_DCMRST"] =
      &CServicePX14::rmi_PX14_RESET_DCMS;
   s_MethodMap["PX14_TREGIO"] =
      &CServicePX14::rmi_PX14_TEMP_REG_IO;
   s_MethodMap["PX14_US_DELAY"] =
      &CServicePX14::rmi_PX14_US_DELAY;
   s_MethodMap["PX14_DSTATS"] =
      &CServicePX14::rmi_PX14_DRIVER_STATS;
   s_MethodMap["PX14_MSET"] =
      &CServicePX14::rmi_PX14_MODE_SET;
   s_MethodMap["PX14_GETSTATE"] =
      &CServicePX14::rmi_PX14_GET_DEVICE_STATE;
   s_MethodMap["PX14_WAITAX"] =
      &CServicePX14::rmi_PX14_WAIT_ACQ_OR_XFER;
   s_MethodMap["PX14_FWVER"] =
      &CServicePX14::rmi_PX14_FW_VERSIONS;
   s_MethodMap["PX14_READTS"] =
      &CServicePX14::rmi_PX14_READ_TIMESTAMPS;
   s_MethodMap["PX14_NDCMRST"] =
      &CServicePX14::rmi_PX14_NEED_DCM_RESET;
   s_MethodMap["PX14_GET_HWCFG_EX"] =
      &CServicePX14::rmi_PX14_GET_HW_CONFIG_EX;

   s_MethodMap["PX14_SPUD"] =
      &CServicePX14::rmi_SPUD;
   s_MethodMap["PX14_CLKSRC"] =
      &CServicePX14::rmi_SetAdcClockSource;
}

bool CServicePX14::EnsureScratchBuffer (unsigned total_bytes)
{
   if (m_scratch_bytes >= total_bytes)
   {
      SIGASSERT_POINTER(m_scratch_bufp, char);
      return true;
   }

   // Need to (re)allocate buffer
   if (m_scratch_bufp)
   {
      my_free(m_scratch_bufp);
      m_scratch_bufp = NULL;
      m_scratch_bytes = 0;
   }

   m_scratch_bufp = my_malloc(total_bytes);
   if (NULL == m_scratch_bufp)
      return false;

   m_scratch_bytes = total_bytes;
   return true;
}

int CServicePX14::HandleRequest (SIGSVC_SOCKET s, void* reqp, int req_fmt)
{
   xmlNodePtr nodep;
   xmlDocPtr docp;
   int res;

   const MethodMap_t& methodMap = GetMethodMap();

   // We only understand the SIGSRVREQFMT_LIBXML2_DOCP request format
   if (req_fmt != SIGSRVREQFMT_LIBXML2_DOCP)
      return SIG_INVALIDARG;
   if (NULL == (docp = reinterpret_cast<xmlDocPtr>(reqp)))
      return SIG_INVALIDARG;

   // Get the desired method's name
   std::string strMethod;
   nodep = my_xmlFindSingleNamedNode(docp,
                                     BAD_CAST "/SigServiceRequest/Call/@method");
   if (!my_xmlNodeGetContent(nodep, strMethod) || strMethod.empty())
      res = SIG_PX14_INVALID_CLIENT_REQUEST;
   else
   {
      // Look up and call method's implementation routine
      MethodMap_t::const_iterator iImp(methodMap.find(strMethod));
      if (iImp != methodMap.end())
      {
         // Call method implementor through pointer to member
         res = (this->*(iImp->second))(s, docp);
      }
      else
      {
         // We don't handle this request
         res = 0;
      }
   }

   if (res < 0)
      return SendErrorResponse(s, res);

   return res;
}

int CServicePX14::SendErrorResponse (px14_socket_t s, int res)
{
   std::string errStr;
   GetErrorTextStringPX14(res, errStr, 0, m_hBrd);
   my_SendCannedResponse(s, false, errStr.c_str());
   return 1;
}

int CServicePX14::rmi_ConnectToDevice (px14_socket_t s, xmlDocPtr docp)
{
   unsigned int brdNum, ordNum, virtVal;
   CStatePX14* statep;
   bool bVirtual;
   int res;

   // -- Marshall input

   MI_GET_PARM_REQ("BoardNum", brdNum);
   MI_GET_PARM_OPT("Virtual", virtVal);
   bVirtual = (SIG_SUCCESS == res) && virtVal;
   if (bVirtual)
      MI_GET_PARM_REQ("OrdNum", ordNum);

   // -- Execute request

   if (bVirtual)
      res = ConnectToVirtualDevicePX14(&m_hBrd, brdNum, ordNum);
   else
      res = ConnectToDevicePX14(&m_hBrd, brdNum);
   PX14_RETURN_ON_FAIL(res);
   statep = PX14_H2B(m_hBrd);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "SerialNum",    statep->m_serial_num)
      MI_RESPONSE_ENTRY(oss, "OrdinalNum",   statep->m_ord_num)
      MI_RESPONSE_ENTRY(oss, "Features",     statep->m_features)
      MI_RESPONSE_ENTRY(oss, "Ch1Imp",       (unsigned)statep->m_chanImpCh1)
      MI_RESPONSE_ENTRY(oss, "Ch2Imp",       (unsigned)statep->m_chanImpCh2)
      MI_RESPONSE_ENTRY(oss, "LibFlags",     statep->m_flags)
      MI_RESPONSE_ENTRY(oss, "SlaveDelayNs", statep->m_slave_delay_ns)
      MI_RESPONSE_ENTRY(oss, "DriverVer",    statep->m_driver_ver)
      MI_RESPONSE_ENTRY(oss, "FwVerPci",     statep->m_fw_ver_pci)
      MI_RESPONSE_ENTRY(oss, "FwVerSab",     statep->m_fw_ver_sab)
      MI_RESPONSE_ENTRY(oss, "FwVerPkg",     statep->m_fw_ver_pkg)
      MI_RESPONSE_ENTRY(oss, "FwPreRel",     statep->m_fw_pre_rel)
      MI_REPONSE_END(oss)

      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_GetDeviceCount (px14_socket_t s, xmlDocPtr docp)
{
   int res, i, device_count;
   unsigned int sn;
   HPX14 hTmp;

   // -- No input

   // -- Execute request

   device_count = GetDeviceCountPX14();

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "DeviceCount",  device_count)

      // We also return all board serial numbers
      for (i=0; i<device_count; i++)
      {
         res = ConnectToDevicePX14(&hTmp, i+1);
         if (SIG_SUCCESS == res)
         {
            GetSerialNumberPX14(hTmp, &sn);
            DisconnectFromDevicePX14(hTmp);

            oss << "<Device" << i << ">";
            oss << sn;
            oss << "</Device" << i << ">";
         }
      }

   MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_RefreshLocalRegisterCache (px14_socket_t s,
                                                 xmlDocPtr docp)
{
   int i, res, bFromHardware;
   CStatePX14* statep;

   // -- Marshall input

   MI_GET_PARM_REQ("bFromHardware", bFromHardware);

   // -- Execute request

   res = RefreshLocalRegisterCachePX14(m_hBrd, bFromHardware);
   PX14_RETURN_ON_FAIL(res);

   statep = PX14_H2B(m_hBrd);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)

      // Register counts
      MI_RESPONSE_ENTRY(oss, "DevRegCnt",    PX14_DEVICE_REG_COUNT)
      MI_RESPONSE_ENTRY(oss, "ClkRegCnt",    PX14_CLKGEN_LOGICAL_REG_CNT)
      MI_RESPONSE_ENTRY(oss, "DrvRegCnt",    PX14_DRIVER_REG_COUNT)

      // Device registers
      for (i=0; i<PX14_DEVICE_REG_COUNT; i++)
      {
         oss << "<DevReg" << i << ">";
         oss << statep->m_regDev.values[i];
         oss << "</DevReg" << i << ">";
      }

   // Clock register
   for (i=0; i<PX14_CLKGEN_LOGICAL_REG_CNT; i++)
   {
      oss << "<ClkReg"  << '_' << i << ">";
      oss << statep->m_regClkGen.values[i];
      oss << "</ClkReg" << '_' << i << ">";
   }

   // Driver registers
   for (i=0; i<PX14_DRIVER_REG_COUNT; i++)
   {
      oss << "<DrvReg" << i << ">";
      oss << statep->m_regDriver.values[i];
      oss << "</DrvReg" << i << ">";
   }

   MI_REPONSE_END(oss)

      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

/// Remote implementation of IOCTL_PX14_DRIVER_VERSION device request
int CServicePX14::rmi_PX14_DRIVER_VERSION (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_DRIVER_VER ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_DRIVER_VER);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- No input

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_DRIVER_VERSION, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "struct_size", ctx.struct_size)
      MI_RESPONSE_ENTRY(oss, "version",     ctx.version)
      MI_REPONSE_END(oss)

      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

/// Remote implementation of IOCTL_PX14_EEPROM_IO device request
int CServicePX14::rmi_PX14_EEPROM_IO (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_EEPROM_IO ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_EEPROM_IO);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- Marshall input

   MI_GET_PARM_OPT("bRead",       ctx.bRead);
   MI_GET_PARM_REQ("eeprom_addr", ctx.eeprom_addr);
   if (!ctx.bRead)
      MI_GET_PARM_REQ("eeprom_val",  ctx.eeprom_val);

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_EEPROM_IO, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "struct_size", ctx.struct_size)
      MI_RESPONSE_ENTRY(oss, "bRead",       ctx.bRead)
      MI_RESPONSE_ENTRY(oss, "eeprom_addr", ctx.eeprom_addr)
      MI_RESPONSE_ENTRY(oss, "eeprom_val",  ctx.eeprom_val)
      MI_REPONSE_END(oss)

      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_PX14_DBG_REPORT (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_DBG_REPORT ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_DBG_REPORT);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- Marshall input

   MI_GET_PARM_REQ("report_type", ctx.report_type);

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_DBG_REPORT, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- No output

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_PX14_DEVICE_REG_WRITE (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_DEV_REG_WRITE ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_DEV_REG_WRITE);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- Marshall input

   MI_GET_PARM_OPT("reg_set",       ctx.reg_set);
   MI_GET_PARM_REQ("reg_idx",       ctx.reg_idx);
   MI_GET_PARM_REQ("reg_mask",      ctx.reg_mask);
   MI_GET_PARM_REQ("reg_val",       ctx.reg_val);
   MI_GET_PARM_OPT("pre_delay_us",  ctx.pre_delay_us);
   MI_GET_PARM_OPT("post_delay_us", ctx.post_delay_us);
   if (PX14REGSET_CLKGEN == ctx.reg_set)
   {
      MI_GET_PARM_REQ("cg_log_reg",    ctx.cg_log_reg);
      MI_GET_PARM_REQ("cg_byte_idx",   ctx.cg_byte_idx);
      MI_GET_PARM_REQ("cg_addr",       ctx.cg_addr);
   }

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_DEVICE_REG_WRITE, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- No output

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_PX14_DEVICE_REG_READ (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_DEV_REG_READ ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_DEV_REG_READ);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- Marshall input

   MI_GET_PARM_REQ("reg_set",       ctx.reg_set);
   MI_GET_PARM_REQ("reg_idx",       ctx.reg_idx);
   MI_GET_PARM_OPT("pre_delay_us",  ctx.pre_delay_us);
   MI_GET_PARM_OPT("post_delay_us", ctx.post_delay_us);
   MI_GET_PARM_OPT("read_how",      ctx.read_how);
   if (PX14REGSET_CLKGEN == ctx.reg_set)
   {
      MI_GET_PARM_REQ("cg_log_reg",    ctx.cg_log_reg);
      MI_GET_PARM_REQ("cg_byte_idx",   ctx.cg_byte_idx);
      MI_GET_PARM_REQ("cg_addr",       ctx.cg_addr);
   }

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_DEVICE_REG_READ, &ctx, ss, 4);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      // Register value is passed back in struct_size field
      MI_RESPONSE_ENTRY(oss, "struct_size",   ctx.struct_size)
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_NoParams (px14_socket_t s, xmlDocPtr docp, io_req_t req)
{
   int res;

   // -- Execute request
   res = DeviceRequest(m_hBrd, req);
   PX14_RETURN_ON_FAIL(res);

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_PX14_RESET_DCMS (px14_socket_t s, xmlDocPtr docp)
{
   return rmi_NoParams(s, docp, IOCTL_PX14_RESET_DCMS);
}

int CServicePX14::rmi_PX14_HWCFG_REFRESH (px14_socket_t s, xmlDocPtr docp)
{
   return rmi_NoParams(s, docp, IOCTL_PX14_HWCFG_REFRESH);
}

int CServicePX14::rmi_PX14_TEMP_REG_IO (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_RAW_REG_IO ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_RAW_REG_IO);
   memset (&ctx, 0, ss);

   // -- Marshall input

   MI_GET_PARM_OPT("bRead",     ctx.bRead);
   MI_GET_PARM_OPT("nRegion",   ctx.nRegion);
   MI_GET_PARM_REQ("nRegister", ctx.nRegister);
   if (!ctx.bRead)
      MI_GET_PARM_REQ("regVal",    ctx.regVal);

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_RAW_REG_IO, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "regVal",        ctx.regVal)
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_PX14_US_DELAY (px14_socket_t s, xmlDocPtr docp)
{
   int res, us_delay;

   // -- Marshall input

   MI_GET_PARM_REQ("us_delay", us_delay);

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_US_DELAY, &us_delay, 4);
   PX14_RETURN_ON_FAIL(res);

   // -- No output

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_PX14_DRIVER_STATS (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_DRIVER_STATS ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_DRIVER_STATS);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- Marshall input

   MI_GET_PARM_REQ("nConnections", ctx.nConnections);

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_DRIVER_STATS, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "struct_size",      ctx.struct_size)
      MI_RESPONSE_ENTRY(oss, "nConnections",     ctx.nConnections)
      MI_RESPONSE_ENTRY(oss, "isr_cnt",          ctx.isr_cnt)
      MI_RESPONSE_ENTRY(oss, "dma_finished_cnt", ctx.dma_finished_cnt)
      MI_RESPONSE_ENTRY(oss, "dma_started_cnt",  ctx.dma_started_cnt)
      MI_RESPONSE_ENTRY(oss, "acq_started_cnt",  ctx.acq_started_cnt)
      MI_RESPONSE_ENTRY(oss, "acq_finished_cnt", ctx.acq_finished_cnt)
      MI_RESPONSE_ENTRY(oss, "dma_bytes",        ctx.dma_bytes)
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_PX14_MODE_SET (px14_socket_t s, xmlDocPtr docp)
{
   int res, op_mode;

   // -- Marshall input

   MI_GET_PARM_REQ("op_mode", op_mode);

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_MODE_SET, &op_mode, 4);
   PX14_RETURN_ON_FAIL(res);

   // -- No output

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_PX14_GET_DEVICE_STATE (px14_socket_t s, xmlDocPtr docp)
{
   int res, dev_state;

   // -- No input

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_GET_DEVICE_STATE,
                       &dev_state, 0, 4);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "dev_state", dev_state)
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_PX14_WAIT_ACQ_OR_XFER (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_WAIT_OP ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_WAIT_OP);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- Marshall input

   MI_GET_PARM_REQ("wait_op",    ctx.wait_op);
   MI_GET_PARM_REQ("timeout_ms", ctx.timeout_ms);

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_WAIT_ACQ_OR_XFER, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- No output

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_PX14_FW_VERSIONS (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_FW_VERSIONS ctx;
   size_t ss;
   int res;

   ss = sizeof(PX14S_FW_VERSIONS);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- No input

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_FW_VERSIONS, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "struct_size",   ctx.struct_size)
      MI_RESPONSE_ENTRY(oss, "fw_ver_pci",    ctx.fw_ver_pci)
      MI_RESPONSE_ENTRY(oss, "fw_ver_sab",    ctx.fw_ver_sab)
      MI_RESPONSE_ENTRY(oss, "fw_ver_pkg",    ctx.fw_ver_pkg)
      MI_RESPONSE_ENTRY(oss, "pre_rel_flags", ctx.pre_rel_flags)
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_PX14_READ_TIMESTAMPS (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_READ_TIMESTAMPS ctx;
   px14_timestamp_t* ts_bufp;
   size_t ss;
   int res;

   ss = sizeof(PX14S_READ_TIMESTAMPS);
   memset (&ctx, 0, ss);
   ctx.struct_size = static_cast<unsigned>(ss);

   // -- Marshall input

   MI_GET_PARM_REQ("buffer_items", ctx.buffer_items);
   MI_GET_PARM_OPT("flags",        ctx.flags);
   MI_GET_PARM_OPT("timeout_ms",   ctx.timeout_ms);

   // We'll need a scratch buffer for this request to buffer timestamps
   if (!EnsureScratchBuffer(ctx.buffer_items * sizeof(px14_timestamp_t)))
      return SIG_OUTOFMEMORY;
   ts_bufp = reinterpret_cast<px14_timestamp_t*>(GetScratchBuf());
   ctx.user_virt_addr =
      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(ts_bufp));

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_READ_TIMESTAMPS, &ctx, ss, ss);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "struct_size",      ctx.struct_size)
      MI_RESPONSE_ENTRY(oss, "buffer_items",     ctx.buffer_items)
      MI_RESPONSE_ENTRY(oss, "flags",            ctx.flags)
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   // Now send timestamp data as raw blob after response; client will
   //  know length from ctx.buffer_items return item
   my_socket_send(s, reinterpret_cast<char*>(ts_bufp),
                  (int)(ctx.buffer_items * sizeof(px14_timestamp_t)), 0);

   return 1;
}

int CServicePX14::rmi_ReadSampleRam (px14_socket_t s, xmlDocPtr docp)
{
   unsigned sample_start, chunk_samples;
   px14_sample_t* bufp;
   int res;

   // -- Marshall input

   MI_GET_PARM_REQ("sample_start", sample_start);
   MI_GET_PARM_REQ("sample_count", chunk_samples);
   if (chunk_samples > PX14_MAX_REMOTE_CHUNK_SAMPS)
      return SIG_PX14_XFER_SIZE_TOO_LARGE;

   // We'll need a scratch buffer for this request to buffer data
   if (!EnsureScratchBuffer(
                            PX14_MAX_REMOTE_CHUNK_SAMPS * sizeof(px14_sample_t)))
   {
      return SIG_OUTOFMEMORY;
   }
   bufp = reinterpret_cast<px14_sample_t*>(GetScratchBuf());

   // -- Execute request

   res = ReadSampleRamBufPX14 (m_hBrd, sample_start, chunk_samples, bufp);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   my_SendCannedResponse(s, true);
   res = my_socket_send(s,
                        reinterpret_cast<const char*>(bufp),
                        chunk_samples * sizeof(px14_sample_t), 0);

   return 1;
}

int CServicePX14::rmi_PX14_NEED_DCM_RESET (px14_socket_t s, xmlDocPtr docp)
{
   int res, bSet;

   // -- Marshall input

   MI_GET_PARM_REQ("bSet", bSet);

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_NEED_DCM_RESET, &bSet, 4);
   PX14_RETURN_ON_FAIL(res);

   // -- No output

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_PX14_GET_HW_CONFIG_EX (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_HW_CONFIG_EX ctx;
   int res;

   // -- Marshall input

   MI_GET_PARM_REQ("struct_size", ctx.struct_size);
   // Don't ask for more than what we know
   if (ctx.struct_size > _PX14SO_PX14S_HW_CONFIG_EX_V1)
      ctx.struct_size = _PX14SO_PX14S_HW_CONFIG_EX_V1;

   // -- Execute request

   res = DeviceRequest(m_hBrd, IOCTL_PX14_GET_HW_CONFIG_EX,
                       &ctx, 4, ctx.struct_size);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "struct_size",   ctx.struct_size)
      MI_RESPONSE_ENTRY(oss, "chan_imps",     ctx.chan_imps)
      MI_RESPONSE_ENTRY(oss, "chan_filters",  ctx.chan_filters)
      MI_RESPONSE_ENTRY(oss, "sys_fpga_type", ctx.sys_fpga_type)
      MI_RESPONSE_ENTRY(oss, "sab_fpga_type", ctx.sab_fpga_type)
      MI_RESPONSE_ENTRY(oss, "board_rev",     ctx.board_rev);
   MI_RESPONSE_ENTRY(oss, "board_rev_sub", ctx.board_rev_sub);

   if (ctx.struct_size > _PX14SO_PX14S_HW_CONFIG_EX_V1)
   {
      MI_RESPONSE_ENTRY(oss, "custFwPkg", ctx.custFwPkg)
         MI_RESPONSE_ENTRY(oss, "custFwPci", ctx.custFwPci)
         MI_RESPONSE_ENTRY(oss, "custFwSab", ctx.custFwSab)
         MI_RESPONSE_ENTRY(oss, "custHw",    ctx.custHw)
   }
   MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;

   return SIG_SUCCESS;
}

int CServicePX14::rmi_SPUD (px14_socket_t s, xmlDocPtr)
{
   int res;

   // -- Execute request
   res = SetPowerupDefaultsPX14(m_hBrd);
   PX14_RETURN_ON_FAIL(res);

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_SetAdcClockSource (px14_socket_t s, xmlDocPtr docp)
{
   unsigned int val;
   int res;

   // -- Marshall input

   MI_GET_PARM_REQ("val", val);

   // -- Execute request

   res = SetAdcClockSourcePX14(m_hBrd, val);
   PX14_RETURN_ON_FAIL(res);

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_CreateRecordingSession (px14_socket_t s, xmlDocPtr docp)
{
   PX14S_REC_SESSION_PARAMS ctx;
   HPX14RECORDING hRec;
   int res;

   CFileParamsCnt filwp;

   memset (&ctx, 0, sizeof(PX14S_REC_SESSION_PARAMS));
   ctx.struct_size = sizeof(PX14S_REC_SESSION_PARAMS);

   // -- Marshall input

   MI_GET_PARM_OPT("rec_flags",	  ctx.rec_flags);
   MI_GET_PARM_OPT("rec_samples",	  ctx.rec_samples);
   MI_GET_PARM_OPT("acq_samples",	  ctx.acq_samples);
   MI_GET_PARM_OPT("xfer_samples",	  ctx.xfer_samples);
   res = _ExtractMiParm_FilewParms(docp, filwp);
   if (res < 0) return res;
   ctx.filwp = (res > 0) ? &filwp.m_fwp : NULL;
   MI_GET_PARM_OPT("ss_len_samples", ctx.ss_len_samples);
   MI_GET_PARM_OPT("ss_period_xfer", ctx.ss_period_xfer);
   MI_GET_PARM_OPT("ss_period_ms",	  ctx.ss_period_ms);
   MI_GET_PARM_OPT("callbackData",	  ctx.callbackData);

   // -- Execute request

   res = CreateRecordingSessionPX14(m_hBrd, &ctx, &hRec);
   PX14_RETURN_ON_FAIL(res);

   // We track recording handles so we can clean them if needed
   m_recSet.insert(hRec);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "remote_handle",
                        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hRec)))
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_RecSesHandleOnly (px14_socket_t s,
                                        xmlDocPtr docp,
                                        FuncRecSesHandleOnly_t funcp,
                                        HPX14RECORDING* handlep)
{
   HPX14RECORDING hRec;
   unsigned long long qw_rec;
   int res;

   SIGASSERT_NULL_OR_POINTER(handlep, HPX14RECORDING);

   // -- Marshall input

   MI_GET_PARM_REQ("remote_handle", qw_rec);

   // -- Execute request

   hRec = reinterpret_cast<HPX14RECORDING>(static_cast<uintptr_t>(qw_rec));
   res = (*funcp)(hRec);
   PX14_RETURN_ON_FAIL(res);

   if (handlep)
      *handlep = hRec;

   // -- No output

   my_SendCannedResponse(s, true);
   return 1;
}

int CServicePX14::rmi_AbortRecordingSession (px14_socket_t s, xmlDocPtr docp)
{
   return rmi_RecSesHandleOnly(s, docp, AbortRecordingSessionPX14);
}

int CServicePX14::rmi_ArmRecordingSession (px14_socket_t s, xmlDocPtr docp)
{
   return rmi_RecSesHandleOnly(s, docp, ArmRecordingSessionPX14);
}

int CServicePX14::rmi_DeleteRecordingSession (px14_socket_t s, xmlDocPtr docp)
{
   HPX14RECORDING hRec;
   int res;

   res = rmi_RecSesHandleOnly(s, docp, DeleteRecordingSessionPX14, &hRec);
   m_recSet.erase(hRec);

   return res;
}

int CServicePX14::rmi_GetRecordingSessionOutFlags  (px14_socket_t s,
                                                    xmlDocPtr docp)
{
   unsigned int out_flags;
   HPX14RECORDING hRec;
   unsigned long long qw_rec;
   int res;

   // -- Marshall input

   MI_GET_PARM_REQ("remote_handle", qw_rec);

   // -- Execute request

   hRec = reinterpret_cast<HPX14RECORDING>(static_cast<uintptr_t>(qw_rec));
   res = GetRecordingSessionOutFlagsPX14(hRec, &out_flags);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "out_flags", out_flags)
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_GetRecordingSessionProgress (px14_socket_t s,
                                                   xmlDocPtr docp)
{
   PX14S_REC_SESSION_PROG ctx;
   HPX14RECORDING hRec;
   unsigned int flags;
   unsigned long long qw_rec;
   int res;

   memset (&ctx, 0, sizeof(PX14S_REC_SESSION_PROG));
   ctx.struct_size = sizeof(PX14S_REC_SESSION_PROG);

   // -- Marshall input

   MI_GET_PARM_REQ("remote_handle", qw_rec);
   MI_GET_PARM_OPT("flags", flags);

   // -- Execute request

   hRec = reinterpret_cast<HPX14RECORDING>(static_cast<uintptr_t>(qw_rec));
   res = GetRecordingSessionProgressPX14(hRec, &ctx, flags);
   PX14_RETURN_ON_FAIL(res);

   std::string str_error;
   if ((ctx.status == PX14RECSTAT_ERROR) && (NULL != ctx.err_textp))
   {
      str_error.assign(ctx.err_textp);
      FreeMemoryPX14(ctx.err_textp);
   }

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "struct_size",      ctx.struct_size)
      MI_RESPONSE_ENTRY(oss, "status",           ctx.status)
      MI_RESPONSE_ENTRY(oss, "elapsed_time_ms",  ctx.elapsed_time_ms)
      MI_RESPONSE_ENTRY(oss, "samps_recorded",   ctx.samps_recorded)
      MI_RESPONSE_ENTRY(oss, "samps_to_record",  ctx.samps_to_record)
      MI_RESPONSE_ENTRY(oss, "xfer_samples",     ctx.xfer_samples)
      MI_RESPONSE_ENTRY(oss, "xfer_count",       ctx.xfer_count)
      MI_RESPONSE_ENTRY(oss, "snapshot_counter", ctx.snapshot_counter)

      if (ctx.status == PX14RECSTAT_ERROR)
      {
         MI_RESPONSE_ENTRY(oss,     "err_res", ctx.err_res)
            if (!str_error.empty())
               MI_RESPONSE_ENTRY(oss, "err_textp", str_error.c_str())
      }

   MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   return 1;
}

int CServicePX14::rmi_GetRecordingSnapshot (px14_socket_t s, xmlDocPtr docp)
{
   unsigned int samples, samples_got, ss_count;
   HPX14RECORDING hRec;
   px14_sample_t* bufp;
   unsigned long long qw_rec;
   int res;

   // -- Marshall input

   MI_GET_PARM_REQ("remote_handle", qw_rec);
   MI_GET_PARM_REQ("samples",       samples);

   // -- Execute request

   if (!EnsureScratchBuffer(samples * sizeof(px14_sample_t)))
      return SIG_OUTOFMEMORY;
   bufp = reinterpret_cast<px14_sample_t*>(GetScratchBuf());

   hRec = reinterpret_cast<HPX14RECORDING>(static_cast<uintptr_t>(qw_rec));
   res = GetRecordingSnapshotPX14(hRec, bufp, samples,
                                  &samples_got, &ss_count);
   PX14_RETURN_ON_FAIL(res);

   // -- Marshall output

   std::ostringstream oss;
   MI_RESPONSE_START(oss)
      MI_RESPONSE_ENTRY(oss, "samples_got", samples_got)
      MI_RESPONSE_ENTRY(oss, "ss_count",    ss_count)
      MI_REPONSE_END(oss)
      std::string sr(oss.str());
   my_SendLengthPrefixedData(s, sr.c_str(), (unsigned)sr.length());

   // Send snapshot data as binary blob after response
   my_socket_send(s, reinterpret_cast<const char*>(bufp),
                  samples_got * sizeof(px14_sample_t), 0);

   return 1;
}

// PX14 library exports implementation --------------------------------- //

/** @brief
  Call once per application instance to initialize sockets
  implementation

  If your application does not already initialize your platform's
  sockets implementation then you should call this function once
  at application startup.
  */
PX14API SocketsInitPX14()
{
   return SysSocketsInit();
}

/** @brief
  Sockets implementation cleanup; call if your app calls
  InitSocketsPX14

  If your application calls the SocketsInitPX14 function then this
  function should be invoked as part of your application cleanup
  */
PX14API SocketsCleanupPX14()
{
   return SysSocketsCleanup();
}

// Obtain a handle to a PX14400 device on a remote system
PX14API ConnectToRemoteDeviceAPX14 (HPX14* phDev,
                                    unsigned int brdNum,
                                    PX14S_REMOTE_CONNECT_CTXA* ctxp)
{
   return _ConnectToRemoteDeviceWork(phDev, brdNum, 0, false, ctxp);
}

// Obtain a handle to a virtual (fake) PX14 device
PX14API ConnectToRemoteVirtualDeviceAPX14 (HPX14* phDev,
                                           unsigned int serialNum,
                                           unsigned int brdNum,
                                           PX14S_REMOTE_CONNECT_CTXA* ctxp)
{
   return _ConnectToRemoteDeviceWork(phDev, serialNum, brdNum, true, ctxp);
}

PX14API GetRemoteDeviceCountAPX14 (const char* server_addrp,
                                   unsigned short port,
                                   unsigned int** sn_bufpp)
{
   PX14S_REMOTE_CONNECT_CTXA ctx;
   unsigned int device_count, i;
   HPX14 hTmp;
   int res;

   SIGASSERT_NULL_OR_POINTER(sn_bufpp, unsigned int*);
   SIGASSERT_POINTER (server_addrp, char);
   if (NULL == server_addrp)
      return SIG_PX14_INVALID_ARG_1;

   // Connect to the service
   memset (&ctx, 0, sizeof(PX14S_REMOTE_CONNECT_CTX));
   ctx.struct_size = sizeof(PX14S_REMOTE_CONNECT_CTX);
   ctx.pServerAddress = server_addrp;
   ctx.port = port;
   ctx.pSubServices = NULL;
   res = ConnectToRemoteDeviceAPX14(&hTmp, PX14_BOARD_NUM_INVALID, &ctx);
   PX14_RETURN_ON_FAIL(res);

   // Object will ensure we disconnect from server
   CAutoDisconnectPX14 ard(hTmp);

   // Generate server request
   std::string strReq;
   res = my_GenerateMethodRequest("PX14_DEVCNT", NULL, strReq);
   PX14_RETURN_ON_FAIL(res);

   // Make the request
   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hTmp, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // Parse results
   MC_GET_RETVAL_REQ("DeviceCount",  device_count);
   if (device_count > PX14_MAX_DEVICES)
      device_count = PX14_MAX_DEVICES;

   if (device_count && sn_bufpp)
   {
      static const int cBufBytes = 16;

      char cBuf[cBufBytes];

      *sn_bufpp = my_malloc_arrayT<unsigned int>(device_count, true);
      if (NULL == *sn_bufpp)
         return SIG_OUTOFMEMORY;

      for (i=0; i<device_count; i++)
      {
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
         _snprintf_s (cBuf, cBufBytes, "Device%d", i);
#else
         snprintf(cBuf, cBufBytes, "Device%d", i);
#endif
         MC_GET_RETVAL_REQ(cBuf, (*sn_bufpp)[i]);
      }
   }

   return static_cast<int>(device_count);
}

PX14API GetServiceSocketPX14 (HPX14 hBrd, px14_socket_t* sockp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(sockp, px14_socket_t);
   if (NULL == sockp)
      return SIG_PX14_INVALID_ARG_2;

   res = IsDeviceRemotePX14(hBrd);
   if (res < 0)
      return res;
   if (0 == res)
      return SIG_PX14_NOT_REMOTE;

   statep = PX14_H2B(hBrd);
   *sockp = statep->m_client_statep->m_sock;
   return SIG_SUCCESS;
}

// Free a response from a previous remote service request
PX14API FreeServiceResponsePX14 (void* bufp)
{
   SIGASSERT_NULL_OR_POINTER(bufp, xmlDoc);
   xmlFreeDoc(reinterpret_cast<xmlDoc*>(bufp));

   return SIG_SUCCESS;
}

/*
   The format of a request is:
   <?xml version="1.0" encoding="utf-8"?>
   <SigServiceRequest>
   <!-- Insert service specific request here -->
   </SigServiceRequest>
   */
/** @brief Send a client request to a remote PX14 server

  @param hBrd
  The PX14 device handle. The ConnectToRemoteDevicePX14
  function is used to connect to a remote PX14 device.
  @param svc_reqp
  A pointer to a buffer containing the request. The request is XML
  data and may use any valid encoding (as deemed by iconv: ASCII,
  UTF-8, UTF16, char, wchar_t, etc)
  @param req_bytes
  The size of the request in bytes
  @param responsepp
  A pointer to a void* variable which will receive the address of
  the library allocated response buffer. Depending on the flags passed
  in the flags parameter, the response data will be one of the
following:

If the PX14SRF_AUTO_HANDLE_RESPONSE flag is specified then the
response data will be service specific response value passed in the
XML response data. The format of this data is UTF-8 encoded data but
is safe to convert to ASCII. (Signatec authored services will only
return data safe for implicit ASCII conversion.) The FreeMemoryPX14
function should be used to free this memory.

If the PX14SRF_AUTO_HANDLE_RESPONSE flag is not specified then the
response data will be a pointer to an xmlDocPtr that points to an
xmlDoc object that contains the parsed response information. This
document may be interrogated via libxml2 routines. The
FreeServiceResponsePX14 function should be used to free this memory.
@param timeoutMs
An optional timeout value. It's specified in terms of milliseconds
but will most likely operate on granularity of a second.
@param flags
Optional flags (PX14SRF_*) that dictate behavior

@sa FreeServiceResponsePX14
*/
PX14API SendServiceRequestPX14 (HPX14 hBrd,
                                const void* svc_reqp,
                                int req_bytes,
                                void** responsepp,
                                unsigned int timeoutMs,
                                unsigned int flags)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   res = _SendServiceRequestWork(*statep, svc_reqp, req_bytes,
                                 responsepp, timeoutMs, flags);

   // Free service request if necessary
   if (svc_reqp && (flags & PX14SRF_AUTO_FREE_REQUEST))
      xmlFree(const_cast<void*>(svc_reqp));

   return res;
}

// Obtain remote server information (UNICODE)
PX14API GetHostServerInfoWPX14 (HPX14 hBrd,
                                wchar_t** server_addrpp,
                                unsigned short* portp)
{
   char* abufp = NULL;
   int res;

   SIGASSERT_NULL_OR_POINTER (server_addrpp, wchar_t*);
   SIGASSERT_NULL_OR_POINTER (portp, unsigned short);

   res = GetHostServerInfoAPX14(hBrd,
                                server_addrpp ? &abufp : NULL, portp);
   PX14_RETURN_ON_FAIL(res);

   res = ConvertAsciiToWideCharAlloc(abufp, server_addrpp);
   if (abufp)
      FreeMemoryPX14(abufp);

   return res;
}

PX14API GetHostServerInfoAPX14 (HPX14 hBrd,
                                char** server_addrpp,
                                unsigned short* portp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(server_addrpp, char*);
   SIGASSERT_NULL_OR_POINTER(portp, unsigned short);

   res = IsDeviceRemotePX14(hBrd);
   if (res < 0) return res;

   if (res > 0)
   {
      // Device is remote

      statep = PX14_H2B(hBrd);
      SIGASSERT_POINTER(statep->m_client_statep, CRemoteCtxPX14);
      if (!statep->m_client_statep)
         return SIG_PX14_UNEXPECTED;

      if (server_addrpp)
      {
         res = AllocAndCopyString(
                                  statep->m_client_statep->m_strSrvAddr.c_str(),
                                  server_addrpp);
         PX14_RETURN_ON_FAIL(res);
      }

      if (portp)
         *portp = statep->m_client_statep->m_srvPort;
   }
   else
   {
      // Device is local; possibly virtual

      if (server_addrpp)
      {
         std::string local_addr;
         res = SysNetworkGetLocalAddress(local_addr);
         PX14_RETURN_ON_FAIL(res);

         res = AllocAndCopyString(local_addr.c_str(), server_addrpp);
         PX14_RETURN_ON_FAIL(res);
      }

      if (portp)
         *portp = 0;		// Identifies device as local
   }

   return SIG_SUCCESS;
}

int RemoteDeviceRequest (HPX14 hBrd,
                         io_req_t req,
                         void* ctxp,
                         size_t in_bytes,
                         size_t out_bytes)
{
   int res;

   switch (req)
   {
      case IOCTL_PX14_DEVICE_REG_WRITE:
         res = rmc_PX14_DEVICE_REG_WRITE(hBrd,
                                         reinterpret_cast<PX14S_DEV_REG_WRITE*>(ctxp));
         break;
      case IOCTL_PX14_DEVICE_REG_READ:
         res = rmc_PX14_DEVICE_REG_READ(hBrd,
                                        reinterpret_cast<PX14S_DEV_REG_READ*>(ctxp));
         break;

      case IOCTL_PX14_MODE_SET:
         res = rmc_PX14_MODE_SET(hBrd,
                                 reinterpret_cast<int*>(ctxp));
         break;

      case IOCTL_PX14_DRIVER_VERSION:
         res = rmc_PX14_DRIVER_VERSION(hBrd,
                                       reinterpret_cast<PX14S_DRIVER_VER*>(ctxp));
         break;

      case IOCTL_PX14_EEPROM_IO:
         res = rmc_PX14_EEPROM_IO(hBrd,
                                  reinterpret_cast<PX14S_EEPROM_IO*>(ctxp));
         break;

      case IOCTL_PX14_DBG_REPORT:
         res = rmc_PX14_DBG_REPORT(hBrd,
                                   reinterpret_cast<PX14S_DBG_REPORT*>(ctxp));
         break;

      case IOCTL_PX14_RESET_DCMS:
         res = rmc_PX14_NoParams(hBrd, "PX14_DCMRST");
         break;
      case IOCTL_PX14_NEED_DCM_RESET:
         res = rmc_PX14_NEED_DCM_RESET(hBrd,
                                       reinterpret_cast<int*>(ctxp));
         break;

      case IOCTL_PX14_HWCFG_REFRESH:
         res = rmc_PX14_NoParams(hBrd, "PX14_HWCFG_REFRESH");
         break;

      case IOCTL_PX14_RAW_REG_IO:
         res = rmc_PX14_TEMP_REG_IO(hBrd,
                                    reinterpret_cast<PX14S_RAW_REG_IO*>(ctxp));
         break;

      case IOCTL_PX14_US_DELAY:
         res = rmc_PX14_US_DELAY (hBrd,
                                  reinterpret_cast<int*>(ctxp));
         break;

      case IOCTL_PX14_DRIVER_STATS:
         res = rmc_PX14_DRIVER_STATS (hBrd,
                                      reinterpret_cast<PX14S_DRIVER_STATS*>(ctxp));
         break;

      case IOCTL_PX14_GET_DEVICE_STATE:
         res = rmc_PX14_GET_DEVICE_STATE (hBrd,
                                          reinterpret_cast<int*>(ctxp));
         break;

      case IOCTL_PX14_WAIT_ACQ_OR_XFER:
         res = rmc_PX14_WAIT_ACQ_OR_XFER (hBrd,
                                          reinterpret_cast<PX14S_WAIT_OP*>(ctxp));
         break;

      case IOCTL_PX14_FW_VERSIONS:
         res = rmc_PX14_FW_VERSIONS (hBrd,
                                     reinterpret_cast<PX14S_FW_VERSIONS*>(ctxp));
         break;

      case IOCTL_PX14_READ_TIMESTAMPS:
         res = rmc_PX14_READ_TIMESTAMPS (hBrd,
                                         reinterpret_cast<PX14S_READ_TIMESTAMPS*>(ctxp));
         break;

      case IOCTL_PX14_GET_HW_CONFIG_EX:
         res = rmc_PX14_GET_HW_CONFIG_EX (hBrd,
                                          reinterpret_cast<PX14S_HW_CONFIG_EX*>(ctxp));
         break;

         // We don't implement remote implementations for these guys
      case IOCTL_PX14_JTAG_STREAM:
      case IOCTL_PX14_JTAG_IO:
      case IOCTL_PX14_DRIVER_BUFFERED_XFER:
      case IOCTL_PX14_DMA_XFER:
      case IOCTL_PX14_GET_DEVICE_ID:
      case IOCTL_PX14_DMA_BUFFER_ALLOC:
      case IOCTL_PX14_DMA_BUFFER_FREE:
      default:
         res = SIG_PX14_REMOTE_CALL_NOT_AVAILABLE;
   }

   return res;
}

// Module-local function implementation -------------------------------- //

int _SendServiceRequestWork (CStatePX14& state,
                             const void* svc_reqp,
                             int req_bytes,
                             void** responsepp,
                             unsigned int timeoutMs,
                             unsigned int flags)
{
   xmlDocPtr docp;
   int res, data;
   HPX14 hBrd;

   hBrd = PX14_B2H(&state);

   SIGASSERT_POINTER(svc_reqp, char);
   if (NULL == svc_reqp)
      return SIG_PX14_INVALID_ARG_2;
   if (0 == (flags & PX14SRF_AUTO_HANDLE_RESPONSE))
   {
      SIGASSERT_POINTER(responsepp, char*);
      if (NULL == responsepp)
         return SIG_PX14_INVALID_ARG_4;
   }
   if (!IsDeviceRemotePX14(hBrd) &&
       (0 == (state.m_flags & PX14LIBF_REMOTE_CONNECTING)))
   {
      return SIG_PX14_NOT_REMOTE;
   }

   // -- Send the request

   // Send request length in bytes
   data = htonl(req_bytes);
   res = my_socket_send(state.m_client_statep->m_sock,
                        reinterpret_cast<const char*>(&data), 4, 0);
   PX14_RETURN_ON_FAIL(res);

   // Send request data
   res = my_socket_send(state.m_client_statep->m_sock,
                        reinterpret_cast<const char*>(svc_reqp), req_bytes, 0);
   PX14_RETURN_ON_FAIL(res);

#ifndef PX14PP_NO_REMOTE_CALL_TIMEOUTS
   // Wait for response
   if (timeoutMs)
   {
      struct timeval tvWait;
      fd_set fdsRead;

      FD_ZERO(&fdsRead);
      FD_SET (state.m_client_statep->m_sock, &fdsRead);

      tvWait.tv_sec = timeoutMs / 1000;
      tvWait.tv_usec = (timeoutMs % 1000) * 1000;	// ms to us
      // WIN32: First parameter of select is ignored
      res = select(static_cast<int>(state.m_client_statep->m_sock+1), &fdsRead, NULL, NULL, &tvWait);
      if (-1 == res)
         return SIG_PX14_SOCKET_ERROR;

      if (!FD_ISSET(state.m_client_statep->m_sock, &fdsRead))
         return SIG_PX14_TIMED_OUT;
   }
#endif

   // -- Read the response

   if (!state.m_client_statep->m_recv_bufp.get())
   {
      // Library should have allocated receive buffer!
      SIGASSERT(PX14_FALSE);
      return SIG_PX14_UNEXPECTED;
   }

   res = my_ReadResponse(state.m_client_statep->m_sock,
                         state.m_client_statep->m_recv_bufp.get(),
                         state.m_client_statep->m_recv_buf_bytes, &docp);
   PX14_RETURN_ON_FAIL(res);
   if (responsepp) *responsepp = docp;

   if (0 == (flags & PX14SRF_NO_VALIDATION))
   {
      // Validate bare minimum
      res = my_VerifyGenericResponse(docp,
                                     &state.m_client_statep->m_strSrvErr);
      if (SIG_SUCCESS != res)
      {
         FreeServiceResponsePX14(reinterpret_cast<void*>(docp));
         if (responsepp) *responsepp = NULL;

         return res;
      }
   }

   // -- Auto handle the response if necessary

   if (flags & PX14SRF_AUTO_HANDLE_RESPONSE)
   {
      std::string strResponse;

      // Validate the response
      res = my_ValidateResponse(docp, &strResponse);
      FreeServiceResponsePX14(reinterpret_cast<void*>(docp));
      if (SIG_SUCCESS != res)
      {
         if (flags & PX14SRF_CONNECTING)
            return SIG_PX14_CONNECTION_REFUSED;

         return res;
      }

      if (responsepp)
      {
         char* chp;

         if (!strResponse.empty())
         {
            res = AllocAndCopyString(strResponse.c_str(), &chp);
            PX14_RETURN_ON_FAIL(res);
         }
         else
            chp = NULL;


         *responsepp = chp;
      }

      return SIG_SUCCESS;
   }

   return SIG_SUCCESS;
}

/** @brief Establish a connection to a remote PX14 device

  @note
  See CServicePX14::rmi_ConnectToDevice for server side
  */
int _ConnectToRemoteDeviceWork (HPX14* phDev,
                                unsigned int brdNum,
                                unsigned int ordNum,
                                bool bVirtual,
                                PX14S_REMOTE_CONNECT_CTXA* ctxp)
{
   CStatePX14* statep;
   HPX14 hDevWork;
   int res;

   res = Remote_ConnectToService(&hDevWork, ctxp);
   PX14_RETURN_ON_FAIL(res);
   statep = PX14_H2B(hDevWork);

   if (brdNum != PX14_BOARD_NUM_INVALID)
   {
      // Establish a connection to desired PX14 device
      res = _SendDevConnectRequest(*statep, brdNum, ordNum, bVirtual);
      if (SIG_SUCCESS != res)
      {
         Remote_DisconnectFromService(*statep);
         return res;
      }

      if (bVirtual)
         statep->m_flags |= PX14LIBF_VIRTUAL;

      // Cache the remainder of the configuration info
      res = CacheHwCfgEx(hDevWork);
      PX14_RETURN_ON_FAIL(res);

      // Update our local register cache will remote's cache
      res = RefreshLocalRegisterCachePX14(hDevWork, PX14_FALSE);
   }

   *phDev = hDevWork;
   return SIG_SUCCESS;
}

/**
  We're now connected to the server but not associated with any particular
  device, connect to requested device
  */
int _SendDevConnectRequest (CStatePX14& state,
                            unsigned int brdNum,
                            unsigned int ordNum,
                            bool bVirtual)
{
   unsigned srv_libflags;
   int res;

   MethodReqParamList mrpl;
   MC_MRPL_ADD(mrpl, "BoardNum", brdNum);
   if (bVirtual)
   {
      MC_MRPL_ADD(mrpl, "OrdNum", ordNum);
      MC_MRPL_ADD(mrpl, "Virtual", 1);
   }

   std::string strReq;
   res = my_GenerateMethodRequest("PX14_CONNECT", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(PX14_B2H(&state), strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // Grab board information
   MC_GET_RETVAL_REQ("SerialNum",    state.m_serial_num);
   MC_GET_RETVAL_REQ("OrdinalNum",   state.m_ord_num);
   MC_GET_RETVAL_REQ("Features",     state.m_features);
   MC_GET_RETVAL_REQ("Ch1Imp",       state.m_chanImpCh1);
   MC_GET_RETVAL_REQ("Ch2Imp",       state.m_chanImpCh2);
   MC_GET_RETVAL_REQ("LibFlags",     srv_libflags);
   MC_GET_RETVAL_OPT("SlaveDelayNs", state.m_slave_delay_ns);
   MC_GET_RETVAL_OPT("DriverVer",    state.m_driver_ver);
   MC_GET_RETVAL_OPT("FwVerPci",     state.m_fw_ver_pci);
   MC_GET_RETVAL_OPT("FwVerSab",     state.m_fw_ver_sab);
   MC_GET_RETVAL_OPT("FwVerPkg",     state.m_fw_ver_pkg);
   MC_GET_RETVAL_OPT("FwPreRel",     state.m_fw_pre_rel);

   state.m_flags &= ~PX14LIBF__REMOTE_COPY;
   state.m_flags |= (srv_libflags & PX14LIBF__REMOTE_COPY);

   return res;
}

void _AppendMrpl_FilwParms (MethodReqParamList& mrpl,
                            PX14S_FILE_WRITE_PARAMS* ctxp)
{
   MC_MRPL_ADD (mrpl, "filwp_" "struct_size",      ctxp->struct_size);
   if (ctxp->pathname)
   {
      std::string s(ctxp->pathname);
      my_EscapeReservedXmlMarkupChars(s);
      MC_MRPL_ADD (mrpl, "filwp_" "pathname",     s.c_str());
   }
   if (ctxp->pathname2)
   {
      std::string s(ctxp->pathname2);
      my_EscapeReservedXmlMarkupChars(s);
      MC_MRPL_ADD (mrpl, "filwp_" "pathname2",    s.c_str());
   }
   MC_MRPL_ADD_NON_ZERO (mrpl, "filwp_" "flags",            ctxp->flags);
   MC_MRPL_ADD_NON_ZERO (mrpl, "filwp_" "init_bytes_skip",  ctxp->init_bytes_skip);
   MC_MRPL_ADD_NON_ZERO (mrpl, "filwp_" "max_file_seg",     ctxp->max_file_seg);
   MC_MRPL_ADD_NON_ZERO (mrpl, "filwp_" "flags_out",        ctxp->flags_out);
   if (ctxp->operator_notes)
   {
      std::string s(ctxp->operator_notes);
      my_EscapeReservedXmlMarkupChars(s);
      MC_MRPL_ADD (mrpl, "filwp_" "operator_notes", s.c_str());
   }
   if (ctxp->ts_filenamep)
   {
      std::string s(ctxp->ts_filenamep);
      my_EscapeReservedXmlMarkupChars(s);
      MC_MRPL_ADD (mrpl, "filwp_" "ts_filenamep", s.c_str());
   }
}

int _ExtractMiParm_FilewParms (xmlDoc* docp, CFileParamsCnt& filwp)
{
   unsigned int struct_size;
   int res;

   MI_GET_PARM_OPT("filwp_" "struct_size", struct_size);
   if (SIG_SUCCESS != res)
      return 0;

   MI_GET_PARM_OPT("filwp_" "pathname",  filwp.m_strPath1);
   MI_GET_PARM_OPT("filwp_" "pathname2", filwp.m_strPath2);
   MI_GET_PARM_OPT("filwp_" "flags",     filwp.m_fwp.flags);
   MI_GET_PARM_OPT("filwp_" "init_bytes_skip",
                   filwp.m_fwp.init_bytes_skip);
   MI_GET_PARM_OPT("filwp_" "max_file_seg",
                   filwp.m_fwp.max_file_seg);
   MI_GET_PARM_OPT("filwp_" "flags_out", filwp.m_fwp.flags_out);
   MI_GET_PARM_OPT("filwp_" "operator_notes", filwp.m_strOpNotes);
   MI_GET_PARM_OPT("filwp_" "ts_filenamep", filwp.m_strTsFile);

   filwp.AssignStrings();
   return 1;
}

/// Connect to PX14 service only; no devices connected
int Remote_ConnectToService (HPX14* phDev, PX14S_REMOTE_CONNECT_CTXA* ctxp)
{
   int res, req_bytes;
   CStatePX14* statep;
   void* requestp;

   SIGASSERT_POINTER (phDev, HPX14);
   if (NULL == phDev)
      return SIG_PX14_INVALID_ARG_1;
   SIGASSERT_POINTER (ctxp, PX14S_REMOTE_CONNECT_CTX);
   if (NULL == ctxp)
      return SIG_PX14_INVALID_ARG_2;
   SIGASSERT_POINTER(ctxp->pServerAddress, char);
   if (NULL == ctxp->pServerAddress)
      return SIG_INVALIDARG;
   SIGASSERT_NULL_OR_POINTER(ctxp->pApplicationName, char);
   SIGASSERT_NULL_OR_POINTER(ctxp->pSubServices, char);

   // Allocate a new device connection
   try
   {
      statep = new CStatePX14(PX14_BAD_DEV_HANDLE, 0, 0);
      statep->m_client_statep = new CRemoteCtxPX14;
      statep->m_client_statep->AllocateChunkBuffer();
      statep->m_client_statep->m_strSrvAddr.assign(ctxp->pServerAddress);
      if (ctxp->pApplicationName)
      {
         statep->m_client_statep->m_strAppName.assign(
                                                      ctxp->pApplicationName);
      }
      if (ctxp->pSubServices)
      {
         statep->m_client_statep->m_strSubSvcs.assign(
                                                      ctxp->pSubServices);
      }
      statep->m_client_statep->m_srvPort = ctxp->port;
      // PX14400 device is remote, so pass service requests to
      //  remote server
      statep->m_pfnSvcProc = RemoteDeviceRequest;
   }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }
   // Auto-free if we fail below
   std::auto_ptr<CStatePX14> spState(statep);

   // Establish raw connection to remote system
   res = SysNetworkConnectTcp(&statep->m_client_statep->m_sock,
                              ctxp->pServerAddress, ctxp->port);
   PX14_RETURN_ON_FAIL(res);

   // Now establish connection to our service
   res = my_GenerateConnectRequest(PX14_SERVICE_NAME, &requestp, &req_bytes,
                                   ctxp->pApplicationName, ctxp->pSubServices);
   PX14_RETURN_ON_FAIL(res);

   SIGASSERT_POINTER(requestp, char);
   SIGASSERT(req_bytes > 0);
   statep->m_flags |= PX14LIBF_REMOTE_CONNECTING;
   res = SendServiceRequestPX14(PX14_B2H(statep), requestp, req_bytes,
                                NULL, PX14_SERVER_REQ_TIMEOUT_DEF,
                                PX14SRF_AUTO_HANDLE_RESPONSE | PX14SRF_CONNECTING);
   statep->m_flags &= ~PX14LIBF_REMOTE_CONNECTING;
   PX14_RETURN_ON_FAIL(res);
   statep->m_flags |=  PX14LIBF_REMOTE_CONNECTED;

   *phDev = reinterpret_cast<HPX14>(spState.release());
   return SIG_SUCCESS;
}

int Remote_DisconnectFromService (CStatePX14& state)
{
   static const char* dc_reqp =
      "<SigServiceRequest><Disconnect/></SigServiceRequest>";

   // Send the disconnect request to the server
   SendServiceRequestPX14(PX14_B2H(&state), dc_reqp,
                          (int)strlen(dc_reqp), NULL, 1000,
                          PX14SRF_AUTO_HANDLE_RESPONSE);

   // Physically disconnect from socket
   delete state.m_client_statep;		// Will close socket
   state.m_client_statep = NULL;

   state.m_flags &=
      ~(PX14LIBF_REMOTE_CONNECTED | PX14LIBF_REMOTE_CONNECTING);

   return SIG_SUCCESS;
}

int rmc_RefreshLocalRegisterCache (HPX14 hBrd, int bFromHardware)
{
   int res, regCount, i;
   CStatePX14* statep;

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "bFromHardware", bFromHardware);
   res = my_GenerateMethodRequest("PX14_REG_CACHE", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   statep = PX14_H2B(hBrd);

   // Device registers
   MC_GET_RETVAL_REQ("DevRegCnt",    regCount);
   if (regCount > PX14_DEVICE_REG_COUNT)
      regCount = PX14_DEVICE_REG_COUNT;
   for (i=0; i<regCount; i++)
   {
      std::ostringstream oss;
      oss << "DevReg" << i;
      MC_GET_RETVAL_REQ(oss.str().c_str(), statep->m_regDev.values[i]);
   }

   // Clock registers
   MC_GET_RETVAL_REQ("ClkRegCnt",    regCount);
   if (regCount > PX14_CLKGEN_LOGICAL_REG_CNT)
      regCount = PX14_CLKGEN_LOGICAL_REG_CNT;

   for (i=0; i<regCount; i++)
   {
      std::ostringstream oss;
      oss << "ClkReg" << '_' << i;
      MC_GET_RETVAL_REQ(oss.str().c_str(),
                        statep->m_regClkGen.values[i]);
   }

   // Driver registers
   MC_GET_RETVAL_REQ("DrvRegCnt",    regCount);
   if (regCount > PX14_DRIVER_REG_COUNT)
      regCount = PX14_DRIVER_REG_COUNT;
   for (i=0; i<regCount; i++)
   {
      std::ostringstream oss;
      oss << "DrvReg" << i;
      MC_GET_RETVAL_REQ(oss.str().c_str(), statep->m_regDriver.values[i]);
   }

   return SIG_SUCCESS;
}

int rmc_PX14_DRIVER_VERSION (HPX14 hBrd, PX14S_DRIVER_VER* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DRIVER_VER, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14_DRIVER_VER_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   res = my_GenerateMethodRequest("PX14_DVER", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   MC_GET_RETVAL_REQ("version", ctxp->version);

   return SIG_SUCCESS;
}

int rmc_PX14_EEPROM_IO (HPX14 hBrd, PX14S_EEPROM_IO* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_EEPROM_IO, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_EEPROM_IO_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD         (mrpl, "struct_size", ctxp->struct_size);
   MC_MRPL_ADD_NON_ZERO(mrpl, "bRead",       ctxp->bRead);
   MC_MRPL_ADD         (mrpl, "eeprom_addr", ctxp->eeprom_addr);
   if (!ctxp->bRead)
      MC_MRPL_ADD     (mrpl, "eeprom_val",  ctxp->eeprom_val);
   res = my_GenerateMethodRequest("PX14_EEPROM_IO", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   MC_GET_RETVAL_REQ("struct_size", ctxp->struct_size);
   MC_GET_RETVAL_REQ("bRead",       ctxp->bRead);
   MC_GET_RETVAL_REQ("eeprom_addr", ctxp->eeprom_addr);
   MC_GET_RETVAL_REQ("eeprom_val",  ctxp->eeprom_val);

   return SIG_SUCCESS;
}

int rmc_PX14_DBG_REPORT (HPX14 hBrd, PX14S_DBG_REPORT* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DBG_REPORT, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_DBG_REPORT_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "struct_size", ctxp->struct_size);
   MC_MRPL_ADD(mrpl, "report_type", ctxp->report_type);
   res = my_GenerateMethodRequest("PX14_DBG_REPORT", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - No output

   return SIG_SUCCESS;
}

int rmc_PX14_DEVICE_REG_WRITE (HPX14 hBrd, PX14S_DEV_REG_WRITE* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DEV_REG_WRITE, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_DEV_REG_WRITE_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "struct_size",       ctxp->struct_size);
   if (ctxp->reg_set)
      MC_MRPL_ADD(mrpl, "reg_set",       ctxp->reg_set);
   MC_MRPL_ADD(mrpl, "reg_idx",           ctxp->reg_idx);
   MC_MRPL_ADD(mrpl, "reg_mask",          ctxp->reg_mask);
   MC_MRPL_ADD(mrpl, "reg_val",           ctxp->reg_val);
   if (ctxp->pre_delay_us)
      MC_MRPL_ADD(mrpl, "pre_delay_us",  ctxp->pre_delay_us);
   if (ctxp->post_delay_us)
      MC_MRPL_ADD(mrpl, "post_delay_us", ctxp->post_delay_us);
   if (PX14REGSET_CLKGEN == ctxp->reg_set)
   {
      MC_MRPL_ADD(mrpl, "cg_log_reg",    ctxp->cg_log_reg);
      MC_MRPL_ADD(mrpl, "cg_byte_idx",   ctxp->cg_byte_idx);
      MC_MRPL_ADD(mrpl, "cg_addr",       ctxp->cg_addr);
   }

   res = my_GenerateMethodRequest("PX14_REG_W", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - No output results

   return SIG_SUCCESS;
}

int rmc_PX14_DEVICE_REG_READ (HPX14 hBrd, PX14S_DEV_REG_READ* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DEV_REG_READ, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_DEV_REG_READ_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "struct_size",           ctxp->struct_size);
   MC_MRPL_ADD(mrpl, "reg_set",               ctxp->reg_set);
   MC_MRPL_ADD(mrpl, "reg_idx",               ctxp->reg_idx);
   if (ctxp->pre_delay_us)
      MC_MRPL_ADD(mrpl, "pre_delay_us",      ctxp->pre_delay_us);
   if (ctxp->post_delay_us)
      MC_MRPL_ADD(mrpl, "post_delay_us",     ctxp->post_delay_us);
   if (ctxp->read_how)
      MC_MRPL_ADD(mrpl, "read_how",          ctxp->read_how);
   if (PX14REGSET_CLKGEN == ctxp->reg_set)
   {
      MC_MRPL_ADD(mrpl, "cg_log_reg",        ctxp->cg_log_reg);
      MC_MRPL_ADD(mrpl, "cg_byte_idx",       ctxp->cg_byte_idx);
      MC_MRPL_ADD(mrpl, "cg_addr",           ctxp->cg_addr);
   }

   res = my_GenerateMethodRequest("PX14_REG_R", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   // For this IO, the driver places the register value in the struct_size
   //  field and that's all it returns

   MC_GET_RETVAL_REQ("struct_size",   ctxp->struct_size);

   return SIG_SUCCESS;
}

int rmc_PX14_NoParams (HPX14 hBrd, const char* method_namep)
{
   int res;

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   res = my_GenerateMethodRequest(method_namep, &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   return SendServiceRequestPX14(hBrd, strReq.c_str(),
                                 (int)strReq.length(), af_docp);
}

int rmc_PX14_TEMP_REG_IO (HPX14 hBrd, PX14S_RAW_REG_IO* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_RAW_REG_IO, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD_NON_ZERO(mrpl, "bRead",       ctxp->bRead);
   MC_MRPL_ADD_NON_ZERO(mrpl, "nRegion",     ctxp->nRegion);
   MC_MRPL_ADD         (mrpl, "nRegister",   ctxp->nRegister);
   if (!ctxp->bRead)
      MC_MRPL_ADD     (mrpl, "regVal",      ctxp->regVal);
   res = my_GenerateMethodRequest("PX14_TREGIO", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   MC_GET_RETVAL_REQ("regVal", ctxp->regVal);

   return SIG_SUCCESS;
}

int rmc_PX14_US_DELAY (HPX14 hBrd, int* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, int, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "us_delay", *ctxp);
   res = my_GenerateMethodRequest("PX14_US_DELAY", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - No output

   return SIG_SUCCESS;
}

int rmc_PX14_DRIVER_STATS (HPX14 hBrd, PX14S_DRIVER_STATS* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DRIVER_STATS, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_DRIVER_STATS_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "struct_size",  ctxp->struct_size);
   MC_MRPL_ADD(mrpl, "nConnections", ctxp->nConnections);
   res = my_GenerateMethodRequest("PX14_DSTATS", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   MC_GET_RETVAL_REQ("struct_size",      ctxp->struct_size);
   MC_GET_RETVAL_REQ("nConnections",     ctxp->nConnections);
   MC_GET_RETVAL_REQ("isr_cnt",          ctxp->isr_cnt);
   MC_GET_RETVAL_REQ("dma_finished_cnt", ctxp->dma_finished_cnt);
   MC_GET_RETVAL_REQ("dma_started_cnt",  ctxp->dma_started_cnt);
   MC_GET_RETVAL_REQ("acq_started_cnt",  ctxp->acq_started_cnt);
   MC_GET_RETVAL_REQ("acq_finished_cnt", ctxp->acq_finished_cnt);
   MC_GET_RETVAL_REQ("dma_bytes",        ctxp->dma_bytes);

   return SIG_SUCCESS;
}

int rmc_PX14_MODE_SET (HPX14 hBrd, int* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, int, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "op_mode", *ctxp);
   res = my_GenerateMethodRequest("PX14_MSET", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - No output

   return SIG_SUCCESS;
}

int rmc_PX14_GET_DEVICE_STATE (HPX14 hBrd, int* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, int, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   res = my_GenerateMethodRequest("PX14_GETSTATE", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   MC_GET_RETVAL_REQ("dev_state", *ctxp);

   return SIG_SUCCESS;
}

int rmc_PX14_WAIT_ACQ_OR_XFER (HPX14 hBrd, PX14S_WAIT_OP* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_WAIT_OP, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_WAIT_OP_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "struct_size", ctxp->struct_size);
   MC_MRPL_ADD(mrpl, "wait_op",     ctxp->wait_op);
   MC_MRPL_ADD(mrpl, "timeout_ms",  ctxp->timeout_ms);
   res = my_GenerateMethodRequest("PX14_WAITAX", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - No output

   return SIG_SUCCESS;
}

int rmc_PX14_FW_VERSIONS (HPX14 hBrd, PX14S_FW_VERSIONS* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_FW_VERSIONS, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_FW_VERSIONS_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "struct_size", ctxp->struct_size);
   res = my_GenerateMethodRequest("PX14_FWVER", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   MC_GET_RETVAL_REQ("struct_size",   ctxp->struct_size);
   MC_GET_RETVAL_REQ("fw_ver_pci",    ctxp->fw_ver_pci);
   MC_GET_RETVAL_REQ("fw_ver_sab",    ctxp->fw_ver_sab);
   MC_GET_RETVAL_REQ("fw_ver_pkg",    ctxp->fw_ver_pkg);
   MC_GET_RETVAL_REQ("pre_rel_flags", ctxp->pre_rel_flags);

   return SIG_SUCCESS;
}

int rmc_PX14_READ_TIMESTAMPS (HPX14 hBrd, PX14S_READ_TIMESTAMPS* ctxp)
{
   px14_timestamp_t* ts_bufp;
   int res, bytes_incoming;
   CStatePX14* statep;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_READ_TIMESTAMPS, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_READ_TIMESTAMPS_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD         (mrpl, "struct_size",  ctxp->struct_size);
   MC_MRPL_ADD         (mrpl, "buffer_items", ctxp->buffer_items);
   MC_MRPL_ADD_NON_ZERO(mrpl, "flags",        ctxp->flags);
   MC_MRPL_ADD_NON_ZERO(mrpl, "timeout_ms",   ctxp->timeout_ms);
   res = my_GenerateMethodRequest("PX14_READTS", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   MC_GET_RETVAL_REQ("struct_size",  ctxp->struct_size);
   MC_GET_RETVAL_REQ("buffer_items", ctxp->buffer_items);
   MC_GET_RETVAL_REQ("flags",        ctxp->flags);

   // Method implemented sends timestamp data as raw blob after response
   bytes_incoming = (int)(ctxp->buffer_items * sizeof(px14_timestamp_t));
   ts_bufp = *reinterpret_cast<px14_timestamp_t**>(&ctxp->user_virt_addr);
   statep = PX14_H2B(hBrd);
   res = my_socket_recv(statep->m_client_statep->m_sock,
                        reinterpret_cast<char*>(ts_bufp), bytes_incoming, 0);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

int rmc_PX14_NEED_DCM_RESET (HPX14 hBrd, int* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, int, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "bSet", *ctxp);
   res = my_GenerateMethodRequest("PX14_NDCMRST", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - No output

   return SIG_SUCCESS;
}

int rmc_PX14_GET_HW_CONFIG_EX (HPX14 hBrd, PX14S_HW_CONFIG_EX* ctxp)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_HW_CONFIG_EX, "ctxp");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_HW_CONFIG_EX_V1, "ctxp");

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "struct_size",  ctxp->struct_size);

   res = my_GenerateMethodRequest("PX14_GET_HWCFG_EX", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Extract results

   MC_GET_RETVAL_REQ("struct_size",   ctxp->struct_size);
   MC_GET_RETVAL_REQ("chan_imps",     ctxp->chan_imps);
   MC_GET_RETVAL_REQ("chan_filters",  ctxp->chan_filters);
   MC_GET_RETVAL_REQ("sys_fpga_type", ctxp->sys_fpga_type);
   MC_GET_RETVAL_REQ("sab_fpga_type", ctxp->sab_fpga_type);

   // Earlier library versions were not returning board revision. This is a
   //  bug and there's not much we can do about it now, just use some assumed
   //  defaults.
   MC_GET_RETVAL_OPT("board_rev",     ctxp->board_rev);
   if (SIG_SUCCESS != res)
      ctxp->board_rev = PX14BRDREV_1;
   MC_GET_RETVAL_OPT("board_rev_sub", ctxp->board_rev_sub);
   if (SIG_SUCCESS != res)
      ctxp->board_rev_sub = PX14BRDREVSUB_1_DR;

   if (ctxp->struct_size > _PX14SO_PX14S_HW_CONFIG_EX_V1)
   {
      MC_GET_RETVAL_REQ("custFwPkg", ctxp->custFwPkg);
      MC_GET_RETVAL_REQ("custFwPci", ctxp->custFwPci);
      MC_GET_RETVAL_REQ("custFwSab", ctxp->custFwSab);
      MC_GET_RETVAL_REQ("custHw",    ctxp->custHw);
   }

   return SIG_SUCCESS;
}

int rmc_ReadSampleRam (HPX14 hBrd,
                       unsigned int sample_start,
                       unsigned int sample_count,
                       px14_sample_t* bufp)
{
   unsigned int chunk_samples;
   px14_socket_t s;
   int res;

   GetServiceSocketPX14(hBrd, &s);

   while (sample_count)
   {
      chunk_samples = PX14_MIN(sample_count, PX14_MAX_REMOTE_CHUNK_SAMPS);

      // - Setup request

      MethodReqParamList mrpl;
      std::string strReq;

      MC_MRPL_ADD(mrpl, "sample_start", sample_start);
      MC_MRPL_ADD(mrpl, "sample_count", chunk_samples);
      res = my_GenerateMethodRequest("PX14_RAM_R", &mrpl, strReq);
      PX14_RETURN_ON_FAIL(res);

      // - Send request and wait for response

      CAutoFreeServiceResponse af_docp;
      res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                   (int)strReq.length(), af_docp);
      PX14_RETURN_ON_FAIL(res);

      // Nothing in reponse, but on success we'll have raw data sent

      res = my_socket_recv(s,
                           reinterpret_cast<char*>(bufp),
                           chunk_samples * sizeof(px14_sample_t), 0);
      PX14_RETURN_ON_FAIL(res);

      sample_count -= chunk_samples;
      sample_start += chunk_samples;
      bufp += chunk_samples;
   }

   return SIG_SUCCESS;
}

int rmc_CreateRecordingSession (HPX14 hBrd,
                                PX14S_REC_SESSION_PARAMS* ctxp,
                                HPX14RECORDING* handlep)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_REC_SESSION_PARAMS, "ctxp");
   PX14_ENSURE_POINTER(hBrd, handlep, HPX14RECORDING, "handlep");

   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_REC_SESSION_PARAMS_V1, "ctxp");
   if (ctxp->filwp)
      PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp->filwp, _PX14SO_FILE_WRITE_PARAMS_V1, "ctxp->filwp");

   CPX14SessionBase* newp;
   try { newp = new CPX14SessionBase(true); }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }
   std::auto_ptr<CPX14SessionBase> spHandle(newp);
   newp->m_hBrd = hBrd;

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD         (mrpl, "struct_size",    ctxp->struct_size);
   MC_MRPL_ADD_NON_ZERO(mrpl, "rec_flags",      ctxp->rec_flags);
   MC_MRPL_ADD_NON_ZERO(mrpl, "rec_samples",    ctxp->rec_samples);
   MC_MRPL_ADD_NON_ZERO(mrpl, "acq_samples",    ctxp->acq_samples);
   MC_MRPL_ADD_NON_ZERO(mrpl, "xfer_samples",   ctxp->xfer_samples);
   if (ctxp->filwp) _AppendMrpl_FilwParms(mrpl, ctxp->filwp);
   MC_MRPL_ADD_NON_ZERO(mrpl, "ss_len_samples", ctxp->ss_len_samples);
   MC_MRPL_ADD_NON_ZERO(mrpl, "ss_period_xfer", ctxp->ss_period_xfer);
   MC_MRPL_ADD_NON_ZERO(mrpl, "ss_period_ms",   ctxp->ss_period_ms);
   MC_MRPL_ADD_NON_ZERO(mrpl, "callbackData",   ctxp->callbackData);

   res = my_GenerateMethodRequest("PX14_REC_NEW", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Marshall output

   MC_GET_RETVAL_REQ("remote_handle", newp->m_remote_handle);

   *handlep = reinterpret_cast<HPX14RECORDING>(spHandle.release());
   return SIG_SUCCESS;
}

int rmc_AbortRecordingSession (HPX14RECORDING hRec)
{
   return rmc_RecSesHandleParamOnly(hRec,
                                    "PX14_REC_ABORT");
}

int rmc_ArmRecordingSession (HPX14RECORDING hRec)
{
   return rmc_RecSesHandleParamOnly(hRec,
                                    "PX14_REC_ARM");
}

int rmc_DeleteRecordingSession (HPX14RECORDING hRec)
{
   int res;

   res = rmc_RecSesHandleParamOnly(hRec, "PX14_REC_DEL");
   PX14_RETURN_ON_FAIL(res);

   // Free our local context structure
   CPX14SessionBase* ctxp = reinterpret_cast<CPX14SessionBase*>(hRec);
   delete ctxp;

   return SIG_SUCCESS;
}

int rmc_RecSesHandleParamOnly (HPX14RECORDING hRec, const char* namep)
{
   CPX14SessionBase* rec_ctxp;
   int res;

   rec_ctxp = reinterpret_cast<CPX14SessionBase*>(hRec);

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "remote_handle", rec_ctxp->m_remote_handle);

   res = my_GenerateMethodRequest(namep, &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   return SendServiceRequestPX14(rec_ctxp->m_hBrd, strReq.c_str(),
                                 (int)strReq.length(), af_docp);
}

int rmc_GetRecordingSessionOutFlags (HPX14RECORDING hRec,
                                     unsigned int* flagsp)
{
   CPX14SessionBase* rec_ctxp;
   int res;

   rec_ctxp = reinterpret_cast<CPX14SessionBase*>(hRec);

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "remote_handle", rec_ctxp->m_remote_handle);

   res = my_GenerateMethodRequest("PX14_REC_OFLAGS", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(rec_ctxp->m_hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Marshall output

   MC_GET_RETVAL_REQ("out_flags", *flagsp);

   return SIG_SUCCESS;
}

int rmc_GetRecordingSessionProgress (HPX14RECORDING hRec,
                                     PX14S_REC_SESSION_PROG* progp,
                                     unsigned flags)
{
   CPX14SessionBase* rec_ctxp;
   int res;

   rec_ctxp = reinterpret_cast<CPX14SessionBase*>(hRec);

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD         (mrpl, "remote_handle", rec_ctxp->m_remote_handle);
   MC_MRPL_ADD         (mrpl, "struct_size", sizeof(PX14S_REC_SESSION_PROG));
   MC_MRPL_ADD_NON_ZERO(mrpl, "flags", flags);

   res = my_GenerateMethodRequest("PX14_REC_PROG", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(rec_ctxp->m_hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Marshall output

   std::string errText;

   MC_GET_RETVAL_REQ("struct_size",      progp->struct_size);
   MC_GET_RETVAL_REQ("status",           progp->status);
   MC_GET_RETVAL_REQ("elapsed_time_ms",  progp->elapsed_time_ms);
   MC_GET_RETVAL_REQ("samps_recorded",   progp->samps_recorded);
   MC_GET_RETVAL_REQ("samps_to_record",  progp->samps_to_record);
   MC_GET_RETVAL_REQ("xfer_samples",     progp->xfer_samples);
   MC_GET_RETVAL_REQ("xfer_count",       progp->xfer_count);
   MC_GET_RETVAL_REQ("snapshot_counter", progp->snapshot_counter);

   if (progp->status == PX14RECSTAT_ERROR)
   {
      MC_GET_RETVAL_REQ("err_res", progp->err_res);
      MC_GET_RETVAL_OPT("err_textp", errText);
      if (SIG_SUCCESS == res)
      {
         res = AllocAndCopyString(errText.c_str(), &progp->err_textp);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   return SIG_SUCCESS;
}

int rmc_GetRecordingSnapshot (HPX14RECORDING hRec,
                              px14_sample_t* bufp,
                              unsigned int samples,
                              unsigned int* samples_gotp,
                              unsigned int* ss_countp)
{
   unsigned int ss_count_def, samples_got_def;
   CPX14SessionBase* rec_ctxp;
   px14_socket_t sock;
   int res;

   rec_ctxp = reinterpret_cast<CPX14SessionBase*>(hRec);
   if (NULL == samples_gotp) samples_gotp = &samples_got_def;
   if (NULL == ss_countp) ss_countp = &ss_count_def;

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "remote_handle", rec_ctxp->m_remote_handle);
   MC_MRPL_ADD(mrpl, "samples", samples);

   res = my_GenerateMethodRequest("PX14_REC_SS",
                                  &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(rec_ctxp->m_hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp);
   PX14_RETURN_ON_FAIL(res);

   // - Marshall output

   std::string errText;

   MC_GET_RETVAL_REQ("samples_got", *samples_gotp);
   MC_GET_RETVAL_REQ("ss_count",    *ss_countp);

   if (*samples_gotp)
   {
      GetServiceSocketPX14(rec_ctxp->m_hBrd, &sock);

      // Server sends snapshot data as binary blob after response
      res = my_socket_recv(sock, reinterpret_cast<char*>(bufp),
                           *samples_gotp * sizeof(px14_sample_t), 0);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

int rmc_SetPowerupDefaults (HPX14 hBrd)
{
   int res;

   // Apply the power-up defaults
   res = rmc_PX14_NoParams(hBrd, "PX14_SPUD");
   PX14_RETURN_ON_FAIL(res);

   // Refresh our register cache
   return RefreshLocalRegisterCachePX14(hBrd, PX14_FALSE);
}

int rmc_SetAdcClockSource (HPX14 hBrd, unsigned int val)
{
   int res;

   // - Setup request

   MethodReqParamList mrpl;
   std::string strReq;

   MC_MRPL_ADD(mrpl, "val", val);
   res = my_GenerateMethodRequest("PX14_CLKSRC", &mrpl, strReq);
   PX14_RETURN_ON_FAIL(res);

   // - Send request and wait for response

   CAutoFreeServiceResponse af_docp;
   res = SendServiceRequestPX14(hBrd, strReq.c_str(),
                                (int)strReq.length(), af_docp,
                                PX14_SERVER_REQ_TIMEOUT_DEF + PX14_DEF_PLL_LOCK_WAIT_MS);
   PX14_RETURN_ON_FAIL(res);

   // - No output, but need to update register cache.

   return RefreshLocalRegisterCachePX14(hBrd, PX14_FALSE);
}


