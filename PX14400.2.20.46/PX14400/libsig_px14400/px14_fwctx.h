/** @file px14_fwctx.h
*/
#ifndef _px14_fwctx_header_defined
#define _px14_fwctx_header_defined

// -- PX14 firmware chunk flags (PX14FWCF_*)
/// This is prerelease firmware
#define PX14FWCF_PRERELEASE					0x00000001
/// System requires a full shutdown and powerup for changes to have effect
#define PX14FWCF_REQUIRES_SHUTDOWN			0x00000002
/// System requires a reboot for changes to have effect
#define PX14FWCF_REQUIRES_REBOOT			0x00000004
#define PX14FWCF__DEFAULT					0

// -- PX14 firmware chunk IDs (PX14FWCHUNKID_*)
/// First PCIe FPGA EEPROM - Virtex 5 LX50T
#define PX14FWCHUNKID_SYS_V5LX50T_1			0
/// Second PCIe FPGA EEPROM - Virtex 5 LX50T; should be EEPROM erase only
#define PX14FWCHUNKID_SYS_V5LX50T_2			1
/// First SAB FPGA EEPROM - Virtex 5 SX50T
#define PX14FWCHUNKID_SAB_V5SX50T_1			2
/// Second SAB FPGA EEPROM - Virtex 5 SX50T; should be EEPROM erase only
#define PX14FWCHUNKID_SAB_V5SX50T_2			3
/// First SAB FPGA EEPROM - Virtex 5 SX95T
#define PX14FWCHUNKID_SAB_V5SX95T_1			4
/// Second SAB FPGA EEPROM - Virtex 5 SX95T
#define PX14FWCHUNKID_SAB_V5SX95T_2			5
#define PX14FWCHUNKID__COUNT				6

// Any firmware file that has this prefix is assumed to an EEPROM erase
//  firmware file (as in instruction to erase the specific EEPROM). Since
//  upload times can be a little long, the library implements an 
//  optimization that skips an 'erase' firmware file upload if the EEPROM
//  is already blank.
#define PX14_BLANK_FW_PREFIX				"erase_"

// Embedded firmware readme name
#define PX14FW2_FW_NOTES_NAME				"fw_readme.txt"

// Start of custom firmware enumeration values for third-party providers
#define PX14_CUSTOM_FW_PROVIDER_ENUM_START	1000
// Note this isn't a hard rule, we do have a customer ID of 10 in existence

/// PX14 firmware update file context - version 2
class CFwContextPX14
{
public:

	// -- Construction

	CFwContextPX14();

	CFwContextPX14(const CFwContextPX14& cpy);

	// -- Methods

	// Parse given PX14 firmware context file
	virtual int ParseFrom (const char* ctx_pathp);

	// Write firmware context file with current data
	virtual int WriteTo (const char* out_pathp);

	// Reset state to default values
	virtual void ResetState();

	// -- Implementation

	virtual ~CFwContextPX14();

	CFwContextPX14& operator= (const CFwContextPX14& cpy);

	int ParseFirmwareChunk (void* xml_doc_ptr, void* xml_node_ptr);
	int ParseUserData (void* xml_doc_ptr);

	int DumpUserData (std::ostream& out);
	int DumpFwChunks (std::ostream& out);

	// -- Type defintions

	typedef std::pair<std::string, std::string> UserCtxPair;
	typedef std::list<UserCtxPair> UserDataList;
	typedef std::list<std::string> FileList;

	class CFirmwareChunk
	{
	public:

		// -- Construction

		CFirmwareChunk();
		CFirmwareChunk(const CFirmwareChunk& cpy);

		// -- Implementation

		virtual ~CFirmwareChunk();

		CFirmwareChunk& operator= (const CFirmwareChunk& cpy);

		// -- Public members

		unsigned int	fwc_version;	///< Firmware chunk version
		unsigned int	fwc_cust_enum;	///< Custom firmware enumeration
		unsigned int	fwc_flags;		///< PX14FWCF_*
		int				fwc_file_count;	///< Number of XSVF files
		FileList		fwc_file_list;	///< List of XSVF files
	};

	/// Map a firmware chunk ID to its state
	typedef std::map<int, CFirmwareChunk> FwChunkMap;

	// -- Public members

	// - Top-level firmware information

	unsigned int		fw_ver_pkg;		///< Firmware package version
	unsigned int		pkg_cust_enum;	///< Custom firmware package enum
	unsigned int		ucd;			///< Signatec UCD# of logic release
	std::string			rel_date;		///< FW release date
	unsigned int		fw_flags;		///< PX14FWCTXF_*

	FwChunkMap			fw_chunk_map;	///< Firmware chunks

	// - Hardware/software requirements for this firmware

	int					brd_rev;		///< PX14BRDREV_*
	unsigned short		cust_hw;		///< Custom hardware enumeration
	unsigned			max_hw_rev;		///< Maximum hardware revision
	unsigned			min_hw_rev;		///< Minimum hardware revision
	unsigned long long	min_sw_ver;		///< Minimum software release

	// - Readme info

	int					readme_sev;		///< PX14FWNOTESEV_*

	// - Other, auxiliary files to embed in firmware update file

	int					aux_file_count;
	FileList			aux_file_list;

	// - User defined context items

	UserDataList		userDataLst;
};

/// A helper class for working with custom FPGA processing providers
class CCustomLogicProviderPX14
{
public:

	// -- Construction

	CCustomLogicProviderPX14();
	CCustomLogicProviderPX14(const CCustomLogicProviderPX14& cpy);

	// -- Methods

	// Parse given custom logic provider context file
	virtual int ParseFrom (const char* pathnamep);

	// Reset all state to initial conditions
	virtual void ResetState();

	// -- Implementation

	CCustomLogicProviderPX14& operator= (const CCustomLogicProviderPX14& cpy);

	virtual ~CCustomLogicProviderPX14();

	// -- Public members

	std::string		m_guidStr;			// Ex: {00000000-0000-0000-0000-000000000000}
	std::string		m_clientName;		// Ex: Signatec, Inc
	std::string		m_shortName;		// Ex: Signatec
	unsigned short	m_custFwEnum;		// Ex: 1001
};

// Parse string into 32-bit version a.b.c.d
bool ParseStrVer32 (const std::string& str, unsigned& ver);
// Parse string into 64-bit version a.b.c.d
bool ParseStrVer64 (const std::string& str, unsigned long long& ver);


#endif // _px14_fwctx_header_defined


