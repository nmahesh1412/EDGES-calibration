/** @file	px14_sync.h
	@brief	Thread synchronization utility classes
*/
#ifndef __px14_sync_header_defined
#define __px14_sync_header_defined

// Note: pthreads standard does not include semaphore support but our 
//  pthread-win32 library implements the POSIX standard semaphore 
//  interface.
class CSemaphorePX14
{
	sem_t		m_sem;
	bool		m_bInit;		// Semaphore initialized?
public:

	// -- Construction

	CSemaphorePX14() : m_bInit(false) {}

	// -- Implementation

	~CSemaphorePX14() { if (m_bInit) sem_destroy(&m_sem); }

	// -- Methods

	/** @brief              Initialize semaphore; must be called prior to using semaphore
		@param init_count   Initial semaphore count
		@param max_count    Maximum semaphore count
		@retval             Returns SIG_*
	*/
	int Init (int init_count, int max_count)
	{
		SIGASSERT(!m_bInit);
		if (sem_init(&m_sem, 0, static_cast<unsigned int>(init_count)))
			return SIG_PX14_RESOURCE_ALLOC_FAILURE;
		m_bInit = true;
		return SIG_SUCCESS;
	}

	/** @brief              Acquire semaphore (pend/P/down)
		@param ms_timeout   Timeout in milliseconds or 0 for no timeout
		@retval             Returns SIG_*
	*/
	int Acquire (unsigned int ms_timeout = 0)
	{
		int res, ret_val;

		if (ms_timeout)
		{
			struct timespec ts;
			SysRelativeMsToTimespec(ms_timeout, &ts);
			while ((res = sem_timedwait(&m_sem, &ts)) == -1 && errno==EINTR)
				continue;
		}
		else
		{
			while ((res = sem_wait(&m_sem)) == -1 && errno==EINTR)
				continue;
		}

		if (!res)
			ret_val = SIG_SUCCESS;
		else
		{
			switch (errno)
			{
			case ETIMEDOUT: ret_val = SIG_PX14_TIMED_OUT;     break;
			case EINVAL:    ret_val = SIG_INVALIDARG;         break;
			case EPERM:     ret_val = SIG_PX14_ACCESS_DENIED; break;
			default:        ret_val = SIG_ERROR;
			}
		}

		return ret_val;
	}

	/** @brief              Release semaphore (post/V/up)
		@retval             Returns SIG_*
	*/
	int Release(int count = 1)
	{
		for (int i=0; i<count; i++)
			sem_post(&m_sem);
		return SIG_SUCCESS;
	}
};

/// Boolean synchronization event for two threads
class CSyncEventPX14
{
	pthread_cond_t		m_cnd;
	pthread_mutex_t		m_mux;
	bool				m_b;

public:

	// -- Construction

	CSyncEventPX14 (bool bInitState = false) : m_b(bInitState)
	{
		pthread_mutex_init(&m_mux, NULL);
		pthread_cond_init (&m_cnd, NULL);
	}

	// -- Implementation

	~CSyncEventPX14()
	{
		pthread_cond_destroy (&m_cnd);
		pthread_mutex_destroy(&m_mux);
	}

	// -- Methods

	void SetEvent()
	{
		pthread_mutex_lock (&m_mux);
		{
			m_b = true;
			pthread_cond_signal(&m_cnd);
		}
		pthread_mutex_unlock(&m_mux);
	}

	void ClearEvent()
	{
		pthread_mutex_lock (&m_mux);
		{
			m_b = false;
		}
		pthread_mutex_unlock(&m_mux);
	}

	int WaitEvent (unsigned int timeout_ms = 0)
	{
		struct timespec ts_timeout;
		int res, ret_val;

		pthread_mutex_lock (&m_mux);
		{
			if (m_b)
				res = SIG_SUCCESS;
			else
			{
				if (timeout_ms)
				{
					SysRelativeMsToTimespec(timeout_ms, &ts_timeout);
					do { res = pthread_cond_timedwait(&m_cnd, &m_mux, &ts_timeout); }
					while ((!res && !m_b) || (res && errno==EINTR));
				}
				else
				{
					do { res = pthread_cond_wait(&m_cnd, &m_mux); }
					while ((!res && !m_b) || (res && errno==EINTR));
				}
			}
		}
		pthread_mutex_unlock(&m_mux);

		switch (res)
		{
		case 0:         ret_val = SIG_SUCCESS;            break;
		case ETIMEDOUT: ret_val = SIG_PX14_TIMED_OUT;     break;
		case EINVAL:    ret_val = SIG_INVALIDARG;         break;
		case EPERM:     ret_val = SIG_PX14_ACCESS_DENIED; break;
		default:        ret_val = SIG_ERROR;
		}

		return ret_val;
	}
};

#endif

