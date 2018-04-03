/**  @file px14_volt_rng.cpp
*/
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"

static int _SetInputVoltRangeD2 (int chanIdx, HPX14 hBrd, unsigned int val);
static int _GetInputVoltRangeD2 (int chanIdx, HPX14 hBrd, int bFromCache);

/// Actual peak-to-peak voltage range values for PX14VOLTRNG_* values
static const double s_vr_vect[] =
{
   // Static volt range used for non-amplified devices
   1.100,
   // User-selectable input volt ranges for amplified devices
   0.220, 0.247, 0.277, 0.311, 0.349, 0.391, 0.439, 0.493, 0.553, 0.620,
   0.696, 0.781, 0.876, 0.983, 1.103, 1.237, 1.388, 1.557, 1.748, 1.961,
   2.200, 2.468, 2.770, 3.108, 3.487,
   // User-selectable input volt ranges for PX14400D2 devices
   3.000, 1.600, 1.000, 0.600, 0.333, 0.200
};

/// Set channel 1 input voltage range; (PX14VOLTRNG_*)
PX14API SetInputVoltRangeCh1PX14 (HPX14 hBrd, unsigned int val)
{
   CStatePX14* statep;
   int res;

   // Compile time assertion to check size of s_vr_vect
   PX14_CT_ASSERT((sizeof(s_vr_vect) / sizeof(s_vr_vect[0])) == PX14VOLTRNG__COUNT);

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsPX14400D2())
      return _SetInputVoltRangeD2 (0, hBrd, val);

   if (val > PX14VOLTRNG__END)
      return SIG_PX14_INVALID_ARG_2;

   if (!statep->IsChanAmplified(0))
   {
      if (val != PX14VOLTRNG_STATIC_1_100_VPP)
         return SIG_PX14_INVALID_CHAN_IMP;

      // Nothing to write to hardware; static value
      return SIG_SUCCESS;
   }
   else
   {
      if (val == PX14VOLTRNG_STATIC_1_100_VPP)
         val = PX14VOLTRNG_1_103_VPP;
   }

   PX14U_REG_DEV_07 r7;
   r7.bits.ATT1 = val - 1;
   return WriteDeviceRegPX14(hBrd, 7, PX14REGMSK_ATT1, r7.val);
}

/// Get channel 1 input voltage range; (PX14VOLTRNG_*)
PX14API GetInputVoltRangeCh1PX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsPX14400D2())
      return _GetInputVoltRangeD2(0, hBrd, bFromCache);

   if (!statep->IsChanAmplified(0))
      return PX14VOLTRNG_STATIC_1_100_VPP;

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 7, NULL, PX14REGREAD_DEFAULT);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg7.bits.ATT1) + 1;
}

/// Set channel 2 input voltage range; (PX14VOLTRNG_*)
PX14API SetInputVoltRangeCh2PX14 (HPX14 hBrd, unsigned int val)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsPX14400D2())
      return _SetInputVoltRangeD2 (1, hBrd, val);

   if (val > PX14VOLTRNG__END)
      return SIG_PX14_INVALID_ARG_2;

   if (!statep->IsChanAmplified(1))
   {
      if (val != PX14VOLTRNG_STATIC_1_100_VPP)
         return SIG_PX14_INVALID_CHAN_IMP;

      // Nothing to write to hardware; static value
      return SIG_SUCCESS;
   }
   else
   {
      if (val == PX14VOLTRNG_STATIC_1_100_VPP)
         val = PX14VOLTRNG_1_103_VPP;
   }

   PX14U_REG_DEV_07 r7;
   r7.bits.ATT2 = val - 1;
   return WriteDeviceRegPX14(hBrd, 7, PX14REGMSK_ATT2, r7.val);
}

/// Get channel 2 input voltage range; (PX14VOLTRNG_*)
PX14API GetInputVoltRangeCh2PX14 (HPX14 hBrd, int bFromCache)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsPX14400D2())
      return _GetInputVoltRangeD2(0, hBrd, bFromCache);

   if (!statep->IsChanAmplified(1))
      return PX14VOLTRNG_STATIC_1_100_VPP;

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 7);
      PX14_RETURN_ON_FAIL(res);
   }

   return static_cast<int>(statep->m_regDev.fields.reg7.bits.ATT2) + 1;
}

