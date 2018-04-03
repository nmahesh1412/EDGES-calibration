/** @file	px14_jtag.cpp
	@brief	PX14 JTAG IO routines
*/
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"

static int DoJtagIo (HPX14 hBrd, PX14S_JTAGIO& jio);

int WriteJtagPX14 (HPX14 hBrd, unsigned val, unsigned mask, unsigned flags)
{
	PX14S_JTAGIO jio;

	jio.struct_size = sizeof(PX14S_JTAGIO);
	jio.flags = flags;
	jio.maskW = mask;
	jio.valW = val;

	return DoJtagIo(hBrd, jio);
}

int ReadJtagPX14 (HPX14 hBrd, unsigned* valp, unsigned flags)
{
	PX14S_JTAGIO jio;
	int res;

	SIGASSERT_POINTER(valp, unsigned);
	if (NULL == valp)
		return SIG_PX14_INVALID_ARG_2;

	jio.struct_size = sizeof(PX14S_JTAGIO);
   jio.flags = PX14JIOF_POST_READ | flags;
   jio.maskW = 0;		// Don't write anything

   if (SIG_SUCCESS != (res = DoJtagIo(hBrd, jio)))
      return res;

   *valp = jio.valR;
   return SIG_SUCCESS;
}

int JtagSessionCtrlPX14 (HPX14 hBrd, int bEnable, int* pbAccepted)
{
   PX14S_JTAGIO jio;
   int res;

   // pbAccepted is optional if !bEnable
   SIGASSERT_NULL_OR_POINTER(pbAccepted, int);

   jio.struct_size = sizeof(PX14S_JTAGIO);
   jio.flags = bEnable ? PX14JIOF_START_SESSION : PX14JIOF_END_SESSION;
   jio.maskW = 0;	// Not writing anything.

   if (SIG_SUCCESS != (res = DoJtagIo(hBrd, jio)))
      return res;

   if (pbAccepted)
      *pbAccepted = IsDeviceVirtualPX14(hBrd) ? 1 : jio.valR ? 1 : 0;

   return SIG_SUCCESS;
}

/** @brief Stream in/out a number of bits through the JTAG chain.

  This function is meant to stream in and/or stream out a number
  of bits through the JTAG stream. In situations where a batch of data
  is to be streamed, using the elementary ReadJtagPX14 and WriteJtagPX14
  functions are inefficient since a trip into the driver is required for
  each bit.

  @pre
  This function assumes that the current JTAG TAP state is SHIFT-DR or
  SHIFT-IR.

  @param hBrd
  A handle to the target PX14 device
  @param nBits
  The total number of bits to shift in/out.
  @param flags
  A set of PX14JSS_* flags that control the behavior of the function.
  @param pTdiData
  A pointer to the buffer containing the data that will be written to
  TDI. This buffer must be at least ((nBits + 7) / 8) bytes in size.
  If no data is being written then this value may be NULL.
  @param pTdoData
  A pointer to the buffer that will receive data read from the JTAG
  chain (TDO). This buffer must be at least ((nBits + 7) / 8) bytes
  in size. If no data is being read then this value may be NULL.

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values
  on error.

  @sa ReadJtagPX14, WriteJtagPX14
  */
int JtagShiftStreamPX14 (HPX14 hBrd,
                         unsigned nBits,
                         unsigned flags,
                         const unsigned char* pTdiData,
                         unsigned char* pTdoData)
{
   unsigned nBytes, sz, szIn, szOut;
   PX14S_JTAG_STREAM* ppjs;
   int bRead, bWrite;
   int res;

   if (flags & PX14JSS_WRITE_TDI)
   {
      SIGASSERT_POINTER(pTdiData, const unsigned char);
      if (NULL == pTdiData)
         return SIG_PX14_INVALID_ARG_4;
   }
   if (flags & PX14JSS_READ_TDO)
   {
      SIGASSERT_POINTER(pTdoData, unsigned char);
      if (NULL == pTdoData)
         return SIG_PX14_INVALID_ARG_5;
   }

   bWrite = flags & PX14JSS_WRITE_TDI;
   bRead = flags & PX14JSS_READ_TDO;
   nBytes = (nBits + 7) >> 3;

   // Allocate our request
   sz = sizeof(PX14S_JTAG_STREAM) + nBytes;
   ppjs = (PX14S_JTAG_STREAM*)my_malloc(sz);
   if (NULL == ppjs)
      return SIG_OUTOFMEMORY;

   ppjs->flags = flags;
   ppjs->nBits = nBits;
   szOut = bRead ? sz : 0;

   if (bWrite)
   {
      memcpy (ppjs->dwData, pTdiData, nBytes);
      szIn = sz;
   }
   else
      szIn = sizeof(PX14S_JTAG_STREAM);

   // Make request to driver.
   res = DeviceRequest(hBrd, IOCTL_PX14_JTAG_STREAM, ppjs, szIn, szOut);
   if (SIG_SUCCESS == res)
   {
      if (bRead && !IsDeviceVirtualPX14(hBrd))
         memcpy (pTdoData, ppjs->dwData, nBytes);
   }

   my_free(ppjs);

   return res;
}

int DoJtagIo (HPX14 hBrd, PX14S_JTAGIO& jio)
{
   size_t sizeOut;

#ifdef _PX14PP_LINUX_PLATFORM
   SysMicroSleep (20);
#endif

   sizeOut = (jio.flags & PX14JIOF__RESULTANT_DATA)
      ? sizeof(PX14S_JTAGIO) : 0;
   return DeviceRequest(hBrd, IOCTL_PX14_JTAG_IO,
                        &jio, sizeof(PX14S_JTAGIO), sizeOut);
}

