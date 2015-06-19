/************************************************************************************

Filename    :   PackageFiles.cpp
Content     :   Read files from the application package zip
Created     :   August 18, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "PackageFiles.h"

#include "Android/LogUtils.h"

#include "3rdParty/minizip/unzip.h"
#include "GlTexture.h"
#include "ModelFile.h"

namespace OVR
{

static	unzFile			packageZipFile = 0;

OvrApkFile::OvrApkFile( void * zipFile ) : 
	ZipFile( zipFile ) 
{ 
}

OvrApkFile::~OvrApkFile()
{
	ovr_CloseOtherApplicationPackage( ZipFile );
}

void *	ovr_GetApplicationPackageFile()
{
	return packageZipFile;
}

void ovr_OpenApplicationPackage( const char * packageCodePath )
{
	if ( packageZipFile )
	{
		return;
	}
	packageZipFile = ovr_OpenOtherApplicationPackage( packageCodePath );
}

void* ovr_OpenOtherApplicationPackage( const char * packageCodePath )
{
	void * zipFile = unzOpen( packageCodePath );

// enable the following block if you need to see the list of files in the application package
// This is useful for finding a file added in one of the res/ sub-folders (necesary if you want
// to include a resource file in every project that links VRLib).
#if 0
	// enumerate the files in the package for us so we can see if the VRLib res/raw files are in there
	if ( unzGoToFirstFile( zipFile ) == UNZ_OK )
	{
		LOG( "FilesInPackage", "Files in package:" );
		do
		{
			unz_file_info fileInfo;
			char fileName[512];
			if ( unzGetCurrentFileInfo( zipFile, &fileInfo, fileName, sizeof( fileName ), NULL, 0, NULL, 0 ) == UNZ_OK )
			{
				LOG( "FilesInPackage", "%s", fileName );
			}
		} while ( unzGoToNextFile( zipFile ) == UNZ_OK );
	}
#endif
	return zipFile;
}

void ovr_CloseOtherApplicationPackage( void * & zipFile )
{
	if ( zipFile == 0 )
	{
		return;
	}
	unzClose( zipFile );
	zipFile = 0;
}

bool ovr_PackageFileExists( const char * nameInZip )
{
	return ovr_OtherPackageFileExists( packageZipFile, nameInZip );
}

bool ovr_OtherPackageFileExists( void* zipFile, const char * nameInZip )
{
	const int locateRet = unzLocateFile( zipFile, nameInZip, 2 /* case insensitive */ );
	if ( locateRet != UNZ_OK )
	{
		LOG( "File '%s' not found in apk!", nameInZip );
		return false;
	}

	const int openRet = unzOpenCurrentFile( zipFile );
	if ( openRet != UNZ_OK )
	{
		WARN( "Error opening file '%s' from apk!", nameInZip );
		return false;
	}

	unzCloseCurrentFile( zipFile );

	return true;
}

bool ovr_ReadFileFromApplicationPackage( const char * nameInZip, int & length, void * & buffer )
{
	return ovr_ReadFileFromOtherApplicationPackage( packageZipFile, nameInZip, length, buffer );
}

bool ovr_ReadFileFromApplicationPackage( const char * nameInZip, MemBufferFile & memBufferFile )
{
	memBufferFile.FreeData();

	int length = 0;
	void * buffer = NULL;

	if ( !ovr_ReadFileFromOtherApplicationPackage( packageZipFile, nameInZip, length, buffer ) )
	{
		return false;
	}

	memBufferFile.Buffer = buffer;
	memBufferFile.Length = length;

	return true;
}

// This is DEFINITELY NOT thread safe! 
bool ovr_ReadFileFromOtherApplicationPackage( void * zipFile, const char * nameInZip, int & length, void * & buffer )
{
	length = 0;
	buffer = NULL;
	if ( zipFile == 0 )
	{
		return false;
	}

	const int locateRet = unzLocateFile( zipFile, nameInZip, 2 /* case insensitive */ );
	if ( locateRet != UNZ_OK )
	{
		LOG( "File '%s' not found in apk!", nameInZip );
		return false;
	}

	unz_file_info	info;
	const int getRet = unzGetCurrentFileInfo( zipFile, &info, NULL,0, NULL,0, NULL,0);
	if ( getRet != UNZ_OK )
	{
		WARN( "File info error reading '%s' from apk!", nameInZip );
		return false;
	}
	const int openRet = unzOpenCurrentFile( zipFile );
	if ( openRet != UNZ_OK )
	{
		WARN( "Error opening file '%s' from apk!", nameInZip );
		return false;
	}

	length = info.uncompressed_size;
	buffer = malloc( length );

	const int readRet = unzReadCurrentFile( zipFile, buffer, length );
	if ( readRet <= 0 )
	{
		WARN( "Error reading file '%s' from apk!", nameInZip );
		free( buffer );
		length = 0;
		buffer = NULL;
		return false;
	}

	unzCloseCurrentFile( zipFile );

	return true;
}

unsigned int LoadTextureFromApplicationPackage( const char * nameInZip, const TextureFlags_t & flags, int & width, int & height )
{
	return LoadTextureFromOtherApplicationPackage( packageZipFile, nameInZip, flags, width, height );
}

unsigned int LoadTextureFromOtherApplicationPackage( void * zipFile, const char * nameInZip, const TextureFlags_t & flags, int & width, int & height )
{
	width = 0;
	height = 0;
	if ( zipFile == 0 )
	{
		return 0;
	}

	void * 	buffer;
	int		bufferLength;

	ovr_ReadFileFromOtherApplicationPackage( zipFile, nameInZip, bufferLength, buffer );
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
	return LoadModelFileFromOtherApplicationPackage( packageZipFile, fileName, programs, materialParms );
}

ModelFile * LoadModelFileFromOtherApplicationPackage( void * zipFile, const char* fileName,
		const ModelGlPrograms & programs, const MaterialParms & materialParms )
{
	void * 	buffer;
	int		bufferLength;

	ovr_ReadFileFromOtherApplicationPackage( zipFile, fileName, bufferLength, buffer );
	if ( buffer == NULL )
	{
		WARN( "Failed to load model file '%s' from apk", fileName );
		return NULL;
	}

	ModelFile * scene = LoadModelFileFromMemory( fileName,
				buffer, bufferLength,
				programs, materialParms );

	free( buffer );
	return scene;
}


} // namespace OVR
