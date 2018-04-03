/** @file	px14_hw_set.cpp
  @brief	Main PX14 library implementation
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"
#include "px14_remote.h"
#include "px14_config.h"

// Module-local function prototypes ------------------------------------- //

static int _SetSabModePX14 (HPX14 hBrd, unsigned int val);
//static int _GetSabModePX14 (HPX14 hBrd, int bFromCache = PX14_TRUE);
static int _PostSabOp (HPX14 hBrd, int op_mode_sab);
static int _PreSabOp (HPX14 hBrd, int op_mode_sab);

static int InitRegSetDevice (HPX14 hBrd);

static int _InitializeDcOffsetDac (HPX14 hBrd);

// PX14 library exports implementation --------------------------------- //

/** Restores all PX14 settings to power up default values

  Call this function to restore the PX14 back to power up conditions. The
  following are side-effects of this function:
  - Current DMA transfer is cancelled
  - Any threads waiting for Samples Complete interrupt are resumed

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetPowerupDefaultsPX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (IsDeviceRemotePX14(hBrd))
      return rmc_SetPowerupDefaults(hBrd);

   // Put board into standby mode
   res = SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
   PX14_RETURN_ON_FAIL(res);
   statep->m_pll_defer_cnt = 0;

   res = InitRegSetDevice(hBrd);
   PX14_RETURN_ON_FAIL(res);
   res = WriteAllDeviceRegistersPX14(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // Update driver (software-only) registers that reflect hardware values
   res = WriteDeviceRegPX14(hBrd, 0, 0xFFFFFFFF, PX14DREGDEFAULT_0, 0, 0, PX14REGSET_DRIVER);
   PX14_RETURN_ON_FAIL(res);
   if (statep->IsPX12500())
   {
      res = WriteDeviceRegPX14(hBrd, 1, 0xFFFFFFFF, PX14DREGDEFAULT_1_PX12500, 0, 0, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDeviceRegPX14(hBrd, 2, 0xFFFFFFFF, PX14DREGDEFAULT_2_PX12500, 0, 0, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDeviceRegPX14(hBrd, 3, 0xFFFFFFFF, PX14DREGDEFAULT_3_PX12500, 0, 0, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDeviceRegPX14(hBrd, 4, 0xFFFFFFFF, PX14DREGDEFAULT_4_PX12500, 0, 0, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);

   }
   else
   {
      res = WriteDeviceRegPX14(hBrd, 1, 0xFFFFFFFF, PX14DREGDEFAULT_1, 0, 0, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDeviceRegPX14(hBrd, 2, 0xFFFFFFFF, PX14DREGDEFAULT_2, 0, 0, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDeviceRegPX14(hBrd, 3, 0xFFFFFFFF, PX14DREGDEFAULT_3, 0, 0, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDeviceRegPX14(hBrd, 4, 0xFFFFFFFF, PX14DREGDEFAULT_4, 0, 0, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }
   res = WriteDeviceRegPX14(hBrd, 5, 0xFFFFFFFF, PX14DREGDEFAULT_5, 0, 0, PX14REGSET_DRIVER);
   PX14_RETURN_ON_FAIL(res);
   PX14_CT_ASSERT(PX14_DRIVER_REG_COUNT == 6);

   // Turn on reference and center DC offsets; only has effect on DC devices
   res = _InitializeDcOffsetDac(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // Initialize clock generators to startup conditions
   res = InitializeClockGeneratorPX14(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // Wait for PLL to lock
   if (PX14CLKSRC_INT_VCO == GetAdcClockSourcePX14(hBrd))
   {
      res = _WaitForPllLockPX14(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }

   // Resynchronize all clock gen outputs to be in phase
   res = ResyncClockOutputsPX14(hBrd);
   PX14_RETURN_ON_FAIL(res);

   res = ResetTimestampFifoPX14(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // We need to resynchronize firmware to acquisition clock
   res = SyncFirmwareToAdcClockPX14(hBrd, PX14_TRUE);
   PX14_RETURN_ON_FAIL(res);

  return SIG_SUCCESS;  
}

/** @brief
  Bring hardware settings up to date with current cache settings

  The PX14 operating mode is not considered in this routine; the PX14
  will be put into Standby mode prior to updating hardware settings and
  will not be changed.
  */
PX14API RewriteHardwareSettingsPX14 (HPX14 hBrd)
{
   return CopyHardwareSettingsPX14(hBrd, hBrd);
}

/**	@brief Determine if the PX14 is idle; in Standby or Off mode

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @return
  Returns 0 if board is currently active (acquiring or transferring),
  a positive value if the board is idle (in Standby or Off operating
  mode), or a negative (SIG_*) value on error.
  */
PX14API InIdleModePX14 (HPX14 hBrd)
{
   int res;

   res = GetOperatingModePX14(hBrd, PX14_GET_FROM_DRIVER);
   if (res < 0)
      return res;

   return ((res == PX14MODE_STANDBY) || (res == PX14MODE_OFF));
}

/** @brief Determine if the PX14 is acquiring (or waiting to acquire)

  @note This is checked at the driver level

  @return
  Returns 0 if board is not in an acquisition operating mode.
  Returns a positive value if the board is in an acquisition
  operating mode.
  Returns a negative (SIG_*) error value on error.
  */
PX14API InAcquisitionModePX14 (HPX14 hBrd)
{
   int operating_mode;

   // Get current operating mode from the driver
   operating_mode = GetOperatingModePX14(hBrd, PX14_GET_FROM_DRIVER);
   if (operating_mode < 0)
      return operating_mode;	// Error

   if ((operating_mode >= PX14MODE_ACQ_RAM) &&
       (operating_mode <= PX14MODE_ACQ_SAB_BUF))
   {
      return operating_mode;
   }

   return 0;
}

/** @brief Set the PX14's operating mode

  If the PX14 is being put into an SAB operating mode (acquisition or
  transfer) then the current SAB board settings will be validated and
  possibly changed if invalid. In particular if SAB board number is 0
  (SAB disabled) it will be changed to 1.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetOperatingModePX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int cur_mode;//, pre_reg_delay_us;
   CStatePX14* statep;
   int res;

   if (val >= PX14MODE__COUNT)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // PATCH for the problem of aborting a pending DMA in kernel
   // If the board is waiting for trigger, aborting a pending DMA in kernel puts the board in a strange state and it froze the system later
   // Force to complete the pending DMA graciously by sending a SW trigger
   if ( IsTransferInProgressPX14(hBrd) )
   {
      unsigned int CleanupDma = PX14MODE_CLEANUP_PENDING_DMA;

      // Ask driver to prepare to complete the current DMA
      res = DeviceRequest(hBrd, IOCTL_PX14_MODE_SET, &CleanupDma, sizeof(int));
      PX14_RETURN_ON_FAIL(res);

      IssueSoftwareTriggerPX14( hBrd );
      // Wait for SW trigger take effect
      SysSleep(100);
   }

   cur_mode = static_cast<unsigned int>(GetOperatingModePX14(hBrd));
   //pre_reg_delay_us = 0;

   if ((statep->m_pll_defer_cnt > 0) && PX14_IS_ACQ_MODE(val))
   {
      // PLL lock deferral is turned on, if we're going into an
      //  acquisition then the buck stops here. Wait for PLL if
      //  necessary.
      statep->m_pll_defer_cnt = 0;
      if (statep->m_flags & PX14LIBF_NEED_PLL_LOCK_CHECK)
      {
         res = _WaitForPllLockPX14(hBrd);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   // Make sure we're always going to or coming from Standby mode
   if ((cur_mode != PX14MODE_STANDBY) && (val != PX14MODE_STANDBY))
   {
      res = SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
      PX14_RETURN_ON_FAIL(res);
   }

   // Handle any pre-SAB operation duties
   if (PX14_IS_SAB_MODE(val))
   {
      res = _PreSabOp(hBrd, val);
      PX14_RETURN_ON_FAIL(res);
   }

   // Ask driver to make operating mode change
   res = DeviceRequest(hBrd, IOCTL_PX14_MODE_SET, &val, sizeof(int));
   PX14_RETURN_ON_FAIL(res);
   statep->m_regDev.fields.regB.bits.OM = val;

   if (val == PX14MODE_STANDBY)
   {
      // Handle any post-SAB operation duties
      if (PX14_IS_SAB_MODE(cur_mode))
      {
         res = _PostSabOp(hBrd, cur_mode);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   return SIG_SUCCESS;
}

/** @brief Get the PX14's operating mode

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current segment size on success or one of the SIG_*
  error values on error. All SIG_* error values are negative.
  */
