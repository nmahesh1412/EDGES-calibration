/** @file 	px14_read.c
    @brief	Implementation of driver-mapped DMA transfer from device
 */
#include "px14_drv.h"

// This is actually a Windows limitation; we're keeping it here to keep the
// code the same. Working around this isn't a big deal anyway.
#define PX14_MAX_DEVICE_READ_BYTES		(60 * 1048576)

#define PX14_DEINT_CHUNK_BYTES			PAGE_SIZE
#define PX14_DEINT_CHUNK_SAMPLES		\
  (PX14_DEINT_CHUNK_BYTES / sizeof(PX14_SAMPLE_TYPE))


struct _DBXFERCTX_tag;

/// Signature of data deinterleave function used by driver-buffered xfers
typedef int (*btDeIntFunc_t) (px14_device* devp,
			      struct _DBXFERCTX_tag* dbctxp,
			      u_int samples_to_copy);

/// Driver-buffered transfer context
typedef struct _DBXFERCTX_tag {

  PX14_SAMPLE_TYPE*	btUserBufCh1;	///< Cur pos in user space buf (ch1)
  PX14_SAMPLE_TYPE*	btUserBufCh2;	///< Cur pos in user space buf (ch2)
  PX14_SAMPLE_TYPE*	btIntBufCh1;	///< Interleave buffer (ch1)
  PX14_SAMPLE_TYPE*	btIntBufCh2;	///< Interleave buffer (ch2)
  u_int			btSkipSamples;	///< Number of header samples to skip
  u_int			btSampsLeft;	///< Samples left for drv-buf xfer
  u_int			btFlags;	///< PX14DBTF_*
  btDeIntFunc_t		btDeintFunc;	///< Func to use for data deint

} DBXFERCTX;

static void StartNextBtReadSeg_PX14 (px14_device* devp, DBXFERCTX* dbctxp);
static int OnBtReadSegCompleted (px14_device* devp, DBXFERCTX* dbctxp);
static int DeinterleaveData (px14_device* devp, DBXFERCTX* dbctxp,
			     u_int samples_to_copy);

static int DeIntImpChan_both (px14_device* devp, DBXFERCTX* dbctxp, 
			      u_int samples_to_copy);
static int DeIntImpChan_1 (px14_device* devp, DBXFERCTX* dbctxp,
			   u_int samples_to_copy);
static int DeIntImpChan_2 (px14_device* devp, DBXFERCTX* dbctxp,
			   u_int samples_to_copy);

