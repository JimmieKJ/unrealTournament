/************************************************************************************

Filename    :   VRMenuObjectLocal.cpp
Content     :   Menuing system for VR apps.
Created     :   May 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "VRMenuObjectLocal.h"

#include "../GlTexture.h"
#include "../App.h"			// for loading images from the assets folder
#include "../ModelTrace.h"
#include "../BitmapFont.h"
#include "VRMenuMgr.h"
#include "VRMenuComponent.h"
#include "ui_default.h"	// embedded default UI texture (loaded as a placeholder when something doesn't load)
#include "PackageFiles.h"


namespace OVR {

float const	VRMenuObject::TEXELS_PER_METER		= 500.0f;
float const	VRMenuObject::DEFAULT_TEXEL_SCALE	= 1.0f / TEXELS_PER_METER;

const float VRMenuSurface::Z_BOUNDS = 0.05f;

// too bad this doesn't work
//#pragma GCC diagnostic ignored "-Werror"

//======================================================================================
// VRMenuSurfaceTexture

//==============================
// VRMenuSurfaceTexture::VRMenuSurfaceTexture::
VRMenuSurfaceTexture::	VRMenuSurfaceTexture() :
		Handle( 0 ),
		Width( 0 ),
		Height( 0 ),
		Type( SURFACE_TEXTURE_MAX ),
        OwnsTexture( false )
{
}

//==============================
// VRMenuSurfaceTexture::LoadTexture
bool VRMenuSurfaceTexture::LoadTexture( eSurfaceTextureType const type, char const * imageName, bool const allowDefault )
{
    Free();

	OVR_ASSERT( type >= 0 && type < SURFACE_TEXTURE_MAX );

	Type = type;

	if ( imageName != NULL && imageName[0] != '\0' )
	{
		void * 	buffer;
		int		bufferLength;
		ovr_ReadFileFromApplicationPackage( imageName, bufferLength, buffer );
		if ( !buffer )
		{
			Handle = 0;
		}
		else
		{
			Handle = LoadTextureFromBuffer( imageName, MemBuffer( buffer, bufferLength ),
					TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), Width, Height );
			free( buffer );
		}
	}

	if ( Handle == 0 && allowDefault )
	{
		Handle = LoadTextureFromBuffer( imageName, MemBuffer( uiDefaultTgaData, uiDefaultTgaSize ), 
							TextureFlags_t(), Width, Height );
		WARN( "VRMenuSurfaceTexture::CreateFromImage: failed to load image '%s' - default loaded instead!", imageName );
	}
    OwnsTexture = true;
	return Handle != 0;
}

//==============================
// VRMenuSurfaceTexture::LoadTexture
void VRMenuSurfaceTexture::LoadTexture( eSurfaceTextureType const type, const GLuint texId, const int width, const int height )
{
	Free();

	OVR_ASSERT( type >= 0 && type < SURFACE_TEXTURE_MAX );

	Type = type;
    OwnsTexture = false;
	Handle = texId;
	Width = width;
	Height = height;
}

//==============================
// VRMenuSurfaceTexture::Free
void VRMenuSurfaceTexture::Free()
{
	if ( Handle != 0 )
	{
        if ( OwnsTexture )
        {
		    glDeleteTextures( 1, &Handle );
        }
		Handle = 0;
		Width = 0;
		Height = 0;
		Type = SURFACE_TEXTURE_MAX;
        OwnsTexture = false;
	}
}

//======================================================================================
// VRMenuSurfaceTris


//======================================================================================
// VRMenuSurface

#if 0
static void PrintBounds( const char * name, char const * prefix, Bounds3f const & bounds )
{
	LOG( "'%s' %s: min( %.2f, %.2f, %.2f ) - max( %.2f, %.2f, %.2f )", 
		name, prefix,
		bounds.GetMins().x, bounds.GetMins().y, bounds.GetMins().z,
		bounds.GetMaxs().x, bounds.GetMaxs().y, bounds.GetMaxs().z );
}
#endif

//==============================
// VRMenuSurface::VRMenuSurface
VRMenuSurface::VRMenuSurface() :
	Color( 1.0f ),
	Border( 0.0f, 0.0f, 0.0f, 0.0f ),
	Contents( CONTENT_SOLID ),
	Visible( true ),
	ProgramType( PROGRAM_MAX )
{
}

//==============================
// VRMenuSurface::~VRMenuSurface
VRMenuSurface::~VRMenuSurface()
{
	Free();
}

//==============================
// VRMenuSurface::CreateImageGeometry
//
// This creates a quad for mapping the texture.
void VRMenuSurface::CreateImageGeometry( int const textureWidth, int const textureHeight, const Vector2f &dims, const Vector4f &border, ContentFlags_t const contents )
{
	//OVR_ASSERT( Geo.vertexBuffer == 0 && Geo.indexBuffer == 0 && Geo.vertexArrayObject == 0 );

	int vertsX = 0;
	int vertsY = 0;
	float vertUVX[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float vertUVY[ 4 ] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float vertPosX[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float vertPosY[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// x components
	vertPosX[ vertsX ] = 0.0f;
	vertUVX[ vertsX++ ] = 0.0f;

	if ( border[ BORDER_LEFT ] > 0.0f )
	{
		vertPosX[ vertsX ] 	= border[ BORDER_LEFT ] / dims.x;
		vertUVX[ vertsX++ ] = border[ BORDER_LEFT ] / (float)textureWidth;
	}

	if ( border[ BORDER_RIGHT ] > 0.0f )
	{
		vertPosX[ vertsX ]  = 1.0f - border[ BORDER_RIGHT ] / dims.x;
		vertUVX[ vertsX++ ] = 1.0f - border[ BORDER_RIGHT ] / (float)textureWidth;
	}

	vertPosX[ vertsX ] = 1.0f;
	vertUVX[ vertsX++ ] = 1.0f;

	// y components
	vertPosY[ vertsY ] = 0.0f;
	vertUVY[ vertsY++ ] = 0.0f;

	if ( border[ BORDER_BOTTOM ] > 0.0f )
	{
		vertPosY[ vertsY ] 	= border[ BORDER_BOTTOM ] / dims.y;
		vertUVY[ vertsY++ ] = border[ BORDER_BOTTOM ] / (float)textureHeight;
	}

	if ( border[ BORDER_TOP ] > 0.0f )
	{
		vertPosY[ vertsY ]  = 1.0f - border[ BORDER_TOP ] / dims.y;
		vertUVY[ vertsY++ ] = 1.0f - border[ BORDER_TOP ] / (float)textureHeight;
	}

	vertPosY[ vertsY ] = 1.0f;
	vertUVY[ vertsY++ ] = 1.0f;

	// create the vertices
	const int vertexCount = vertsX * vertsY;
	const int horizontal = vertsX - 1;
	const int vertical = vertsY - 1;

	VertexAttribs attribs;
	attribs.position.Resize( vertexCount );
	attribs.uv0.Resize( vertexCount );
	attribs.uv1.Resize( vertexCount );
	attribs.color.Resize( vertexCount );

	Vector4f color( 1.0f, 1.0f, 1.0f, 1.0f );

	for ( int y = 0; y <= vertical; y++ )
	{
		const float yPos = ( -1 + vertPosY[ y ] * 2 ) * ( dims.y * VRMenuObject::DEFAULT_TEXEL_SCALE * 0.5f );
		const float uvY = 1.0f - vertUVY[ y ];

		for ( int x = 0; x <= horizontal; x++ )
		{
			const int index = y * ( horizontal + 1 ) + x;
			attribs.position[index].x = ( -1 + vertPosX[ x ] * 2 ) * ( dims.x * VRMenuObject::DEFAULT_TEXEL_SCALE * 0.5f );
			attribs.position[index].z = 0;
			attribs.position[index].y = yPos;
			attribs.uv0[index].x = vertUVX[ x ];
			attribs.uv0[index].y = uvY;
			attribs.uv1[index] = attribs.uv0[index];
			attribs.color[index] = color;
		}
	}

	Array< TriangleIndex > indices;
	indices.Resize( horizontal * vertical * 6 );

	// If this is to be used to draw a linear format texture, like
	// a surface texture, it is better for cache performance that
	// the triangles be drawn to follow the side to side linear order.
	int index = 0;
	for ( int y = 0; y < vertical; y++ )
	{
		for ( int x = 0; x < horizontal; x++ )
		{
			indices[index + 0] = y * (horizontal + 1) + x;
			indices[index + 1] = y * (horizontal + 1) + x + 1;
			indices[index + 2] = (y + 1) * (horizontal + 1) + x;
			indices[index + 3] = (y + 1) * (horizontal + 1) + x;
			indices[index + 4] = y * (horizontal + 1) + x + 1;
			indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
			index += 6;
		}
	}

    Tris.Init( attribs.position, indices, contents );

	if ( Geo.vertexBuffer == 0 && Geo.indexBuffer == 0 && Geo.vertexArrayObject == 0 )
	{
		Geo.Create( attribs, indices );
	}
	else
	{
		Geo.Update( attribs );
	}
}

//==============================
// VRMenuSurface::Render
void VRMenuSurface::Render( OvrVRMenuMgr const & menuMgr, Matrix4f const & mvp, SubmittedMenuObject const & sub ) const
{
	if ( Geo.vertexCount == 0 )
	{
		return;	// surface wasn't initialized with any geometry -- this can happen if diffuse and additive are both invalid
	}

    //LOG( "Render Surface '%s', skip = '%s'", SurfaceName.ToCStr(), skipAdditivePass ? "true" : "false" );

	GL_CheckErrors( "VRMenuSurface::Render - pre" );

	GlProgram const * program = NULL;

	glEnable( GL_BLEND );

	eGUIProgramType pt = ProgramType;

	if ( sub.SkipAdditivePass )
	{
		if ( pt == PROGRAM_DIFFUSE_PLUS_ADDITIVE || pt == PROGRAM_DIFFUSE_COMPOSITE )
		{
			pt = PROGRAM_DIFFUSE_ONLY;	// this is used to not render the gazeover hilights
		}
	}

    program = menuMgr.GetGUIGlProgram( pt );

	switch( pt )
	{
		case PROGRAM_DIFFUSE_ONLY:
		{
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			int diffuseIndex = IndexForTextureType( SURFACE_TEXTURE_DIFFUSE, 1 );
			DROID_ASSERT( diffuseIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			// bind the texture
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, Textures[diffuseIndex].GetHandle() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			break;
		}
		case PROGRAM_DIFFUSE_COMPOSITE:
		{
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			int diffuseIndex = IndexForTextureType( SURFACE_TEXTURE_DIFFUSE, 1 );
			DROID_ASSERT( diffuseIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			int diffuse2Index = IndexForTextureType( SURFACE_TEXTURE_DIFFUSE, 2 );
			DROID_ASSERT( diffuse2Index >= 0, "VrMenu" );	// surface setup should have detected this!
			// bind both textures
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, Textures[diffuseIndex].GetHandle() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			glActiveTexture( GL_TEXTURE1 );
			glBindTexture( GL_TEXTURE_2D, Textures[diffuse2Index].GetHandle() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			break;
		}
		case PROGRAM_ADDITIVE_ONLY:
		{
			glBlendFunc( GL_SRC_ALPHA, GL_ONE );
			int additiveIndex = IndexForTextureType( SURFACE_TEXTURE_ADDITIVE, 1 );
			DROID_ASSERT( additiveIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			// bind the texture
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, Textures[additiveIndex].GetHandle() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			break;
		}
		case PROGRAM_DIFFUSE_PLUS_ADDITIVE:		// has a diffuse and an additive
		{
			//glBlendFunc( GL_ONE, GL_ONE );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			int diffuseIndex = IndexForTextureType( SURFACE_TEXTURE_DIFFUSE, 1 );
			DROID_ASSERT( diffuseIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			int additiveIndex = IndexForTextureType( SURFACE_TEXTURE_ADDITIVE, 1 );
			DROID_ASSERT( additiveIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			// bind both textures
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, Textures[diffuseIndex].GetHandle() );
			glActiveTexture( GL_TEXTURE1 );
			glBindTexture( GL_TEXTURE_2D, Textures[additiveIndex].GetHandle() );
			break;
		}
		case PROGRAM_DIFFUSE_COLOR_RAMP:			// has a diffuse and color ramp, and color ramp target is the diffuse
		{
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			int diffuseIndex = IndexForTextureType( SURFACE_TEXTURE_DIFFUSE, 1 );
			DROID_ASSERT( diffuseIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			int rampIndex = IndexForTextureType( SURFACE_TEXTURE_COLOR_RAMP, 1 );
			DROID_ASSERT( rampIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			// bind both textures
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, Textures[diffuseIndex].GetHandle() );
			glActiveTexture( GL_TEXTURE1 );
			glBindTexture( GL_TEXTURE_2D, Textures[rampIndex].GetHandle() );
			// do not do any filtering on the "palette" texture
			if ( EXT_texture_filter_anisotropic )
			{
				glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
			}
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			break;
		}
		case PROGRAM_DIFFUSE_COLOR_RAMP_TARGET:	// has diffuse, color ramp, and a separate color ramp target
		{
			//LOG( "Surface '%s' - PROGRAM_COLOR_RAMP_TARGET", SurfaceName );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			int diffuseIndex = IndexForTextureType( SURFACE_TEXTURE_DIFFUSE, 1 );
			DROID_ASSERT( diffuseIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			int rampIndex = IndexForTextureType( SURFACE_TEXTURE_COLOR_RAMP, 1 );
			DROID_ASSERT( rampIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			int targetIndex = IndexForTextureType( SURFACE_TEXTURE_COLOR_RAMP_TARGET, 1 );
			DROID_ASSERT( targetIndex >= 0, "VrMenu" );	// surface setup should have detected this!
			// bind both textures
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, Textures[diffuseIndex].GetHandle() );
			glActiveTexture( GL_TEXTURE1 );
			glBindTexture( GL_TEXTURE_2D, Textures[targetIndex].GetHandle() );
			glActiveTexture( GL_TEXTURE2 );
			glBindTexture( GL_TEXTURE_2D, Textures[rampIndex].GetHandle() );
			// do not do any filtering on the "palette" texture
			if ( EXT_texture_filter_anisotropic )
			{
				glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
			}
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			break;
		}
		case PROGRAM_MAX:
		{
			WARN( "Unsupported texture map combination." );
			return;
		}
		default:
		{
			DROID_ASSERT( !"Unhandled ProgramType", "Uhandled ProgramType" );
			return;
		}
	}

	DROID_ASSERT( program != NULL, "VrMenu" );

	glUseProgram( program->program );

	glUniformMatrix4fv( program->uMvp, 1, GL_FALSE, mvp.M[0] );

	glUniform4fv( program->uColor, 1, &sub.Color.x );
	glUniform3fv( program->uFadeDirection, 1, &sub.FadeDirection.x );
	glUniform2fv( program->uColorTableOffset, 1, &sub.ColorTableOffset.x );

	// render
	glBindVertexArrayOES_( Geo.vertexArrayObject );
	glDrawElements( GL_TRIANGLES, Geo.indexCount, GL_UNSIGNED_SHORT, NULL );
	glBindVertexArrayOES_( 0 );

	GL_CheckErrors( "VRMenuSurface::Render - post" );
}

//==============================
// VRMenuSurface::CreateFromSurfaceParms
void VRMenuSurface::CreateFromSurfaceParms( VRMenuSurfaceParms const & parms )
{
	Free();

	SurfaceName = parms.SurfaceName;

	// verify the input parms have a valid image name and texture type
	bool isValid = false;
	for ( int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i )	
	{
		if ( !parms.ImageNames[i].IsEmpty() && 
            ( parms.TextureTypes[i] >= 0 && parms.TextureTypes[i] < SURFACE_TEXTURE_MAX ) )
	    {
    		isValid = true;
            Textures[i].LoadTexture( parms.TextureTypes[i], parms.ImageNames[i], true );
		}
		else if ( ( parms.ImageTexId[i] != 0 ) &&
            ( parms.TextureTypes[i] >= 0 && parms.TextureTypes[i] < SURFACE_TEXTURE_MAX ) )
	    {
    		isValid = true;
            Textures[i].LoadTexture( parms.TextureTypes[i], parms.ImageTexId[i], parms.ImageWidth[i], parms.ImageHeight[i] );
		}
	}
	if ( !isValid )
	{
		//LOG( "VRMenuSurfaceParms '%s' - no valid images - skipping", parms.SurfaceName.ToCStr() );
		return;
	}

	// make sure we have a surface for sizing the geometry
	int surfaceIdx = -1;
	for ( int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i )
	{
		if ( Textures[i].GetHandle() != 0 )
		{
			surfaceIdx = i;
			break;
		}
	}
	if ( surfaceIdx < 0 )
	{
		//LOG( "VRMenuSurface::CreateFromImageParms - no suitable image for surface creation" );
		return;
	}

	TextureDims.x = Textures[surfaceIdx].GetWidth();
	TextureDims.y = Textures[surfaceIdx].GetHeight();

	if ( ( parms.Dims.x == 0 ) || ( parms.Dims.y == 0 ) )
	{
		Dims = TextureDims;
	}
	else
	{
		Dims = parms.Dims;
	}

	Border = parms.Border;
	Anchors = parms.Anchors;
	Contents = parms.Contents;

	CreateImageGeometry( TextureDims.x, TextureDims.y, Dims, Border, Contents );

	// now, based on the combination of surfaces, determine the render prog to use
	if ( HasTexturesOfType( SURFACE_TEXTURE_DIFFUSE, 1 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_COLOR_RAMP, 1 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_COLOR_RAMP_TARGET, 1 ) )
	{
		ProgramType = PROGRAM_DIFFUSE_COLOR_RAMP_TARGET;
	}
	else if ( HasTexturesOfType( SURFACE_TEXTURE_DIFFUSE, 1 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_MAX, 2 ) )
	{
		ProgramType = PROGRAM_DIFFUSE_ONLY;
	}
	else if ( HasTexturesOfType( SURFACE_TEXTURE_ADDITIVE, 1 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_MAX, 2 ) )
	{
		ProgramType = PROGRAM_ADDITIVE_ONLY;
	}
	else if ( HasTexturesOfType( SURFACE_TEXTURE_DIFFUSE, 2 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_MAX, 1 ) )
	{
		ProgramType = PROGRAM_DIFFUSE_COMPOSITE;
	}
	else if ( HasTexturesOfType( SURFACE_TEXTURE_DIFFUSE, 1 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_COLOR_RAMP, 1 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_MAX, 1 ) )
	{
		ProgramType = PROGRAM_DIFFUSE_COLOR_RAMP;
	}
	else if ( HasTexturesOfType( SURFACE_TEXTURE_DIFFUSE, 1 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_ADDITIVE, 1 ) &&
		HasTexturesOfType( SURFACE_TEXTURE_MAX, 1 ) )
	{
		ProgramType = PROGRAM_DIFFUSE_PLUS_ADDITIVE;
	}
	else
	{
		WARN( "Invalid material combination -- either add a shader to support it or fix it." );
		ProgramType = PROGRAM_MAX;
	}
}

//==============================
// VRMenuSurface::RegenerateSurfaceGeometry
void VRMenuSurface::RegenerateSurfaceGeometry()
{
	CreateImageGeometry( TextureDims.x, TextureDims.y, Dims, Border, Contents );
}

//==============================
// VRMenuSurface::
bool VRMenuSurface::HasTexturesOfType( eSurfaceTextureType const t, int const requiredCount ) const
{
	int count = 0;
	for ( int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i )
	{
		if ( Textures[i].GetType() == t ) {
			count++;
		}
	}
	return ( requiredCount == count );	// must be the exact same number
}

int VRMenuSurface::IndexForTextureType( eSurfaceTextureType const t, int const occurenceCount ) const
{
	int count = 0;
	for ( int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i )
	{
		if ( Textures[i].GetType() == t ) {
			count++;
			if ( count == occurenceCount )
			{
				return i;
			}
		}
	}
	return -1;
}

//==============================
// VRMenuSurface::Free
void VRMenuSurface::Free()
{
	for ( int i = 0; i < VRMENUSURFACE_IMAGE_MAX; ++i )
	{
		Textures[i].Free();
	}
}

//==============================
// VRMenuSurface::IntersectRay
bool VRMenuSurface::IntersectRay( Vector3f const & start, Vector3f const & dir, Posef const & pose,
                                  Vector3f const & scale, ContentFlags_t const testContents,
								  OvrCollisionResult & result ) const
{
    return Tris.IntersectRay( start, dir, pose, scale, testContents, result );
}

//==============================
// VRMenuSurface::IntersectRay
bool VRMenuSurface::IntersectRay( Vector3f const & localStart, Vector3f const & localDir, 
                                  Vector3f const & scale, ContentFlags_t const testContents,
								  OvrCollisionResult & result ) const
{
    return Tris.IntersectRay( localStart, localDir, scale, testContents, result );
}

//==============================
// VRMenuSurface::LoadTexture
void VRMenuSurface::LoadTexture( int const textureIndex, eSurfaceTextureType const type, 
        const GLuint texId, const int width, const int height )
{
    if ( textureIndex < 0 || textureIndex >= VRMENUSURFACE_IMAGE_MAX )
    {
        DROID_ASSERT( textureIndex >= 0 && textureIndex < VRMENUSURFACE_IMAGE_MAX, "VrMenu" );
        return;
    }
    Textures[textureIndex].LoadTexture( type, texId, width, height );
}

//==============================
// VRMenuSurface::GetAnchorOffsets
Vector2f VRMenuSurface::GetAnchorOffsets() const { 
	return Vector2f( ( ( 1.0f - Anchors.x ) - 0.5f ) * Dims.x * VRMenuObject::DEFAULT_TEXEL_SCALE, // inverted so that 0.0 is left-aligned
					 ( Anchors.y - 0.5f ) * Dims.y * VRMenuObject::DEFAULT_TEXEL_SCALE ); 
}

void VRMenuSurface::SetOwnership( int const index, bool const isOwner )
{
	Textures[ index ].SetOwnership( isOwner );
}

//======================================================================================
// VRMenuObjectLocal

//==================================
// VRMenuObjectLocal::VRMenuObjectLocal
VRMenuObjectLocal::VRMenuObjectLocal( VRMenuObjectParms const & parms, 
		menuHandle_t const handle ) :
	Type( parms.Type ),
	Handle( handle ),
	Id( parms.Id ),
	Flags( parms.Flags ),
	LocalPose( parms.LocalPose ),
	LocalScale( parms.LocalScale ),
    HilightPose( Quatf(), Vector3f( 0.0f, 0.0f, 0.0f ) ),
    HilightScale( 1.0f ),
    TextLocalPose( parms.TextLocalPose ),
    TextLocalScale( parms.TextLocalScale ),
	Text( parms.Text ),
	CollisionPrimitive( NULL ),
	Contents( CONTENT_SOLID ),
	Color( parms.Color ),
    TextColor( parms.TextColor ),
	ColorTableOffset( 0.0f ),
	FontParms( parms.FontParms ),
	Hilighted( false ),
	Selected( false ),
    TextDirty( true ),
	MinsBoundsExpand( 0.0f ),
	MaxsBoundsExpand( 0.0f ),
	TextMetrics(),
	WrapWidth( 0.0f )
{
	CullBounds.Clear();
}

//==================================
// VRMenuObjectLocal::~VRMenuObjectLocal
VRMenuObjectLocal::~VRMenuObjectLocal()
{
	if ( CollisionPrimitive != NULL )
	{
		delete CollisionPrimitive;
		CollisionPrimitive = NULL;
	}

    // all components must be dynamically allocated
    for ( int i = 0; i < Components.GetSizeI(); ++i )
    {
        delete Components[i];
        Components[i] = NULL;
    }
    Components.Clear();
	Handle.Release();
	ParentHandle.Release();
	Type = VRMENU_MAX;
}

//==================================
// VRMenuObjectLocal::Init
void VRMenuObjectLocal::Init( VRMenuObjectParms const & parms )
{
	for ( int i = 0; i < parms.SurfaceParms.GetSizeI(); ++i )
	{
		int idx = Surfaces.AllocBack();
		Surfaces[idx].CreateFromSurfaceParms( parms.SurfaceParms[i] );
	}

	// bounds are nothing submitted for rendering
	CullBounds = Bounds3f( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
	FontParms = parms.FontParms;
    for ( int i = 0; i < parms.Components.GetSizeI(); ++i )
    {
	    AddComponent( parms.Components[i] );
    }
}

//==================================
// VRMenuObjectLocal::FreeChildren
void VRMenuObjectLocal::FreeChildren( OvrVRMenuMgr & menuMgr )
{
	for ( int i = 0; i < Children.GetSizeI(); ++i ) 
	{
		menuMgr.FreeObject( Children[i] );
	}
	Children.Resize( 0 );
    // NOTE! bounds will be incorrect now until submitted for rendering
}

//==================================
// VRMenuObjectLocal::IsDescendant
bool VRMenuObjectLocal::IsDescendant( OvrVRMenuMgr & menuMgr, menuHandle_t const handle ) const
{
	for ( int i = 0; i < Children.GetSizeI(); ++i )
	{
		if ( Children[i] == handle )
		{
			return true;
		}
	}

	for ( int i = 0; i < Children.GetSizeI(); ++i )
	{
		VRMenuObject * child = menuMgr.ToObject( Children[i] );
		if ( child != NULL )
		{
			bool r = child->IsDescendant( menuMgr, handle );
			if ( r )
			{
				return true;
			}
		}
	}

	return false;
}

//==============================
// VRMenuObjectLocal::AddChild
void VRMenuObjectLocal::AddChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle )
{
	Children.PushBack( handle );

	VRMenuObject * child = menuMgr.ToObject( handle );
	if ( child != NULL )
	{
		child->SetParentHandle( this->Handle );
	}
    // NOTE: bounds will be incorrect until submitted for rendering
}

//==============================
// VRMenuObjectLocal::RemoveChild
void VRMenuObjectLocal::RemoveChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle )
{
	for ( int i = 0; i < Children.GetSizeI(); ++i )
	{
		if ( Children[i] == handle )
		{
			Children.RemoveAtUnordered( i );
			return;
		}
	}
}

//==============================
// VRMenuObjectLocal::FreeChild
void VRMenuObjectLocal::FreeChild( OvrVRMenuMgr & menuMgr, menuHandle_t const handle )
{
	for ( int i = 0; i < Children.GetSizeI(); ++i) 
	{
		menuHandle_t childHandle = Children[i];
		if ( childHandle == handle )
		{
			Children.RemoveAtUnordered( i );
			menuMgr.FreeObject( childHandle );
			return;
		}
	}
}

//==============================
// VRMenuObjectLocal::Frame
void VRMenuObjectLocal::Frame( OvrVRMenuMgr & menuMgr, Matrix4f const & viewMatrix )
{
	for ( int i = 0; i < Children.GetSizeI(); ++i )
	{
		VRMenuObject * child = menuMgr.ToObject( Children[i] );
		if ( child != NULL )
		{
			child->Frame( menuMgr, viewMatrix );
		}
	}
}

//==============================
// IntersectRayBounds
// Reports true if the hit was at or beyond start in the ray direction, 
// or if the start point was inside of the bounds.
bool VRMenuObjectLocal::IntersectRayBounds( Vector3f const & start, Vector3f const & dir, 
        Vector3f const & mins, Vector3f const & maxs, ContentFlags_t const testContents, float & t0, float & t1 ) const
{
	if ( !( testContents & GetContents() ) )
	{
		return false;
	}

    if ( Bounds3f( mins, maxs ).Contains( start, 0.1f ) )
    {
        return true;
    }
	Intersect_RayBounds( start, dir, mins, maxs, t0, t1 );
	return t0 >= 0.0f && t1 >= 0.0f && t1 >= t0;
}

//==============================
// VRMenuObjectLocal::IntersectRay
bool VRMenuObjectLocal::IntersectRay( Vector3f const & localStart, Vector3f const & localDir, Vector3f const & parentScale, Bounds3f const & bounds,
        float & bounds_t0, float & bounds_t1, ContentFlags_t const testContents, OvrCollisionResult & result ) const
{
	result = OvrCollisionResult();

    // bounds are already computed with scale applied
    if ( !IntersectRayBounds( localStart, localDir, bounds.GetMins(), bounds.GetMaxs(), testContents, bounds_t0, bounds_t1 ) )
    {
        bounds_t0 = FLT_MAX;
        bounds_t1 = FLT_MAX;
        return false;
    }

    // if marked to check only the bounds, then we've hit the object
	if ( Flags & VRMENUOBJECT_HIT_ONLY_BOUNDS )
	{
		result.t = bounds_t0;
		return true;
	}

    // vertices have not had the scale applied yet
    Vector3f const scale = GetLocalScale() * parentScale;

	// test vs. collision primitive
	if ( CollisionPrimitive != NULL )
	{
		CollisionPrimitive->IntersectRay( localStart, localDir, scale, testContents, result );
	}
	
	// test vs. surfaces
	if (  GetType() != VRMENU_CONTAINER )
	{
		int numSurfaces = 0;
		for ( int i = 0; i < Surfaces.GetSizeI(); ++i )
		{
			if ( Surfaces[i].IsRenderable() )
			{
				numSurfaces++;

				OvrCollisionResult localResult;
				if ( Surfaces[i].IntersectRay( localStart, localDir, scale, testContents, localResult ) )
				{
					if ( localResult.t < result.t )
					{
						result = localResult;
					}
				}
			}
		}
	}

    return result.TriIndex >= 0;
}

//==============================
// VRMenuObjectLocal::HitTest_r
bool VRMenuObjectLocal::HitTest_r( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font, 
		Posef const & parentPose, Vector3f const & parentScale, Vector3f const & rayStart, Vector3f const & rayDir,
		ContentFlags_t const testContents, HitTestResult & result ) const
{
	if ( Flags & VRMENUOBJECT_DONT_RENDER )
	{
		return false;
	}

	if ( Flags & VRMENUOBJECT_DONT_HIT_ALL )
	{
		return false;
	}

	// transform ray into local space
	Vector3f const & localScale = GetLocalScale();
	Vector3f scale = parentScale.EntrywiseMultiply( localScale );
	Posef modelPose;
	modelPose.Position = parentPose.Position + ( parentPose.Orientation * parentScale.EntrywiseMultiply( LocalPose.Position ) );
	modelPose.Orientation = LocalPose.Orientation * parentPose.Orientation;
	Vector3f localStart = modelPose.Orientation.Inverted().Rotate( rayStart - modelPose.Position );
	Vector3f localDir = modelPose.Orientation.Inverted().Rotate( rayDir );
/*
    DROIDLOG( "Spam", "Hit test vs '%s', start: (%.2f, %.2f, %.2f ) cull bounds( %.2f, %.2f, %.2f ) -> ( %.2f, %.2f, %.2f )", GetText().ToCStr(),
            localStart.x, localStart.y, localStart.z,
            CullBounds.b[0].x, CullBounds.b[0].y, CullBounds.b[0].z,
            CullBounds.b[1].x, CullBounds.b[1].y, CullBounds.b[1].z );
*/
    // test against cull bounds if we have children  ... otherwise cullBounds == localBounds
    if ( Children.GetSizeI() > 0 )  
    {
        if ( CullBounds.IsInverted() )
        {
            DROIDLOG( "Spam", "CullBounds are inverted!!" );
            return false;
        }
	    float cullT0;
	    float cullT1;
		// any contents will hit cull bounds
		ContentFlags_t allContents( ALL_BITS );
	    bool hitCullBounds = IntersectRayBounds( localStart, localDir, CullBounds.GetMins(), CullBounds.GetMaxs(), 
									allContents, cullT0, cullT1 );

//        DROIDLOG( "Spam", "Cull hit = %s, t0 = %.2f t1 = %.2f", hitCullBounds ? "true" : "false", cullT0, cullT1 );

	    if ( !hitCullBounds )
	    {
            return false;
        }
    }

	// test against self first, if not a container
    if ( GetContents() & testContents )
    {
        if ( Flags & VRMENUOBJECT_BOUND_ALL )
        {
            // local bounds are the union of surface bounds and text bounds
            Bounds3f localBounds = GetLocalBounds( font ) * parentScale;
            float t0;
	        float t1;
	        bool hit = IntersectRayBounds( localStart, localDir, localBounds.GetMins(), localBounds.GetMaxs(), testContents, t0, t1 );
            if ( hit )
            {
                result.HitHandle = Handle;
                result.t = t1;
				result.uv = Vector2f( 0.0f );	// unknown
            }
        }
        else
        {
	        float selfT0;
	        float selfT1;
			OvrCollisionResult cresult;
	        Bounds3f const & localBounds = GetLocalBounds( font ) * parentScale;
            OVR_ASSERT( !localBounds.IsInverted() );

	        bool hit = IntersectRay( localStart, localDir, parentScale, localBounds, selfT0, selfT1, testContents, cresult );
            if ( hit )
    	    {
				//app->ShowInfoText( 0.0f, "tri: %i", (int)cresult.TriIndex );
				result = cresult;
		        result.HitHandle = Handle;
	        }

            // also check vs. the text bounds if there is any text
            if ( !Text.IsEmpty() && GetType() != VRMENU_CONTAINER && ( Flags & VRMENUOBJECT_DONT_HIT_TEXT ) == 0 )
            {
                float textT0;
                float textT1;
                Bounds3f bounds = GetTextLocalBounds( font ) * parentScale;
                bool textHit = IntersectRayBounds( localStart, localDir, bounds.GetMins(), bounds.GetMaxs(), testContents, textT0, textT1 );
                if ( textHit && textT1 < result.t )
                {
                    result.HitHandle = Handle;
                    result.t = textT1;
					result.uv = Vector2f( 0.0f );	// unknown
                }
            }
        }
    }

	// test against children
	for ( int i = 0; i < Children.GetSizeI(); ++i )
	{
		VRMenuObjectLocal * child = static_cast< VRMenuObjectLocal* >( menuMgr.ToObject( Children[i] ) );
		if ( child != NULL )
		{
			HitTestResult childResult;
			bool intersected = child->HitTest_r( app, menuMgr, font, modelPose, scale, rayStart, rayDir, testContents, childResult );
			if ( intersected && childResult.t < result.t )
			{
				result = childResult;
			}
		}
    }
	return result.HitHandle.IsValid();
}