PX14API GetOperatingModePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 0xB);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.regB.bits.OM);
}

/** @brief Set SAB board number; uniquely identifies a board on SAB bus

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Specifies the board's SAB board number. This can be any number from
  0 to 8 (inclusive). Setting this parameter to 0 will disable all SAB
  control. Each board on the SAB must have a unique SAB board number.

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetSabBoardNumberPX14 (HPX14 hBrd, unsigned int val)
{
   int res;

   // Make sure board implements SAB
   res = CheckFeaturePX14(hBrd, PX14FEATURE_SAB);
   PX14_RETURN_ON_FAIL(res);

   // Clip offset to max
   if (val > PX14_MAX_SAB_BOARD_NUMBER)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_09 r9;
   r9.bits.BN = val;
   return WriteDeviceRegPX14(hBrd, 9, PX14REGMSK_SAB_BRDNUM, r9.val);
}

/** @brief Get SAB board number; uniquely identifies a board on SAB bus

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current SAB board number on success or one of the SIG_*
  error values on error. All SIG_* error values are negative.
  */
PX14API GetSabBoardNumberPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 9);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg9.bits.BN);
}

/** @brief Set SAB configuration; defines which bus lines are used

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetSabConfigurationPX14 (HPX14 hBrd, unsigned int val)
{
   int res;

   // Make sure board implements SAB
   res = CheckFeaturePX14(hBrd, PX14FEATURE_SAB);
   PX14_RETURN_ON_FAIL(res);

   if (val >= PX14SABCFG__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_09 r9;
   r9.bits.CNF = val;
   return WriteDeviceRegPX14(hBrd, 9, PX14REGMSK_SAB_CNF, r9.val);
}

/** @brief Get SAB configuration; defines which bus lines are used

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current SAB configuration value (PX14SABCFG_*) on success
  or one of the SIG_* error values on error. All SIG_* error values
  are negative.
  */
PX14API GetSabConfigurationPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 9);
      PX14_RETURN_ON_FAIL(res);
   }

   return statep->m_regDev.fields.reg9.bits.CNF;
}

/** @brief Set SAB clock; governs rate when writing to SAB bus

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetSabClockPX14 (HPX14 hBrd, unsigned int val)
{
   int res;

   // Make sure board implements SAB
   res = CheckFeaturePX14(hBrd, PX14FEATURE_SAB);
   PX14_RETURN_ON_FAIL(res);

   if (val >= PX14SABCLK__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // In order for the firmware to recognize the change we need to ensure
   //  a transition into the selected clock. So first, select any other
   //  source clock, then re-set with given clock value
   PX14U_REG_DEV_09 r9;
   r9.bits.CLK = (val + 1) % PX14SABCLK__COUNT;
   res = WriteDeviceRegPX14(hBrd, 9, PX14REGMSK_SAB_CLOCK, r9.val);
   PX14_RETURN_ON_FAIL(res);

   // Update hardware with correct clock setting
   r9.bits.CLK = val;
   return WriteDeviceRegPX14(hBrd, 9, PX14REGMSK_SAB_CLOCK, r9.val);
}

/** @brief Get SAB clock; governs rate when writing to SAB bus

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current SAB clock value (PX14SABCLK_*) on success or one
  of the SIG_* error values on error. All SIG_* error values are
  negative.
  */
PX14API GetSabClockPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 9);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg9.bits.CLK);
}

/** @brief Synchronize the PX14 firmware to the ADC clock

  Prior to beginning a data acquisition, the PX14 firmware must first be
  synchronized to the ADC clock. This synchronization will hold until the
  ADC clock frequency changes. This synchronization is also sometimes
  referred to as "resetting the DCMs".

  Most users will not need to invoke this function as the PX14 library
  implementation automatically invokes it when necessary.
  */
PX14API SyncFirmwareToAdcClockPX14 (HPX14 hBrd, int bForce)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (bForce)
   {
      res = DeviceRequest(hBrd, IOCTL_PX14_RESET_DCMS);
      if (SIG_SUCCESS != res)
      {
         if (SIG_PX14_TIMED_OUT == res)
            return SIG_PX14_DCM_SYNC_FAILED;

         return res;
      }
   }

   return SIG_SUCCESS;
}

/** @brief Copy hardware settings from another PX14 device

  Calling this function will copy all hardware settings of the PX14
  associated with the source device to the PX14 associated with the
  destination device. The function will use the hardware settings as
  they are defined in the handle-specific software register cache.

  Operating mode is not copied and the destination board is placed into
  Standby operating mode before any settings are applied.

  <b>Master/slave devices:</b> Master/slave configuration is not copied.
  The primary reason for this is safety; if two devices on the same
  master/slave connector are designated as masters, they both will be
  driving a common bus which can potentially damage hardware.

  If the source device is a master and the destination device is a slave
  then the function will ensure that the slave acquisition clock settings
  are configured properly by selecting the clock source as external and
  setting the external clock rate equal to the master's current
  acquisition rate (for whichever clock source is used).

  @param hBrdDst
  A handle to the PX14 device to which the copied hardware settings
  will be applied.
  @param hBrdSrc
  A handle to the PX14 device from which the hardware settings are
  copied from.
  */
