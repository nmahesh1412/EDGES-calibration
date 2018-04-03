/** @file	px14_virtual.h
	@brief	Routines for virtual-only devices
*/
#ifndef _px14_virtual_header_defined
#define _px14_virtual_header_defined

int GenerateVirtualDataPX14 (HPX14 hBrd,
							 px14_sample_t* bufp,
							 unsigned int samples,
							 unsigned int sample_start,
							 int channel = -1);

// Implements device IO controls for virtual devices
int VirtualDriverRequest (HPX14 hBrd, io_req_t req, void* inp,
						  size_t in_bytes, size_t out_bytes);

// Initialize software register cache
int VirtualInitSwrCache (HPX14 hBrd);

#endif // _px14_virtual_header_defined

