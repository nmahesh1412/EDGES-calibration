/** @file px14_fw_patch_32p.cpp

	In November 2011, several of the PX14 board revisions went through a new
	hardware revision that included a change to the components in the JTAG 
	chain.
	
	Prior to this change all PX14 devices had the following three items at
	the start of the JTAG chain:
	 1) SYS1 logic EEPROM #2 (xcf16p)
	 2) SYS1 logic EEPROM #1 (xcf32p)
	 3) SYS1 FPGA
	 ... (other stuff)

	In the scheme above, all logic was loaded on EEPROM #1 and EEPROM #2 was
	never used. (It was intended to be used in the event that the SYS1 FPGA
	was ever upgraded to a larger model.)

	As of the November 2011 change, all PX14 devices now have the following
	two items at the start of the JTAG chain:
	 1) SYS1 logic EEPROM (xcf16p)
	 2) SYS1 FPGA

	With this change, we no longer have a xcf32p EEPROM for SYS1 logic and
	so the SYS1 logic will now be stored on a xcf16p EEPROM instead. 
	
	One solution to this is to just start generating two versions of the 
	SYS1 logic, one for the 16p and the other for a 32p. There are a few
	disadvantages to doing this. a) The size of our firmware update files
	will grow by about a megabyte, b) The firmware guys will have to 
	generate two XSVF files, and c) we'd have to regenerate a bunch of
	firmware files for the new cards.

	Another solution is to tweak the 32p XSVF file such that it can be 
	loaded onto a 16p. This file implements this method.

	Q: How did we come up with what needs to be changed?
	A: Using the same underlying device firmware, we generated SVF (not
	   XSVF) files for both 32p and 16p EEPROMs. SVF are text-based JTAG
	   instructions. (XSVF is essentially binary version of SVF.) From there
	   we did a windiff on the two SVF files to find the differences. There
	   were 6 differences in total:
	    4 simple data values differences (XSDRTDO)
		1 simple data value difference   (XRUNTEST)
		1 instance of a couple of extra devce status checks are being done
		  for the 32p device. (16p does one check, 32p does 3 checks)
	   
	The first five items are accounted for in this implementation. The last
	item is left in such that the extra device status checks are run on the
	16p.
*/
#include "stdafx.h"
#include "px14_top.h"
#include "sig_xsvf_player.h"

/** @brief
	  XSVF "player" that will patch an XSVF file generated for a xcv32p 
	  EEPROM such that it can be loaded onto an xcv16p EEPROM.

	  Starting a particular board-revision specific hardware revision, the
	  hardware was modified such that the SYS1 logic is contained on a 
	  single xcv16p EEPROM instead of an xcv32p EEPROM on the previous 
	  hardware revisions.

	  When an instance of this XSVF player is created and is used to play
	  the XSVF data, no data will be uploaded to the hardware. Instead, the
	  player will find the few discrete differences between an XSVF file
	  for an xcv32p and an XSVF file for an xcv16p (using the same EEPROM
	  content of course). After the XSVF playing is complete, the class
	  will then go back and patch the source XSVF file with values 
	  appropriate for an xcv16p. This patched file can then be used by a
	  real XSVF player to load the EEPROM.

	  This allows us to reuse all existing logic update/verify files for
	  both newer and older hardware revisions.
*/
class CPatchXsvfPlayer : public CXsvfFilePlayer
{
public:

	CPatchXsvfPlayer() : m_instLast(0) {m_bNoFailTdoMasking = true;}

	virtual ~CPatchXsvfPlayer() {}

	// - Virtual overrides
	virtual int xsvfExecute (const char* pPathname, int iHir=0, int iTir=0, int iHdr=0, int iTdr=0, int iHdrFpga=0)
	{
		if (pPathname) m_strPathname.assign(pPathname);
		return CXsvfFilePlayer::xsvfExecute(pPathname, iHir, iTir, iHdr, iTdr, iHdrFpga);
	}
	virtual void xsvfCleanup (SXsvfInfo* pXsvfInfo);

	// Dummy implementations for most everything; we're not really doing any JTAG access
	virtual void setPort(short p, short val) {}
	virtual unsigned char readTDOBit() { return 0; }
	virtual void waitTime (int microsec) {}
	virtual void xsvfShiftOnly (long lNumBits, lenVal *plvTdi, lenVal *plvTdoCaptured, int iExitShift) {}
	virtual void xsvfTmsTransition (short sTms) {}
	virtual int xsvfGotoTapState (unsigned char *pucTapState, unsigned char ucTargetState)
		{ *pucTapState = ucTargetState; return XSVF_ERROR_NONE; }