PX14API CopyHardwareSettingsPX14 (HPX14 hBrdDst, HPX14 hBrdSrc)
{
   bool bSrcMaster, bDstSlave;
   int res, msCfg;
   double dVal;

   // NOTE: Don't try to be smart and optimize out if handles are the same;
   // we're using this as our implementation for RewriteHardwareSettingsPX14

   res = ValidateHandle(hBrdDst, NULL);
   PX14_RETURN_ON_FAIL(res);

   res = ValidateHandle(hBrdSrc, NULL);
   PX14_RETURN_ON_FAIL(res);

   res = SetOperatingModePX14(hBrdDst, PX14MODE_STANDBY);
   PX14_RETURN_ON_FAIL(res);

   // Defer the checking of the PLL lock; we're going to be changing PLL
   // parameters several times. Without this deferral we'll be waiting
   // after each change when we really only need to wait after all
   // settings have been applied.
   CAutoPllLockCheckDeferral auto_pcd(hBrdDst);

   // Master slave configuration is omitted for safety; two masters driving
   //  at the same time can damage components
   msCfg = GetMasterSlaveConfigurationPX14(hBrdSrc);
   bSrcMaster = PX14_IS_MASTER(msCfg);
   msCfg = GetMasterSlaveConfigurationPX14(hBrdDst);
   bDstSlave = PX14_IS_SLAVE(msCfg);

   // PX14HWSET

   SetSegmentSizePX14(hBrdDst, GetSegmentSizePX14(hBrdSrc));
   SetSampleCountPX14(hBrdDst, GetSampleCountPX14(hBrdSrc));
   SetStartSamplePX14(hBrdDst, GetStartSamplePX14(hBrdSrc));
   SetTriggerDelaySamplesPX14(hBrdDst, GetTriggerDelaySamplesPX14(hBrdSrc));
   SetPreTriggerSamplesPX14(hBrdDst, GetPreTriggerSamplesPX14(hBrdSrc));
   SetTriggerLevelAPX14(hBrdDst, GetTriggerLevelAPX14(hBrdSrc));
   SetTriggerLevelBPX14(hBrdDst, GetTriggerLevelBPX14(hBrdSrc));
   SetTriggerModePX14(hBrdDst, GetTriggerModePX14(hBrdSrc));
   SetTriggerSourcePX14(hBrdDst, GetTriggerSourcePX14(hBrdSrc));
   SetTriggerDirectionAPX14(hBrdDst, GetTriggerDirectionAPX14(hBrdSrc));
   SetTriggerDirectionBPX14(hBrdDst, GetTriggerDirectionBPX14(hBrdSrc));
   SetTriggerDirectionExtPX14(hBrdDst, GetTriggerDirectionExtPX14(hBrdSrc));
   SetInputVoltRangeCh1PX14(hBrdDst, GetInputVoltRangeCh1PX14(hBrdSrc));
   SetInputVoltRangeCh2PX14(hBrdDst, GetInputVoltRangeCh2PX14(hBrdSrc));

   // These only have effect if device is DC-coupled. Safe to set on
   //  AC-coupled devices.
   SetDcOffsetCh1PX14(hBrdDst, GetDcOffsetCh1PX14(hBrdSrc));
   SetDcOffsetCh2PX14(hBrdDst, GetDcOffsetCh2PX14(hBrdSrc));
   SetInputGainLevelDcPX14(hBrdDst, GetInputGainLevelDcPX14(hBrdSrc));
   // Fine DC offset only has effect on PX14400D2 devices
   SetFineDcOffsetCh1PX14(hBrdDst, GetFineDcOffsetCh1PX14(hBrdSrc));
   SetFineDcOffsetCh2PX14(hBrdDst, GetFineDcOffsetCh2PX14(hBrdSrc));

   SetTimestampModePX14(hBrdDst, GetTimestampModePX14(hBrdSrc));
   SetTimestampCounterModePX14(hBrdDst, GetTimestampCounterModePX14(hBrdSrc));

   res = SetBoardProcessingEnablePX14(hBrdDst, GetBoardProcessingEnablePX14(hBrdSrc));
   PX14_RETURN_ON_FAIL(res);

   SetDigitalIoModePX14(hBrdDst, GetDigitalIoModePX14(hBrdSrc));
   SetDigitalIoEnablePX14(hBrdDst, GetDigitalIoEnablePX14(hBrdSrc));
   SetActiveChannelsPX14(hBrdDst, GetActiveChannelsPX14(hBrdSrc));
   SetPowerDownOverridePX14(hBrdDst, GetPowerDownOverridePX14(hBrdSrc));

   res = CheckFeaturePX14(hBrdDst, PX14FEATURE_SAB);
   if (SIG_SUCCESS == res)
   {
      SetSabBoardNumberPX14(hBrdDst, GetSabBoardNumberPX14(hBrdSrc));
      SetSabConfigurationPX14(hBrdDst, GetSabConfigurationPX14(hBrdSrc));
      SetSabClockPX14(hBrdDst, GetSabClockPX14(hBrdSrc));
   }

   SetInternalAdcClockReferencePX14(hBrdDst, GetInternalAdcClockReferencePX14(hBrdSrc));
   SetPostAdcClockDividerPX14(hBrdDst, GetPostAdcClockDividerPX14(hBrdSrc));

   if (bDstSlave)
   {
      res = SetAdcClockSourcePX14(hBrdDst, PX14CLKSRC_EXTERNAL);
      PX14_RETURN_ON_FAIL(res);

      SetExtClockDivider1PX14(hBrdDst, 1);
      SetExtClockDivider2PX14(hBrdDst, 1);

      if (bSrcMaster)
      {
         GetActualAdcAcqRatePX14(hBrdSrc, &dVal);
         SetExternalClockRatePX14(hBrdDst, dVal);
      }
   }
   else
   {
      SetExtClockDivider1PX14(hBrdDst, GetExtClockDivider1PX14(hBrdSrc));
      SetExtClockDivider2PX14(hBrdDst, GetExtClockDivider2PX14(hBrdSrc));

      res = SetAdcClockSourcePX14(hBrdDst, GetAdcClockSourcePX14(hBrdSrc));
      PX14_RETURN_ON_FAIL(res);

      GetExternalClockRatePX14(hBrdSrc, &dVal);
      SetExternalClockRatePX14(hBrdDst,  dVal);

      GetInternalAdcClockRatePX14(hBrdSrc, &dVal);
      res = SetInternalAdcClockRatePX14(hBrdDst,  dVal);
      PX14_RETURN_ON_FAIL(res);
   }

   // Explicitly release so we can check result
   res = auto_pcd.Release();
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

/** @brief
  Set the PX14's segment size for use with Segmented triggering mode

  The segment size setting is only relevant when the Segmented triggering
  mode is enabled. The segment size defines how many samples will be
  acquired before the board stops and waits for another trigger condition.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  The segment size, in samples, to set. This should be an even value.

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.

  @sa SetTriggerModePX14
  */
PX14API SetSegmentSizePX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int val_adj;

   if (val)
   {
      unsigned unaligned;

      // Realign (down) if necessary
      if ((unaligned = val % PX14_ALIGN_SEGMENT_SAMPLES) > 0)
         val -= unaligned;

      // Clip to extrema
      if (val > PX14_MAX_SEGMENT_SAMPLES)
         val = PX14_MAX_SEGMENT_SAMPLES;
      else if (val < PX14_MIN_SEGMENT_SAMPLES)
         val = PX14_MIN_SEGMENT_SAMPLES;
   }

   val_adj = val >> PX14REGHWSH_SEG_SIZE;

   // Update hardware
   PX14U_REG_DEV_00 r0;
   r0.bits.SEGSIZE = val_adj;
   return WriteDeviceRegPX14(hBrd, 0, PX14REGMSK_SEG_SIZE, r0.val);
}

/** @brief
  Get the PX14's segment size for use with Segmented triggering mode

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current segment size on success or one of the SIG_*
  error values on error. All SIG_* error values are negative.
  */
PX14API GetSegmentSizePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 0);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(
                           statep->m_regDev.fields.reg0.bits.SEGSIZE << PX14REGHWSH_SEG_SIZE);
}

/** @brief Set the PX14's total sample count. Defines length of acq/xfer

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  The new total sample count. This should be a multiple of 32. This
  value will be clipped to the largest valid sample count value,
  PX14_MAX_SAMPLE_COUNT (2147483616). Note that this is larger than
  the PX14 RAM size; larger acquisition sizes can be used with SAB
  acquisitions.

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetSampleCountPX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int mask;

   // Clip value to maximum
   if (val > PX14_MAX_SAMPLE_COUNT)
      val = PX14_MAX_SAMPLE_COUNT;

   // NOTE: We can set a total sample count larger than our sample RAM.

   // Update hardware
   PX14U_REG_DEV_01 r1;
   if (PX14_FREE_RUN != val)
   {
      mask = PX14REGMSK_BURST_COUNT | PX14REGMSK_PARTIAL_BURST
         | PX14REGMSK_FREE_RUN;

      // 1 burst = 1024 elements = 4096 samples = 2 ^ 12
      // Partial burst unit = 8 elements = 32 samples = 2 ^ 5
      r1.bits.BC = val >> 12;
      r1.bits.PS = (val & 0xFFF) >> 5;

      r1.bits.FRUN = 0;
   }
   else
   {
      mask = PX14REGMSK_FREE_RUN | PX14REGMSK_BURST_COUNT;
      r1.bits.FRUN = 1;
      // firmware needs non-zero burst count
      r1.bits.BC = 1;
   }

   return WriteDeviceRegPX14(hBrd, 1, mask, r1.val);
}

/** @brief Get the PX14's total sample count; defines length of acq/xfer

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current total sample count on success or one of the
  SIG_* error values on error. All SIG_* error values are negative.
  */
PX14API GetSampleCountPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 1);
      PX14_RETURN_ON_FAIL(res);
   }

   PX14U_REG_DEV_01& r1 = statep->m_regDev.fields.reg1;

   if (r1.bits.FRUN)
      res = PX14_FREE_RUN;
   else
   {
      // 1 burst = 1024 elements = 4096 samples = 2 ^ 12
      // Partial burst unit = 8 elements = 32 samples = 2 ^ 5
      res = static_cast<int>((r1.bits.BC << 12) + (r1.bits.PS << 5));
   }

   return res;
}

