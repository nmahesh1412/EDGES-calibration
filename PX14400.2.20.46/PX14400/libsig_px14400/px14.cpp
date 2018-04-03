/** @file	px14.cpp
  @brief	Main PX14 library implementation

  Third-party libraries used to build this library:
Windows: pthreads-win32
*/
#include "stdafx.h"
#include "px14_top.h"
#include "px14_config.h"

// TODO : Look into using madvise for Linux

// Module-local function prototypes ------------------------------------- //

// Obtains current firmware versions from driver or EEPROM
static int CacheFwVersions (HPX14 hBrd);

// Cache hardware configuration info
static int GetHwCfgInfo (HPX14 hBrd, PX14S_HW_CONFIG_EX& cfg);

#ifdef _DEBUG
static void _CompileTimeChecks();
#endif

// CStatePX14 class implementation -------------------------------------- //

   CStatePX14::CStatePX14(dev_handle_t h, unsigned sn, unsigned on)
: m_magic(PX14_CTX_MAGIC), m_hDev(h), m_flags(PX14LIBF__INIT),
   m_serial_num(sn), m_ord_num(on), m_pll_defer_cnt(0), m_userData(NULL),
   m_features(0), m_chanImpCh1(0), m_chanImpCh2(0),
   m_chanFiltCh1(0), m_chanFiltCh2(0),
   m_fpgaTypeSys(0), m_fpgaTypeSab(0), m_boardRev(0), m_boardRevSub(0),
   m_custFwPkg(0), m_custFwPci(0), m_custFwSab(0),
   m_slave_delay_ns(2.5), m_driver_ver(0), m_hw_revision(0),
   m_fw_ver_pci(0), m_fw_ver_sab(0), m_fw_ver_pkg(0), m_fw_pre_rel(0),
   m_pfnSvcProc(NULL), m_virtual_statep(NULL), m_client_statep(NULL),
   m_utility_bufp(NULL), m_utility_buf_samples(0),
   m_utility_buf2p(NULL), m_utility_buf2_samples(0),
   m_utility_buf_chainp(NULL)
{
   memset (&m_regDev, 0, sizeof(m_regDev));

   memset (&m_regClkGen, 0, sizeof(m_regClkGen));

   memset (&m_regDriver, 0, sizeof(m_regDriver));

   // Default to PX14400 device driver
   m_pfnSvcProc = DriverRequest;
}

CStatePX14::~CStatePX14()
{
   SIGASSERT_NULL_OR_POINTER(m_client_statep, CRemoteCtxPX14);
   delete m_client_statep;

   SIGASSERT_NULL_OR_POINTER(m_virtual_statep, CVirtualCtxPX14);
   delete m_virtual_statep;

   if (m_hDev != PX14_BAD_DEV_HANDLE)
      ReleaseDeviceHandle(m_hDev);
}

/**
  Initialize this state item with data from given state. Previous
  content is igonred.

  All data is duplicated with the following excpetions:

  - A new device handle is obtained
  - Utility DMA buffer data is not duplicated
  */
int CStatePX14::DuplicateFrom (const CStatePX14& cpy)
{
   // We do not duplicate utility DMA buffer data
   m_utility_buf_samples = 0;
   m_utility_bufp = NULL;
   m_utility_buf2_samples = 0;
   m_utility_buf2p = NULL;
   m_utility_buf_chainp = NULL;
   m_pll_defer_cnt        = 0;			// Not copied

   // Duplicate all other members
   m_flags = cpy.m_flags & ~PX14LIBF__NEVER_COPY;
   m_features = cpy.m_features;
   m_chanImpCh1 = cpy.m_chanImpCh1;
   m_chanImpCh2 = cpy.m_chanImpCh2;
   m_chanFiltCh1 = cpy.m_chanFiltCh1;
   m_chanFiltCh2 = cpy.m_chanFiltCh2;
   m_fpgaTypeSys = cpy.m_fpgaTypeSys;
   m_fpgaTypeSab = cpy.m_fpgaTypeSab;
   m_boardRev = cpy.m_boardRev;
   m_boardRevSub = cpy.m_boardRevSub;
   m_custFwPkg = cpy.m_custFwPkg;
   m_custFwPci = cpy.m_custFwPci;
   m_custFwSab = cpy.m_custFwSab;
   m_userData = cpy.m_userData;

   m_driver_ver = cpy.m_driver_ver;
   m_hw_revision = cpy.m_hw_revision;
   m_fw_ver_pci = cpy.m_fw_ver_pci;
   m_fw_ver_sab = cpy.m_fw_ver_sab;
   m_fw_ver_pkg = cpy.m_fw_ver_pkg;
   m_fw_pre_rel = cpy.m_fw_pre_rel;

   // Duplicate register caches
   memcpy (&m_regDev, &cpy.m_regDev, sizeof(PX14U_DEVICE_REGISTER_SET));
   memcpy (&m_regClkGen, &cpy.m_regClkGen, sizeof(PX14U_CLKGEN_REGISTER_SET));
   memcpy (&m_regDriver, &cpy.m_regDriver, sizeof(PX14U_DRIVER_REGISTER_SET));

   return SIG_SUCCESS;
}

double CStatePX14::GetExtRateMHz() const
{
   unsigned long long rate_scaled;

   rate_scaled = (unsigned long long)m_regDriver.fields.dreg4.val << 32;
   rate_scaled |= m_regDriver.fields.dreg3.val;
   return rate_scaled / double(PX14_ACQ_CLK_DRV_SCALE);
}

void CStatePX14::SetExtRateMHz (double dRateMHz)
{
   unsigned long long rate_scaled;

   rate_scaled = static_cast<unsigned long long>(
                                                 dRateMHz * PX14_ACQ_CLK_DRV_SCALE);
   m_regDriver.fields.dreg4.val = static_cast<unsigned>(rate_scaled >> 32);
   m_regDriver.fields.dreg3.val = static_cast<unsigned>(rate_scaled);
}

double CStatePX14::GetIntAcqRateMHz() const
{
   unsigned long long rate_scaled;

   rate_scaled = (unsigned long long)m_regDriver.fields.dreg2.val << 32;
   rate_scaled |= m_regDriver.fields.dreg1.val;
   return rate_scaled / double(PX14_ACQ_CLK_DRV_SCALE);
}

void CStatePX14::SetIntAcqRateMHz (double dRateMHz)
{
   unsigned long long rate_scaled;

   rate_scaled = static_cast<unsigned long long>(
                                                 dRateMHz * PX14_ACQ_CLK_DRV_SCALE);
   m_regDriver.fields.dreg2.val = static_cast<unsigned>(rate_scaled >> 32);
   m_regDriver.fields.dreg1.val = static_cast<unsigned>(rate_scaled);
}

bool CStatePX14::IsDriverVerLessThan (unsigned maj,
                                      unsigned min,
                                      unsigned submin,
                                      unsigned package) const
{
   return IsDriverVerLessThan(PX14_VER64(maj, min, submin, package));
}

bool CStatePX14::IsDriverVerLessThan (const unsigned long long& ver) const
{
   return m_driver_ver < ver;
}

bool CStatePX14::IsDriverVerGreaterThan (unsigned maj,
                                         unsigned min,
                                         unsigned submin,
                                         unsigned package) const
{
   return IsDriverVerGreaterThan(PX14_VER64(maj, min, submin, package));
}

bool CStatePX14::IsDriverVerGreaterThan (const unsigned long long& ver) const
{
   return m_driver_ver > ver;
}

bool CStatePX14::IsChanAmplified (int chanIdx) const
{
   unsigned char ch;
   SIGASSERT ((chanIdx == 0) || (chanIdx == 1));
   ch = chanIdx ? m_chanImpCh2 : m_chanImpCh1;
   return 0 == (ch & PX14CHANIMPF_NON_AMPLIFIED);
}

// PX14 library exports implementation --------------------------------- //

/** @brief Obtain a count of all PX14 devices present in the system

  @return
  Returns a count of all physical PX14 devices present in the system
  */
PX14API GetDeviceCountPX14()
{
   dev_handle_t h;
   int i;

   // Count how many PX14 devices we can connect to
   for (i=0; i<PX14_MAX_DEVICES; i++)
   {
      if (PX14_BAD_DEV_HANDLE == (h = GetDeviceHandle(i)))
         break;
      ReleaseDeviceHandle(h);
   }

   return i;
}

