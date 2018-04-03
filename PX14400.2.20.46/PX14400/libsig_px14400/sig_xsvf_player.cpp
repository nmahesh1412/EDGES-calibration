/** @file	sig_xsvf_player.cpp
  @brief	Wrapper class implementation of the Xilinx XSVF file player
  */
#include "stdafx.h"
#include "sig_xsvf_player.h"
#include "stdio.h"

typedef CXsvfFilePlayer::lenVal lenVal;

#define XSVF_VERSION    "5.01.03"

#ifdef  XSVF_SUPPORT_ERRORCODES
#define XSVF_ERRORCODE(errorCode)   errorCode
#else   /* Use legacy error code */
#define XSVF_ERRORCODE(errorCode)   ((errorCode==XSVF_ERROR_NONE)?1:0)
#endif  /* XSVF_SUPPORT_ERRORCODES */

/*============================================================================
 * DEBUG_MODE #define
 ============================================================================*/

#ifdef DEBUG_MODE
# define XSVFDBG_PRINTF(iDebugLevel,pzFormat) \
{ if (m_xsvf_iDebugLevel >= iDebugLevel) printf(pzFormat); }
# define XSVFDBG_PRINTF1(iDebugLevel,pzFormat,arg1) \
{ if (m_xsvf_iDebugLevel >= iDebugLevel) printf(pzFormat, arg1); }
# define XSVFDBG_PRINTF2(iDebugLevel,pzFormat,arg1,arg2) \
{ if (m_xsvf_iDebugLevel >= iDebugLevel) printf(pzFormat, arg1, arg2); }
# define XSVFDBG_PRINTF3(iDebugLevel,pzFormat,arg1,arg2,arg3) \
{ if (m_xsvf_iDebugLevel >= iDebugLevel) printf(pzFormat, arg1, arg2, arg3); }
# define XSVFDBG_PRINTLENVAL(iDebugLevel,plenVal) \
{ if (m_xsvf_iDebugLevel >= iDebugLevel) xsvfPrintLenVal(plenVal); }
#else
# define XSVFDBG_PRINTF(iDebugLevel,pzFormat)
# define XSVFDBG_PRINTF1(iDebugLevel,pzFormat,arg1)
# define XSVFDBG_PRINTF2(iDebugLevel,pzFormat,arg1,arg2)
# define XSVFDBG_PRINTF3(iDebugLevel,pzFormat,arg1,arg2,arg3)
# define XSVFDBG_PRINTLENVAL(iDebugLevel,plenVal)
#endif

CXsvfFilePlayer::CXsvfFilePlayer() : m_xsvf_iDebugLevel(0), 
   m_bNoFailTdoMasking(false)
{
}

/** @brief	Extract the long value from the lenval array.

  @param plvValue		Pointer to lenval.
  @return				Returns the extracted value.
  */
long CXsvfFilePlayer::value (lenVal *plvValue)
{
   long    lValue;         /* result to hold the accumulated result */
   short   sIndex;

   lValue  = 0;
   for (sIndex = 0; sIndex < plvValue->len ; ++sIndex)
   {
      lValue <<= 8;                       /* shift the accumulated result */
      lValue |= plvValue->val[ sIndex];   /* get the last byte first */
   }

   return(lValue);
}

/** @brief	Initialize the lenval array with the given value.
  @note	Assumes lValue is less than 256.
  @param plv		Pointer to lenval
  @param lValue	The value to set.
  */
void CXsvfFilePlayer::initLenVal (lenVal *plv, long lValue)
{
   plv->len    = 1;
   plv->val[0] = (unsigned char)lValue;
}

/** @brief	Compare two lenval arrays with an optional mask.
  @param plvTdoExpected	Pointer to lenval #1.
  @param plvTdoCaptured	Pointer to lenval #2.
  @param plvTdoMask		Optional ptr to mask (=0 if no mask).
  @return 0 = mismatch; 1 = equal.
  */
short CXsvfFilePlayer::EqualLenVal (lenVal*  plvTdoExpected,
                                    lenVal*  plvTdoCaptured,
                                    lenVal*  plvTdoMask)
{
   unsigned char ucByteVal1, ucByteVal2, ucByteMask;
   short sEqual, sIndex;

   if (m_bNoFailTdoMasking)
      return 1;

   sEqual  = 1;
   sIndex  = plvTdoExpected->len;

   while (sEqual && sIndex--)
   {
      ucByteVal1  = plvTdoExpected->val[ sIndex ];
      ucByteVal2  = plvTdoCaptured->val[ sIndex ];
      if (plvTdoMask)
      {
         ucByteMask  = plvTdoMask->val[ sIndex ];
         ucByteVal1  &= ucByteMask;
         ucByteVal2  &= ucByteMask;
      }
      if (ucByteVal1 != ucByteVal2)
      {
         sEqual  = 0;
      }
   }

   return(sEqual);
}

/** @brief	Return the (byte, bit) of lv (reading from left to right).
  @param plv		Pointer to lenval.
  @param iByte	The byte to get the bit from.
  @param iBit		The bit number (0=msb)
  @return			Returns the bit value.
  */
short CXsvfFilePlayer::RetBit (lenVal *plv, int iByte, int iBit)
{
   /* assert((iByte >= 0) && (iByte < plv->len)); */
   /* assert((iBit >= 0) && (iBit < 8)); */
   return((short)((plv->val[ iByte ] >> (7 - iBit)) & 0x1));
}


/**	@brief	Set the (byte, bit) of lv equal to val

Example:      SetBit("00000000",byte, 1) equals "01000000".

@param plv     Pointer to lenval.
@param iByte   The byte to get the bit from.
@param iBit    The bit number (0=msb).
@param sVal    The bit value to set.
*/
void CXsvfFilePlayer::SetBit (lenVal *plv, int iByte, int iBit, short sVal)
{
   unsigned char   ucByteVal;
   unsigned char   ucBitMask;

   ucBitMask   = (unsigned char)(1 << (7 - iBit));
   ucByteVal   = (unsigned char)(plv->val[ iByte ] & (~ucBitMask));

   if (sVal)
   {
      ucByteVal   |= ucBitMask;
   }
   plv->val[ iByte ]   = ucByteVal;
}

/**	@brief	Add val1 to val2 and store in resVal;
  @pre	Assumes val1 and val2  are of equal length.

  @param plvResVal   Pointer to result.
  @param plvVal1     Pointer of addendum.
  @param plvVal2     Pointer of addendum.
  */
void CXsvfFilePlayer::addVal (lenVal *plvResVal, lenVal *plvVal1, lenVal *plvVal2)
{
   unsigned char   ucCarry;
   unsigned short  usSum;
   unsigned short  usVal1;
   unsigned short  usVal2;
   short           sIndex;

   plvResVal->len  = plvVal1->len;         /* set up length of result */

   /* start at least significant bit and add bytes    */
   ucCarry = 0;
   sIndex  = plvVal1->len;
   while (sIndex--)
   {
      usVal1  = plvVal1->val[ sIndex ];   /* i'th byte of val1 */
      usVal2  = plvVal2->val[ sIndex ];   /* i'th byte of val2 */

      /* add the two bytes plus carry from previous addition */
      usSum   = (unsigned short)(usVal1 + usVal2 + ucCarry);

      /* set up carry for next byte */
      ucCarry = (unsigned char)((usSum > 255) ? 1 : 0);

      /* set the i'th byte of the result */
      plvResVal->val[ sIndex ]    = (unsigned char)usSum;
   }
}

/**	@brief Read from XSVF numBytes bytes of data into x.
  @param plv			Pointer to lenval in which to put the bytes read.
  @param sNumBytes	The number of bytes to read.
  */
void CXsvfFilePlayer::readVal (lenVal *plv, short sNumBytes)
{
   unsigned char*  pucVal;

   plv->len    = sNumBytes;        /* set the length of the lenVal        */
   for (pucVal = plv->val; sNumBytes; --sNumBytes, ++pucVal)
   {
      /* read a byte of data into the lenVal */
      readByte(pucVal);
   }
}

void CXsvfFilePlayer::pulseClock()
{
   setPort(TCK,0);  /* set the TCK port to low  */
   setPort(TCK,1);  /* set the TCK port to high */
}

/*============================================================================
 * XSVF Type Declarations
 ============================================================================*/

/* Declare pointer to functions that perform XSVF commands */
typedef int (CXsvfFilePlayer::*TXsvfDoCmdFuncPtr) (CXsvfFilePlayer::SXsvfInfo*);

/*============================================================================
 * XSVF Command Bytes
 ============================================================================*/