/** @brief Set the PX14's start sample; defines start of acq/xfer

  The starting sample setting defines where in sample RAM the next RAM
  acquisition or RAM transfer operation will begin from.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  The new starting sample. This should be a multiple of 8 samples.
  This value will be clipped to the largest valid starting sample,
  PX14_MAX_START_SAMPLE (268435452).

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetStartSamplePX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int val_adj;

   // Align down if necessary
   if (val % PX14_ALIGN_START_SAMPLE)
      val -= (val % PX14_ALIGN_START_SAMPLE);
   // Clip value to maximum
   if (val > PX14_MAX_START_SAMPLE)
      val = PX14_MAX_START_SAMPLE;

   val_adj = val >> PX14REGHWSH_START_SAMPLE;

   // Update hardware
   PX14U_REG_DEV_02 r2;
   r2.bits.SA = val_adj;
   return WriteDeviceRegPX14(hBrd, 2, PX14REGMSK_START_SAMPLE, r2.val);
}

/** @brief Get the PX14's start sample; defines start of acq/xfer

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current starting sample setting on success or one of the
  SIG_* error values on error. All SIG_* error values are negative.
  */
PX14API GetStartSamplePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 2);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(
                           statep->m_regDev.fields.reg2.bits.SA << PX14REGHWSH_START_SAMPLE);
}

/** @brief Set trigger delay samples; count of samples to skip after trigger

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  The number of delay samples to set. This can be any even number not
  greater than 2097148 (or 262140 if firmware package version is
  less than 1.4) The function will clip this value at the maximum
  allowed value if necessary.

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetTriggerDelaySamplesPX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int val_adj;
   int res;

   // We update this setting in two parts; one part that defines the number
   //  of samples and another part that enables/disables the feature.

   if (val)
   {
      unsigned unaligned;

      // Realign (down) if necessary
      if ((unaligned = val % PX14_ALIGN_TRIG_DELAY_SAMPLES) > 0)
         val -= unaligned;

      // Clip to maximum value
      if (val > PX14_MAX_TRIG_DELAY_SAMPLES_OLD)
      {
         /*res = CheckFirmwareVerPX14(hBrd, PX14_VER32(1,3,3,0));
           if (SIG_SUCCESS == res)
           {*/
         if (val > PX14_MAX_TRIG_DELAY_SAMPLES)
            val = PX14_MAX_TRIG_DELAY_SAMPLES;
         /*}
           else
           val = PX14_MAX_TRIG_DELAY_SAMPLES_OLD;*/
      }
      else if (val < PX14_MIN_TRIG_DELAY_SAMPLES)
         val = PX14_MIN_TRIG_DELAY_SAMPLES;
   }

   val_adj = val >> PX14REGHWSH_TRIG_DELAY;

   // Update hardware - delay samples count
   PX14U_REG_DEV_03 r3;
   r3.val = 0;
   r3.bits.TD    = val_adj & 0x0000FFFF;
   r3.bits.TD_HI = (val_adj & 0x00070000) >> 16;
   res = WriteDeviceRegPX14(hBrd, 3,
                            PX14REGMSK_TRIG_DELAY | PX14REGMSK_TRIG_DELAY_HI, r3.val);
   PX14_RETURN_ON_FAIL(res);

   // Update hardware - delay samples enable
   PX14U_REG_DEV_05 r5;
   r5.bits.DTRIG = val_adj ? 1 : 0;
   return WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_DTRIG_ENABLE, r5.val);
}

/** @brief Get trigger delay samples; count of samples to skip after trigger

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current trigger delay sample count on success or one of
  the SIG_* error values on error. All SIG_* error values are
  negative.
  */
PX14API GetTriggerDelaySamplesPX14 (HPX14 hBrd, int bFromCache)
{
   unsigned int val_adj;
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 3);
      PX14_RETURN_ON_FAIL(res);
   }

   val_adj =
      (statep->m_regDev.fields.reg3.bits.TD_HI << 16) |
      statep->m_regDev.fields.reg3.bits.TD;

   return static_cast<int>(val_adj << PX14REGHWSH_TRIG_DELAY);
}

/** @brief
  Set pre-trigger sample count; count of samples to keep prior to
  trigger

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  The number of pre-trigger samples to set. This can be any even
  number not greater than PX14_MAX_PRE_TRIGGER_SAMPLES (131070). The
  function will clip this value at the maximum allowed value if
  necessary. If this value is 0 then no pre-trigger samples are
  collected

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetPreTriggerSamplesPX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int val_adj;
   CStatePX14* statep;
   int res;

   // We update this setting in two parts; one part that defines the number
   //  of samples and another part that enables/disables the feature.

   if (val)
   {
      unsigned unaligned, max_count;

      // Realign (down) if necessary
      if ((unaligned = val % PX14_ALIGN_PRE_TRIGGER_SAMPLES) > 0)
         val -= unaligned;

      res = ValidateHandle(hBrd, &statep);
      PX14_RETURN_ON_FAIL(res);

      max_count = statep->IsPX12500()
         ? PX14_MAX_PRE_TRIGGER_SAMPLES_PX12500 : PX14_MAX_PRE_TRIGGER_SAMPLES;

      // Clip to extrema
      if (val > max_count)
         val = max_count;
      else if (val < PX14_MIN_PRE_TRIGGER_SAMPLES)
         val = PX14_MIN_PRE_TRIGGER_SAMPLES;
   }

   val_adj = val >> PX14REGHWSH_PRETRIG_SAMPLES;

   // Update hardware - pre-trigger sample count
   PX14U_REG_DEV_03 r3;
   r3.val = 0;
   r3.bits.PS = val_adj;
   res = WriteDeviceRegPX14(hBrd, 3, PX14REGMSK_PRETRIG_SAMPLES, r3.val);
   PX14_RETURN_ON_FAIL(res);

   // Update hardware - pre-trigger samples enable
   PX14U_REG_DEV_05 r5;
   r5.bits.PTRIG = val_adj ? 1 : 0;
   return WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_PTRIG_ENABLE, r5.val);
}

/** @brief
  Get pre-trigger sample count; count of samples to keep prior to
  trigger

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current pre-trigger sample count on success or one of
  the SIG_* error values on error. All SIG_* error values are
  negative.
  */
PX14API GetPreTriggerSamplesPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 3);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg3.bits.PS
                           << PX14REGHWSH_PRETRIG_SAMPLES);
}

/** @brief
  Set trigger A level; defines threshold for an internal trigger event

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  The digital ADC value that defines the trigger level threshold for
  trigger A

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetTriggerLevelAPX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int val_adj;

   if (val > PX14_MAX_TRIGGER_LEVEL)
      return SIG_PX14_INVALID_ARG_2;

   val_adj = val >> PX14REGHWSH_TRIGLEVEL_A;

   // Update hardware
   PX14U_REG_DEV_04 r4;
   r4.bits.TLA = val_adj;
   return WriteDeviceRegPX14(hBrd, 4, PX14REGMSK_TRIGLEVEL_A, r4.val);
}

/** @brief
  Get trigger A level; defines threshold for an internal trigger event

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current trigger level A value on success or one of the
  SIG_* error values on error. All SIG_* error values are negative.
  */
PX14API GetTriggerLevelAPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 4);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg4.bits.TLA
                           << PX14REGHWSH_TRIGLEVEL_A);
}

/** @brief
  Set trigger B level; defines threshold for an internal trigger event

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  The digital ADC value that defines the trigger level threshold for
  trigger B

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetTriggerLevelBPX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int val_adj;

   if (val > PX14_MAX_TRIGGER_LEVEL)
      return SIG_PX14_INVALID_ARG_2;

   val_adj = val >> PX14REGHWSH_TRIGLEVEL_B;

   // Update hardware
   PX14U_REG_DEV_04 r4;
   r4.bits.TLB = val_adj;
   return WriteDeviceRegPX14(hBrd, 4, PX14REGMSK_TRIGLEVEL_B, r4.val);
}

/** @brief
  Get trigger B level; defines threshold for an internal trigger event

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current trigger level B value on success or one of the
  SIG_* error values on error. All SIG_* error values are negative.
  */
PX14API GetTriggerLevelBPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 4);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(
                           statep->m_regDev.fields.reg4.bits.TLB << PX14REGHWSH_TRIGLEVEL_B);
}

