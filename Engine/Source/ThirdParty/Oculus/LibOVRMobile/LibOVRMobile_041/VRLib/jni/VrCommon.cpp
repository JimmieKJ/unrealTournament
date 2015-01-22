/************************************************************************************

Filename    :   VrCommon.cpp
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrCommon.h"

#include <pthread.h>
#include <sched.h>
#include <unistd.h>			// for usleep
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

#include "GlUtils.h"		// for egl declarations
#include "Log.h"
#include "3rdParty/turbojpeg.h"
#include "MemBuffer.h"


namespace OVR {

void LogMatrix( const char * title, const Matrix4f & m )
{
	LOG( "%s:", title );
	for ( int i = 0; i < 4; i++ )
	{
		LOG("%6.3f %6.3f %6.3f %6.3f", m.M[i][0], m.M[i][1], m.M[i][2], m.M[i][3] );
	}
}


static Vector3f	MatrixOrigin( const Matrix4f & m )
{
	return Vector3f( -m.M[0][3], -m.M[1][3], -m.M[2][3] );
}
static Vector3f	MatrixForward( const Matrix4f & m )
{
	return Vector3f( -m.M[2][0], -m.M[2][1], -m.M[2][2] );
}

// -1 to 1 range on panelMatrix, returns -2,-2 if looking away from the panel
Vector2f	GazeCoordinatesOnPanel( const Matrix4f & viewMatrix, const Matrix4f panelMatrix, const bool floatingPanel )
{
	// project along -Z in the viewMatrix onto the Z = 0 plane of activityMatrix

	const Vector3f viewForward = MatrixForward( viewMatrix ).Normalized();

	Vector3f panelForward;
	float approach;
	if ( floatingPanel )
	{
		Matrix4f mat = panelMatrix;
		mat.SetTranslation( Vector3f( 0.0f ) );
		panelForward = mat.Transform( Vector3f( 0.0f, 0.0f, 1.0f ) ).Normalized();
		approach = viewForward.Dot( panelForward );
		if ( approach >= -0.1 )
		{	// looking away
			return Vector2f( -2.0f, -2.0f );
		}
	}
	else
	{
		panelForward = -MatrixForward( panelMatrix ).Normalized();
		approach = viewForward.Dot( panelForward );
		if ( approach <= 0.1 )
		{	// looking away
			return Vector2f( -2.0f, -2.0f );
		}
	}

	const Matrix4f panelInvert = panelMatrix.Inverted();
	const Matrix4f viewInvert = viewMatrix.Inverted();

	const Vector3f viewOrigin = viewInvert.Transform( Vector3f( 0.0f ) );
	const Vector3f panelOrigin = MatrixOrigin( panelMatrix );

	// Should we disallow using panels from behind?
	const float d = panelOrigin.Dot( panelForward );
	const float t = -( viewOrigin.Dot( panelForward ) + d ) / approach;

	const Vector3f impact = viewOrigin + viewForward * t;
	const Vector3f localCoordinate = panelInvert.Transform( impact );

	return Vector2f( localCoordinate.x, localCoordinate.y );
}

int StringCompare( const void *a, const void * b )
{
	const String *sa = ( String * )a;
	const String *sb = ( String * )b;
	return sa->CompareNoCase( *sb );
}

void SortStringArray( Array<String> & strings )
{
	if ( strings.GetSize() > 1 )
	{
		qsort( ( void * )&strings[ 0 ], strings.GetSize(), sizeof( String ), StringCompare );
	}
}

// DirPath should by a directory with a trailing slash.
// Returns all files in all search paths, as unique relative paths.
// Subdirectories will have a trailing slash.
// All files and directories that start with . are skipped.
StringHash< String > RelativeDirectoryFileList( SearchPaths const & searchPaths, const char * RelativeDirPath )
{
	//Check each of the mirrors in SearchPaths and build up a list of unique strings
	StringHash< String >	uniqueStrings;

	const int numSearchPaths = searchPaths.NumPaths();
	for ( int index = 0; index < numSearchPaths; ++index )
	{
		const String fullPath = String( searchPaths.GetPath( index ) ) + String( RelativeDirPath );

		DIR * dir = opendir( fullPath.ToCStr() );
		if ( dir != NULL )
		{
			struct dirent * entry;
			while ( ( entry = readdir( dir ) ) != NULL )
			{
				if ( entry->d_name[ 0 ] == '.' )
				{
					continue;
				}
				if ( entry->d_type == DT_DIR )
				{
					String s( RelativeDirPath );
					s += entry->d_name;
					s += "/";
					uniqueStrings.SetCaseInsensitive( s, s );
				}
				else if ( entry->d_type == DT_REG )
				{
					String s( RelativeDirPath );
					s += entry->d_name;
					uniqueStrings.SetCaseInsensitive( s, s );
				}
			}
			closedir( dir );
		}
	}

	return uniqueStrings;
}

// DirPath should by a directory with a trailing slash.
// Returns all files in the directory, already prepended by root.
// Subdirectories will have a trailing slash.
// All files and directories that start with . are skipped.
Array<String> DirectoryFileList( const char * DirPath )
{
	Array<String>	strings;

	DIR * dir = opendir( DirPath );
	if ( dir != NULL )
	{
		struct dirent * entry;
		while ( ( entry = readdir( dir ) ) != NULL )
		{
			if ( entry->d_name[ 0 ] == '.' )
			{
				continue;
			}
			if ( entry->d_type == DT_DIR )
			{
				String s( DirPath );
				s += entry->d_name;
				s += "/";
				strings.PushBack( s );
			}
			else if ( entry->d_type == DT_REG )
			{
				String s( DirPath );
				s += entry->d_name;
				strings.PushBack( s );
			}
		}
		closedir( dir );
	}

	SortStringArray( strings );

	return strings;
}

bool FileExists( const char * filename )
{
	FILE *f = fopen( filename, "r" );
	if ( !f )
	{
		return false;
	}
	else
	{
		fclose( f );
		return true;
	}
}

bool MatchesExtension( const char * fileName, const char * ext )
{
	const int extLen = strlen( ext );
	const int sLen = strlen( fileName );
	if ( sLen < extLen + 1 )
	{
		return false;
	}
	return ( 0 == strcmp( &fileName[ sLen - extLen ], ext ) );
}

String ExtractFileBase( const String & s )
{
	const int l = s.GetSize();
	if ( l == 0 )
	{
		return String( "" );
	}

	int	end;
	if ( s[ l - 1 ] == '/' )
	{	// directory ends in a slash
		end = l - 1;
	}
	else
	{
		for ( end = l - 1; end > 0 && s[ end ] != '.'; end-- )
			;
		if ( end == 0 )
		{
			end = l - 1;
		}
	}
	int	start;
	for ( start = end - 1; start > -1 && s[ start ] != '/'; start-- )
		;
	start++;

	return String( &s[ start ], end - start );
}



void WriteJpeg( const char * fullName, const unsigned char * rgbxBuffer, int width, int height )
{
	tjhandle tj = tjInitCompress();

	unsigned char * jpegBuf = NULL;
	unsigned long jpegSize = 0;

	const int r = tjCompress2( tj, ( unsigned char * )rgbxBuffer,	// TJ isn't const correct...
		width, width * 4, height, TJPF_RGBX, &jpegBuf,
		&jpegSize, TJSAMP_444 /* TJSAMP_422 */, 90 /* jpegQual */, 0 /* flags */ );
	if ( r != 0 )
	{
		LOG( "tjCompress2 returned %s for %s", tjGetErrorStr(), fullName );
		return;
	}
	MemBuffer toFile( jpegBuf, jpegSize );
	toFile.WriteToFile( fullName );
	tjFree( jpegBuf );

	tjDestroy( tj );
}


