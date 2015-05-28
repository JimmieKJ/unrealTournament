/************************************************************************************

Filename    :   GlTexture.h
Content     :   OpenGL texture loading.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVRGLTEXTURE_H
#define OVRGLTEXTURE_H

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_BitFlags.h"
#include "Kernel/OVR_MemBuffer.h"
#include "Android/GlUtils.h"

// Explicitly using unsigned instead of GLUint / GLenum to avoid including GL headers

namespace OVR {

enum eTextureFlags
{
	// Normally, a failure to load will create an 8x8 default texture, but
	// if you want to take explicit action, setting this flag will cause
	// it to return 0 for the texId.
	TEXTUREFLAG_NO_DEFAULT,
	// Use GL_SRGB8 / GL_SRGB8_ALPHA8 / GL_COMPRESSED_SRGB8_ETC2 formats instead
	// of GL_RGB / GL_RGBA / GL_ETC1_RGB8_OES
	TEXTUREFLAG_USE_SRGB,
	// No mip maps are loaded or generated when this flag is specified.
	TEXTUREFLAG_NO_MIPMAPS
};

typedef BitFlagsT< eTextureFlags > TextureFlags_t;

// texture id/target pair
// the auto-casting should be removed but allows the target to be ignored by the code that does not care
struct GlTexture
{
	GlTexture() : texture( 0 ), target( 0 ) {}
	GlTexture( unsigned texture_ );
	GlTexture( unsigned texture_, unsigned target_ ) : texture( texture_ ), target( target_ ) {}
	operator unsigned() const { return texture; }

	unsigned	texture;
	unsigned	target;
};

// Allocates a GPU texture and uploads the raw data.
GlTexture	LoadRGBATextureFromMemory( const unsigned char * texture, const int width, const int height, const bool useSrgbFormat );
GlTexture	LoadRGBTextureFromMemory( const unsigned char * texture, const int width, const int height, const bool useSrgbFormat );
GlTexture	LoadRTextureFromMemory( const unsigned char * texture, const int width, const int height );
GlTexture	LoadASTCTextureFromMemory( const uint8_t * buffer, const size_t bufferSize, const int numPlanes );

void		MakeTextureClamped( GlTexture texid );
void		MakeTextureLodClamped( GlTexture texId, int maxLod );
void		MakeTextureTrilinear( GlTexture texid );
void		MakeTextureLinear( GlTexture texId );
void		MakeTextureAniso( GlTexture texId, float maxAniso );
void		BuildTextureMipmaps( GlTexture texid );

// FileName's extension determines the file type, but the data is taken from an
// already loaded buffer.
//
// The stb_image file formats are supported:
// .jpg .tga .png .bmp .psd .gif .hdr .pic
//
// Limited support for the PVR and KTX container formats.
//
// If TEXTUREFLAG_NO_DEFAULT, no default texture will be created.
// Otherwise a default square texture will be created on any failure.
//
// Uncompressed image formats will have mipmaps generated and trilinear filtering set.
GlTexture	LoadTextureFromBuffer( const char * fileName, const MemBuffer & buffer,
				const TextureFlags_t & flags, int & width, int & height );

unsigned char * LoadPVRBuffer( const char * fileName, int & width, int & height );

// glDeleteTextures()
// Can be safely called on a 0 texture without checking.
void		FreeTexture( GlTexture texId );

}

#endif	// !OVRGLTEXTURE_H