/** @brief Establish a connection to a local PX14 device

  This function is used to establish a connection to a PX14 data
  acquisition device, represented by a PX14 device handle (HPX14).

  @note
WINDOWS: A PX14 device handle is not the same as, nor is it
compatible with, a Windows HANDLE object.

@param phDev
A pointer to a HPX14 variable that will receive the PX14 device
handle. This PX14 device handle is only valid in the current
process. This handle should be treated as an opaque object;
interpretation of the handle's value is PX14 library specific.
@param brdNum
This parameter is used to select which PX14 in the system to
connect to. If this value is in the range [1, PX14_MAX_DEVICES (32)]
then the number is assumed to be the ordinal number of the device in
the system. A board's ordinal represents the system-specific
enumeration of the device. This may or may not follow the order of
the physical PCIe slots. If this value is outside the
aforementioned range then it is assumed to be the PX14 serial
number of the device in which to connect.

@return
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.

*/
PX14API ConnectToDevicePX14 (HPX14* phDev, unsigned int brdNum)
{
   CStatePX14* statep;
   PX14S_DEVICE_ID dd;
   HPX14 hBrd;
   int res, i;

   SIGASSERT_POINTER(phDev, HPX14);
   if (!phDev)
      return SIG_INVALIDARG;

   // Allocate our device connection state
   try { statep = new CStatePX14(PX14_BAD_DEV_HANDLE, 0, 0); }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }
   // Auto free if we fail later...
   std::auto_ptr<CStatePX14> spBrd(statep);
   hBrd = PX14_B2H(statep);

   dd.struct_size = sizeof(PX14S_DEVICE_ID);

   if (brdNum && (brdNum < PX14_MAX_DEVICES))
   {
      // Opening device by ordinal number in sytem
      statep->m_hDev = GetDeviceHandle(brdNum - 1);

      if (PX14_BAD_DEV_HANDLE != statep->m_hDev)
      {
         // Ask driver for device ID info
         res = DeviceRequest(hBrd, IOCTL_PX14_GET_DEVICE_ID, &dd, 0, sizeof(dd));
         PX14_RETURN_ON_FAIL(res);
      }
   }
   else
   {
      // Opening device by serial number

      // Find it by looking for it
      for (i=0; i<PX14_MAX_DEVICES; i++)
      {
         statep->m_hDev = GetDeviceHandle(i);
         if (PX14_BAD_DEV_HANDLE == statep->m_hDev)
            break;

         // Ask driver for device ID info
         res = DeviceRequest(hBrd, IOCTL_PX14_GET_DEVICE_ID, &dd, 0, sizeof(dd));
         if ((SIG_SUCCESS == res) && (dd.serial_num == brdNum))
            break;		// Found it

         ReleaseDeviceHandle(statep->m_hDev);
      }
   }
   if (PX14_BAD_DEV_HANDLE == statep->m_hDev)
      return SIG_NOSUCHBOARD;

   // Cache hardware configuration
   statep->m_serial_num = dd.serial_num;
   statep->m_ord_num = dd.ord_num;
   statep->m_features = dd.feature_flags;

   // Cache the remainder of the configuration info
   res = CacheHwCfgEx(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // Cache driver version
   statep->m_driver_ver = 0;
   GetDriverVersionPX14(hBrd, &statep->m_driver_ver);
   statep->m_hw_revision = 0;
   GetHardwareRevisionPX14(hBrd, &statep->m_hw_revision);

   // Cache firmware version information
   res = CacheFwVersions(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // Grab current device register values from driver
   res = RefreshLocalRegisterCachePX14(hBrd);
   PX14_RETURN_ON_FAIL(res);

	//Set a different Pre-trigger mode for D2 (TODPT - Trigger occur during pre-tigger)
	if (statep->IsPX14400D2())
		SetTODPT_Enable(hBrd, TODPT_ENABLE);
	else
		SetTODPT_Enable(hBrd, TODPT_DISABLE);

   *phDev = PX14_B2H(spBrd.release());
   return SIG_SUCCESS;
}

/** @brief Establish a connection to a virtual (fake) PX14 device

  This function is used to establish a connection to a virtual PX14 data
  acquisition device. A virtual device is one that is not connected to any
  real PX14 hardware. Virtual devices are mainly used for software
  development and debugging.

  @param phDev
  A pointer to a HPX14 variable that will receive the virtual PX14
  device handle.
  @param serialNum
  The serial number to use for the virtual device. This can be any
  number; the PX14 library ignores this value.
  @param brdNum
  The board number to use for the virtual device. This can be any
  number; the PX14 library ignores this value.

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.
  */
PX14API ConnectToVirtualDevicePX14 (HPX14* phDev,
                                    unsigned int serialNum,
                                    unsigned int brdNum)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER (phDev, HPX14);
   if (NULL == phDev)
      return SIG_INVALIDARG;

   // Allocate our device connection state
   try
   {
      statep = new CStatePX14(PX14_BAD_DEV_HANDLE, serialNum, brdNum);
      statep->m_virtual_statep = new CVirtualCtxPX14;

      // Since we're virtual we don't have a real device driver so pass
      //  service requests to an emulated driver
      statep->m_pfnSvcProc = VirtualDriverRequest;
   }
   catch (std::bad_alloc) { return SIG_OUTOFMEMORY; }

   // Assume all features are present
   statep->m_features = 0xFFFFFFFF & ~PX14FEATURE_NO_INTERNAL_CLOCK;

   // Cache the remainder of the configuration info
   res = CacheHwCfgEx(PX14_B2H(statep));
   PX14_RETURN_ON_FAIL(res);

   statep->m_flags |= PX14LIBF_VIRTUAL;

   // Init device registers; uses same logic as when driver loads
   res = VirtualInitSwrCache(PX14_B2H(statep));
   PX14_RETURN_ON_FAIL(res);

   *phDev = PX14_B2H(statep);
   return SIG_SUCCESS;
}

/** @brief Closes a PX14 device handle

  This function will close the given PX14 device handle.

  Note that closing a PX14 device handle will have no effect on the
  underlying hardware. This allows one to connect/disconnect to a PX14
  device in an unobtrusive manner. A slight exception to this is that the
  PX14 driver may interact with the PX14 hardware when all connections
  to a particular PX14 (over all processes in the system) have been
  closed.

  @param hBrd
  The PX14 device handle to close. This handle ceases to be valid
  once this function returns successfully.

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.
  */
PX14API DisconnectFromDevicePX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   if (SIG_SUCCESS != (res = ValidateHandle(hBrd, &statep)))
      return res;

   // Free any utility DMA buffer that we may have allocated
   FreeUtilityDmaBufferPX14(hBrd);
   FreeUtilityDmaBuffer2PX14(hBrd);
   if (statep->m_utility_buf_chainp)
   {
      // This call will invalidate statep->m_utility_buf_chainp
      FreeDmaBufferChainPX14(hBrd, statep->m_utility_buf_chainp);
   }

   // If we're connected remotely, disconnect
   if (IsDeviceRemotePX14(hBrd) > 0)
      Remote_DisconnectFromService(*statep);

   // Disable magic in the event that someone tries to use the handle
   //  after we've released it
   statep->m_magic = ~statep->m_magic;
   delete statep;
   return SIG_SUCCESS;
}

/** @brief	Duplicate a PX14 device handle
*/
PX14API DuplicateHandlePX14 (HPX14 hBrd, HPX14* phNew)
{
   CStatePX14* statep, *newp;
   int res;

   SIGASSERT_POINTER(phNew, HPX14);
   if (NULL == phNew)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsRemote())
   {
      // We're duplicating a handle for a remote device
      SIGASSERT_POINTER(statep->m_client_statep, CRemoteCtxPX14);
      CRemoteCtxPX14& remCtx = *statep->m_client_statep;

      PX14S_REMOTE_CONNECT_CTXA connect_ctx;
      memset (&connect_ctx, 0, sizeof(PX14S_REMOTE_CONNECT_CTXA));
      connect_ctx.struct_size = sizeof(PX14S_REMOTE_CONNECT_CTXA);
      connect_ctx.port = remCtx.m_srvPort;
      connect_ctx.pServerAddress = remCtx.m_strSrvAddr.c_str();
      if (!remCtx.m_strAppName.empty())
         connect_ctx.pApplicationName = remCtx.m_strAppName.c_str();
      if (!remCtx.m_strSubSvcs.empty())
         connect_ctx.pSubServices = remCtx.m_strSubSvcs.c_str();

      if (statep->IsVirtual())
      {
         res = ConnectToRemoteVirtualDeviceAPX14(phNew,
                                                 statep->m_serial_num, statep->m_ord_num, &connect_ctx);
      }
      else
      {
         res = ConnectToRemoteDeviceAPX14(phNew, statep->m_serial_num,
                                          &connect_ctx);
      }
   }
   else if (statep->IsLocalVirtual())
   {
      // We're duplicating a handle for a local, virtual device
      res = ConnectToVirtualDevicePX14(phNew,
                                       statep->m_serial_num, statep->m_ord_num);
   }
   else
   {
      // We're duplicating a handle for a normal, local device
      res = ConnectToDevicePX14(phNew, statep->m_serial_num);
   }
   PX14_RETURN_ON_FAIL(res);

   newp = PX14_H2B(*phNew);
   // Duplicate from other device
   res = newp->DuplicateFrom(*statep);
   if (SIG_SUCCESS != res)
   {
      delete newp;
      return res;
   }

   *phNew = reinterpret_cast<HPX14>(newp);
   return SIG_SUCCESS;
}

/** @brief
  Obtain the serial number of the PX14 connected to the given handle

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param snp
  A pointer to the variable that will receive the PX14's serial
  number

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.
  */
PX14API GetSerialNumberPX14 (HPX14 hBrd, unsigned int* snp)
{
   CStatePX14* brdp;
   int res;

   SIGASSERT_NULL_OR_POINTER(snp, unsigned int);
   if (!snp)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &brdp);
   PX14_RETURN_ON_FAIL(res);

   *snp = brdp->m_serial_num;
   return SIG_SUCCESS;
}

/** @brief
  Obtain the ordinal number of the PX14 connected to the given handle

  A PX14's ordinal number is the number of the PX14 in the system, as in
  the first, second, third, etc board in the system. This order is determined
  by the system and may or may not be the same order as the physical PCI
  slots.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param onp
  A pointer to the variable that will receive the PX14's ordinal
  number

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.
  */
PX14API GetOrdinalNumberPX14 (HPX14 hBrd, unsigned int* onp)
{
   CStatePX14* brdp;
   int res;

   SIGASSERT_NULL_OR_POINTER(onp, unsigned int);
   if (!onp)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &brdp);
   PX14_RETURN_ON_FAIL(res);

   *onp = 1 + brdp->m_ord_num;
   return SIG_SUCCESS;
}