int px14ioc_driver_buffered_xfer (struct file* filp,
				 px14_device* devp,
				 u_long arg)
{
  static const unsigned dma_frm_samps = PX14_DMA_XFER_FRAME_SIZE_IN_SAMPLES;
  static const unsigned max_read_bytes = PX14_MAX_DEVICE_READ_BYTES;

  u_int sample_start_hw, sample_count_hw, highest_dma_valid_ram_addr;
  int bDualChanOp, bReadingDevice, res;
  int cur_op_mode, req_op_mode, devState;
  PX14S_DRIVER_BUFFERED_XFER ctx;
  DBXFERCTX dbctx;
  unsigned long f;

  // Input is a PX14S_DRIVER_BUFFERED_XFER structure
  res = __copy_from_user(&ctx,(void*)arg, sizeof(PX14S_DRIVER_BUFFERED_XFER));
  if (res != 0)
    return -EFAULT;

  // Validate starting sample and sample count
  if (ctx.sample_count * PX14_SAMPLE_SIZE_IN_BYTES > max_read_bytes) {
    // Library should know about this and be able to work around it
    return -EINVAL;
  }
  if ((ctx.sample_start > devp->ram_samples) ||
      (ctx.sample_start+(u_long)ctx.sample_count>devp->ram_samples)) {
    return -EINVAL;
  }
  // Must have channel 1 buffer if we're not interleaving/deinterleaving
  bDualChanOp = (ctx.flags & PX14DBTF_DEINTERLEAVE);
  if (!bDualChanOp && (0 == ctx.user_virt_addr)) {
    return -EINVAL;
  }

  if (ctx.flags & PX14DBTF_ASYNCHRONOUS)
    return -SIG_PX14_NOT_IMPLEMENTED_IN_DRIVER;	// Maybe later?

  // Validate operating mode
  req_op_mode = ctx.flags & PX14DBTF__MODE_MASK;
  if (req_op_mode == PX14DBTF__MODE_MASK)
    req_op_mode = -1;
  else {
    if (req_op_mode<0 || req_op_mode>=PX14MODE__COUNT)
      return -EINVAL;
  }
  bReadingDevice = (req_op_mode == PX14MODE_RAM_WRITE_PCI);

  // Get rid of a few trivialities that might actually goof us up if they 
  // sneak through
  if (ctx.sample_count == 0)
    return 0;
  if (bDualChanOp && (0 == ctx.user_virt_addr) && 
      (0 == ctx.user_virt_addr2)) {
    return 0;
  }

  memset (&dbctx, 0, sizeof(DBXFERCTX));

  PX14_LOCK_MUTEX(devp);
  do
  {
    PX14_LOCK_DEVICE(devp,f)
    {
      cur_op_mode = PX14_OP_MODE(devp);
      devState = devp->DeviceState;
      devp->dma_filp = filp;
    }
    PX14_UNLOCK_DEVICE(devp,f);

    if (NULL == devp->db_dma_descp) {
      // We weren't able to allocate or map driver's DMA buffer
      res = -SIG_DMABUFALLOCFAIL;
      break;
    }
    if (devState != PX14STATE_IDLE) {
      // We're already busy with another operation
      res = -SIG_PX14_BUSY;
      break;
    }
    if ((req_op_mode != -1) && (cur_op_mode != PX14MODE_STANDBY)) {
      // We're not in standby mode; let user-mode handle this
      res = -SIG_INVALID_MODE;
      break;
    }

    // Validate user's buffer(s)
    dbctx.btUserBufCh1 = (void*)(u_long)ctx.user_virt_addr;
    dbctx.btUserBufCh2 = (void*)(u_long)ctx.user_virt_addr2;
    dbctx.btDeintFunc = NULL;

    if (req_op_mode != -1) {
      // If req_op_mode is not -1 then we are responsible for doing the
      //  mode and active memory setup.
      
      // -- Define active memory region for aggregation of all DMA xfers

      // Partial-frame DMA transfers are not implemented so we need to 
      //  that ensure starting address is far back enough to allow for
      //  one DMA frame to be transferred.
      highest_dma_valid_ram_addr = devp->ram_samples - dma_frm_samps;

      // Starting address
      sample_start_hw = ctx.sample_start & ~(PX14_ALIGN_START_SAMPLE - 1);
      if (sample_start_hw > highest_dma_valid_ram_addr)
	sample_start_hw = highest_dma_valid_ram_addr;

      dbctx.btSkipSamples = ctx.sample_start - sample_start_hw;
			
      // Total sample count; aligned up to DMA frame size
      sample_count_hw = 
	(ctx.sample_count + dbctx.btSkipSamples + 
	 ((2*dma_frm_samps) - 1)) & ~((2*dma_frm_samps) - 1);

      if ((sample_start_hw > PX14_MAX_START_SAMPLE) || 
	  (sample_count_hw > PX14_MAX_SAMPLE_COUNT)) {
	res = -EINVAL;
	break;
      }

      SetActiveMemoryRegion_PX14(devp, sample_start_hw, sample_count_hw);
    }
    else {
      // If mode is -1 then we are not responsible for operating mode
      //  or active memory region setup. This is primarily for when 
      //  we're pulling data during a free run PCI buffered acq. In
      //  this case, starting sample is ignored, and sample count must
      //  be DMA transfer size aligned.

      if (ctx.sample_count % PX14_DMA_XFER_FRAME_SIZE_IN_SAMPLES) {
	res = -EINVAL;
	break;
      }
      
      dbctx.btSkipSamples = 0;
    }
    
    // Setup buffered-transfer state
    dbctx.btSampsLeft = ctx.sample_count;
    dbctx.btFlags = ctx.flags;

    // Setup device state
    devp->DeviceState = PX14STATE_DMA_XFER_BUFFERED;

    if (req_op_mode != -1)
      SetOperatingMode_PX14(devp, req_op_mode);

    res = 0;
  }
  while (0);
  PX14_UNLOCK_MUTEX(devp);

  // Main loop...
  while (!res && dbctx.btSampsLeft) {

    // Start next DMA transfer
    StartNextBtReadSeg_PX14(devp, &dbctx);
    // Wait for it to finish
    res = WaitForDmaTransferToComplete_PX14(devp);
    if (0 != res)
      break;

    // Copy data to user
    OnBtReadSegCompleted(devp, &dbctx);
  }

  if (req_op_mode != -1)
    SetOperatingMode_PX14(devp, PX14MODE_STANDBY);

  PX14_LOCK_DEVICE(devp, f) 
    {
      devp->dma_filp = NULL;
      devp->DeviceState = PX14STATE_IDLE;
    }
  PX14_UNLOCK_DEVICE(devp, f);

  return res;
}

