/************************************************************************************

Filename    :   GlProgram.h
Content     :   Shader program compilation.
Created     :   October 11, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_GlProgram_h
#define OVR_GlProgram_h

#include "Android/GlUtils.h"

namespace OVR
{

// STRINGIZE is used so program text strings can include lines like:
// "uniform highp mat4 Joints["MAX_JOINTS_STRING"];\n"

#define STRINGIZE( x )			#x
#define STRINGIZE_VALUE( x )	STRINGIZE( x )

#define MAX_JOINTS				16
#define MAX_JOINTS_STRING		STRINGIZE_VALUE( MAX_JOINTS )

// No attempt is made to support sharing shaders between programs,
// it isn't worth avoiding the duplication.

// Shader uniforms Texture0 - Texture7 are bound to texture units 0 - 7

enum VertexAttributeLocation
{
	VERTEX_ATTRIBUTE_LOCATION_POSITION		= 0,
	VERTEX_ATTRIBUTE_LOCATION_NORMAL		= 1,
	VERTEX_ATTRIBUTE_LOCATION_TANGENT		= 2,
	VERTEX_ATTRIBUTE_LOCATION_BINORMAL		= 3,
	VERTEX_ATTRIBUTE_LOCATION_COLOR			= 4,
	VERTEX_ATTRIBUTE_LOCATION_UV0			= 5,
	VERTEX_ATTRIBUTE_LOCATION_UV1			= 6,
	VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES	= 7,
	VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS	= 8,
	VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS	= 9
};

struct GlProgram
{
		GlProgram() :
			program( 0 ),
			vertexShader( 0 ),
			fragmentShader( 0 ),
			uMvp( -1 ),
			uModel( -1 ),
			uView( -1 ),
			uProjection( -1 ),
			uColor( -1 ),
			uFadeDirection( -1 ),
			uTexm( -1 ),
			uTexm2( -1 ),
			uJoints( -1 ),
			uColorTableOffset( -1 ) {};

	// These will always be > 0 after a build, any errors will abort()
	unsigned	program;
	unsigned	vertexShader;
	unsigned	fragmentShader;

	// Uniforms that aren't found will have a -1 value
	int		uMvp;				// uniform Mvpm
	int		uModel;				// uniform Modelm
	int		uView;				// uniform Viewm
	int		uProjection;		// uniform Projectionm
	int		uColor;				// uniform UniformColor
	int		uFadeDirection;		// uniform FadeDirection
	int		uTexm;				// uniform Texm
	int		uTexm2;				// uniform Texm2
	int		uJoints;			// uniform Joints
	int		uColorTableOffset;	// uniform offset to apply to color table index
};

// It probably isn't worth keeping these shared here, each user
// should just duplicate them.
extern const char * externalFragmentShaderSource;
extern const char * textureFragmentShaderSource;
extern const char * identityVertexShaderSource;
extern const char * untexturedFragmentShaderSource;

extern const char * VertexColorVertexShaderSrc;
extern const char * VertexColorSkinned1VertexShaderSrc;
extern const char * VertexColorFragmentShaderSrc;

extern const char * SingleTextureVertexShaderSrc;
extern const char * SingleTextureSkinned1VertexShaderSrc;
extern const char * SingleTextureFragmentShaderSrc;

extern const char * LightMappedVertexShaderSrc;
extern const char * LightMappedSkinned1VertexShaderSrc;
extern const char * LightMappedFragmentShaderSrc;

extern const char * ReflectionMappedVertexShaderSrc;
extern const char * ReflectionMappedSkinned1VertexShaderSrc;
extern const char * ReflectionMappedFragmentShaderSrc;

// Will abort() after logging an error if either compiles or the link status
// fails, but not if uniforms are missing.
GlProgram	BuildProgram( const char * vertexSrc, const char * fragmentSrc );

void		DeleteProgram( GlProgram & prog );

}	// namespace OVR

#endif	// OVR_GlProgram_h