//==============================
// VRMenuObjectLocal::HitTest
menuHandle_t VRMenuObjectLocal::HitTest( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font, Posef const & worldPose, 
        Vector3f const & rayStart, Vector3f const & rayDir, ContentFlags_t const testContents, HitTestResult & result ) const
{
	HitTest_r( app, menuMgr, font, worldPose, Vector3f( 1.0f ), rayStart, rayDir, testContents, result );

	return result.HitHandle;
}

//==============================
// VRMenuObjectLocal::RenderSurface
void VRMenuObjectLocal::RenderSurface( OvrVRMenuMgr const & menuMgr, Matrix4f const & mvp, SubmittedMenuObject const & sub ) const
{
	Surfaces[sub.SurfaceIndex].Render( menuMgr, mvp, sub );
}

//==============================
// VRMenuObjectLocal::GetLocalBounds
Bounds3f VRMenuObjectLocal::GetLocalBounds( BitmapFont const & font ) const { 
	Bounds3f bounds;
	bounds.Clear();
    Vector3f const localScale = GetLocalScale();
	for ( int i = 0; i < Surfaces.GetSizeI(); i++ )
	{
		Bounds3f const & surfaceBounds = Surfaces[i].GetLocalBounds() * localScale;
		bounds = Bounds3f::Union( bounds, surfaceBounds );
	}

	if ( CollisionPrimitive != NULL )
	{
		bounds = Bounds3f::Union( bounds, CollisionPrimitive->GetBounds() );
	}

    // transform surface bounds by whatever the hilight pose is
    if ( !bounds.IsInverted() )
    {
        bounds = Bounds3f::Transform( HilightPose, bounds );
    }

	// also union the text bounds, as long as we're not a container (containers don't render anything)
	if ( !Text.IsEmpty() > 0 && GetType() != VRMENU_CONTAINER )
	{
		bounds = Bounds3f::Union( bounds, GetTextLocalBounds( font ) );
	}

    // if no valid surface bounds, then the local bounds is the local translation
    if ( bounds.IsInverted() )
    {
        bounds.AddPoint( LocalPose.Position );
        bounds = Bounds3f::Transform( HilightPose, bounds );
    }

	// after everything is calculated, expand (or contract) the bounds some custom amount
	bounds = Bounds3f::Expand( bounds, MinsBoundsExpand, MaxsBoundsExpand );
    
    return bounds;
}

