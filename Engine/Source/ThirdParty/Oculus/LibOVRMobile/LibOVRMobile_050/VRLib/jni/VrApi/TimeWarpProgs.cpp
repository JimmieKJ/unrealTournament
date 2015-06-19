/************************************************************************************

Filename    :   TimeWarpProgs.cpp
Content     :   Build TimeWarp Progs
Created     :   December 12, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "TimeWarpLocal.h"

namespace OVR
{

static WarpProgram BuildWarpProg( const char * vertex, const char * fragment )
{
	WarpProgram wp;
	WarpProgram_Create( &wp, vertex, fragment );
	return wp;
}

void TimeWarpLocal::BuildWarpProgPair( ovrTimeWarpProgram simpleIndex,
		const char * simpleVertex, const char * simpleFragment,
		const char * chromaticVertex, const char * chromaticFragment
		)
{
	warpPrograms[ simpleIndex ] = BuildWarpProg( simpleVertex, simpleFragment );
	warpPrograms[ simpleIndex + ( WP_CHROMATIC - WP_SIMPLE ) ] = BuildWarpProg( chromaticVertex, chromaticFragment );
}

void TimeWarpLocal::BuildWarpProgMatchedPair( ovrTimeWarpProgram simpleIndex,
		const char * simpleVertex, const char * simpleFragment
		)
{
	warpPrograms[ simpleIndex ] = BuildWarpProg( simpleVertex, simpleFragment );
	warpPrograms[ simpleIndex + ( WP_CHROMATIC - WP_SIMPLE ) ] = BuildWarpProg( simpleVertex, simpleFragment );
}


void TimeWarpLocal::BuildWarpProgs()
{
    BuildWarpProgPair( WP_SIMPLE,
    // low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   vec3 left = vec3( Texm * vec4(TexCoord,-1,1) );\n"
		"   vec3 right = vec3( Texm2 * vec4(TexCoord,-1,1) );\n"
		"   vec3 proj = mix( left, right, TexCoord1.x );\n"
		"	float projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = texture2D(Texture0, oTexCoord);\n"
		"}\n"
	,
    // high quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"	// green
		"attribute vec2 TexCoord1;\n"	// .x = interpolated warp frac, .y = intensity scale
		"attribute vec2 Normal;\n"		// red
		"attribute vec2 Tangent;\n"		// blue
		"varying  vec2 oTexCoord1r;\n"
		"varying  vec2 oTexCoord1g;\n"
		"varying  vec2 oTexCoord1b;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(Normal,-1,1) ), vec3( Texm2 * vec4(Normal,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord1r = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord1g = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   proj = mix( vec3( Texm * vec4(Tangent,-1,1) ), vec3( Texm2 * vec4(Tangent,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord1b = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"varying highp vec2 oTexCoord1r;\n"
		"varying highp vec2 oTexCoord1g;\n"
		"varying highp vec2 oTexCoord1b;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color1r = texture2D(Texture0, oTexCoord1r);\n"
		"	lowp vec4 color1g = texture2D(Texture0, oTexCoord1g);\n"
		"	lowp vec4 color1b = texture2D(Texture0, oTexCoord1b);\n"
		"	lowp vec4 color1 = vec4( color1r.x, color1g.y, color1b.z, 1.0 );\n"
		"	gl_FragColor = color1;\n"
		"}\n"
	);

    BuildWarpProgPair( WP_MASKED_PLANE,
	// low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"varying  vec3 oTexCoord2;\n"	// Must do the proj in fragment shader or you get wiggles when you view the plane at even modest angles.
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   oTexCoord2 = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		""
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform sampler2D Texture1;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec3 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"	{\n"
		"		lowp vec4 color1 = vec4( texture2DProj(Texture1, oTexCoord2).xyz, 1.0 );\n"
		"		gl_FragColor = mix( color1, color0, color0.w );\n"	// pass through destination alpha
		"	}\n"
		"}\n"
    ,
	// high quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"	// green
		"attribute vec2 TexCoord1;\n"
		"attribute vec2 Normal;\n"		// red
		"attribute vec2 Tangent;\n"		// blue
		"varying  vec2 oTexCoord;\n"
		"varying  vec3 oTexCoord2r;\n"	// These must do the proj in fragment shader or you
		"varying  vec3 oTexCoord2g;\n"	// get wiggles when you view the plane at even
		"varying  vec3 oTexCoord2b;\n"	// modest angles.
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   oTexCoord2r = mix( vec3( Texm3 * vec4(Normal,-1,1) ), vec3( Texm4 * vec4(Normal,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2g = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2b = mix( vec3( Texm3 * vec4(Tangent,-1,1) ), vec3( Texm4 * vec4(Tangent,-1,1) ), TexCoord1.x );\n"
		""
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform sampler2D Texture1;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec3 oTexCoord2r;\n"
		"varying highp vec3 oTexCoord2g;\n"
		"varying highp vec3 oTexCoord2b;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"	{\n"
		"		lowp vec4 color1r = texture2DProj(Texture1, oTexCoord2r);\n"
		"		lowp vec4 color1g = texture2DProj(Texture1, oTexCoord2g);\n"
		"		lowp vec4 color1b = texture2DProj(Texture1, oTexCoord2b);\n"
		"		lowp vec4 color1 = vec4( color1r.x, color1g.y, color1b.z, 1.0 );\n"
		"		gl_FragColor = mix( color1, color0, color0.w );\n"	// pass through destination alpha
		"	}\n"
		"}\n"
    );
    BuildWarpProgPair( WP_MASKED_PLANE_EXTERNAL,
	// low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"varying  vec3 oTexCoord2;\n"	// Must do the proj in fragment shader or you get wiggles when you view the plane at even modest angles.
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   oTexCoord2 = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		""
		"}\n"
	,
		"#extension GL_OES_EGL_image_external : require\n"
		"uniform sampler2D Texture0;\n"
		"uniform samplerExternalOES Texture1;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec3 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"	{\n"
		"		lowp vec4 color1 = vec4( texture2DProj(Texture1, oTexCoord2).xyz, 1.0 );\n"
		"		gl_FragColor = mix( color1, color0, color0.w );\n"	// pass through destination alpha
		"	}\n"
		"}\n"
	,
	// high quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"	// green
		"attribute vec2 TexCoord1;\n"
		"attribute vec2 Normal;\n"		// red
		"attribute vec2 Tangent;\n"		// blue
		"varying  vec2 oTexCoord;\n"
		"varying  vec3 oTexCoord2r;\n"	// These must do the proj in fragment shader or you
		"varying  vec3 oTexCoord2g;\n"	// get wiggles when you view the plane at even
		"varying  vec3 oTexCoord2b;\n"	// modest angles.
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   oTexCoord2r = mix( vec3( Texm3 * vec4(Normal,-1,1) ), vec3( Texm4 * vec4(Normal,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2g = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2b = mix( vec3( Texm3 * vec4(Tangent,-1,1) ), vec3( Texm4 * vec4(Tangent,-1,1) ), TexCoord1.x );\n"
		""
		"}\n"
	,
		"#extension GL_OES_EGL_image_external : require\n"
		"uniform sampler2D Texture0;\n"
		"uniform samplerExternalOES Texture1;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec3 oTexCoord2r;\n"
		"varying highp vec3 oTexCoord2g;\n"
		"varying highp vec3 oTexCoord2b;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"	{\n"
		"		lowp vec4 color1r = texture2DProj(Texture1, oTexCoord2r);\n"
		"		lowp vec4 color1g = texture2DProj(Texture1, oTexCoord2g);\n"
		"		lowp vec4 color1b = texture2DProj(Texture1, oTexCoord2b);\n"
		"		lowp vec4 color1 = vec4( color1r.x, color1g.y, color1b.z, 1.0 );\n"
		"		gl_FragColor = mix( color1, color0, color0.w );\n"	// pass through destination alpha
		"	}\n"
		"}\n"
    );
    BuildWarpProgPair( WP_MASKED_CUBE,
	// low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"
		"uniform mediump vec2 FrameNum;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"varying  vec3 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   oTexCoord2 = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		""
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform samplerCube Texture1;\n"
		"uniform lowp float UniformColor;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec3 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"	lowp vec4 color1 = textureCube(Texture1, oTexCoord2) * UniformColor;\n"
		"	gl_FragColor = vec4( mix( color1.xyz, color0.xyz, color0.w ), 1.0);\n"	// pass through destination alpha
		"}\n"
	,
	// high quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"
		"uniform mediump vec2 FrameNum;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"	// green
		"attribute vec2 TexCoord1;\n"
		"attribute vec2 Normal;\n"		// red
		"attribute vec2 Tangent;\n"		// blue
		"varying  vec2 oTexCoord;\n"
		"varying  vec3 oTexCoord2r;\n"
		"varying  vec3 oTexCoord2g;\n"
		"varying  vec3 oTexCoord2b;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   oTexCoord2r = mix( vec3( Texm3 * vec4(Normal,-1,1) ), vec3( Texm4 * vec4(Normal,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2g = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2b = mix( vec3( Texm3 * vec4(Tangent,-1,1) ), vec3( Texm4 * vec4(Tangent,-1,1) ), TexCoord1.x );\n"
		""
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform samplerCube Texture1;\n"
		"uniform lowp float UniformColor;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec3 oTexCoord2r;\n"
		"varying highp vec3 oTexCoord2g;\n"
		"varying highp vec3 oTexCoord2b;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"	lowp vec4 color1r = textureCube(Texture1, oTexCoord2r);\n"
		"	lowp vec4 color1g = textureCube(Texture1, oTexCoord2g);\n"
		"	lowp vec4 color1b = textureCube(Texture1, oTexCoord2b);\n"
		"	lowp vec3 color1 = vec3( color1r.x, color1g.y, color1b.z ) * UniformColor;\n"
		"	gl_FragColor = vec4( mix( color1, color0.xyz, color0.w ), 1.0);\n"	// pass through destination alpha
		"}\n"
    );
    BuildWarpProgPair( WP_CUBE,
	// low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"
		"uniform mediump vec2 FrameNum;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec3 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord2 = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"}\n"
	,
		"uniform samplerCube Texture1;\n"
		"uniform samplerCube Texture2;\n"
		"uniform samplerCube Texture3;\n"
		"varying highp vec3 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color1 = vec4( textureCube(Texture2, oTexCoord2).xyz, 1.0 );\n"
		"	gl_FragColor = color1;\n"
		"}\n"
	,
	// high quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"
		"uniform mediump vec2 FrameNum;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"	// green
		"attribute vec2 TexCoord1;\n"
		"attribute vec2 Normal;\n"		// red
		"attribute vec2 Tangent;\n"		// blue
		"varying  vec3 oTexCoord2r;\n"
		"varying  vec3 oTexCoord2g;\n"
		"varying  vec3 oTexCoord2b;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord2r = mix( vec3( Texm3 * vec4(Normal,-1,1) ), vec3( Texm4 * vec4(Normal,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2g = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2b = mix( vec3( Texm3 * vec4(Tangent,-1,1) ), vec3( Texm4 * vec4(Tangent,-1,1) ), TexCoord1.x );\n"
		"}\n"
	,
		"uniform samplerCube Texture1;\n"
		"uniform samplerCube Texture2;\n"
		"uniform samplerCube Texture3;\n"
		"varying highp vec3 oTexCoord2r;\n"
		"varying highp vec3 oTexCoord2g;\n"
		"varying highp vec3 oTexCoord2b;\n"
		"void main()\n"
		"{\n"
		"	lowp float color1r = textureCube(Texture1, oTexCoord2r).x;\n"
		"	lowp float color1g = textureCube(Texture2, oTexCoord2g).x;\n"
		"	lowp float color1b = textureCube(Texture3, oTexCoord2b).x;\n"
		"	gl_FragColor = vec4( color1r, color1g, color1b, 1.0);\n"
		"}\n"
    );
    BuildWarpProgPair( WP_LOADING_ICON,
	// low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   vec3 left = vec3( Texm * vec4(TexCoord,-1,1) );\n"
		"   vec3 right = vec3( Texm2 * vec4(TexCoord,-1,1) );\n"
		"   vec3 proj = mix( left, right, TexCoord1.x );\n"
		"	float projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform sampler2D Texture1;\n"
		"uniform highp vec4 RotateScale;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color = texture2D(Texture0, oTexCoord);\n"
		"	highp vec2 iconCenter = vec2( 0.5, 0.5 );\n"
		"	highp vec2 localCoords = oTexCoord - iconCenter;\n"
		"	highp vec2 iconCoords = vec2(	( localCoords.x * RotateScale.y - localCoords.y * RotateScale.x ) * RotateScale.z + iconCenter.x,\n"
		"								( localCoords.x * RotateScale.x + localCoords.y * RotateScale.y ) * -RotateScale.z + iconCenter.x );\n"
		"	if ( iconCoords.x > 0.0 && iconCoords.x < 1.0 && iconCoords.y > 0.0 && iconCoords.y < 1.0 )\n"
		"	{\n"
		"		lowp vec4 iconColor = texture2D(Texture1, iconCoords);"
		"		color.rgb = ( 1.0 - iconColor.a ) * color.rgb + ( iconColor.a ) * iconColor.rgb;\n"
		"	}\n"
		"	gl_FragColor = color;\n"
		"}\n"
	,
	// high quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   vec3 left = vec3( Texm * vec4(TexCoord,-1,1) );\n"
		"   vec3 right = vec3( Texm2 * vec4(TexCoord,-1,1) );\n"
		"   vec3 proj = mix( left, right, TexCoord1.x );\n"
		"	float projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform sampler2D Texture1;\n"
		"uniform highp vec4 RotateScale;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color = texture2D(Texture0, oTexCoord);\n"
		"	highp vec2 iconCenter = vec2( 0.5, 0.5 );\n"
		"	highp vec2 localCoords = oTexCoord - iconCenter;\n"
		"	highp vec2 iconCoords = vec2(	( localCoords.x * RotateScale.y - localCoords.y * RotateScale.x ) * RotateScale.z + iconCenter.x,\n"
		"								( localCoords.x * RotateScale.x + localCoords.y * RotateScale.y ) * -RotateScale.z + iconCenter.x );\n"
		"	if ( iconCoords.x > 0.0 && iconCoords.x < 1.0 && iconCoords.y > 0.0 && iconCoords.y < 1.0 )\n"
		"	{\n"
		"		lowp vec4 iconColor = texture2D(Texture1, iconCoords);"
		"		color.rgb = ( 1.0 - iconColor.a ) * color.rgb + ( iconColor.a ) * iconColor.rgb;\n"
		"	}\n"
		"	gl_FragColor = color;\n"
		"}\n"
    );
    BuildWarpProgPair( WP_MIDDLE_CLAMP,
	// low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   vec3 left = vec3( Texm * vec4(TexCoord,-1,1) );\n"
		"   vec3 right = vec3( Texm2 * vec4(TexCoord,-1,1) );\n"
		"   vec3 proj = mix( left, right, TexCoord1.x );\n"
		"	float projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform highp vec2 TexClamp;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = texture2D(Texture0, vec2( clamp( oTexCoord.x, TexClamp.x, TexClamp.y ), oTexCoord.y ) );\n"
		"}\n"
	,
	// high quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   vec3 left = vec3( Texm * vec4(TexCoord,-1,1) );\n"
		"   vec3 right = vec3( Texm2 * vec4(TexCoord,-1,1) );\n"
		"   vec3 proj = mix( left, right, TexCoord1.x );\n"
		"	float projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform highp vec2 TexClamp;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = texture2D(Texture0, vec2( clamp( oTexCoord.x, TexClamp.x, TexClamp.y ), oTexCoord.y ) );\n"
		"}\n"
    );

    BuildWarpProgPair( WP_OVERLAY_PLANE,
	// low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
		"varying  vec3 oTexCoord2;\n"	// Must do the proj in fragment shader or you get wiggles when you view the plane at even modest angles.
   		"varying  float clampVal;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   oTexCoord2 = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		""
		// We need to clamp the projected texcoords to keep from getting a mirror
		// image behind the view, and mip mapped edge clamp (I wish we had CLAMP_TO_BORDER)
		// issues far off to the sides.
		"	vec2 clampXY = oTexCoord2.xy / oTexCoord2.z;\n"
// this is backwards on Stratum    		"	clampVal = ( oTexCoord2.z > -0.01 || clampXY.x < -0.1 || clampXY.y < -0.1 || clampXY.x > 1.1 || clampXY.y > 1.1 ) ? 1.0 : 0.0;\n"
    		"	clampVal = ( oTexCoord2.z < -0.01 || clampXY.x < -0.1 || clampXY.y < -0.1 || clampXY.x > 1.1 || clampXY.y > 1.1 ) ? 1.0 : 0.0;\n"
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform sampler2D Texture1;\n"
		"varying lowp float clampVal;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec3 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"	if ( clampVal == 1.0 )\n"
		"	{\n"
		"		gl_FragColor = color0;\n"
		"	} else {\n"
		"		lowp vec4 color1 = texture2DProj(Texture1, oTexCoord2);\n"
		"		gl_FragColor = mix( color0, color1, color1.w );\n"
		"	}\n"
		"}\n"
    ,
	// high quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
		"uniform mediump mat4 Texm4;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"	// green
		"attribute vec2 TexCoord1;\n"
		"attribute vec2 Normal;\n"		// red
		"attribute vec2 Tangent;\n"		// blue
		"varying  vec2 oTexCoord;\n"
		"varying  vec3 oTexCoord2r;\n"	// These must do the proj in fragment shader or you
		"varying  vec3 oTexCoord2g;\n"	// get wiggles when you view the plane at even
		"varying  vec3 oTexCoord2b;\n"	// modest angles.
		"varying  float clampVal;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"   oTexCoord2r = mix( vec3( Texm3 * vec4(Normal,-1,1) ), vec3( Texm4 * vec4(Normal,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2g = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
		"   oTexCoord2b = mix( vec3( Texm3 * vec4(Tangent,-1,1) ), vec3( Texm4 * vec4(Tangent,-1,1) ), TexCoord1.x );\n"
    	""
    	// We need to clamp the projected texcoords to keep from getting a mirror
    	// image behind the view, and mip mapped edge clamp (I wish we had CLAMP_TO_BORDER)
    	// issues far off to the sides.
		"	vec2 clampXY = oTexCoord2r.xy / oTexCoord2r.z;\n"
    	"	clampVal = ( oTexCoord2r.z > -0.01 || clampXY.x < -0.1 || clampXY.y < -0.1 || clampXY.x > 1.1 || clampXY.y > 1.1 ) ? 1.0 : 0.0;\n"
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform sampler2D Texture1;\n"
		"varying lowp float clampVal;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec3 oTexCoord2r;\n"
		"varying highp vec3 oTexCoord2g;\n"
		"varying highp vec3 oTexCoord2b;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"	if ( clampVal == 1.0 )\n"
		"	{\n"
		"		gl_FragColor = color0;\n"
		"	} else {\n"
		"		lowp vec4 color1r = texture2DProj(Texture1, oTexCoord2r);\n"
		"		lowp vec4 color1g = texture2DProj(Texture1, oTexCoord2g);\n"
		"		lowp vec4 color1b = texture2DProj(Texture1, oTexCoord2b);\n"
		"		lowp vec4 color1 = vec4( color1r.x, color1g.y, color1b.z, 1.0 );\n"
		"		gl_FragColor = mix( color0, color1, vec4( color1r.w, color1g.w, color1b.w, 1.0 ) );\n"
		"	}\n"
		"}\n"
    );


    // Debug program to color tint the overlay for LOD visualization
    BuildWarpProgMatchedPair( WP_OVERLAY_PLANE_SHOW_LOD,
    		"#version 300 es\n"
    		"uniform mediump mat4 Mvpm;\n"
    		"uniform mediump mat4 Texm;\n"
    		"uniform mediump mat4 Texm2;\n"
    		"uniform mediump mat4 Texm3;\n"
    		"uniform mediump mat4 Texm4;\n"

    		"in vec4 Position;\n"
    		"in vec2 TexCoord;\n"
    		"in vec2 TexCoord1;\n"
    		"out vec2 oTexCoord;\n"
    		"out vec3 oTexCoord2;\n"	// Must do the proj in fragment shader or you get wiggles when you view the plane at even modest angles.
       		"out float clampVal;\n"
    		"void main()\n"
    		"{\n"
    		"   gl_Position = Mvpm * Position;\n"
    		"	vec3 proj;\n"
    		"	float projIZ;\n"
    		""
    		"   proj = mix( vec3( Texm * vec4(TexCoord,-1,1) ), vec3( Texm2 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
    		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
    		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
    		""
    		"   oTexCoord2 = mix( vec3( Texm3 * vec4(TexCoord,-1,1) ), vec3( Texm4 * vec4(TexCoord,-1,1) ), TexCoord1.x );\n"
    		""
    		// We need to clamp the projected texcoords to keep from getting a mirror
    		// image behind the view, and mip mapped edge clamp (I wish we had CLAMP_TO_BORDER)
    		// issues far off to the sides.
    		"	vec2 clampXY = oTexCoord2.xy / oTexCoord2.z;\n"
    		"	clampVal = ( oTexCoord2.z > -0.01 || clampXY.x < -0.1 || clampXY.y < -0.1 || clampXY.x > 1.1 || clampXY.y > 1.1 ) ? 1.0 : 0.0;\n"
    		"}\n"
    	,
    		"#version 300 es\n"
    		"uniform sampler2D Texture0;\n"
    		"uniform sampler2D Texture1;\n"
    		"in lowp float clampVal;\n"
    		"in highp vec2 oTexCoord;\n"
    		"in highp vec3 oTexCoord2;\n"
    		"out mediump vec4 fragColor;\n"
    		"void main()\n"
    		"{\n"
    		"	lowp vec4 color0 = texture(Texture0, oTexCoord);\n"
    		"	if ( clampVal == 1.0 )\n"
    		"	{\n"
    		"		fragColor = color0;\n"
    		"	} else {\n"
    		"		highp vec2 proj = vec2( oTexCoord2.x, oTexCoord2.y ) / oTexCoord2.z;\n"
    		"		lowp vec4 color1 = texture(Texture1, proj);\n"
    		"		mediump vec2 stepVal = fwidth( proj ) * vec2( textureSize( Texture1, 0 ) );\n"
    		"		mediump float w = max( stepVal.x, stepVal.y );\n"
    		"		if ( w < 1.0 ) { color1 = mix( color1, vec4( 0.0, 1.0, 0.0, 1.0 ), min( 1.0, 2.0 * ( 1.0 - w ) ) ); }\n"
			"		else { color1 = mix( color1, vec4( 1.0, 0.0, 0.0, 1.0 ), min( 1.0, w - 1.0 ) ); }\n"
    		"		fragColor = mix( color0, color1, color1.w );\n"
    		"	}\n"
    		"}\n"
    );

    BuildWarpProgMatchedPair( WP_CAMERA,
	// low quality
		"uniform mediump mat4 Mvpm;\n"
		"uniform mediump mat4 Texm;\n"
		"uniform mediump mat4 Texm2;\n"
		"uniform mediump mat4 Texm3;\n"
   		"uniform mediump mat4 Texm4;\n"
   		"uniform mediump mat4 Texm5;\n"

		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"attribute vec2 TexCoord1;\n"
		"varying  vec2 oTexCoord;\n"
   		"varying  vec2 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"

    	"   vec4 lens = vec4(TexCoord,-1.0,1.0);"
		"	vec3 proj;\n"
		"	float projIZ;\n"
		""
		"   proj = mix( vec3( Texm * lens ), vec3( Texm2 * lens ), TexCoord1.x );\n"
		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
		"	oTexCoord = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
   		"   vec4 dir = mix( lens, Texm2 * lens, TexCoord1.x );\n"
    		" dir.xy /= dir.z*-1.0;\n"
    		" dir.z = -1.0;\n"
    		" dir.w = 1.0;\n"
    	"	float rolling = Position.y * -1.5 + 0.5;\n"	// roughly 0 = top of camera, 1 = bottom of camera
   		"   proj = mix( vec3( Texm3 * lens ), vec3( Texm4 * lens ), rolling );\n"
   		"	projIZ = 1.0 / max( proj.z, 0.00001 );\n"
   		"	oTexCoord2 = vec2( proj.x * projIZ, proj.y * projIZ );\n"
		""
		"}\n"
	,
		"#extension GL_OES_EGL_image_external : require\n"
		"uniform sampler2D Texture0;\n"
		"uniform samplerExternalOES Texture1;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying highp vec2 oTexCoord2;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 color0 = texture2D(Texture0, oTexCoord);\n"
		"		lowp vec4 color1 = vec4( texture2D(Texture1, oTexCoord2).xyz, 1.0 );\n"
		"		gl_FragColor = mix( color1, color0, color0.w );\n"	// pass through destination alpha
//		" gl_FragColor = color1;"
		"}\n"
	);

}

}	// namespace OVR
