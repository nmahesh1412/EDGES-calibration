/** @file	sig_srd_file.h
	@brief	Internal API for Signatec Recorded Data Context files (*.srdc)
*/
#ifndef sig_srd_file_header_defined
#define sig_srd_file_header_defined

#include <libxml/parser.h>
#include <libxml/xmlsave.h>
#include <libxml/xpath.h>

// -- Forward declarations
class CAutoXmlDocPtr;

// -- Item Value Flags (IVF_*)
/// Item has been written to file; used for organizing output
#define IVF_SAVED							0x00000001
/// Item has been modified since last save
#define IVF_MODIFIED						0x00000002
#define IVF__DEFAULT						0

// -- SRDC enumeration flags (SIGSRDCENUM_*)
/// Skip standard SRDC items; use to obtain only user-defined items
#define SIGSRDCENUM_SKIP_STD				0x00000001
/// Skip user-defined SRDC items; use to obtain only standard items
#define SIGSRDCENUM_SKIP_USER_DEFINED		0x00000002
/// Only include modified items
#define SIGSRDCENUM_MODIFIED_ONLY			0x00000004

class CSigRecDataFile
{
public:

	// -- Construction

	CSigRecDataFile();

	// -- Operations

	// Load SRDC file
	virtual bool LoadFrom (const char* pathnamep,
		bool bResetContent = true);
	// Save SRDC file
	virtual bool SaveTo (const char* pathnamep = NULL);

	virtual bool EnumItems (std::string& str_enum, unsigned int flags);

	// Reset all content
	virtual void ResetContent();

	// Add or modify an item name/value pair to SRDC data
	bool RefreshItem (const char* itemName, const char* itemValue);
	// Returns true if we've got an item with given name in SRDC data
	bool ItemExists(const char* itemName) const;
	// Returns true if named item found
	bool GetItemValue (const char* itemName, std::string& itemValue) const;

	// -- Implementation

	virtual ~CSigRecDataFile();

	// Sets error text and always returns false
	bool _Failed(const char* reason = NULL, 
		bool bAppendXmlErroInfo = false);

	// Generate XML content for SRDC output
	virtual bool CreateOutputXml(CAutoXmlDocPtr& doc);

	// Initialize known SRDC file parameters
	virtual bool RefreshKnownParameters();

	// Append item to root and mark as saved
	bool OutputItem (xmlNodePtr rootp, const char* itemName);
	// Append all unsaved items to root in map order
	bool OutputUnsavedItems(xmlNodePtr rootp);

#ifdef _WIN32
	/// Read file into class-allocated buffer; free with 'free'
	bool BufferFileInMemory (const char* pathnamep, 
		void** bufpp, unsigned* bytesp);
#endif
	
	// -- Local data types

	/// Predefined acquisition board parameters
	class AcqDeviceParams
	{
	public:

		AcqDeviceParams() : serialNum(0), channelCount(1), channelNum(0),
			channelMask(0),
			sampSizeBytes(0), sampSizeBits(0), bSignedSamples(0),
			sampleRateMHz(0), inputVoltRngPP(0), 
			segment_size(0), pretrig_samples(0), delaytrig_samples(0)
		{
		}

		AcqDeviceParams (const AcqDeviceParams& cpy);
		AcqDeviceParams& operator= (const AcqDeviceParams& cpy);

		// -- Source board information

		std::string		boardName;			// SourceBoard
		unsigned		serialNum;			// SourceBoardSerialNum

		// -- Data sample information

		unsigned		channelCount;		// ChannelCount
		unsigned		channelNum;			// ChannelId
		unsigned		channelMask;		// ChannelMask
		unsigned		sampSizeBytes;		// SampleSizeBytes 
		unsigned		sampSizeBits;		// SampleSizeBits
		int				bSignedSamples;		// SampleFormat

		// -- Data acquisition information

		double			sampleRateMHz;		// SamplingRateMHz
		double			inputVoltRngPP;		// PeakToPeakInputVoltRange

		unsigned		segment_size;		// SegmentSize
		unsigned		pretrig_samples;	// PreTriggerSampleCount
		unsigned		delaytrig_samples;	// TriggerDelaySamples

	};

	// Other standard items not directly specified by this class
	// FileFormat(Binary,Text), SampleRadix(10,16)
	// HeaderBytes, OperatorNotes

	typedef struct _ItemValue_tag
	{
		std::string		itemValue;
		unsigned		flags;			// IVF_*

		// -- Implementation

		_ItemValue_tag() : flags(IVF__DEFAULT) {}
		_ItemValue_tag(const _ItemValue_tag& cpy) : 
			itemValue(cpy.itemValue), flags(cpy.flags) {}

		_ItemValue_tag& operator= (const _ItemValue_tag& cpy)
		{
			itemValue = cpy.itemValue;
			flags = cpy.flags;

			return *this;
		}

	} ItemValue;

	/// Map a file attribute to it's value
	typedef std::map<std::string, ItemValue> ValueMap;

	// Fill in given AcqDeviceParams with current data
	bool GetAcqDeviceParamsFromData(AcqDeviceParams& adp);

	bool IsStdItem (const std::string& sItem) const;

	// -- Public members

	std::string			m_pathname;
	std::string			m_errText;
	ValueMap			m_valMap;
	bool				m_bModified;

protected:

	virtual bool GetPredefinedParameters (AcqDeviceParams& adp);
};

#endif // sig_srd_file_header_defined

