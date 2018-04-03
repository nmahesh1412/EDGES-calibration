/** @file	px14_record.h
*/
#ifndef __px14_record_header_defined
#define __px14_record_header_defined

#include "px14_ses_base.h"

// -- Forward declarations
class IIoSinkCtxPX14;

/// -- PX14 Recording Session flags (PX14RECIMPF_*)
/// We need to free DMA buffer on session cleanup
#define PX14RECIMPF_FREE_DMA_BUFFER			0x00000001
/// Recording thread has been created
#define PX14RECIMPF_REC_THREAD_CREATED		0x00000002
/// Recording thread has been armed
#define PX14RECIMPF_REC_THREAD_ARMED		0x00000004
#define PX14RECIMPF__DEFAULT				0

/** @brief PX14400 recording session state

	All  th_* methods run in recording thread
	All !th_* methods run in primary thread
*/
class CPX14RecSession : public CPX14SessionBase
{
public:

	static int ValidateRecHandle (HPX14RECORDING hRec, CPX14RecSession** ctxpp);

	// -- Construction

	CPX14RecSession();

	// -- Methods

	virtual int CreateSession (HPX14 hBrd, PX14S_REC_SESSION_PARAMS* rec_paramsp);
	virtual int Arm();
	virtual int Abort();
	virtual int Progress (PX14S_REC_SESSION_PROG* progp, unsigned flags);
	virtual int Snapshot (px14_sample_t* bufp, unsigned int samples,
		unsigned int* samples_gotp, unsigned int* ss_countp);

	unsigned int GetOutFlags() const { return m_fil_params.flags_out; }

	// -- Implementation

	virtual ~CPX14RecSession();

protected:

	// - Called from main thread -

	virtual unsigned int DetermineRecXferSize (unsigned int hint);

	// Called after recording state and buffers are initialized
	//  and just before primary recording thread creation
	virtual int PreThreadCreate();
	// Called after primary thread executes
	virtual void PostThreadRun();

	// Allocate/ready buffers needed for recording
	virtual int InitRecordingBuffers();

	int CopyRecParams (PX14S_REC_SESSION_PARAMS* rec_paramsp);
	int EnsureSnapshotBuf (unsigned int samples);
	int DoInitialProgressCheck();

	// ---- Thread functions (th_*) ----

	/// Main recording thread implemented by concrete subclass
	virtual int th_RecordMain() = 0;

	virtual int th_CreateIoSink(IIoSinkCtxPX14** sinkpp);
	static  int th_MyRecSrdcGenCallback (HPX14SRDC hSrdc, void* callback_ctxp);
	virtual int th_PostRecordUpdateAllSrdcData();
	virtual int th_PostRecUpdateSrdcFile (HPX14SRDC hSrdc);
	bool th_Snapshot (const px14_sample_t* rec_data, unsigned int rec_data_samples);

	// Returns res
	int th_RuntimeError (int res, const char* descp);

private:

	// Actual recording thread function; calls instanced th_main
	static void* th_raw (void* paramp);
	virtual void th_main();

protected:

	// - Aliases

	typedef PX14S_REC_SESSION_PARAMS	RecParams;
	typedef PX14S_FILE_WRITE_PARAMS		FileParams;
	typedef std::list<std::string>		SrdcFileList;

	// -- Member data

	static const unsigned int _magic = 0xE145E500;
	static const unsigned int _def_chain_samps = (128 * 1048576);
	static const unsigned int _def_xfer_samps = 1048576 * 2;
	static const unsigned int _rec_timeout_ms = 5000;
	static const unsigned int s_update_prog_ticks = 400;

	// - Used by main thread only

	unsigned int		m_magic;			///< == _magic
	unsigned int		m_flags;			///< PX14RECIMPF_*

	HPX14				m_hBrdMainThread;	///< Main thread's handle
	RecParams			m_rec_params;		///< Deep copy of user's params
	FileParams			m_fil_params;		///< Deep copy of user's params
	std::string			m_pathname;
	std::string			m_pathname2;

	volatile bool		m_bStopRecPlease;
	pthread_t			m_thread_rec;