// Drop-in replacement for stbi_load_from_memory(), but without component specification.
// Often 2x - 3x faster.
unsigned char * TurboJpegLoadFromMemory( const unsigned char * jpg, const int length, int * width, int * height )
{
	tjhandle tj = tjInitDecompress();
	int	jpegWidth;
	int	jpegHeight;
	int jpegSubsamp;
	int jpegColorspace;
	const int headerRet = tjDecompressHeader3( tj,
		( unsigned char * )jpg /* tj isn't const correct */, length, &jpegWidth, &jpegHeight,
		&jpegSubsamp, &jpegColorspace );
	if ( headerRet )
	{
		LOG( "TurboJpegLoadFromMemory: header: %s", tjGetErrorStr() );
		tjDestroy( tj );
		return NULL;
	}
	MemBuffer	tjb( jpegWidth * jpegHeight * 4 );

	const int decompRet = tjDecompress2( tj,
		( unsigned char * )jpg, length, ( unsigned char * )tjb.Buffer,
		jpegWidth, jpegWidth * 4, jpegHeight, TJPF_RGBX, 0 /* flags */ );
	if ( decompRet )
	{
		LOG( "TurboJpegLoadFromMemory: decompress: %s", tjGetErrorStr() );
		tjDestroy( tj );
		tjb.FreeData();
		return NULL;
	}

	tjDestroy( tj );

	*width = jpegWidth;
	*height = jpegHeight;

	return ( unsigned char * )tjb.Buffer;
}


unsigned char * TurboJpegLoadFromFile( const char * filename, int * width, int * height )
{
	const int fd = open( filename, O_RDONLY );
	if ( fd <= 0 )
	{
		return NULL;
	}
	struct stat	st = {};
	if ( fstat( fd, &st ) == -1 )
	{
		LOG( "fstat failed on %s", filename );
		close( fd );
		return NULL;
	}

	const unsigned char * jpg = ( unsigned char * )mmap(
		NULL, ( size_t )st.st_size, PROT_READ, MAP_SHARED, fd, 0 );
	if ( jpg == MAP_FAILED )
	{
		LOG( "Failed to mmap %s, len %i: %s", filename, ( int )st.st_size,
			strerror( errno ) );
		close( fd );
		return NULL;
	}
	LOG( "mmap %s, %i bytes at %p", filename, ( int )st.st_size, jpg );

	unsigned char * ret = TurboJpegLoadFromMemory( jpg, ( int )st.st_size, width, height );
	close( fd );
	munmap( ( void * )jpg, ( size_t )st.st_size );
	return ret;
}


}	// namespace OVR

