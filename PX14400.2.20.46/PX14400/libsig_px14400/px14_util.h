/** @file	px14_util.h
	@brief	PX14 library utility classes
*/
#ifndef __px14_util_header_defined
#define __px14_util_header_defined

/** @brief Automatic temporary character buffer
*/
class CAutoCharBuf
{
public:

	CAutoCharBuf (const wchar_t* wide_bufp) : m_charp(NULL)
	{
		int res;
		
		if (SIG_SUCCESS != (res = init(wide_bufp)))
			throw res;
	}

	virtual ~CAutoCharBuf() 
		{ clean_up(); }

	operator char*() { return m_charp; }
	operator const char*() const { return m_charp; }

	int init (const wchar_t* wide_bufp)
	{
		int len, res;

		clean_up();

		SIGASSERT_NULL_OR_POINTER(wide_bufp, wchar_t);

		if (wide_bufp)
		{
			len = static_cast<int>(wcslen(wide_bufp) + 1);
			m_charp = reinterpret_cast<char*>(my_malloc (len * sizeof(char)));
			if (NULL == m_charp)
				return SIG_OUTOFMEMORY;

			res = ConvertWideCharToAscii(wide_bufp, len, m_charp);
			PX14_RETURN_ON_FAIL(res);
		}

		return SIG_SUCCESS;
	}

	char* get() 
		{ return m_charp; }

	char* release()
	{
		char* ret = m_charp;
		m_charp = NULL;
		return ret;
	}

protected:

	void clean_up()
	{
		SIGASSERT_NULL_OR_POINTER(m_charp, char);
		my_free(m_charp);
	}

private:

	char*	m_charp;
};

class CAutoSrdcHandle
{
	HPX14SRDC	m_hFile;

public:

	CAutoSrdcHandle (HPX14SRDC hFile) : m_hFile(hFile)
	{
	}

	~CAutoSrdcHandle()
	{
		if (PX14_INVALID_HPX14SRDC != m_hFile)
			CloseSrdcFilePX14(m_hFile);
	}
};

class CAutoStandbyModePX14
{
	HPX14	m_hBrd;

public:

	CAutoStandbyModePX14 (HPX14 hBrd) : m_hBrd(hBrd) 
	{}

	~CAutoStandbyModePX14()
	{
		SetOperatingModePX14(m_hBrd, PX14MODE_STANDBY);
	}
};

class CAutoPllLockCheckDeferral
{
	HPX14		m_hBrd;

public:

	CAutoPllLockCheckDeferral (HPX14 hBrd): m_hBrd(hBrd)
	{
		_DeferPllLockCheckPX14(m_hBrd, PX14_TRUE);
	}

	~CAutoPllLockCheckDeferral()
	{
		if (m_hBrd != PX14_INVALID_HANDLE) Release();
	}

	int Release()
	{
		int res = _DeferPllLockCheckPX14(m_hBrd, PX14_FALSE);
		m_hBrd = PX14_INVALID_HANDLE;
		return res;
	}
};
template <typename T>
T* my_malloc_arrayT (unsigned element_count, bool bZero = false)
{
	T* ret = reinterpret_cast<T*>(my_malloc(element_count * sizeof(T)));
	if (ret && bZero)
		memset (ret, 0, element_count * sizeof(T));

	return ret;
}

// Union used for returned user-virtual addr -> pointer conversions
// If going the other way, be sure to zero union out first!
typedef union _PX14U_ADDR_UNION_tag
{
    unsigned long long      addr_int;
    px14_timestamp_t*       timestampp;
    px14_sample_t*          sampp;
    void*                   voidp;

} PX14U_ADDR_UNION;

// Set additional context-specific error text
int SetErrorExtra (HPX14 hBrd, const char* err_extrap);

// std::string wrapper for GetErrorTextAPX14
int GetErrorTextStringPX14 (int res, std::string& s, 
                            unsigned int flags = 0,
                            HPX14 hBrd = PX14_INVALID_HANDLE);

int MakeTimeString (const time_t& t, std::string& s);

// Obtain generic name for given board revision (PX14BRDREV_*)
bool GetBoardRevisionStr (int brd_rev, std::string& s);

#endif // __px14_util_header_defined