/** @brief Set triggering mode; relates trigger events to how data is saved

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetTriggerModePX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14TRIGMODE__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_05 r5;
   r5.bits.TM = val;
   return WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_TRIGGER_MODE, r5.val);
}

/** @brief Get triggering mode; relates trigger events to how data is saved

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current trigger mode value (PX14TRIGMODE_*) on success or
  one of the SIG_* error values on error. All SIG_* error values are
  negative.
  */
PX14API GetTriggerModePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
		res = ReadDeviceRegPX14(hBrd, PX14REGIDX_TRIGGER_MODE);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg5.bits.TM);
}


/** @brief Trigger Selection

000 : TRIG A AND HYSTERESIS
001 : TRIG A AND TRIG B
011 : TRIG A OR  TRIG B
*/
PX14API SetTriggerSelectionPX14(HPX14 hBrd, int val)
{
   int res;
   PX14U_REG_DEV_10 r10;

   r10.bits.trig_sel = val;
   res = WriteDeviceRegPX14(hBrd, PX14REGIDX_TRIG_SEL, PX14REGMSK_10_TRIG_SEL, r10.val);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

PX14API GetTriggerSelectionPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
		res = ReadDeviceRegPX14(hBrd, PX14REGIDX_TRIG_SEL);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg10.bits.trig_sel);
}


/** @brief Trigger Hysteresis
*/
PX14API SetTriggerHysteresisPX14(HPX14 hBrd, int val)
{
   int res;
   PX14U_REG_DEV_10 r10;

   r10.bits.hyst = val;
   res = WriteDeviceRegPX14(hBrd, PX14REGIDX_HYST, PX14REGMSK_10_HYST, r10.val);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

PX14API GetTriggerHysteresisPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
		res = ReadDeviceRegPX14(hBrd, PX14REGIDX_HYST);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg10.bits.hyst);
}

//Set TODPT mode enable/disable

PX14API SetTODPT_Enable(HPX14 hBrd, int val)
{ 
	int res;
	PX14U_REG_DEV_10 r10;

	r10.bits.TODPT_Mode = val;
	res = WriteDeviceRegPX14(hBrd, PX14REGIDX_TODPT, PX14REGMSK_10_TODPT, r10.val);
	PX14_RETURN_ON_FAIL(res);

	return SIG_SUCCESS;
}

//Get TODPT mode enable/disable

PX14API GetTODPT_Enable(HPX14 hBrd, int bFromCache)
{
	CStatePX14* statep;
	int res;
	
	res = ValidateHandle(hBrd, &statep);
	PX14_RETURN_ON_FAIL(res);

	if (!bFromCache)
	{
		res = ReadDeviceRegPX14(hBrd, PX14REGIDX_TODPT);
		PX14_RETURN_ON_FAIL(res);
	}

	return static_cast<int>(statep->m_regDev.fields.reg10.bits.TODPT_Mode);
}

/** @brief Get FPGA temperature

	@param hBrd
		A handle to a PX14400 device obtained by calling ConnectToDevicePX14
	@param bFromCache
		If non-zero, the setting will be read from the local device register
		cache associated with the given PX14 handle, which will result in
		no hardware or driver access. If zero, the setting is obtained from
		the driver which may or may not result in an actual PX14 device 
		register read.

	@return
		Returns the current temperature in celcius on success or
		one of the SIG_* error values on error. All SIG_* error values are 
		negative.
*/
PX14API GetFPGATemperaturePX14(HPX14 hBrd, int bFromCache)
{
	CStatePX14* statep;
	int res;
	int FPGA_Temp_10bit;
	
	res = ValidateHandle(hBrd, &statep);
	PX14_RETURN_ON_FAIL(res);

	if (!bFromCache)
	{
		res = ReadDeviceRegPX14(hBrd, PX14REGIDX_FPGA_TEMP);
		PX14_RETURN_ON_FAIL(res);
	}

	FPGA_Temp_10bit =  static_cast<int>(statep->m_regDev.fields.regD.bits.FPGA_Temp);

	//Base on a linear regression
	return (int)(0.4916267942583732*FPGA_Temp_10bit-272.93);
}

/** @brief Set trigger source; defines where trigger events originate

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetTriggerSourcePX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14TRIGSRC__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_05 r5;
   r5.bits.TS = val;
   return WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_TRIGGER_SOURCE, r5.val);
}

/** @brief Get trigger source; defines where trigger events originate

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current trigger source value (PX14TRIGSRC_*) on success
  or one of the SIG_* error values on error. All SIG_* error values
  are negative.
  */
PX14API GetTriggerSourcePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 5);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg5.bits.TS);
}

/** @brief
  Set trigger A direction; defines the direction that defines a
  trigger

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetTriggerDirectionAPX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14TRIGDIR__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_05 r5;
   r5.bits.TDIRA = val;
   return WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_TRIGGER_DIRA, r5.val);
}

/** @brief
  Get trigger A direction; defines the direction that defines a
  trigger

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current trigger direction A value (PX14TRIGDIR_*) on
  success or one of the SIG_* error values on error. All SIG_* error
  values are negative.
  */
PX14API GetTriggerDirectionAPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 5);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg5.bits.TDIRA);
}

/** @brief
  Set trigger B direction; defines the direction that defines a
  trigger

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetTriggerDirectionBPX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14TRIGDIR__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_05 r5;
   r5.bits.TDIRB = val;
   return WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_TRIGGER_DIRB, r5.val);
}

/** @brief
  Get trigger B direction; defines the direction that defines a
  trigger

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current trigger direction B value (PX14TRIGDIR_*) on
  success or one of the SIG_* error values on error. All SIG_* error
  values are negative.
  */
PX14API GetTriggerDirectionBPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 5);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg5.bits.TDIRB);
}

/** @brief Set external trigger direction

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetTriggerDirectionExtPX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14TRIGDIR__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_05 r5;
   r5.bits.TDIR_E = val;
   return WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_TRIGGER_DIR_EXT, r5.val);
}

/** @brief Get external trigger direction

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current trigger direction B value (PX14TRIGDIR_*) on
  success or one of the SIG_* error values on error. All SIG_* error
  values are negative.
  */
PX14API GetTriggerDirectionExtPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 5);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg5.bits.TDIR_E);
}

/** @brief Issue a software-generated trigger event to a PX14

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API IssueSoftwareTriggerPX14 (HPX14 hBrd)
{
   int res;

   // Pulse STR bit
   PX14U_REG_DEV_05 r5;
   r5.bits.STR = 1;
   res = WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_STR, r5.val);
   PX14_RETURN_ON_FAIL(res);

   r5.bits.STR = 0;
   return WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_STR, r5.val);
}

/** @brief
  Set the digital output mode; determines what is output on digital
  output

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetDigitalIoModePX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14DIGIO__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_06 r6;
   r6.bits.DOUT = val;
   return WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_DOUT_SEL, r6.val);
}

/** @brief
  Get the digital output mode; determines what is output on digital
  output

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current digital output mode value on success or one of
  the SIG_* error values on error. All SIG_* error values are
  negative.
  */
PX14API GetDigitalIoModePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 6);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg6.bits.DOUT);
}

/** @brief Enable/disable the PX14's digital output

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bEnable
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetDigitalIoEnablePX14 (HPX14 hBrd, unsigned int bEnable)
{
   // Update hardware
   PX14U_REG_DEV_06 r6;
   r6.bits.DOUT_EN = bEnable ? 1 : 0;
   return WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_DOUT_EN, r6.val);
}

/** @brief Get state of PX14's digital output enable

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns a positive value if digitial output is enabled, zero if
  digital output is disabled, or one of the SIG_* error values on
  error. All SIG_* error values are negative.
  */
PX14API GetDigitalIoEnablePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 6);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg6.bits.DOUT_EN);
}

/** @brief
  Get status of the samples complete flag; set when a RAM acquisition
  completes
  */
PX14API GetSamplesCompleteFlagPX14 (HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xD, NULL, PX14REGREAD_HARDWARE);
   PX14_RETURN_ON_FAIL(res);

   return PX14_H2Bref(hBrd).m_regDev.fields.regD.bits.SCF ? 1 : 0;
}