/* encodings of xsvf instructions */
#define XCOMPLETE        0
#define XTDOMASK         1
#define XSIR             2
#define XSDR             3
#define XRUNTEST         4
/* Reserved              5 */
/* Reserved              6 */
#define XREPEAT          7
#define XSDRSIZE         8
#define XSDRTDO          9
#define XSETSDRMASKS     10
#define XSDRINC          11
#define XSDRB            12
#define XSDRC            13
#define XSDRE            14
#define XSDRTDOB         15
#define XSDRTDOC         16
#define XSDRTDOE         17
#define XSTATE           18         /* 4.00 */
#define XENDIR           19         /* 4.04 */
#define XENDDR           20         /* 4.04 */
#define XSIR2            21         /* 4.10 */
#define XCOMMENT         22         /* 4.14 */
#define XWAIT            23         /* 5.00 */
/* Insert new commands here */
/* and add corresponding xsvfDoCmd function to s_xsvf_pfDoCmd below. */
#define XLASTCMD         24         /* Last command marker */


/*============================================================================
 * XSVF Global Variables
 ============================================================================*/

/* Array of XSVF command functions.  Must follow command byte value order! */
/* If your compiler cannot take this form, then convert to a switch statement*/
static const TXsvfDoCmdFuncPtr   s_xsvf_pfDoCmd[]  =
{
   &CXsvfFilePlayer::xsvfDoXCOMPLETE,        /*  0 */
   &CXsvfFilePlayer::xsvfDoXTDOMASK,         /*  1 */
   &CXsvfFilePlayer::xsvfDoXSIR,             /*  2 */
   &CXsvfFilePlayer::xsvfDoXSDR,             /*  3 */
   &CXsvfFilePlayer::xsvfDoXRUNTEST,         /*  4 */
   &CXsvfFilePlayer::xsvfDoIllegalCmd,       /*  5 */
   &CXsvfFilePlayer::xsvfDoIllegalCmd,       /*  6 */
   &CXsvfFilePlayer::xsvfDoXREPEAT,          /*  7 */
   &CXsvfFilePlayer::xsvfDoXSDRSIZE,         /*  8 */
   &CXsvfFilePlayer::xsvfDoXSDRTDO,          /*  9 */
#ifdef  XSVF_SUPPORT_COMPRESSION
   &CXsvfFilePlayer::xsvfDoXSETSDRMASKS,     /* 10 */
   &CXsvfFilePlayer::xsvfDoXSDRINC,          /* 11 */
#else
   &CXsvfFilePlayer::xsvfDoIllegalCmd,       /* 10 */
   &CXsvfFilePlayer::xsvfDoIllegalCmd,       /* 11 */
#endif  /* XSVF_SUPPORT_COMPRESSION */
   &CXsvfFilePlayer::xsvfDoXSDRBCE,          /* 12 */
   &CXsvfFilePlayer::xsvfDoXSDRBCE,          /* 13 */
   &CXsvfFilePlayer::xsvfDoXSDRBCE,          /* 14 */
   &CXsvfFilePlayer::xsvfDoXSDRTDOBCE,       /* 15 */
   &CXsvfFilePlayer::xsvfDoXSDRTDOBCE,       /* 16 */
   &CXsvfFilePlayer::xsvfDoXSDRTDOBCE,       /* 17 */
   &CXsvfFilePlayer::xsvfDoXSTATE,           /* 18 */
   &CXsvfFilePlayer::xsvfDoXENDXR,           /* 19 */
   &CXsvfFilePlayer::xsvfDoXENDXR,           /* 20 */
   &CXsvfFilePlayer::xsvfDoXSIR2,            /* 21 */
   &CXsvfFilePlayer::xsvfDoXCOMMENT,         /* 22 */
   &CXsvfFilePlayer::xsvfDoXWAIT             /* 23 */
      /* Insert new command functions here */
};

#ifdef  DEBUG_MODE

static const char* xsvf_pzCommandName[]  =
{
   "XCOMPLETE",
   "XTDOMASK",
   "XSIR",
   "XSDR",
   "XRUNTEST",
   "Reserved5",
   "Reserved6",
   "XREPEAT",
   "XSDRSIZE",
   "XSDRTDO",
   "XSETSDRMASKS",
   "XSDRINC",
   "XSDRB",
   "XSDRC",
   "XSDRE",
   "XSDRTDOB",
   "XSDRTDOC",
   "XSDRTDOE",
   "XSTATE",
   "XENDIR",
   "XENDDR",
   "XSIR2",
   "XCOMMENT",
   "XWAIT"
};

static const char*   xsvf_pzErrorName[]  =
{
   "No error",
   "ERROR:  Unknown",
   "ERROR:  TDO mismatch",
   "ERROR:  TDO mismatch and exceeded max retries",
   "ERROR:  Unsupported XSVF command",
   "ERROR:  Illegal state specification",
   "ERROR:  Data overflows allocated LENVAL_MAX_LEN buffer size"
};

static const char*   xsvf_pzTapState[] =
{
   "RESET",        /* 0x00 */
   "RUNTEST/IDLE", /* 0x01 */
   "DRSELECT",     /* 0x02 */
   "DRCAPTURE",    /* 0x03 */
   "DRSHIFT",      /* 0x04 */
   "DREXIT1",      /* 0x05 */
   "DRPAUSE",      /* 0x06 */
   "DREXIT2",      /* 0x07 */
   "DRUPDATE",     /* 0x08 */
   "IRSELECT",     /* 0x09 */
   "IRCAPTURE",    /* 0x0A */
   "IRSHIFT",      /* 0x0B */
   "IREXIT1",      /* 0x0C */
   "IRPAUSE",      /* 0x0D */
   "IREXIT2",      /* 0x0E */
   "IRUPDATE"      /* 0x0F */
};

#endif  /* DEBUG_MODE */

/*============================================================================
 * Utility Functions
 ============================================================================*/

/*****************************************************************************
 * Function:     xsvfPrintLenVal
 * Description:  Print the lenval value in hex.
 * Parameters:   plv     - ptr to lenval.
 * Returns:      void.
 *****************************************************************************/
#ifdef DEBUG_MODE
void xsvfPrintLenVal(lenVal *plv)
{
   int i;

   if (plv)
   {
      printf("0x");
      for (i = 0; i < plv->len; ++i)
      {
         printf("%02x", ((UINT)(plv->val[ i ])));
      }
   }
}
#endif


/*****************************************************************************
 * Function:     xsvfInfoInit
 * Description:  Initialize the xsvfInfo data.
 * Parameters:   pXsvfInfo   - ptr to the XSVF info structure.
 * Returns:      int         - 0 = success; otherwise error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfInfoInit(SXsvfInfo* pXsvfInfo)
{
   XSVFDBG_PRINTF1(4, "    sizeof(SXsvfInfo) = %d bytes\n", sizeof(SXsvfInfo));

   pXsvfInfo->ucComplete       = 0;
   pXsvfInfo->ucCommand        = XCOMPLETE;
   pXsvfInfo->lCommandCount    = 0;
   pXsvfInfo->iErrorCode       = XSVF_ERROR_NONE;
   pXsvfInfo->ucMaxRepeat      = 0;
   pXsvfInfo->ucTapState       = XTAPSTATE_RESET;
   pXsvfInfo->ucEndIR          = XTAPSTATE_RUNTEST;
   pXsvfInfo->ucEndDR          = XTAPSTATE_RUNTEST;
   pXsvfInfo->lShiftLengthBits = 0;
   pXsvfInfo->sShiftLengthBytes= 0;
   pXsvfInfo->lRunTestTime     = 0;
   pXsvfInfo->iHir             = 0;
   pXsvfInfo->iTir             = 0;
   pXsvfInfo->iHdr             = 0;
   pXsvfInfo->iTdr             = 0;
   pXsvfInfo->iHdrFpga         = 0;

   return(0);
}

/*****************************************************************************
 * Function:     xsvfInfoCleanup
 * Description:  Cleanup the xsvfInfo data.
 * Parameters:   pXsvfInfo   - ptr to the XSVF info structure.
 * Returns:      void.
 *****************************************************************************/
void CXsvfFilePlayer::xsvfInfoCleanup(SXsvfInfo* pXsvfInfo)
{
}

/*****************************************************************************
 * Function:     xsvfGetAsNumBytes
 * Description:  Calculate the number of bytes the given number of bits
 *               consumes.
 * Parameters:   lNumBits    - the number of bits.
 * Returns:      short       - the number of bytes to store the number of bits.
 *****************************************************************************/