//==============================
// VRMenuObjectLocal::GetTextLocalBounds
Bounds3f VRMenuObjectLocal::GetTextLocalBounds( BitmapFont const & font ) const
{
    if ( TextDirty )
    {
		TextDirty = false;

		// also union the text bounds
		if ( Text.IsEmpty() )
		{
			TextMetrics = textMetrics_t();
		}
		else
		{
			size_t len;
			int const MAX_LINES = 16;
			float lineWidths[MAX_LINES];
			int numLines = 0;

			font.CalcTextMetrics( Text.ToCStr(), len, TextMetrics.w, TextMetrics.h, 
					TextMetrics.ascent, TextMetrics.descent, TextMetrics.fontHeight, lineWidths, MAX_LINES, numLines );
		}
    }

	// NOTE: despite being 3 scalars, text scaling only uses the x component since
	// DrawText3D doesn't take separate x and y scales right now.
	Vector3f const localScale = GetLocalScale();
	Vector3f const textLocalScale = GetTextLocalScale();
	float const scale = localScale.x * textLocalScale.x * FontParms.Scale;
	// this seems overly complex because font characters are rendered so that their origin
	// is on their baseline and not on one of the corners of the glyph. Because of this
	// we must treat the initial ascent (amount the font goes above the first baseline) and
	// final descent (amount the font goes below the final baseline) independently from the
	// lines in between when centering.
	Bounds3f textBounds( Vector3f( 0.0f, ( TextMetrics.h - TextMetrics.ascent ) * -1.0f, 0.0f ) * scale,
						 Vector3f( TextMetrics.w, TextMetrics.ascent, 0.0f ) * scale );

	Vector3f trans = Vector3f::ZERO;
	switch( FontParms.AlignVert )
	{
		case VERTICAL_BOTTOM :
			trans.y = 0.0f;
			break;

		case VERTICAL_CENTER :
		{
			trans.y = ( TextMetrics.h * 0.5f ) - TextMetrics.ascent;
			break;
		}

		case VERTICAL_CENTER_FIXEDHEIGHT :
		{
			trans.y = ( TextMetrics.fontHeight * -0.5f );
			break;
		}

		case VERTICAL_TOP :
		{
			trans.y = TextMetrics.h - TextMetrics.ascent;
			break;
		}
	}

	switch( FontParms.AlignHoriz )
	{
		case HORIZONTAL_LEFT :
			trans.x = 0.0f;
			break;

		case HORIZONTAL_CENTER :
		{
			trans.x = TextMetrics.w * -0.5f;
			break;
		}
		case HORIZONTAL_RIGHT :
		{
			trans.x = TextMetrics.w;
			break;
		}
	}

	textBounds.Translate( trans * scale );

	Bounds3f textLocalBounds = Bounds3f::Transform( GetTextLocalPose(), textBounds );
	// transform by hilightpose here since surfaces are transformed by it before unioning the bounds
	textLocalBounds = Bounds3f::Transform( HilightPose, textLocalBounds );

	return textLocalBounds;
}

