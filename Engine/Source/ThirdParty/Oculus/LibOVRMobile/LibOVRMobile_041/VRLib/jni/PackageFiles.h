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

// This can be called multiple times, but it is ignored after the first one.
void ovr_OpenApplicationPackage( const char * packageName );

// These are probably NOT thread safe!
bool ovr_PackageFileExists( const char * nameInZip );

// Returns NULL buffer if the file is not found.
void ovr_ReadFileFromApplicationPackage( const char * nameInZip, int &length, void * & buffer );

// Returns 0 if the file is not found.
unsigned int	LoadTextureFromApplicationPackage( const char * nameInZip,
									const TextureFlags_t & flags, int & width, int & height );

// Returns NULL if the file is not found.
ModelFile *		LoadModelFileFromApplicationPackage( const char* nameInZip,
									const ModelGlPrograms & programs,
									const MaterialParms & materialParms );


}	// namespace OVR

#endif	// !OVRGLTEXTURE_H