short CXsvfFilePlayer::xsvfGetAsNumBytes (long lNumBits)
{
   return (short)((lNumBits + 7) >> 3);
}

/*****************************************************************************
 * Function:     xsvfTmsTransition
 * Description:  Apply TMS and transition TAP controller by applying one TCK
 *               cycle.
 * Parameters:   sTms    - new TMS value.
 * Returns:      void.
 *****************************************************************************/
void CXsvfFilePlayer::xsvfTmsTransition(short sTms)
{
   setPort(TMS, sTms);
   setPort(TCK, 0);
   setPort(TCK, 1);
}

/*****************************************************************************
 * Function:     xsvfGotoTapState
 * Description:  From the current TAP state, go to the named TAP state.
 *               A target state of RESET ALWAYS causes TMS reset sequence.
 *               All SVF standard stable state paths are supported.
 *               All state transitions are supported except for the following
 *               which cause an XSVF_ERROR_ILLEGALSTATE:
 *                   - Target==DREXIT2;  Start!=DRPAUSE
 *                   - Target==IREXIT2;  Start!=IRPAUSE
 * Parameters:   pucTapState     - Current TAP state; returns final TAP state.
 *               ucTargetState   - New target TAP state.
 * Returns:      int             - 0 = success; otherwise error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfGotoTapState (unsigned char *pucTapState, unsigned char ucTargetState)
{
   int i;
   int iErrorCode;

   iErrorCode  = XSVF_ERROR_NONE;
   if (ucTargetState == XTAPSTATE_RESET)
   {
      /* If RESET, always perform TMS reset sequence to reset/sync TAPs */
      xsvfTmsTransition(1);
      for (i = 0; i < 5; ++i)
         pulseClock();

      *pucTapState    = XTAPSTATE_RESET;
      XSVFDBG_PRINTF( 3, "   TMS Reset Sequence -> Test-Logic-Reset\n");
      XSVFDBG_PRINTF1(3, "   TAP State = %s\n",
                      xsvf_pzTapState[ *pucTapState ]);
   }
   else if ((ucTargetState != *pucTapState) &&
            (((ucTargetState == XTAPSTATE_EXIT2DR) && (*pucTapState != XTAPSTATE_PAUSEDR)) ||
             ((ucTargetState == XTAPSTATE_EXIT2IR) && (*pucTapState != XTAPSTATE_PAUSEIR))))
   {
      /* Trap illegal TAP state path specification */
      iErrorCode  = XSVF_ERROR_ILLEGALSTATE;
   }
   else
   {
      if (ucTargetState == *pucTapState)
      {
         /* Already in target state.  Do nothing except when in DRPAUSE
            or in IRPAUSE to comply with SVF standard */
         if (ucTargetState == XTAPSTATE_PAUSEDR)
         {
            xsvfTmsTransition(1);
            *pucTapState    = XTAPSTATE_EXIT2DR;
            XSVFDBG_PRINTF1(3, "   TAP State = %s\n",
                            xsvf_pzTapState[ *pucTapState ]);
         }
         else if (ucTargetState == XTAPSTATE_PAUSEIR)
         {
            xsvfTmsTransition(1);
            *pucTapState    = XTAPSTATE_EXIT2IR;
            XSVFDBG_PRINTF1(3, "   TAP State = %s\n",
                            xsvf_pzTapState[ *pucTapState ]);
         }
      }

      /* Perform TAP state transitions to get to the target state */
      while (ucTargetState != *pucTapState)
      {
         switch (*pucTapState)
         {
            case XTAPSTATE_RESET:
               xsvfTmsTransition(0);
               *pucTapState    = XTAPSTATE_RUNTEST;
               break;
            case XTAPSTATE_RUNTEST:
               xsvfTmsTransition(1);
               *pucTapState    = XTAPSTATE_SELECTDR;
               break;
            case XTAPSTATE_SELECTDR:
               if (ucTargetState >= XTAPSTATE_IRSTATES)
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_SELECTIR;
               }
               else
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_CAPTUREDR;
               }
               break;
            case XTAPSTATE_CAPTUREDR:
               if (ucTargetState == XTAPSTATE_SHIFTDR)
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_SHIFTDR;
               }
               else
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_EXIT1DR;
               }
               break;
            case XTAPSTATE_SHIFTDR:
               xsvfTmsTransition(1);
               *pucTapState    = XTAPSTATE_EXIT1DR;
               break;
            case XTAPSTATE_EXIT1DR:
               if (ucTargetState == XTAPSTATE_PAUSEDR)
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_PAUSEDR;
               }
               else
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_UPDATEDR;
               }
               break;
            case XTAPSTATE_PAUSEDR:
               xsvfTmsTransition(1);
               *pucTapState    = XTAPSTATE_EXIT2DR;
               break;
            case XTAPSTATE_EXIT2DR:
               if (ucTargetState == XTAPSTATE_SHIFTDR)
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_SHIFTDR;
               }
               else
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_UPDATEDR;
               }
               break;
            case XTAPSTATE_UPDATEDR:
               if (ucTargetState == XTAPSTATE_RUNTEST)
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_RUNTEST;
               }
               else
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_SELECTDR;
               }
               break;
            case XTAPSTATE_SELECTIR:
               xsvfTmsTransition(0);
               *pucTapState    = XTAPSTATE_CAPTUREIR;
               break;
            case XTAPSTATE_CAPTUREIR:
               if (ucTargetState == XTAPSTATE_SHIFTIR)
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_SHIFTIR;
               }
               else
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_EXIT1IR;
               }
               break;
            case XTAPSTATE_SHIFTIR:
               xsvfTmsTransition(1);
               *pucTapState    = XTAPSTATE_EXIT1IR;
               break;
            case XTAPSTATE_EXIT1IR:
               if (ucTargetState == XTAPSTATE_PAUSEIR)
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_PAUSEIR;
               }
               else
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_UPDATEIR;
               }
               break;
            case XTAPSTATE_PAUSEIR:
               xsvfTmsTransition(1);
               *pucTapState    = XTAPSTATE_EXIT2IR;
               break;
            case XTAPSTATE_EXIT2IR:
               if (ucTargetState == XTAPSTATE_SHIFTIR)
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_SHIFTIR;
               }
               else
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_UPDATEIR;
               }
               break;
            case XTAPSTATE_UPDATEIR:
               if (ucTargetState == XTAPSTATE_RUNTEST)
               {
                  xsvfTmsTransition(0);
                  *pucTapState    = XTAPSTATE_RUNTEST;
               }
               else
               {
                  xsvfTmsTransition(1);
                  *pucTapState    = XTAPSTATE_SELECTDR;
               }
               break;
            default:
               iErrorCode      = XSVF_ERROR_ILLEGALSTATE;
               *pucTapState    = ucTargetState;    /* Exit while loop */
               break;
         }
         XSVFDBG_PRINTF1(3, "   TAP State = %s\n",
                         xsvf_pzTapState[ *pucTapState ]);
      }
   }

   return(iErrorCode);
}

/***************************************************************************
 * Function:     xsvfShiftOnly
 * Description:  Assumes that starting TAP state is SHIFT-DR or SHIFT-IR.
 *               Shift the given TDI data into the JTAG scan chain.
 *               Optionally, save the TDO data shifted out of the scan chain.
 *               Last shift cycle is special:  capture last TDO, set last
 *               TDI, but does not pulse TCK.  Caller must pulse TCK and
 *               optionally set TMS=1 to exit shift state.
 * Parameters:  lNumBits        - number of bits to shift.
 *              plvTdi          - ptr to lenval for TDI data.
 *              plvTdoCaptured  - ptr to lenval for storing captured TDO data.
 *              iExitShift      - 1=exit at end of shift; 0=stay in Shift-DR.
 * Returns:     void.
 ***************************************************************************/
