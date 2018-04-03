/** @file	px14_fixed_logic.cpp
*/
#include "stdafx.h"
#include "px14_top.h"

// Module-local function prototypes ------------------------------------- //

// Library exports implementation --------------------------------------- //

PX14API IsFixedLogicPackagePresentPX14 (HPX14 hBrd, unsigned short flp)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // For now, it's safe to assume virtual devices can support any
   //  fixed (virtual) logic package
   if (statep->IsVirtual())
      return SIG_SUCCESS;

#if 0
   if ((0 == (statep->m_features & PX14FEATURE_SAB)) ||	// No SAB present
         (flp == PX14CUSTHW_ONESHOT)	||						// Non-unique fw
         (statep->m_custFwSab != flp))						// Primary check
   {
     return SIG_PX14_INCORRECT_LOGIC_PACKAGE;
   }
#endif

   SysSleep(1);
   res = WriteDeviceRegPX14(hBrd, 0xA, 0xFFFFFFFF, 8);
   PX14_RETURN_ON_FAIL(res);
   SysSleep(1);
   res = WriteDeviceRegPX14(hBrd, 0xA, 0xFFFFFFFF, 8);
   PX14_RETURN_ON_FAIL(res);
   SysSleep(1);
   unsigned int readVal = 0;
   res = ReadDeviceRegPX14(hBrd, 0xA, &readVal,PX14REGREAD_HARDWARE);
   PX14_RETURN_ON_FAIL(res);
   SysSleep(1);
   unsigned short wVal = statep->m_regDev.fields.regA.bits.preg_val;

   if ((0 == (statep->m_features & PX14FEATURE_SAB)) ||	// No SAB present
       (flp == PX14CUSTHW_ONESHOT)	||						// Non-unique fw
       (wVal != flp))					// Primary check
   {
      return SIG_PX14_INCORRECT_LOGIC_PACKAGE;
   }

   return SIG_SUCCESS;
}

//
// C2: Decimation
//

/// Set decimation factor (PX14C2DECF_*)
PX14API FLPC2_SetDecimationFactorPX14 (HPX14 hBrd, unsigned val)
{
   int res;

   if (val >= PX14C2DECF__COUNT)
      return SIG_PX14_INVALID_ARG_2;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C2_DECIMATION);
   PX14_RETURN_ON_FAIL(res);

   return SetBoardProcessingParamPX14(hBrd, 0x8009,
                                      static_cast<unsigned short>(val));
}

// Set the NCO frequency in MHz; call after acqusition clock rate has been set
PX14API FLPC2_SetNcoFrequencyPX14 (HPX14 hBrd, double nco_freqMHz)
{
   unsigned long long qw;
   double dAcqRateMHz;
   int res;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C2_DECIMATION);
   PX14_RETURN_ON_FAIL(res);

   // NCO frequency setting depends on acquisition clock rate
   res = GetEffectiveAcqRatePX14(hBrd, &dAcqRateMHz);
   PX14_RETURN_ON_FAIL(res);

   static const unsigned long long my_factor = 0x00000003FFFFFFFFULL;

   // NCO frequency
   qw = static_cast<unsigned long long>(
                                        2.0 * (nco_freqMHz / dAcqRateMHz) * my_factor);
   res = SetBoardProcessingParamPX14(hBrd, 0x8001,
                                     (unsigned short)qw);
   PX14_RETURN_ON_FAIL(res);
   res = SetBoardProcessingParamPX14(hBrd, 0x8002,
                                     (unsigned short)(qw >> 16));
   PX14_RETURN_ON_FAIL(res);
   res = SetBoardProcessingParamPX14(hBrd, 0x8003,
                                     (unsigned short)(qw >> 32));
   PX14_RETURN_ON_FAIL(res);
   res = SetBoardProcessingParamPX14(hBrd, 0x8004,
                                     (unsigned short)(qw >> 48));
   PX14_RETURN_ON_FAIL(res);

   // NCO phase
   qw = static_cast<unsigned long long>(
                                        1.0 * (nco_freqMHz / dAcqRateMHz) * my_factor);
   res = SetBoardProcessingParamPX14(hBrd, 0x8005,
                                     (unsigned short)qw);
   PX14_RETURN_ON_FAIL(res);
   res = SetBoardProcessingParamPX14(hBrd, 0x8006,
                                     (unsigned short)(qw >> 16));
   PX14_RETURN_ON_FAIL(res);
   res = SetBoardProcessingParamPX14(hBrd, 0x8007,
                                     (unsigned short)(qw >> 32));
   PX14_RETURN_ON_FAIL(res);
   res = SetBoardProcessingParamPX14(hBrd, 0x8008,
                                     (unsigned short)(qw >> 48));
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

