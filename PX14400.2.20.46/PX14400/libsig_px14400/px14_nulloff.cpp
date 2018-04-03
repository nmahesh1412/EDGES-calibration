/** @file px14_nulloff.cpp
*/
#include "stdafx.h"
#include "px14_top.h"

#ifdef _DEBUG
# define PX14_LIBCHECK_EXTRA(res, msg, hBrd)				\
	if ((res)<SIG_SUCCESS) { if (msg) SetErrorExtra(hBrd, msg); return (res); }
#else
# define PX14_LIBCHECK_EXTRA(res, msg, hBrd)				\
	if ((res)<SIG_SUCCESS) { (void)msg; return (res); }
#endif

static int _CenterSigWork  (HPX14 hBrd, int chan_mask, int abs_chan_idx, px14_sample_t center_to);
static int _CalcNewAverage (HPX14 hBrd, unsigned int samps_per_chan, unsigned int chan_count, int rel_idx,
							px14_sample_t* bufp, px14_sample_t& samp_avg);

class CHwSetCache
{
public:

	CHwSetCache(HPX14 hBrd);
	~CHwSetCache() { if (m_hBrd != PX14_INVALID_HANDLE) Restore(); }

protected:

	int Restore();

private:

	HPX14	m_hBrd;

	double	int_clk_rate;
	int		ms_cfg;
	int		clk_src;
	int		clk_ref_src;
	int		pll_disable;
	int		post_adc_div;

	int		trig_mode;
	int		pre_trig_samps;
	int		delay_trig_samps;

	int		active_channels;
	int		fpga_proc_en;
} ;