//==============================
// VRMenuObjectLocal::AddComponent
void VRMenuObjectLocal::AddComponent( VRMenuComponent * component )
{
	if ( component == NULL )
	{
		return;	// this is fine... makes submitting VRMenuComponentParms easier.
	}

	int componentIndex = GetComponentIndex( component );
	if ( componentIndex >= 0 )
	{
		// cannot add the same component twice!
		DROID_ASSERT( componentIndex < 0, "VRMenu" );
		return;
	}
	Components.PushBack( component );
}

//==============================
// VRMenuObjectLocal::RemoveComponent
void VRMenuObjectLocal::RemoveComponent( VRMenuComponent * component )
{
	int componentIndex = GetComponentIndex( component );
	if ( componentIndex < 0 )
	{
		return;
	}
	// maintain order because components of the same handler type may be have intentionally 
	// been added in a specific order
	Components.RemoveAt( componentIndex );
}

//==============================
// VRMenuObjectLocal::GetComponentIndex
int VRMenuObjectLocal::GetComponentIndex( VRMenuComponent * component ) const
{	
	for ( int i = 0; i < Components.GetSizeI(); ++i )
	{
		if ( Components[i] == component )
		{
			return i;
		}
	}
	return -1;
}

//==============================
// VRMenuObjectLocal::GetComponentById
VRMenuComponent * VRMenuObjectLocal::GetComponentById_Impl( int id ) const
{
	Array< VRMenuComponent* > comps = GetComponentList( );
	for ( int c = 0; c < comps.GetSizeI(); ++c )
	{
		if ( VRMenuComponent * comp = comps[ c ] )
		{
			if ( comp->GetTypeId( ) == id )
			{
				return comp;
			}
		}
		else
		{
			OVR_ASSERT( comp );
		}
	}

	return NULL;
}