//
// C4: FFT
//

#define PX14_C4_MIN_FFT_SIZE				1024
#define PX14_C4_MAX_FFT_SIZE				2048

PX14API FLPC4_GetFftSizeLimitsPX14 (HPX14 hBrd,
                                    unsigned int* min_fft_sizep,
                                    unsigned int* max_fft_sizep)
{
   int res;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C4_FFT);
   PX14_RETURN_ON_FAIL(res);

   if (min_fft_sizep) *min_fft_sizep = PX14_C4_MIN_FFT_SIZE;
   if (max_fft_sizep) *max_fft_sizep = PX14_C4_MAX_FFT_SIZE;
   return SIG_SUCCESS;
}

/// Set the FFT size (1024 or 2048)
PX14API FLPC4_SetFftSizePX14 (HPX14 hBrd, unsigned fft_size)
{
   int res;

   // Currently only support two FFT sizes: 1024 and 2048
   if ((fft_size != 1024) && (fft_size != 2048))
      return SIG_PX14_INVALID_ARG_2;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C4_FFT);
   PX14_RETURN_ON_FAIL(res);

   return SetBoardProcessingParamPX14(hBrd, 0x0801,
                                      static_cast<unsigned short>(fft_size));
}

/// Set the number of data points to zero-pad (appended)
PX14API FLPC4_SetZeroPadPointsPX14 (HPX14 hBrd, unsigned zero_points)
{
   int res;

   if (zero_points > PX14_C4_MAX_FFT_SIZE - 32)
      return SIG_PX14_INVALID_ARG_2;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C4_FFT);
   PX14_RETURN_ON_FAIL(res);

   return SetBoardProcessingParamPX14(hBrd, 0x0800,
                                      static_cast<unsigned short>(zero_points));
}

/// Set the magnitude-squared enable
PX14API FLPC4_SetMagnitudeSquaredEnablePX14 (HPX14 hBrd, int bEnable)
{
   int res;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C4_FFT);
   PX14_RETURN_ON_FAIL(res);

   return SetBoardProcessingParamPX14(hBrd, 0x0802, bEnable ? 1 : 0);
}

/// Load FFT window data
PX14API FLPC4_LoadWindow (HPX14 hBrd,
                          const short* coefficients,
                          unsigned coefficient_count)
{
   unsigned i;
   int res;

   PX14_ENSURE_POINTER(hBrd, coefficients, short, "FLPC4_LoadWindow");

   if (coefficient_count > 2048)
      return SIG_PX14_INVALID_ARG_3;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C4_FFT);
   PX14_RETURN_ON_FAIL(res);

   for (i=0; i<coefficient_count; i++)
   {
      res = SetBoardProcessingParamPX14(hBrd,
                                        static_cast<unsigned short>(i), *coefficients++);
      PX14_RETURN_ON_FAIL(res);
   }


   return SIG_SUCCESS;
}

//
// C5: FIR FILTERING (Single channel)
//