void CXsvfFilePlayer::xsvfShiftOnly (long lNumBits,
                                     lenVal *plvTdi,
                                     lenVal *plvTdoCaptured,
                                     int iExitShift)
{
   unsigned char*  pucTdi;
   unsigned char*  pucTdo;
   unsigned char   ucTdiByte;
   unsigned char   ucTdoByte;
   unsigned char   ucTdoBit;
   int             i;

   /* assert(((lNumBits + 7) / 8) == plvTdi->len); */

   /* Initialize TDO storage len == TDI len */
   pucTdo  = 0;
   if (plvTdoCaptured)
   {
      plvTdoCaptured->len = plvTdi->len;
      pucTdo              = plvTdoCaptured->val + plvTdi->len;
   }

   /* Shift LSB first.  val[N-1] == LSB.  val[0] == MSB. */
   pucTdi  = plvTdi->val + plvTdi->len;
   while (lNumBits)
   {
      /* Process on a byte-basis */
      ucTdiByte   = (*(--pucTdi));
      ucTdoByte   = 0;
      for (i = 0; (lNumBits && (i < 8)); ++i)
      {
         --lNumBits;
         if (iExitShift && !lNumBits)
         {
            /* Exit Shift-DR state */
            setPort(TMS, 1);
         }

         /* Set the new TDI value */
         setPort(TDI, (short)(ucTdiByte & 1));
         ucTdiByte   >>= 1;

         /* Set TCK low */
         setPort(TCK, 0);

         if (pucTdo)
         {
            /* Save the TDO value */
            ucTdoBit    = readTDOBit();
            ucTdoByte   |= (ucTdoBit << i);
         }

         /* Set TCK high */
         setPort(TCK, 1);
      }

      /* Save the TDO byte value */
      if (pucTdo)
      {
         (*(--pucTdo))   = ucTdoByte;
      }
   }
}

/*****************************************************************************
 * Function:     xsvfShift
 * Description:  Goes to the given starting TAP state.
 *               Calls xsvfShiftOnly to shift in the given TDI data and
 *               optionally capture the TDO data.
 *               Compares the TDO captured data against the TDO expected
 *               data.
 *               If a data mismatch occurs, then executes the exception
 *               handling loop upto ucMaxRepeat times.
 * Parameters:   pucTapState     - Ptr to current TAP state.
 *               ucStartState    - Starting shift state: Shift-DR or Shift-IR.
 *               lNumBits        - number of bits to shift.
 *               plvTdi          - ptr to lenval for TDI data.
 *               plvTdoCaptured  - ptr to lenval for storing TDO data.
 *               plvTdoExpected  - ptr to expected TDO data.
 *               plvTdoMask      - ptr to TDO mask.
 *               ucEndState      - state in which to end the shift.
 *               lRunTestTime    - amount of time to wait after the shift.
 *               ucMaxRepeat     - Maximum number of retries on TDO mismatch.
 *               iHeader         - Number of header bits to shift before data.
 *               iTrailer        - Number of trailer bits to shift after data.
 * Returns:      int             - 0 = success; otherwise TDO mismatch.
 * Notes:        XC9500XL-only Optimization:
 *               Skip the waitTime() if plvTdoMask->val[0:plvTdoMask->len-1]
 *               is NOT all zeros and sMatch==1.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfShift(unsigned char*   pucTapState,
                               unsigned char    ucStartState,
                               long             lNumBits,
                               lenVal*          plvTdi,
                               lenVal*          plvTdoCaptured,
                               lenVal*          plvTdoExpected,
                               lenVal*          plvTdoMask,
                               unsigned char    ucEndState,
                               long             lRunTestTime,
                               unsigned char    ucMaxRepeat,
                               int              iHeader,
                               int              iTrailer)
{
   int             iErrorCode;
   int             iMismatch;
   unsigned char   ucRepeat;
   int             iExitShift;
   int             iHeaderCycles;
   int             iTrailerCycles;

   iErrorCode  = XSVF_ERROR_NONE;
   iMismatch   = 0;
   ucRepeat    = 0;
   iExitShift  = (ucStartState != ucEndState);

   XSVFDBG_PRINTF1(3, "   Shift Length = %ld\n", lNumBits);
   XSVFDBG_PRINTF1(4, "    Header Length  = %d\n", iHeader);
   XSVFDBG_PRINTF1(4, "    Trailer Length = %d\n", iTrailer);
   XSVFDBG_PRINTF(4, "    TDI          = ");
   XSVFDBG_PRINTLENVAL(4, plvTdi);
   XSVFDBG_PRINTF(4, "\n");
   XSVFDBG_PRINTF(4, "    TDO Expected = ");
   XSVFDBG_PRINTLENVAL(4, plvTdoExpected);
   XSVFDBG_PRINTF(4, "\n");

   if (!lNumBits)
   {
      /* Compatibility with XSVF2.00:  XSDR 0 = no shift, but wait in RTI */
      if (lRunTestTime)
      {
         /* Wait for prespecified XRUNTEST time */
         xsvfGotoTapState(pucTapState, XTAPSTATE_RUNTEST);
         XSVFDBG_PRINTF1(3, "   Wait = %ld usec\n", lRunTestTime);
         waitTime(lRunTestTime);
      }
   }
   else
   {
      do
      {
         iHeaderCycles   = iHeader;
         iTrailerCycles  = iTrailer;

         /* Goto Shift-DR or Shift-IR */
         xsvfGotoTapState(pucTapState, ucStartState);

         if (iHeaderCycles)
         {
            setPort(TDI, (short)((ucStartState==XTAPSTATE_SHIFTIR)?1:0));
            while (iHeaderCycles--)
            {
               pulseClock();
            }
         }

         /* Shift TDI and capture TDO */
         xsvfShiftOnly(lNumBits, plvTdi, plvTdoCaptured,
                       (iExitShift && !iTrailerCycles));

         if (plvTdoExpected)
         {
            /* Compare TDO data to expected TDO data */
            iMismatch   = !EqualLenVal(plvTdoExpected,
                                       plvTdoCaptured,
                                       plvTdoMask);
         }

         if (iExitShift)
         {
            if (iTrailerCycles)
            {
               setPort(TDI,(short)((ucStartState==XTAPSTATE_SHIFTIR)?1:0));
               while (iTrailerCycles--)
               {
                  if (iTrailerCycles == 0)
                  {
                     setPort(TMS, 1);
                  }
                  pulseClock();
               }
            }

            /* Update TAP state:  Shift->Exit */
            ++(*pucTapState);
            XSVFDBG_PRINTF1(3, "   TAP State = %s\n",
                            xsvf_pzTapState[ *pucTapState ]);

            if (iMismatch && lRunTestTime && (ucRepeat < ucMaxRepeat))
            {
               XSVFDBG_PRINTF(4, "    TDO Expected = ");
               XSVFDBG_PRINTLENVAL(4, plvTdoExpected);
               XSVFDBG_PRINTF(4, "\n");
               XSVFDBG_PRINTF(4, "    TDO Captured = ");
               XSVFDBG_PRINTLENVAL(4, plvTdoCaptured);
               XSVFDBG_PRINTF(4, "\n");
               XSVFDBG_PRINTF(4, "    TDO Mask     = ");
               XSVFDBG_PRINTLENVAL(4, plvTdoMask);
               XSVFDBG_PRINTF(4, "\n");
               XSVFDBG_PRINTF1(3, "   Retry #%d\n", (ucRepeat + 1));
               /* Do exception handling retry - ShiftDR only */
               xsvfGotoTapState(pucTapState, XTAPSTATE_PAUSEDR);
               /* Shift 1 extra bit */
               xsvfGotoTapState(pucTapState, XTAPSTATE_SHIFTDR);
               /* Increment RUNTEST time by an additional 25% */
               lRunTestTime    += (lRunTestTime >> 2);
            }
            else
            {
               /* Do normal exit from Shift-XR */
               xsvfGotoTapState(pucTapState, ucEndState);
            }

            if (lRunTestTime)
            {
               /* Wait for prespecified XRUNTEST time */
               xsvfGotoTapState(pucTapState, XTAPSTATE_RUNTEST);
               XSVFDBG_PRINTF1(3, "   Wait = %ld usec\n", lRunTestTime);
               waitTime(lRunTestTime);
            }
         }
      } while (iMismatch && (ucRepeat++ < ucMaxRepeat));
   }

   if (iMismatch)
   {
      XSVFDBG_PRINTF(1, " TDO Expected = ");
      XSVFDBG_PRINTLENVAL(1, plvTdoExpected);
      XSVFDBG_PRINTF(1, "\n");
      XSVFDBG_PRINTF(1, " TDO Captured = ");
      XSVFDBG_PRINTLENVAL(1, plvTdoCaptured);
      XSVFDBG_PRINTF(1, "\n");
      XSVFDBG_PRINTF(1, " TDO Mask     = ");
      XSVFDBG_PRINTLENVAL(1, plvTdoMask);
      XSVFDBG_PRINTF(1, "\n");
      if (ucMaxRepeat && (ucRepeat > ucMaxRepeat))
      {
         iErrorCode  = XSVF_ERROR_MAXRETRIES;
      }
      else
      {
         iErrorCode  = XSVF_ERROR_TDOMISMATCH;
      }
   }

   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfBasicXSDRTDO
 * Description:  Get the XSDRTDO parameters and execute the XSDRTDO command.
 *               This is the common function for all XSDRTDO commands.
 * Parameters:   pucTapState         - Current TAP state.
 *               lShiftLengthBits    - number of bits to shift.
 *               sShiftLengthBytes   - number of bytes to read.
 *               plvTdi              - ptr to lenval for TDI data.
 *               lvTdoCaptured       - ptr to lenval for storing TDO data.
 *               iEndState           - state in which to end the shift.
 *               lRunTestTime        - amount of time to wait after the shift.
 *               ucMaxRepeat         - maximum xc9500/xl retries.
 *               iHeader             - Num. of header bits to shift before data.
 *               iTrailer            - Num. of trailer bits to shift after data.
 * Returns:      int                 - 0 = success; otherwise TDO mismatch.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfBasicXSDRTDO(unsigned char*    pucTapState,
                                      long              lShiftLengthBits,
                                      short             sShiftLengthBytes,
                                      lenVal*           plvTdi,
                                      lenVal*           plvTdoCaptured,
                                      lenVal*           plvTdoExpected,
                                      lenVal*           plvTdoMask,
                                      unsigned char     ucEndState,
                                      long              lRunTestTime,
                                      unsigned char     ucMaxRepeat,
                                      int               iHeader,
                                      int               iTrailer)
{
   readVal(plvTdi, sShiftLengthBytes);
   if (plvTdoExpected)
   {
      readVal(plvTdoExpected, sShiftLengthBytes);
   }
   return(xsvfShift(pucTapState, XTAPSTATE_SHIFTDR, lShiftLengthBits,
                    plvTdi, plvTdoCaptured, plvTdoExpected, plvTdoMask,
                    ucEndState, lRunTestTime, ucMaxRepeat,
                    iHeader, iTrailer));
}

