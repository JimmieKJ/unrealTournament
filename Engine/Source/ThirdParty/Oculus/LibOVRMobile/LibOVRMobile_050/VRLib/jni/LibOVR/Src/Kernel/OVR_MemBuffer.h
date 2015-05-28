/************************************************************************************

Filename    :   MemBuffer.h
Content     :	Memory buffer.
Created     :	May 13, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef MEMBUFFER_H
#define MEMBUFFER_H

namespace OVR {


// This does NOT free the memory on delete, it is just a wrapper around
// memory, and can be copied freely for passing / returning as a value.
// This does NOT have a virtual destructor, so if a copy is made of
// a derived version, it won't cause it to be freed.
class MemBuffer
{
public:
	MemBuffer() : Buffer( 0 ), Length( 0 ) {}
	MemBuffer( const void * buffer, int length ) : Buffer( buffer), Length( length ) {}
	explicit MemBuffer( int length );
	~MemBuffer();

	// Calls Free() on Buffer and sets it to NULL and lengrh to 0
	void FreeData();

	bool WriteToFile( const char * filename );

	const void *	Buffer;
	int		Length;
};

// This DOES free the memory on delete.
class MemBufferFile : public MemBuffer
{
public:
	enum eNoInit {
		NoInit
	};

	explicit MemBufferFile( const char * filename );
	explicit MemBufferFile( eNoInit const noInit );	// use this constructor as MemBufferFile( NoInit ) -- this takes a parameters so you still can't defalt-construct by accident
	virtual ~MemBufferFile();

	bool LoadFile( const char * filename );

	// Moves the data to a new MemBuffer that won't
	// be deleted on destruction, removing it from the
	// MemBufferFile.
	MemBuffer ToMemBuffer();
};

class MemBufferAPK
{

};

}	// namespace OVR

#endif // MEMBUFFER_H
