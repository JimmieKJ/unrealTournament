/************************************************************************************

Filename    :   ModelRender.cpp
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "ModelRender.h"

#include <stdlib.h>

#include "Android/GlUtils.h"
#include "Android/LogUtils.h"

#include "GlTexture.h"
#include "GlProgram.h"

namespace OVR
{

void ChangeGpuState( const GpuState oldState, const GpuState newState ) {
	if ( newState.blendEnable != oldState.blendEnable )
	{
		if ( newState.blendEnable )
		{
			glEnable( GL_BLEND );
		}
		else
		{
			glDisable( GL_BLEND );
		}
	}
	if ( newState.blendSrc != oldState.blendSrc || newState.blendDst != oldState.blendDst )
	{
		if ( newState.blendEnable == GpuState::BLEND_ENABLE_SEPARATE )
		{
			glBlendFuncSeparate( newState.blendSrc, newState.blendDst,
					newState.blendSrcAlpha, newState.blendDstAlpha );
		}
		else
		{
			glBlendFunc( newState.blendSrc, newState.blendDst );
		}
	}
	if ( newState.depthFunc != oldState.depthFunc )
	{
		glDepthFunc( newState.depthFunc );
	}
	if ( newState.depthEnable != oldState.depthEnable )
	{
		if ( newState.depthEnable )
		{
			glEnable( GL_DEPTH_TEST );
		}
		else
		{
			glDisable( GL_DEPTH_TEST );
		}
	}
	if ( newState.depthMaskEnable != oldState.depthMaskEnable )
	{
		if ( newState.depthMaskEnable )
		{
			glDepthMask( GL_TRUE );
		}
		else
		{
			glDepthMask( GL_FALSE );
		}
	}
	if ( newState.polygonOffsetEnable != oldState.polygonOffsetEnable )
	{
		if ( newState.polygonOffsetEnable )
		{
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( 1.0f, 1.0f );
		}
		else
		{
			glDisable( GL_POLYGON_OFFSET_FILL );
		}
	}
	// extend as needed
}

// transform transposed, as OpenGL will
Vector4f GLTransform( const Matrix4f & m, const Vector4f & v )
{
    return Vector4f(
		m.M[0][0] * v.x + m.M[1][0] * v.y + m.M[2][0] * v.z + m.M[3][0],
		m.M[0][1] * v.x + m.M[1][1] * v.y + m.M[2][1] * v.z + m.M[3][1],
		m.M[0][2] * v.x + m.M[1][2] * v.y + m.M[2][2] * v.z + m.M[3][2],
		m.M[0][3] * v.x + m.M[1][3] * v.y + m.M[2][3] * v.z + m.M[3][3] );
}

// Returns 0 if the bounds is culled by the mvp, otherwise returns the max W
// value of the bounds corners so it can be sorted into roughly front to back
// order for more efficient Z cull.  Sorting bounds in increasing order of
// their farthest W value usually makes characters and objects draw before
// the environments they are in, and draws sky boxes last, which is what we want.
float BoundsSortCullKey( const Bounds3f & bounds, const Matrix4f & mvp ) {
	Vector4f c[8];

	// Always cull empty bounds, which can be used to disable a surface.
	// Don't just check a single axis, or billboards would be culled.
	if ( bounds.b[1].x == bounds.b[0].x &&  bounds.b[1].y == bounds.b[0].y ) {
		return 0;
	}

	// Not very efficient code...
	for ( int i = 0; i < 8; i++ ) {
		Vector4f world;
		world.x = bounds.b[(i&1)].x;
		world.y = bounds.b[(i&2)>>1].y;
		world.z = bounds.b[(i&4)>>2].z;
		world.w = 1.0f;

		c[i] = GLTransform( mvp, world );
	}

	int i;
	for ( i = 0; i < 8; i++ ) {
		if ( c[i].x > -c[i].w ) {
			break;
		}
	}
	if ( i == 8 ) {
		return 0;	// all off one side
	}
	for ( i = 0; i < 8; i++ ) {
		if ( c[i].x < c[i].w ) {
			break;
		}
	}
	if ( i == 8 ) {
		return 0;	// all off one side
	}

	for ( i = 0; i < 8; i++ ) {
		if ( c[i].y > -c[i].w ) {
			break;
		}
	}
	if ( i == 8 ) {
		return 0;	// all off one side
	}
	for ( i = 0; i < 8; i++ ) {
		if ( c[i].y < c[i].w ) {
			break;
		}
	}
	if ( i == 8 ) {
		return 0;	// all off one side
	}

	for ( i = 0; i < 8; i++ ) {
		if ( c[i].z > -c[i].w ) {
			break;
		}
	}
	if ( i == 8 ) {
		return 0;	// all off one side
	}
	for ( i = 0; i < 8; i++ ) {
		if ( c[i].z < c[i].w ) {
			break;
		}
	}
	if ( i == 8 ) {
		return 0;	// all off one side
	}

	// calculate the farthest W point for front to back sorting
	float maxW = 0;
	for ( i = 0; i < 8; i++ ) {
		const float w = c[i].w;
		if ( w > maxW ) {
			maxW = w;
		}
	}

	return maxW;		// couldn't cull
}

struct bsort_t
{
	float						key;
	const DrawMatrices * 		matrices;
	const Array< Matrix4f > *	joints;
	const SurfaceDef *			surface;
	GLuint						textureOverload;	// if 0, there's no overload
	bool						transparent;
};

int bsortComp( const void * p1, const void * p2 )
{
	bsort_t const * b1 = static_cast< bsort_t const * >( p1 );
	bsort_t const * b2 = static_cast< bsort_t const * >( p2 );
	bool trans1 = b1->transparent;
	bool trans2 = b2->transparent;
	if ( trans1 == trans2 )
	{
		float f1 = b1->key;
		float f2 = b2->key;
		if ( !trans1 )
		{
			// both are solid, sort front-to-back
			if ( f1 < f2 ) 
			{
				return -1;
			}
			if ( f1 > f2 ) 
			{
				return 1;
			}
			return 0;
		}
		else
		{
			// both are transparent, sort back-to-front
			if ( f1 < f2 )
			{
				return 1;
			}
			if ( f1 > f2 )
			{
				return -1;
			}
			return 0;
		}
	}
	// otherwise, one is solid and one is translucent... the solid is always rendered first
	if ( trans1 ) 
	{
		return 1;
	}
	return -1;
}

const DrawSurfaceList & BuildDrawSurfaceList( const OVR::Array<ModelState> & modelRenderList,
			const Matrix4f & viewMatrix, const Matrix4f & projectionMatrix )
{
	// A mobile GPU will be in trouble if it draws more than this.
	static const int MAX_DRAW_SURFACES = 1024;
	bsort_t	bsort[ MAX_DRAW_SURFACES ];
	static const int MAX_DRAW_MODELS = 128;
	static DrawMatrices drawMatrices[MAX_DRAW_MODELS];

	const Matrix4f vpMatrix = ( projectionMatrix * viewMatrix ).Transposed();

	int	numSurfaces = 0;
	int	numDrawMatrices = 0;
	int	cullCount = 0;

	// Loop through all the models
	for ( int modelNum = 0; modelNum < modelRenderList.GetSizeI(); modelNum++ )
	{
		const ModelState & modelState = modelRenderList[ modelNum ];
		if ( modelState.Flags.Hide )
		{
			continue;
		}
		const ModelDef & modelDef = *modelState.modelDef;

		// make a table of surface texture overloads so we are only doing table look ups per-surface
		// most models will never have these
		const int MAX_TEXTURE_OVERLOADS_PER_MODEL = 16;
		int surfaceOverloads[MAX_TEXTURE_OVERLOADS_PER_MODEL] = { };
		for ( int overloadIdx = 0; overloadIdx < modelState.SurfaceTextureOverloads.GetSizeI(); ++overloadIdx )
		{
			const SurfaceTextureOverload & overload = modelState.SurfaceTextureOverloads[overloadIdx];
			if ( overload.SurfaceIndex >= MAX_TEXTURE_OVERLOADS_PER_MODEL )
			{
				continue;
			}
//			LOG( "surfaceOverloads[%i] = %u", overload.SurfaceIndex, overload.TextureId );
			surfaceOverloads[overload.SurfaceIndex] = overload.TextureId;
		}

		if ( numDrawMatrices == MAX_DRAW_MODELS ) 
		{
			break;
		}

		DrawMatrices & matrices = drawMatrices[numDrawMatrices++];

		matrices.Model = modelState.modelMatrix.Transposed();
		matrices.Mvp = matrices.Model * vpMatrix;

		for ( int surfaceNum = 0; surfaceNum < modelDef.surfaces.GetSizeI(); surfaceNum++ ) {
			const SurfaceDef & surfaceDef = modelDef.surfaces[ surfaceNum ];
			const float sort = BoundsSortCullKey( surfaceDef.cullingBounds, matrices.Mvp );
			if ( sort == 0 ) 
			{
				cullCount++;
				continue;
			}

			if ( numSurfaces == MAX_DRAW_SURFACES ) 
			{
				break;
			}

			bsort[ numSurfaces ].key = sort;
			bsort[ numSurfaces ].matrices = &matrices;
			bsort[ numSurfaces ].joints = &modelState.Joints;
			bsort[ numSurfaces ].surface = &surfaceDef;
			bsort[ numSurfaces ].textureOverload = surfaceNum < MAX_TEXTURE_OVERLOADS_PER_MODEL ? surfaceOverloads[surfaceNum] : 0;
			bsort[ numSurfaces ].transparent = surfaceDef.materialDef.gpuState.blendEnable;
			if ( bsort[ numSurfaces ].textureOverload > 0 )
			{
				LOG( "surfaceNum = %i, surfaceOverloads[surfaceNum] = %i, bsort[%i].textureOverload = %u", surfaceNum, surfaceOverloads[surfaceNum], numSurfaces, bsort[ numSurfaces ].textureOverload );
			}
			numSurfaces++;
		}
	}

	// sort by the far W
	qsort( bsort, numSurfaces, sizeof( bsort[0] ), bsortComp );

	// extract the drawSurface_t info
	static DrawSurface drawSurfaces[ MAX_DRAW_SURFACES ];
	for ( int i = 0; i < numSurfaces; i++ ) 
	{
		drawSurfaces[i].matrices = bsort[i].matrices;
		drawSurfaces[i].joints = bsort[i].joints;
		drawSurfaces[i].surface = bsort[i].surface;
		drawSurfaces[i].textureOverload = bsort[i].textureOverload;
	}

//	LOG( "Culled %i, draw %i", cullCount, numSurfaces );
	static DrawSurfaceList surfaceList;
	surfaceList.viewMatrix = viewMatrix.Transposed();
	surfaceList.projectionMatrix = projectionMatrix.Transposed();
	surfaceList.numDrawSurfaces = numSurfaces;
	surfaceList.drawSurfaces = drawSurfaces;
	surfaceList.numCulledSurfaces = cullCount;

	return surfaceList;
}


// Renders a list of pointers to models in order.
DrawCounters RenderSurfaceList( const DrawSurfaceList & drawSurfaceList ) {
	// This state could be made to persist across multiple calls to RenderModelList,
	// but the benefit would be small.
	GpuState			currentGpuState;
	GLuint				currentTextures[ MAX_PROGRAM_TEXTURES ] = {};	// TODO: This should be a range checked container.
	const DrawMatrices * currentMatrices = NULL;
	GLuint				currentProgramObject = 0;

	// default joints if no joints are specified
	static const Matrix4f defaultJoints[MAX_JOINTS];

	// counters
	DrawCounters counters;

	// Loop through all the surfaces
	for ( int surfaceNum = 0; surfaceNum < drawSurfaceList.numDrawSurfaces; surfaceNum++ ) 
	{
		const DrawSurface & drawSurface = drawSurfaceList.drawSurfaces[ surfaceNum ];
		const SurfaceDef & surfaceDef = *drawSurface.surface;
		const MaterialDef & materialDef = surfaceDef.materialDef;

		// Update GPU state -- blending, etc
		if ( !currentGpuState.Equals( materialDef.gpuState ) ) 
		{
			ChangeGpuState( currentGpuState, materialDef.gpuState );
			currentGpuState = materialDef.gpuState;
		}

		// Update texture bindings
		assert( materialDef.numTextures <= MAX_PROGRAM_TEXTURES );
		for ( int textureNum = 0; textureNum < materialDef.numTextures; textureNum++ ) 
		{
			const GLuint texNObj = ( drawSurface.textureOverload == 0 ) ? materialDef.textures[textureNum].texture : drawSurface.textureOverload;
			if ( currentTextures[textureNum] != texNObj )
			{
				counters.numTextureBinds++;
				currentTextures[textureNum] = texNObj;
				glActiveTexture( GL_TEXTURE0 + textureNum );
				// Something is leaving target set to 0; assume GL_TEXTURE_2D
				glBindTexture( materialDef.textures[textureNum].target ?
						materialDef.textures[textureNum].target : GL_TEXTURE_2D, texNObj );
			}
		}

		// Update program object
		assert( materialDef.programObject != 0 );
		if ( materialDef.programObject != currentProgramObject ) 
		{
			counters.numProgramBinds++;

			currentProgramObject = materialDef.programObject;
			glUseProgram( currentProgramObject );

			// It is possible the program still has the correct MVP,
			// but we don't want to track it, so reset it anyway.
			currentMatrices = NULL;
		}

		// Update the program parameters
		if ( drawSurface.matrices != currentMatrices ) 
		{
			counters.numParameterUpdates++;

			currentMatrices = drawSurface.matrices;

			// set the mvp matrix
			glUniformMatrix4fv( materialDef.uniformMvp, 1, 0, &currentMatrices->Mvp.M[0][0] );

			// set the model matrix
			if ( materialDef.uniformModel != -1 )
			{
				glUniformMatrix4fv( materialDef.uniformModel, 1, 0, &currentMatrices->Model.M[0][0] );
			}

			// set the view matrix
			if ( materialDef.uniformView != -1 )
			{
				glUniformMatrix4fv( materialDef.uniformView, 1, 0, &drawSurfaceList.viewMatrix.M[0][0] );
			}

			// set the projection matrix
			if ( materialDef.uniformProjection != -1 )
			{
				glUniformMatrix4fv( materialDef.uniformProjection, 1, 0, &drawSurfaceList.projectionMatrix.M[0][0] );
			}

			// set the joint matrices
			if ( materialDef.uniformJoints != -1 )
			{
				if ( drawSurface.joints != NULL && drawSurface.joints->GetSize() > 0 )
				{
					glUniformMatrix4fv( materialDef.uniformJoints, Alg::Min( drawSurface.joints->GetSizeI(), MAX_JOINTS ), 0, &drawSurface.joints->At( 0 ).M[0][0] );
				}
				else
				{
					glUniformMatrix4fv( materialDef.uniformJoints, MAX_JOINTS, 0, &defaultJoints[0].M[0][0] );
				}
			}
		}

		for ( int unif = 0; unif < MAX_PROGRAM_UNIFORMS; unif++ )
		{
			const int slot = materialDef.uniformSlots[unif];
			if ( slot == -1 )
			{
				break;
			}
			counters.numParameterUpdates++;
			glUniform4fv( slot, 1, materialDef.uniformValues[unif] );
		}

		counters.numDrawCalls++;

		// Bind all the vertex and element arrays
		surfaceDef.geo.Draw();
	}

	// set the gpu state back to the default
	ChangeGpuState( currentGpuState, GpuState() );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glUseProgram( 0 );
	glBindVertexArrayOES_( 0 );

	return counters;
}

void ModelState::SetSurfaceTextureOverload( const int surfaceIndex, const GLuint textureId )
{
	for ( int i = 0; i < SurfaceTextureOverloads.GetSizeI(); ++i )
	{
		if ( surfaceIndex == SurfaceTextureOverloads[i].SurfaceIndex )
		{
			SurfaceTextureOverloads[i].TextureId = textureId;
			return;
		}
	}
	SurfaceTextureOverload overload;
	overload.SurfaceIndex = surfaceIndex;
	overload.TextureId = textureId;
	SurfaceTextureOverloads.PushBack( overload );
}

void ModelState::ClearSurfaceTextureOverload( const int surfaceIndex )
{
	for ( int i = 0; i < SurfaceTextureOverloads.GetSizeI(); ++i )
	{
		if ( surfaceIndex == SurfaceTextureOverloads[i].SurfaceIndex )
		{
			SurfaceTextureOverloads.RemoveAtUnordered( i );
			return;
		}
	}
}

}	// namespace OVR