/*****************************************************************************
 * Function:     xsvfDoSDRMasking
 * Description:  Update the data value with the next XSDRINC data and address.
 * Example:      dataVal=0x01ff, nextData=0xab, addressMask=0x0100,
 *               dataMask=0x00ff, should set dataVal to 0x02ab
 * Parameters:   plvTdi          - The current TDI value.
 *               plvNextData     - the next data value.
 *               plvAddressMask  - the address mask.
 *               plvDataMask     - the data mask.
 * Returns:      void.
 *****************************************************************************/
#ifdef  XSVF_SUPPORT_COMPRESSION
void CXsvfFilePlayer::xsvfDoSDRMasking (lenVal*  plvTdi,
                                        lenVal*  plvNextData,
                                        lenVal*  plvAddressMask,
                                        lenVal*  plvDataMask)
{
   int             i;
   unsigned char   ucTdi;
   unsigned char   ucTdiMask;
   unsigned char   ucDataMask;
   unsigned char   ucNextData;
   unsigned char   ucNextMask;
   short           sNextData;

   /* add the address Mask to dataVal and return as a new dataVal */
   addVal(plvTdi, plvTdi, plvAddressMask);

   ucNextData  = 0;
   ucNextMask  = 0;
   sNextData   = plvNextData->len;
   for (i = plvDataMask->len - 1; i >= 0; --i)
   {
      /* Go through data mask in reverse order looking for mask (1) bits */
      ucDataMask  = plvDataMask->val[ i ];
      if (ucDataMask)
      {
         /* Retrieve the corresponding TDI byte value */
         ucTdi       = plvTdi->val[ i ];

         /* For each bit in the data mask byte, look for 1's */
         ucTdiMask   = 1;
         while (ucDataMask)
         {
            if (ucDataMask & 1)
            {
               if (!ucNextMask)
               {
                  /* Get the next data byte */
                  ucNextData  = plvNextData->val[ --sNextData ];
                  ucNextMask  = 1;
               }

               /* Set or clear the data bit according to the next data */
               if (ucNextData & ucNextMask)
               {
                  ucTdi   |= ucTdiMask;       /* Set bit */
               }
               else
               {
                  ucTdi   &= (~ucTdiMask);  /* Clear bit */
               }

               /* Update the next data */
               ucNextMask  <<= 1;
            }
            ucTdiMask   <<= 1;
            ucDataMask  >>= 1;
         }

         /* Update the TDI value */
         plvTdi->val[ i ]    = ucTdi;
      }
   }
}
#endif  /* XSVF_SUPPORT_COMPRESSION */

/*============================================================================
 * XSVF Command Functions (type = TXsvfDoCmdFuncPtr)
 * These functions update pXsvfInfo->iErrorCode only on an error.
 * Otherwise, the error code is left alone.
 * The function returns the error code from the function.
 ============================================================================*/