/** @brief Obtain board revision and/or sub-revision

  The board revision defines the major revision of the underlying device.
  Currently there are two supported revisions:
  - PX14BRDREV_PX14400 - PX14400 device: 2 channel 400MHz 14-bit ADC card
  - PX14BRDREV_PX12500 - PX12500 device: 2 channel 500MHz 12-bit ADC card

  In general, interpretation of the sub-revision depends on the board
  revision. However, for both currently defined board revisions, they have
  the same sub-revisions:
  - PX14BRDREVSUB_1_SP - SP (signal processor) device
  - PX14BRDREVSUB_1_DR - DR (data recorder) device

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param revp
  A pointer to the variable that will receive the revision of the
  device. This parameter may be NULL if the revision is not needed
  @param sub_revp
  A pointer to the variable that will receive the sub-revision of the
  device. This parameter may be NULL if the sub-revision is not needed

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.
  */
PX14API GetBoardRevisionPX14 (HPX14 hBrd,
                              unsigned int* revp,
                              unsigned int* sub_revp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(sub_revp, unsigned int);
   SIGASSERT_NULL_OR_POINTER(revp, unsigned int);

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (sub_revp) *sub_revp = statep->m_boardRevSub;
   if (revp) *revp         = statep->m_boardRev;

   return SIG_SUCCESS;
}

/** @brief Set user-defined data value associated with PX14 handle

  Each PX14 device connection contains a user-defined
  pointer-precision data value set aside for application usage. The
  interpretation of this data value is application defined and is ignored
  by the library. This data is initialized to zero when the connection is
  established. This connection is established by calling the
  ConnectToDevicePX14 (or ConnectToVirtualDevicePX14) library function.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param data
  User-defined data value

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.
  */
PX14API SetUserDataPX14 (HPX14 hBrd, void* data)
{
   CStatePX14* brdp;
   int res;

   if (SIG_SUCCESS != (res = ValidateHandle(hBrd, &brdp)))
      return res;

   brdp->m_userData = data;
   return SIG_SUCCESS;
}

/** @brief Obtain user-defined data value associated with PX14 handle

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param datap
  A pointer to the variable that will receive the user-defined data
  value associated with the given device handle.

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.

  @see SetUserDataPX14
  */
PX14API GetUserDataPX14 (HPX14 hBrd, void** datap)
{
   CStatePX14* brdp;
   int res;

   if (NULL == datap)
      return SIG_INVALIDARG;
   if (SIG_SUCCESS != (res = ValidateHandle(hBrd, &brdp)))
      return res;

   *datap = brdp->m_userData;
   return SIG_SUCCESS;
}

/** @brief Obtains the version of the PX14 driver

  The PX14 driver version is a 64-bit value broken up into four 16-bit
fields:
- Major version     (Mask = 0xFFFF000000000000ULL)
- Minor version     (Mask = 0x0000FFFF00000000ULL)
- Sub-minor version (Mask = 0x00000000FFFF0000ULL)
- Package number    (Mask = 0x000000000000FFFFULL)

@param hBrd
A handle to a PX14400 device obtained by calling ConnectToDevicePX14
@param verp
A pointer to the variable that will receive the PX14 driver version

@return
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.

@see GetLibVersionPX14
*/
PX14API GetDriverVersionPX14 (HPX14 hBrd, unsigned long long* verp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(verp, unsigned long long);
   if (NULL == verp)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!statep->IsLocalVirtual())
   {
      PX14S_DRIVER_VER dreq;

      // Ask driver for its version
      dreq.struct_size = sizeof(PX14S_DRIVER_VER);

      res = DeviceRequest(hBrd, IOCTL_PX14_DRIVER_VERSION,
                          reinterpret_cast<void*>(&dreq),
                          sizeof(PX14S_DRIVER_VER), sizeof(PX14S_DRIVER_VER));
      PX14_RETURN_ON_FAIL(res);

      *verp = dreq.version;
   }

   return SIG_SUCCESS;
}

/** @brief
  Determines if the given PX14 device handle is connected to a device

  A handle is valid if it is connected to a local, remote, or virtual
  PX14 device.

  @return
  Returns non-zero if given handle is connected to a PX14 (or
  virtual PX14) device or zero if given handle is not a valid PX14
  device handle.
  */
PX14API IsHandleValidPX14 (HPX14 hBrd)
{
   return (SIG_SUCCESS == ValidateHandle(hBrd, NULL)) ? 1 : 0;
}

/** @brief Determine if given handle is attached to a virtual PX14 device

  Use this function to determine if the given handle is associated with a
  virtual PX14 device. A virtual PX14 is not connected to real PX14
  hardware and is mainly used for software development and debugging.

  @param hBrd
  The PX14 device handle to check. A PX14 device handle is obtained
  by calling the ConnectToDevicePX14 or ConnectToVirtualDevicePX14
  function.

  @return
  Returns a SIG_* error value (< 0) on error. Returns zero if
  underlying device is not virtual or non-zero if underlying device is
  virtual.
  */
PX14API IsDeviceVirtualPX14 (HPX14 hBrd)
{
   CStatePX14* brdp;
   int res;

   if (SIG_SUCCESS != (res = ValidateHandle(hBrd, &brdp)))
      return res;

   return brdp->IsVirtual() ? 1 : 0;
}

/** @brief Determines if a given PX14 device handle is for a remote device

  @param hBrd
  The PX14 device handle to check. The ConnectToRemoteDevicePX14
  function is used to connect to a remote PX14 device.

  @return
  Returns a positive number if the given handle is a handle to a
  remote PX14, return 0 if the given handle is not a handle to a
  remote PX14, or a negative (SIG_*) value is returned on error.
  */
PX14API IsDeviceRemotePX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->m_flags & PX14LIBF_REMOTE_CONNECTED)
   {
      SIGASSERT_POINTER(statep->m_client_statep, CRemoteCtxPX14);
      if (statep->m_client_statep)
      {
         SIGASSERT(statep->m_client_statep->m_sock != PX14_INVALID_SOCKET);
         if (statep->m_client_statep->m_sock != PX14_INVALID_SOCKET)
            return 1;
      }
   }

   return 0;
}

/** @brief Obtain an element from the PX14 configuration EEPROM
*/
PX14API GetEepromDataPX14 (HPX14 hBrd,
                           unsigned int eeprom_addr,
                           unsigned short* eeprom_datap)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(eeprom_datap, unsigned short);
   if (!eeprom_datap)
      return SIG_INVALIDARG;
   if (eeprom_addr >= PX14_EEPROM_WORDS)
      return SIG_OUTOFBOUNDS;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // Ask the driver to perform EEPROM read
   PX14S_EEPROM_IO dreq;
   dreq.struct_size = sizeof(PX14S_EEPROM_IO);
   dreq.bRead = PX14_TRUE;
   dreq.eeprom_addr = eeprom_addr;

   res = DeviceRequest(hBrd, IOCTL_PX14_EEPROM_IO,
                       reinterpret_cast<void*>(&dreq),
                       sizeof(PX14S_EEPROM_IO), sizeof(PX14S_EEPROM_IO));
   PX14_RETURN_ON_FAIL(res);

   *eeprom_datap = dreq.eeprom_val;

   return SIG_SUCCESS;
}

PX14API SetEepromDataPX14 (HPX14 hBrd,
                           unsigned int eeprom_addr,
                           unsigned short eeprom_data)
{
   CStatePX14* statep;
   int res;

   if (eeprom_addr >= PX14_EEPROM_WORDS)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!statep->IsLocalVirtual())
   {
      PX14S_EEPROM_IO dreq;

      // Ask the driver to perform EEPROM write
      dreq.struct_size = sizeof(PX14S_EEPROM_IO);
      dreq.bRead = PX14_FALSE;
      dreq.eeprom_addr = eeprom_addr;
      dreq.eeprom_val = eeprom_data;

      res = DeviceRequest(hBrd, IOCTL_PX14_EEPROM_IO,
                          reinterpret_cast<void*>(&dreq),
                          sizeof(PX14S_EEPROM_IO), sizeof(PX14S_EEPROM_IO));
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

/** @brief Read an element from the PX14 configuration EEPROM
*/
PX14API ReadConfigEepromPX14 (HPX14 hBrd,
                              unsigned int eeprom_addr,
                              unsigned short* eeprom_datap)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (0 == (statep->m_flags & PX14LIBF_NO_PROTECT_EEPROM))
   {
      if ((eeprom_addr < PX14EA_USER_ADDR_MIN) ||
          (eeprom_addr > PX14EA_USER_ADDR_MAX))
      {
         return SIG_PX14_INVALID_ARG_2;
      }
   }

   return GetEepromDataPX14(hBrd, eeprom_addr, eeprom_datap);
}

/** @brief Write an element from the PX14 configuration EEPROM
*/
PX14API WriteConfigEepromPX14 (HPX14 hBrd,
                               unsigned int eeprom_addr,
                               unsigned short eeprom_data)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (0 == (statep->m_flags & PX14LIBF_NO_PROTECT_EEPROM))
   {
      if ((eeprom_addr < PX14EA_USER_ADDR_MIN) ||
          (eeprom_addr > PX14EA_USER_ADDR_MAX))
      {
         return SIG_PX14_INVALID_ARG_2;
      }
   }

   return SetEepromDataPX14(hBrd, eeprom_addr, eeprom_data);
}

