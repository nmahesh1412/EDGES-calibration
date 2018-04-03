/** @file px14_xsvf_player.cpp
*/
#include "stdafx.h"
#include "px14_xsvf_player.h"
#include "px14_private.h"
#include "px14_jtag.h"

CMyXsvfPlayer::CMyXsvfPlayer(HPX14 hBrd)
: m_hBrd(hBrd), m_flags(PX14XSVFF__DEFAULT), m_callbackp(NULL),
   m_callback_ctx(0), m_nFileCur(1), m_nFileTotal(1)
{
}

CMyXsvfPlayer::~CMyXsvfPlayer()
{
}

void CMyXsvfPlayer::SetCallback (PX14_FW_UPLOAD_CALLBACK callbackp,
                                 void* callback_ctx)
{
   m_callback_ctx = callback_ctx;
   m_callbackp = callbackp;
}

void CMyXsvfPlayer::SetFileCountInfo (int file_cur, int file_total)
{
   m_nFileTotal = file_total;
   m_nFileCur = file_cur;
}

void CMyXsvfPlayer::SetFlags (unsigned flags)
{
   m_flags = flags;
   m_bNoFailTdoMasking = 0 != (m_flags & PX14XSVFF_VIRTUAL);
}

void CMyXsvfPlayer::setPort(short p, short port_val)
{
   unsigned mask, val;

   switch (p)
   {
      case TCK: mask = PX14JIO_TCK; val = port_val ? PX14JIO_TCK : 0; break;
      case TDI: mask = PX14JIO_TDI; val = port_val ? PX14JIO_TDI : 0; break;
      case TMS: mask = PX14JIO_TMS; val = port_val ? PX14JIO_TMS : 0; break;
      default: SIGASSERT(PX14_FALSE); return;
   }

   WriteJtagPX14(m_hBrd, val, mask, PX14JIOF__DEF);
}

unsigned char CMyXsvfPlayer::readTDOBit()
{
   unsigned data;
   ReadJtagPX14(m_hBrd, &data);
   return (data & PX14JIO_TDO) ? 1 : 0;
}

void CMyXsvfPlayer::pulseClock()
{
   WriteJtagPX14(m_hBrd, 0, 0, PX14JIOF_PULSE_TCK);
}

void CMyXsvfPlayer::waitTime (int microsec)
{
   bool bNotify;

   if (m_flags & PX14XSVFF_WAIT_IS_TICKS)
   {
      // We are most likely programming a Virtex II device. In this
      //  case microsec is not a delay value but a loop count for
      //  clock pulse operations.
      WriteJtagPX14(m_hBrd, microsec, 0, PX14JIOF_PULSE_TCK_LOOP);
   }
   else
   {
      // XC18V00/XCF00 EEPROMs require clock to be low during wait.
      if (m_flags & PX14XSVFF_LOW_CLOCK_ON_WAIT)
         setPort (TCK, 0);

      bNotify = (NULL != m_callbackp) && (microsec > 1000 * 1000);
      if (bNotify)
      {
         // We're going to have to wait for a bit, give caller a chance
         //  to update UI to reflect this.
         (*m_callbackp)(m_hBrd, m_callback_ctx, 0, 0, 0,
                        microsec / 1000);
      }

      if (!IsDeviceVirtualPX14(m_hBrd))
      {
         // Driver will put thread to sleep if delay greater than
         // 7 milliseconds, else it will stall the processor.
         DeviceRequest(m_hBrd, IOCTL_PX14_US_DELAY,
                       &microsec, sizeof(int));
      }

      if (bNotify)
         (*m_callbackp)(m_hBrd, m_callback_ctx, 0, 0, 0, 0);
   }
}

int CMyXsvfPlayer::xsvfInitialize (SXsvfInfo* pXsvfInfo)
{
   int res, bAccepted;

   // Make ourself (the owner of this board handle) the JTAG owner
   res = JtagSessionCtrlPX14(m_hBrd, 1, &bAccepted);
   if ((SIG_SUCCESS != res) || !bAccepted)
      return XSVF_ERROR_UNKNOWN;

   // Enable JTAG access
   WriteJtagPX14 (m_hBrd, PX14JIO_CTRL, PX14JIO_CTRL);

   return CXsvfFilePlayer::xsvfInitialize(pXsvfInfo);
}

void CMyXsvfPlayer::xsvfCleanup (SXsvfInfo* pXsvfInfo)
{
   CXsvfFilePlayer::xsvfCleanup(pXsvfInfo);

   // Disable JTAG port and release JTAG owner status
   WriteJtagPX14 (m_hBrd, PX14JIO_TCK, PX14JIO_TCK);	// Keep clock high.
   WriteJtagPX14 (m_hBrd, 0, PX14JIO_CTRL);
   JtagSessionCtrlPX14(m_hBrd, 0, NULL);
}

void CMyXsvfPlayer::xsvfProgress (int cur, int total)
{
   if (m_callbackp)
   {
      (*m_callbackp)(m_hBrd, m_callback_ctx, cur, total,
                     m_nFileCur, m_nFileTotal);
   }
}


void CMyXsvfPlayer::xsvfTmsTransition (short sTms)
{
   WriteJtagPX14(m_hBrd, sTms ? PX14JIO_TMS : 0,
                 PX14JIO_TMS, PX14JIOF_PULSE_TCK);
}

void CMyXsvfPlayer::xsvfShiftOnly (long lNumBits,
                                   lenVal *plvTdi,
                                   lenVal *plvTdoCaptured,
                                   int iExitShift)
{
   unsigned flags;
   unsigned char *pucTdo;

   flags = PX14JSS_WRITE_TDI;
   if (iExitShift)
      flags |= PX14JSS_EXIT_SHIFT;
   if (plvTdoCaptured)
   {
      plvTdoCaptured->len = plvTdi->len;
      flags |= PX14JSS_READ_TDO;
      pucTdo = plvTdoCaptured->val;
   }
   else
      pucTdo = NULL;

   JtagShiftStreamPX14 (m_hBrd, lNumBits, flags, plvTdi->val, pucTdo);
}

