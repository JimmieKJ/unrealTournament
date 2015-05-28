/************************************************************************************

Filename    :   OVR_MappedFile.h
Content     :   Cross-platform memory-mapped file wrapper.
Created     :   May 12, 2014
Authors     :   Chris Taylor

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#ifndef OVR_MappedFile_h
#define OVR_MappedFile_h

#include "OVR_Types.h"

#ifdef OVR_OS_WIN32
#include <windows.h>
#endif

/*
	Memory-mapped files are a fairly good compromise between performance and flexibility.

	Compared with asynchronous io, memory-mapped files are:
		+ Much easier to implement in a portable way
		+ Automatically paged in and out of RAM
		+ Automatically read-ahead cached

	When asynch IO is not available or blocking is acceptable then this is a
	great alternative with low overhead and similar performance.

	For random file access, use MappedView with a MappedFile that has been
	opened with random_access = true.  Random access is usually used for a
	database-like file type, which is much better implemented using asynch IO.
*/

namespace OVR
{

// Read-only memory mapped file
class MappedFile
{
	friend class MappedView;

public:
					MappedFile();
					~MappedFile();

	// Opens the file for shared read-only access with other applications
	// Returns false on error (file not found, etc)
	bool			OpenRead( const char * path, bool read_ahead = false, bool no_cache = false );

	// Creates and opens the file for exclusive read/write access
	bool			OpenWrite( const char * path, UPInt size );

	void			Close();

	bool			IsReadOnly() const { return ReadOnly; }
	UPInt			GetLength() const { return Length; }
	bool			IsValid() const { return ( Length != 0 ); }

private:
#if defined( OVR_OS_WIN32 )
	HANDLE			File;
#else
	int				File;
#endif

	bool			ReadOnly;
    UPInt			Length;
};


// View of a portion of the memory mapped file
class MappedView
{
public:
					MappedView();
					~MappedView();

	bool			Open( MappedFile * file ); // Returns false on error
	UByte *			MapView( UPInt offset = 0, UInt32 length = 0 ); // Returns 0 on error, 0 length means whole file
	void			Close();

	bool			IsValid() const { return ( Data != 0 ); }
	UPInt			GetOffset() const { return Offset; }
	UInt32			GetLength() const { return Length; }
	MappedFile *	GetFile() { return File; }
	UByte *			GetFront() { return Data; }

private:
#if defined( OVR_OS_WIN32 )
	HANDLE			Map;
#else
	void *			Map;
#endif

	MappedFile *	File;
	UByte *			Data;
	UPInt			Offset;
	UInt32			Length;
};

} // namespace OVR

#endif // OVR_MappedFile_h
