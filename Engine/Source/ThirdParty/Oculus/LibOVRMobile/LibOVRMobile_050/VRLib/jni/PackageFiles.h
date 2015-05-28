/************************************************************************************

Filename    :   PackageFiles.h
Content     :   Read files from the application package zip
Created     :   August 18, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVRPACKAGEFILES_H
#define OVRPACKAGEFILES_H

#include "GlTexture.h"

// The application package is the moral equivalent of the filesystem, so
// I don't feel too bad about making it globally accessible, versus requiring
// an App pointer to be handed around to everything that might want to load
// a texture or model.

namespace OVR {

class ModelFile;
class ModelGlPrograms;
class MaterialParms;

//==============================================================
// OvrApkFile
// RAII class for application packages
//==============================================================
class OvrApkFile
{
public:
	OvrApkFile( void * zipFile );
	~OvrApkFile();

	operator void* () const { return ZipFile; }
	operator bool () const { return ZipFile != 0; }

private:
	void *	ZipFile;
};

//--------------------------------------------------------------
// Functions for reading assets from other application packages
//--------------------------------------------------------------

// Call this to open a specific package and use the returned handle in calls to functions for
// loading from other application packages.
void* ovr_OpenOtherApplicationPackage( const char * packageName );

// Call this to close another application package after loading resources from it.
void ovr_CloseOtherApplicationPackage( void * & zipFile );

// These are probably NOT thread safe!
bool ovr_OtherPackageFileExists( void * zipFile, const char * nameInZip );

// Returns NULL buffer if the file is not found.
bool ovr_ReadFileFromOtherApplicationPackage( void * zipFile, const char * nameInZip, int &length, void * & buffer );

// Returns 0 if the file is not found.
// For a file placed in the project assets folder, nameInZip would be
// something like "assets/cube.pvr".
// See GlTexture.h for supported formats.
unsigned int	LoadTextureFromOtherApplicationPackage( void * zipFile, const char * nameInZip,
									const TextureFlags_t & flags, int & width, int & height );

// Returns NULL if the file is not found.
ModelFile *		LoadModelFileFromOtherApplicationPackage( void * zipFile, const char* nameInZip,
									const ModelGlPrograms & programs,
									const MaterialParms & materialParms );


//--------------------------------------------------------------
// Functions for reading assets from this process's application package
//--------------------------------------------------------------

// returns the zip file for the applications own package
void *			ovr_GetApplicationPackageFile();

// This can be called multiple times, but it is ignored after the first one.
void ovr_OpenApplicationPackage( const char * packageName );

// These are probably NOT thread safe!
bool ovr_PackageFileExists( const char * nameInZip );

// Returns NULL buffer if the file is not found.
bool ovr_ReadFileFromApplicationPackage( const char * nameInZip, int & length, void * & buffer );

// Returns an empty MemBufferFile if the file is not found.
bool ovr_ReadFileFromApplicationPackage( const char * nameInZip, MemBufferFile & memBufferFile );

// Returns 0 if the file is not found.
// For a file placed in the project assets folder, nameInZip would be
// something like "assets/cube.pvr".
// See GlTexture.h for supported formats.
unsigned int	LoadTextureFromApplicationPackage( const char * nameInZip,
									const TextureFlags_t & flags, int & width, int & height );

// Returns NULL if the file is not found.
ModelFile *		LoadModelFileFromApplicationPackage( const char* nameInZip,
									const ModelGlPrograms & programs,
									const MaterialParms & materialParms );


}	// namespace OVR

#endif	// !OVRGLTEXTURE_H
