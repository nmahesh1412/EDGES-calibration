/** @file	px14_zip.h
	@brief	Interface to PX14 zip/unzip routines
*/
#ifndef _px14_zip_header_defined
#define _px14_zip_header_defined

/// A list of {PathName, Name}
typedef std::list<std::pair<std::string, std::string> > ZipFilesList;

// Unzip a file from an archive and copy to destination
int UnzipFile (const char* zipp, const char* srcp, const char *dstp);

// Create a zip file and add files
int CreateZip (const char* zipp, const ZipFilesList& files);

// Obtain list of all files embedded in zip file
int EnumZipFiles (const char* zipp, std::list<std::string>& files);


#endif // _px14_zip_header_defined