//==============================
// VRMenuObjectLocal::GetComponentByName
VRMenuComponent * VRMenuObjectLocal::GetComponentByName_Impl( const char * typeName ) const
{
	Array< VRMenuComponent* > comps = GetComponentList();
	for ( int c = 0; c < comps.GetSizeI(); ++c )
	{
		if ( VRMenuComponent * comp = comps[ c ] )
		{
			if ( comp->GetTypeName( ) == typeName )
			{
				return comp;
			}
		}
		else
		{
			OVR_ASSERT( comp );
		}
	}

	return NULL;
}

//==============================
// VRMenuObjectLocal::GetColorTableOffset
Vector2f const &	VRMenuObjectLocal::GetColorTableOffset() const
{
	return ColorTableOffset;
}

//==============================
// VRMenuObjectLocal::SetColorTableOffset
void VRMenuObjectLocal::SetColorTableOffset( Vector2f const & ofs )
{
	ColorTableOffset = ofs;
}

//==============================
// VRMenuObjectLocal::GetColor
Vector4f const & VRMenuObjectLocal::GetColor() const
{
	return Color;
}

//==============================
// VRMenuObjectLocal::SetColor
void VRMenuObjectLocal::SetColor( Vector4f const & c )
{
	Color = c;
}

void VRMenuObjectLocal::SetVisible( bool visible )
{
	if ( visible )
	{
		Flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
	}
	else
	{
		Flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
	}
}