/** @brief Recalculate and set configuration EEPROM checksum
*/
PX14API ResetEepromChecksumPX14 (HPX14 hBrd)
{
   unsigned short checksum, v, addr;
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!statep->IsVirtual())
   {
      // Recalculate checksum.
      for (checksum=0,addr=0; addr<PX14EA_CHECKSUM; addr++)
      {
         res = GetEepromDataPX14(hBrd, addr, &v);
         PX14_RETURN_ON_FAIL(res);

         checksum += v;
      }

      // Write the new checksum
      return SetEepromDataPX14(hBrd, PX14EA_CHECKSUM, checksum);
   }

   return SIG_SUCCESS;
}

PX14API RequestDbgReportPX14 (HPX14 hBrd, int report_type)
{
   // Send request to device
   PX14S_DBG_REPORT dreq;
   dreq.struct_size = sizeof(PX14S_DBG_REPORT);
   dreq.report_type = report_type;

   return DeviceRequest(hBrd, IOCTL_PX14_DBG_REPORT,
                        reinterpret_cast<void*>(&dreq), sizeof(PX14S_DBG_REPORT));
}

PX14API DmaTransferPX14 (HPX14 hBrd,
                         void* bufp,
                         unsigned int bytes,
                         int bRead,
                         int bAsynch)
{
   PX14S_DMA_XFER dreq;
   dreq.struct_size = sizeof(PX14S_DMA_XFER);
   dreq.xfer_bytes = bytes;
   dreq.virt_addr = reinterpret_cast<uintptr_t>(bufp);
   dreq.bRead = bRead;
   dreq.bAsynch = bAsynch;

   return DeviceRequest(hBrd, IOCTL_PX14_DMA_XFER,
                        &dreq, sizeof(PX14S_DMA_XFER));
}

PX14API EnableEepromProtectionPX14 (HPX14 hBrd, int bEnable)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (bEnable)
      statep->m_flags &= ~PX14LIBF_NO_PROTECT_EEPROM;
   else
      statep->m_flags |=  PX14LIBF_NO_PROTECT_EEPROM;

   return SIG_SUCCESS;
}

PX14API ValidateConfigEepromPX14 (HPX14 hBrd)
{
   unsigned short eeprom_data, checksum, addr;
   unsigned int sn, sn_eeprom;
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsLocalVirtual())
      return SIG_SUCCESS;

   // Check magic value
   res = GetEepromDataPX14(hBrd, PX14EA_MAGIC_NUM, &eeprom_data);
   PX14_RETURN_ON_FAIL(res);
   if (eeprom_data != PX14_EEPROM_MAGIC_NUMBER)
   {
      SetErrorExtra(hBrd, "Magic number mismatch");
      return SIG_PX14_CFG_EEPROM_VALIDATE_ERROR;
   }

   // Check checksum
   for (checksum=0,addr=0; addr<PX14EA_CHECKSUM; addr++)
   {
      res = GetEepromDataPX14(hBrd, addr, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      checksum += eeprom_data;
   }
   res = GetEepromDataPX14(hBrd, PX14EA_CHECKSUM, &eeprom_data);
   PX14_RETURN_ON_FAIL(res);
   if (eeprom_data != checksum)
   {
      SetErrorExtra(hBrd, "Checksum mismatch");
      return SIG_PX14_CFG_EEPROM_VALIDATE_ERROR;
   }

   // Check serial number if we have a good one
   GetSerialNumberPX14(hBrd, &sn);
   if ((sn != 0) && (sn != 0xFFFFFFFF))
   {
      res = GetEepromDataPX14(hBrd, PX14EA_SERIALNUM_LOWER, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      sn_eeprom = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_SERIALNUM_UPPER, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      sn_eeprom |= static_cast<unsigned>(eeprom_data) << 16;
      if (sn_eeprom != sn)
      {
         SetErrorExtra(hBrd, "Serial number mismatch");
         return SIG_PX14_CFG_EEPROM_VALIDATE_ERROR;
      }
   }

   return SIG_SUCCESS;
}

/** @brief Obtains the features implemented on given PX14
*/
PX14API GetBoardFeaturesPX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   return statep->m_features;
}

/** @brief Obtain the given channels hardware implementation (PX14CHANIMP_*)

  A channel's hardware implementation is not software selectable. There
  are currently two types of channel implementations:
  - PX14CHANIMP_AMPLIFIED - Channel is amplified and is configured with
  the SetInputVoltRangeCh1PX14 and SetInputVoltRangeCh2PX14 functions
  - PX14CHANIMP_
  */
PX14API GetChannelImplementationPX14 (HPX14 hBrd, unsigned int chan_idx)
{
   CStatePX14* statep;
   int res;

   if ((chan_idx != 0) && (chan_idx != 1))
      return SIG_OUTOFBOUNDS;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   return chan_idx ? statep->m_chanImpCh2 : statep->m_chanImpCh1;
}

/** @brief Obtain filter type installed for the given channel (PX14CHANFILTER_*)

  Channel filters are not software selectable.
  */
PX14API GetChannelFilterTypePX14 (HPX14 hBrd, unsigned int chan_idx)
{
   CStatePX14* statep;
   int res;

   if ((chan_idx != 0) && (chan_idx != 1))
      return SIG_OUTOFBOUNDS;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   return chan_idx ? statep->m_chanFiltCh2 : statep->m_chanFiltCh1;
}


// Note: This function is omitted from px14.h
PX14API GetRegSetPX14 (HPX14 hBrd, unsigned int reg_set, void** pp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(pp, char);
   if (NULL == pp)
      return SIG_PX14_INVALID_ARG_3;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   switch (reg_set)
   {
      case PX14REGSET_DEVICE:
         *pp = &statep->m_regDev;
         return SIG_SUCCESS;

      case PX14REGSET_CLKGEN:
         *pp = &statep->m_regClkGen;
         return SIG_SUCCESS;

      default: break;
   }

   return SIG_PX14_INVALID_ARG_2;
}

/** @brief De-interleave dual channel data into separate buffers

  When dual channel data is acquired on the PX14, sample data is
interleaved: Channel 1 Sample 1, Channel 2 Sample 1, Channel 1 Sample 2,
Channel 2 Sample 2, ... Use this function to extract out one or both
channels of data into separate buffers.

@param srcp
A pointer to a buffer containing interleaved dual-channel data
@param samples_in
The total number of samples contained in the buffer pointed to by
srcp
@param dst_ch1p
A pointer to a buffer that will receive channel 1 data. This
parameter may be NULL if channel 1 data isn't needed.
@param dst_ch2p
A pointer to a buffer that will receive channel 2 data. This
parameter may be NULL if channel 2 data isn't needed.

@return
Returns SIG_SUCCESS on success or one of the SIG_* error values on
error.
*/
PX14API DeInterleaveDataPX14 (const px14_sample_t* srcp,
                              unsigned int samples_in,
                              px14_sample_t* dst_ch1p,
                              px14_sample_t* dst_ch2p)
{
   unsigned int i, loops;

   // Validate parameters.
   SIGASSERT_NULL_OR_POINTER(dst_ch1p, px14_sample_t);
   SIGASSERT_NULL_OR_POINTER(dst_ch2p, px14_sample_t);
   SIGASSERT_POINTER(srcp, px14_sample_t);
   if (NULL == srcp)
      return SIG_PX14_INVALID_ARG_1;

   loops = samples_in >> 1;
   i = 0;

   // Does user want both channels?
   if (dst_ch1p && dst_ch2p)
   {
      for (; i<loops; i++)
      {
         *dst_ch1p++ = *srcp++;
         *dst_ch2p++ = *srcp++;
      }

      if (samples_in & 1)
         *dst_ch1p = *srcp;
   }
   // Does user want only channel 1?
   else if (dst_ch1p)
   {
      for (; i<loops; i++)
      {
         *dst_ch1p++ = *srcp;
         srcp += 2;
      }

      if (samples_in & 1)
         *dst_ch1p = *srcp;
   }
   // Does user want only channel 2?
   else if (dst_ch2p)
   {
      srcp++;		// Skip channel 1 value

      for (; i<loops; i++)
      {
         *dst_ch2p++ = *srcp;
         srcp += 2;
      }
   }

   return SIG_SUCCESS;
}

/** brief (Re)interleave dual channel data into a single buffer.

  This function may be used to (re)create interleaved dual channel data.

  @param src_ch1p
  A pointer to a buffer that contains channel 1 data. If this
  parameter is NULL then no data is copied over the channel 1 data
  samples in the destination buffer
  @param src_ch2p
  A pointer to a buffer that contains channel 2 data. If this
  parameter is NULL then no data is copied over the channel 2 data
  samples in the destination buffer
  @param samps_per_chan
  The number of samples contained in each of the input channel data
  buffers
  @param dstp
  A pointer to the buffer that will receive the interleaved data. This
  buffer must be at least (2 * samps_per_chan) samples in size

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.

  @see DeInterleaveDataPX14
  */
PX14API InterleaveDataPX14 (const px14_sample_t* src_ch1p,
                            const px14_sample_t* src_ch2p,
                            unsigned int samps_per_chan,
                            px14_sample_t* dstp)
{
   unsigned int i;

   SIGASSERT_POINTER(dstp, px14_sample_t);
   if (NULL == dstp)
      return SIG_PX14_INVALID_ARG_4;

   // Handle trivial cases.
   if (!samps_per_chan || (NULL==src_ch1p && NULL==src_ch2p))
      return SIG_SUCCESS;

   i = 0;

   if (src_ch1p && src_ch2p)
   {
      // -- Interleave both channels
      SIGASSERT_POINTER(src_ch1p, px14_sample_t);
      SIGASSERT_POINTER(src_ch2p, px14_sample_t);
      for (; i<samps_per_chan; i++)
      {
         *dstp++ = *src_ch1p++;
         *dstp++ = *src_ch2p++;
      }
   }
   else if (src_ch1p)
   {
      // -- Interleave channel 1 only
      SIGASSERT_POINTER(src_ch1p, px14_sample_t);
      for (; i<samps_per_chan; i++,dstp+=2)
         *dstp = *src_ch1p++;
   }
   else
   {
      dstp++;		// Skip channel 1 sample

      // -- Interleave channel 2 only
      SIGASSERT_POINTER(src_ch2p, px14_sample_t);
      for (; i<samps_per_chan; i++,dstp+=2)
         *dstp = *src_ch2p++;
   }

   return SIG_SUCCESS;
}

