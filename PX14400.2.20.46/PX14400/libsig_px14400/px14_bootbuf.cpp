/** @file	px14_bootbuf.cpp
	@brief	PX14 boot-time DMA buffer allocation support
*/
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"

/** @brief Check out boot buffer with given index

	This function will check out the boot-buffer with the given index. If 
	the buffer is already checked out by another handle then the function 
	will return error code SIG_PX14_BUFFER_CHECKED_OUT. If no buffer has 
	been allocated for the given index then the function will return 
	SIG_PX14_BUFFER_NOT_ALLOCATED.

	When a boot-buffer is checked out, it is checked out to a particular
	board handle (HPX14). When this handle closes, all boot-buffers checked
	out to it are automatically checked-in. This includes explicit 
	disconnection from the handle (DisconnectFromDevicePX14) or implicit 
	disconnection (process cleanup).

	A checked out buffer is only valid in the process that checked the
	buffer out. A boot-buffer may not be checked out to more than one
	process at a time. It is okay for one process to check out some boot
	buffers (say indices 0 and 1) and another process to check out other
	boot buffers (say 2 and 3).

	If there are multiple custom applications on the same system that use
	boot buffers it is up them to arbitrate access to the boot buffers. The
	PX14400 library checks out buffers on a first-come first-served basis.
	Signatec software, such as the PX14400 Scope Application, will only use
	boot buffers if explicitly enabled by the user.

	@param hBrd
		A handle to a PX14400 device obtained by calling ConnectToDevicePX14
	@param buf_idx
		The 0-based index of the buffer to check out
	@param dma_bufpp
		A pointer to a px14_sample_t* variable that will receive the address
		of the DMA buffer. This buffer can be used by any library function
		that takes a DMA buffer parameter.
	@pbuf_samples
		A pointer to an unsigned integer variable that will receive the size
		of the checked-out buffer in px14_sample_t elements. This parameter
		may be NULL.

	@return
		Returns SIG_SUCCESS on success of one of the SIG_* error values (which are all negative) on error.
*/
PX14API BootBufCheckOutPX14 (HPX14           hBrd,
							 unsigned short  buf_idx,
							 px14_sample_t** dma_bufpp,
							 unsigned int*   pbuf_samples)
{
	int res;

	PX14_ENSURE_POINTER(hBrd, dma_bufpp, px14_sample_t*, "BootBufCheckOutPX14");
	SIGASSERT_NULL_OR_POINTER(pbuf_samples, unsigned int);

	PX14S_BOOTBUF_CTRL ctx;
	ctx.struct_size = sizeof(PX14S_BOOTBUF_CTRL);
	ctx.operation   = PX14BBOP_CHECK_OUT;
	ctx.buf_idx     = buf_idx;
	res = DeviceRequest(hBrd, IOCTL_PX14_BOOTBUF_CTRL, &ctx,
		sizeof(PX14S_BOOTBUF_CTRL), sizeof(PX14S_BOOTBUF_CTRL));
	PX14_RETURN_ON_FAIL(res);
	
	PX14U_ADDR_UNION au;
	au.addr_int = ctx.virt_addr;
	*dma_bufpp  = au.sampp;

	if (pbuf_samples)
		*pbuf_samples = ctx.buf_samples;

	return SIG_SUCCESS;
}

/** @brief Check in boot buffer

	This function will return SIG_PX14_BUSY if the specified buffer is 
	currently in use for a DMA transfer operation. A boot buffer cannot be
	checked in while in use. The current DMA transfer can be cancelled by
	putting the card into Standby mode and then the buffer can be checked in.

	@param hBrd
		A handle to a PX14400 device obtained by calling ConnectToDevicePX14
	@param dma_bufp
		The address of the boot-time allocated DMA buffer to check in.

	@return
		Returns SIG_SUCCESS on success of one of the SIG_* error values (which are all negative) on error.
*/
PX14API BootBufCheckInPX14 (HPX14 hBrd, px14_sample_t* dma_bufp)
{
	PX14U_ADDR_UNION au;
	au.addr_int = 0;
	au.sampp    = dma_bufp;

	PX14S_BOOTBUF_CTRL ctx;
	ctx.struct_size = sizeof(PX14S_BOOTBUF_CTRL);
	ctx.operation   = PX14BBOP_CHECK_IN;
	ctx.virt_addr   = au.addr_int;
	return DeviceRequest(hBrd, IOCTL_PX14_BOOTBUF_CTRL, &ctx, sizeof(PX14S_BOOTBUF_CTRL));
}

/** @brief Query boot buffer status

	@param hBrd
		A handle to a PX14400 device obtained by calling ConnectToDevicePX14
	@param buf_idx
		The 0-based index of the buffer to check out.
	@param pchecked_out
		A pointer to an integer that will receive the check-out status of
		the buffer with the given index. If the buffer is currently checked
		out then the check-out status will be a non-zero value. If the 
		buffer is not checked out then the check-out status will be zero.
	@param pbuf_samples
		A pointer to an unsigned integer variable that will receive the
		size of the buffer with the given index in px14_sample_t elments.
		Zero will be used if no buffer has been allocated.
*/
PX14API BootBufQueryPX14 (HPX14          hBrd,
						  unsigned short buf_idx,
						  int*           pchecked_out,
						  unsigned int*  pbuf_samples)
{
	int res;

	SIGASSERT_NULL_OR_POINTER(pchecked_out, int);
	SIGASSERT_NULL_OR_POINTER(pbuf_samples, unsigned int);

	PX14S_BOOTBUF_CTRL ctx;
	ctx.struct_size = sizeof(PX14S_BOOTBUF_CTRL);
	ctx.operation   = PX14BBOP_QUERY;
	ctx.buf_idx     = buf_idx;
	res = DeviceRequest(hBrd, IOCTL_PX14_BOOTBUF_CTRL, &ctx,
		sizeof(PX14S_BOOTBUF_CTRL), sizeof(PX14S_BOOTBUF_CTRL));
	PX14_RETURN_ON_FAIL(res);

	if (pchecked_out)
		*pchecked_out = (ctx.virt_addr != 0);
	if (pbuf_samples)
		*pbuf_samples = ctx.buf_samples;

	return SIG_SUCCESS;
}

