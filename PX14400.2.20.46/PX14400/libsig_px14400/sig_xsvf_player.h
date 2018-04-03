/** @file	sig_xsvf_player.h
	@brief	Class wrapper around Xilinx XSVF file player

	This file defines the class CXsvfFilePlayer
*/
#ifndef sig_xsvf_player_header_defined
#define sig_xsvf_player_header_defined

#include <fstream>

/// Define this to support the XC9500/XL XSVF data compression scheme.
/// Define this to support the new XSVF error codes.
#define XSVF_SUPPORT_ERRORCODES

#define TCK ((short) 0)
#define TMS ((short) 1)
#define TDI ((short) 2)

#ifdef _DEBUG
# define DEBUG_MODE
#endif

/* Legacy error codes for xsvfExecute from original XSVF player v2.0 */
#define XSVF_LEGACY_SUCCESS		1
#define XSVF_LEGACY_ERROR		0
/* 4.04 [NEW] Error codes for xsvfExecute. */
/* Must #define XSVF_SUPPORT_ERRORCODES to get these codes */
#define XSVF_ERROR_NONE         0
#define XSVF_ERROR_UNKNOWN      1
#define XSVF_ERROR_TDOMISMATCH  2
#define XSVF_ERROR_MAXRETRIES   3   /* TDO mismatch after max retries */
#define XSVF_ERROR_ILLEGALCMD   4
#define XSVF_ERROR_ILLEGALSTATE 5
#define XSVF_ERROR_DATAOVERFLOW 6   /* Data > lenVal LENVAL_MAX_LEN buffer size*/
#define XSVF_ERROR_FILEIO		7
/* Insert new errors here */
#define XSVF_ERROR_LAST         8

/*============================================================================
* XSVF Command Parameter Values
============================================================================*/

#define XSTATE_RESET     0          /* 4.00 parameter for XSTATE */
#define XSTATE_RUNTEST   1          /* 4.00 parameter for XSTATE */

#define XENDXR_RUNTEST   0          /* 4.04 parameter for XENDIR/DR */
#define XENDXR_PAUSE     1          /* 4.04 parameter for XENDIR/DR */

/* TAP states */
#define XTAPSTATE_RESET     0x00
#define XTAPSTATE_RUNTEST   0x01    /* a.k.a. IDLE */
#define XTAPSTATE_SELECTDR  0x02
#define XTAPSTATE_CAPTUREDR 0x03
#define XTAPSTATE_SHIFTDR   0x04
#define XTAPSTATE_EXIT1DR   0x05
#define XTAPSTATE_PAUSEDR   0x06
#define XTAPSTATE_EXIT2DR   0x07
#define XTAPSTATE_UPDATEDR  0x08
#define XTAPSTATE_IRSTATES  0x09    /* All IR states begin here */
#define XTAPSTATE_SELECTIR  0x09
#define XTAPSTATE_CAPTUREIR 0x0A
#define XTAPSTATE_SHIFTIR   0x0B
#define XTAPSTATE_EXIT1IR   0x0C
#define XTAPSTATE_PAUSEIR   0x0D
#define XTAPSTATE_EXIT2IR   0x0E
#define XTAPSTATE_UPDATEIR  0x0F

#define LENVAL_MAX_LEN			7000			// Maximum lenval

class CXsvfFilePlayer
{
public:

	// -- Construction

	CXsvfFilePlayer();

	// -- Attributes

	int GetVerbosity() const;
	void SetVerbosity (int val);

	// -- Methods

	virtual int xsvfExecute (const char* pPathname, int iHir=0, int iTir=0, 
		int iHdr=0, int iTdr=0, int iHdrFpga=0);

	// -- Implementation

	virtual ~CXsvfFilePlayer() {}

	//  - Mandatory overrides

	/// Set the port "p" (TCK, TMS, or TDI) to val (0 or 1)
	virtual void setPort(short p, short val) = 0;

	/// Read the TDO bit and store it in val
	virtual unsigned char readTDOBit() = 0;

	// -- lenval ------

	typedef struct var_len_byte
	{
		short len;   /* number of chars in this value */
		unsigned char val[LENVAL_MAX_LEN+1];  /* bytes of data */

	} lenVal;

	/// Return the long representation of a lenVal
	long value(lenVal *x);
	/// Set lenVal equal to value
	void initLenVal(lenVal *x, long value);
	/// Check if expected equals actual (taking the mask into account)
	short EqualLenVal(lenVal *expected, lenVal *actual, lenVal *mask);
	/// Add val1+val2 and put the result in resVal
	void addVal(lenVal *resVal, lenVal *val1, lenVal *val2);
	/// Return the (byte, bit) of lv (reading from left to right)
	short RetBit(lenVal *lv, int byte, int bit);
	/// Set the (byte, bit) of lv equal to val
	void SetBit(lenVal *lv, int byte, int bit, short val);
	/// Read from XSVF numBytes bytes of data into x
	void readVal(lenVal *x, short numBytes);

	// -- ports ---------

	/// make clock go down->up->down
	virtual void pulseClock();
	/// read the next byte of data from the xsvf file
	virtual void readByte(unsigned char *data);
	/// Delay/stall
	virtual void waitTime(int microsec);

	// -- micro -------------