/** @brief	Free memory allocated by the PX14 library

  Certain PX14 library functions will allocate storage for items on
  behalf of the caller. The caller should use this function to return
  the memory to the system. Failure to free the memory may result in
  memory leaks.

  This function only needs to be used for items explicitly stated in
  the particular library function's documentation.
  */
PX14API FreeMemoryPX14 (void* p)
{
   SIGASSERT_NULL_OR_POINTER(p, unsigned char);
   my_free(p);

   return SIG_SUCCESS;
}

/** @brief
  Obtain a user-friendly string describing the given SIG_* error value

  This function is used to get a user-friendly string describing the
  given library error value. For ambiguous (and certain known) error
  values, information on the current (thread-specific) system error state
  is also provided.

  The PX14 library actually implements two versions of this function:
  GetErrorTextAPX14 that generates ASCII data and GetErrorTextWPX14 that
  generates UNICODE data. If _UNICODE is defined then GetErrorTextPX14 is
  defined as GetErrorTextWPX14 (UNICODE), else GetErrorTextAPX14 (ASCII) is
  defined.

  @param res
  The library error value for which to obtain information. This can
  be any (generic) SIG_*, or (PX14-specific) SIG_PX14_* error value.
  @param bufpp
  A pointer to a char pointer that will receive the address of the
  library allocated buffer containing the error text string. This
  buffer must be freed by caller by calling the FreeMemoryPX14 library
  function
  @param flags
  A set of flags (PX14ETF_*) that dictate function behavior.
  - PX14ETF_IGNORE_SYSERROR : Ignore system specific error
  information. (Windows: GetLastError, Linux: errno)
  - PX14ETF_NO_SYSERROR_TEXT : Do not generate any text for system
  specific error.
  - PX14ETF_FORCE_SYSERROR : Force inclusion of system error
  information even if it might not be relevant.
  @param hBrd
  A handle to the PX14 device for which the error occured. This
  parameter may be PX14_INVALID_HANDLE.

  @return
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.
  */
