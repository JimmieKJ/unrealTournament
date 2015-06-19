/************************************************************************************

Filename    :   WarpProgram.h
Content     :   Time warp shader program compilation.
Created     :   March 3, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_WarpProgram_h
#define OVR_WarpProgram_h

#include "Android/GlUtils.h"

namespace OVR
{

// Shader uniforms Texture0 - Texture7 are bound to texture units 0 - 7

enum VertexAttributeLocation
{
	VERTEX_ATTRIBUTE_LOCATION_POSITION		= 0,
	VERTEX_ATTRIBUTE_LOCATION_NORMAL		= 1,
	VERTEX_ATTRIBUTE_LOCATION_TANGENT		= 2,
	VERTEX_ATTRIBUTE_LOCATION_BINORMAL		= 3,
	VERTEX_ATTRIBUTE_LOCATION_COLOR			= 4,
	VERTEX_ATTRIBUTE_LOCATION_UV0			= 5,
	VERTEX_ATTRIBUTE_LOCATION_UV1			= 6
};

struct WarpProgram
{
		WarpProgram() :
			program( 0 ),
			vertexShader( 0 ),
			fragmentShader( 0 ),
			uMvp( -1 ),
			uModel( -1 ),
			uView( -1 ),
			uProjection( -1 ),
			uColor( -1 ),
			uTexm( -1 ),
			uTexm2( -1 ),
			uTexm3( -1 ),
			uTexm4( -1 ),
			uTexm5( -1 ),
			uTexClamp( -1 ),
			uRotateScale( -1 ) {}

	// These will always be > 0 after a build, any errors will abort()
	GLuint	program;
	GLuint	vertexShader;
	GLuint	fragmentShader;

	// Uniforms that aren't found will have a -1 value
	GLint	uMvp;				// uniform Mvpm
	GLint	uModel;				// uniform Modelm
	GLint	uView;				// uniform Viewm
	GLint	uProjection;		// uniform Projectionm
	GLint	uColor;				// uniform UniformColor
	GLint	uTexm;				// uniform Texm
	GLint	uTexm2;				// uniform Texm2
	GLint	uTexm3;				// uniform Texm3
	GLint	uTexm4;				// uniform Texm4
	GLint	uTexm5;				// uniform Texm5
	GLint	uTexClamp;			// uniform TexClamp
	GLint	uRotateScale;		// uniform RotateScale
};

// Will abort() after logging an error if either compiles or the link status
// fails, but not if uniforms are missing.
void WarpProgram_Create( WarpProgram * prog, const char * vertexSrc, const char * fragmentSrc );
void WarpProgram_Destroy( WarpProgram * prog );

}	// namespace OVR

#endif	// OVR_WarpProgram_h