//==============================
// VRMenuObjectLocal::ChildHandleForId
menuHandle_t VRMenuObjectLocal::ChildHandleForId( OvrVRMenuMgr & menuMgr, VRMenuId_t const id ) const
{
	int n = NumChildren();
	for ( int i = 0; i < n; ++i )
	{
		VRMenuObjectLocal const * child = static_cast< VRMenuObjectLocal* >( menuMgr.ToObject( GetChildHandleForIndex( i ) ) );
		if ( child != NULL )
		{
			if ( child->GetId() == id )
			{
				return child->GetHandle();
			}
			else
			{
				menuHandle_t handle = child->ChildHandleForId( menuMgr, id );
				if ( handle.IsValid() )
				{
					return handle;
				}
			}
		}
	}
	return menuHandle_t();
}

//==============================
// VRMenuObjectLocal::GetLocalScale
Vector3f VRMenuObjectLocal::GetLocalScale() const 
{ 
    return Vector3f( LocalScale.x * HilightScale, LocalScale.y * HilightScale, LocalScale.z * HilightScale ); 
}

//==============================
// VRMenuObjectLocal::GetTextLocalScale
Vector3f VRMenuObjectLocal::GetTextLocalScale() const 
{
    return Vector3f( TextLocalScale.x * HilightScale, TextLocalScale.y * HilightScale, TextLocalScale.z * HilightScale ); 
}