/** @brief
	  Adjust DC offset for given channel such that average sample value
	  sits a specified value

	This function adjusts the DC offset for the given channel until the
	average sample value is equal to the given center_to parameter. In order
	for this function to work correctly a properly terminated connection
	must be made to the analog input channel. The actual analog input should
	be either nothing or a symmetric input such as a sine wave. (If using a
	sine wave, be careful that it is not saturated when calling this 
	function or it will skew the results.)

	It is the caller's responsibility to ensure that properly terminated
	input is connected to the analog input(s) to be adjusted.

	This function requires a valid clock to be present and will use the
	internal clock if present. Some slave devices may not have an internal
	clock available. For cards with no internal clock, this function will
	assume that a valid external clock is present and use it for the DC
	offset adjustment.

	The adjustment works as follows:
	 - The function will adjust clock/trigger settings as necessary. All
	   original hardware settings (except the DC offset of course) will 
	   be restored before the function returns.
	 - The adjustment starts at the current DC offset level.
	 - A small RAM acquisition is performed and the average sample value is
	   calculated.
	 - If the average sample value is less than the center_to parameter
	   value the DC offset is incremented by one.
	 - If the average sample value is greater than the center_to parameter
	   value the DC offset is decremented by one.
	 - If the average sample value is equal to the center_to parameter the
	   adjustment is complete.
	 - Adjustment fails if the minimum/maximum DC offset value is reached.

	The adjusted DC-offset value is not returned by this function. Use the 
	appropriate GetDcOffsetCh*PX14 function to get the resultant DC offset
	value.

	@param hBrd
		A handle to a PX14400 device obtained by calling ConnectToDevicePX14
	@param chan_idx
		The 0-based absolute channel index to adjust. If this parameter is 
		-1 then all currently active channels (according to 
		GetActiveChannelMaskPX14) will be adjusted. If a specific channel
		index is specified and that channel is not currently active, the
		active channel selection will be updated to select that channel
		(single channel acquisition) for the adjustment. In this case the
		original active channel selection will be restored on function exit.
	@param center_to
		The target average sample value.
		
	@return
		Returns SIG_SUCCESS on success of one of the SIG_* error values 
		(which are all negative) on error.
*/
PX14API CenterSignalPX14 (HPX14         hBrd,
						  int           chan_idx,
						  px14_sample_t center_to)
{
	int res, chan_mask, ms_cfg;
	unsigned int brd_rev;
	
	if ((chan_idx < -1) || (chan_idx > PX14_MAX_CHANNELS))
		return SIG_PX14_INVALID_ARG_2;
	
	res = ValidateHandle(hBrd);
	PX14_RETURN_ON_FAIL(res);

	res = GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
	PX14_RETURN_ON_FAIL(res);

	// Don't even bother if board revision isn't DC-coupled
	if ((brd_rev == PX14BRDREV_PX14400) || (brd_rev == PX14BRDREV_PX12500))
		return SIG_SUCCESS;
	if (IsDeviceVirtualPX14(hBrd))
		return SIG_SUCCESS;

	if (!InIdleModePX14(hBrd))
		return SIG_PX14_BUSY;

	chan_mask = GetActiveChannelMaskPX14(hBrd);

	// Object will restore any hardware settings that we change on destruction.
	// Note this is not a general purpose class; only a subset of all hardware
	//  settings are considered.
	CHwSetCache hsc(hBrd);

	res = SetBoardProcessingEnablePX14(hBrd, PX14_FALSE);
	PX14_LIBCHECK_EXTRA(res, "Failed to set board processing enable", hBrd);

	// Setup active channels as necessary
	if ((chan_idx != -1) && (0 == (chan_mask & (1 << chan_idx))))
	{
		res = SetActiveChannelsMaskPX14(hBrd, 1 << chan_idx);
		PX14_LIBCHECK_EXTRA(res, "Failed to set active channel mask", hBrd);
	}

	// Can't center signal as a slave
	ms_cfg = GetMasterSlaveConfigurationPX14(hBrd);
	if (PX14_IS_SLAVE(ms_cfg))
	{
		res = SetMasterSlaveConfigurationPX14(hBrd, PX14MSCFG_STANDALONE);
		PX14_LIBCHECK_EXTRA(res, "Failed to set master slave configuration", hBrd);
	}

	// Setup internal clock
	if (0 == (GetBoardFeaturesPX14(hBrd) & PX14FEATURE_NO_INTERNAL_CLOCK))
	{
		// Set the external clock so that the following items don't all wait for PLL lock
		res = SetAdcClockSourcePX14(hBrd, PX14CLKSRC_EXTERNAL);
		PX14_LIBCHECK_EXTRA(res, "Failed to set clock source", hBrd);

		res = SetInternalAdcClockReferencePX14(hBrd, PX14CLKREF_INT_10MHZ);
		PX14_LIBCHECK_EXTRA(res, "Failed to set internal clock reference source", hBrd);
		res = _SetPllDisablePX14(hBrd, PX14_FALSE);
		PX14_LIBCHECK_EXTRA(res, "Failed to set PLL enable", hBrd);
		res = SetInternalAdcClockRatePX14(hBrd, 400);
		PX14_LIBCHECK_EXTRA(res, "Failed to set internal clock rate", hBrd);

		// Lastly set internal clock
		res = SetAdcClockSourcePX14(hBrd, PX14CLKSRC_INT_VCO);
		PX14_LIBCHECK_EXTRA(res, "Failed to set internal clock source", hBrd);
	}
	res = SetPostAdcClockDividerPX14(hBrd, PX14POSTADCCLKDIV_01);
	PX14_LIBCHECK_EXTRA(res, "Failed to set post ADC clock div", hBrd);

	// Setup trigger
	res = SetTriggerModePX14(hBrd, PX14TRIGMODE_POST_TRIGGER);
	PX14_LIBCHECK_EXTRA(res, "Failed to set trigger source", hBrd);
	res = SetPreTriggerSamplesPX14(hBrd, 0);
	PX14_LIBCHECK_EXTRA(res, "Failed to set pre trigger samples", hBrd);
	res = SetTriggerDelaySamplesPX14(hBrd, 0);
	PX14_LIBCHECK_EXTRA(res, "Failed to set trigger delay samples", hBrd);

	try
	{
		if (chan_idx == -1)
		{
			for (int ch=0; ch<PX14_MAX_CHANNELS; ch++)
			{
				if (chan_mask & (1 << ch))
				{
					res = _CenterSigWork(hBrd, chan_mask, ch, center_to);
					PX14_RETURN_ON_FAIL(res);
				}
			}
		}
		else
		{
			res = _CenterSigWork(hBrd, chan_mask, chan_idx, center_to);
			PX14_RETURN_ON_FAIL(res);
		}
	}
	catch (std::bad_alloc)
	{
		return SIG_OUTOFMEMORY;
	}
	
	return SIG_SUCCESS;
}

