// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRendererPrivate.h"
#include "SlateOpenGLRenderer.h"
#include "OpenGL/SlateOpenGLTextures.h"

GLuint FSlateOpenGLTexture::NullTexture = 0;

void FSlateOpenGLTexture::Init( GLenum TexFormat, const TArray<uint8>& TextureData )
{
	// Create a new OpenGL texture
	glGenTextures(1, &ShaderResource);
	CHECK_GL_ERRORS;

	// Ensure texturing is enabled before setting texture properties
#if !PLATFORM_USES_ES2 && !PLATFORM_LINUX
	glEnable(GL_TEXTURE_2D);
#endif
	glBindTexture(GL_TEXTURE_2D, ShaderResource);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#if !PLATFORM_LINUX
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
#endif

	// the raw data is in bgra or bgr
	const GLint Format = GL_BGRA;

	// Upload the texture data
	glTexImage2D( GL_TEXTURE_2D, 0, TexFormat, SizeX, SizeY, 0, Format, GL_UNSIGNED_INT_8_8_8_8_REV, TextureData.GetData() );
	CHECK_GL_ERRORS;
}

void FSlateOpenGLTexture::Init( GLuint TextureID )
{
	ShaderResource = TextureID;
}

void FSlateOpenGLTexture::ResizeTexture(uint32 Width, uint32 Height)
{
	SizeX = Width;
	SizeY = Height;
}

void FSlateOpenGLTexture::UpdateTexture(const TArray<uint8>& Bytes)
{
	// Ensure texturing is enabled before setting texture properties
#if !PLATFORM_USES_ES2 && !PLATFORM_LINUX
	glEnable(GL_TEXTURE_2D);
#endif
	glBindTexture(GL_TEXTURE_2D, ShaderResource);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#if !PLATFORM_LINUX
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#endif

	// Upload the texture data
#if !PLATFORM_USES_ES2
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, SizeX, SizeY, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, Bytes.GetData());
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8_EXT, SizeX, SizeY, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, Bytes.GetData());
#endif
	CHECK_GL_ERRORS;
}

FSlateFontTextureOpenGL::FSlateFontTextureOpenGL( uint32 Width, uint32 Height )
	: FSlateFontAtlas( Width, Height ) 
	, FontTexture(nullptr)
{

}

FSlateFontTextureOpenGL::~FSlateFontTextureOpenGL()
{
	delete FontTexture;
}

void FSlateFontTextureOpenGL::CreateFontTexture()
{
	// Generate an ID for this texture
	GLuint TextureID;
	glGenTextures(1, &TextureID);

	// Bind the texture so we can specify filtering and the data to use
	glBindTexture(GL_TEXTURE_2D, TextureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Font textures use an alpha texture only
	GLint Format = GL_ALPHA;
	// Upload the data to the texture
	glTexImage2D( GL_TEXTURE_2D, 0, Format, AtlasWidth, AtlasHeight, 0, Format, GL_UNSIGNED_BYTE, NULL );

	// Create a new slate texture for use in rendering
	FontTexture = new FSlateOpenGLTexture( AtlasWidth, AtlasHeight );
	FontTexture->Init( TextureID );


}

void FSlateFontTextureOpenGL::ConditionalUpdateTexture()
{
	// The texture may not be valid when calling this as OpenGL must wait until after the first viewport has been created to create a texture
	if( bNeedsUpdate && FontTexture )
	{
		check(AtlasData.Num()>0);

		// Completely the texture data each time characters are added
		glBindTexture(GL_TEXTURE_2D, FontTexture->GetTypedResource() );
		GLint Format = GL_ALPHA;
#if PLATFORM_MAC // Make this texture use a DMA'd client storage backing store on OS X, where these extensions always exist
				 // This avoids a problem on Intel & Nvidia cards that makes characters disappear as well as making the texture updates
				 // as fast as they possibly can be.
		glTextureRangeAPPLE(GL_TEXTURE_2D, AtlasData.Num(), AtlasData.GetData());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
		glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
#endif
#if PLATFORM_LINUX
		glTexImage2D( GL_TEXTURE_2D, 0, GL_R8, AtlasWidth, AtlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, AtlasData.GetData() );
#else
		glTexImage2D( GL_TEXTURE_2D, 0, Format, AtlasWidth, AtlasHeight, 0, Format, GL_UNSIGNED_BYTE, AtlasData.GetData() );
#endif
#if PLATFORM_MAC
		glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
#endif
		
		bNeedsUpdate = false;
	}
}