//==============================
// VRMenuObjectLocal::SetSurfaceTexture
void  VRMenuObjectLocal::SetSurfaceTexture( int const surfaceIndex, int const textureIndex, 
        eSurfaceTextureType const type, GLuint const texId, int const width, int const height )
{
    if ( surfaceIndex < 0 || surfaceIndex >= Surfaces.GetSizeI() )
    {
        DROID_ASSERT( surfaceIndex >= 0 && surfaceIndex < Surfaces.GetSizeI(), "VrMenu" );
        return;
    }
    Surfaces[surfaceIndex].LoadTexture( textureIndex, type, texId, width, height );
}

//==============================
// VRMenuObjectLocal::SetSurfaceTexture
void  VRMenuObjectLocal::SetSurfaceTextureTakeOwnership( int const surfaceIndex, int const textureIndex,
	eSurfaceTextureType const type, GLuint const texId,
	int const width, int const height )
{
	if ( surfaceIndex < 0 || surfaceIndex >= Surfaces.GetSizeI() )
	{
		DROID_ASSERT( surfaceIndex >= 0 && surfaceIndex < Surfaces.GetSizeI(), "VrMenu" );
		return;
	}
	Surfaces[ surfaceIndex ].LoadTexture( textureIndex, type, texId, width, height );
	Surfaces[ surfaceIndex ].SetOwnership( textureIndex, true );
}