/** @brief Get status of the FIFO full flag

  @note
  Currently this flag is only used for PCI buffered acquisitions.
  */
PX14API GetFifoFullFlagPX14 (HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xD, NULL, PX14REGREAD_HARDWARE);
   PX14_RETURN_ON_FAIL(res);

   return PX14_H2Bref(hBrd).m_regDev.fields.regD.bits.FFF ? 1 : 0;
}

/** @brief
  Get PLL lock status; only valid for internal clock
  */
PX14API GetPllLockStatusPX14 (HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xD, NULL, PX14REGREAD_HARDWARE);
   PX14_RETURN_ON_FAIL(res);

   return static_cast<int>(PX14_H2Bref(hBrd).m_regDev.fields.regD.bits.CG_LOCK);
}

/** @brief
  Get ADC clock measure; read-only
  */
PX14API GetSystemInternalClockRatePX14(HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xC);
   PX14_RETURN_ON_FAIL(res);

   int mes =  static_cast<int>(PX14_H2Bref(hBrd).m_regDev.fields.regC.bits.RA);

   if(mes != 0)
   {
      return (2*1000*200/mes);
   }
   else
   {
      return 0;
   }
}

/** @brief
  Get ACQ FIFO latched overflow flag; read-only
  */
PX14API GetAcqFifoLatchedOverflowFlagPX14 (HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xD);
   PX14_RETURN_ON_FAIL(res);

   return static_cast<int>(PX14_H2Bref(hBrd).m_regDev.fields.regD.bits.ACQFIFO);
}

/** @brief
  Get Acq lock status; read-only
  */
PX14API GetAcqDcmLockStatusPX14(HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xD);
   PX14_RETURN_ON_FAIL(res);

   return static_cast<int>(PX14_H2Bref(hBrd).m_regDev.fields.regD.bits.ACQDCMLCK);
}

/** @brief
  Get Acq latched lock status; read-only
  */
PX14API GetAcqDcmLatchedLockStatusPX14 (HPX14 hBrd)
{
   int res;

   res = ReadDeviceRegPX14(hBrd, 0xD);
   PX14_RETURN_ON_FAIL(res);

   return static_cast<int>(PX14_H2Bref(hBrd).m_regDev.fields.regD.bits.ACQDCMLCKLATCHED);
}

/** @brief
  Clear ACQ in FIFO (Write-only)
  */
PX14API ClearAcqFifoFlagPX14(HPX14 hBrd)
{
   int res;

   PX14U_REG_DEV_11 r11;

   r11.bits.ACQ_IN_FIFO_CLEAR = 1;
   res = WriteDeviceRegPX14(hBrd, PX14REGIDX_ACQ_IN_FIFO_CLEAR, PX14REGMSK_ACQ_IN_FIFO_CLEAR, r11.val);
   PX14_RETURN_ON_FAIL(res);

   r11.bits.ACQ_IN_FIFO_CLEAR = 0;
   res = WriteDeviceRegPX14(hBrd, PX14REGIDX_ACQ_IN_FIFO_CLEAR, PX14REGMSK_ACQ_IN_FIFO_CLEAR, r11.val);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

/** @brief
  Clear status dcm lock3 (Write-only)
  */
PX14API ClearDcmLockPX14(HPX14 hBrd)
{
   int res;

   PX14U_REG_DEV_11 r11;

   r11.bits.DCM_LOCK3_CLR = 1;
   res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DCM_LOCK3_CLR, PX14REGMSK_DCM_LOCK3_CLR, r11.val);
   PX14_RETURN_ON_FAIL(res);

   r11.bits.DCM_LOCK3_CLR = 0;
   res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DCM_LOCK3_CLR, PX14REGMSK_DCM_LOCK3_CLR, r11.val);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

/** @brief Verify if it a D2 version
*/
PX14API VerifyD2config(HPX14 p_hBrd)
{
   CStatePX14* statep;

   statep = PX14_H2B(p_hBrd);
   SIGASSERT_POINTER(statep, CStatePX14);

   if (statep->IsPX14400D2())
   {
      return SIG_SUCCESS;
   }
   else
   {
      return -1;
   }

   return -1;
}

PX14API ResetDcmPX14(HPX14 p_hBrd)
{

   int res;

   PX14U_REG_DEV_05 r05;

   r05.bits.DCMRES4 = 1;
   res = WriteDeviceRegPX14(p_hBrd, PX14REGIDX_DCM4_RESET, PX14REGMSK_DCM4_RESET, r05.val);
   PX14_RETURN_ON_FAIL(res);

   r05.bits.DCMRES4 = 0;
   res = WriteDeviceRegPX14(p_hBrd, PX14REGIDX_DCM4_RESET, PX14REGMSK_DCM4_RESET, r05.val);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;

}

/** @brief Get size of PX14 sample RAM in samples

  @param hBrd
  A handle to a PX14400 device. This parameter may also be
  PX14_INVALID_HANDLE in which case the function will return the
  default PX14 RAM size (128 kibi-samples).
  @param samplesp
  A pointer to an unsigned integer that will receive the PX14 sample
  RAM size in samples.

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error. All SIG_* error values are negative.

*/
PX14API GetSampleRamSizePX14 (HPX14 hBrd, unsigned int* samplesp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(samplesp, unsigned int);
   if (NULL == samplesp)
      return SIG_PX14_INVALID_ARG_2;

   if (PX14_INVALID_HANDLE == hBrd)
   {
      *samplesp = PX14_RAM_SIZE_IN_SAMPLES;
      return SIG_SUCCESS;
   }

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   *samplesp = PX14_RAM_SIZE_IN_SAMPLES;
   return SIG_SUCCESS;
}

/** @brief Get type of SAB FPGA present on PX14400; (PX14SABFPGA_*)

  This function consults cached hardware configuration to obtain the type
  of SAB FPGA present on the given device. This value is meaningless if no
  SAB is present on the board. GetBoardFeaturesPX14 can be used to
  determine if the board implements SAB.

  The SAB FPGA implements Signatec Auxiliary Bus (SAB) logic and/or
  custom, third-party firmware.
  */
PX14API GetSabFpgaTypePX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   return static_cast<int>(statep->m_fpgaTypeSab);
}

/** @brief Get the Slave #2-#3-#4 hardware configuration

  In order to select Slave #2, Slave #3, or Slave #4 as master/slave
  configuration, the PX14400 device must be hardware configured for that
  selection. All other master/slave configurations are always software
  selectable. This function returns the current hardware configuration
  for Slave 2-4.

  @return
  Returns 0 if device is unconfigured, 2 if device is configured for
  Slave #2, 3 if device is configured for Slave #3, 4 if device is
  configured for Slave #4 or library error code (SIG_*) on error
  */
PX14API GetSlave234TypePX14 (HPX14 hBrd)
{
   unsigned short slave234;
   int res;

   res = GetEepromDataPX14(hBrd, PX14EA_BOARD_SLAVE234, &slave234);
   PX14_RETURN_ON_FAIL(res);

   return static_cast<int>(slave234);
}

// Set FPGA processing enable
PX14API SetBoardProcessingEnablePX14 (HPX14 hBrd, unsigned int bEnable)
{
   int res;

   //// Make sure FPGA processing is enabled for this device
   //res = CheckFeaturePX14(hBrd, PX14FEATURE_FPGA_PROC);
   //PX14_RETURN_ON_FAIL(res);

   PX14U_REG_DEV_06 r6;
   r6.bits.FPGA_PROC = bEnable ? 1 : 0;
   res = WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_FPGA_PROC, r6.val);
   PX14_RETURN_ON_FAIL(res);

   // This code added 1.13.1
   // TEMP: Issuing DCM reset when board processing enabled
   if (bEnable)
   {
      res = SyncFirmwareToAdcClockPX14(hBrd, PX14_TRUE);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

// Get FPGA processing enable
PX14API GetBoardProcessingEnablePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 6, NULL, PX14REGREAD_DEFAULT);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg6.bits.FPGA_PROC);
}