PX14API GetErrorTextAPX14 (int res,
                           char** bufpp,
                           unsigned int flags,
                           HPX14 hBrd)
{
   bool bReferLastError, bAppendLibXmlInfo, bAppendSocketInfo;
   bool bAppendRemoteInfo, bAppendExtraInfo;
   const char* pExtraInfoPreface;
   sys_error_t my_sys_err;

   // Grab this right away so no one else can overwrite it
   my_sys_err = GetSystemErrorVal();

   bAppendLibXmlInfo = bReferLastError = bAppendSocketInfo = false;
   bAppendRemoteInfo = bAppendExtraInfo = false;
   pExtraInfoPreface = NULL;

   SIGASSERT_POINTER(bufpp, char*);
   if (NULL == bufpp)
      return SIG_PX14_INVALID_ARG_2;

   std::ostringstream oss;

   // Note: @!@ will be replaced with underlying device
   //  revision name: e.g. PX14400 or PX12500. PX14400 will be used for
   //  unknown revision.

   switch (res)
   {
      // -- Generic Signatec errors
      case SIG_SUCCESS:
         oss << "No error"; break;
      case SIG_ERROR:
         oss << "Non-specific error"; bReferLastError = true; break;
      case SIG_INVALIDARG:
         oss << "Invalid argument"; break;
      case SIG_OUTOFBOUNDS:
         oss << "An argument was out of valid bounds"; break;
      case SIG_NODEV:
         oss << "Not attached to a PX14400 or PX12500 device"; break;
      case SIG_OUTOFMEMORY:
         oss << "Failed to allocate memory from heap"; break;
      case SIG_DMABUFALLOCFAIL:
         oss << "Failed to allocate DMA buffer"; break;
      case SIG_NOSUCHBOARD:
         oss << "Board with given serial number not found"; break;
      case SIG_NT_ONLY:
         oss << "This feature is only available on the Windows NT platform";
         break;
      case SIG_INVALID_MODE:
         oss << "Invalid operation for current operating mode"; break;
      case SIG_CANCELLED:
         oss << "Operation cancelled"; break;
      case SIG_PENDING:
         oss << "Operation is pending"; break;

         // -- PX14 specific errors
      case SIG_PX14_NOT_IMPLEMENTED:
         oss << "This operation is not currently implemented"; break;
      case SIG_PX14_INVALID_HANDLE:
         oss << "An invalid PX14400 or PX12500 device handle (HPX14) was specified";
         break;
      case SIG_PX14_BUFFER_TOO_SMALL:
         oss << "A specified buffer is too small"; break;
      case SIG_PX14_INVALID_ARG_1:
         oss << "Function argument #1 was invalid"; break;
      case SIG_PX14_INVALID_ARG_2:
         oss << "Function argument #2 was invalid"; break;
      case SIG_PX14_INVALID_ARG_3:
         oss << "Function argument #3 was invalid"; break;
      case SIG_PX14_INVALID_ARG_4:
         oss << "Function argument #4 was invalid"; break;
      case SIG_PX14_INVALID_ARG_5:
         oss << "Function argument #5 was invalid"; break;
      case SIG_PX14_INVALID_ARG_6:
         oss << "Function argument #6 was invalid"; break;
      case SIG_PX14_INVALID_ARG_7:
         oss << "Function argument #7 was invalid"; break;
      case SIG_PX14_INVALID_ARG_8:
         oss << "Function argument #8 was invalid"; break;
      case SIG_PX14_XFER_SIZE_TOO_SMALL:
         oss << "Requested transfer size is too small"; break;
      case SIG_PX14_XFER_SIZE_TOO_LARGE:
         oss << "Requested transfer size is too large"; break;
      case SIG_PX14_INVALID_DMA_ADDR:
         oss << "Given address is not part of a DMA buffer"; break;
      case SIG_PX14_WOULD_OVERRUN_BUFFER:
         oss << "Operation would overrun given buffer"; break;
      case SIG_PX14_BUSY:
         oss << "Device is busy; try again later"; break;
      case SIG_PX14_INVALID_CHAN_IMP:
         oss << "Incorrect function for channel's implementation"; break;
      case SIG_PX14_XML_MALFORMED:
         oss << "Malformed XML data was encountered";
         bAppendLibXmlInfo = true;
         break;
      case SIG_PX14_XML_INVALID:
         oss << "XML data was well formed, but not valid";
         bAppendLibXmlInfo = true;
         break;
      case SIG_PX14_XML_GENERIC:
         oss << "Generic XML releated error";
         bAppendLibXmlInfo = true;
         break;
      case SIG_PX14_RATE_TOO_FAST:
         oss << "The specified rate is too fast"; break;
      case SIG_PX14_RATE_TOO_SLOW:
         oss << "The specified rate is too slow"; break;
      case SIG_PX14_RATE_NOT_AVAILABLE:
         oss << "The specified frequency is not available"; break;
      case SIG_PX14_UNEXPECTED:
         oss << "An unexpected error occurred"; break;
      case SIG_PX14_SOCKET_ERROR:
         oss << "A socket error occurred";
         bAppendSocketInfo = true;
         break;
      case SIG_PX14_NETWORK_NOT_READY:
         oss << "Network subsystem is not ready for network ";
         oss << "communication"; break;
      case SIG_PX14_SOCKETS_TOO_MANY_TASKS:
         oss << "Limit on number of tasks/processes using sockets has been ";
         oss << "reached"; break;
      case SIG_PX14_SOCKETS_INIT_ERROR:
         oss << "Generic sockets implementation start up failure"; break;
      case SIG_PX14_NOT_REMOTE:
         oss << "Not connected to a remote @!@ device"; break;
      case SIG_PX14_TIMED_OUT:
         oss << "Operation timed out"; break;
      case SIG_PX14_CONNECTION_REFUSED:
         oss << "Connection refused by server; service may not be ";
         oss << "running"; break;
      case SIG_PX14_INVALID_CLIENT_REQUEST:
         oss << "Received an invalid client request"; break;
      case SIG_PX14_INVALID_SERVER_RESPONSE:
         oss << "Received an invalid server response"; break;
      case SIG_PX14_REMOTE_CALL_RETURNED_ERROR:
         oss << "Remote service call returned with an error";
         bAppendRemoteInfo = true;
         break;
      case SIG_PX14_UNKNOWN_REMOTE_METHOD:
         oss << "Undefined method invoked on remote server"; break;
      case SIG_PX14_SERVER_DISCONNECTED:
         oss << "Server closed the connection"; break;
      case SIG_PX14_REMOTE_CALL_NOT_AVAILABLE:
         oss << "Remote call for this operation is not implemented or ";
         oss << "available"; break;
      case SIG_PX14_UNKNOWN_FW_FILE:
         oss << "Unknown firmware file type"; break;
      case SIG_PX14_FIRMWARE_UPLOAD_FAILED:
         oss << "Firmware upload failed"; break;
      case SIG_PX14_INVALID_FW_FILE:
         oss << "Invalid firmware upload file"; break;
      case SIG_PX14_DEST_FILE_OPEN_FAILED:
         oss << "Failed to open destination file";
         bReferLastError = true;
         break;
      case SIG_PX14_SOURCE_FILE_OPEN_FAILED:
         oss << "Failed to open source file";
         bReferLastError = true;
         break;
      case SIG_PX14_FILE_IO_ERROR:
         oss << "File IO error";
         bReferLastError = true;
         break;
      case SIG_PX14_INCOMPATIBLE_FIRMWARE:
         oss << "Firmware is incompatible with @!@";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_UNKNOWN_STRUCT_SIZE:
         oss << "Unknown structure version specified to library function";
         pExtraInfoPreface = ", struct: ";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_INVALID_REGISTER:
         oss << "An invalid hardware register read/write was attempted";
         break;
      case SIG_PX14_FIFO_OVERFLOW:
         oss << "An internal FIFO overflowed during acquisition; couldn't ";
         oss << "keep up with acquisition data rate";
         break;
      case SIG_PX14_DCM_SYNC_FAILED:
         oss << "@!@ firmware could not synchronize with acquisition ";
         oss << "clock";
         break;
      case SIG_PX14_DISK_FULL:
         oss << "Could not write all data; disk is full";
         break;
      case SIG_PX14_INVALID_OBJECT_HANDLE:
         oss << "An invalid object handle was used";
         break;
      case SIG_PX14_THREAD_CREATE_FAILURE:
         oss << "Failed to create a thread";
         break;
      case SIG_PX14_PLL_LOCK_FAILED:
         oss << "Phase lock loop (PLL) failed to lock; clock may be bad";
         break;
      case SIG_PX14_THREAD_NOT_RESPONDING:
         oss << "Recording thread is not responding";
         break;
      case SIG_PX14_REC_SESSION_ERROR:
         oss << "A recording sesssion error occurred";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_REC_SESSION_CANNOT_ARM:
         oss << "Cannot arm recording sesssion; already armed or stopped";
         break;
      case SIG_PX14_SNAPSHOTS_NOT_ENABLED:
         oss << "Snapshots not enabled for given recording session";
         break;
      case SIG_PX14_SNAPSHOT_NOT_AVAILABLE:
         oss << "No data snapshot is available";
         break;
      case SIG_PX14_SRDC_FILE_ERROR:
         oss << "An error occurred while processing .srdc file";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_NAMED_ITEM_NOT_FOUND:
         oss << "Named item could not be found";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_CANNOT_FIND_SRDC_DATA:
         oss << "Could not find Signatec Recorded Data info";
         break;
      case SIG_PX14_NOT_IMPLEMENTED_IN_FIRMWARE:
         oss << "Feature or parameter is not implemented in current firmware ";
         oss << "version; upgrade firmware";
         break;
      case SIG_PX14_TIMESTAMP_FIFO_OVERFLOW:
         oss << "Timestamp FIFO overflowed during recording";
         break;
      case SIG_PX14_CANNOT_DETERMINE_FW_REQ:
         oss << "Cannot determine which firmware needs to be uploaded; ";
         oss << "update @!@ software";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_REQUIRED_FW_NOT_FOUND:
         oss << "Required firmware not found in firmware update file";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_FIRMWARE_IS_UP_TO_DATE:
         oss << "Loaded firmware is up to date with firmware update file";
         break;
      case SIG_PX14_NO_VIRTUAL_IMPLEMENTATION:
         oss << "Operation not implemented for virtual devices";
         break;
      case SIG_PX14_DEVICE_REMOVED:
         oss << "@!@ device has been removed from system";
         break;
      case SIG_PX14_PLL_LOCK_FAILED_UNSTABLE:
         oss << "Phase lock loop (PLL) failed to lock; lock not stable";
         break;
      case SIG_PX14_ALIGNMENT_ERROR:
         oss << "A parameter is not aligned properly";
         break;
      case SIG_PX14_JTAG_IO_ERROR:
         oss << "JTAG IO error";
         break;
      case SIG_PX14_UNREACHABLE:
         oss << "Item could not be set with the given constraints";
         break;
      case SIG_PX14_INVALID_MODE_CHANGE:
         oss << "Invalid operating mode change; all changes must go or come from Standby";
         break;
      case SIG_PX14_DEVICE_NOT_READY:
         oss << "Underlying device is not yet ready; try again shortly";
         break;
      case SIG_PX14_INVALID_OP_FOR_BRD_CONFIG:
         oss << "Invalid operation for current board configuration";
         break;
      case SIG_PX14_UNKNOWN_JTAG_CHAIN:
         oss << "Unknown JTAG chain configuration; out-of-date software or hardware error";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_SLAVE_FAILED_MASTER_SYNC:
         oss << "Slave device failed to synchronize to master device";
         break;
      case SIG_PX14_NOT_IMPLEMENTED_IN_DRIVER:
         oss << "Feature is not implemented in current driver version; upgrade software";
         break;
      case SIG_PX14_TEXT_CONV_ERROR:
         oss << "An error occurred while converting text from one encoding to another";
         break;
      case SIG_PX14_INVALIDARG_NULL_POINTER:
         oss << "An invalid pointer was specified";
         pExtraInfoPreface = " to function: ";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_INCORRECT_LOGIC_PACKAGE:
         oss << "Operation not available in current logic package";
         break;
      case SIG_PX14_CFG_EEPROM_VALIDATE_ERROR:
         oss << "Configuration EEPROM validation failed";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_RESOURCE_ALLOC_FAILURE:
         oss << "Failed to allocate or create a system object such as semaphore, mutex, etc";
         bReferLastError = true;
         break;
      case SIG_PX14_OPERATION_FAILED:
         oss << "The operation failed";
         bAppendExtraInfo = true;
         break;
      case SIG_PX14_BUFFER_CHECKED_OUT:
         oss << "Requested buffer is already checked out";
         break;
      case SIG_PX14_BUFFER_NOT_ALLOCATED:
         oss << "Requested buffer does not exist";
         break;

      case SIG_PX14_QUASI_SUCCESSFUL:
         oss << "Operation was quasi-successful; one or more items failed";
         break;

      default:
      {
         oss << "Unknown error (" << res << ")";
         bReferLastError = true;
         break;
      }
   }

   if (0 == (flags & PX14ETF_NO_SYSERROR_TEXT))
   {
#ifndef PX14PP_NO_XML_SUPPORT
      if (bAppendLibXmlInfo)
      {
         std::string sText;
         if (SIG_SUCCESS == GetLibXmlErrorText(sText))
            oss << " (libxml: " << sText.c_str() << ")";
      }
#endif

      if (bAppendSocketInfo)
      {
         std::string sText;
         if (SIG_SUCCESS == GetSocketsErrorText(sText))
            oss << " (sockets: " << sText.c_str() << ")";
      }

      if (bAppendRemoteInfo && (PX14_INVALID_HANDLE != hBrd))
      {
         CStatePX14* statep;
         int my_res;

         if ((SIG_SUCCESS == (my_res = ValidateHandle(hBrd, &statep))) &&
             (NULL != statep->m_client_statep) &&
             (!statep->m_client_statep->m_strSrvErr.empty()))
         {
            oss << " (SigSrv: "
               << statep->m_client_statep->m_strSrvErr.c_str() << ")";
         }
      }

      // Append system error information?
      if ((flags & PX14ETF_FORCE_SYSERROR) ||
          (bReferLastError && (0 == (flags & PX14ETF_IGNORE_SYSERROR))))
      {
         oss << " (" << PX14_SYS_ERR_NAME_A << "=" << my_sys_err;

         // Append system generated description of error?
         if (0 == (flags & PX14ETF_NO_SYSERROR_TEXT))
         {
            std::string str;
            if (GenSysErrorString(my_sys_err, str))
               oss << " : " << str.c_str();
         }

         oss << ')';
      }

      if (bAppendExtraInfo && (PX14_INVALID_HANDLE != hBrd))
      {
         CStatePX14* statep;
         int my_res;

         my_res = ValidateHandle(hBrd, &statep);
         if ((SIG_SUCCESS == my_res) && !statep->m_err_extra.empty())
         {
            if (pExtraInfoPreface)
               oss << pExtraInfoPreface;
            else
               oss << ": ";

            oss << statep->m_err_extra.c_str();
         }
      }
   }

   std::string str(oss.str());

   // Replace instances of @!@ with underlying device name
   static const char* rep_text = "@!@";
   std::string::size_type pos = str.find(rep_text);
   if (pos != std::string::npos)
   {
      unsigned int brd_rev;

      std::string sRevName;
      brd_rev = PX14BRDREV_PX14400;
      if (PX14_INVALID_HANDLE != hBrd)
         GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
      if (!GetBoardRevisionStr(brd_rev, sRevName))
         sRevName.assign(PX14_REV1_NAMEA);

      do
      {
         str.replace(pos, strlen(rep_text), sRevName);
         pos = str.find(rep_text, pos);
      }
      while (pos != std::string::npos);
   }

   return AllocAndCopyString(str.c_str(), bufpp);
}

/// Obtain PX14 driver/device statistics
PX14API GetDriverStatsPX14 (HPX14 hBrd,
                            PX14S_DRIVER_STATS* statsp,
                            int bReset)
{
   SIGASSERT_POINTER(statsp, PX14S_DRIVER_STATS);
   if (NULL == statsp)
      return SIG_PX14_INVALID_ARG_2;
   if (statsp->struct_size > 512)
      return SIG_PX14_INVALID_ARG_2;

   // For input the nConnections member is our reset stats flag
   statsp->nConnections = bReset;

   return DeviceRequest(hBrd, IOCTL_PX14_DRIVER_STATS,
                        statsp, statsp->struct_size, statsp->struct_size);
}

