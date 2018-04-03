/** @file px14_timestamp.h
*/
#ifndef _px14_timestamp_header_defined
#define _px14_timestamp_header_defined

// -- Timestamp manager implementation flags (TSMGRF_*)
/// We've created the timestamp thread
#define TSMGRF_THREAD_CREATED				0x00000001
/// Timestamp thread has been armed
#define TSMGRF_THREAD_ARMED					0x00000002
#define TSMGRF__DEFAULT						0

/// Timestamp thread state
class CTimestampMgrPX14
{
public:

	// -- Construction

	CTimestampMgrPX14();
	
	// -- Methods

	/// Initialize object; creates but does not arm timestamp thread
	int Init (HPX14 hBrd, const char* ts_pathnamep, unsigned int flags);

	/// Arm the timestamp thread; begins reading from timestamp FIFO
	int ArmTimestampThread();

	void StopTimestampThread();

	// -- Attributes

	// Returns true if timestamp FIFO has overflowed at any point
	bool TimestampFifoOverflowed();

	// Returns total number of timestamps processed; not threadsafe
	unsigned GetTimestampCount();

	// -- Implementation

	virtual ~CTimestampMgrPX14();

	// - Binary file output
	int _IoInit_Binary();
	int _IoDump_Binary (const px14_timestamp_t* bufp, unsigned nItems);
	int _IoCleanup_Binary();

	// - Text file output
	int _IoInit_Text();
	int _IoDump_Text (const px14_timestamp_t* bufp, unsigned nItems);
	int _IoCleanup_Text();

	typedef int (CTimestampMgrPX14::*pmfIoInit_t) ();
	typedef int (CTimestampMgrPX14::*pmfIoDump_t) (const px14_timestamp_t*,
		unsigned);
	typedef int (CTimestampMgrPX14::*pmfIoCleanup_t) ();

	class CAutoIoCleanup
	{
		typedef CTimestampMgrPX14::pmfIoCleanup_t _fnc;

		CTimestampMgrPX14&	m_tsm;
		_fnc				m_pmf;

	public:

		CAutoIoCleanup(CTimestampMgrPX14& tsm, _fnc pmf = NULL) :
		  m_tsm(tsm), m_pmf(pmf)
		{
		}

		~CAutoIoCleanup() { Release(); }

		void ReInit (_fnc pmf) { Release(); m_pmf = pmf; }

		void Release() { if (m_pmf) { (m_tsm.*m_pmf)(); m_pmf = NULL; } }
	};

protected:

	/// Threadsafe: Obtains current thread state (PX14RECSTAT_*)
	int GetThreadStatus();

	/// Raw timestamp thread function; calls instanced TimestampThread
	static void* raw_ts_thread_func(void* paramp);

	// - Virtual overrides

	virtual int TimestampThread();
	virtual void Cleanup();

	// -- Members

	std::string			m_ts_filename;	///< Timestamp pathname
	unsigned int		m_ts_flags;		///< PX14FILWF_*
	HPX14				m_hBrdMain;		///< Main thread device handle
	unsigned int		m_imp_flags;	///< TSMGRF_*

	volatile bool		m_bStopPlease;
	pthread_t			m_thread_ts;		///< Timestamp thread

	CSyncEventPX14		m_sync_ts_start;	///< Sync thread start/stop
	CSyncEventPX14		m_sync_ts_arm;		///< Sync arming of thread
	CSyncEventPX14		m_sync_ts_quit;		///< Sync quit request

	// - Timestamp thread data
	HPX14				mt_hBrd;
	int					mt_ts_result;		///< SIG_*
	const char*			mt_err_preamble;
	FILE*				mt_filp_bin;		///< File for binary output
	std::ofstream		mt_file_txt;		///< File for text output
	unsigned int		mt_ts_save_count;	///< Number of timestamps saved
	volatile bool		mt_bFifoOverflow;	///< Set if TS FIFO ever fills

	// Shared data protected by m_mux
	pthread_mutex_t		m_mux;
	int					m_ts_status;		///< PX14RECSTAT_*
};

#endif // _px14_timestamp_header_defined

