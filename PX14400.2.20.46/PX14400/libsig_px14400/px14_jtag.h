/** @file px14_jtag.h
*/
#ifndef _px14_jtag_header_defined
#define _px14_jtag_header_defined

int JtagSessionCtrlPX14 (HPX14 hBrd, int bEnable, int* pbAccepted);
int WriteJtagPX14 (HPX14 hBrd, unsigned val, unsigned mask, 
				   unsigned flags = PX14JIOF__DEF);
int ReadJtagPX14 (HPX14 hBrd, unsigned* valp, 
				  unsigned flags = PX14JIOF__DEF);

int JtagShiftStreamPX14 (HPX14 hBrd, unsigned nBits, unsigned flags,
						 const unsigned char* pTdiData, 
						 unsigned char* pTdoData);

#endif // _px14_jtag_header_defined