PX14API FLPC5_LoadCoefficientsPX14 (HPX14 hBrd,
                                    const short* coefficients,
                                    int ordering)
{
   static const unsigned tap_count = 129;
   static const unsigned short fpi_start = 0x8001;
   static const unsigned short fpi_load  = 0x8002;

   unsigned i, idx;
   int res;

   PX14_ENSURE_POINTER(hBrd, coefficients, short, "FLPC5_LoadCoefficientsPX14");

   // The ordering parameter is reserved for a future alternate ordering that
   //  would be necessary if we use a different type of filter
   if (ordering != 0)
      return SIG_PX14_INVALID_ARG_3;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C5_FIRFILT_SINGLE_CHAN);
   PX14_RETURN_ON_FAIL(res);

   // Write any value to 0x8000; signals firmware that we're loading coefficients
   res = SetBoardProcessingParamPX14(hBrd, fpi_start, 1);

   // Load the coefficients in reverse order
   for (i=0,idx=tap_count-1; i<tap_count; i++,idx--)
   {
      res = SetBoardProcessingParamPX14(hBrd, fpi_load,
                                        static_cast<unsigned short>(coefficients[idx]));
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

//
// C6: FIR FILTERING (Dual channel)
//

// Load the 65 channel 1 filter coefficients
PX14API FLPC6_LoadCoefficientsCh1PX14 (HPX14 hBrd,
                                       const short* coefficients,
                                       int ordering)
{
   static const unsigned tap_count = 65;
   static const unsigned short fpi_start = 0x8001;
   static const unsigned short fpi_load  = 0x8002;

   unsigned i, idx;
   int res;

   PX14_ENSURE_POINTER(hBrd, coefficients, short, "FLPC6_LoadCoefficientsCh1PX14");

   // The ordering parameter is reserved for a future alternate ordering that
   //  would be necessary if we use a different type of filter
   if (ordering != 0)
      return SIG_PX14_INVALID_ARG_3;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C6_FIRFILT_DUAL_CHAN);
   PX14_RETURN_ON_FAIL(res);

   // Write any value to 0x8000; signals firmware that we're loading coefficients
   res = SetBoardProcessingParamPX14(hBrd, fpi_start, 1);

   // Load the coefficients in reverse order
   for (i=0,idx=tap_count-1; i<tap_count; i++,idx--)
   {
      res = SetBoardProcessingParamPX14(hBrd, fpi_load,
                                        static_cast<unsigned short>(coefficients[idx]));
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

// Load the 65 channel 2 filter coefficients
PX14API FLPC6_LoadCoefficientsCh2PX14 (HPX14 hBrd,
                                       const short* coefficients,
                                       int ordering)
{
   static const unsigned tap_count = 65;
   static const unsigned short fpi_start = 0x8003;
   static const unsigned short fpi_load  = 0x8004;

   unsigned i, idx;
   int res;

   PX14_ENSURE_POINTER(hBrd, coefficients, short, "FLPC6_LoadCoefficientsCh2PX14");

   // The ordering parameter is reserved for a future alternate ordering that
   //  would be necessary if we use a different type of filter
   if (ordering != 0)
      return SIG_PX14_INVALID_ARG_3;

   // Make sure the correct firmware package is installed
   res = IsFixedLogicPackagePresentPX14(hBrd, PX14FLP_C6_FIRFILT_DUAL_CHAN);
   PX14_RETURN_ON_FAIL(res);

   // Write any value to 0x8000; signals firmware that we're loading coefficients
   res = SetBoardProcessingParamPX14(hBrd, fpi_start, 1);

   // Load the coefficients in reverse order
   for (i=0,idx=tap_count-1; i<tap_count; i++,idx--)
   {
      res = SetBoardProcessingParamPX14(hBrd, fpi_load,
                                        static_cast<unsigned short>(coefficients[idx]));
      PX14_RETURN_ON_FAIL(res);
   }

   return SIG_SUCCESS;
}

// Module private function implementation ------------------------------- //


