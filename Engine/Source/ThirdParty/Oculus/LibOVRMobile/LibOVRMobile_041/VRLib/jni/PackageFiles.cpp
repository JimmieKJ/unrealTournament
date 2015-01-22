/************************************************************************************

Filename    :   PackageFiles.cpp
Content     :   Read files from the application package zip
Created     :   August 18, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "PackageFiles.h"
#include "Log.h"
#include "3rdParty/minizip/unzip.h"
#include "GlTexture.h"
#include "ModelFile.h"

namespace OVR
{

static	unzFile			packageZipFile;

void ovr_OpenApplicationPackage( const char * packageCodePath )
{
	if ( packageZipFile )
	{
		return;
	}
	packageZipFile = unzOpen( packageCodePath );

// enable the following block if you need to see the list of files in the application package
// This is useful for finding a file added in one of the res/ sub-folders (necesary if you want
// to include a resource file in every project that links VRLib).
#if 0
	// enumerate the files in the package for us so we can see if the VRLib res/raw files are in there
	if ( unzGoToFirstFile( packageZipFile ) == UNZ_OK )
	{
		LOG( "FilesInPackage", "Files in package:" );
		do
		{
			unz_file_info fileInfo;
			char fileName[512];
			if ( unzGetCurrentFileInfo( packageZipFile, &fileInfo, fileName, sizeof( fileName ), NULL, 0, NULL, 0 ) == UNZ_OK )
			{
				LOG( "FilesInPackage", "%s", fileName );
			}
		} while ( unzGoToNextFile( packageZipFile ) == UNZ_OK );
	}
#endif
}

bool ovr_PackageFileExists( const char * nameInZip )
{
	const int locateRet = unzLocateFile( packageZipFile, nameInZip, 2 /* case insensitive */ );
	if ( locateRet != UNZ_OK )
	{
		LOG( "File '%s' not found in apk!", nameInZip );
		return false;
	}

	const int openRet = unzOpenCurrentFile( packageZipFile );
	if ( openRet != UNZ_OK )
	{
		LOG( "Error opening file '%s' from apk!", nameInZip );
		return false;
	}

	unzCloseCurrentFile( packageZipFile );

	return true;
}

// This is DEFINITELY NOT thread safe! 
void ovr_ReadFileFromApplicationPackage( const char * nameInZip, int &length, void * & buffer )
{
	length = 0;
	buffer = NULL;
	const int locateRet = unzLocateFile( packageZipFile, nameInZip, 2 /* case insensitive */ );
	if ( locateRet != UNZ_OK )
	{
		LOG( "File '%s' not found in apk!", nameInZip );
		return;
	}

	unz_file_info	info;
	const int getRet = unzGetCurrentFileInfo( packageZipFile, &info, NULL,0, NULL,0, NULL,0);
	if ( getRet != UNZ_OK )
	{
		LOG( "File info error reading '%s' from apk!", nameInZip );
		return;
	}
	const int openRet = unzOpenCurrentFile( packageZipFile );
	if ( openRet != UNZ_OK )
	{
		LOG( "Error opening file '%s' from apk!", nameInZip );
		return;
	}

	length = info.uncompressed_size;
	buffer = malloc( length );

	const int readRet = unzReadCurrentFile( packageZipFile, buffer, length );
	if ( readRet <= 0 )
	{
		LOG( "Error reading file '%s' from apk!", nameInZip );
		free( buffer );
		length = 0;
		buffer = NULL;
		return;
	}

	unzCloseCurrentFile( packageZipFile );
}

unsigned int LoadTextureFromApplicationPackage( const char * nameInZip, const TextureFlags_t & flags, int & width, int & height )
{
	void * 	buffer;
	int		bufferLength;

	ovr_ReadFileFromApplicationPackage( nameInZip, bufferLength, buffer );
	if ( !buffer )
	{
		return 0;
	}
	unsigned texId = LoadTextureFromBuffer( nameInZip, MemBuffer( buffer, bufferLength ),
			flags, width, height );
	free( buffer );
	return texId;
}

ModelFile * LoadModelFileFromApplicationPackage( const char* fileName,
		const ModelGlPrograms & programs, const MaterialParms & materialParms )
{
	void * 	buffer;
	int		bufferLength;

	ovr_ReadFileFromApplicationPackage( fileName, bufferLength, buffer );
	if ( buffer == NULL )
	{
		LOG( "Failed to load model file '%s' from apk", fileName );
		return NULL;
	}

	ModelFile * scene = LoadModelFileFromMemory( fileName,
				buffer, bufferLength,
				programs, materialParms );

	free( buffer );
	return scene;
}


} // namespace OVR
