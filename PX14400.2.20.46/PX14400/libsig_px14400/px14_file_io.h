/** @file	px14_file_io.h
*/
#ifndef __px14_file_io_header_defined_20070220
#define __px14_file_io_header_defined_20070220

#include <fstream>
#include "px14_timestamp.h"

/// Interface for dumping PX14 sample data to a file
class IIoSinkCtxPX14
{
public:

	virtual ~IIoSinkCtxPX14() {}

	virtual int Init (HPX14 hBrd, 
		unsigned long long total_samples_to_move,
		PX14S_FILE_WRITE_PARAMS& params) = 0;
	virtual int Write (px14_sample_t* bufp, unsigned int samples) = 0;
	virtual void Release() = 0;

	typedef int (*SrdcGenCallback_t) (HPX14SRDC hSrdc, void* ctxp);
	virtual void SetSrdcGenCallback (SrdcGenCallback_t pFunc,
		void* ctxp) = 0;
};

//
// Concrete IIoSinkCtxPX14 implementations
//

/** @brief Base-class sink device class; also works as a NULL device

	This base class also handles the saving of timestamp data if specified
	in the file write parameters
*/
class CIoSinkCtx_Base : public IIoSinkCtxPX14
{
public:

	CIoSinkCtx_Base();

	virtual int Init (HPX14 hBrd, unsigned long long total_samples_to_move,
		PX14S_FILE_WRITE_PARAMS& params);
	virtual int Write (px14_sample_t* bufp, unsigned int samples);
	virtual void Release();

	// -- Implementation

	virtual ~CIoSinkCtx_Base();

	virtual void SetSrdcGenCallback (SrdcGenCallback_t pFunc, void* ctxp);

	// -- Overridables
	virtual const char* GetCurrentFilePath();
	virtual unsigned long long GetCurrentFileSamples();

protected:

	virtual int OnNewOutputFileOpened (const char* pathnamep, int active_channels);

	// Initialize state for timestamp generation
	virtual int ReadyTimestampState();

	typedef PX14S_FILE_WRITE_PARAMS _Params;

	_Params*		m_paramsp;
	HPX14			m_hBrd;

	unsigned long long		m_samps_to_move;		///< Total samples to write
	unsigned long long		m_samps_moved;			///< Samples written so far

	CTimestampMgrPX14*	m_pTsMgr;			///< Timestamp manager or NULL

	SrdcGenCallback_t	m_pSrdcCallback;
	void*				m_srdcCallbackCtx;

private:

	int GenerateSrdFile (const char* pathnamep, int active_channels,
		bool bEmbedAsAfs = false);
};

/// Binary file dump for single channel data
class CIoSinkCtx_BinarySingleChannel : public CIoSinkCtx_Base
{
public:

	CIoSinkCtx_BinarySingleChannel();

	virtual int Init (HPX14 hBrd, unsigned long long total_samples_to_move,
		PX14S_FILE_WRITE_PARAMS& params);
	virtual int Write (px14_sample_t* bufp, unsigned int samples);
	virtual void Release();

	// -- Implementation

	virtual ~CIoSinkCtx_BinarySingleChannel();

private:

	int				m_sysOpenCookie;
	int				m_fd;						///< File descriptor
};

/// Binary file dump for single channel data with maximum file size
class CIoSinkCtx_BinSingle_Seg : public CIoSinkCtx_Base
{
public:

	CIoSinkCtx_BinSingle_Seg();

	virtual int Init (HPX14 hBrd, unsigned long long total_samples_to_move,
		PX14S_FILE_WRITE_PARAMS& params);
	virtual int Write (px14_sample_t* bufp, unsigned int samples);
	virtual void Release();

	// -- Implementation

	virtual ~CIoSinkCtx_BinSingle_Seg();

protected:

	int StartNextFile();

	/// Generate pathname for next file
	void NextFileName(std::string& p);

private:

	std::string		m_pathname_no_ext;
	std::string		m_filename_ext;

	unsigned long long		m_samples_in_file;
	unsigned		m_file_index;

	int				m_sysOpenCookie;
	int				m_fd;						///< File descriptor
};

/// Binary file dump for dual file data; dual channel implied
class CIoSinkCtx_BinaryDualFile : public CIoSinkCtx_Base
{
public:

	CIoSinkCtx_BinaryDualFile();

	virtual int Init (HPX14 hBrd, unsigned long long total_samples_to_move,
		PX14S_FILE_WRITE_PARAMS& params);
	virtual int Write (px14_sample_t* bufp, unsigned int samples);
	virtual void Release();