	// These overrides will catch items we need to patch
	virtual int xsvfShift (unsigned char* pucTapState, unsigned char ucStartState, long lNumBits,
        lenVal *plvTdi, lenVal *plvTdoCaptured, lenVal *plvTdoExpected,
        lenVal *plvTdoMask, unsigned char ucEndState, long lRunTestTime, unsigned char ucMaxRepeat,
		int iHeader, int iTrailer );
	virtual void OnRunTest(long runTestTime);

	int ApplyPatches();

protected:

	struct PatchItem
	{
		std::streamoff	file_offset_bytes;
		unsigned int	value;
		unsigned int	value_bytes;		// 1 or 4
	};

	typedef std::list<PatchItem> PatchList;

	bool AddPatchItem (unsigned int value, unsigned int value_bytes, int offset)
	{
		PatchItem pi;

		if ((value_bytes == 4) || (value_bytes == 1))
		{
			pi.file_offset_bytes = (size_t)m_fin.tellg() - offset;
			pi.value             = value;
			pi.value_bytes       = value_bytes;

			m_patchList.push_back(pi);
			return true;
		}

		return false;
	}

	std::string		m_strPathname;
	PatchList		m_patchList;
	long			m_instLast;
};


int CPatchXsvfPlayer::xsvfShift (unsigned char* pucTapState,
								 unsigned char ucStartState,
								 long lNumBits,
								 lenVal *plvTdi,
								 lenVal *plvTdoCaptured,
								 lenVal *plvTdoExpected,
								 lenVal *plvTdoMask,
								 unsigned char ucEndState,
								 long lRunTestTime,
								 unsigned char ucMaxRepeat,
								 int iHeader,
								 int iTrailer)
{ 
	if (ucStartState == XTAPSTATE_SHIFTIR)
	{
		// Copy last instruction
		m_instLast = value(plvTdi);
		return XSVF_ERROR_NONE;
	}

	if ((ucStartState == XTAPSTATE_SHIFTDR) && (plvTdoExpected->len <= 32))
	{
		// IDCODE
		if ((m_instLast == 0xFE) && (0xF5059093 == value(plvTdoExpected)))
		{
			// Jump back 4 bytes and write 0xF5058093
			AddPatchItem(0x938005F5, 4, 4);	// <- Code reads a byte at a time, write backward
			return XSVF_ERROR_NONE;
		}

		// XSC_DATA_BTC
		if ((m_instLast == 0xF2) && (0xFFFFFFEC == value(plvTdi)))
		{
			// Jump back 8 bytes and write 0xFFFFFFE4
			AddPatchItem(0xE4FFFFFF, 4, 8);
			return XSVF_ERROR_NONE;
		}

		// XSC_DATA_DONE
		if ((m_instLast == 0x09) && (0xC0 == value(plvTdi)))
		{
			// Jump back 2 bytes and write 0xCC
			AddPatchItem(0xCC, 1, 2);
			return XSVF_ERROR_NONE;
		}
	}

	return XSVF_ERROR_NONE;
}

void CPatchXsvfPlayer::OnRunTest(long runTestTime)
{
	if (runTestTime == 140000000)
	{
		// Jump back 4 bytes and write 80000000 == 0x04C4B400
		AddPatchItem(0x00B4C404, 4, 4);
	}
}

void CPatchXsvfPlayer::xsvfCleanup (SXsvfInfo* pXsvfInfo)
{
	CXsvfFilePlayer::xsvfCleanup(pXsvfInfo);
	ApplyPatches();
}

int CPatchXsvfPlayer::ApplyPatches()
{
	if (!m_patchList.empty())
	{
		FILE* filp;

#if defined(_WIN32) && !defined(PX14PP_NO_SECURE_CRT)
		filp = NULL;
		fopen_s(&filp, m_strPathname.c_str(), "r+");
#else
		filp = fopen(m_strPathname.c_str(), "r+");
#endif
		if (NULL == filp)
			return SIG_PX14_FILE_IO_ERROR;

		PatchList::const_iterator iPatch(m_patchList.begin());
		for (; iPatch!=m_patchList.end(); iPatch++)
		{
			const PatchItem& pi = *iPatch;
#ifdef _WIN64
			_fseeki64(filp, pi.file_offset_bytes, SEEK_SET);
#else
			fseek(filp, pi.file_offset_bytes, SEEK_SET);
#endif
			fwrite(&pi.value, pi.value_bytes, 1, filp);
		}

		fclose(filp);
	}

	return SIG_SUCCESS;
}


int PatchXsvf_32p_to_16p (const char* pXsvfFile)
{
	int res;

	CPatchXsvfPlayer xp;
	res = xp.xsvfExecute(pXsvfFile);
	if (SIG_SUCCESS != res)
		return SIG_PX14_FIRMWARE_UPLOAD_FAILED;

	return SIG_SUCCESS;
}


