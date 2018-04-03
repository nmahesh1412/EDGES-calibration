/** @file 	px14_timestamps.c
    @brief 	PX14400 timestamp register IO routines
*/
#include "px14_drv.h"

/// Type of a PX14400 timestamp (64-bit unsigned)
typedef unsigned long long px14_timestamp_t;

typedef union _MY_ULARGE_INTEGER_tag
{
  unsigned long long QuadPart;
  struct {
    unsigned int LowPart;
    unsigned int HighPart;
  };

} MY_ULARGE_INTEGER; 

static int ReadTimeStamp(px14_device* devp, px14_timestamp_t* pts);


int px14ioc_read_timestamps (px14_device* devp, u_long arg)
{
  unsigned user_buf_bytes, reg_val, i, ts_to_read, ts_got;
  px14_timestamp_t __user* upBuf;
  px14_timestamp_t ts_cur;
  PX14S_READ_TIMESTAMPS ctx;
  int res, bSkipTsRead;
  unsigned long f;

  // Input is a PX14S_READ_TIMESTAMPS structure
  res = __copy_from_user(&ctx, (void*)arg, sizeof(PX14S_READ_TIMESTAMPS));
  if (res != 0)
    return -EFAULT;

  if (ctx.struct_size < _PX14SO_PX14S_READ_TIMESTAMPS_V1)
    return -SIG_PX14_UNKNOWN_STRUCT_SIZE;
  if (!ctx.buffer_items)
    return -SIG_INVALIDARG;
  upBuf = (px14_timestamp_t __user*)(unsigned long)ctx.user_virt_addr;
  if (ctx.buffer_items && !upBuf)
    return -SIG_INVALIDARG;

  // Currently only implementing synchronous: read what we can and return

  PX14_LOCK_MUTEX(devp)
  {
    ts_to_read = ctx.buffer_items;
    ts_got = 0;

    // Reset output flags
    ctx.flags &= ~PX14TSREAD__OUTPUT_FLAG_MASK;
    bSkipTsRead = 0;

    if (ctx.flags & PX14TSREAD_IGNORE_FIFO_FLAGS) {

      // Caller doesn't care about FIFO flags; just read the specified
      //  number of timestamps and return. This is intended for internal
      //  testing and troublshooting

      MY_ULARGE_INTEGER liTs;
      for (i=0; i<ts_to_read; i++) {
	PX14_LOCK_DEVICE(devp, f);
	{
	  liTs.HighPart = ReadDevReg(devp, 0xE);
	  liTs.LowPart  = ReadDevReg(devp, 0xF);
	}
	PX14_UNLOCK_DEVICE(devp, f);
	ts_cur = liTs.QuadPart;
	ts_got++;

	// Copy timestamp to userspace buffer
	res = copy_to_user(upBuf++, &ts_cur, sizeof(px14_timestamp_t));
	if (res != 0) {
	  res = -EFAULT;
	  break;
	}
      }  
    }
    else {

      // Is timestamp FIFO full?
      reg_val = ReadDevReg(devp, PX14REGIDX_TS_OVRF);
      if (reg_val & PX14REGMSK_TS_OVRF) {
	ctx.flags |= 
	  PX14TSREAD_FIFO_OVERFLOW | PX14TSREAD_MORE_AVAILABLE;
      
	if (0 == (ctx.flags & PX14TSREAD_READ_FROM_FULL_FIFO))
	  bSkipTsRead = 1;
      }

      if (!bSkipTsRead) {
	for (res=0,i=0; i<ts_to_read; i++,ts_got++) {

	  if (!ReadTimeStamp(devp, &ts_cur))
	    break;

	  // Copy timestamp to userspace buffer
	  res = copy_to_user(upBuf++, &ts_cur, sizeof(px14_timestamp_t));
	  if (res != 0) {
	    res = -EFAULT;
	    break;
	  }
	}
	if (!res && i>=ts_to_read) {
	  // We've read all requested timestamps, make a quick check
	  //  to see if more are available since we're already here in
	  //  the driver.
	  reg_val = ReadDevReg(devp, PX14REGIDX_TSFIFO_EMPTY);
	  if (0 == (reg_val & PX14REGMSK_TSFIFO_EMPTY))
	    ctx.flags |= PX14TSREAD_MORE_AVAILABLE;
	}
      }
    }
  }
  PX14_UNLOCK_MUTEX(devp);

  if (0 == res) {

    ctx.buffer_items = ts_got;

    // Only copy out first three items; want flags and item count
    res = __copy_to_user ((void*)arg, &ctx, sizeof(unsigned int) * 3) 
      ? -EFAULT : 0;
  }
  
  return res;
}

int ReadTimeStamp(px14_device* devp, px14_timestamp_t* pts)
{
  MY_ULARGE_INTEGER liTs;
  unsigned reg_val;
  unsigned long f;
  int bGotTs;
	
  reg_val = ReadDevReg(devp, PX14REGIDX_TSFIFO_EMPTY);
  if (reg_val & PX14REGMSK_TSFIFO_EMPTY)
    {
      // Come back later; no timestamps are in TS FIFO
      bGotTs = 0;
    }
  else
    {
      PX14_LOCK_DEVICE(devp, f);
      {
	liTs.HighPart = ReadDevReg(devp, 0xE);
	liTs.LowPart  = ReadDevReg(devp, 0xF);
      }
      PX14_UNLOCK_DEVICE(devp, f);
      bGotTs = 1;
    }

  if (bGotTs)
    *pts = liTs.QuadPart;

  return bGotTs;
}