void StartNextBtReadSeg_PX14 (px14_device* devp, DBXFERCTX* dbctxp)
{
  u_int xfer_samples, total_samples_left, db_buf_samps;
  unsigned long f;

  PX14_LOCK_DEVICE(devp,f)
    {
      // Determine how large next DMA transfer will be
      db_buf_samps = devp->db_dma_descp->buffer_bytes / PX14_SAMPLE_SIZE_IN_BYTES;
      total_samples_left = dbctxp->btSampsLeft + dbctxp->btSkipSamples;
      xfer_samples = PX14_MIN(total_samples_left, db_buf_samps);

      // Start the transfer
      BeginDmaTransfer_PX14(devp, devp->db_dma_descp->physAddr, xfer_samples, 0);
    }
  PX14_UNLOCK_DEVICE(devp,f);
}

int OnBtReadSegCompleted (px14_device* devp, DBXFERCTX* dbctxp)
{
  u_int new_samples_count, samples_to_copy;
  int res;

  // Determine how much data to copy
  new_samples_count = devp->dmaBytes / PX14_SAMPLE_SIZE_IN_BYTES;
  if (dbctxp->btSkipSamples) {
    PX14_ASSERT(new_samples_count > dbctxp->btSkipSamples);
    new_samples_count -= dbctxp->btSkipSamples;
  }
  samples_to_copy = PX14_MIN(new_samples_count, dbctxp->btSampsLeft);

  // Copy new data
  if (dbctxp->btFlags & PX14DBTF_DEINTERLEAVE) {
    DeinterleaveData (devp, dbctxp, samples_to_copy);

    if (dbctxp->btUserBufCh1)
      dbctxp->btUserBufCh1 += samples_to_copy >> 1;
    if (dbctxp->btUserBufCh2)
      dbctxp->btUserBufCh2 += samples_to_copy >> 1;
  }
  else {
    res = copy_to_user (dbctxp->btUserBufCh1, 
			devp->db_dma_descp->pBufKern + dbctxp->btSkipSamples,
			samples_to_copy * PX14_SAMPLE_SIZE_IN_BYTES);
    if (res != 0)
      return -EFAULT;

    dbctxp->btUserBufCh1 += samples_to_copy;
  }

  // Update counters
  if (dbctxp->btSkipSamples) 
    dbctxp->btSkipSamples = 0;
  dbctxp->btSampsLeft -= samples_to_copy;

  return 0;
}

int DeinterleaveData (px14_device* devp,
		      DBXFERCTX* dbctxp,
		      u_int samples_to_copy)
{
  int res;

  if (NULL == dbctxp->btDeintFunc) {

    // Allocate interleave buffers if necessary; this only happens the first
    //  time we run this function.
    if (NULL == devp->btIntBufCh1) {
      devp->btIntBufCh1 = kmalloc(PX14_DEINT_CHUNK_BYTES, GFP_KERNEL);
      if (NULL == devp->btIntBufCh1)
	return -ENOMEM;
    }
    if (NULL == devp->btIntBufCh2) {
      devp->btIntBufCh2 = kmalloc(PX14_DEINT_CHUNK_BYTES, GFP_KERNEL);
      if (NULL == devp->btIntBufCh2)
	return -ENOMEM;
    }

    // Figure out which data deinterleave function to use    
    if (dbctxp->btUserBufCh1 && dbctxp->btUserBufCh2) {
      // User wants both channels of data
      dbctxp->btDeintFunc = DeIntImpChan_both;
    }
    else if (dbctxp->btUserBufCh1) {
      // User only wants channel 1 data
      dbctxp->btDeintFunc = DeIntImpChan_1;
    }
    else if (dbctxp->btUserBufCh2) {
      // User only wants channel 2 data
      dbctxp->btDeintFunc = DeIntImpChan_2;
    }
    else 
      return -EINVAL;
  }

  return (*dbctxp->btDeintFunc)(devp, dbctxp, samples_to_copy);
}


