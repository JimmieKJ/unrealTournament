/************************************************************************************

Filename    :   WarpProgram.cpp
Content     :   Time warp shader program compilation.
Created     :   March 3, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "WarpProgram.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Android/GlUtils.h"
#include "Android/LogUtils.h"

namespace OVR
{

// Returns false and logs the ShaderInfoLog on failure.
static bool CompileShader( const GLuint shader, const char * src )
{
	glShaderSource( shader, 1, &src, 0 );
	glCompileShader( shader );

	GLint r;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &r );
	if ( r == GL_FALSE )
	{
		LOG( "Compiling shader:\n%s\n****** failed ******\n", src );
		GLchar msg[4096];
		glGetShaderInfoLog( shader, sizeof( msg ), 0, msg );
		LOG( "%s\n", msg );
		return false;
	}
	return true;
}

void WarpProgram_Create( WarpProgram * prog, const char * vertexSrc, const char * fragmentSrc )
{
	prog->vertexShader = glCreateShader( GL_VERTEX_SHADER );
	if ( !CompileShader( prog->vertexShader, vertexSrc ) )
	{
		FAIL( "Failed to compile vertex shader" );
	}
	prog->fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
	if ( !CompileShader( prog->fragmentShader, fragmentSrc ) )
	{
		FAIL( "Failed to compile fragment shader" );
	}

	prog->program = glCreateProgram();
	glAttachShader( prog->program, prog->vertexShader );
	glAttachShader( prog->program, prog->fragmentShader );

	// set attributes before linking
	glBindAttribLocation( prog->program, VERTEX_ATTRIBUTE_LOCATION_POSITION,		"Position" );
	glBindAttribLocation( prog->program, VERTEX_ATTRIBUTE_LOCATION_NORMAL,			"Normal" );
	glBindAttribLocation( prog->program, VERTEX_ATTRIBUTE_LOCATION_TANGENT,			"Tangent" );
	glBindAttribLocation( prog->program, VERTEX_ATTRIBUTE_LOCATION_BINORMAL,		"Binormal" );
	glBindAttribLocation( prog->program, VERTEX_ATTRIBUTE_LOCATION_COLOR,			"VertexColor" );
	glBindAttribLocation( prog->program, VERTEX_ATTRIBUTE_LOCATION_UV0,				"TexCoord" );
	glBindAttribLocation( prog->program, VERTEX_ATTRIBUTE_LOCATION_UV1,				"TexCoord1" );

	// link and error check
	glLinkProgram( prog->program );
	GLint r;
	glGetProgramiv( prog->program, GL_LINK_STATUS, &r );
	if ( r == GL_FALSE )
	{
		GLchar msg[1024];
		glGetProgramInfoLog( prog->program, sizeof( msg ), 0, msg );
		FAIL( "Linking program failed: %s\n", msg );
	}
	prog->uMvp = glGetUniformLocation( prog->program, "Mvpm" );
	prog->uModel = glGetUniformLocation( prog->program, "Modelm" );
	prog->uView = glGetUniformLocation( prog->program, "Viewm" );
	prog->uProjection = glGetUniformLocation( prog->program, "Projectionm" );
	prog->uColor = glGetUniformLocation( prog->program, "UniformColor" );
	prog->uTexm = glGetUniformLocation( prog->program, "Texm" );
	prog->uTexm2 = glGetUniformLocation( prog->program, "Texm2" );
	prog->uTexm3 = glGetUniformLocation( prog->program, "Texm3" );
	prog->uTexm4 = glGetUniformLocation( prog->program, "Texm4" );
	prog->uTexm5 = glGetUniformLocation( prog->program, "Texm5" );
	prog->uTexClamp = glGetUniformLocation( prog->program, "TexClamp" );
	prog->uRotateScale = glGetUniformLocation( prog->program, "RotateScale" );

	glUseProgram( prog->program );

	// texture and image_external bindings
	for ( int i = 0; i < 8; i++ )
	{
		char name[32];
		sprintf( name, "Texture%i", i );
		const GLint uTex = glGetUniformLocation( prog->program, name );
		if ( uTex != -1 )
		{
			glUniform1i( uTex, i );
		}
	}

	glUseProgram( 0 );
}

void WarpProgram_Destroy( WarpProgram * prog )
{
	if ( prog->program != 0 )
	{
		glDeleteProgram( prog->program );
	}
	if ( prog->vertexShader != 0 )
	{
		glDeleteShader( prog->vertexShader );
	}
	if ( prog->fragmentShader != 0 )
	{
		glDeleteShader( prog->fragmentShader );
	}
	prog->program = 0;
	prog->vertexShader = 0;
	prog->fragmentShader = 0;
}

}	// namespace OVR