/** @brief Request driver to reallocate boot buffers now

	Calling this function will have the driver attempt to allocate boot
	buffers now. If a particular boot buffer is already large enough it
	will not be reallocated. If a boot buffer is allocated and the new
	size is 0, the boot buffer will be freed.

	This function is intended as an attempt to get boot buffers after
	making a configuration change without having to reboot the system.

	This function will return SIG_PX14_BUSY if any boot buffers are 
	checked out. This function may only be called while all boot buffers
	are checked in.

	If any of the boot buffers could not be allocated, then the function
	will return SIG_DMABUFALLOCFAIL. BootBufQueryPX14 can be used to query
	sizes and check-out status of all buffers.
*/
PX14API BootBufReallocNowPX14 (HPX14 hBrd)
{
	PX14S_BOOTBUF_CTRL ctx;
	ctx.struct_size = sizeof(PX14S_BOOTBUF_CTRL);
	ctx.operation   = PX14BBOP_REALLOC;
	return DeviceRequest(hBrd, IOCTL_PX14_BOOTBUF_CTRL, &ctx, sizeof(PX14S_BOOTBUF_CTRL));
}

/** @brief Set boot buffer configuration; set requested boot buffer size

	This information is stored in the given board's configuration EEPROM
	so the setting will stick until explicitly changed. The configuration
	EEPROM will only be written if the current configuration is different.

	Note that changing this configuration will have no effect on currently
	allocated boot buffers. As part of the device driver's loading, it will
	consult the boot buffer configuration and allocate accordingly. Software
	can force the driver to reallocate (if necessary) boot buffers by 
	calling BootBufReallocNowPX14.
*/
PX14API BootBufCfgSetPX14 (HPX14 hBrd, unsigned short buf_idx, unsigned int buf_samples)
{
	unsigned short bytes_low_r, bytes_high_r;
	unsigned short bytes_low_w, bytes_high_w;
	unsigned int eeprom_addr, buffer_bytes;
	int res;

	if (buf_idx >= PX14_BOOTBUF_COUNT)
		return SIG_OUTOFBOUNDS;

	buffer_bytes = buf_samples * sizeof(px14_sample_t);
	eeprom_addr = PX14EA_BOOTBUF_BASE + (buf_idx << 1);

	// EEPROM has a finite number of writes. Only do the write if
	//  we're changing anything

	res = GetEepromDataPX14(hBrd, eeprom_addr + 0, &bytes_low_r);
	PX14_RETURN_ON_FAIL(res);
	res = GetEepromDataPX14(hBrd, eeprom_addr + 1, &bytes_high_r);
	PX14_RETURN_ON_FAIL(res);

	bytes_low_w  = static_cast<unsigned short>(buffer_bytes & 0xFFFF);
	bytes_high_w = static_cast<unsigned short>(buffer_bytes >> 16);

	if (bytes_low_r != bytes_low_w)
	{
		res = SetEepromDataPX14(hBrd, eeprom_addr + 0, bytes_low_w);
		PX14_RETURN_ON_FAIL(res);
	}
	if (bytes_high_r != bytes_high_w)
	{
		res = SetEepromDataPX14(hBrd, eeprom_addr + 1, bytes_high_w);
		PX14_RETURN_ON_FAIL(res);
	}
	if ((bytes_low_r != bytes_low_w) || (bytes_high_r != bytes_high_w))
	{
		res = ResetEepromChecksumPX14(hBrd);
		PX14_RETURN_ON_FAIL(res);
	}

	return SIG_SUCCESS;
}

/** @brief Get boot buffer configuration; get requested boot buffer size
*/
PX14API BootBufCfgGetPX14 (HPX14 hBrd, unsigned short buf_idx, unsigned int* pbuf_samples)
{
	unsigned short bytes_low_r, bytes_high_r;
	unsigned int eeprom_addr;
	int res;

	PX14_ENSURE_POINTER(hBrd, pbuf_samples, unsigned int, "BootBufCfgGetPX14");

	if (buf_idx >= PX14_BOOTBUF_COUNT)
		return SIG_OUTOFBOUNDS;

	eeprom_addr = PX14EA_BOOTBUF_BASE + (buf_idx << 1);
	res = GetEepromDataPX14(hBrd, eeprom_addr + 0, &bytes_low_r);
	PX14_RETURN_ON_FAIL(res);
	res = GetEepromDataPX14(hBrd, eeprom_addr + 1, &bytes_high_r);
	PX14_RETURN_ON_FAIL(res);

	*pbuf_samples = (static_cast<unsigned>(bytes_high_r) << 16) | bytes_low_r;
	*pbuf_samples /= sizeof(px14_sample_t);
	return SIG_SUCCESS;
}

/** @brief Get maximum boot buffer count for given device
*/
PX14API BootBufGetMaxCountPX14 (HPX14 hBrd)
{
	(void)hBrd;
	return PX14_BOOTBUF_COUNT;
}

