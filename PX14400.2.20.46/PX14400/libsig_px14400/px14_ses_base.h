/** @file px14_ses_base.h
*/
#ifndef __px14_ses_base_header_defined
#define __px14_ses_base_header_defined

/** @brief Base class for asynchronous session objects
*/
class CPX14SessionBase
{
public:

	// -- Birth and death

	CPX14SessionBase (bool bRemoteSession) : m_ses_magic(_ses_magic),
    m_hBrd(PX14_INVALID_HANDLE), m_bRemote(bRemoteSession),
    m_remote_handle(0)
	{
	}

	virtual ~CPX14SessionBase()
	{
	}

	static bool IsRemoteSessionHandle (void* objp)
	{ 
		CPX14SessionBase* thisp;

		if (NULL == objp)
			return false;
		thisp = reinterpret_cast<CPX14SessionBase*>(objp);
		SIGASSERT_POINTER(thisp, CPX14SessionBase);
		if (thisp->m_ses_magic != _ses_magic)
			return false;

		return thisp->m_bRemote;
	}

	// -- Member data

	static const unsigned int _ses_magic = 0xE145E5B5;

	unsigned int	m_ses_magic;		///<  == _ses_magic
	HPX14			m_hBrd;				///< Handle to underlying PX14

	bool			m_bRemote;
	uint64_t		m_remote_handle;	///< Remote session handle
};




#endif // __px14_ses_base_header_defined



