/** @file	px14_clock.cpp
  @brief	PX14 clock management routines
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_remote.h"
#include "px14_config.h"

#define PX14_PHASE_DETECT_FREQ_MHZ				0.100
#define PX14_REF_FREQ_MHZ						10
#define PX14_REF_CLK_DIVIDER						\
   ((unsigned)(PX14_REF_FREQ_MHZ / PX14_PHASE_DETECT_FREQ_MHZ))

// Module-local function prototypes ------------------------------------- //

static int _GetDividersForAcqRate (double dRateMHz,
                                   int* vcoDivp, int* chanDivp);
static int _SetChanDivider (HPX14 hBrd, int chanDiv);		// Clock div 1
static int _SetVcoDivider (HPX14 hBrd, int vcoDiv);			// Clock div 2


static int _SetupVcoPll (CStatePX14& state);
static int _DisablePll (HPX14 hBrd);
static int _SetClockDevPowerSettings (HPX14 hBrd);
static int _UpdateFwDcmSel (HPX14 hBrd);
static int _UpdateSabDcmSelect (HPX14 hBrd);
static int _MasterSlaveSync (HPX14 hBrd);

// Call this anytime the clock rate has changed
static int _OnAcqRateChange (HPX14 hBrd);

// DC: Call this whenever active channels or gain select are changed
//  This function has no effect on AC-coupled devices
static int UpdateDcAmplifierPowerups (HPX14 hBrd);

// PX14 library exports implementation --------------------------------- //

/** @brief Set the PX14's source ADC clock

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  The identifier of the ADC clock source to set. Can be any of the
following:
- PX14CLKSRC_INT_VCO	    (0) Internal VCO (default)
- PX14CLKSRC_EXTERNAL		(1) External clock

@return
Returns SIG_SUCCESS on success or one of the SIG_* error values on
error.

@sa SetInternalAdcClockRatePX14, SetExternalClockRatePX14
*/
PX14API SetAdcClockSourcePX14 (HPX14 hBrd, unsigned int val)
{
   CStatePX14* statep;
   int res;

   if (val >= PX14CLKSRC__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if ((val == PX14CLKSRC_INT_VCO) &&
       (GetBoardFeaturesPX14(hBrd) & PX14FEATURE_NO_INTERNAL_CLOCK))
   {
      // No internal clock available on this board
      return SIG_PX14_INVALID_OP_FOR_BRD_CONFIG;
   }

   if (IsDeviceRemotePX14(hBrd))
      return rmc_SetAdcClockSource(hBrd, val);

   // - Update device registers

   // Select ADC clock source
   PX14U_REG_DEV_05 r5;
   r5.bits.CLKSRC = val;
   res = WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_ADCCLOCK_SOURCE, r5.val);
   PX14_RETURN_ON_FAIL(res);

   // Turn on/off various clocks depending on current settings
   res = _SetClockDevPowerSettings(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // Update clock gen registers
   if (val == PX14CLKSRC_INT_VCO)
   {
      // Setup the PLL (manages clock division)
      // Note: Still do this if we're going to disable it, it does other cfg
      res = _SetupVcoPll(*statep);
      PX14_RETURN_ON_FAIL(res);

      if (_GetPllDisablePX14(hBrd))
      {
         res = _DisablePll(hBrd);
         PX14_RETURN_ON_FAIL(res);
      }
   }
   else if (val == PX14CLKSRC_EXTERNAL)
   {
      // Disable PLL
      res = _DisablePll(hBrd);
      PX14_RETURN_ON_FAIL(res);

      // Setup clock division
      res = _SetChanDivider(hBrd,
                            statep->m_regDriver.fields.dreg0.bits.ext_clk_div1 + 1);
      PX14_RETURN_ON_FAIL(res);
      res = _SetVcoDivider(hBrd,
                           statep->m_regDriver.fields.dreg0.bits.ext_clk_div2 + 1);
      PX14_RETURN_ON_FAIL(res);
   }

   // Update firmware items dependent on acquisition rate
   res = _OnAcqRateChange(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // Make sure PLLs lock for non-slave internal clock
   if (val == PX14CLKSRC_INT_VCO)
   {
      int msCfg = GetMasterSlaveConfigurationPX14(hBrd);
      if (!PX14_IS_SLAVE(msCfg))
      {
         res = _WaitForPllLockPX14(hBrd, PX14_DEF_PLL_LOCK_WAIT_MS);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   //SysSleep (2);
   return ResetDcmPX14(hBrd);
}

/** @brief Get the PX14's source ADC clock

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the identifier of the current ADC clock source value
  (PX14CLKSRC_*) on success or one of the SIG_* error values on error.
  All SIG_* error values are negative.
  */
PX14API GetAdcClockSourcePX14 (HPX14 hBrd, int bFromCache)
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

   return static_cast<int>(statep->m_regDev.fields.reg5.bits.CLKSRC);
}

PX14API _SetPllDisablePX14 (HPX14 hBrd, int bDisable)
{
   int res;

   PX14U_DREG_00 dr0;
   dr0.bits.pll_disable = bDisable ? 1 : 0;
   res = WriteDeviceRegPX14(hBrd, 0, PX14DREGMSK_PLL_DISABLE, dr0.val,
                            0, 0, PX14REGSET_DRIVER);
   PX14_RETURN_ON_FAIL(res);

   if (PX14CLKSRC_INT_VCO == GetAdcClockSourcePX14(hBrd))
   {
      res = SetAdcClockSourcePX14(hBrd, PX14CLKSRC_INT_VCO);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

PX14API _GetPllDisablePX14 (HPX14 hBrd, int bFromCache)
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

   return static_cast<int>(statep->m_regDriver.fields.dreg0.bits.pll_disable);
}

/** @brief Defer waiting for PLL lock until later

  This function is used to enable/disable deferral the waiting for the
  internal acquisition clock PLL lock. This is useful when the software
  is about to change a bunch of hardware settings, serveral of which may
  modify a PLL parameter. By deferring the waiting for the lock the
  software can make all of the hardware settings changes and then wait
  for the PLL lock after.

  The PLL lock deferral is implemented with a reference count. When the
  deferral is enabled, an internal reference count is incremented. As long
  as this reference count is greater than zero _WaitForPllLockPX14 will do
  two things: set a flag to indicate that a PLL parameter has changed and
  then just return SIG_SUCCESS.

  When the PLL lock deferral is disabled, the reference count is
  decremented. When the count reaches zero, if the aforementioned flag is
  set then _WaitForPllLockPX14 will be called to wait for the PLL to lock.
  (Note that if the clock source is not internal at this point, there will
  be no wait for the lock.)

  Deferral of PLL lock check is only valid on the device handle for which
  the function is called. Each device handle has its own internal PLL lock
  deferral reference count. Reference count is not propagated via
  DuplicateHandlePX14.

  <B>Calling SetPowerupDefaultsPX14 will unconditionally reset the
  reference count back to zero and will always wait for PLLs to lock</B>

  If the operating mode is changed into an acquisition mode and the
  reference count is non-zero then the reference count will be explicitly
  reset to zero and _WaitForPllLockPX14 will be called.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bEnable
  If this parameter is non-zero then PLL lock check deferral will be
  enabled. Enabling the deferral will increment an internal reference
  count.
  */
PX14API _DeferPllLockCheckPX14 (HPX14 hBrd, int bEnable)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (bEnable)
      statep->m_pll_defer_cnt++;
   else
   {
      if (--statep->m_pll_defer_cnt < 0)
      {
         // Negative PLL lock check deferral reference count
         SIGASSERT(PX14_FALSE);
         statep->m_pll_defer_cnt = 0;
      }

      if ((0 == statep->m_pll_defer_cnt) && (statep->m_flags & PX14LIBF_NEED_PLL_LOCK_CHECK))
      {
         res = _WaitForPllLockPX14(hBrd);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   return SIG_SUCCESS;
}

PX14API _WaitForPllLockPX14 (HPX14 hBrd, unsigned int max_wait_ms)
{
   unsigned int reg_val, tick_start, tick_now;
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->m_pll_defer_cnt > 0)
   {
      // PLL lock check deferal is enabled; we'll wait for the PLL lock
      //  later on.
      statep->m_flags |= PX14LIBF_NEED_PLL_LOCK_CHECK;
      return SIG_SUCCESS;
   }

   statep->m_flags &= ~PX14LIBF_NEED_PLL_LOCK_CHECK;

   // PLL only applies to internal clock
   if (GetAdcClockSourcePX14(hBrd) != PX14CLKSRC_INT_VCO)
      return SIG_SUCCESS;
   if (IsDeviceVirtualPX14(hBrd) > 0)
      return SIG_SUCCESS;
   if (_GetPllDisablePX14(hBrd))
      return SIG_SUCCESS;

   // Give PLL a chance to stabilize
   SysSleep(150);

   tick_start = SysGetTickCount();
   while (1)
   {
      res = ReadDeviceRegPX14(hBrd, PX14REGIDX_CG_LOCK, &reg_val,
                              PX14REGREAD_HARDWARE);
      PX14_RETURN_ON_FAIL(res);

      if (reg_val & PX14REGMSK_CG_LOCK)
      {
         int i;

         // Lock bit set; should remain set
         for (i=0; i<10; i++)
         {
            res = ReadDeviceRegPX14(hBrd, PX14REGIDX_CG_LOCK, &reg_val,
                                    PX14REGREAD_HARDWARE);
            PX14_RETURN_ON_FAIL(res);

            if (0 == (reg_val & PX14REGMSK_CG_LOCK))
            {
               // Lock went low after being set; problem with clock
               return SIG_PX14_PLL_LOCK_FAILED_UNSTABLE;
            }
         }

         return SIG_SUCCESS;				// Locked
      }

      SysSleep(20);
      tick_now = SysGetTickCount();
      if (max_wait_ms && (tick_now - tick_start > max_wait_ms))
         break;							// Timed out
   }

   return SIG_PX14_PLL_LOCK_FAILED;
}

/** @brief Set the PX14's clock divider values

  The PX14 has two clock dividers that operate in series. Each clock
  divider can take a value between 1 and 32 for up to 354 unique clock
  divisions ranging from 1 to 1024.

  <br>
  Clock division is only applied when the 160MHz oscillator is selected
  as the clock source. Clock division is not required for 1.5GHz voltage
  controlled oscillator because a particular rate can be dialed in.

  <br>
  The effective clock divider is div1 * div2.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param div1
  The divider to use for clock divider #1 [1,32]
  @param div2
  The divider to use for clock divider #2 [1,32]

  @return
  Returns SIG_SUCCESS (0) on success or one of the SIG_* error
  values (<0) to indicate an error.

  @sa GetExtClockDivider1PX14, GetExtClockDivider2PX14
  */
PX14API SetExtClockDividersPX14 (HPX14 hBrd,
                                 unsigned int div1,
                                 unsigned int div2)
{
   CStatePX14* statep;
   int res;

   if (div1<0 || div1>PX14CLKDIV1_MAX)
      return SIG_PX14_INVALID_ARG_2;
   if (div2<0 || div2>PX14CLKDIV2_MAX)
      return SIG_PX14_INVALID_ARG_3;
   if (!div1) div1 = 1;
   if (!div2) div2 = 1;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   statep->m_flags |= PX14LIBF_IGNORE_ACQ_FREQ_CHANGES;

   res = SetExtClockDivider1PX14(hBrd, div1);
   if (SIG_SUCCESS == res)
      res = SetExtClockDivider2PX14(hBrd, div2);

   statep->m_flags &= ~PX14LIBF_IGNORE_ACQ_FREQ_CHANGES;

   PX14_RETURN_ON_FAIL(res);

   if (PX14CLKSRC_EXTERNAL == GetAdcClockSourcePX14(hBrd))
   {
      res = _OnAcqRateChange(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

/** @brief Set the PX14's #1 clock divider value
*/
PX14API SetExtClockDivider1PX14 (HPX14 hBrd, unsigned int div1)
{
   CStatePX14* statep;
   int res;

   res = GetAdcClockSourcePX14(hBrd, PX14_GET_FROM_CACHE);
   if (res < 0)
      return res;

   if (!div1) div1 = 1;
   if (div1 > PX14CLKDIV1_MAX)
      return SIG_PX14_INVALID_ARG_2;

   // Only set in hardware if we're not using the internal VCO.
   if (res != PX14CLKSRC_INT_VCO)
   {
      res = _SetChanDivider(hBrd, div1);
      PX14_RETURN_ON_FAIL(res);

      if (0 == (PX14_H2B(hBrd)->m_flags & PX14LIBF_IGNORE_ACQ_FREQ_CHANGES))
      {
         res = _OnAcqRateChange(hBrd);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   statep = PX14_H2B(hBrd);

   statep->m_regDriver.fields.dreg0.bits.ext_clk_div1 = div1 - 1;
   return WriteDeviceRegPX14(hBrd, PX14DREGIDX_CLK_DIV_1,
                            PX14DREGMSK_CLK_DIV_1, statep->m_regDriver.fields.dreg0.val,
                            0, 0, PX14REGSET_DRIVER);
   //PX14_RETURN_ON_FAIL(res);

   //SysSleep (2);
   //return ResetDcmPX14(hBrd);
}

/** @brief Get the PX14's #1 clock divider value
*/
PX14API GetExtClockDivider1PX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, PX14DREGIDX_CLK_DIV_1, NULL,
                              PX14REGREAD_CACHE_ONLY, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   return statep->m_regDriver.fields.dreg0.bits.ext_clk_div1 + 1;
}

/** @brief Set the PX14's #2 clock divider value
*/
PX14API SetExtClockDivider2PX14 (HPX14 hBrd, unsigned int div2)
{
   CStatePX14* statep;
   int res;

   res = GetAdcClockSourcePX14(hBrd, PX14_GET_FROM_CACHE);
   if (res < 0)
      return res;

   if (!div2) div2 = 1;
   if (div2 > PX14CLKDIV2_MAX)
      return SIG_PX14_INVALID_ARG_2;

   // Only set in hardware if we're not using the internal VCO
   if (res != PX14CLKSRC_INT_VCO)
   {
      res = _SetVcoDivider(hBrd, div2);
      PX14_RETURN_ON_FAIL(res);

      if (0 == (PX14_H2B(hBrd)->m_flags & PX14LIBF_IGNORE_ACQ_FREQ_CHANGES))
      {
         res = _OnAcqRateChange(hBrd);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   statep = PX14_H2B(hBrd);
   statep->m_regDriver.fields.dreg0.bits.ext_clk_div2 = div2 - 1;
   return WriteDeviceRegPX14(hBrd, PX14DREGIDX_CLK_DIV_2,
                            PX14DREGMSK_CLK_DIV_2, statep->m_regDriver.fields.dreg0.val,
                            0, 0, PX14REGSET_DRIVER);
   //PX14_RETURN_ON_FAIL(res);

   //SysSleep (2);
   //return ResetDcmPX14(hBrd);
}

/** @brief Get the PX14's #2 clock divider value
*/
PX14API GetExtClockDivider2PX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, PX14DREGIDX_CLK_DIV_2, NULL,
                              PX14REGREAD_CACHE_ONLY, PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   return statep->m_regDriver.fields.dreg0.bits.ext_clk_div2 + 1;
}

/** @brief
  Set the PX14's post-ADC clock divider; division by dropping samples

  The post-ADC clock divider is applied to both channels.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetPostAdcClockDividerPX14 (HPX14 hBrd, unsigned int val)
{
   int res;

   if (val >= PX14POSTADCCLKDIV__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update hardware
   PX14U_REG_DEV_05 r5;
   r5.bits.CD = val;
   res = WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_POSTADC_CLKDIV, r5.val);
   PX14_RETURN_ON_FAIL(res);

   res = _UpdateSabDcmSelect(hBrd);
   PX14_RETURN_ON_FAIL(res);

   //SysSleep (2);
   return ResetDcmPX14(hBrd);
}

/** @brief
  Get the PX14's post-ADC clock divider; division by dropping samples

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the identifier of the current pos-ADC clock divider value
  (PX14POSTADCCLKDIV_*) on success or one of the SIG_* error values on
  error. All SIG_* error values are negative.
  */
PX14API GetPostAdcClockDividerPX14 (HPX14 hBrd, int bFromCache)
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

   return static_cast<int>(statep->m_regDev.fields.reg5.bits.CD);
}

/** @brief Obtain effective acquisition rate considering post-ADC division

  @sa GetActualAdcAcqRatePX14
  */
PX14API GetEffectiveAcqRatePX14 (HPX14 hBrd, double* pRateMHz)
{
   int res;

   res = GetActualAdcAcqRatePX14(hBrd, pRateMHz);
   if (res != SIG_SUCCESS)
      return res;

   // Consider post-ADC clock division. (This is division by dropping
   //  samples after they've been digitized.)
   *pRateMHz /= (1 << GetPostAdcClockDividerPX14(hBrd, PX14_GET_FROM_CACHE));
   return SIG_SUCCESS;
}

/** @brief Obtain the actual ADC acquisition rate

  This routine is used to obtain the actual ADC acquisition rate
  taking the current ADC clock source and any clock dividers into
  consideration.

  This routine does not consider post-ADC clock division. For the
  effective acquisition rate, which does take post-ADC division into
  consideration, use the GetEffectiveAcqRatePX14 function.

  @sa GetEffectiveAcqRatePX14
  */
PX14API GetActualAdcAcqRatePX14 (HPX14 hBrd, double* pRateMHz)
{
   int clk_src, clk_div;
   double dAcqRate;

   SIGASSERT_POINTER(pRateMHz, double);
   if (NULL == pRateMHz)
      return SIG_PX14_INVALID_ARG_2;

   clk_src = GetAdcClockSourcePX14(hBrd, PX14_GET_FROM_CACHE);
   if (clk_src < 0)
      return clk_src;

   // Determine actual acquisition rate
   if (clk_src == PX14CLKSRC_INT_VCO)
   {
      // For internal VCO user supplies acquisition rate
      GetInternalAdcClockRatePX14(hBrd, &dAcqRate);
   }
   else
   {
      // Base external clock rate
      GetExternalClockRatePX14(hBrd, &dAcqRate);

      // Apply clock dividers
      clk_div = GetExtClockDivider1PX14(hBrd, PX14_GET_FROM_CACHE) *
         GetExtClockDivider2PX14(hBrd, PX14_GET_FROM_CACHE);
      SIGASSERT(clk_div > 0);
      if (clk_div > 0)
         dAcqRate /= clk_div;
   }

   *pRateMHz = dAcqRate;
   return SIG_SUCCESS;
}

/** @brief
  Set master/slave configuration; slaves are clock/trig synched with
  master

  Configuring a PX14 device as a slave will automatically change the
  clock source to external and disable all clock division

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetMasterSlaveConfigurationPX14 (HPX14 hBrd, unsigned int val)
{
   int res;

   if (val >= PX14MSCFG__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   res = ValidateHandle(hBrd);
   PX14_RETURN_ON_FAIL(res);

   // As it turns out, master/slave configuration is not 100% software
   //  selectable. If a card is being configured as slave 2, 3, or 4 then
   //  it requires a particular solder jumper to be set. The state of the
   //  solder jumper is located in the configuration EEPROM
   //
   // In addition to the above, beginning with system firmware 1.10.1
   //  slave #1 has moved in the hardware and now also requires a solder
   //  jumper setting. The new solder jumper configuration (which the
   //  software doesn't really care about) is identified by the
   //  PX14FEATURE_HWMSCFG_2 feature flag being set. When this bit is set
   //  then PX14EA_BOARD_SLAVE234 will contain the slave number (1,2,3,4)
   //  for which the hardware is configured.
   if (!IsDeviceVirtualPX14(hBrd))
   {
      unsigned short slave_sel;
      bool bHwMsCfg2;

      bHwMsCfg2 = 0 != (GetBoardFeaturesPX14(hBrd) & PX14FEATURE_HWMSCFG_2);
      if ((bHwMsCfg2 && PX14_IS_SLAVE(val)) ||
          ((val >= PX14MSCFG_SLAVE_2) && (val <= PX14MSCFG_SLAVE_4)))
      {
         res = GetEepromDataPX14(hBrd, PX14EA_BOARD_SLAVESEL, &slave_sel);
         PX14_RETURN_ON_FAIL(res);

         if (slave_sel && (slave_sel != 1 + (val - PX14MSCFG_SLAVE_1)))
            return  SIG_PX14_INVALID_OP_FOR_BRD_CONFIG;
      }
   }

   // Update hardware
   PX14U_REG_DEV_06 r6;
   r6.bits.MS_CFG = val;
   res = WriteDeviceRegPX14(hBrd, 6, PX14REGMSK_MS_CFG, r6.val);
   PX14_RETURN_ON_FAIL(res);

   if (PX14_IS_SLAVE(val))
   {
      SetAdcClockSourcePX14(hBrd, PX14CLKSRC_EXTERNAL);
      SetExtClockDividersPX14(hBrd, 1, 1);
   }
   else
   {
      // We're being set as a master or standalone board; reset ADC clock
      //  source since outputs may need to be updated
      res = SetAdcClockSourcePX14(hBrd, GetAdcClockSourcePX14(hBrd));
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

/** @brief
  Get master/slave configuration; slaves are clock/trig synched with
  master

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the identifier of the current master/slave configuration
  (PX14MSCFG_*) on success or one of the SIG_* error values on error.
  All SIG_* error values are negative.
  */
PX14API GetMasterSlaveConfigurationPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 5);
      if (SIG_SUCCESS != res)
         return res;
   }

   return static_cast<int>(statep->m_regDev.fields.reg6.bits.MS_CFG);
}

/// Set the ADC internal clock reference
PX14API SetInternalAdcClockReferencePX14 (HPX14 hBrd, unsigned int val)
{
   unsigned int mask;
   int res;

   if (val >= PX14CLKREF__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Update device register
   PX14U_REG_DEV_05 dr5;
   dr5.bits.CG_REF_SEL = val ? 1 : 0;
   res = WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_CG_REF_SEL, dr5.val);
   PX14_RETURN_ON_FAIL(res);

   // Next select correct reference in CG part
   PX14U_REG_CLK_LOG_04 r4;
   if (PX14CLKREF_INT_10MHZ == val)
   {
      // Using 10MHz internal reference

      r4.bits.SelectRef2 = 0;			// Select REF1
      r4.bits.Ref1PowerOn = 1;		// Turn on REF1
      r4.bits.Ref2PowerOn = 0;		// Turn off REF2
   }
   else
   {
      // Using external clock reference

      r4.bits.SelectRef2 = 1;			// Select REF1
      r4.bits.Ref1PowerOn = 0;		// Turn off REF1
      r4.bits.Ref2PowerOn = 1;		// Turn on REF2
   }
   // Use register to determine reference select; not pin
   r4.bits.UseRefSelPin = 0;

   mask =
      PX14CLKREGMSK_01C_REF1POWERON |
      PX14CLKREGMSK_01C_REF2POWERON |
      PX14CLKREGMSK_01C_USEREFSELPIN |
      PX14CLKREGMSK_01C_SELECTREF2;

   res = WriteClockGenRegPX14(hBrd, 0x1C, mask, r4.bytes[0], 4, 0,
                              0, 0, PX14_TRUE);
   PX14_RETURN_ON_FAIL(res);

   res = _OnAcqRateChange(hBrd);
   PX14_RETURN_ON_FAIL(res);

   if (PX14CLKSRC_INT_VCO == GetAdcClockSourcePX14(hBrd))
   {
      int msCfg = GetMasterSlaveConfigurationPX14(hBrd);

      if (!PX14_IS_SLAVE(msCfg))
      {
         res = _WaitForPllLockPX14(hBrd, PX14_DEF_PLL_LOCK_WAIT_MS);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   //SysSleep (2);
   return ResetDcmPX14(hBrd);
}

/// Get the ADC internal clock reference
PX14API GetInternalAdcClockReferencePX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadClockGenRegPX14(hBrd, 0x1C, 4, 0);
      PX14_RETURN_ON_FAIL(res);
   }

   return statep->m_regClkGen.fields.reg04.bits.SelectRef2
      ? PX14CLKREF_EXT : PX14CLKREF_INT_10MHZ;
}

/** @brief Set the assumed external clock rate

  The assumed external clock rate has no effect on the underlying PX14
  hardware. It exists solely as a convenience for the applications
  programming. The updating of this setting is up to the end user since
  the software cannot know the external clock rate.

  The GetEffectiveClockRatePX14 function implementation will use this
  assumed rate when calculating the effective clock rate when the external
  clock is selected.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param dRateMHz
  The assumed external clock rate in MHz. An error will be returned
  if this is not a well-formed finite value.

  @retval
  Returns SIG_SUCCESS (0) on success or one of the (negative) SIG_*
  error values on error
  */
PX14API SetExternalClockRatePX14 (HPX14 hBrd, double dRateMHz)
{
   CStatePX14* statep;
   double dCurRate;
   int res;

   if (!isfinite(dRateMHz))
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   dCurRate = statep->GetExtRateMHz();
   statep->SetExtRateMHz(dRateMHz);

   // Update driver with value
   res = WriteDeviceRegPX14(hBrd, 3, 0xFFFFFFFF,
                            statep->m_regDriver.values[3], 0, 0, PX14REGSET_DRIVER);
   PX14_RETURN_ON_FAIL(res);
   res =  WriteDeviceRegPX14(hBrd, 4, 0xFFFFFFFF,
                             statep->m_regDriver.values[4], 0, 0, PX14REGSET_DRIVER);
   PX14_RETURN_ON_FAIL(res);

   // If we've changed rates then we'll need to resynchronize the firmware
   //  with the clock
   if ((dCurRate != dRateMHz) &&
       (PX14CLKSRC_EXTERNAL == GetAdcClockSourcePX14(hBrd)))
   {
      // Update firmware items dependent on acquisition rate
      res = _OnAcqRateChange(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

/** @brief Obtain the assumed external clock rate
*/
PX14API GetExternalClockRatePX14 (HPX14 hBrd, double* ratep, int bFromCache)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(ratep, double);
   if (NULL == ratep)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 3, NULL, PX14REGREAD_CACHE_ONLY,
                              PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
      res = ReadDeviceRegPX14(hBrd, 4, NULL, PX14REGREAD_CACHE_ONLY,
                              PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   *ratep = statep->GetExtRateMHz();
   return SIG_SUCCESS;
}

/** @brief Set the ADC clock rate

  Only applies to internal 1.5 GHz VCO
  */
PX14API SetInternalAdcClockRatePX14 (HPX14 hBrd, double dRateMHz)
{
   CStatePX14* statep;
   int res;

   if (!isfinite(dRateMHz))
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // Validate acquistion rate
   res = _GetDividersForAcqRate(dRateMHz, NULL, NULL);
   PX14_RETURN_ON_FAIL(res);

   // Can only call this function while the board is not acquiring
   res = InAcquisitionModePX14(hBrd);
   if (res < 0)
      return res;
   if (res > 0)
      return SIG_INVALID_MODE;

   statep->SetIntAcqRateMHz(dRateMHz);
   WriteDeviceRegPX14(hBrd, 1, 0xFFFFFFFF,
                      statep->m_regDriver.values[1], 0, 0, PX14REGSET_DRIVER);
   WriteDeviceRegPX14(hBrd, 2, 0xFFFFFFFF,
                      statep->m_regDriver.values[2], 0, 0, PX14REGSET_DRIVER);

   if (GetAdcClockSourcePX14(hBrd, PX14_GET_FROM_CACHE) == PX14CLKSRC_INT_VCO)
   {
      // Reset dividers and PLL stuff
      res = SetAdcClockSourcePX14(hBrd, PX14CLKSRC_INT_VCO);
      PX14_RETURN_ON_FAIL(res);
   }

   SysSleep (25);
   return ResetDcmPX14(hBrd);
}

/** @brief
  Get the ADC clock rate as specified by the SetInternalAdcClockRatePX14
  function
  */
PX14API GetInternalAdcClockRatePX14 (HPX14 hBrd, double* ratep, int bFromCache)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(ratep, double);
   if (NULL == ratep)
      return SIG_PX14_INVALID_ARG_2;
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 1, NULL, PX14REGREAD_CACHE_ONLY,
                              PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
      res = ReadDeviceRegPX14(hBrd, 2, NULL, PX14REGREAD_CACHE_ONLY,
                              PX14REGSET_DRIVER);
      PX14_RETURN_ON_FAIL(res);
   }

   *ratep = statep->GetIntAcqRateMHz();
   return SIG_SUCCESS;
}

/** @brief Initialize all clock generator device

  This routine is used to initialize the clock generator device to
  proper power-up defaults. The implementation assumes the following
settings:

Clock source: Internal VCO @ 1500MHz
Outputs     : OUT0 and OUT1 turned on; all others turned off

Note: The PX14 driver should automatically do this operation upon
connecting to the hardware. This function exists for troubleshooting
purposes only.
*/
PX14API InitializeClockGeneratorPX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   PX14U_CLKGEN_REGISTER_SET rs;
   memset (&rs, 0, sizeof(rs));

   // Do a soft reset on clock generator device
   rs.values[0] = PX14CLKREGDEF_00;
   rs.fields.reg00.bits.SoftReset = 1;
   rs.fields.reg00.bits.SoftResetM = 1;
   // Register update not required for soft reset
   WriteClockGenRegPX14(hBrd, 0, 0xFF, rs.fields.reg00.bytes[0], 0, 0);
   rs.fields.reg00.bits.SoftReset = 0;
   rs.fields.reg00.bits.SoftResetM = 0;
   WriteClockGenRegPX14(hBrd, 0, 0xFF, rs.fields.reg00.bytes[0], 0, 0, 0, 25);

   // -- Setup a local register set with default values

   rs.values[ 0] = PX14CLKREGDEF_00;
   rs.values[ 1] = PX14CLKREGDEF_01;
   rs.values[ 2] = PX14CLKREGDEF_02;
   rs.values[ 3] = PX14CLKREGDEF_03;
   rs.values[ 4] = PX14CLKREGDEF_04;
   rs.values[ 5] = PX14CLKREGDEF_05;
   rs.values[ 6] = PX14CLKREGDEF_06;
   rs.values[ 7] = PX14CLKREGDEF_07;
   rs.values[ 8] = PX14CLKREGDEF_08;
   rs.values[ 9] = PX14CLKREGDEF_09;
   rs.values[10] = PX14CLKREGDEF_10;
   rs.values[11] = PX14CLKREGDEF_11;
   rs.values[12] = PX14CLKREGDEF_12;
   rs.values[13] = PX14CLKREGDEF_13;
   rs.values[14] = PX14CLKREGDEF_14;
   rs.values[15] = PX14CLKREGDEF_15;
   rs.values[16] = PX14CLKREGDEF_16;
   rs.values[17] = PX14CLKREGDEF_17;
   rs.values[18] = PX14CLKREGDEF_18;
   rs.values[19] = PX14CLKREGDEF_19;

   if (statep->IsPX12500())
   {
      rs.values[12] = PX14CLKREGDEF_12_PX12500;
      rs.values[19] = PX14CLKREGDEF_19_PX12500;
   }

   if (statep->m_features & PX14FEATURE_NO_INTERNAL_CLOCK)
   {
      rs.fields.reg01.bits.PLL_PowerDown = 1;
      rs.fields.reg19.bits.BypassVcoDivider = 1;
   }

   // -- Update hardware with these defaults
   // (Note: Active CG registers not updated until last write)

   // - Logical register 0
   // CG reg 000
   res = WriteClockGenRegPX14(hBrd, 0x000, 0xFF,
                              rs.fields.reg00.bytes[0], 0, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 004
   res = WriteClockGenRegPX14(hBrd, 0x004, 0xFF,
                              rs.fields.reg00.bytes[1], 0, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 1
   // CG reg 010
   res = WriteClockGenRegPX14(hBrd, 0x010, 0xFF,
                              rs.fields.reg01.bytes[0], 1, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 011
   res = WriteClockGenRegPX14(hBrd, 0x011, 0xFF,
                              rs.fields.reg01.bytes[1], 1, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 012
   res = WriteClockGenRegPX14(hBrd, 0x012, 0xFF,
                              rs.fields.reg01.bytes[2], 1, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 013
   res = WriteClockGenRegPX14(hBrd, 0x013, 0xFF,
                              rs.fields.reg01.bytes[3], 1, 3, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 2
   // CG reg 014
   res = WriteClockGenRegPX14(hBrd, 0x014, 0xFF,
                              rs.fields.reg02.bytes[0], 2, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 015
   res = WriteClockGenRegPX14(hBrd, 0x015, 0xFF,
                              rs.fields.reg02.bytes[1], 2, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 016
   res = WriteClockGenRegPX14(hBrd, 0x016, 0xFF,
                              rs.fields.reg02.bytes[2], 2, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 017
   res = WriteClockGenRegPX14(hBrd, 0x017, 0xFF,
                              rs.fields.reg02.bytes[3], 2, 3, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 3
   // CG reg 018
   res = WriteClockGenRegPX14(hBrd, 0x018, 0xFF,
                              rs.fields.reg03.bytes[0], 3, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 019
   res = WriteClockGenRegPX14(hBrd, 0x019, 0xFF,
                              rs.fields.reg03.bytes[1], 3, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 01A
   res = WriteClockGenRegPX14(hBrd, 0x01A, 0xFF,
                              rs.fields.reg03.bytes[2], 3, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 01B
   res = WriteClockGenRegPX14(hBrd, 0x01B, 0xFF,
                              rs.fields.reg03.bytes[3], 3, 3, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 4
   // CG reg 01C
   res = WriteClockGenRegPX14(hBrd, 0x01C, 0xFF,
                              rs.fields.reg04.bytes[0], 4, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 01D
   res = WriteClockGenRegPX14(hBrd, 0x01D, 0xFF,
                              rs.fields.reg04.bytes[1], 4, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 01E
   res = WriteClockGenRegPX14(hBrd, 0x01E, 0xFF,
                              rs.fields.reg04.bytes[2], 4, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 01F
   res = WriteClockGenRegPX14(hBrd, 0x01F, 0xFF,
                              rs.fields.reg04.bytes[3], 4, 3, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 5
   // CG reg 0A0
   res = WriteClockGenRegPX14(hBrd, 0x0A0, 0xFF,
                              rs.fields.reg05.bytes[0], 5, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0A1
   res = WriteClockGenRegPX14(hBrd, 0x0A1, 0xFF,
                              rs.fields.reg05.bytes[1], 5, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0A2
   res = WriteClockGenRegPX14(hBrd, 0x0A2, 0xFF,
                              rs.fields.reg05.bytes[2], 5, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 6
   // CG reg 0A3
   res = WriteClockGenRegPX14(hBrd, 0x0A3, 0xFF,
                              rs.fields.reg06.bytes[0], 6, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0A4
   res = WriteClockGenRegPX14(hBrd, 0x0A4, 0xFF,
                              rs.fields.reg06.bytes[1], 6, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0A5
   res = WriteClockGenRegPX14(hBrd, 0x0A5, 0xFF,
                              rs.fields.reg06.bytes[2], 6, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 7
   // CG reg 0A6
   res = WriteClockGenRegPX14(hBrd, 0x0A6, 0xFF,
                              rs.fields.reg07.bytes[0], 7, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0A7
   res = WriteClockGenRegPX14(hBrd, 0x0A7, 0xFF,
                              rs.fields.reg07.bytes[1], 7, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0A8
   res = WriteClockGenRegPX14(hBrd, 0x0A8, 0xFF,
                              rs.fields.reg07.bytes[2], 7, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 8
   // CG reg 0A9
   res = WriteClockGenRegPX14(hBrd, 0x0A9, 0xFF,
                              rs.fields.reg08.bytes[0], 8, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0AA
   res = WriteClockGenRegPX14(hBrd, 0x0AA, 0xFF,
                              rs.fields.reg08.bytes[1], 8, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0AB
   res = WriteClockGenRegPX14(hBrd, 0x0AB, 0xFF,
                              rs.fields.reg08.bytes[2], 8, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 9
   // CG reg 0F0
   res = WriteClockGenRegPX14(hBrd, 0x0F0, 0xFF,
                              rs.fields.reg09.bytes[0], 9, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0F1
   res = WriteClockGenRegPX14(hBrd, 0x0F1, 0xFF,
                              rs.fields.reg09.bytes[1], 9, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0F2
   res = WriteClockGenRegPX14(hBrd, 0x0F2, 0xFF,
                              rs.fields.reg09.bytes[2], 9, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0F3
   res = WriteClockGenRegPX14(hBrd, 0x0F3, 0xFF,
                              rs.fields.reg09.bytes[3], 9, 3, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 10
   // CG reg 0F4
   res = WriteClockGenRegPX14(hBrd, 0x0F4, 0xFF,
                              rs.fields.reg10.bytes[0], 10, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 0F5
   res = WriteClockGenRegPX14(hBrd, 0x0F5, 0xFF,
                              rs.fields.reg10.bytes[1], 10, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 11
   // CG reg 140
   res = WriteClockGenRegPX14(hBrd, 0x140, 0xFF,
                              rs.fields.reg11.bytes[0], 11, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 141
   res = WriteClockGenRegPX14(hBrd, 0x141, 0xFF,
                              rs.fields.reg11.bytes[1], 11, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 142
   res = WriteClockGenRegPX14(hBrd, 0x142, 0xFF,
                              rs.fields.reg11.bytes[2], 11, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 143
   res = WriteClockGenRegPX14(hBrd, 0x143, 0xFF,
                              rs.fields.reg11.bytes[3], 11, 3, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 12
   // CG reg 190
   res = WriteClockGenRegPX14(hBrd, 0x190, 0xFF,
                              rs.fields.reg12.bytes[0], 12, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 191
   res = WriteClockGenRegPX14(hBrd, 0x191, 0xFF,
                              rs.fields.reg12.bytes[1], 12, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 192
   res = WriteClockGenRegPX14(hBrd, 0x192, 0xFF,
                              rs.fields.reg12.bytes[2], 12, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 13
   // CG reg 193
   res = WriteClockGenRegPX14(hBrd, 0x193, 0xFF,
                              rs.fields.reg13.bytes[0], 13, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 194
   res = WriteClockGenRegPX14(hBrd, 0x194, 0xFF,
                              rs.fields.reg13.bytes[1], 13, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 195
   res = WriteClockGenRegPX14(hBrd, 0x195, 0xFF,
                              rs.fields.reg13.bytes[2], 13, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 14
   // CG reg 196
   res = WriteClockGenRegPX14(hBrd, 0x196, 0xFF,
                              rs.fields.reg14.bytes[0], 14, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 197
   res = WriteClockGenRegPX14(hBrd, 0x197, 0xFF,
                              rs.fields.reg14.bytes[1], 14, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 198
   res = WriteClockGenRegPX14(hBrd, 0x198, 0xFF,
                              rs.fields.reg14.bytes[2], 14, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 15
   // CG reg 199
   res = WriteClockGenRegPX14(hBrd, 0x199, 0xFF,
                              rs.fields.reg15.bytes[0], 15, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 16
   // CG reg 19A
   res = WriteClockGenRegPX14(hBrd, 0x19A, 0xFF,
                              rs.fields.reg16.bytes[0], 16, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 19B
   res = WriteClockGenRegPX14(hBrd, 0x19B, 0xFF,
                              rs.fields.reg16.bytes[1], 16, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 19C
   res = WriteClockGenRegPX14(hBrd, 0x19C, 0xFF,
                              rs.fields.reg16.bytes[2], 16, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 19D
   res = WriteClockGenRegPX14(hBrd, 0x19D, 0xFF,
                              rs.fields.reg16.bytes[3], 16, 3, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 17
   // CG reg 19E
   res = WriteClockGenRegPX14(hBrd, 0x19E, 0xFF,
                              rs.fields.reg17.bytes[0], 17, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 18
   // CG reg 19F
   res = WriteClockGenRegPX14(hBrd, 0x19F, 0xFF,
                              rs.fields.reg18.bytes[0], 18, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 1A0
   res = WriteClockGenRegPX14(hBrd, 0x1A0, 0xFF,
                              rs.fields.reg18.bytes[1], 18, 1, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 1A1
   res = WriteClockGenRegPX14(hBrd, 0x1A1, 0xFF,
                              rs.fields.reg18.bytes[2], 18, 2, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 1A2
   res = WriteClockGenRegPX14(hBrd, 0x1A2, 0xFF,
                              rs.fields.reg18.bytes[3], 18, 3, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);

   // - Logical register 19
   // CG reg 1E0
   res = WriteClockGenRegPX14(hBrd, 0x1E0, 0xFF,
                              rs.fields.reg19.bytes[0], 19, 0, 0, 0, PX14_FALSE);
   PX14_RETURN_ON_FAIL(res);
   // CG reg 1E1
   res = WriteClockGenRegPX14(hBrd, 0x1E1, 0xFF,
                              rs.fields.reg19.bytes[1], 19, 1, 0, 0, PX14_TRUE);	// Last write, update regs
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

/** @brief Resynchronize clock component outputs

  This operation needs to take place any time a clock source or rate
  changes. It ensures that ADC clocks are in phase.
  */
PX14API ResyncClockOutputsPX14 (HPX14 hBrd)
{
   int res;

   // Toggle the CG_SYNC_ (active low) bit in register 5
   res = WriteDeviceRegPX14(hBrd, PX14REGIDX_CG_SYNC_,
                            PX14REGMSK_CG_SYNC_, 0x00000000);
   PX14_RETURN_ON_FAIL(res);
   // Read Device register to force flush of previous write
   ReadDeviceRegPX14(hBrd, 0xD);

   res = WriteDeviceRegPX14(hBrd, PX14REGIDX_CG_SYNC_,
                            PX14REGMSK_CG_SYNC_, 0xFFFFFFFF);
   PX14_RETURN_ON_FAIL(res);
   // Read Device register to force flush of previous write
   ReadDeviceRegPX14(hBrd, 0xD);

   return SIG_SUCCESS;
}

// Set manual DCM disable state
PX14API SetManualDcmDisablePX14 (HPX14 hBrd, unsigned int bDisable)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (bDisable)
      statep->m_flags |=  PX14LIBF_MANUAL_DCM_DISABLE;
   else
      statep->m_flags &= ~PX14LIBF_MANUAL_DCM_DISABLE;

   return _OnAcqRateChange(hBrd);		// Disabled manual DCM control
}

// Get manual DCM disable state
PX14API GetManualDcmDisablePX14 (HPX14 hBrd, int bFromHardware)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   return (statep->m_flags & PX14LIBF_MANUAL_DCM_DISABLE) ? 1 : 0;
}

/** @brief Set active channels; defines which channels are digitized

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param val
  Document me

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API SetActiveChannelsPX14 (HPX14 hBrd, unsigned int val)
{
   int res;

   if (val >= PX14CHANNEL__COUNT)
      return SIG_PX14_INVALID_ARG_2;

#if 0
   if (val == PX14CHANNEL_TWO)
   {
      // Single channel - channel 2 available with system firmware 1.5.1 and higher
      res = CheckFirmwareVerPX14(hBrd, PX14_VER32(1,5,1,0));
      PX14_RETURN_ON_FAIL(res);
      PX14_CT_ASSERT(0x31 == PX14_BOARD_TYPE);
   }
#endif

   // Update hardware
   PX14U_REG_DEV_07 r7;
   switch (val)
   {
      default:
      case PX14CHANNEL_DUAL: r7.bits.CHANSEL = 0; break;
      case PX14CHANNEL_ONE:  r7.bits.CHANSEL = 2; break;
      case PX14CHANNEL_TWO:  r7.bits.CHANSEL = 3; break;
   }
   PX14_CT_ASSERT(3 == PX14CHANNEL__COUNT);
   // Now make sure ADCs are powered up/down
   r7.bits.ADCPD1 = 0;		// ADC1 always on; FPGA uses output clock
   r7.bits.ADCPD2 =
      (!GetPowerDownOverridePX14(hBrd) && (val == PX14CHANNEL_ONE)) ? 1 : 0;
   res = WriteDeviceRegPX14(hBrd, 7,
                            PX14REGMSK_CHANSEL | PX14REGMSK_ADCPD1 | PX14REGMSK_ADCPD2, r7.val);
   PX14_RETURN_ON_FAIL(res);

   res = _SetClockDevPowerSettings(hBrd);
   PX14_RETURN_ON_FAIL(res);
   res = _OnAcqRateChange(hBrd);
   PX14_RETURN_ON_FAIL(res);

   res = UpdateDcAmplifierPowerups(hBrd);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

/** @brief Get active channels; defines which channels are digitized

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromCache
  If non-zero, the setting will be read from the local device register
  cache associated with the given PX14 handle, which will result in
  no hardware or driver access. If zero, the setting is obtained from
  the driver which may or may not result in an actual PX14 device
  register read.

  @return
  Returns the current active channels selection value (PX14CHANNEL_*)
  on success or one of the SIG_* error values on error. All SIG_*
  error values are negative.
  */
PX14API GetActiveChannelsPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 7);
      PX14_RETURN_ON_FAIL(res);
   }

   switch (statep->m_regDev.fields.reg7.bits.CHANSEL)
   {
      default:
      case 0:	res = PX14CHANNEL_DUAL; break;
      case 2: res = PX14CHANNEL_ONE; break;
      case 3: res = PX14CHANNEL_TWO; break;
   }
   PX14_CT_ASSERT(3 == PX14CHANNEL__COUNT);

   return res;
}

// Set active channel mask; defines which channels are digitized
PX14API SetActiveChannelsMaskPX14 (HPX14 hBrd, unsigned int chan_mask)
{
   int active_channels;

   PX14_CT_ASSERT(PX14CHANNEL__COUNT == 3);
   switch (chan_mask)
   {
      case 0x3: active_channels = PX14CHANNEL_DUAL; break;
      case 0x1: active_channels = PX14CHANNEL_ONE;  break;
      case 0x2: active_channels = PX14CHANNEL_TWO;  break;
      default:  active_channels = -1; break;
   }

   return SetActiveChannelsPX14(hBrd, active_channels);
}

// Get active channel mask; defines which channels are digitized
PX14API GetActiveChannelMaskPX14 (HPX14 hBrd, int bFromCache)
{
   int active_channels, chan_mask;

   if ((active_channels = GetActiveChannelsPX14(hBrd, bFromCache)) < 0)
      return active_channels;

   PX14_CT_ASSERT(PX14CHANNEL__COUNT == 3);
   switch (active_channels)
   {
      case PX14CHANNEL_DUAL: chan_mask = 0x3; break;
      case PX14CHANNEL_ONE:  chan_mask = 0x1; break;
      case PX14CHANNEL_TWO:  chan_mask = 0x2; break;
      default:
         SIGASSERT(0);
         chan_mask = SIG_PX14_UNEXPECTED;
         break;
   }

   return chan_mask;
}

// Returns non-zero if given active channel mask is supported by the device
PX14API IsActiveChannelMaskSupportedPX14 (HPX14 hBrd, unsigned int chan_mask)
{
   // At this time, all PX14400 devices support the same channel masks
   (void)hBrd;

   return
      (chan_mask == 0x3) ||
      (chan_mask == 0x1) ||
      (chan_mask == 0x2);
}

/** @brief
  Returns the absolute 0-based channel index from the relative channel
  index of a particular channel mask

  Note that for the PX14400, this function isn't terribly exciting. This
  functionality is more helpful for boards with larger active channel
  counts (and therefore more varied potential channel masks) such as the
  PX1500. This function exists as a convenience for porting code to/from
  other data acquisition devices.

  This function is used for determining what physical, absolute channel
  corresponds to a relative channel index from a given channel mask.

  The only example where this function is useful with the PX14400 is for
  finding out which physical channel is being used for a single channel
  acquisition. The PX14400 has two possible single channel active channel
selections: Channel 1 or Channel 2. Because the data is single channel,
the only valid relative index is 0. This function will look at the given
channel mask and tell you that relative index maps to absolute index 0
or 1 (channel 1 or channel 2) depending on the actual channel mask.

@return Returns absolute channel index or -1 on invalid rel_idx
*/
PX14API GetAbsChanIdxFromChanMaskPX14 (int chan_mask, int rel_idx)
{
   int i, f, cur_idx;

   for (f=1,cur_idx=i=0; i<PX14_MAX_CHANNELS; i++,f<<=1)
   {
      if ((f & chan_mask) && (cur_idx++ == rel_idx))
         return i;
   }

   return -1;
}

/** @brief
  Returns the relative 0-based channel index from the absolulte channel
  index of a particular channel mask

  This function is the inverse of GetAbsChanIdxFromChanMaskPX14. It will
  return the relative channel index for the given absolute index in the
  given channel mask.

  @return Returns relative channel index or -1 if abs_idx in not set in channel mask
  */
PX14API GetRelChanIdxFromChanMaskPX14 (int chan_mask, int abs_idx)
{
   int i, f, rel_idx;

   if (0 == (chan_mask & (1 << abs_idx)))
      return -1;

   for (rel_idx=-1,i=0,f=1; i<=abs_idx; i++,f<<=1)
   {
      if (f & chan_mask)
         rel_idx++;
   }

   return rel_idx;
}

/// Returns the number of channels active in the given channel mask
PX14API GetChanCountFromChanMaskPX14 (int chan_mask)
{
   int i, res;

   for (i=0,res=0; i<PX14_MAX_CHANNELS; i++)
   {
      if (chan_mask & (1 << i))
         res++;
   }

   return res;
}

// DC-coupled devices: Set input gain level (PX14DGAIN_*)
PX14API SetInputGainLevelDcPX14 (HPX14 hBrd, unsigned int val)
{
   int res;

   if (val >= PX14DGAIN__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   PX14U_REG_DEV_07 r7;
   r7.bits.higain = val ? 1 : 0;
   res = WriteDeviceRegPX14(hBrd, 7, PX14REGMSK_HIGAIN, r7.val);
   PX14_RETURN_ON_FAIL(res);

   return UpdateDcAmplifierPowerups(hBrd);
}

// DC-coupled devices: Get input gain level (PX14DGAIN_*)
PX14API GetInputGainLevelDcPX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 7);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg7.bits.higain);
}

// Module private function implementation ------------------------------- //


/** @brief Call this anytime the clock rate has changed

  This function takes care of updating all firmware items that are
  dependent on the board's acquisition rate.
  */
int _OnAcqRateChange (HPX14 hBrd)
{
   int res, msCfg;
   unsigned bSet;

   // Make sure correct DCM selections are in place
   res = _UpdateFwDcmSel(hBrd);
   PX14_RETURN_ON_FAIL(res);
   res = _UpdateSabDcmSelect(hBrd);
   PX14_RETURN_ON_FAIL(res);

   msCfg = GetMasterSlaveConfigurationPX14(hBrd);

   // Driver keeps track of this and applies DCM reset prior to next
   //  acquisition
   bSet = 1;
   res = DeviceRequest(hBrd, IOCTL_PX14_NEED_DCM_RESET,
                       &bSet, sizeof(unsigned));
   PX14_RETURN_ON_FAIL(res);

   if (PX14_IS_MASTER(msCfg) || PX14_IS_SLAVE(msCfg))
   {
      // Make sure master/slaves are synchronized
      res = _MasterSlaveSync(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }
   else
   {
      res = ResyncClockOutputsPX14(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }

   //SysSleep (2);
   return ResetDcmPX14(hBrd);
}

int _UpdateFwDcmSel (HPX14 hBrd)
{
   double dRateMHz, dFastCutoff, dSlowCutoff;
   bool bDcmOverride, bPX12500;
   PX14U_REG_DEV_05 r5;
   CStatePX14* statep;
   int res;

   res = GetActualAdcAcqRatePX14(hBrd, &dRateMHz);
   PX14_RETURN_ON_FAIL(res);

   statep = PX14_H2B(hBrd);
   bDcmOverride = 0 != (statep->m_flags & PX14LIBF_MANUAL_DCM_DISABLE);
   bPX12500 = statep->IsPX12500();

   if (bPX12500)
   {
      /*res = CheckFirmwareVerPX14(hBrd,
        PX14_VER32(1,6,0,0), PX14FWTYPE_SYS, PX14VCT_GT);
        if (SIG_SUCCESS == res)*/
      r5.bits.DCM_500 = dRateMHz < 380 ? 0 : 1;
      /*else
        r5.bits.DCM_500 = dRateMHz < 450 ? 0 : 1;*/
   }
   else
      r5.bits.DCM_500 = 0;	// Unused for non-PX12500
   // 32 MHz is the cut off for "slow" DCM, but fw gets ADC clock / 2
   dSlowCutoff = bPX12500 ? ( 40 * 2) : ( 32 * 2);
   r5.bits.NO_DCM  = (bDcmOverride || (dRateMHz <= dSlowCutoff)) ? 1 : 0;
   // 135 MHz is the cut off for "fast" DCM, but fw gets ADC clock / 2
   dFastCutoff = bPX12500 ? (140 * 2) : (135 * 2);
   r5.bits.DCM_SEL = (dRateMHz <= dFastCutoff) ? 1 : 0;
   return WriteDeviceRegPX14(hBrd, 5,
                             PX14REGMSK_DCM_SEL | PX14REGMSK_NO_DCM | PX14REGMSK_DCM_500, r5.val);
}

/**
  This algorithm is based off of the ADC Clock Synthesizer section in the
  PX14 Implementation Document

  This function may be used to validate an internal VCO acquisition rate
  */
int _GetDividersForAcqRate (double dRateMHz, int* vcoDivp, int* chanDivp)
{
   int vcoDiv, chanDiv;

   SIGASSERT_NULL_OR_POINTER(chanDivp, int);
   SIGASSERT_NULL_OR_POINTER(vcoDivp, int);

   if (dRateMHz > 555.00)
      return SIG_PX14_RATE_TOO_FAST;
   if (dRateMHz < 20.00)
      return SIG_PX14_RATE_TOO_SLOW;

   // The PX14 has a few unreachable frequencies:
   if (((dRateMHz >= 277.5) && (dRateMHz <= 308.3)) ||
       ((dRateMHz >= 444.0) && (dRateMHz <= 463.0)))
   {
      return SIG_PX14_RATE_NOT_AVAILABLE;
   }

   if (dRateMHz >= 463.0)
   {
      vcoDiv = 2;
      chanDiv = 2;
   }
   // Hole: (444.0, 463.0)
   else if (dRateMHz > 370.0)
   {
      vcoDiv = 5;
      chanDiv = 1;
   }
   else if (dRateMHz > 308.3)
   {
      vcoDiv = 2;
      chanDiv = 3;
   }
   // Hole: (277.5, 308.3)
   else if (dRateMHz > 240.0)
   {
      vcoDiv = 2;
      chanDiv = 4;
   }
   else if (dRateMHz > 214.0)
   {
      vcoDiv = 3;
      chanDiv = 3;
   }
   else if (dRateMHz > 185.0)
   {
      vcoDiv = 2;
      chanDiv = 5;
   }
   else if (dRateMHz > 155.0)
   {
      vcoDiv = 2;
      chanDiv = 6;
   }
   else if (dRateMHz > 135.7)
   {
      vcoDiv = 2;
      chanDiv = 7;
   }
   else if (dRateMHz > 120.0)
   {
      vcoDiv = 2;
      chanDiv = 8;
   }
   else if (dRateMHz > 107.0)
   {
      vcoDiv = 2;
      chanDiv = 9;
   }
   else if (dRateMHz > 99.0)
   {
      vcoDiv = 2;
      chanDiv = 10;
   }
   else if (dRateMHz > 90.5)
   {
      vcoDiv = 3;
      chanDiv = 7;
   }
   else if (dRateMHz > 80.0)
   {
      vcoDiv = 2;
      chanDiv = 12;
   }
   else if (dRateMHz > 71.0)
   {
      vcoDiv = 3;
      chanDiv = 9;
   }
   else if (dRateMHz > 63.5)
   {
      vcoDiv = 2;
      chanDiv = 15;
   }
   else if (dRateMHz > 56.5)
   {
      vcoDiv = 2;
      chanDiv = 17;
   }
   else if (dRateMHz > 51.0)
   {
      vcoDiv = 2;
      chanDiv = 19;
   }
   else if (dRateMHz > 46.0)
   {
      vcoDiv = 2;
      chanDiv = 21;
   }
   else if (dRateMHz > 41.5)
   {
      vcoDiv = 2;
      chanDiv = 23;
   }
   else if (dRateMHz > 37.0)
   {
      vcoDiv = 4;
      chanDiv = 13;
   }
   else if (dRateMHz > 32.8)
   {
      vcoDiv = 2;
      chanDiv = 29;
   }
   else if (dRateMHz > 28.9)
   {
      vcoDiv = 6;
      chanDiv = 11;
   }
   else if (dRateMHz > 25.5)
   {
      vcoDiv = 5;
      chanDiv = 15;
   }
   else if (dRateMHz > 22.7)
   {
      vcoDiv = 4;
      chanDiv = 21;
   }
   else //if (dRateMHz > 20.0)
   {
      vcoDiv = 5;
      chanDiv = 19;
   }

   if (vcoDivp)
      *vcoDivp = vcoDiv;
   if (chanDivp)
      *chanDivp = chanDiv;

   return SIG_SUCCESS;
}

int _SetupVcoPll (CStatePX14& state)
{
   int res, vcoDiv, chanDiv, eff_div;
   unsigned int N, B, A;
   double dRateMHz;
   HPX14 hBrd;

   hBrd = PX14_B2H(&state);

   // Setup clock dividers
   res = _GetDividersForAcqRate(state.GetIntAcqRateMHz(),
                                &vcoDiv, &chanDiv);
   PX14_RETURN_ON_FAIL(res);
   SIGASSERT ((chanDiv >= 1) && (chanDiv <= 32));
   SIGASSERT ((vcoDiv >= 2) && (vcoDiv <= 6));

   res = _SetVcoDivider(hBrd, vcoDiv);
   PX14_RETURN_ON_FAIL(res);
   res = _SetChanDivider(hBrd, chanDiv);
   PX14_RETURN_ON_FAIL(res);
   eff_div = vcoDiv * chanDiv;

   // Update clock gen registers (PLL)

   // N == PB + A == Fvco / PX14_PHASE_DETECT_FREQ_MHZ
   dRateMHz = state.GetIntAcqRateMHz() * (1 / PX14_PHASE_DETECT_FREQ_MHZ);
   dRateMHz *= eff_div;		// == Fvco

   N = static_cast<unsigned int>(static_cast<int>(dRateMHz));
   B = PX14_MIN (N >> 5, 0x1FFF);		// B = N / 32   : B has 13 bits
   A = PX14_MIN (N - (B << 5), 0x3F);	// A = N - 32B  : A has 6 bits

   // Reassign our clock rate
   dRateMHz = (((32.0 * B) + A) * PX14_PHASE_DETECT_FREQ_MHZ) / eff_div;
   state.SetIntAcqRateMHz(dRateMHz);

   PX14U_REG_CLK_LOG_01 r1;
   PX14U_REG_CLK_LOG_02 r2;

   // PLL power, charge pump, PFD polarity
   r1.bits.PLL_PowerDown = 0;				// PLL on
   r1.bits.ChargePumpMode = 3;				// Normal operation
   r1.bits.ChargePumpCurrent = 0;			// 0.6 mA
   r1.bits.PFD_Polarity = 1;				// Negative polarity
   res = WriteClockGenRegPX14(hBrd, 0x10,
                              PX14CLKREGMSK_010_PLL_POWERDOWN | PX14CLKREGMSK_010_CP_MODE |
                              PX14CLKREGMSK_010_CP_CURRENT | PX14CLKREGMSK_010_PFD_POLARITY,
                              r1.bytes[0], 1, 0);
   PX14_RETURN_ON_FAIL(res);

   // R Divider (LSB)
   r1.bits.R_Divider_LSB = PX14_REF_CLK_DIVIDER & 0xFF;
   res = WriteClockGenRegPX14(hBrd, 0x11,
                              PX14CLKREGMSK_011_R_DIV_LSB, r1.bytes[1], 1, 1);
   PX14_RETURN_ON_FAIL(res);

   // R Divider (MSB)
   r1.bits.R_Divider_MSB = (PX14_REF_CLK_DIVIDER >> 8) & 0x3F;
   res = WriteClockGenRegPX14(hBrd, 0x12,
                              PX14CLKREGMSK_012_R_DIV_MSB, r1.bytes[2], 1, 2);
   PX14_RETURN_ON_FAIL(res);

   // A Counter
   r1.bits.A_Counter = A;
   res = WriteClockGenRegPX14(hBrd, 0x13,
                              PX14CLKREGMSK_013_A_COUNTER, r1.bytes[3], 1, 3);
   PX14_RETURN_ON_FAIL(res);

   // B Counter (LSB)
   r2.bits.B_Counter_LSB = (B & 0xFF);
   res = WriteClockGenRegPX14(hBrd, 0x14,
                              PX14CLKREGMSK_014_B_CNT_LSB, r2.bytes[0], 2, 0);
   PX14_RETURN_ON_FAIL(res);

   // B Counter (MSB)
   r2.bits.B_Counter_MSB = (B >> 8) & 0x1F;
   res = WriteClockGenRegPX14(hBrd, 0x15,
                              PX14CLKREGMSK_015_B_CNT_MSB, r2.bytes[1], 2, 1);
   PX14_RETURN_ON_FAIL(res);

   // B Bypass, Prescaler P
   r2.bits.B_CounterBypass = 0;
   r2.bits.Prescaler_P = 6;	// 110 -> 32/33
   res = WriteClockGenRegPX14(hBrd, 0x16,
                              PX14CLKREGMSK_016_PRESCALER_P | PX14CLKREGMSK_016_B_CNT_BYPASS,
                              r2.bytes[2], 2, 2);
   PX14_RETURN_ON_FAIL(res);

   // - Last clock gen write; update registers

   // STATUS Pin Control
   r2.bits.StatusPinCtrl = 0x2D;	// Digital Lock Detect (Active High)
   res = WriteClockGenRegPX14(hBrd, 0x17,
                              PX14CLKREGMSK_017_STATUSPINCTRL, r2.bytes[3], 2, 3,
                              0, 0, PX14_TRUE);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

int _UpdateSabDcmSelect (HPX14 hBrd)
{
   static const double DcmThreshMHz = 30.0;

   double dRateMHz;
   bool bNoDcm;
   int res;

   // Turn off SAB's DCM if effective acquisition rate (including post ADC
   //  clock division emulation) is less than DcmThreshMHz.
   res = GetEffectiveAcqRatePX14(hBrd, &dRateMHz);
   PX14_RETURN_ON_FAIL(res);
   if (PX14CHANNEL_DUAL != GetActiveChannelsPX14(hBrd, PX14_GET_FROM_CACHE))
      dRateMHz /= 2;
   bNoDcm = dRateMHz < DcmThreshMHz;

   PX14U_REG_DEV_09 r9;
   r9.bits.NO_SAB_DCM = bNoDcm ? 1 : 0;
   return WriteDeviceRegPX14(hBrd, 9, PX14REGMSK_NO_SAB_DCM, r9.val);
}

int _DisablePll (HPX14 hBrd)
{
   PX14U_REG_CLK_LOG_01 r1;
   r1.bits.PLL_PowerDown = 1;		// Asynchronous power down

   return WriteClockGenRegPX14(hBrd, 0x10,
                               PX14CLKREGMSK_010_PLL_POWERDOWN, r1.bytes[0], 1, 0,
                               0, 0, PX14_TRUE);
}

int _SetClockDevPowerSettings (HPX14 hBrd)
{
   int res, ms_cfg, slave_count;
   bool bMaster, bCh2Active;
   PX14U_REG_CLK_LOG_09 r9;
   PX14U_REG_CLK_LOG_10 r10;
   PX14U_REG_CLK_LOG_11 r11;

   ms_cfg = GetMasterSlaveConfigurationPX14(hBrd);
   bMaster =
      (ms_cfg >= PX14MSCFG_MASTER_WITH_1_SLAVE) &&
      (ms_cfg <= PX14MSCFG_MASTER_WITH_4_SLAVES);
   slave_count = bMaster ? 1 + (ms_cfg - PX14MSCFG_MASTER_WITH_1_SLAVE ) : 0;

   bCh2Active = GetActiveChannelsPX14(hBrd) != PX14CHANNEL_ONE;
   // Above may need to be tweaked when master/slave clock sources added
   PX14_CT_ASSERT(PX14CLKSRC__COUNT == 2);

   // OUT0 must always be on
   r9.bits.Out0PowerDown = 0;
   res = WriteClockGenRegPX14(hBrd, 0x0F0,
                              PX14CLKREGMSK_0FX_OUTXPOWERDOWN, r9.bytes[0], 9, 0);
   PX14_RETURN_ON_FAIL(res);

   // OUT1 used for ADC2
   r9.bits.Out1PowerDown = bCh2Active ? 0 : 1;
   res = WriteClockGenRegPX14(hBrd, 0x0F1,
                              PX14CLKREGMSK_0FX_OUTXPOWERDOWN, r9.bytes[1], 9, 1);
   PX14_RETURN_ON_FAIL(res);

   // OUT[2-5] used by master to drive slave clocks

   unsigned int Out2PowerDown, Out3PowerDown, Out4PowerDown, Out5PowerDown;

   // Newer firmware uses different outputs for slave clocks
#if 0
   res = CheckFirmwareVerPX14(hBrd, PX14_VER32(1,10,0,0), PX14FWTYPE_SYS, PX14VCT_GT);
   if (SIG_SUCCESS == res)
   {
      // S1=Out3,S2=Out2,S3=Out4,S4=Out5
      Out2PowerDown = slave_count > 1 ? 0 : 1;
      Out3PowerDown = slave_count > 0 ? 0 : 1;
      Out4PowerDown = slave_count > 2 ? 0 : 1;
      Out5PowerDown = slave_count > 3 ? 0 : 1;
   }
   else
   {
      // S1=Out4,S2=Out5,S3=Out3,S4=Out2
#endif
      Out2PowerDown = slave_count > 3 ? 0 : 1;
      Out3PowerDown = slave_count > 2 ? 0 : 1;
      Out4PowerDown = slave_count > 0 ? 0 : 1;
      Out5PowerDown = slave_count > 1 ? 0 : 1;
#if 0
   }
#endif

   // Slave 4
   //r9.bits.Out2PowerDown = slave_count > 3 ? 0 : 1;
   r9.bits.Out2PowerDown = Out2PowerDown;
   res = WriteClockGenRegPX14(hBrd, 0x0F2,
                              PX14CLKREGMSK_0FX_OUTXPOWERDOWN, r9.bytes[2], 9, 2);
   PX14_RETURN_ON_FAIL(res);

   // Slave 3
   //r9.bits.Out3PowerDown = slave_count > 2 ? 0 : 1;
   r9.bits.Out3PowerDown = Out3PowerDown;
   res = WriteClockGenRegPX14(hBrd, 0x0F3,
                              PX14CLKREGMSK_0FX_OUTXPOWERDOWN, r9.bytes[3], 9, 3);
   PX14_RETURN_ON_FAIL(res);

   // Slave 1
   //r10.bits.Out4PowerDown = slave_count > 0 ? 0 : 1;
   r10.bits.Out4PowerDown = Out4PowerDown;
   res = WriteClockGenRegPX14(hBrd, 0x0F4,
                              PX14CLKREGMSK_0FX_OUTXPOWERDOWN, r10.bytes[0], 10, 0);
   PX14_RETURN_ON_FAIL(res);

   // Slave 2
   //r10.bits.Out5PowerDown = slave_count > 1 ? 0 : 1;
   r10.bits.Out5PowerDown = Out5PowerDown;
   res = WriteClockGenRegPX14(hBrd, 0x0F5,
                              PX14CLKREGMSK_0FX_OUTXPOWERDOWN, r10.bytes[1], 10, 1);
   PX14_RETURN_ON_FAIL(res);

   // Remaining outputs are currently unused

   r11.bits.Out6PowerDown = 1;
   res = WriteClockGenRegPX14(hBrd, 0x140,
                              PX14CLKREGMSK_14X_OUTXPOWERDOWN, r11.bytes[0], 11, 0);
   PX14_RETURN_ON_FAIL(res);

   r11.bits.Out7PowerDown = 1;
   res = WriteClockGenRegPX14(hBrd, 0x141,
                              PX14CLKREGMSK_14X_OUTXPOWERDOWN, r11.bytes[1], 11, 1);
   PX14_RETURN_ON_FAIL(res);

   r11.bits.Out8PowerDown = 1;
   res = WriteClockGenRegPX14(hBrd, 0x142,
                              PX14CLKREGMSK_14X_OUTXPOWERDOWN, r11.bytes[2], 11, 2);
   PX14_RETURN_ON_FAIL(res);

   // Last write, update registers
   r11.bits.Out9PowerDown = 1;
   res = WriteClockGenRegPX14(hBrd, 0x143,
                              PX14CLKREGMSK_14X_OUTXPOWERDOWN, r11.bytes[3], 11, 3, 0, 0, PX14_TRUE);
   PX14_RETURN_ON_FAIL(res);

   SysSleep(150);

   return SIG_SUCCESS;
}

int _SetVcoDivider (HPX14 hBrd, int vcoDiv)
{
   PX14U_REG_CLK_LOG_19 r19;
   int res;

   r19.val = 0;

   if ((vcoDiv < 0) || (vcoDiv > 6))
      return SIG_PX14_UNEXPECTED;
   if (!vcoDiv) vcoDiv = 1;

   if (vcoDiv > 1)
   {
      // Specify divider
      r19.bits.VCO_Divider = vcoDiv - 2;
      res = WriteClockGenRegPX14(hBrd, 0x1E0,
                                 PX14CLKREGMSK_1E0_VCO_DIVIDER, r19.bytes[0], 19, 0);
      PX14_RETURN_ON_FAIL(res);
   }

   // Turn on/off VCO divider bypass
   r19.bits.BypassVcoDivider = (vcoDiv > 1) ? 0 : 1;
   return WriteClockGenRegPX14(hBrd, 0x1E1,
                               PX14CLKREGMSK_1E1_BYPASS_VCO_DIV, r19.bytes[1], 19, 1,
                               0, 0, PX14_TRUE);
}

int _SetChanDivider (HPX14 hBrd, int chanDiv)
{
   static const unsigned int hl_mask =
      PX14CLKREGMSK_XXX_DIVX_HIGHCYCLES |
      PX14CLKREGMSK_XXX_DIVX_LOWCYCLES;

   unsigned int low_cycles, high_cycles, bypass;
   int res, bMaster;

   PX14U_REG_CLK_LOG_12 r12;
   PX14U_REG_CLK_LOG_13 r13;
   PX14U_REG_CLK_LOG_14 r14;

   if ((chanDiv < 0) || (chanDiv > 32))
      return SIG_PX14_UNEXPECTED;
   if (!chanDiv) chanDiv = 1;

   // We'll need to set additional dividers if we've any slave boards
   res = GetMasterSlaveConfigurationPX14(hBrd, PX14_GET_FROM_CACHE);
   if (res < 0)
      return res;
   bMaster = PX14_IS_MASTER(res);

   // -- Set divider(s)

   if (chanDiv > 1)
   {
      high_cycles = (chanDiv - 2) >> 1;
      low_cycles  = (chanDiv - 2) >> 1;
      if (chanDiv & 1) high_cycles++;

      // ADC output dividers - Divider 0
      r12.bits.Div0HighCycles = high_cycles;
      r12.bits.Div0LowCycles  = low_cycles;
      res = WriteClockGenRegPX14(hBrd, 0x190, hl_mask,
                                 r12.bytes[0], 12, 0);
      PX14_RETURN_ON_FAIL(res);

      if (bMaster)
      {
         // Divider 2: (SYS FW > 1.10) ? (S3 and S4) : (S1 and S2)
         r14.bits.Div2HighCycles = high_cycles;
         r14.bits.Div2LowCycles  = low_cycles;
         res = WriteClockGenRegPX14(hBrd, 0x196, hl_mask,
                                    r14.bytes[0], 14, 0);
         PX14_RETURN_ON_FAIL(res);

         // Divider 1: (SYS DW > 1.10) ? (S2 and S1) : (S4 and S3)
         r13.bits.Div1HighCycles = high_cycles;
         r13.bits.Div1LowCycles  = low_cycles;
         res = WriteClockGenRegPX14(hBrd, 0x193, hl_mask,
                                    r13.bytes[0], 13, 0);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   // -- Turn on/off divider bypass

   bypass = (chanDiv > 1) ? 0 : 1;

   if (bMaster)
   {
      r13.bits.Div1Bypass = bypass;
      res = WriteClockGenRegPX14(hBrd, 0x194,
                                 PX14CLKREGMSK_XXX_DIVX_BYPASS, r13.bytes[1], 13, 1);
      PX14_RETURN_ON_FAIL(res);

      r14.bits.Div2Bypass = bypass;
      res = WriteClockGenRegPX14(hBrd, 0x197,
                                 PX14CLKREGMSK_XXX_DIVX_BYPASS, r14.bytes[1], 14, 1);
      PX14_RETURN_ON_FAIL(res);
   }

   // Last write; clock gen registers will be updated after this write
   r12.bits.Div0Bypass = bypass;
   res = WriteClockGenRegPX14(hBrd, 0x191,
                              PX14CLKREGMSK_XXX_DIVX_BYPASS, r12.bytes[1], 12, 1,
                              0, 0, PX14_TRUE);

   return res;
}

int _MasterSlaveSync (HPX14 hBrd)
{
   double dAcqRateMHz;
   int res, msCfg;
   bool bSet;

#if 0
   // Master/slave synch only applies to system firmware > 1.0.0.0
   res = CheckFirmwareVerPX14(hBrd, PX14_VER32(1,0,0,0), PX14FWTYPE_SYS, PX14VCT_GT);
   if (SIG_SUCCESS != res)
   {
      // This is older firmware that doesn't support this master/slave
      //  synchronization. Just resynch outputs and we're good.
      return ResyncClockOutputsPX14(hBrd);
   }
#endif

   res = GetActualAdcAcqRatePX14(hBrd, &dAcqRateMHz);
   PX14_RETURN_ON_FAIL(res);

   msCfg = GetMasterSlaveConfigurationPX14(hBrd);

   if (PX14_IS_MASTER(msCfg))
   {
      PX14U_REG_CLK_LOG_12 cr12;
      PX14U_REG_CLK_LOG_13 cr13;
      PX14U_REG_DEV_05 dr5;
      unsigned int delay;

      // Set master's trigger select
      bSet = dAcqRateMHz  < 300;
      dr5.bits.MST_CLK_SLOW = bSet ? 1 : 0;
      res = WriteDeviceRegPX14(hBrd, 5, PX14REGMSK_MST_CLK_SLOW, dr5.val);
      PX14_RETURN_ON_FAIL(res);

      // Setup output delay for slave
      //  This is static for now but might be programmable at some point
      delay = 1;	//(dAcqRateMHz > 215) ? 2 : 1;
      cr12.bits.Div0PhaseOffset = delay;
      res = WriteClockGenRegPX14(hBrd, 0x191,
                                 PX14CLKREGMSK_XXX_DIVX_PHASEOFF, cr12.bytes[1], 12, 1);
      PX14_RETURN_ON_FAIL(res);
      cr13.bits.Div1PhaseOffset = delay;
      res = WriteClockGenRegPX14(hBrd, 0x194,
                                 PX14CLKREGMSK_XXX_DIVX_PHASEOFF, cr13.bytes[1], 13, 1,
                                 0, 0, PX14_TRUE);
      PX14_RETURN_ON_FAIL(res);

      // Resync clock outputs so above has effect
      res = ResyncClockOutputsPX14(hBrd);
      PX14_RETURN_ON_FAIL(res);
      // Force DCM reset now; slaves will need a good clock
      res = SyncFirmwareToAdcClockPX14(hBrd, PX14_TRUE);
      PX14_RETURN_ON_FAIL(res);
   }

   else if (PX14_IS_SLAVE(msCfg))
   {
      static const int _max_tries = 20;

      unsigned reg_val, mask, expected;
      int i, j, bVirtual;

      bVirtual = IsDeviceVirtualPX14(hBrd);

      if (dAcqRateMHz < 120)
      {
         mask     = PX14REGMSK_SLAVE_SYNC2;
         expected = 0;
      }
      else if (dAcqRateMHz <= 280)
      {
         mask     = PX14REGMSK_SLAVE_SYNC1;
         expected = 0;
      }
      else  if (dAcqRateMHz < 335)
      {
         mask     = PX14REGMSK_SLAVE_SYNC2;
         expected = PX14REGMSK_SLAVE_SYNC2;
      }
      else
      {
         mask     = PX14REGMSK_SLAVE_SYNC1;
         expected = PX14REGMSK_SLAVE_SYNC1;
      }

      for (i=0; i<_max_tries && !bVirtual; i++)
      {
         res = ResyncClockOutputsPX14(hBrd);
         PX14_RETURN_ON_FAIL(res);
         res = SyncFirmwareToAdcClockPX14(hBrd, PX14_TRUE);
         PX14_RETURN_ON_FAIL(res);

         ReadDeviceRegPX14(hBrd, 0xD);	// <- Bus flush
         SysSleep(10);

         res = ReadDeviceRegPX14(hBrd, 0xD, &reg_val, PX14REGREAD_HARDWARE);
         PX14_RETURN_ON_FAIL(res);

         if (expected == (reg_val & mask))
         {
            // Read 10 times to ensure stable
            for (j=0; j<10; j++)
            {
               res = ReadDeviceRegPX14(hBrd, 0xD, &reg_val, PX14REGREAD_HARDWARE);
               PX14_RETURN_ON_FAIL(res);

               if (expected != (reg_val & mask))
                  break;
            }
         }
         if (expected == (reg_val & mask))
            break;
      }

      if ((i >= _max_tries))
         return SIG_PX14_SLAVE_FAILED_MASTER_SYNC;
   }

   return SIG_SUCCESS;
}


int UpdateDcAmplifierPowerups (HPX14 hBrd)
{
   static const unsigned pdx_mask =
      PX14REGMSG_PD1_LOGAIN_CH1_ | PX14REGMSG_PD2_HIGAIN_CH1_ |
      PX14REGMSG_PD3_LOGAIN_CH2_ | PX14REGMSG_PD4_HIGAIN_CH2_;

   int gain_sel, active_chan_sel;

   PX14U_REG_DEV_07 r7;

   if (GetPowerDownOverridePX14(hBrd))
   {
      r7.bits.pd1_logain_ch1_ = 1;
      r7.bits.pd2_higain_ch1_ = 1;
      r7.bits.pd3_logain_ch2_ = 1;
      r7.bits.pd4_higain_ch2_ = 1;
   }
   else
   {
      active_chan_sel = GetActiveChannelsPX14(hBrd);
      gain_sel = GetInputGainLevelDcPX14(hBrd);

      r7.bits.pd1_logain_ch1_ =
         ((gain_sel == PX14DGAIN_LOW)  && (active_chan_sel != PX14CHANNEL_TWO)) ? 1 : 0;
      r7.bits.pd2_higain_ch1_ =
         ((gain_sel == PX14DGAIN_HIGH) && (active_chan_sel != PX14CHANNEL_TWO)) ? 1 : 0;
      r7.bits.pd3_logain_ch2_ =
         ((gain_sel == PX14DGAIN_LOW)  && (active_chan_sel != PX14CHANNEL_ONE)) ? 1 : 0;
      r7.bits.pd4_higain_ch2_ =
         ((gain_sel == PX14DGAIN_HIGH) && (active_chan_sel != PX14CHANNEL_ONE)) ? 1 : 0;
   }

   return WriteDeviceRegPX14(hBrd, 7, pdx_mask, r7.val, 0, 20);
}