	// - Shared data; protected via mutex m_mux
	pthread_mutex_t		m_mux;
	int					m_rec_status;		///< PX14RECSTAT_*
	unsigned long		m_elapsed_time_ms;	///< Elapsed rec time in ms
	unsigned long long	m_samps_recorded;	///< Samps recorded so far
	unsigned int		m_xfer_count;		///< Current transfer counter
	px14_sample_t*		m_ss_bufp;			///< Data snapshot buffer
	unsigned int		m_ss_buf_samps;		///< SS buffer size in samples
	unsigned int		m_ss_buf_valid;		///< # valid samples in ss buf
	unsigned int		m_ss_counter;		///< Sata snapshot counter

	// - Used by rec thread only; read by main thread after rec thread
	bool				mt_bSnapshots;		///< Snapshots enabled?
	int					mt_rec_result;		///< SIG_*
	const char*			mt_err_preamble;
	sys_error_t			mt_sys_err_code;
	px14_sample_t*		mt_xbuf1p;			///< Transfer buffer #1
	px14_sample_t*		mt_xbuf2p;			///< Transfer buffer #2

	bool				mt_ss_by_xfers;		///< SS period is in DMA xfers
	unsigned int		mt_ss_period;		///< Xfers or ms
	unsigned int		mt_ss_cur;			///< Xfers left or last ss time

	time_t				mt_time_armed;		///< Time device was armed
	time_t				mt_time_ended;		///< Time rec thread stopped

	SrdcFileList		mt_srdc_list;

	// - Synchronize recording thread start and end
	CSyncEventPX14		m_sync_rec_thread;
	// - Synchronize primary arming of recording
	CSyncEventPX14		m_sync_arm;
};

// -- Concrete recording session classes

/// RAM acquisition and transfer recording
class CPX14RecSes_RamAcq : public CPX14RecSession
{
public:

	CPX14RecSes_RamAcq();

protected:

	virtual int th_RecordMain();
	bool th_SnapshotRamRec (HPX14 hBrd);
};

/// RAM-buffered PCIe acquisition (double-buffer imp)
class CPX14RecSes_PciBuf : public CPX14RecSession
{
public:

	CPX14RecSes_PciBuf();

protected:

	int CheckForBootBuffers();

	virtual int InitRecordingBuffers();
	virtual void PostThreadRun();

	virtual int th_RecordMain();

private:

	bool	m_bCheckIn1;	// Check in boot-buffer mt_xbuf1p?
	bool	m_bCheckIn2;	// Check in boot-buffer mt_xbuf2p?
};

/// RAM-buffered PCIe acquisition (buffer chain imp)
class CPX14RecSes_PciBufChained : public CPX14RecSession
{
public:

	CPX14RecSes_PciBufChained();

	virtual ~CPX14RecSes_PciBufChained();

protected:

	static const unsigned int s_min_chain_samples = 4 * _1mebi;
	static const int s_min_buf_count = 2;

	virtual int InitRecordingBuffers();
	virtual int PreThreadCreate();
	virtual void PostThreadRun();
	virtual int Abort();

	virtual int th_RecordMain(); // -> thd_DmaThread

	// This class implements the recording using two threads:
	//  1) th_RecordMain  = DMA transfer thread
	//  2) thp_ProcThread = Data processing thread

	int thp_ProcThread();
	void thp_RuntimeError (int res, const char* descp);

	int thd_DmaThread();
	void thd_RuntimeError (int res, const char* descp);

private:

	static void* th2_raw(void* paramp);

	volatile bool		m_thp_stop_please;

	CSemaphorePX14		m_semDma;
	CSemaphorePX14		m_semProc;
	PX14S_BUFNODE*		m_bufList;

	// DMA thread state
	const char*			m_thd_err_str;		// Set by thread on error
	sys_error_t			m_thd_lastError;	// Set by thread on error
	int					m_thd_res;			// Result of thread (SIG_*)
	volatile int		m_thd_bufIdxStop;

	// Processing thread state
	bool				m_thp_thread_is_alive;
	pthread_t			m_thp_thread;

	// - Synchronize proc thread start and end
	CSyncEventPX14		m_sync_proc_start;
	CSyncEventPX14		m_sync_proc_end;
};

#endif // __px14_record_header_defined