int DeIntImpChan_both (px14_device* devp,
		       DBXFERCTX* dbctxp,
		       u_int samples_to_copy)
{
  u_int this_chunk_samples, loops, i;
  PX14_SAMPLE_TYPE* srcp;
  PX14_SAMPLE_TYPE* dstch1p;
  PX14_SAMPLE_TYPE* dstch2p;
  PX14_SAMPLE_TYPE* user_ch1;
  PX14_SAMPLE_TYPE* user_ch2;
  int res;

  user_ch1 = dbctxp->btUserBufCh1;
  user_ch2 = dbctxp->btUserBufCh2;
  srcp = devp->db_dma_descp->pBufKern;

  while (samples_to_copy) {

    this_chunk_samples = PX14_MIN(PX14_DEINT_CHUNK_SAMPLES, samples_to_copy);
    loops = this_chunk_samples >> 1;

    dstch1p = devp->btIntBufCh1;
    dstch2p = devp->btIntBufCh2;

    // Deinterleave into our deint buffer
    for (i=0; i<loops; i++) {
      *dstch1p++ = *srcp++;
      *dstch2p++ = *srcp++;
    }

    // Copy to user space
    //  Channel 1
    res = copy_to_user (user_ch1,
			devp->btIntBufCh1, 
			loops * PX14_SAMPLE_SIZE_IN_BYTES);
    if (res) return -EFAULT;
    user_ch1 += loops;
    
    //  Channel 2
    res = copy_to_user (user_ch2,
			devp->btIntBufCh2, 
			loops * PX14_SAMPLE_SIZE_IN_BYTES);
    if (res) return -EFAULT;
    user_ch2 += loops;

    samples_to_copy -= this_chunk_samples;
  }
  
  return 0;
}

int DeIntImpChan_1 (px14_device* devp,
		    DBXFERCTX* dbctxp,
		    u_int samples_to_copy)
{
  u_int this_chunk_samples, loops, i;
  PX14_SAMPLE_TYPE* srcp;
  PX14_SAMPLE_TYPE* dstch1p;
  PX14_SAMPLE_TYPE* user_ch1;
  int res;

  user_ch1 = dbctxp->btUserBufCh1;
  srcp = devp->db_dma_descp->pBufKern;

  while (samples_to_copy) {

    this_chunk_samples = PX14_MIN(PX14_DEINT_CHUNK_SAMPLES, samples_to_copy);
    loops = this_chunk_samples >> 1;

    dstch1p = devp->btIntBufCh1;

    // Deinterleave into our deint buffer
    for (i=0; i<loops; i++) {
      *dstch1p++ = *srcp;
      srcp += 2;
    }

    // Copy to user space
    //  Channel 1
    res = copy_to_user (user_ch1,
			devp->btIntBufCh1, 
			loops * PX14_SAMPLE_SIZE_IN_BYTES);
    if (res) return -EFAULT;
    user_ch1 += loops;
    
    samples_to_copy -= this_chunk_samples;
  }
  
  return 0;
}

int DeIntImpChan_2 (px14_device* devp,
		    DBXFERCTX* dbctxp,
		    u_int samples_to_copy)
{
  u_int this_chunk_samples, loops, i;
  PX14_SAMPLE_TYPE* srcp;
  PX14_SAMPLE_TYPE* dstch2p;
  PX14_SAMPLE_TYPE* user_ch2;
  int res;

  user_ch2 = dbctxp->btUserBufCh2;
  srcp = devp->db_dma_descp->pBufKern;

  // Skip channel 1's first sample
  srcp++;

  while (samples_to_copy) {

    this_chunk_samples = PX14_MIN(PX14_DEINT_CHUNK_SAMPLES, samples_to_copy);
    loops = this_chunk_samples >> 1;

    dstch2p = devp->btIntBufCh2;

    // Deinterleave into our deint buffer
    for (i=0; i<loops; i++) {
      *dstch2p++ = *srcp;
      srcp += 2;
    }

    // Copy to user space
    //  Channel 2
    res = copy_to_user (user_ch2,
			devp->btIntBufCh2, 
			loops * PX14_SAMPLE_SIZE_IN_BYTES);
    if (res) return -EFAULT;
    user_ch2 += loops;

    samples_to_copy -= this_chunk_samples;
  }
  
  return 0;
}
