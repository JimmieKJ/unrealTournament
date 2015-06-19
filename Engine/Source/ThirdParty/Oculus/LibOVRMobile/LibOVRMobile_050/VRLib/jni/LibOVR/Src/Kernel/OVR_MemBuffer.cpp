/************************************************************************************

Filename    :   MemBuffer.cpp
Content     :	Memory buffer.
Created     :	May 13, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_MemBuffer.h"

#include <stdio.h>
#include <stdlib.h>

#include "OVR_Log.h"

namespace OVR
{

bool MemBuffer::WriteToFile( const char * filename )
{
	LogText( "Writing %i bytes to %s", Length, filename );
	FILE * f = fopen( filename, "wb" );
	if ( f != NULL )
	{
		fwrite( Buffer, Length, 1, f );
		fclose( f );
		return true;
	}
	else
	{
		LogText( "MemBuffer::WriteToFile failed to write to %s", filename );
	}
	return false;
}

MemBuffer::MemBuffer( int length ) : Buffer( malloc( length ) ), Length( length )
{
}

MemBuffer::~MemBuffer()
{
}

void MemBuffer::FreeData()
{
	if ( Buffer != NULL )
	{
		free( (void *)Buffer );
		Buffer = NULL;
		Length = 0;
	}
}

MemBufferFile::MemBufferFile( const char * filename )
{
	LoadFile( filename );
}

bool MemBufferFile::LoadFile( const char * filename )
{
	FreeData();

	FILE * f = fopen( filename, "rb" );
	if ( !f )
	{
		LogText( "Couldn't open %s", filename );
		Buffer = NULL;
		Length = 0;
		return false;
	}
	fseek( f, 0, SEEK_END );
	Length = ftell( f );
	fseek( f, 0, SEEK_SET );
	Buffer = malloc( Length );
	const int readRet = fread( (unsigned char *)Buffer, 1, Length, f );
	fclose( f );
	if ( readRet != Length )
	{
		LogText( "Only read %i of %i bytes in %s", readRet, Length, filename );
		Buffer = NULL;
		Length = 0;
		return false;
	}
	return true;
}

MemBufferFile::MemBufferFile( eNoInit const noInit )
{
}

MemBuffer MemBufferFile::ToMemBuffer()
{
	MemBuffer	mb( Buffer, Length );
	Buffer = NULL;
	Length = 0;
	return mb;
}

MemBufferFile::~MemBufferFile()
{
	FreeData();
}

}	// namespace OVR
