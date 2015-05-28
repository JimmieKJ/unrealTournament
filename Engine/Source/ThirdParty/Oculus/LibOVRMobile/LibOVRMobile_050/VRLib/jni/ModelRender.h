/************************************************************************************

Filename    :   ModelRender.h
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_ModelRender_h
#define OVR_ModelRender_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Android/GlUtils.h"

#include "GlTexture.h"
#include "GlGeometry.h"

using namespace OVR;

namespace OVR
{

// can be made as high as 16
static const int MAX_PROGRAM_TEXTURES = 5;

// number of vec4
static const int MAX_PROGRAM_UNIFORMS = 1;

struct GpuState
{
	GpuState()
	{
		blendSrc = GL_ONE;
		blendDst = GL_ZERO;
		blendSrcAlpha = GL_ONE;
		blendDstAlpha = GL_ZERO;
		depthFunc = GL_LEQUAL;
		blendEnable = false;
		depthEnable = true;
		depthMaskEnable = true;
		polygonOffsetEnable = false;
	}

	bool Equals( const GpuState & a )
	{
		return blendSrc == a.blendSrc
				&& blendDst == a.blendDst
				&& blendSrcAlpha == a.blendSrcAlpha
				&& blendDstAlpha == a.blendDstAlpha
				&& depthFunc == a.depthFunc
				&& blendEnable == a.blendEnable
				&& depthEnable == a.depthEnable
				&& depthMaskEnable == a.depthMaskEnable
				&& polygonOffsetEnable == a.polygonOffsetEnable;
	}

	GLenum	blendSrc;
	GLenum	blendDst;
	GLenum	blendSrcAlpha;
	GLenum	blendDstAlpha;
	GLenum	depthFunc;
	static const int BLEND_DISABLE = 0;
	static const int BLEND_ENABLE = 1;
	static const int BLEND_ENABLE_SEPARATE = 2;
	unsigned char	blendEnable;	// off, normal, separate
	bool	depthEnable;
	bool	depthMaskEnable;
	bool	polygonOffsetEnable;
};

struct MaterialDef
{
	MaterialDef() :
		programObject( 0 ),
		uniformMvp( -1 ),
		uniformModel( -1 ),
		uniformView( -1 ),
		uniformProjection( -1 ),
		uniformJoints( -1 ),
		numTextures( 0 ) {
		uniformSlots[0] = -1;
	}

	GpuState	gpuState;				// blending, depth testing, etc

	// We might want to reference a gpuProgram_t object instead of copying these.
	GLuint		programObject;
	GLint		uniformMvp;
	GLint		uniformModel;
	GLint		uniformView;
	GLint		uniformProjection;
	GLint		uniformJoints;

	// Parameter setting stops when uniformSlots[x] == -1
	GLint		uniformSlots[MAX_PROGRAM_UNIFORMS];
	GLfloat		uniformValues[MAX_PROGRAM_UNIFORMS][4];

	// Additional uniforms for lighting and so on will need to be added.

	// Currently assumes GL_TEXTURE_2D for all; will need to be
	// extended for GL_TEXTURE_CUBE and GL_TEXTURE_EXTERNAL_OES.
	//
	// There should never be any 0 textures in the active elements.
	//
	// This should be a range checked container.
	int			numTextures;
	GlTexture	textures[MAX_PROGRAM_TEXTURES];
};

struct SurfaceDef
{
	SurfaceDef() {};

	// Name from the model file, can be used to control surfaces with code.
	// May be multiple semi-colon separated names if multiple source meshes
	// were merged into one surface.
	String			surfaceName;

	// We may want a do-not-cull flag for trivial quads and
	// skybox sorts of geometry.
	Bounds3f		cullingBounds;

	// There is a space savings to be had with triangle strips
	// if primitive restart is supported, but it is a net speed
	// loss on most architectures.  Adreno docs still recommend,
	// so it might be worth trying.
	GlGeometry		geo;

	// This could be a constant reference, but inline has some
	// advantages for now while the definition is small.
	MaterialDef		materialDef;
};

// This data is constant after model load, and can be referenced by
// multiple ModelState instances.
struct ModelDef
{
	ModelDef() {};

	OVR::Array<SurfaceDef>	surfaces;
};

struct SurfaceTextureOverload
{
	SurfaceTextureOverload() : SurfaceIndex( 0 ), TextureId( 0 ) { }

	int		SurfaceIndex;	// index to overload
	GLuint	TextureId;		// GL texture handle
};

struct ModelStateFlags
{
	ModelStateFlags() : Hide( false ), Pause( false ) {}
	bool				Hide;
	bool				Pause;
};

struct ModelState
{
	ModelState() : modelDef( NULL ) {}
	ModelState( const ModelDef & modelDef_ ) : modelDef( &modelDef_ ) { modelMatrix.Identity(); }

	const ModelDef *	modelDef;

	Matrix4f			modelMatrix;	// row major

	// Objects that retain the same modelMatrix for many
	// frames can sometimes benefit from having a world space
	// bounds calculated, which can be culled against the vp matrix
	// without building an mvp matrix.  The culling is slightly looser
	// because of axializing the rotated bounds.  Not doing it
	// for now.

	Array< Matrix4f >	Joints;			// OpenGL column major

	ModelStateFlags		Flags;

	void	SetSurfaceTextureOverload( const int surfaceIndex, const GLuint textureId );
	void	ClearSurfaceTextureOverload( const int surfaceIndex );

	// Other surface customization data will be added here.
	OVR::ArrayPOD< SurfaceTextureOverload > SurfaceTextureOverloads;
};

struct DrawCounters
{
	DrawCounters() : numElements( 0 ), numDrawCalls( 0 ), numProgramBinds( 0 ), numParameterUpdates( 0 ), numTextureBinds( 0 ) {}

	int		numElements;
	int		numDrawCalls;
	int		numProgramBinds;
	int		numParameterUpdates;		// MVP, etc
	int		numTextureBinds;
};

struct DrawMatrices
{
	// Avoid the overhead from initializing the matrices.
	DrawMatrices() :
		Model( Matrix4f::NoInit ),
		Mvp( Matrix4f::NoInit ) {}

	Matrix4f	Model;		// OpenGL column major
	Matrix4f	Mvp;		// OpenGL column major
};

struct DrawSurface
{
	void Clear() { matrices = NULL; joints = NULL; surface = NULL; textureOverload = 0; }
	const DrawMatrices *		matrices;			// OpenGL column major
	const Array< Matrix4f > *	joints;				// OpenGL column major
	const SurfaceDef *			surface;
	GLuint						textureOverload;	// if != 0, overload with this texture handle
};

struct DrawSurfaceList
{
	Matrix4f				viewMatrix;				// OpenGL column major
	Matrix4f				projectionMatrix;		// OpenGL column major
	int						numCulledSurfaces;		// just for developer feedback
	int						numDrawSurfaces;
	const DrawSurface *		drawSurfaces;
};


// Culls the surfaces in the model list to the MVP matrix and sorts front to back.
// Not thread safe, uses a static buffer for the surfaces.
// Additional, application specific culling or surface insertion can be done on the
// results of this call before calling DrawSurfaceList.
const DrawSurfaceList & BuildDrawSurfaceList( const OVR::Array<ModelState> & modelRenderList,
							const Matrix4f & viewMatrix, const Matrix4f & projectionMatrix );

// Draws a list of surfaces in order.
// Any sorting or culling should be performed before calling.
DrawCounters RenderSurfaceList( const DrawSurfaceList & drawSurfaceList );

} // namespace OVR

#endif	// OVR_ModelRender_h