// Obtain actual peak-to-peak voltage for given PX14VOLTRNG_*
PX14API GetInputVoltRangeFromSettingPX14 (int vr_sel, double* vpp)
{
   SIGASSERT_POINTER(vpp, double);
   if ((vr_sel < 0) || vr_sel >= PX14VOLTRNG__COUNT)
      return SIG_PX14_INVALID_ARG_1;
   if (NULL == vpp)
      return SIG_PX14_INVALID_ARG_2;

   *vpp = s_vr_vect[vr_sel];
   return SIG_SUCCESS;
}

// Obtain actual peak-to-peak voltage for channel 1 input
PX14API GetInputVoltageRangeVoltsCh1PX14 (HPX14 hBrd,
                                          double* voltsp,
                                          int bFromCache)
{
   int vr_sel;

   SIGASSERT_POINTER(voltsp, double);
   if (NULL == voltsp)
      return SIG_PX14_INVALID_ARG_2;

   vr_sel = GetInputVoltRangeCh1PX14(hBrd, bFromCache);
   if (vr_sel < 0)
      return vr_sel;

   return GetInputVoltRangeFromSettingPX14(vr_sel, voltsp);
}

// Obtain actual peak-to-peak voltage for channel 2 input
PX14API GetInputVoltageRangeVoltsCh2PX14 (HPX14 hBrd,
                                          double* voltsp,
                                          int bFromCache)
{
   int vr_sel;

   SIGASSERT_POINTER(voltsp, double);
   if (NULL == voltsp)
      return SIG_PX14_INVALID_ARG_2;

   vr_sel = GetInputVoltRangeCh2PX14(hBrd, bFromCache);
   if (vr_sel < 0)
      return vr_sel;

   return GetInputVoltRangeFromSettingPX14(vr_sel, voltsp);
}

