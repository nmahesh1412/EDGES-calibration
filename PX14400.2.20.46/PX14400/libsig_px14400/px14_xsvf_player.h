/** @file px14_xsvf_player.h
*/
#ifndef px14_xsvf_player_header_defined
#define px14_xsvf_player_header_defined

#include "sig_xsvf_player.h"
#include "px14.h"

class CMyXsvfPlayer : public CXsvfFilePlayer
{
public:

	// -- Construction

	CMyXsvfPlayer(HPX14 hBrd);

	// -- Attributes

	HPX14 GetBoardHandle() const { return m_hBrd; }

	void SetFlags (unsigned flags);
	unsigned GetFlags() const		{ return m_flags; }

	// -- Methods

	void SetFileCountInfo (int file_cur, int file_total);

	void SetCallback (PX14_FW_UPLOAD_CALLBACK callbackp,
		void* callback_ctx);

	// -- Implementation

	virtual ~CMyXsvfPlayer();

	/// Set the port "p" (TCK, TMS, or TDI) to val (0 or 1)
	virtual void setPort(short p, short val);
	/// Read the TDO bit and store it in val
	virtual unsigned char readTDOBit();
	/// Make clock go down->up->down
	virtual void pulseClock();
	/// Delay/stall
	virtual void waitTime (int microsec);

	virtual int xsvfInitialize (SXsvfInfo* pXsvfInfo);
	virtual void xsvfCleanup (SXsvfInfo* pXsvfInfo);
	virtual void xsvfProgress (int cur, int total);
	virtual void xsvfTmsTransition (short sTms);
	virtual void xsvfShiftOnly (long lNumBits, lenVal *plvTdi, 
		lenVal *plvTdoCaptured, int iExitShift);

private:

	HPX14						m_hBrd;
	unsigned					m_flags;		///< PX14XSVFF_*

	PX14_FW_UPLOAD_CALLBACK		m_callbackp;
	void*						m_callback_ctx;

	int							m_nFileCur;
	int							m_nFileTotal;
};


#endif // px14_xsvf_player_header_defined