/*****************************************************************************
 * Function:     xsvfDoIllegalCmd
 * Description:  Function place holder for illegal/unsupported commands.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoIllegalCmd(SXsvfInfo* pXsvfInfo)
{
   XSVFDBG_PRINTF2(0, "ERROR:  Encountered unsupported command #%d (%s)\n",
                   ((UINT)(pXsvfInfo->ucCommand)),
                   ((pXsvfInfo->ucCommand < XLASTCMD)
                    ? (xsvf_pzCommandName[pXsvfInfo->ucCommand])
                    : "Unknown"));
   pXsvfInfo->iErrorCode   = XSVF_ERROR_ILLEGALCMD;
   return(pXsvfInfo->iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXCOMPLETE
 * Description:  XCOMPLETE (no parameters)
 *               Update complete status for XSVF player.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXCOMPLETE(SXsvfInfo* pXsvfInfo)
{
   pXsvfInfo->ucComplete   = 1;
   return(XSVF_ERROR_NONE);
}

/*****************************************************************************
 * Function:     xsvfDoXTDOMASK
 * Description:  XTDOMASK <lenVal.TdoMask[XSDRSIZE]>
 *               Prespecify the TDO compare mask.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXTDOMASK(SXsvfInfo* pXsvfInfo)
{
   readVal(&(pXsvfInfo->lvTdoMask), pXsvfInfo->sShiftLengthBytes);
   XSVFDBG_PRINTF(4, "    TDO Mask     = ");
   XSVFDBG_PRINTLENVAL(4, &(pXsvfInfo->lvTdoMask));
   XSVFDBG_PRINTF(4, "\n");
   return(XSVF_ERROR_NONE);
}

/*****************************************************************************
 * Function:     xsvfDoXSIR
 * Description:  XSIR <(byte)shiftlen> <lenVal.TDI[shiftlen]>
 *               Get the instruction and shift the instruction into the TAP.
 *               If prespecified XRUNTEST!=0, goto RUNTEST and wait after
 *               the shift for XRUNTEST usec.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXSIR(SXsvfInfo* pXsvfInfo)
{
   unsigned char   ucShiftIrBits;
   short           sShiftIrBytes;
   int             iErrorCode;

   /* Get the shift length and store */
   readByte(&ucShiftIrBits);
   sShiftIrBytes   = xsvfGetAsNumBytes(ucShiftIrBits);
   XSVFDBG_PRINTF1(3, "   XSIR length = %d\n",
                   ((UINT)ucShiftIrBits));

   if (sShiftIrBytes > LENVAL_MAX_LEN)
   {
      iErrorCode  = XSVF_ERROR_DATAOVERFLOW;
   }
   else
   {
      /* Get and store instruction to shift in */
      readVal(&(pXsvfInfo->lvTdi), xsvfGetAsNumBytes(ucShiftIrBits));

      /* Shift the data */
      iErrorCode  = xsvfShift(&(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTIR,
                              ucShiftIrBits, &(pXsvfInfo->lvTdi),
                              /*plvTdoCaptured*/0, /*plvTdoExpected*/0,
                              /*plvTdoMask*/0, pXsvfInfo->ucEndIR,
                              pXsvfInfo->lRunTestTime, /*ucMaxRepeat*/0,
                              pXsvfInfo->iHir, pXsvfInfo->iTir);
   }

   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXSIR2
 * Description:  XSIR <(2-byte)shiftlen> <lenVal.TDI[shiftlen]>
 *               Get the instruction and shift the instruction into the TAP.
 *               If prespecified XRUNTEST!=0, goto RUNTEST and wait after
 *               the shift for XRUNTEST usec.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXSIR2(SXsvfInfo* pXsvfInfo)
{
   long            lShiftIrBits;
   short           sShiftIrBytes;
   int             iErrorCode;

   /* Get the shift length and store */
   readVal(&(pXsvfInfo->lvTdi), 2);
   lShiftIrBits    = value(&(pXsvfInfo->lvTdi));
   sShiftIrBytes   = xsvfGetAsNumBytes(lShiftIrBits);
   XSVFDBG_PRINTF1(3, "   XSIR2 length = %d\n", lShiftIrBits);

   if (sShiftIrBytes > LENVAL_MAX_LEN)
   {
      iErrorCode  = XSVF_ERROR_DATAOVERFLOW;
   }
   else
   {
      /* Get and store instruction to shift in */
      readVal(&(pXsvfInfo->lvTdi), xsvfGetAsNumBytes(lShiftIrBits));

      /* Shift the data */
      iErrorCode  = xsvfShift(&(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTIR,
                              lShiftIrBits, &(pXsvfInfo->lvTdi),
                              /*plvTdoCaptured*/0, /*plvTdoExpected*/0,
                              /*plvTdoMask*/0, pXsvfInfo->ucEndIR,
                              pXsvfInfo->lRunTestTime, /*ucMaxRepeat*/0,
                              pXsvfInfo->iHir, pXsvfInfo->iTir);
   }

   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXSDR
 * Description:  XSDR <lenVal.TDI[XSDRSIZE]>
 *               Shift the given TDI data into the JTAG scan chain.
 *               Compare the captured TDO with the expected TDO from the
 *               previous XSDRTDO command using the previously specified
 *               XTDOMASK.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXSDR(SXsvfInfo* pXsvfInfo)
{
   int iErrorCode;
   readVal(&(pXsvfInfo->lvTdi), pXsvfInfo->sShiftLengthBytes);
   /* use TDOExpected from last XSDRTDO instruction */
   iErrorCode  = xsvfShift(&(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTDR,
                           pXsvfInfo->lShiftLengthBits, &(pXsvfInfo->lvTdi),
                           &(pXsvfInfo->lvTdoCaptured),
                           &(pXsvfInfo->lvTdoExpected),
                           &(pXsvfInfo->lvTdoMask), pXsvfInfo->ucEndDR,
                           pXsvfInfo->lRunTestTime, pXsvfInfo->ucMaxRepeat,
                           pXsvfInfo->iHdr, pXsvfInfo->iTdr);
   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXRUNTEST
 * Description:  XRUNTEST <uint32>
 *               Prespecify the XRUNTEST wait time for shift operations.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXRUNTEST(SXsvfInfo* pXsvfInfo)
{
   readVal(&(pXsvfInfo->lvTdi), 4);
   pXsvfInfo->lRunTestTime = value(&(pXsvfInfo->lvTdi));
   OnRunTest(pXsvfInfo->lRunTestTime);
   XSVFDBG_PRINTF1(3, "   XRUNTEST = %ld\n", pXsvfInfo->lRunTestTime);
   return(XSVF_ERROR_NONE);
}

/*****************************************************************************
 * Function:     xsvfDoXREPEAT
 * Description:  XREPEAT <byte>
 *               Prespecify the maximum number of XC9500/XL retries.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXREPEAT(SXsvfInfo* pXsvfInfo)
{
   readByte(&(pXsvfInfo->ucMaxRepeat));
   XSVFDBG_PRINTF1(3, "   XREPEAT = %d\n",
                   ((UINT)(pXsvfInfo->ucMaxRepeat)));
   return(XSVF_ERROR_NONE);
}

/*****************************************************************************
 * Function:     xsvfDoXSDRSIZE
 * Description:  XSDRSIZE <uint32>
 *               Prespecify the XRUNTEST wait time for shift operations.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXSDRSIZE(SXsvfInfo* pXsvfInfo)
{
   int iErrorCode;
   iErrorCode  = XSVF_ERROR_NONE;
   readVal(&(pXsvfInfo->lvTdi), 4);
   pXsvfInfo->lShiftLengthBits = value(&(pXsvfInfo->lvTdi));
   pXsvfInfo->sShiftLengthBytes= xsvfGetAsNumBytes(pXsvfInfo->lShiftLengthBits);
   XSVFDBG_PRINTF1(3, "   XSDRSIZE = %ld\n", pXsvfInfo->lShiftLengthBits);
   if (pXsvfInfo->sShiftLengthBytes > LENVAL_MAX_LEN)
   {
      iErrorCode  = XSVF_ERROR_DATAOVERFLOW;
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXSDRTDO
 * Description:  XSDRTDO <lenVal.TDI[XSDRSIZE]> <lenVal.TDO[XSDRSIZE]>
 *               Get the TDI and expected TDO values.  Then, shift.
 *               Compare the expected TDO with the captured TDO using the
 *               prespecified XTDOMASK.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXSDRTDO(SXsvfInfo* pXsvfInfo)
{
   int iErrorCode;
   iErrorCode  = xsvfBasicXSDRTDO(&(pXsvfInfo->ucTapState),
                                  pXsvfInfo->lShiftLengthBits,
                                  pXsvfInfo->sShiftLengthBytes,
                                  &(pXsvfInfo->lvTdi),
                                  &(pXsvfInfo->lvTdoCaptured),
                                  &(pXsvfInfo->lvTdoExpected),
                                  &(pXsvfInfo->lvTdoMask),
                                  pXsvfInfo->ucEndDR,
                                  pXsvfInfo->lRunTestTime,
                                  pXsvfInfo->ucMaxRepeat,
                                  pXsvfInfo->iHdr, pXsvfInfo->iTdr);
   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXSETSDRMASKS
 * Description:  XSETSDRMASKS <lenVal.AddressMask[XSDRSIZE]>
 *                            <lenVal.DataMask[XSDRSIZE]>
 *               Get the prespecified address and data mask for the XSDRINC
 *               command.
 *               Used for xc9500/xl compressed XSVF data.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
#ifdef  XSVF_SUPPORT_COMPRESSION
int CXsvfFilePlayer::xsvfDoXSETSDRMASKS(SXsvfInfo* pXsvfInfo)
{
   /* read the addressMask */
   readVal(&(pXsvfInfo->lvAddressMask), pXsvfInfo->sShiftLengthBytes);
   /* read the dataMask    */
   readVal(&(pXsvfInfo->lvDataMask), pXsvfInfo->sShiftLengthBytes);

   XSVFDBG_PRINTF(4, "    Address Mask = ");
   XSVFDBG_PRINTLENVAL(4, &(pXsvfInfo->lvAddressMask));
   XSVFDBG_PRINTF(4, "\n");
   XSVFDBG_PRINTF(4, "    Data Mask    = ");
   XSVFDBG_PRINTLENVAL(4, &(pXsvfInfo->lvDataMask));
   XSVFDBG_PRINTF(4, "\n");

   return(XSVF_ERROR_NONE);
}
#endif  /* XSVF_SUPPORT_COMPRESSION */

/*****************************************************************************
 * Function:     xsvfDoXSDRINC
 * Description:  XSDRINC <lenVal.firstTDI[XSDRSIZE]> <byte(numTimes)>
 *                       <lenVal.data[XSETSDRMASKS.dataMask.len]> ...
 *               Get the XSDRINC parameters and execute the XSDRINC command.
 *               XSDRINC starts by loading the first TDI shift value.
 *               Then, for numTimes, XSDRINC gets the next piece of data,
 *               replaces the bits from the starting TDI as defined by the
 *               XSETSDRMASKS.dataMask, adds the address mask from
 *               XSETSDRMASKS.addressMask, shifts the new TDI value,
 *               and compares the TDO to the expected TDO from the previous
 *               XSDRTDO command using the XTDOMASK.
 *               Used for xc9500/xl compressed XSVF data.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
#ifdef  XSVF_SUPPORT_COMPRESSION
int CXsvfFilePlayer::xsvfDoXSDRINC(SXsvfInfo* pXsvfInfo)
{
   int             iErrorCode;
   int             iDataMaskLen;
   unsigned char   ucDataMask;
   unsigned char   ucNumTimes;
   unsigned char   i;

   readVal(&(pXsvfInfo->lvTdi), pXsvfInfo->sShiftLengthBytes);
   iErrorCode  = xsvfShift(&(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTDR,
                           pXsvfInfo->lShiftLengthBits,
                           &(pXsvfInfo->lvTdi), &(pXsvfInfo->lvTdoCaptured),
                           &(pXsvfInfo->lvTdoExpected),
                           &(pXsvfInfo->lvTdoMask), pXsvfInfo->ucEndDR,
                           pXsvfInfo->lRunTestTime, pXsvfInfo->ucMaxRepeat,
                           pXsvfInfo->iHdr, pXsvfInfo->iTdr);
   if (!iErrorCode)
   {
      /* Calculate number of data mask bits */
      iDataMaskLen    = 0;
      for (i = 0; i < pXsvfInfo->lvDataMask.len; ++i)
      {
         ucDataMask  = pXsvfInfo->lvDataMask.val[ i ];
         while (ucDataMask)
         {
            iDataMaskLen    += (ucDataMask & 1);
            ucDataMask      >>= 1;
         }
      }

      /* Get the number of data pieces, i.e. number of times to shift */
      readByte(&ucNumTimes);

      /* For numTimes, get data, fix TDI, and shift */
      for (i = 0; !iErrorCode && (i < ucNumTimes); ++i)
      {
         readVal(&(pXsvfInfo->lvNextData),
                 xsvfGetAsNumBytes(iDataMaskLen));
         xsvfDoSDRMasking(&(pXsvfInfo->lvTdi),
                          &(pXsvfInfo->lvNextData),
                          &(pXsvfInfo->lvAddressMask),
                          &(pXsvfInfo->lvDataMask));
         iErrorCode  = xsvfShift(&(pXsvfInfo->ucTapState),
                                 XTAPSTATE_SHIFTDR,
                                 pXsvfInfo->lShiftLengthBits,
                                 &(pXsvfInfo->lvTdi),
                                 &(pXsvfInfo->lvTdoCaptured),
                                 &(pXsvfInfo->lvTdoExpected),
                                 &(pXsvfInfo->lvTdoMask),
                                 pXsvfInfo->ucEndDR,
                                 pXsvfInfo->lRunTestTime,
                                 pXsvfInfo->ucMaxRepeat,
                                 pXsvfInfo->iHdr, pXsvfInfo->iTdr);
      }
   }
   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}
#endif  /* XSVF_SUPPORT_COMPRESSION */

/*****************************************************************************
 * Function:     xsvfDoXSDRBCE
 * Description:  XSDRB/XSDRC/XSDRE <lenVal.TDI[XSDRSIZE]>
 *               If not already in SHIFTDR, goto SHIFTDR.
 *               Shift the given TDI data into the JTAG scan chain.
 *               Ignore TDO.
 *               If cmd==XSDRE, then goto ENDDR.  Otherwise, stay in ShiftDR.
 *               XSDRB, XSDRC, and XSDRE are the same implementation.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXSDRBCE(SXsvfInfo* pXsvfInfo)
{
   unsigned char   ucEndDR;
   int             iErrorCode;
   ucEndDR = (unsigned char)((pXsvfInfo->ucCommand == XSDRE) ?
                             pXsvfInfo->ucEndDR : XTAPSTATE_SHIFTDR);
   iErrorCode  = xsvfBasicXSDRTDO(&(pXsvfInfo->ucTapState),
                                  pXsvfInfo->lShiftLengthBits,
                                  pXsvfInfo->sShiftLengthBytes,
                                  &(pXsvfInfo->lvTdi),
                                  /*plvTdoCaptured*/0, /*plvTdoExpected*/0,
                                  /*plvTdoMask*/0, ucEndDR,
                                  /*lRunTestTime*/0, /*ucMaxRepeat*/0,
                                  ((pXsvfInfo->ucTapState==XTAPSTATE_SHIFTDR)?
                                   0:(pXsvfInfo->iHdrFpga)),
                                  ((ucEndDR==XTAPSTATE_SHIFTDR)?
                                   0:(pXsvfInfo->iTdr)));
   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXSDRTDOBCE
 * Description:  XSDRB/XSDRC/XSDRE <lenVal.TDI[XSDRSIZE]> <lenVal.TDO[XSDRSIZE]>
 *               If not already in SHIFTDR, goto SHIFTDR.
 *               Shift the given TDI data into the JTAG scan chain.
 *               Compare TDO, but do NOT use XTDOMASK.
 *               If cmd==XSDRTDOE, then goto ENDDR.  Otherwise, stay in ShiftDR.
 *               XSDRTDOB, XSDRTDOC, and XSDRTDOE are the same implementation.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXSDRTDOBCE(SXsvfInfo* pXsvfInfo)
{
   unsigned char   ucEndDR;
   int             iErrorCode;
   ucEndDR = (unsigned char)((pXsvfInfo->ucCommand == XSDRTDOE) ?
                             pXsvfInfo->ucEndDR : XTAPSTATE_SHIFTDR);
   iErrorCode  = xsvfBasicXSDRTDO(&(pXsvfInfo->ucTapState),
                                  pXsvfInfo->lShiftLengthBits,
                                  pXsvfInfo->sShiftLengthBytes,
                                  &(pXsvfInfo->lvTdi),
                                  &(pXsvfInfo->lvTdoCaptured),
                                  &(pXsvfInfo->lvTdoExpected),
                                  /*plvTdoMask*/0, ucEndDR,
                                  /*lRunTestTime*/0, /*ucMaxRepeat*/0,
                                  ((pXsvfInfo->ucTapState==XTAPSTATE_SHIFTDR)?
                                   0:(pXsvfInfo->iHdrFpga)),
                                  ((ucEndDR==XTAPSTATE_SHIFTDR)?
                                   0:(pXsvfInfo->iTdr)));
   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXSTATE
 * Description:  XSTATE <byte>
 *               <byte> == XTAPSTATE;
 *               Get the state parameter and transition the TAP to that state.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXSTATE(SXsvfInfo* pXsvfInfo)
{
   unsigned char   ucNextState;
   int             iErrorCode;
   readByte(&ucNextState);
   iErrorCode  = xsvfGotoTapState(&(pXsvfInfo->ucTapState), ucNextState);
   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXENDXR
 * Description:  XENDIR/XENDDR <byte>
 *               <byte>:  0 = RUNTEST;  1 = PAUSE.
 *               Get the prespecified XENDIR or XENDDR.
 *               Both XENDIR and XENDDR use the same implementation.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXENDXR(SXsvfInfo* pXsvfInfo)
{
   int             iErrorCode;
   unsigned char   ucEndState;

   iErrorCode  = XSVF_ERROR_NONE;
   readByte(&ucEndState);
   if ((ucEndState != XENDXR_RUNTEST) && (ucEndState != XENDXR_PAUSE))
   {
      iErrorCode  = XSVF_ERROR_ILLEGALSTATE;
   }
   else
   {

      if (pXsvfInfo->ucCommand == XENDIR)
      {
         if (ucEndState == XENDXR_RUNTEST)
         {
            pXsvfInfo->ucEndIR  = XTAPSTATE_RUNTEST;
         }
         else
         {
            pXsvfInfo->ucEndIR  = XTAPSTATE_PAUSEIR;
         }
         XSVFDBG_PRINTF1(3, "   ENDIR State = %s\n",
                         xsvf_pzTapState[ pXsvfInfo->ucEndIR ]);
      }
      else    /* XENDDR */
      {
         if (ucEndState == XENDXR_RUNTEST)
         {
            pXsvfInfo->ucEndDR  = XTAPSTATE_RUNTEST;
         }
         else
         {
            pXsvfInfo->ucEndDR  = XTAPSTATE_PAUSEDR;
         }
         XSVFDBG_PRINTF1(3, "   ENDDR State = %s\n",
                         xsvf_pzTapState[ pXsvfInfo->ucEndDR ]);
      }
   }

   if (iErrorCode != XSVF_ERROR_NONE)
   {
      pXsvfInfo->iErrorCode   = iErrorCode;
   }
   return(iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXCOMMENT
 * Description:  XCOMMENT <text string ending in \0>
 *               <text string ending in \0> == text comment;
 *               Arbitrary comment embedded in the XSVF.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXCOMMENT(SXsvfInfo* pXsvfInfo)
{
   /* Use the comment for debugging */
   /* Otherwise, read through the comment to the end '\0' and ignore */
   unsigned char   ucText;

#ifdef DEBUG_MODE
   if (m_xsvf_iDebugLevel > 0)
   {
      putchar(' ');
   }
#endif

   do
   {
      readByte(&ucText);
#ifdef DEBUG_MODE
      if (m_xsvf_iDebugLevel > 0)
         putchar(ucText ? ucText : '\n');
#endif
   } while (ucText);

   pXsvfInfo->iErrorCode   = XSVF_ERROR_NONE;

   return(pXsvfInfo->iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfDoXWAIT
 * Description:  XWAIT <wait_state> <end_state> <wait_time>
 *               If not already in <wait_state>, then go to <wait_state>.
 *               Wait in <wait_state> for <wait_time> microseconds.
 *               Finally, if not already in <end_state>, then goto <end_state>.
 * Parameters:   pXsvfInfo   - XSVF information pointer.
 * Returns:      int         - 0 = success;  non-zero = error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfDoXWAIT(SXsvfInfo* pXsvfInfo)
{
   unsigned char   ucWaitState;
   unsigned char   ucEndState;
   long            lWaitTime;

   /* Get Parameters */
   /* <wait_state> */
   readVal(&(pXsvfInfo->lvTdi), 1);
   ucWaitState = pXsvfInfo->lvTdi.val[0];

   /* <end_state> */
   readVal(&(pXsvfInfo->lvTdi), 1);
   ucEndState = pXsvfInfo->lvTdi.val[0];

   /* <wait_time> */
   readVal(&(pXsvfInfo->lvTdi), 4);
   lWaitTime = value(&(pXsvfInfo->lvTdi));
   XSVFDBG_PRINTF2(3, "   XWAIT:  state = %s; time = %ld\n",
                   xsvf_pzTapState[ ucWaitState ], lWaitTime);

   /* If not already in <wait_state>, go to <wait_state> */
   if (pXsvfInfo->ucTapState != ucWaitState)
   {
      xsvfGotoTapState(&(pXsvfInfo->ucTapState), ucWaitState);
   }

   /* Wait for <wait_time> microseconds */
   waitTime(lWaitTime);

   /* If not already in <end_state>, go to <end_state> */
   if (pXsvfInfo->ucTapState != ucEndState)
   {
      xsvfGotoTapState(&(pXsvfInfo->ucTapState), ucEndState);
   }

   return(XSVF_ERROR_NONE);
}


/*============================================================================
 * Execution Control Functions
 ============================================================================*/

/*****************************************************************************
 * Function:     xsvfInitialize
 * Description:  Initialize the xsvf player.
 *               Call this before running the player to initialize the data
 *               in the SXsvfInfo struct.
 *               xsvfCleanup is called to clean up the data in SXsvfInfo
 *               after the XSVF is played.
 * Parameters:   pXsvfInfo   - ptr to the XSVF information.
 * Returns:      int - 0 = success; otherwise error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfInitialize(SXsvfInfo* pXsvfInfo)
{
   /* Initialize values */
   pXsvfInfo->iErrorCode   = xsvfInfoInit(pXsvfInfo);

   if (!pXsvfInfo->iErrorCode)
   {
      /* Initialize the TAPs */
      pXsvfInfo->iErrorCode   = xsvfGotoTapState(&(pXsvfInfo->ucTapState),
                                                 XTAPSTATE_RESET);
   }

   return(pXsvfInfo->iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfRun
 * Description:  Run the xsvf player for a single command and return.
 *               First, call xsvfInitialize.
 *               Then, repeatedly call this function until an error is detected
 *               or until the pXsvfInfo->ucComplete variable is non-zero.
 *               Finally, call xsvfCleanup to cleanup any remnants.
 * Parameters:   pXsvfInfo   - ptr to the XSVF information.
 * Returns:      int         - 0 = success; otherwise error.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfRun(SXsvfInfo* pXsvfInfo)
{
   /* Process the XSVF commands */
   if ((!pXsvfInfo->iErrorCode) && (!pXsvfInfo->ucComplete))
   {
      /* read 1 byte for the instruction */
      readByte(&(pXsvfInfo->ucCommand));
      ++(pXsvfInfo->lCommandCount);

      if (pXsvfInfo->ucCommand < XLASTCMD)
      {
         /* Execute the command.  Func sets error code. */
         XSVFDBG_PRINTF1(2, "  %s\n",
                         xsvf_pzCommandName[pXsvfInfo->ucCommand]);
         /* If your compiler cannot take this form,
            then convert to a switch statement */
         (this->*s_xsvf_pfDoCmd[ pXsvfInfo->ucCommand ])(pXsvfInfo);
      }
      else
      {
         /* Illegal command value.  Func sets error code. */
         xsvfDoIllegalCmd(pXsvfInfo);
      }
   }

   return(pXsvfInfo->iErrorCode);
}

/*****************************************************************************
 * Function:     xsvfCleanup
 * Description:  cleanup remnants of the xsvf player.
 * Parameters:   pXsvfInfo   - ptr to the XSVF information.
 * Returns:      void.
 *****************************************************************************/
void CXsvfFilePlayer::xsvfCleanup(SXsvfInfo* pXsvfInfo)
{
   xsvfInfoCleanup(pXsvfInfo);
   m_fin.close();
}

/*============================================================================
 * xsvfExecute() - The primary entry point to the XSVF player
 ============================================================================*/

/*****************************************************************************
 * Function:     xsvfExecute
 * Description:  Process, interpret, and apply the XSVF commands.
 *               See port.c:readByte for source of XSVF data.
 *               xsvfExecute supports dynamic device target assignment.
 * Parameters:   iHir    - # bits to shift before instruction.
 *                       = Sum of IR lengths following target device.
 *               iTir    - # bits to shift after instruction.
 *                       = Sum of IR lengths prior to target device.
 *               iHdr    - # bits to shift before data.
 *                       = # of devices (BYPASS reg.) following target device.
 *               iTdr    - # bits to shift after data.
 *                       = # of devices (BYPASS reg.) prior to target device.
 *               iHdrFpga- # bits to shift before FPGA data.
 *                       = special 32-bit alignment.
 * Returns:      int - Legacy result values:  1 == success;  0 == failed.
 *****************************************************************************/
int CXsvfFilePlayer::xsvfExecute (const char* pPathname, int iHir, int iTir,
                                  int iHdr, int iTdr, int iHdrFpga)
{
   int lFileSize, lCurPos;
   int res;

   // Open input stream
   m_fin.open(pPathname, std::ios_base::in | std::ios_base::binary);
   if (m_fin.fail())
      return XSVF_ERRORCODE(XSVF_ERROR_FILEIO);

   // Determine source file size.
   m_fin.seekg(0, std::ios_base::end);
   lFileSize = static_cast<int>(m_fin.tellg());
   m_fin.seekg(0, std::ios_base::beg);

   SXsvfInfo   xsvfInfo;
   res = xsvfInitialize (&xsvfInfo);
   if (res != 0)
      return XSVF_ERRORCODE(res);

   xsvfInfo.iHir       = iHir;
   xsvfInfo.iTir       = iTir;
   xsvfInfo.iHdr       = iHdr;
   xsvfInfo.iTdr       = iTdr;
   xsvfInfo.iHdrFpga   = iHdrFpga;

   // Main loop
   while (!xsvfInfo.iErrorCode && (!xsvfInfo.ucComplete))
   {
      lCurPos = static_cast<int>(m_fin.tellg());
      xsvfProgress(lCurPos, lFileSize);
      xsvfRun(&xsvfInfo);
   }

   if (xsvfInfo.iErrorCode)
   {
      XSVFDBG_PRINTF1(0, "%s\n", xsvf_pzErrorName[
                      (xsvfInfo.iErrorCode < XSVF_ERROR_LAST)
                      ? xsvfInfo.iErrorCode : XSVF_ERROR_UNKNOWN ]);
      XSVFDBG_PRINTF2(0, "ERROR at or near XSVF command #%ld.  See line #%ld in the XSVF ASCII file.\n",
                      xsvfInfo.lCommandCount, xsvfInfo.lCommandCount);
#ifdef  DEBUG_MODE
      FILE *fp;

      fopen_s(&fp,"OutputXsvfError.txt","w");
      if(fp != NULL)
      {
         char buffer [150];
         sprintf_s(buffer,
                   "%s\n ERROR at or near XSVF command #%ld.  See line #%ld in the XSVF ASCII file.\n",
                   xsvf_pzErrorName[(xsvfInfo.iErrorCode < XSVF_ERROR_LAST) ? xsvfInfo.iErrorCode : XSVF_ERROR_UNKNOWN ],
                   xsvfInfo.lCommandCount,
                   xsvfInfo.lCommandCount);

         fputs (buffer ,fp);
         fclose (fp);
      }
#endif
   }

   xsvfCleanup(&xsvfInfo);

   return(XSVF_ERRORCODE(xsvfInfo.iErrorCode));
}

/// Read in a byte of XSVF data
void CXsvfFilePlayer::readByte (unsigned char *data)
{
   m_fin.get(reinterpret_cast<char&>(*data));
}

/// Wait at least the specified number of microsec
void CXsvfFilePlayer::waitTime (int microsec)
{
   SysSleep (1 + (microsec / 1000));
}

int CXsvfFilePlayer::GetVerbosity() const
{
   return m_xsvf_iDebugLevel;
}

void CXsvfFilePlayer::SetVerbosity (int val)
{
   m_xsvf_iDebugLevel = val;
}

void CXsvfFilePlayer::xsvfProgress (int cur, int total)
{
   // Do nothing. Override to handle this.
}

