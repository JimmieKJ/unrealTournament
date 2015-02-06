
/************************************************************************************

Filename    :   VrCommon.h
Content     :   Unorganized common code
Created     :   Septembet 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_VrCommon_h
#define OVR_VrCommon_h

#include "OVR.h"	// Matrix4f, etc
#include "Kernel/OVR_StringHash.h"
#include "PathUtils.h"

namespace OVR {

// Debug tool
void LogMatrix( const char * title, const Matrix4f & m );

inline Vector3f GetViewMatrixPosition( Matrix4f const & m )
{
#if 1
	return m.Inverted().GetTranslation();
#else
	// This is much cheaper if the view matrix is a pure rotation plus translation.
	return Vector3f(	m.M[0][0] * m.M[0][3] + m.M[1][0] * m.M[1][3] + m.M[2][0] * m.M[2][3],
						m.M[0][1] * m.M[0][3] + m.M[1][1] * m.M[1][3] + m.M[2][1] * m.M[2][3],
						m.M[0][2] * m.M[0][3] + m.M[1][2] * m.M[1][3] + m.M[2][2] * m.M[2][3] );
#endif
}

inline Vector3f GetViewMatrixForward( Matrix4f const & m )
{
	return Vector3f( -m.M[2][0], -m.M[2][1], -m.M[2][2] ).Normalized();
}


// Determine what part of a -1 to 1 range panel with panelMatrix is centered
// in front of viewMatrix.
//
// -1 to 1 range on panelMatrix, returns -2,-2 if looking away from the panel
Vector2f	GazeCoordinatesOnPanel( const Matrix4f & viewMatrix, const Matrix4f panelMatrix, const bool floatingPanel );

// Returns true if the folder has specified permission
bool HasPermission( const char * fileOrDirName, mode_t mode );

// Returns true if the file exists
bool FileExists( const char * filename );

// Returns true if ext is the end of fileName
bool MatchesExtension( const char * fileName, const char * ext );

void WriteJpeg( const char * fullName, const unsigned char * rgbxBuffer, int width, int height );

// Drop-in replacement for stbi_load_from_memory(), but without component specification.
// Often 2x - 3x faster.
unsigned char * TurboJpegLoadFromMemory( const unsigned char * jpg, const int length, int * width, int * height );

// Uses mmap files and above to replace stbi_load() for jpgs
unsigned char * TurboJpegLoadFromFile( const char * filename, int * width, int * height );

int StringCompare( const void *a, const void * b );

void SortStringArray( Array<String> & strings );

StringHash< String > RelativeDirectoryFileList( const Array< String > & searchPaths, const char * RelativeDirPath );

// DirPath should by a directory with a trailing slash.
// Returns all files in the directory, already prepended by root.
// Subdirectories will have a trailing slash.
// All files and directories that start with . are skipped.
Array<String> DirectoryFileList( const char * DirPath );

// Returns the last token in path s with slashes and file extension stripped
String ExtractFileBase( const String & s );

// Returns the filename with extension from a passed in path
String ExtractFile( const String & s );

// Returns the directory name before the fileName - stripping out parent directories and file
String ExtractDirectory( const String & s );

// Creates all the intermediate directories if they don't exist
void MakePath( const char * dirPath, mode_t mode );

// Returns true if head equals check plus zero or more characters.
bool MatchesHead( const char * head, const char * check );

float LinearRangeMapFloat( float inValue, float inStart, float inEnd, float outStart, float outEnd );

}	// namespace OVR

#endif	// OVR_VrCommon_h