// Set a FPGA processing parameter value
PX14API SetBoardProcessingParamPX14 (HPX14 hBrd,
                                     unsigned short idx,
                                     unsigned short value)
{
   if (idx & PX14_FPGAPROC_REG_READ_BIT)
      return SIG_PX14_INVALID_ARG_2;

   PX14U_REG_DEV_0A rA;
   rA.bits.preg_idx = idx;
   rA.bits.preg_val = value;
   return WriteDeviceRegPX14(hBrd, 0xA, 0xFFFFFFFF, rA.val);
}

// Get an FPGA processing parameter value from hardware
PX14API GetBoardProcessingParamPX14 (HPX14 hBrd,
                                     unsigned short idx,
                                     unsigned short* pVal)
{
   int res;

   PX14_ENSURE_POINTER(hBrd, pVal, unsigned short, "GetBoardProcessingParamPX14");

   if (idx & PX14_FPGAPROC_REG_READ_BIT)
      return SIG_PX14_INVALID_ARG_2;

   PX14U_REG_DEV_0A rA;
   rA.bits.preg_idx = idx;
   rA.bits.preg_idx |= PX14_FPGAPROC_REG_READ_BIT;
   rA.bits.preg_val = 0;
   res = WriteDeviceRegPX14(hBrd, 0xA, 0xFFFFFFFF, rA.val);
   PX14_RETURN_ON_FAIL(res);

   res = ReadDeviceRegPX14 (hBrd, 0xA, &rA.val, PX14REGREAD_HARDWARE);
   PX14_RETURN_ON_FAIL(res);

   *pVal = rA.bits.preg_val;
   return SIG_SUCCESS;
}

// Set channel 1 DC offset; DC-coupled devices only
PX14API SetDcOffsetCh1PX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int brd_rev;
   int res;

   // Clip input to max
   if (val > PX14_MAX_DC_OFFSET)
      val = PX14_MAX_DC_OFFSET;

   // DC offset programmed differently for PX14400D2 devices
   res = GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
   if ((SIG_SUCCESS == res) && (brd_rev == PX14BRDREV_PX14400D2))
   {
      PX14U_DACREGD2IO d2_io;
      d2_io.val = 0;
      d2_io.bits.dac_addr = PX14DACD2ADDR_DACA;
      d2_io.bits.dac_data = val;

      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);
   }
   else
   {
      // -- I1 used for low gain
      res = WriteDacRegPX14(hBrd, val,        PX14DACADDR_I1_POS, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDacRegPX14(hBrd, 4095 - val, PX14DACADDR_I1_NEG, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      // -- Q1 used for high gain
      res = WriteDacRegPX14(hBrd, val,        PX14DACADDR_Q1_POS, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDacRegPX14(hBrd, 4095 - val, PX14DACADDR_Q1_NEG, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
   }

   PX14U_DREG_05 dr5;
   dr5.bits.dc_offset_ch1 = val;
   return WriteDeviceRegPX14(hBrd, 5, PX14DREGMSK_05_DC_OFFSET_CH1, dr5.val, 0, 0, PX14REGSET_DRIVER);
}

// Get channel 1 DC offset; DC-coupled devices only
PX14API GetDcOffsetCh1PX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 5, NULL, PX14REGREAD_DEFAULT, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDriver.fields.dreg5.bits.dc_offset_ch1);
}

// Set channel 2 DC offset; DC-coupled devices only
PX14API SetDcOffsetCh2PX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int brd_rev;
   int res;

   // Clip input to max
   if (val > PX14_MAX_DC_OFFSET)
      val = PX14_MAX_DC_OFFSET;

   // DC offset programmed differently for PX14400D2 devices
   res = GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
   if ((SIG_SUCCESS == res) && (brd_rev == PX14BRDREV_PX14400D2))
   {
      PX14U_DACREGD2IO d2_io;
      d2_io.val = 0;
      d2_io.bits.dac_addr = PX14DACD2ADDR_DACF;
      d2_io.bits.dac_data = val;

      // 50us for serial delivery, 10us for settle time
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);
   }
   else
   {
      // -- I2 used for low gain
      res = WriteDacRegPX14(hBrd, val,        PX14DACADDR_I2_POS, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDacRegPX14(hBrd, 4095 - val, PX14DACADDR_I2_NEG, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      // -- Q2 used for high gain
      res = WriteDacRegPX14(hBrd, val,        PX14DACADDR_Q2_POS, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDacRegPX14(hBrd, 4095 - val, PX14DACADDR_Q2_NEG, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
   }

   PX14U_DREG_05 dr5;
   dr5.bits.dc_offset_ch2 = val;
   return WriteDeviceRegPX14(hBrd, 5, PX14DREGMSK_05_DC_OFFSET_CH2, dr5.val, 0, 0, PX14REGSET_DRIVER);
}

// Get channel 2 DC offset; DC-coupled devices only
PX14API GetDcOffsetCh2PX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 5, NULL, PX14REGREAD_DEFAULT, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDriver.fields.dreg5.bits.dc_offset_ch2);
}

// Set channel 1 fine DC offset; PX14400D2 devices only
PX14API SetFineDcOffsetCh1PX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int brd_rev;
   int res;

   // Clip input to max
   if (val > PX14_MAX_FINE_DC_OFFSET)
      val = PX14_MAX_FINE_DC_OFFSET;

   // DC offset programmed differently for PX14400D2 devices
   res = GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
   if ((SIG_SUCCESS == res) && (brd_rev == PX14BRDREV_PX14400D2))
   {
      PX14U_DACREGD2IO d2_io;
      d2_io.val = 0;
      d2_io.bits.dac_addr = PX14DACD2ADDR_DACB;
      d2_io.bits.dac_data = val << 1;

      // 50us for serial delivery, 10us for settle time
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);
   }

   PX14U_DREG_00 dr0;
   dr0.bits.dc_fine_offset_ch1 = val;
   return WriteDeviceRegPX14(hBrd, 0, PX14DREGMSK_DC_FINE_OFFSET_CH1, dr0.val, 0, 0, PX14REGSET_DRIVER);
}

// Get channel 1 fine DC offset; PX14400D2 devices only
PX14API GetFineDcOffsetCh1PX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 0, NULL, PX14REGREAD_DEFAULT, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDriver.fields.dreg0.bits.dc_fine_offset_ch1);
}

// Set channel 2 fine DC offset; PX14400D2 devices only
PX14API SetFineDcOffsetCh2PX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int brd_rev;
   int res;

   // Clip input to max
   if (val > PX14_MAX_FINE_DC_OFFSET)
      val = PX14_MAX_FINE_DC_OFFSET;

   // DC offset programmed differently for PX14400D2 devices
   res = GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
   if ((SIG_SUCCESS == res) && (brd_rev == PX14BRDREV_PX14400D2))
   {
      PX14U_DACREGD2IO d2_io;
      d2_io.val = 0;
      d2_io.bits.dac_addr = PX14DACD2ADDR_DACC;
      d2_io.bits.dac_data = val << 1;

      // 50us for serial delivery, 10us for settle time
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);
   }

   PX14U_DREG_00 dr0;
   dr0.bits.dc_fine_offset_ch2 = val;
   return WriteDeviceRegPX14(hBrd, 0, PX14DREGMSK_DC_FINE_OFFSET_CH2, dr0.val, 0, 0, PX14REGSET_DRIVER);
}

// Get channel 2 fine DC offset; PX14400D2 devices only
PX14API GetFineDcOffsetCh2PX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 0, NULL, PX14REGREAD_DEFAULT, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDriver.fields.dreg0.bits.dc_fine_offset_ch2);
}

// Set the PX14400's power-down override
PX14API SetPowerDownOverridePX14 (HPX14 hBrd, unsigned int bEnable)
{
   int res;

   PX14U_DREG_00 dr0;
   dr0.bits.pd_override = bEnable ? 1 : 0;
   res = WriteDeviceRegPX14(hBrd, 0, PX14DREGMSK_PD_OVERRIDE, dr0.val, 0, 0, PX14REGSET_DRIVER);
   PX14_RETURN_ON_FAIL(res);

   // Reapply active channel setting; this will update power down state
   return SetActiveChannelsPX14(hBrd, GetActiveChannelsPX14(hBrd));
}