// Obtain user-friendly name for given board (ASCII)
PX14API GetBoardNameAPX14 (HPX14 hBrd, char** bufpp, int flags)
{
   CStatePX14* statep;
   int res;

   PX14_ENSURE_POINTER(hBrd, bufpp, char*, "GetBoardNamePX14");

   if (PX14_INVALID_HANDLE == hBrd)
      return AllocAndCopyString(PX14_REV1_NAMEA, bufpp);

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   std::ostringstream oss;

   if ((flags & PX14GBNF_INC_VIRT_STATUS) && IsDeviceVirtualPX14(hBrd))
      oss << "Virtual ";

   if ((flags & PX14GBNF_INC_REMOTE_STATUS) && IsDeviceRemotePX14(hBrd))
      oss << "Remote ";

   // Master/slave status
   if (flags & PX14GBNF_INC_MSCFG)
   {
      int mscfg = GetMasterSlaveConfigurationPX14(hBrd);
      if (PX14_IS_MASTER(mscfg))
         oss << "Master ";
      else if (PX14_IS_SLAVE(mscfg))
         oss << "Slave ";
   }

   // Main board name
   PX14_CT_ASSERT(PX14BRDREV__COUNT == 4);
   switch(statep->m_boardRev)
   {
      default:
      case PX14BRDREV_PX14400:  oss << PX14_REV1_NAMEA; break;
      case PX14BRDREV_PX12500:  oss << PX14_REV2_NAMEA; break;
      case PX14BRDREV_PX14400D: oss << PX14_REV3_NAMEA; break;
      case PX14BRDREV_PX14400D2:oss << PX14_REV4_NAMEA; break;
   }

   //if (flags & PX14GBNF_INC_CHAN_COUPLING)
   //{
   //		bool bCh1DC, bCh2DC;

   //		bCh1DC = 0 != (statep->m_chanImpCh1 & PX14CHANIMPF_DC_COUPLED);
   //		bCh2DC = 0 != (statep->m_chanImpCh2 & PX14CHANIMPF_DC_COUPLED);

   // AC- or DC-coupling, "A" or "D" suffix respectively
   //		oss << (bCh1DC ? "D" : "A");
   //		if (bCh1DC != bCh2DC)
   //			oss << (bCh2DC ? "D" : "A");
   //}

   if (flags & PX14GBNF_INC_SUB_REVISION)
   {
      PX14_CT_ASSERT(PX14BRDREV__COUNT == 4);

      if ((statep->m_boardRev == PX14BRDREV_PX14400) ||
          (statep->m_boardRev == PX14BRDREV_PX14400D) ||
          (statep->m_boardRev == PX14BRDREV_PX14400D2) ||
          (statep->m_boardRev == PX14BRDREV_PX12500))
      {
         if (statep->m_boardRevSub == PX14BRDREVSUB_1_DR)
         {
            // Data recorder
            oss << " DR";
         }
         else if (statep->m_boardRevSub == PX14BRDREVSUB_1_SP)
         {
            // Signal processor
            oss << " SP";
            switch (statep->m_fpgaTypeSab)
            {
               case PX14SABFPGA_V5_SX50T: oss << "50"; break;
               case PX14SABFPGA_V5_SX95T: oss << "95"; break;
               default: break;
            }
         }
      }
   }

   if (flags & PX14GBNF_INC_CHAN_AMP)
   {
      // Transformer selected = non-amplified
      if (statep->m_chanImpCh1 & PX14CHANIMPF_NON_AMPLIFIED)
         oss << " XF";
   }

   // Board serial or ordinal number
   if (0 == (flags & PX14GBNF_NO_SN))
   {
      unsigned sn;

      if (flags & PX14GBNF_USE_ORD_NUM)
         res = GetOrdinalNumberPX14(hBrd, &sn);
      else
         res = GetSerialNumberPX14(hBrd, &sn);
      PX14_RETURN_ON_FAIL(res);

      oss << ' ';

      if (0 == (flags & PX14GBNF_ALPHANUMERIC_ONLY))
         oss << '#';

      oss << sn;
   }

   std::string s(oss.str());

   if (flags & PX14GBNF_USE_UNDERSCORES)
   {
      std::string::size_type pos = 0;
      while ((pos = s.find(' ', pos)) != std::string::npos)
         s[pos] = '_';
   }

   return AllocAndCopyString(s.c_str(), bufpp);
}

// Dump library error description to given file or stream (ASCII)
PX14API DumpLibErrorAPX14 (int res,
                           const char* pPreamble,
                           HPX14 hBrd,
                           FILE* filp)
{
   char* pErr;
   int my_res;

   SIGASSERT_NULL_OR_POINTER(pPreamble, char);
   SIGASSERT_NULL_OR_POINTER(filp, FILE);

   if (NULL == filp)
      filp = stderr;

   if (pPreamble)
   {
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
      fprintf_s (filp, "%s", pPreamble);
#else
      fprintf (filp, "%s", pPreamble);
#endif
   }

   if (IsHandleValidPX14(hBrd))
   {
      char* brd_name;

      my_res = GetBoardNameAPX14(hBrd, &brd_name, PX14GBNF__DETAILED);
      if ((SIG_SUCCESS == my_res) && brd_name)
      {
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
         fprintf_s(filp, "(Device:%s) ", brd_name);
#else
         fprintf(filp, "(Device:%s) ", brd_name);
#endif
         FreeMemoryPX14(brd_name);
      }
   }

   pErr = NULL;
   my_res = GetErrorTextAPX14(res, &pErr, 0, hBrd);
   if ((SIG_SUCCESS == my_res) && pErr)
   {
#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
      fprintf_s (filp, "%s\n", pErr);
#else
      fprintf (filp, "%s\n", pErr);
#endif
      FreeMemoryPX14(pErr);
   }

   return SIG_SUCCESS;
}

PX14API CheckFeaturePX14 (HPX14 hBrd, unsigned int feature)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (feature == (statep->m_features & feature))
      return SIG_SUCCESS;

   return SIG_PX14_INVALID_OP_FOR_BRD_CONFIG;
}

PX14API StallThreadMicroSecsPX14 (HPX14 hBrd, unsigned int microseconds)
{
   return DeviceRequest(hBrd, IOCTL_PX14_US_DELAY, &microseconds, 4);
}
// Module private function implementation ------------------------------- //

#ifdef _DEBUG
/// This function does nothing but make some compile time assertions
void _CompileTimeChecks()
{
   PX14_CT_ASSERT(PX14_BOARD_TYPE == 0x31);

   // -- Make sure we really know our structure sizes

   PX14_CT_ASSERT(_PX14SO_PX14S_DEVICE_ID_V1 ==
                  sizeof(PX14S_DEVICE_ID));
   PX14_CT_ASSERT(_PX14SO_PX14S_DRIVER_STATS_V1 ==
                  sizeof(PX14S_DRIVER_STATS));
   PX14_CT_ASSERT(_PX14SO_PX14S_JTAGIO_V1 ==
                  sizeof(PX14S_JTAGIO));
   PX14_CT_ASSERT(_PX14SO_PX14S_JTAG_STREAM_V1 ==
                  sizeof(PX14S_JTAG_STREAM));
   PX14_CT_ASSERT(_PX14SO_PX14_DRIVER_VER_V1 ==
                  sizeof(PX14S_DRIVER_VER));
   PX14_CT_ASSERT(_PX14SO_DMA_BUFFER_ALLOC_V1 ==
                  sizeof(PX14S_DMA_BUFFER_ALLOC));
   PX14_CT_ASSERT(_PX14SO_DMA_BUFFER_FREE_V1 ==
                  sizeof(PX14S_DMA_BUFFER_FREE));
   PX14_CT_ASSERT(_PX14SO_EEPROM_IO_V1 ==
                  sizeof(PX14S_EEPROM_IO));
   PX14_CT_ASSERT(_PX14SO_DBG_REPORT_V1 ==
                  sizeof(PX14S_DBG_REPORT));
   PX14_CT_ASSERT(_PX14SO_DEV_REG_WRITE_V1 ==
                  sizeof(PX14S_DEV_REG_WRITE));
   PX14_CT_ASSERT(_PX14SO_DEV_REG_READ_V1 ==
                  sizeof(PX14S_DEV_REG_READ));
   PX14_CT_ASSERT(_PX14SO_DMA_XFER_V1 ==
                  sizeof(PX14S_DMA_XFER));
   PX14_CT_ASSERT(_PX14SO_PX14S_DRIVER_STATS_V1 ==
                  sizeof(PX14S_DRIVER_STATS));
   PX14_CT_ASSERT(_PX14SO_WAIT_OP_V1 ==
                  sizeof(PX14S_WAIT_OP));
   PX14_CT_ASSERT(_PX14SO_DRIVER_BUFFERED_XFER_V1 ==
                  sizeof(PX14S_DRIVER_BUFFERED_XFER));
   PX14_CT_ASSERT(_PX14SO_PX14S_READ_TIMESTAMPS_V1 ==
                  sizeof(PX14S_READ_TIMESTAMPS));
   PX14_CT_ASSERT(_PX14SO_PX14S_FW_VERSIONS_V1 ==
                  sizeof(PX14S_FW_VERSIONS));
   PX14_CT_ASSERT(_PX14SO_PX14S_BOOTBUF_CTRL_V1 >= sizeof(PX14S_BOOTBUF_CTRL));

   PX14_CT_ASSERT(_PX14SO_RECORDED_DATA_INFO_V2 ==
                  sizeof(PX14S_RECORDED_DATA_INFO));
   PX14_CT_ASSERT(_PX14SO_REC_SESSION_PROG_V1 ==
                  sizeof(PX14S_REC_SESSION_PROG));
   PX14_CT_ASSERT(_PX14SO_REC_SESSION_PARAMS_V1 ==
                  sizeof(PX14S_REC_SESSION_PARAMS));
   PX14_CT_ASSERT(_PX14SO_FILE_WRITE_PARAMS_V1 ==
                  sizeof(PX14S_FILE_WRITE_PARAMS));
   PX14_CT_ASSERT(_PX14SO_FW_VER_INFO_V1 ==
                  sizeof(PX14S_FW_VER_INFO));
   PX14_CT_ASSERT(_PX14SO_PX14S_HW_CONFIG_EX_V2 ==
                  sizeof(PX14S_HW_CONFIG_EX));

   // All PX14 device registers are 32-bits wide

   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_00));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_01));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_02));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_03));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_04));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_05));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_06));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_07));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_08));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_09));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_0A));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_0B));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_0C));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_0D));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_0E));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_0F));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_10));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_DEV_11));
   PX14_CT_ASSERT(18 == PX14_DEVICE_REG_COUNT);

   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CFG_00));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CFG_01));
   PX14_CT_ASSERT(2 == PX14_CONFIG_REG_COUNT);

   // All logical clock gen register mappings are 32-bits wide
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_00));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_01));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_02));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_03));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_04));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_05));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_06));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_07));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_08));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_09));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_10));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_11));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_12));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_13));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_14));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_15));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_16));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_17));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_18));
   PX14_CT_ASSERT(4 == sizeof(PX14S_REG_CLK_LOG_19));
   PX14_CT_ASSERT(20 == PX14_CLOCK_REG_COUNT);

   // All driver (software) registers are 32-bits wide
   PX14_CT_ASSERT(4 == sizeof(PX14S_DREG_00));
   PX14_CT_ASSERT(4 == sizeof(PX14S_DREG_01));
   PX14_CT_ASSERT(4 == sizeof(PX14S_DREG_02));
   PX14_CT_ASSERT(4 == sizeof(PX14S_DREG_03));
   PX14_CT_ASSERT(4 == sizeof(PX14S_DREG_04));
   PX14_CT_ASSERT(4 == sizeof(PX14S_DREG_05));
   PX14_CT_ASSERT(6 == PX14_DRIVER_REG_COUNT);
}
#endif