	// -- Implementation

	virtual ~CIoSinkCtx_BinaryDualFile();

private:

	static const unsigned s_int_buf_samps = 16384;

	typedef std::auto_ptr<px14_sample_t> _IntBuf;

	int				m_sysOpenCookieCh1;
	int				m_fdCh1;				///< Ch1 file descriptor
	_IntBuf			m_spBufCh1;

	int				m_sysOpenCookieCh2;
	int				m_fdCh2;				///< Ch2 file descriptor
	_IntBuf			m_spBufCh2;
};

/// Binary file dump for segmented dual file data; dual channel implied
class CIoSinkCtx_BinDual_Seg : public CIoSinkCtx_Base
{
public:

	CIoSinkCtx_BinDual_Seg();

	virtual int Init (HPX14 hBrd, unsigned long long total_samples_to_move,
		PX14S_FILE_WRITE_PARAMS& params);
	virtual int Write (px14_sample_t* bufp, unsigned int samples);
	virtual void Release();

	// -- Implementation

	virtual ~CIoSinkCtx_BinDual_Seg();

protected:

	bool GetFilenameParts (const char* wholep,
		std::string& pathname_no_ext,
		std::string& filename_ext);

	/// Generate pathname for next file
	void NextFileName(std::string& p, bool bCh1);

	int StartNextFiles();

private:

	static const unsigned s_int_buf_samps = 16384;

	typedef std::auto_ptr<px14_sample_t> _IntBuf;

	unsigned long long		m_samples_in_file;
	unsigned		m_file_index;

	bool			m_bUsingCh1;
	int				m_sysOpenCookieCh1;
	int				m_fdCh1;				///< Ch1 file descriptor
	_IntBuf			m_spBufCh1;
	std::string		m_pathname1_no_ext;
	std::string		m_filename1_ext;

	bool			m_bUsingCh2;
	int				m_sysOpenCookieCh2;
	int				m_fdCh2;				///< Ch2 file descriptor
	_IntBuf			m_spBufCh2;
	std::string		m_pathname2_no_ext;
	std::string		m_filename2_ext;
};

/// Text file dump for single file data (single or dual channel)
class CIoSinkCtx_TextSingleFile : public CIoSinkCtx_Base
{
public:

	CIoSinkCtx_TextSingleFile(bool bDualChannel = false);

	virtual int Init (HPX14 hBrd, unsigned long long total_samples_to_move,
		PX14S_FILE_WRITE_PARAMS& params);
	virtual int Write (px14_sample_t* bufp, unsigned int samples);

	// -- Implementation

	virtual ~CIoSinkCtx_TextSingleFile();

private:

	std::ofstream	m_of;
	bool			m_bDualChannel;
};

/// Text file dump for dual file data (dual channel data implied)
class CIoSinkCtx_TextDualFile : public CIoSinkCtx_Base
{
public:

	CIoSinkCtx_TextDualFile();

	virtual int Init (HPX14 hBrd, unsigned long long total_samples_to_move,
		PX14S_FILE_WRITE_PARAMS& params);
	virtual int Write (px14_sample_t* bufp, unsigned int samples);

	virtual ~CIoSinkCtx_TextDualFile();

private:

	static const unsigned s_int_buf_samps = 16384;

	typedef std::auto_ptr<px14_sample_t> _IntBuf;

	std::ofstream	m_ofCh1;
	std::ofstream	m_ofCh2;
	_IntBuf			m_spBufCh1;
	_IntBuf			m_spBufCh2;
};

/// Utility class that releases IIoSinkCtxPX14 on destruction
class CAutoIoSinkCtx
{
	IIoSinkCtxPX14*	m_sinkp;

public:

	CAutoIoSinkCtx (IIoSinkCtxPX14* sinkp) : m_sinkp(sinkp) {}
	~CAutoIoSinkCtx() 
	{ 
		SIGASSERT_NULL_OR_POINTER(m_sinkp, IIoSinkCtxPX14);
		if (m_sinkp) m_sinkp->Release();
	}
};

int CreateIoSinkCtxPX14 (HPX14 hBrd,
						PX14S_FILE_WRITE_PARAMS* paramsp,
						IIoSinkCtxPX14** sinkpp);

int ReadSampleRamFileFastHaveSink (HPX14 hBrd,
								   unsigned int sample_start,
								   unsigned int sample_count,
								   px14_sample_t* dma_bufp,
								   unsigned int dma_buf_samples,
								   IIoSinkCtxPX14* sinkp);

#endif // __px14_file_io_header_defined_20070220

