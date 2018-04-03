/** @file	px14_srd_file.h
*/
#ifndef __px14_srd_file_header_file_defined
#define __px14_srd_file_header_file_defined

#include "sig_srd_file.h"

class CSrdPX14 : public CSigRecDataFile
{
public:

	// -- Construction

	CSrdPX14(HPX14 hBrd);

	// -- Operations

	static int Validate (HPX14SRDC hFile, CSrdPX14** thispp = NULL);

	// -- Implementation

	virtual ~CSrdPX14();

	int SrdError (const char* preamblep);

	// -- Public member data

	static const unsigned	s_magic = 0x00D145DF;

	unsigned				m_magic;		///< Must equal s_magic
	HPX14					m_hBrd;			///< Associated board

protected:

	virtual bool GetPredefinedParameters (AcqDeviceParams& adp);
};


#endif // __px14_srd_file_header_file_defined