/** @brief Convert a PX14 device handle to a device state pointer

  Most all library functions invoke this routine to validate an input
  PX14 device handle.
  */
int ValidateHandle (HPX14 hBrd, CStatePX14** brdpp)
{
   CStatePX14* brdp = PX14_H2B(hBrd);
   if (!brdp || (brdp->m_magic != PX14_CTX_MAGIC))
      return SIG_PX14_INVALID_HANDLE;
   if (brdpp) *brdpp = brdp;
   return SIG_SUCCESS;
}

int DeviceRequest (HPX14 hBrd,
                   io_req_t req,
                   void* p,
                   size_t in_bytes,
                   size_t out_bytes)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(reinterpret_cast<HPX14>(hBrd), &statep);
   PX14_RETURN_ON_FAIL(res);

   return (*statep->m_pfnSvcProc)(hBrd, req, p, in_bytes, out_bytes);
}

int CacheFwVersions (HPX14 hBrd)
{
   PX14S_FW_VERSIONS fwv;
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   memset (&fwv, 0, sizeof(PX14S_FW_VERSIONS));
   fwv.struct_size = sizeof(PX14S_FW_VERSIONS);

   res = DeviceRequest(hBrd, IOCTL_PX14_FW_VERSIONS, &fwv,
                       sizeof(unsigned int), sizeof(PX14S_FW_VERSIONS));
   PX14_RETURN_ON_FAIL(res);

   statep->m_fw_ver_pci = fwv.fw_ver_pci;
   statep->m_fw_ver_sab = fwv.fw_ver_sab;
   statep->m_fw_ver_pkg = fwv.fw_ver_pkg;
   statep->m_fw_pre_rel = fwv.pre_rel_flags;

   return SIG_SUCCESS;
}

int SetErrorExtra (HPX14 hBrd, const char* err_extrap)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(err_extrap, char);

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   try
   {
      if (err_extrap)
         statep->m_err_extra.assign(err_extrap);
      else
         statep->m_err_extra.clear();
   }
   catch (std::bad_alloc)
   {
      return SIG_OUTOFMEMORY;
   }

   return SIG_SUCCESS;
}

/// std::string wrapper for GetErrorTextAPX14; not exported
int GetErrorTextStringPX14 (int res,
                            std::string& s,
                            unsigned int flags,
                            HPX14 hBrd)
{
   char* bufp = NULL;
   int my_res;

   my_res = GetErrorTextAPX14(res, &bufp, flags, hBrd);
   if (SIG_SUCCESS == my_res)
   {
      if (bufp)
      {
         s.assign(bufp);
         FreeMemoryPX14(bufp);
      }
      else
         s.clear();
   }

   return my_res;
}

int CacheHwCfgEx (HPX14 hBrd)
{
   PX14S_HW_CONFIG_EX cfgEx;
   CStatePX14* statep;
   int res;

   res = GetHwCfgInfo(hBrd, cfgEx);
   PX14_RETURN_ON_FAIL(res);

   statep = PX14_H2B(hBrd);
   statep->m_chanImpCh1  =  cfgEx.chan_imps & 0xFF;
   statep->m_chanImpCh2  = (cfgEx.chan_imps >> 8 ) & 0xFF;
   statep->m_chanFiltCh1 =  cfgEx.chan_filters & 0xFF;
   statep->m_chanFiltCh2 = (cfgEx.chan_filters >> 8) & 0xFF;
   statep->m_fpgaTypeSys = cfgEx.sys_fpga_type;
   statep->m_fpgaTypeSab = cfgEx.sab_fpga_type;
   statep->m_boardRev    = cfgEx.board_rev;
   statep->m_boardRevSub = cfgEx.board_rev_sub;
   statep->m_custFwPkg   = cfgEx.custFwPkg;
   statep->m_custFwPci   = cfgEx.custFwPci;
   statep->m_custFwSab   = cfgEx.custFwSab;
   statep->m_custHw      = cfgEx.custHw;

   return SIG_SUCCESS;
}

int GetHwCfgInfo (HPX14 hBrd, PX14S_HW_CONFIG_EX& cfg)
{
   unsigned short eeprom_data;
   unsigned struct_size_in;
   int res;

   // Ask driver for config info
   memset (&cfg, 0, sizeof(PX14S_HW_CONFIG_EX));
   cfg.struct_size = sizeof(PX14S_HW_CONFIG_EX);
   struct_size_in = cfg.struct_size;

   res = DeviceRequest(hBrd, IOCTL_PX14_GET_HW_CONFIG_EX, &cfg,
                       4, struct_size_in);
   if ((SIG_SUCCESS == res) && (cfg.struct_size == struct_size_in))
      return SIG_SUCCESS;			// Driver fully handled request
   if (SIG_SUCCESS != res)
      cfg.struct_size = 0;

   if (cfg.struct_size < _PX14SO_PX14S_HW_CONFIG_EX_V1)
   {
      // Older drivers do not implement this request. We can ask the EEPROM
      //  directly

      res = GetEepromDataPX14(hBrd, PX14EA_CHANNEL_IMP, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.chan_imps = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_CHANFILTERTYPES, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.chan_filters = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_SYS_FPGA_TYPE, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.sys_fpga_type = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_SAB_FPGA_TYPE, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.sab_fpga_type = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_BOARD_REV, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.board_rev = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_BOARD_REV_SUB, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.board_rev_sub = eeprom_data;
   }

   // Future: It is possible that PX14S_HW_CONFIG_EX can be extended in lib,
   //  but not in the driver. The driver will always return the it's known
   //  structure size in the cfg.struct_size. If it returns success and
   //  cfg.struct_size < struct_size_in then we'll need to manually obtain
   //  the difference

   if (cfg.struct_size < _PX14SO_PX14S_HW_CONFIG_EX_V2)
   {
      res = GetEepromDataPX14(hBrd, PX14EA_CUST_FWPKG_ENUM, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.custFwPkg = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_CUSTOM_LOGIC_ENUM, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.custFwPci = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_CUSTOM_SAB_LOGIC_ENUM, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.custFwSab = eeprom_data;

      res = GetEepromDataPX14(hBrd, PX14EA_CUSTOM_HW_ENUM, &eeprom_data);
      PX14_RETURN_ON_FAIL(res);
      cfg.custHw = eeprom_data;
   }

   return SIG_SUCCESS;
}

bool GetBoardRevisionStr (int brd_rev, std::string& s)
{
   switch (brd_rev)
   {
      case PX14BRDREV_PX14400:  s.assign(PX14_REV1_NAMEA); return true;
      case PX14BRDREV_PX12500:  s.assign(PX14_REV2_NAMEA); return true;
      case PX14BRDREV_PX14400D: s.assign(PX14_REV3_NAMEA); return true;
      case PX14BRDREV_PX14400D2: s.assign(PX14_REV4_NAMEA); return true;
      default:break;
   }
   PX14_CT_ASSERT(PX14BRDREV__COUNT == 4);

   std::ostringstream oss;
   oss << "<Unknown revision:" << brd_rev << ">";
   s = oss.str();
   return false;
}

