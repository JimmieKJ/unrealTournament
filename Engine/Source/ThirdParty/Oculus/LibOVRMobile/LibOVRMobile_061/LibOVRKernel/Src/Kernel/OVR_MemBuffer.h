/************************************************************************************

Filename    :   MemBuffer.h
Content     :	Memory buffer.
Created     :	May 13, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef MEMBUFFER_H
#define MEMBUFFER_H

#include <inttypes.h>
#include <cstddef>		// for NULL

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

//==============================================================
// MemBufferT
//
// This allocates memory on construction and frees the memory on delete.
// On copy assignment or copy construct any existing pointer in the destination
// is freed, the pointer from the source is assigned to the destination and
// the pointer in the source is cleared.
template< class C >
class MemBufferT
{
public:
	MemBufferT() 
		: Buffer( 0 )
		, Size( 0 ) 
	{ 
	}

	// allocates a buffer of the specified size.
	explicit MemBufferT( size_t const size )
		: Buffer( 0 )
		, Size( size )
	{
		// FIXME: this can throw on a failed allocation and we are currently building
		// without exception handling. This means we can't catch the throw and we'll
		// get an abort. We either need to turn on exception handling or do our own
		// in-place new and delete.
		Buffer = new C[size];
	}

	// take ownership of an already allocated buffer
	explicit MemBufferT( void * & buffer, size_t const size )
		: Buffer( static_cast< C* >( buffer ) )
		, Size( size )
	{
		buffer = 0;
	}

	// frees the buffer on deconstruction
	~MemBufferT()
	{
		Free();
	}

	// returns a const pointer to the buffer
	operator C const * () const { return Buffer; }
	
	// returns a non-const pointer to the buffer
	operator C * () { return Buffer; }

	size_t	GetSize() const { return Size; }

	// assignment operator
	MemBufferT & operator = ( MemBufferT & other )
	{
		if ( &other == this )
		{
			return *this;
		}

		Free();	// free existing data before copying

		Buffer = other.Buffer;
		Size = other.Size;
		other.Buffer = 0;
		other.Size = 0;

		return *this;
	}

	// frees any existing buffer and allocates a new buffer of the specified size
	void Realloc( size_t size )
	{
		Free();

		Buffer = new C[size];
		Size = size;
	}

	// This is for interop with code that uses void * to allocate a raw buffer
	void TakeOwnershipOfBuffer( void * & buffer, size_t const size )
	{
		Buffer = static_cast< C* >( buffer );
		buffer = NULL;
		Size = size;
	}	

	// This is for interop with code that uses void * to allocate a raw buffer
	// and which uses a signed integer for the buffer size.
	void TakeOwnershipOfBuffer( void * & buffer, int const size )
	{
		Buffer = static_cast< C* >( buffer );
		buffer = NULL;
		Size = static_cast< size_t >( size );
	}	

private:
	C *		Buffer;
	size_t	Size;

private:
	// Private copy constructor to prevent MemBufferT objects from being accidentally
	// passed by value. This will also prevent copy-assignment from a temporary object:
	// MemBufferT< uint8_t > buffer = MemBufferT< uint8_t >( size ); // this will fail
	MemBufferT( MemBufferT & other );

	// frees the existing buffer
	void Free()
	{
		delete [] Buffer;
		Buffer = 0;
		Size = 0;
	}
};

}	// namespace OVR

#endif // MEMBUFFER_H
