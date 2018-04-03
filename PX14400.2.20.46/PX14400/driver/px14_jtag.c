/** @file 	px14_jtag.c
    @brief 	JTAG operations; used for firmware uploading
*/
#include "px14_drv.h"
uint __GFP_REPEAT;
static int JtagStream_WR (px14_device* devp, PX14S_JTAG_STREAM *ppjs);
static int JtagStream_W (px14_device* devp, PX14S_JTAG_STREAM *ppjs);
static int JtagStream_R (px14_device* devp, PX14S_JTAG_STREAM *ppjs);

int px14ioc_jtag_io (px14_device* devp, u_long arg, struct file* filp)
{
  PX14S_JTAGIO ctx;
  unsigned int i;
  int res;

  // Input is a PX14S_JTAGIO structure
  res = __copy_from_user(&ctx, (void*)arg, sizeof(PX14S_JTAGIO));
  if (res != 0)
    return -EFAULT;

  PX14_LOCK_MUTEX(devp)
  {
    // Is user requesting to initiate a JTAG IO session?
    if (ctx.flags & PX14JIOF_START_SESSION) {
      if ((devp->jtag_filp == NULL) ||
	  (filp == devp->jtag_filp)) {
	devp->jtag_filp = filp;
	ctx.valR = 1;
      }
      else {
	// JTAG access is already granted to another file handle
	ctx.valR = 0;
      }
    }

    // Is user requesting to end a JTAG IO session?
    else if (ctx.flags & PX14JIOF_END_SESSION) {
      if (devp->jtag_filp == filp) {
	devp->jtag_filp = NULL;
	ctx.valR = 1;
      }
      else {
	// Calling device isn't current JTAG owner
	ctx.valR = 0;
      }
    }

    else {

      // Actual JTAG IO request, make sure caller is JTAG owner
      
      if (filp != devp->jtag_filp)
	res = -EACCES;
      else {
	
	atomic_inc(&devp->stat_jtag_ops);

	// Is this a clock pulse loop?
	if (ctx.flags & PX14JIOF_PULSE_TCK_LOOP) {
	  // Pulse clock ctx.valW times
	  for (i=0; i<ctx.valW; i++) {
	    devp->regCfg.fields.reg1.bits.tck = 0;
	    WriteHwRegJtagStatus(devp);
	    devp->regCfg.fields.reg1.bits.tck = 1;
	    WriteHwRegJtagStatus(devp);
	  }
	}

	// Is user writing anything?
	if (ctx.maskW) {
	  devp->regCfg.fields.reg1.val &= ~ctx.maskW;
	  devp->regCfg.fields.reg1.val |= (ctx.maskW & ctx.valW);
	  WriteHwRegJtagStatus(devp);
	}

	// Should we be pulsing the clock?
	if (ctx.flags & PX14JIOF_PULSE_TCK) {
	  devp->regCfg.fields.reg1.bits.tck = 0;
	  WriteHwRegJtagStatus(devp);
	  devp->regCfg.fields.reg1.bits.tck = 1;
	  WriteHwRegJtagStatus(devp);
	}
        
	// Is user requesting a JTAG read
	if (ctx.flags & PX14JIOF_POST_READ) {
	  PX14_FLUSH_BUS_WRITES(devp);
	  devp->regCfg.fields.reg1.val = ReadHwRegJtagStatus(devp);
	  ctx.valR = devp->regCfg.fields.reg1.val;
	}
      }
    }
  }
  PX14_UNLOCK_MUTEX(devp);

  // Optional output is PX14S_JTAGIO structure
  if (ctx.flags & PX14JIOF__RESULTANT_DATA)
    res = __copy_to_user ((void*)arg, &ctx, sizeof(PX14S_JTAGIO)) ? -EFAULT:0;
  
  return res;
}

int JtagStream_WR (px14_device* devp, PX14S_JTAG_STREAM *ppjs)
{
  u_char ucTdiByte, ucTdoByte, ucTdoBit;
  u_char *pTdiBuf, *pucTdo, *pucTdi;
  u_int byte_count, bit_count;
  int i;

  byte_count = (ppjs->nBits + 7) >> 3;
  bit_count = ppjs->nBits;

  // Buffer the TDI data so we can use given buffer for TDO.
  if (NULL == (pTdiBuf = kmalloc(byte_count, GFP_KERNEL | __GFP_REPEAT)))
    return -ENOMEM;

  memcpy(pTdiBuf, ppjs->dwData, byte_count);

  pucTdo = ppjs->dwData + byte_count;
  pucTdi = pTdiBuf + byte_count;
  
  while (bit_count) {
    ucTdiByte = *(--pucTdi);
    ucTdoByte = 0;

    for (i=0; bit_count && i<8; i++) {
      bit_count--;

      if (!bit_count && (ppjs->flags & PX14JSS_EXIT_SHIFT)) {
	// Exit SHIFT-?R state
	devp->regCfg.fields.reg1.bits.tms = 1;
      }

      // Set TDI data.
      devp->regCfg.fields.reg1.bits.tdi = (ucTdiByte & 1);
      ucTdiByte >>= 1;

      // De-assert TCK and flush
      devp->regCfg.fields.reg1.bits.tck = 0;
      WriteHwRegJtagStatus(devp);

      // Read TDO
      PX14_FLUSH_BUS_WRITES(devp);
      devp->regCfg.fields.reg1.val = ReadHwRegJtagStatus(devp);
      ucTdoBit = (u_char)devp->regCfg.fields.reg1.bits.tdo;
      ucTdoByte |= (ucTdoBit << i);

      // Re-assert TCK and flush.
      devp->regCfg.fields.reg1.bits.tck = 1;
      WriteHwRegJtagStatus(devp);
    }

    *(--pucTdo) = ucTdoByte;
  }

  kfree(pTdiBuf);

  return 0;
}