//==============================
// VRMenuObjectLocal::RegenerateSurfaceGeometry
void VRMenuObjectLocal::RegenerateSurfaceGeometry( int const surfaceIndex, const bool freeSurfaceGeometry )
{
	if ( surfaceIndex < 0 || surfaceIndex >= Surfaces.GetSizeI() )
	{
		DROID_ASSERT( surfaceIndex >= 0 && surfaceIndex < Surfaces.GetSizeI(), "VrMenu" );
		return;
	}

	if ( freeSurfaceGeometry )
	{
		Surfaces[ surfaceIndex ].Free();
	}

	Surfaces[ surfaceIndex ].RegenerateSurfaceGeometry();
}

//==============================
// VRMenuObjectLocal::GetSurfaceDims
Vector2f const & VRMenuObjectLocal::GetSurfaceDims( int const surfaceIndex ) const
{
	if ( surfaceIndex < 0 || surfaceIndex >= Surfaces.GetSizeI() )
	{
		DROID_ASSERT( surfaceIndex >= 0 && surfaceIndex < Surfaces.GetSizeI(), "VrMenu" );
		return Vector2f::ZERO;
	}

	return Surfaces[ surfaceIndex ].GetDims();
}

//==============================
// VRMenuObjectLocal::SetSurfaceDims
void VRMenuObjectLocal::SetSurfaceDims( int const surfaceIndex, Vector2f const &dims )
{
	if ( surfaceIndex < 0 || surfaceIndex >= Surfaces.GetSizeI() )
	{
		DROID_ASSERT( surfaceIndex >= 0 && surfaceIndex < Surfaces.GetSizeI(), "VrMenu" );
		return;
	}

	Surfaces[ surfaceIndex ].SetDims( dims );
}

//==============================
// VRMenuObjectLocal::GetSurfaceBorder
Vector4f const & VRMenuObjectLocal::GetSurfaceBorder( int const surfaceIndex )
{
	if ( surfaceIndex < 0 || surfaceIndex >= Surfaces.GetSizeI() )
	{
		DROID_ASSERT( surfaceIndex >= 0 && surfaceIndex < Surfaces.GetSizeI(), "VrMenu" );
		return Vector4f::ZERO;
	}

	return Surfaces[ surfaceIndex ].GetBorder();
}

//==============================
// VRMenuObjectLocal::SetSurfaceBorder
void VRMenuObjectLocal::SetSurfaceBorder( int const surfaceIndex, Vector4f const & border )
{
	if ( surfaceIndex < 0 || surfaceIndex >= Surfaces.GetSizeI() )
	{
		DROID_ASSERT( surfaceIndex >= 0 && surfaceIndex < Surfaces.GetSizeI(), "VrMenu" );
		return;
	}

	Surfaces[ surfaceIndex ].SetBorder( border );
}


//==============================
// VRMenuObjectLocal::SetLocalBoundsExpand
void VRMenuObjectLocal::SetLocalBoundsExpand( Vector3f const mins, Vector3f const & maxs )
{
	MinsBoundsExpand = mins;
	MaxsBoundsExpand = maxs;
}

//==============================
// VRMenuObjectLocal::SetCollisionPrimitive
void VRMenuObjectLocal::SetCollisionPrimitive( OvrCollisionPrimitive * c )
{
	if ( CollisionPrimitive != NULL )
	{
		delete CollisionPrimitive;
	}
	CollisionPrimitive = c;
}

//==============================
//  VRMenuObjectLocal::FindSurfaceWithTextureType
int VRMenuObjectLocal::FindSurfaceWithTextureType( eSurfaceTextureType const type, bool const singular ) const
{
	for ( int i = 0; i < Surfaces.GetSizeI(); ++i )
	{
		VRMenuSurface const & surf = Surfaces[i];
		int numTextures = 0;
		bool hasType = false;
		// we have to look through the surface images because we don't know how many are valid
		for ( int j = 0; j < VRMENUSURFACE_IMAGE_MAX; j++ )
		{
			VRMenuSurfaceTexture const & texture = surf.GetTexture( j );
			if ( texture.GetType() != SURFACE_TEXTURE_MAX )
			{
				numTextures++;
			}
			if ( texture.GetType() == type )
			{
				hasType = true;
			}
		}
		if ( hasType )
		{
			if ( !singular || ( singular && numTextures == 1 ) )
			{
				return i;
			}
		}
	}
	return -1;
}

//==============================
// VRMenuObjectLocal::SetSurfaceColor
void VRMenuObjectLocal::SetSurfaceColor( int const surfaceIndex, Vector4f const & color )
{
	VRMenuSurface & surf = Surfaces[surfaceIndex];
	surf.SetColor( color );
}

//==============================
// VRMenuObjectLocal::GetSurfaceColor
Vector4f const & VRMenuObjectLocal::GetSurfaceColor( int const surfaceIndex ) const
{
	VRMenuSurface const & surf = Surfaces[surfaceIndex];
	return surf.GetColor();
}

//==============================
// VRMenuObjectLocal::SetSurfaceVisible
void VRMenuObjectLocal::SetSurfaceVisible( int const surfaceIndex, bool const v )
{
	VRMenuSurface & surf = Surfaces[surfaceIndex];
	surf.SetVisible( v );
}

//==============================
// VRMenuObjectLocal::GetSurfaceVisible
bool VRMenuObjectLocal::GetSurfaceVisible( int const surfaceIndex ) const
{
	VRMenuSurface const & surf = Surfaces[surfaceIndex];
	return surf.GetVisible();
}

//==============================
// VRMenuObjectLocal::NumSurfaces
int VRMenuObjectLocal::NumSurfaces() const
{
	return Surfaces.GetSizeI(); 
}

//==============================
// VRMenuObjectLocal::AllocSurface
int VRMenuObjectLocal::AllocSurface()
{
	return Surfaces.AllocBack();
}


//==============================
// VRMenuObjectLocal::CreateFromSurfaceParms
void VRMenuObjectLocal::CreateFromSurfaceParms( int const surfaceIndex, VRMenuSurfaceParms const & parms )
{
	VRMenuSurface & surf = Surfaces[surfaceIndex];
	surf.CreateFromSurfaceParms( parms );
}

//==============================
// VRMenuObjectLocal::SetTextWordWrapped
void VRMenuObjectLocal::SetTextWordWrapped( char const * text, BitmapFont const & font, float const widthInMeters )
{
	SetText( text );
	font.WordWrapText( Text, widthInMeters, FontParms.Scale );
	WrapWidth = widthInMeters;
}

} // namespace OVR