PX14API SelectInputVoltRangePX14 (HPX14 hBrd,
                                  double vr_pp,
                                  int bCh1,
                                  int bCh2,
                                  int sel_how)
{
   int res, i, vr_sel, nItems;
   CStatePX14* statep;

   if (sel_how >= PX14VRSEL__COUNT)
      return SIG_PX14_INVALID_ARG_4;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   nItems = static_cast<int>(sizeof(s_vr_vect) / sizeof(s_vr_vect[0]));

   if (bCh1 && bCh2 &&
       (statep->IsChanAmplified(0) != statep->IsChanAmplified(1)))
   {
      // Channel 1 first
      res = SelectInputVoltRangePX14(hBrd, vr_pp,
                                     PX14_TRUE, PX14_FALSE, sel_how);
      PX14_RETURN_ON_FAIL(res);

      // Channel 2 second
      res = SelectInputVoltRangePX14(hBrd, vr_pp,
                                     PX14_FALSE, PX14_TRUE, sel_how);

      return res;
   }

   vr_sel = -1;

   if (!statep->IsChanAmplified(bCh1 ? 0 : 1))
   {
      // Channel isn't amplified so we have a static input voltage range
      switch (sel_how)
      {
         case PX14VRSEL_NOT_LT:
            vr_sel = (s_vr_vect[0] < vr_pp) ? -1 : 0;
            break;
         case PX14VRSEL_NOT_GT:
            vr_sel = (s_vr_vect[0] > vr_pp) ? -1 : 0;
            break;
         default:
            vr_sel = 0;
      }
   }
   else
   {
      // Select best input voltage range
      // i: Skip first item in vector; it's for non-amplified channels
      for (i=1; i<nItems; i++)
      {
         if ((sel_how == PX14VRSEL_NOT_LT) && (s_vr_vect[i] < vr_pp))
            continue;
         if ((sel_how == PX14VRSEL_NOT_GT) && (s_vr_vect[i] > vr_pp))
            continue;

         if ((vr_sel == -1) ||
             (abs(vr_pp - s_vr_vect[i])<abs(vr_pp - s_vr_vect[vr_sel])))
         {
            vr_sel = i;
         }
      }
   }
   if (vr_sel == -1)
      return SIG_PX14_UNREACHABLE;

   // Apply input voltage range
   if (bCh1)
   {
      res = SetInputVoltRangeCh1PX14(hBrd, vr_sel);
      PX14_RETURN_ON_FAIL(res);
   }

   if (bCh2)
   {
      res = SetInputVoltRangeCh2PX14(hBrd, vr_sel);
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

/// Set the input voltage range for a PX14400D2 device
int _SetInputVoltRangeD2 (int chanIdx, HPX14 hBrd, unsigned int val)
{
   PX14U_REG_DEV_06 r6;
   unsigned int msk;
   int res;

   SIGASSERT ((chanIdx == 0) || (chanIdx == 1));

   if ((val < PX14VOLTRNG_D2__START) || (val >PX14VOLTRNG_D2__END))
      return SIG_INVALIDARG;

   switch (chanIdx)
   {
      case 0: // Channel 1
         switch (val)
         {
            default:
            case PX14VOLTRNG_D2_3_00_VPP:  r6.bits.D2VR_CH1 = 0; break;
            case PX14VOLTRNG_D2_1_60_VPP:  r6.bits.D2VR_CH1 = 2; break;
            case PX14VOLTRNG_D2_1_00_VPP:  r6.bits.D2VR_CH1 = 6; break;
            case PX14VOLTRNG_D2_0_600_VPP: r6.bits.D2VR_CH1 = 1; break;
            case PX14VOLTRNG_D2_0_333_VPP: r6.bits.D2VR_CH1 = 3; break;
            case PX14VOLTRNG_D2_0_200_VPP: r6.bits.D2VR_CH1 = 7; break;
         }
         msk = PX14REGMSK_D2VR_CH1;
         break;

      case 1: // Channel 2
         switch (val)
         {
            default:
            case PX14VOLTRNG_D2_3_00_VPP:  r6.bits.D2VR_CH2 = 0; break;
            case PX14VOLTRNG_D2_1_60_VPP:  r6.bits.D2VR_CH2 = 2; break;
            case PX14VOLTRNG_D2_1_00_VPP:  r6.bits.D2VR_CH2 = 6; break;
            case PX14VOLTRNG_D2_0_600_VPP: r6.bits.D2VR_CH2 = 1; break;
            case PX14VOLTRNG_D2_0_333_VPP: r6.bits.D2VR_CH2 = 3; break;
            case PX14VOLTRNG_D2_0_200_VPP: r6.bits.D2VR_CH2 = 7; break;
         }
         msk = PX14REGMSK_D2VR_CH2;
         break;

      default:
         return SIG_INVALIDARG;
   }

   res = WriteDeviceRegPX14(hBrd, 6, msk, r6.val);
   PX14_RETURN_ON_FAIL(res);

   // For the first set of boards that went out, there was a small short on
   //  the PCB such that MSB of D2VR_CH2 was not having effect. This has
   //  since been corrected but this workaround is required for the few
   //  cards that went out before PCB update. This workaround will have no
   //  effect on boards with the updated PCB.
   if (chanIdx == 1)
   {
      unsigned int brd_rev = PX14BRDREV_PX14400;
      unsigned long long hw_rev = 0;

      GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
      GetHardwareRevisionPX14(hBrd, &hw_rev);

      if ((brd_rev == PX14BRDREV_PX14400D2) && (hw_rev < PX14_VER64(2,1,0,0)))
      {
         // MRD: This is probably my favorite workaround (hack) ever.
         //  We're using one of the unused offset DAC outputs to drive a
         //  logic level output to control the switch disconnected due to
         //  the short.

         PX14U_DACREGD2IO d2_io;
         d2_io.val = 0;
         d2_io.bits.dac_addr = PX14DACD2ADDR_DACD;
         d2_io.bits.dac_data = (r6.bits.D2VR_CH2 & 4) ? 3333 : 0;
         // 50us for serial delivery, 10us for settle time
         res = WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC,
                                  0xFFFFFFFF, d2_io.val, 0, PX14DACD2_WDELAY_MS);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   return SIG_SUCCESS;
}

int _GetInputVoltRangeD2 (int chanIdx, HPX14 hBrd, int bFromCache)
{
   int res, fw_bits;

   if (!bFromCache)
   {
      res = ReadDeviceRegPX14(hBrd, 6);
      PX14_RETURN_ON_FAIL(res);
   }

   switch (chanIdx)
   {
      case 0: fw_bits = PX14_H2B(hBrd)->m_regDev.fields.reg6.bits.D2VR_CH1; break;
      case 1: fw_bits = PX14_H2B(hBrd)->m_regDev.fields.reg6.bits.D2VR_CH2; break;
      default: return SIG_INVALIDARG;
   }

   switch (fw_bits)
   {
      case 0: res = PX14VOLTRNG_D2_3_00_VPP;  break;
      case 2: res = PX14VOLTRNG_D2_1_60_VPP;  break;
      case 6: res = PX14VOLTRNG_D2_1_00_VPP;  break;
      case 1: res = PX14VOLTRNG_D2_0_600_VPP; break;
      case 3: res = PX14VOLTRNG_D2_0_333_VPP; break;
      case 7: res = PX14VOLTRNG_D2_0_200_VPP; break;
      default: return SIG_PX14_UNEXPECTED;
   }

   return res;
}