CHwSetCache::CHwSetCache(HPX14 hBrd)
	: m_hBrd(hBrd)
{
	SIGASSERT (IsHandleValidPX14(m_hBrd));

	GetInternalAdcClockRatePX14(m_hBrd, &int_clk_rate);

	active_channels  = GetActiveChannelsPX14(m_hBrd);
	ms_cfg           = GetMasterSlaveConfigurationPX14(m_hBrd);
	trig_mode        = GetTriggerModePX14(m_hBrd);
	clk_src          = GetAdcClockSourcePX14(m_hBrd);
	clk_ref_src      = GetInternalAdcClockReferencePX14(m_hBrd);
	fpga_proc_en     = GetBoardProcessingEnablePX14(m_hBrd);
	pll_disable      = _GetPllDisablePX14(m_hBrd);
	post_adc_div     = GetPostAdcClockDividerPX14(m_hBrd);
	pre_trig_samps   = GetPreTriggerSamplesPX14(m_hBrd);
	delay_trig_samps = GetTriggerDelaySamplesPX14(m_hBrd);
}

int CHwSetCache::Restore()
{
	int res, res_fail;
	double dRateNow;
	bool bMultiFail;

	res = IsHandleValidPX14(m_hBrd);
	SIGASSERT (res);
	if (!res) return SIG_NODEV;

	res_fail = SIG_SUCCESS;
	bMultiFail = false;

	// Explicitly select external clock here so that we don't need to wait
	//  for potential side effects (PLL wait) while we're restoring 
	//  settings. The last item below is the clock source which will restore
	//  internal as necessary.
	SetAdcClockSourcePX14(m_hBrd, PX14CLKSRC_EXTERNAL);

	GetInternalAdcClockRatePX14(m_hBrd, &dRateNow);
	if (dRateNow != int_clk_rate)
	{
		res = SetInternalAdcClockRatePX14(m_hBrd, int_clk_rate);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (active_channels != GetActiveChannelsPX14(m_hBrd))
	{
		res = SetActiveChannelsPX14(m_hBrd, active_channels);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (trig_mode != GetTriggerModePX14(m_hBrd))
	{
		res = SetTriggerModePX14(m_hBrd, trig_mode);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (clk_ref_src != GetInternalAdcClockReferencePX14(m_hBrd))
	{
		res = SetInternalAdcClockReferencePX14(m_hBrd, clk_ref_src);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (fpga_proc_en != GetBoardProcessingEnablePX14(m_hBrd))
	{
		res = GetBoardProcessingEnablePX14(m_hBrd, fpga_proc_en);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (pll_disable != _GetPllDisablePX14(m_hBrd))
	{
		res = _SetPllDisablePX14(m_hBrd, pll_disable);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (post_adc_div != GetPostAdcClockDividerPX14(m_hBrd))
	{
		res = SetPostAdcClockDividerPX14(m_hBrd, post_adc_div);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (pre_trig_samps != GetPreTriggerSamplesPX14(m_hBrd))
	{
		res = SetPreTriggerSamplesPX14(m_hBrd, pre_trig_samps);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (delay_trig_samps != GetTriggerDelaySamplesPX14(m_hBrd))
	{
		res = SetTriggerDelaySamplesPX14(m_hBrd, delay_trig_samps);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (ms_cfg != GetMasterSlaveConfigurationPX14(m_hBrd))
	{
		res = SetMasterSlaveConfigurationPX14(m_hBrd, ms_cfg);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	if (clk_src != GetAdcClockSourcePX14(m_hBrd))
	{
		res = SetAdcClockSourcePX14(m_hBrd, clk_src);
		if (res != SIG_SUCCESS) { if (res_fail == SIG_SUCCESS) res_fail = res; else bMultiFail = true; }
	}

	return bMultiFail ? SIG_PX14_QUASI_SUCCESSFUL : res_fail;
}

int _CalcNewAverage (HPX14          hBrd,
					 unsigned int   samps_per_chan,
					 unsigned int   chan_count,
					 int            rel_idx,
					 px14_sample_t* bufp,
					 px14_sample_t& samp_avg)
{
	static const unsigned s_timeout_ms  = 2000;

	register unsigned int i;
	unsigned int total_samples;
	double dSum;
	int res;

	total_samples = samps_per_chan * chan_count;

	// Get fresh data
	res = AcquireToBoardRamPX14(hBrd, 0, total_samples, 0, PX14_TRUE);
	PX14_RETURN_ON_FAIL(res);
	IssueSoftwareTriggerPX14(hBrd);
	res = WaitForAcquisitionCompletePX14(hBrd, s_timeout_ms);
	SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
	PX14_RETURN_ON_FAIL(res);
	res = ReadSampleRamBufPX14 (hBrd, 0, total_samples, bufp);
	PX14_RETURN_ON_FAIL(res);

	// Average it
	bufp += rel_idx;
	for (i=0,dSum=0; i<samps_per_chan; i++,bufp+=chan_count)
		dSum += *bufp;
	samp_avg = static_cast<px14_sample_t>(dSum / samps_per_chan);

	return SIG_SUCCESS;
}

int _CenterSigWork (HPX14 hBrd, int chan_mask, int abs_chan_idx, px14_sample_t center_to)
{
	static const unsigned s_acq_samples_per_chan = 16384;
	
	int res, dc, chan_count, total_samples, rel_idx;
	px14_sample_t samp_avg, samp_avg_last, *bufp;
	px14lib_set_func_t pfnSet;
	px14lib_get_func_t pfnGet;

	// Channel should be set in channel mask
	SIGASSERT(0 != (chan_mask & (1 << abs_chan_idx)));

	PX14_CT_ASSERT(2 == PX14_MAX_CHANNELS);
	switch (abs_chan_idx)
	{
	case 0:
		pfnSet = SetDcOffsetCh1PX14;
		pfnGet = GetDcOffsetCh1PX14;
		break;
	case 1:
		pfnSet = SetDcOffsetCh2PX14;
		pfnGet = GetDcOffsetCh2PX14;
		break;
	default:
		SIGASSERT(0);
		return SIG_PX14_UNEXPECTED;
	}

	rel_idx = GetRelChanIdxFromChanMaskPX14(chan_mask, abs_chan_idx);
	chan_count = GetChanCountFromChanMaskPX14(chan_mask);
	total_samples = chan_count * s_acq_samples_per_chan;
	std::vector<px14_sample_t> acq_buf(total_samples);
	bufp = &*acq_buf.begin();
	
	// Look at current data to see where we're at
	res = _CalcNewAverage(hBrd, s_acq_samples_per_chan, chan_count, rel_idx, bufp, samp_avg);
	PX14_RETURN_ON_FAIL(res);
	if (samp_avg == center_to)
		return SIG_SUCCESS;

	dc = (*pfnGet)(hBrd, PX14_TRUE);
	samp_avg_last = samp_avg;
	
	if (samp_avg < center_to)
	{
		// Moving up
		for (dc++; dc<=PX14_MAX_DC_OFFSET; dc++,samp_avg_last=samp_avg)
		{
			res = (*pfnSet)(hBrd, dc);
			PX14_RETURN_ON_FAIL(res);

			res = _CalcNewAverage(hBrd, s_acq_samples_per_chan, chan_count, rel_idx, bufp, samp_avg);
			PX14_RETURN_ON_FAIL(res);

			if ((samp_avg_last < center_to) && (samp_avg >= center_to))
				return SIG_SUCCESS;
		}
	}
	else
	{
		// Moving down
		for (dc--; dc>=0; dc--,samp_avg_last=samp_avg)
		{
			res = (*pfnSet)(hBrd, dc);
			PX14_RETURN_ON_FAIL(res);

			res = _CalcNewAverage(hBrd, s_acq_samples_per_chan, chan_count, rel_idx, bufp, samp_avg);
			PX14_RETURN_ON_FAIL(res);

			if ((samp_avg_last > center_to) && (samp_avg <= center_to))
				return SIG_SUCCESS;
		}
	}

	return SIG_PX14_OPERATION_FAILED;
}