// Get the PX14400's power-down override
PX14API GetPowerDownOverridePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 0, NULL, PX14REGREAD_DEFAULT, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDriver.fields.dreg0.bits.pd_override);
}

// Module private function implementation ------------------------------- //

/** @brief Set the SAB mode

  Sets the SAB mode setting. This routine is not exported because the
  library automatically manages the state of this setting. We know what
  mode we should be in by virtue of the current operating mode.
  */
int _SetSabModePX14 (HPX14 hBrd, unsigned int val)
{
   if (val >= PX14SABMODE__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_09 r9;
   r9.bits.MD = val;
   return WriteDeviceRegPX14(hBrd, 9, PX14REGMSK_SAB_MODE, r9.val);
}

/** @brief Get the SAB mode
*/
//int _GetSabModePX14 (HPX14 hBrd, int bFromCache)
//{
//	CStatePX14* statep;
//	int res;
//
//	res = ValidateHandle(hBrd, &statep);
//	PX14_RETURN_ON_FAIL(res);
//
//	if (!bFromCache)
//	{
//		res = ReadDeviceRegPX14(hBrd, 9);
//		PX14_RETURN_ON_FAIL(res);
//	}
//
//	return static_cast<int>(statep->m_regDev.fields.reg9.bits.MD);
//}

int _PreSabOp (HPX14 hBrd, int op_mode_sab)
{
   int res;

   if (!PX14_IS_SAB_MODE(op_mode_sab))
      return SIG_SUCCESS;

   // Make sure SAB is present on this device
   res = CheckFeaturePX14(hBrd, PX14FEATURE_SAB);
   PX14_RETURN_ON_FAIL(res);

   // Any SAB operation requires us to be a master
   res = _SetSabModePX14(hBrd, PX14SABMODE_MASTER);
   PX14_RETURN_ON_FAIL(res);

   if (0 == GetSabBoardNumberPX14(hBrd))
   {
      res = SetSabBoardNumberPX14(hBrd, 1);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

int _PostSabOp (HPX14 hBrd, int op_mode_sab)
{
   int res;

   if (!PX14_IS_SAB_MODE(op_mode_sab))
      return SIG_SUCCESS;

   // SAB operation complete, go back to being an active slave
   res = _SetSabModePX14(hBrd, PX14SABMODE_INACTIVE_SLAVE);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

int InitRegSetDevice (HPX14 hBrd)
{
   PX14U_DEVICE_REGISTER_SET* pRegs;
   CStatePX14* statep;
   unsigned r7_def;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   pRegs = &statep->m_regDev;

   pRegs->values[0x0] = PX14REGDEFAULT_0;
   pRegs->values[0x1] = PX14REGDEFAULT_1;
   pRegs->values[0x2] = PX14REGDEFAULT_2;
   pRegs->values[0x3] = PX14REGDEFAULT_3;
   pRegs->values[0x4] = PX14REGDEFAULT_4;
   pRegs->values[0x5] = PX14REGDEFAULT_5;
   pRegs->values[0x6] = PX14REGDEFAULT_6;

   // Register 7 default depends on channel implementations
   r7_def = PX14REGDEFAULT_7;	// Assuming default is amplified channels
   if (statep->m_chanImpCh1 & PX14CHANIMPF_NON_AMPLIFIED)
   {
      r7_def &= ~PX14REGMSK_ATT1;
      r7_def |=  PX14REGMSK_XFMR1_SEL;
   }
   if (statep->m_chanImpCh2 & PX14CHANIMPF_NON_AMPLIFIED)
   {
      r7_def &= ~PX14REGMSK_ATT2;
      r7_def |=  PX14REGMSK_XFMR2_SEL;
   }
   if (statep->m_boardRev == PX14BRDREV_PX14400D2)
   {
      unsigned long long hw_rev = 0;
      GetHardwareRevisionPX14(hBrd, &hw_rev);
      if (hw_rev && (hw_rev < PX14_VER64(2,0,0,0)))
      {
         // Rev1 PX14400D2 cards have a short on one of the pins
         //  used to select channel 2 input voltage range. This is
         //  worked around via hardware and software. This disable
         //  is used to prevent the firmware from driving the
         //  output.
         r7_def |= PX14REGMSK_D2_VR_SEL_PROTECT;
      }
   }
   pRegs->values[0x7] = r7_def;

   pRegs->values[0x8] = PX14REGDEFAULT_8;
   pRegs->values[0x9] = PX14REGDEFAULT_9;
   pRegs->values[0xA] = PX14REGDEFAULT_A;
   pRegs->values[0xB] = PX14REGDEFAULT_B;
   pRegs->values[0xC] = PX14REGDEFAULT_C;
   pRegs->values[0x10] = PX14REGDEFAULT_10;
   pRegs->values[0x11] = PX14REGDEFAULT_11;

   if (statep->IsPX12500())
   {
      // Default rate is 500MHz; DCM_500 should be set when >= 450MHz
      pRegs->fields.reg5.bits.DCM_500 = 1;
   }

   if (statep->m_features & PX14FEATURE_NO_INTERNAL_CLOCK)
   {
      pRegs->fields.reg5.bits.CLKSRC = PX14CLKSRC_EXTERNAL;
   }

   return SIG_SUCCESS;
}

int _InitializeDcOffsetDac (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   statep = PX14_H2B(hBrd);
   SIGASSERT_POINTER(statep, CStatePX14);

   if (statep->IsPX14400D2())
   {
      PX14U_DACREGD2IO d2_io;
      d2_io.val = 0;

      // Turn on reference
      d2_io.bits.dac_addr = PX14DACD2ADDR_CTRL0;
      // 0x6 = Powered up, DOUT off, Internal 2V reference on, straight binary
      d2_io.bits.dac_data = 0x6;
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);

      // Channel 1 - coarse offset
      d2_io.bits.dac_addr = PX14DACD2ADDR_DACA;
      d2_io.bits.dac_data = statep->m_regDriver.fields.dreg5.bits.dc_offset_ch1;
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);

      // Channel 2 - coarse offset
      d2_io.bits.dac_addr = PX14DACD2ADDR_DACF;
      d2_io.bits.dac_data = statep->m_regDriver.fields.dreg5.bits.dc_offset_ch2;
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);

      // Channel 1 - fine offset
      d2_io.bits.dac_addr = PX14DACD2ADDR_DACB;
      d2_io.bits.dac_data = statep->m_regDriver.fields.dreg0.bits.dc_fine_offset_ch1;
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);

      // Channel 2 - fine offset
      d2_io.bits.dac_addr = PX14DACD2ADDR_DACC;
      d2_io.bits.dac_data = statep->m_regDriver.fields.dreg0.bits.dc_fine_offset_ch2;
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                               0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
      PX14_RETURN_ON_FAIL(res);
   }
   else
   {
      // This function only has effect on DC-coupled devices

      // Turn on DAC's internal reference
      res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC, 0xFFFFFFFF, 0x08000001, 0, 4);
      PX14_RETURN_ON_FAIL(res);
      /*
      // Set all DAC outputs to mid-scale
      res = WriteDacRegPX14(hBrd, 2048, PX14DACADDR_ALL_DACS,
      PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      */

      int val = 2048;

      // -- I1 used for low gain
      res = WriteDacRegPX14(hBrd, val,        PX14DACADDR_I1_POS, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDacRegPX14(hBrd, 4095 - val, PX14DACADDR_I1_NEG, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      // -- Q1 used for high gain
      res = WriteDacRegPX14(hBrd, val,        PX14DACADDR_Q1_POS, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDacRegPX14(hBrd, 4095 - val, PX14DACADDR_Q1_NEG, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);

      // -- I2 used for low gain
      res = WriteDacRegPX14(hBrd, val,        PX14DACADDR_I2_POS, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDacRegPX14(hBrd, 4095 - val, PX14DACADDR_I2_NEG, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      // -- Q2 used for high gain
      res = WriteDacRegPX14(hBrd, val,        PX14DACADDR_Q2_POS, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
      res = WriteDacRegPX14(hBrd, 4095 - val, PX14DACADDR_Q2_NEG, PX14DACCMD_WRITE_AND_UPDATE);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