int JtagStream_W (px14_device* devp, PX14S_JTAG_STREAM *ppjs)
{
  u_int byte_count, bit_count;
  u_char *pucTdi, ucTdiByte;
  int i;

  byte_count = (ppjs->nBits + 7) >> 3;
  pucTdi = ppjs->dwData + byte_count;
  bit_count = ppjs->nBits;

  while (bit_count) {
    ucTdiByte = *(--pucTdi);

    for (i=0; bit_count && i<8; i++) {
      bit_count--;

      if (!bit_count && (ppjs->flags & PX14JSS_EXIT_SHIFT)) {
	// Exit SHIFT-?R state
	devp->regCfg.fields.reg1.bits.tms = 1;
      }

      // Set TDI data.
      devp->regCfg.fields.reg1.bits.tdi = (ucTdiByte & 1);
      ucTdiByte >>= 1;

      // De-assert TCK and flush
      devp->regCfg.fields.reg1.bits.tck = 0;
      WriteHwRegJtagStatus(devp);

      // Re-assert TCK and flush.
      devp->regCfg.fields.reg1.bits.tck = 1;
      WriteHwRegJtagStatus(devp);
    }
  }
  
  return 0;
}

int JtagStream_R (px14_device* devp, PX14S_JTAG_STREAM *ppjs)
{
  u_char ucTdoByte, ucTdoBit, *pucTdo;
  u_int byte_count, bit_count;
  int i;

  byte_count = (ppjs->nBits + 7) >> 3;
  pucTdo = ppjs->dwData + byte_count;
  bit_count = ppjs->nBits;

  while (bit_count) {
    ucTdoByte = 0;

    for (i=0; bit_count && i<8; i++) {
      bit_count--;

      if (!bit_count && (ppjs->flags & PX14JSS_EXIT_SHIFT)) {
	// Exit SHIFT-?R state
	devp->regCfg.fields.reg1.bits.tms = 1;
      }

      // De-assert TCK and flush
      devp->regCfg.fields.reg1.bits.tck = 0;
      WriteHwRegJtagStatus(devp);

      // Read TDO
      PX14_FLUSH_BUS_WRITES(devp);
      devp->regCfg.fields.reg1.val = ReadHwRegJtagStatus(devp);
      ucTdoBit = (u_char)devp->regCfg.fields.reg1.bits.tdo;
      ucTdoByte |= (ucTdoBit << i);

      // Re-assert TCK and flush.
      devp->regCfg.fields.reg1.bits.tck = 1;
      WriteHwRegJtagStatus(devp);
    }

    *(--pucTdo) = ucTdoByte;
  }

  return 0;
}


int px14ioc_jtag_stream(px14_device* devp, u_long arg, struct file* filp)
{
  PX14S_JTAG_STREAM ctx_base, *ctxp;
  u_int data_bytes, struct_size;
  int res;

  // Input is a (variable-sized) PX14S_JTAG_STREAM structure
  res = __copy_from_user(&ctx_base, (void*)arg, sizeof(PX14S_JTAG_STREAM));
  if (res != 0)
    return -EFAULT;
  if (0 == ctx_base.nBits)
    return 0;
  // Figure out how large our input data is, and get it
  data_bytes = (ctx_base.nBits + 7) >> 3;
  struct_size = sizeof(PX14S_JTAG_STREAM) + data_bytes;
  ctxp = kmalloc(struct_size, GFP_KERNEL | __GFP_REPEAT);
  if (NULL == ctxp)
    return -ENOMEM;
  // Only read input data if we're writing
  if (PX14JSS_WRITE_TDI & ctx_base.flags) {
    res = copy_from_user(ctxp, (void*)arg, struct_size);
    if (res != 0) {
      kfree (ctxp);
      return -EFAULT;
    }
  }

  PX14_LOCK_MUTEX(devp)
  {
    // Make sure calling user the current JTAG owner.
    if (filp != devp->jtag_filp)
      res = -EACCES;
    else {

      if (PX14JSS_WRITE_READ == (ctx_base.flags & PX14JSS__OP_MASK))
	res = JtagStream_WR (devp, ctxp);
      else if (PX14JSS_WRITE_TDI == (ctx_base.flags & PX14JSS__OP_MASK))
	res = JtagStream_W (devp, ctxp);
      else if (PX14JSS_READ_TDO == (ctx_base.flags & PX14JSS__OP_MASK))
	res = JtagStream_R (devp, ctxp);
      else
	res = -SIG_PX14_UNEXPECTED;	// Unreachable code.
    }
  }
  PX14_UNLOCK_MUTEX(devp);

  // Output is (variable-sized) PX14S_JTAG_STREAM structure
  if ((0 == res) && (PX14JSS_READ_TDO & ctx_base.flags)) {
    res = copy_to_user ((void*)arg, ctxp, struct_size) ? -EFAULT:0;
  }

  kfree (ctxp);

  return res;
}