	/**	This structure contains all of the data used during the
		execution of the XSVF.  Some data is persistent, predefined
		information (e.g. lRunTestTime).  The bulk of this struct's
		size is due to the lenVal structs (defined in lenval.h)
		which contain buffers for the active shift data.  The LENVAL_MAX_LEN
		#define in lenval.h defines the size of these buffers.
		These buffers must be large enough to store the longest
		shift data in your XSVF file.  For example:
		LENVAL_MAX_LEN >= ( longest_shift_data_in_bits / 8 )
		Because the lenVal struct dominates the space usage of this
		struct, the rough size of this struct is:
		sizeof( SXsvfInfo ) ~= LENVAL_MAX_LEN * 7 (number of lenVals)
		xsvfInitialize() contains initialization code for the data
		in this struct.

		xsvfCleanup() contains cleanup code for the data in this
		struct.
	*/
	typedef struct tagSXsvfInfo
	{
		/* XSVF status information */
		unsigned char	ucComplete;         ///< 0 = running; 1 = complete
		unsigned char	ucCommand;          ///< Current XSVF command byte
		long			lCommandCount;      ///< Number of commands processed
		int				iErrorCode;         ///< An error code. 0 = no error

		/* TAP state/sequencing information */
		unsigned char	ucTapState;         ///< Current TAP state
		unsigned char	ucEndIR;            ///< ENDIR TAP state (See SVF)
		unsigned char	ucEndDR;            ///< ENDDR TAP state (See SVF)

		/* RUNTEST information */
		unsigned char	ucMaxRepeat;        ///< Max repeat loops (for xc9500/xl)
		long			lRunTestTime;       ///< Pre-specified RUNTEST time (usec)

		/* Shift Data Info and Buffers */
		long			lShiftLengthBits;   ///< Len. current shift data in bits
		short			sShiftLengthBytes;  ///< Len. current shift data in bytes

		lenVal			lvTdi;              ///< Current TDI shift data
		lenVal			lvTdoExpected;      ///< Expected TDO shift data
		lenVal			lvTdoCaptured;      ///< Captured TDO shift data
		lenVal			lvTdoMask;          ///< TDO mask: 0=dontcare; 1=compare

	#ifdef  XSVF_SUPPORT_COMPRESSION
		/* XSDRINC Data Buffers */
		lenVal			lvAddressMask;      ///< Address mask for XSDRINC
		lenVal			lvDataMask;         ///< Data mask for XSDRINC
		lenVal			lvNextData;         ///< Next data for XSDRINC
	#endif  /* XSVF_SUPPORT_COMPRESSION */

		int				iHir;               ///< # IR header bits
		int				iTir;               ///< # IR trailer bits
		int				iHdr;               ///< # DR header bits
		int				iTdr;               ///< # DR trailer bits
		int				iHdrFpga;           ///< # DR header bits for FPGA

	} SXsvfInfo;

	/*============================================================================
	* XSVF Function Prototypes
	============================================================================*/

	int xsvfDoIllegalCmd (SXsvfInfo* pXsvfInfo);   // Illegal command function
	int xsvfDoXCOMPLETE (SXsvfInfo* pXsvfInfo);
	int xsvfDoXTDOMASK (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSIR (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSIR2 (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSDR (SXsvfInfo* pXsvfInfo);
	int xsvfDoXRUNTEST (SXsvfInfo* pXsvfInfo);
	int xsvfDoXREPEAT (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSDRSIZE (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSDRTDO (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSDRINC (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSDRBCE (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSDRTDOBCE (SXsvfInfo* pXsvfInfo);
	int xsvfDoXSTATE (SXsvfInfo* pXsvfInfo);
	int xsvfDoXENDXR (SXsvfInfo* pXsvfInfo);
	int xsvfDoXCOMMENT (SXsvfInfo* pXsvfInfo);
	int xsvfDoXWAIT (SXsvfInfo* pXsvfInfo);
	/* Insert new command functions here */

	virtual int xsvfInitialize (SXsvfInfo* pXsvfInfo);
	virtual void xsvfCleanup (SXsvfInfo* pXsvfInfo);
	virtual void xsvfProgress (int cur, int total);

	int xsvfRun (SXsvfInfo* pXsvfInfo);
	int xsvfBasicXSDRTDO (unsigned char *pucTapState, long lShiftLengthBits, short sShiftLengthBytes, 
		lenVal *plvTdi, lenVal *plvTdoCaptured, lenVal *plvTdoExpected, lenVal *plvTdoMask,
        unsigned char ucEndState, long lRunTestTime, unsigned char ucMaxRepeat, int iHeader, int iTrailer );
	virtual int xsvfShift (unsigned char*   pucTapState, unsigned char ucStartState, long lNumBits,
        lenVal *plvTdi, lenVal *plvTdoCaptured, lenVal *plvTdoExpected,
        lenVal *plvTdoMask, unsigned char ucEndState, long lRunTestTime, unsigned char ucMaxRepeat,
        int iHeader, int iTrailer );
	virtual int xsvfGotoTapState (unsigned char *pucTapState, unsigned char ucTargetState);
	int xsvfInfoInit (SXsvfInfo* pXsvfInfo);
	void xsvfInfoCleanup (SXsvfInfo* pXsvfInfo);
	short xsvfGetAsNumBytes (long lNumBits);
	virtual void xsvfTmsTransition (short sTms);
	virtual void xsvfShiftOnly (long lNumBits, lenVal *plvTdi, 
		lenVal *plvTdoCaptured, int iExitShift);
	virtual void OnRunTest(long runTestTime) {}

#ifdef XSVF_SUPPORT_COMPRESSION
	int xsvfDoXSETSDRMASKS(SXsvfInfo* pXsvfInfo);
	void xsvfDoSDRMasking (lenVal *plvTdi, lenVal *plvNextData,
        lenVal *plvAddressMask, lenVal *plvDataMask );
#endif

	std::ifstream	m_fin;
	int				m_xsvf_iDebugLevel;
	bool			m_bNoFailTdoMasking;
};

#endif // sig_xsvf_player_header_defined

