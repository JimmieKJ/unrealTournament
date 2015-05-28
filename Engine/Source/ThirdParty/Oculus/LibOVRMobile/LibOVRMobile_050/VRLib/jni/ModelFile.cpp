/************************************************************************************

Filename    :   ModelFile.cpp
Content     :   Model file loading.
Created     :   December 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "ModelFile.h"

#include <math.h>

#include "Kernel/OVR_Alg.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_String_Utils.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_BinaryFile.h"
#include "Kernel/OVR_MappedFile.h"
#include "Android/GlUtils.h"
#include "Android/LogUtils.h"

#include "unzip.h"
#include "GlTexture.h"
#include "ModelRender.h"


// Verbose log, redefine this as LOG() to get lots more info dumped
#define LOGV(...)

#define MEMORY_MAPPED	1

namespace OVR {

//-----------------------------------------------------------------------------
//	ModelFile
//-----------------------------------------------------------------------------

ModelFile::ModelFile() :
	UsingSrgbTextures( false )
{
}

ModelFile::~ModelFile()
{
	LOG( "Destroying ModelFileModel %s", FileName.ToCStr() );

	for ( int i = 0; i < Textures.GetSizeI(); i++ )
	{
		FreeTexture( Textures[i].texid );
	}

	for ( int j = 0; j < Def.surfaces.GetSizeI(); j++ )
	{
		const_cast<GlGeometry *>(&Def.surfaces[j].geo)->Free();
	}
}

SurfaceDef * ModelFile::FindNamedSurface( const char * name ) const
{
	for ( int j = 0; j < Def.surfaces.GetSizeI(); j++ )
	{
		const SurfaceDef & sd = Def.surfaces[j];
		if ( sd.surfaceName.CompareNoCase( name ) == 0 )
		{
			LOG( "Found named surface %s", name );
			return const_cast<SurfaceDef*>(&sd);
		}
	}
	LOG( "Did not find named surface %s", name );
	return NULL;
}

const ModelTexture * ModelFile::FindNamedTexture( const char * name ) const
{
	for ( int i = 0; i < Textures.GetSizeI(); i++ )
	{
		const ModelTexture & st = Textures[i];
		if ( st.name.CompareNoCase( name ) == 0 )
		{
			LOG( "Found named texture %s", name );
			return &st;
		}
	}
	LOG( "Did not find named texture %s", name );
	return NULL;
}

const ModelJoint * ModelFile::FindNamedJoint( const char *name ) const
{
	for ( int i = 0; i < Joints.GetSizeI(); i++ )
	{
		const ModelJoint & joint = Joints[i];
		if ( joint.name.CompareNoCase( name ) == 0 )
		{
			LOG( "Found named joint %s", name );
			return &joint;
		}
	}
	LOG( "Did not find named joint %s", name );
	return NULL;
}

const ModelTag * ModelFile::FindNamedTag( const char *name ) const
{
	for ( int i = 0; i < Tags.GetSizeI(); i++ )
	{
		const ModelTag & tag = Tags[i];
		if ( tag.name.CompareNoCase( name ) == 0 )
		{
			LOG( "Found named tag %s", name );
			return &tag;
		}
	}
	LOG( "Did not find named tag %s", name );
	return NULL;
}

Bounds3f ModelFile::GetBounds() const
{
	Bounds3f modelBounds;
	modelBounds.Clear();
	for ( int j = 0; j < Def.surfaces.GetSizeI(); j++ )
	{
		const SurfaceDef & sd = Def.surfaces[j];
		modelBounds.AddPoint( sd.cullingBounds.b[0] );
		modelBounds.AddPoint( sd.cullingBounds.b[1] );
	}
	return modelBounds;
}

//-----------------------------------------------------------------------------
//	Model Loading
//-----------------------------------------------------------------------------

void LoadModelFileTexture( ModelFile & model, const char * textureName,
							const char * buffer, const int size, const MaterialParms & materialParms )
{
	ModelTexture tex;
	tex.name = textureName;
	tex.name.StripExtension();
    int width;
    int height;
	tex.texid = LoadTextureFromBuffer( textureName, MemBuffer( buffer, size ),
			materialParms.UseSrgbTextureFormats ? TextureFlags_t( TEXTUREFLAG_USE_SRGB ) : TextureFlags_t(), 
			width, height );

	// LOG( ( tex.texid.target == GL_TEXTURE_CUBE_MAP ) ? "GL_TEXTURE_CUBE_MAP: %s" : "GL_TEXTURE_2D: %s", textureName );

	// file name metadata for enabling clamp mode
	// Used for sky sides in Tuscany.
	if ( strstr( textureName, "_c." ) )
	{
		MakeTextureClamped( tex.texid );
	}

	model.Textures.PushBack( tex );
}

template< typename _type_ >
void ReadModelArray( Array< _type_ > & out, const char * string, const BinaryReader & bin, const int numElements )
{
	if ( string != NULL && string[0] != '\0' && numElements > 0 )
	{
		if ( !bin.ReadArray( out, numElements ) )
		{
			StringUtils::StringTo( out, string );
		}
	}
}

void LoadModelFileJson( ModelFile & model,
						const char * modelsJson, const int modelsJsonLength,
						const char * modelsBin, const int modelsBinLength,
						const ModelGlPrograms & programs, const MaterialParms & materialParms )
{
	LOG( "parsing %s", model.FileName.ToCStr() );

	const BinaryReader bin( (const UByte *)modelsBin, modelsBinLength );

	if ( modelsBin != NULL && bin.ReadUInt32() != 0x6272766F )
	{
		LOG( "LoadModelFileJson: bad binary file for %s", model.FileName.ToCStr() );
		return;
	}

	const char * error = NULL;
	JSON * json = JSON::Parse( modelsJson, & error );
	if ( json == NULL )
	{
		WARN( "LoadModelFileJson: Error loading %s : %s", model.FileName.ToCStr(), error );
		return;
	}

	const JsonReader models( json );
	if ( models.IsObject() )
	{
		//
		// Render Model
		//

		const JsonReader render_model( models.GetChildByName( "render_model" ) );
		if ( render_model.IsObject() )
		{
			LOG( "loading render model.." );

			//
			// Render Model Textures
			//

			enum TextureOcclusion
			{
				TEXTURE_OCCLUSION_OPAQUE,
				TEXTURE_OCCLUSION_PERFORATED,
				TEXTURE_OCCLUSION_TRANSPARENT
			};

			Array< GlTexture > glTextures;

			const JsonReader texture_array( render_model.GetChildByName( "textures" ) );
			if ( texture_array.IsArray() )
			{
				while ( !texture_array.IsEndOfArray() )
				{
					const JsonReader texture( texture_array.GetNextArrayElement() );
					if ( texture.IsObject() )
					{
						const String name = texture.GetChildStringByName( "name" );

						// Try to match the texture names with the already loaded texture
						// and create a default texture if the texture file is missing.
						int i = 0;
						for ( ; i < model.Textures.GetSizeI(); i++ )
						{
							if ( model.Textures[i].name.CompareNoCase( name ) == 0 )
							{
								break;
							}
						}
						if ( i == model.Textures.GetSizeI() )
						{
							LOG( "texture %s defaulted", name.ToCStr() );
							// Create a default texture.
							LoadModelFileTexture( model, name, NULL, 0, materialParms );
						}
						glTextures.PushBack( model.Textures[i].texid );

						const String usage = texture.GetChildStringByName( "usage" );
						if ( usage == "diffuse" )
						{
							if ( materialParms.EnableDiffuseAniso == true )
							{
								MakeTextureAniso( model.Textures[i].texid, 2.0f );
							}
						}
						else if ( usage == "emissive" )
						{
							if ( materialParms.EnableEmissiveLodClamp == true )
							{
								// LOD clamp lightmap textures to avoid light bleeding
								MakeTextureLodClamped( model.Textures[i].texid, 1 );
							}
						}
						/*
						const String occlusion = texture.GetChildStringByName( "occlusion" );

						TextureOcclusion textureOcclusion = TEXTURE_OCCLUSION_OPAQUE;
						if ( occlusion == "opaque" )			{ textureOcclusion = TEXTURE_OCCLUSION_OPAQUE; }
						else if ( occlusion == "perforated" )	{ textureOcclusion = TEXTURE_OCCLUSION_PERFORATED; }
						else if ( occlusion == "transparent" )	{ textureOcclusion = TEXTURE_OCCLUSION_TRANSPARENT; }
						*/
					}
				}
			}

			//
			// Render Model Joints
			//

			const JsonReader joint_array( render_model.GetChildByName( "joints" ) );
			if ( joint_array.IsArray() )
			{
				model.Joints.Clear();

				while ( !joint_array.IsEndOfArray() )
				{
					const JsonReader joint( joint_array.GetNextArrayElement() );
					if ( joint.IsObject() )
					{
						const UPInt index = model.Joints.AllocBack();
						model.Joints[index].index = index;
						model.Joints[index].name = joint.GetChildStringByName( "name" );
						StringUtils::StringTo( model.Joints[index].transform, joint.GetChildStringByName( "transform" ) );
						model.Joints[index].animation = MODEL_JOINT_ANIMATION_NONE;
						const String animation = joint.GetChildStringByName( "animation" );
						if ( animation == "none" )			{ model.Joints[index].animation = MODEL_JOINT_ANIMATION_NONE; }
						else if ( animation == "rotate" )	{ model.Joints[index].animation = MODEL_JOINT_ANIMATION_ROTATE; }
						else if ( animation == "sway" )		{ model.Joints[index].animation = MODEL_JOINT_ANIMATION_SWAY; }
						else if ( animation == "bob" )		{ model.Joints[index].animation = MODEL_JOINT_ANIMATION_BOB; }
						model.Joints[index].parameters.x = joint.GetChildFloatByName( "parmX" );
						model.Joints[index].parameters.y = joint.GetChildFloatByName( "parmY" );
						model.Joints[index].parameters.z = joint.GetChildFloatByName( "parmZ" );
						model.Joints[index].timeOffset = joint.GetChildFloatByName( "timeOffset" );
						model.Joints[index].timeScale = joint.GetChildFloatByName( "timeScale" );
					}
				}
			}

			//
			// Render Model Tags
			//

			const JsonReader tag_array( render_model.GetChildByName( "tags" ) );
			if ( tag_array.IsArray() )
			{
				model.Tags.Clear();

				while ( !tag_array.IsEndOfArray() )
				{
					const JsonReader tag( tag_array.GetNextArrayElement() );
					if ( tag.IsObject() )
					{
						const UPInt index = model.Tags.AllocBack();
						model.Tags[index].name = tag.GetChildStringByName( "name" );
						StringUtils::StringTo( model.Tags[index].matrix, 		tag.GetChildStringByName( "matrix" ) );
						StringUtils::StringTo( model.Tags[index].jointIndices, 	tag.GetChildStringByName( "jointIndices" ) );
						StringUtils::StringTo( model.Tags[index].jointWeights, 	tag.GetChildStringByName( "jointWeights" ) );
					}
				}
			}

			//
			// Render Model Surfaces
			//

			const JsonReader surface_array( render_model.GetChildByName( "surfaces" ) );
			if ( surface_array.IsArray() )
			{
				while ( !surface_array.IsEndOfArray() )
				{
					const JsonReader surface( surface_array.GetNextArrayElement() );
					if ( surface.IsObject() )
					{
						const UPInt index = model.Def.surfaces.AllocBack();

						//
						// Source Meshes
						//

						const JsonReader source( surface.GetChildByName( "source" ) );
						if ( source.IsArray() )
						{
							while ( !source.IsEndOfArray() )
							{
								if ( model.Def.surfaces[index].surfaceName.GetLength() )
								{
									model.Def.surfaces[index].surfaceName += ";";
								}
								model.Def.surfaces[index].surfaceName += source.GetNextArrayString();
							}
						}

						LOGV( "surface %s", model.Def.surfaces[index].surfaceName.ToCStr() );

						//
						// Surface Material
						//

						enum
						{
							MATERIAL_TYPE_OPAQUE,
							MATERIAL_TYPE_PERFORATED,
							MATERIAL_TYPE_TRANSPARENT,
							MATERIAL_TYPE_ADDITIVE
						} materialType = MATERIAL_TYPE_OPAQUE;

						int diffuseTextureIndex = -1;
						int normalTextureIndex = -1;
						int specularTextureIndex = -1;
						int emissiveTextureIndex = -1;
						int reflectionTextureIndex = -1;

						const JsonReader material( surface.GetChildByName( "material" ) );
						if ( material.IsObject() )
						{
							const String type = material.GetChildStringByName( "type" );

							if ( type == "opaque" )				{ materialType = MATERIAL_TYPE_OPAQUE; }
							else if ( type == "perforated" )	{ materialType = MATERIAL_TYPE_PERFORATED; }
							else if ( type == "transparent" )	{ materialType = MATERIAL_TYPE_TRANSPARENT; }
							else if ( type == "additive" )		{ materialType = MATERIAL_TYPE_ADDITIVE; }

							diffuseTextureIndex		= material.GetChildInt32ByName( "diffuse", -1 );
							normalTextureIndex		= material.GetChildInt32ByName( "normal", -1 );
							specularTextureIndex	= material.GetChildInt32ByName( "specular", -1 );
							emissiveTextureIndex	= material.GetChildInt32ByName( "emissive", -1 );
							reflectionTextureIndex	= material.GetChildInt32ByName( "reflection", -1 );
						}

						//
						// Surface Bounds
						//

						StringUtils::StringTo( model.Def.surfaces[index].cullingBounds, surface.GetChildStringByName( "bounds" ) );

						//
						// Vertices
						//

						VertexAttribs attribs;

						const JsonReader vertices( surface.GetChildByName( "vertices" ) );
						if ( vertices.IsObject() )
						{
							const int vertexCount = Alg::Min( vertices.GetChildInt32ByName( "vertexCount" ), MAX_GEOMETRY_VERTICES );
							// LOG( "%5d vertices", vertexCount );

							ReadModelArray( attribs.position,     vertices.GetChildStringByName( "position" ),		bin, vertexCount );
							ReadModelArray( attribs.normal,       vertices.GetChildStringByName( "normal" ),		bin, vertexCount );
							ReadModelArray( attribs.tangent,      vertices.GetChildStringByName( "tangent" ),		bin, vertexCount );
							ReadModelArray( attribs.binormal,     vertices.GetChildStringByName( "binormal" ),		bin, vertexCount );
							ReadModelArray( attribs.color,        vertices.GetChildStringByName( "color" ),			bin, vertexCount );
							ReadModelArray( attribs.uv0,          vertices.GetChildStringByName( "uv0" ),			bin, vertexCount );
							ReadModelArray( attribs.uv1,          vertices.GetChildStringByName( "uv1" ),			bin, vertexCount );
							ReadModelArray( attribs.jointIndices, vertices.GetChildStringByName( "jointIndices" ),	bin, vertexCount );
							ReadModelArray( attribs.jointWeights, vertices.GetChildStringByName( "jointWeights" ),	bin, vertexCount );
						}

						//
						// Triangles
						//

						Array< TriangleIndex > indices;

						const JsonReader triangles( surface.GetChildByName( "triangles" ) );
						if ( triangles.IsObject() )
						{
							const int indexCount = Alg::Min( triangles.GetChildInt32ByName( "indexCount" ), MAX_GEOMETRY_INDICES );
							// LOG( "%5d indices", indexCount );

							ReadModelArray( indices, triangles.GetChildStringByName( "indices" ), bin, indexCount );
						}

						//
						// Setup geometry, textures and render programs now that the vertex attributes are known.
						//

						model.Def.surfaces[index].geo.Create( attribs, indices );

						const char * materialTypeString = "opaque";
						OVR_UNUSED( materialTypeString );	// we'll get warnings if the LOGV's compile out

						// set up additional material flags for the surface
						if ( materialType == MATERIAL_TYPE_PERFORATED )
						{
							// Just blend because alpha testing is rather expensive.
							model.Def.surfaces[index].materialDef.gpuState.blendEnable = true;
							model.Def.surfaces[index].materialDef.gpuState.depthMaskEnable = false;
							model.Def.surfaces[index].materialDef.gpuState.blendSrc = GL_SRC_ALPHA;
							model.Def.surfaces[index].materialDef.gpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
							materialTypeString = "perforated";
						}
						else if ( materialType == MATERIAL_TYPE_TRANSPARENT || materialParms.Transparent )
						{
							model.Def.surfaces[index].materialDef.gpuState.blendEnable = true;
							model.Def.surfaces[index].materialDef.gpuState.depthMaskEnable = false;
							model.Def.surfaces[index].materialDef.gpuState.blendSrc = GL_SRC_ALPHA;
							model.Def.surfaces[index].materialDef.gpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
							materialTypeString = "transparent";
						}
						else if ( materialType == MATERIAL_TYPE_ADDITIVE )
						{
							model.Def.surfaces[index].materialDef.gpuState.blendEnable = true;
							model.Def.surfaces[index].materialDef.gpuState.depthMaskEnable = false;
							model.Def.surfaces[index].materialDef.gpuState.blendSrc = GL_ONE;
							model.Def.surfaces[index].materialDef.gpuState.blendDst = GL_ONE;
							materialTypeString = "additive";
						}

						const bool skinned = (	attribs.jointIndices.GetSize() == attribs.position.GetSize() &&
												attribs.jointWeights.GetSize() == attribs.position.GetSize() );

						if ( diffuseTextureIndex >= 0 && diffuseTextureIndex < glTextures.GetSizeI() )
						{
							model.Def.surfaces[index].materialDef.textures[0] = glTextures[diffuseTextureIndex];

							if ( emissiveTextureIndex >= 0 && emissiveTextureIndex < glTextures.GetSizeI() )
							{
								model.Def.surfaces[index].materialDef.textures[1] = glTextures[emissiveTextureIndex];

								if (	normalTextureIndex >= 0 && normalTextureIndex < glTextures.GetSizeI() &&
										specularTextureIndex >= 0 && specularTextureIndex < glTextures.GetSizeI() &&
										reflectionTextureIndex >= 0 && reflectionTextureIndex < glTextures.GetSizeI() )
								{
									// reflection mapped material;
									model.Def.surfaces[index].materialDef.textures[2] = glTextures[normalTextureIndex];
									model.Def.surfaces[index].materialDef.textures[3] = glTextures[specularTextureIndex];
									model.Def.surfaces[index].materialDef.textures[4] = glTextures[reflectionTextureIndex];

									model.Def.surfaces[index].materialDef.numTextures = 5;
									if ( skinned )
									{
										if ( programs.ProgSkinnedReflectionMapped == NULL )
										{
											FAIL( "No ProgSkinnedReflectionMapped set");
										}
										model.Def.surfaces[index].materialDef.programObject = programs.ProgSkinnedReflectionMapped->program;
										model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgSkinnedReflectionMapped->uMvp;
										model.Def.surfaces[index].materialDef.uniformModel = programs.ProgSkinnedReflectionMapped->uModel;
										model.Def.surfaces[index].materialDef.uniformView = programs.ProgSkinnedReflectionMapped->uView;
										model.Def.surfaces[index].materialDef.uniformJoints = programs.ProgSkinnedReflectionMapped->uJoints;
										LOGV( "%s skinned reflection mapped material", materialTypeString );
									}
									else
									{
										if ( programs.ProgReflectionMapped == NULL )
										{
											FAIL( "No ProgReflectionMapped set");
										}
										model.Def.surfaces[index].materialDef.programObject = programs.ProgReflectionMapped->program;
										model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgReflectionMapped->uMvp;
										model.Def.surfaces[index].materialDef.uniformModel = programs.ProgReflectionMapped->uModel;
										model.Def.surfaces[index].materialDef.uniformView = programs.ProgReflectionMapped->uView;
										LOGV( "%s reflection mapped material", materialTypeString );
									}
								}
								else
								{
									// light mapped material
									model.Def.surfaces[index].materialDef.numTextures = 2;
									if ( skinned )
									{
										if ( programs.ProgSkinnedLightMapped == NULL )
										{
											FAIL( "No ProgSkinnedLightMapped set");
										}
										model.Def.surfaces[index].materialDef.programObject = programs.ProgSkinnedLightMapped->program;
										model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgSkinnedLightMapped->uMvp;
										model.Def.surfaces[index].materialDef.uniformJoints = programs.ProgSkinnedLightMapped->uJoints;
										LOGV( "%s skinned light mapped material", materialTypeString );
									}
									else
									{
										if ( programs.ProgLightMapped == NULL )
										{
											FAIL( "No ProgLightMapped set");
										}
										model.Def.surfaces[index].materialDef.programObject = programs.ProgLightMapped->program;
										model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgLightMapped->uMvp;
										LOGV( "%s light mapped material", materialTypeString );
									}
								}
							}
							else 
							{
								// diffuse only material
								model.Def.surfaces[index].materialDef.numTextures = 1;
								if ( skinned )
								{
									if ( programs.ProgSkinnedSingleTexture == NULL )
									{
										FAIL( "No ProgSkinnedSingleTexture set");
									}
									model.Def.surfaces[index].materialDef.programObject = programs.ProgSkinnedSingleTexture->program;
									model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgSkinnedSingleTexture->uMvp;
									model.Def.surfaces[index].materialDef.uniformJoints = programs.ProgSkinnedSingleTexture->uJoints;
									LOGV( "%s skinned diffuse only material", materialTypeString );
								}
								else
								{
									if ( programs.ProgSingleTexture == NULL )
									{
										FAIL( "No ProgSingleTexture set");
									}
									model.Def.surfaces[index].materialDef.programObject = programs.ProgSingleTexture->program;
									model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgSingleTexture->uMvp;
									LOGV( "%s diffuse only material", materialTypeString );
								}
							}
						}
						else if ( attribs.color.GetSizeI() > 0 )
						{
							// vertex color material
							model.Def.surfaces[index].materialDef.numTextures = 0;
							if ( skinned )
							{
								if ( programs.ProgSkinnedVertexColor == NULL )
								{
									FAIL( "No ProgSkinnedVertexColor set");
								}
								model.Def.surfaces[index].materialDef.programObject = programs.ProgSkinnedVertexColor->program;
								model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgSkinnedVertexColor->uMvp;
								LOGV( "%s skinned vertex color material", materialTypeString );
							}
							else
							{
								if ( programs.ProgVertexColor == NULL )
								{
									FAIL( "No ProgVertexColor set");
								}
								model.Def.surfaces[index].materialDef.programObject = programs.ProgVertexColor->program;
								model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgVertexColor->uMvp;
								LOGV( "%s vertex color material", materialTypeString );
							}
						}
						else
						{
							// surface without texture or vertex colors
							model.Def.surfaces[index].materialDef.textures[0] = 0;
							model.Def.surfaces[index].materialDef.numTextures = 1;
							if ( skinned )
							{
								if ( programs.ProgSkinnedSingleTexture == NULL )
								{
									FAIL( "No ProgSkinnedSingleTexture set");
								}
								model.Def.surfaces[index].materialDef.programObject = programs.ProgSkinnedSingleTexture->program;
								model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgSingleTexture->uMvp;
								LOGV( "%s skinned default texture material", materialTypeString );
							}
							else
							{
								if ( programs.ProgSingleTexture == NULL )
								{
									FAIL( "No ProgSingleTexture set");
								}
								model.Def.surfaces[index].materialDef.programObject = programs.ProgSingleTexture->program;
								model.Def.surfaces[index].materialDef.uniformMvp = programs.ProgSingleTexture->uMvp;
								LOGV( "%s default texture material", materialTypeString );
							}
						}

						if ( materialParms.PolygonOffset )
						{
							model.Def.surfaces[index].materialDef.gpuState.polygonOffsetEnable = true;
							LOGV( "polygon offset material" );
						}
					}
				}
			}
		}

		//
		// Collision Model
		//

		const JsonReader collision_model( models.GetChildByName( "collision_model" ) );
		if ( collision_model.IsArray() )
		{
			LOGV( "loading collision model.." );

			while ( !collision_model.IsEndOfArray() )
			{
				const UPInt index = model.Collisions.Polytopes.AllocBack();

				const JsonReader polytope( collision_model.GetNextArrayElement() );
				if ( polytope.IsObject() )
				{
					model.Collisions.Polytopes[index].Name = polytope.GetChildStringByName( "name" );
					StringUtils::StringTo( model.Collisions.Polytopes[index].Planes, polytope.GetChildStringByName( "planes" ) );
				}
			}
		}

		//
		// Ground Collision Model
		//

		const JsonReader ground_collision_model( models.GetChildByName( "ground_collision_model" ) );
		if ( ground_collision_model.IsArray() )
		{
			LOGV( "loading ground collision model.." );

			while ( !ground_collision_model.IsEndOfArray() )
			{
				const UPInt index = model.GroundCollisions.Polytopes.AllocBack();

				const JsonReader polytope( ground_collision_model.GetNextArrayElement() );
				if ( polytope.IsObject() )
				{
					model.GroundCollisions.Polytopes[index].Name = polytope.GetChildStringByName( "name" );
					StringUtils::StringTo( model.GroundCollisions.Polytopes[index].Planes, polytope.GetChildStringByName( "planes" ) );
				}
			}
		}

		//
		// Ray-Trace Model
		//

		const JsonReader raytrace_model( models.GetChildByName( "raytrace_model" ) );
		if ( raytrace_model.IsObject() )
		{
			LOGV( "loading ray-trace model.." );

			ModelTrace &traceModel = model.TraceModel;

			traceModel.header.numVertices	= raytrace_model.GetChildInt32ByName( "numVertices" );
			traceModel.header.numUvs		= raytrace_model.GetChildInt32ByName( "numUvs" );
			traceModel.header.numIndices	= raytrace_model.GetChildInt32ByName( "numIndices" );
			traceModel.header.numNodes		= raytrace_model.GetChildInt32ByName( "numNodes" );
			traceModel.header.numLeafs		= raytrace_model.GetChildInt32ByName( "numLeafs" );
			traceModel.header.numOverflow	= raytrace_model.GetChildInt32ByName( "numOverflow" );

			StringUtils::StringTo( traceModel.header.bounds, raytrace_model.GetChildStringByName( "bounds" ) );

			ReadModelArray( traceModel.vertices, raytrace_model.GetChildStringByName( "vertices" ), bin, traceModel.header.numVertices );
			ReadModelArray( traceModel.uvs, raytrace_model.GetChildStringByName( "uvs" ), bin, traceModel.header.numUvs );
			ReadModelArray( traceModel.indices, raytrace_model.GetChildStringByName( "indices" ), bin, traceModel.header.numIndices );

			if ( !bin.ReadArray( traceModel.nodes, traceModel.header.numNodes ) )
			{
				const JsonReader nodes_array( raytrace_model.GetChildByName( "nodes" ) );
				if ( nodes_array.IsArray() )
				{
					while ( !nodes_array.IsEndOfArray() )
					{
						const UPInt index = traceModel.nodes.AllocBack();

						const JsonReader node( nodes_array.GetNextArrayElement() );
						if ( node.IsObject() )
						{
							traceModel.nodes[index].data = (UInt32) node.GetChildInt64ByName( "data" );
							traceModel.nodes[index].dist = node.GetChildFloatByName( "dist" );
						}
					}
				}
			}

			if ( !bin.ReadArray( traceModel.leafs, traceModel.header.numLeafs ) )
			{
				const JsonReader leafs_array( raytrace_model.GetChildByName( "leafs" ) );
				if ( leafs_array.IsArray() )
				{
					while ( !leafs_array.IsEndOfArray() )
					{
						const UPInt index = traceModel.leafs.AllocBack();

						const JsonReader leaf( leafs_array.GetNextArrayElement() );
						if ( leaf.IsObject() )
						{
							StringUtils::StringTo( traceModel.leafs[index].triangles, RT_KDTREE_MAX_LEAF_TRIANGLES, leaf.GetChildStringByName( "triangles" ) );
							StringUtils::StringTo( traceModel.leafs[index].ropes, 6, leaf.GetChildStringByName( "ropes" ) );
							StringUtils::StringTo( traceModel.leafs[index].bounds, leaf.GetChildStringByName( "bounds" ) );
						}
					}
				}
			}

			ReadModelArray( traceModel.overflow, raytrace_model.GetChildStringByName( "overflow" ), bin, traceModel.header.numOverflow );
		}
	}
	json->Release();

	if ( !bin.IsAtEnd() )
	{
		WARN( "failed to properly read binary file" );
	}
}

static ModelFile * LoadModelFile( unzFile zfp, const char * fileName,
								const char * fileData, const int fileDataLength,
								const ModelGlPrograms & programs,
								const MaterialParms & materialParms )
{
	const LogCpuTime logTime( "LoadModelFile" );

	ModelFile * modelPtr = new ModelFile;
	ModelFile & model = *modelPtr;

	model.FileName = fileName;
	model.UsingSrgbTextures = materialParms.UseSrgbTextureFormats;

	if ( !zfp )
	{
		WARN( "Error: can't load %s", fileName );
		return modelPtr;
	}

	// load all texture files and locate the model files

	const char * modelsJson = NULL;
	int modelsJsonLength = 0;

	const char * modelsBin = NULL;
	int modelsBinLength = 0;

	for ( int ret = unzGoToFirstFile( zfp ); ret == UNZ_OK; ret = unzGoToNextFile( zfp ) )
	{
		unz_file_info finfo;
		char entryName[256];
		unzGetCurrentFileInfo( zfp, &finfo, entryName, sizeof( entryName ), NULL, 0, NULL, 0 );
		LOGV( "zip level: %ld, file: %s", finfo.compression_method, entryName );

		if ( unzOpenCurrentFile( zfp ) != UNZ_OK )
		{
			WARN( "Failed to open %s from %s", entryName, fileName );
			continue;
		}
		
		const int size = finfo.uncompressed_size;
		char * buffer = NULL;

		if ( finfo.compression_method == 0 && fileData != NULL )
		{
			buffer = (char *)fileData + unzGetCurrentFileZStreamPos64( zfp );
		}
		else
		{
			buffer = new char[size + 1];
			buffer[size] = '\0';	// always zero terminate text files

			if ( unzReadCurrentFile( zfp, buffer, size ) != size )
			{
				WARN( "Failed to read %s from %s", entryName, fileName );
				delete [] buffer;
				continue;
			}
		}

		// assume a 3 character extension
		const size_t entryLength = strlen( entryName );
		const char * extension = ( entryLength >= 4 ) ? &entryName[entryLength - 4] : entryName;

		if ( strcasecmp( entryName, "models.json" ) == 0 )
		{
			// save this for parsing
			modelsJson = (const char *)buffer;
			modelsJsonLength = size;
			buffer = NULL;	// don't free it now
		}
		else if ( strcasecmp( entryName, "models.bin" ) == 0 )
		{
			// save this for parsing
			modelsBin = (const char *)buffer;
			modelsBinLength = size;
			buffer = NULL;	// don't free it now
		}
		else if (	strcasecmp( extension, ".pvr" ) == 0 ||
					strcasecmp( extension, ".ktx" ) == 0 )
		{
			// only support .pvr and .ktx containers for now
			LoadModelFileTexture( model, entryName, buffer, size, materialParms );
		}
		else
		{
			// ignore other files
			LOG( "Ignoring %s", entryName );
		}

		if ( buffer < fileData || buffer > fileData + fileDataLength )
		{
			delete [] buffer;
		}

		unzCloseCurrentFile( zfp );
	}
	unzClose( zfp );

	if ( modelsJson != NULL )
	{
		LoadModelFileJson( model,
							modelsJson, modelsJsonLength,
							modelsBin, modelsBinLength,
							programs, materialParms );
	}

	if ( modelsJson < fileData || modelsJson > fileData + fileDataLength )
	{
		delete modelsJson;
	}
	if ( modelsBin < fileData || modelsBin > fileData + fileDataLength )
	{
		delete modelsBin;
	}

	return modelPtr;
}

#if MEMORY_MAPPED

// Memory-mapped wrapper for ZLIB/MiniZip

struct zlib_mmap_opaque
{
	MappedFile		file;
	MappedView		view;
	const UByte *	data;
	const UByte *	ptr;
	int				len;
	int				left;
};

static voidpf ZCALLBACK mmap_fopen_file_func( voidpf opaque, const char *, int )
{
	return opaque;
}

static uLong ZCALLBACK mmap_fread_file_func( voidpf opaque, voidpf, void * buf, uLong size )
{
	zlib_mmap_opaque *state = (zlib_mmap_opaque *)opaque;

	if ( (int)size <= 0 || state->left < (int)size )
	{
		return 0;
	}

	memcpy( buf, state->ptr, size );
	state->ptr += size;
	state->left -= size;

	return size;
}

static uLong ZCALLBACK mmap_fwrite_file_func( voidpf, voidpf, const void *, uLong )
{
	return 0;
}

static long ZCALLBACK mmap_ftell_file_func( voidpf opaque, voidpf )
{
	zlib_mmap_opaque *state = (zlib_mmap_opaque *)opaque;

	return state->len - state->left;
}

static long ZCALLBACK mmap_fseek_file_func( voidpf opaque, voidpf, uLong offset, int origin )
{
	zlib_mmap_opaque *state = (zlib_mmap_opaque *)opaque;

	switch ( origin ) {
		case SEEK_SET:
			if ( (int)offset < 0 || (int)offset > state->len )
			{
				return 0;
			}
			state->ptr = state->data + offset;
			state->left = state->len - offset;
			break;
		case SEEK_CUR:
			if ( (int)offset < 0 || (int)offset > state->left )
			{
				return 0;
			}
			state->ptr += offset;
			state->left -= offset;
			break;
		case SEEK_END:
			state->ptr = state->data + state->len;
			state->left = 0;
			break;
	}

	return 0;
}

static int ZCALLBACK mmap_fclose_file_func( voidpf, voidpf )
{
	return 0;
}

static int ZCALLBACK mmap_ferror_file_func( voidpf, voidpf )
{
	return 0;
}

static void mem_set_opaque( zlib_mmap_opaque & opaque, const unsigned char * data, int len )
{
	opaque.data = data;
	opaque.len = len;
	opaque.ptr = data;
	opaque.left = len;
}

static bool mmap_open_opaque( const char * fileName, zlib_mmap_opaque & opaque )
{
	// If unable to open the ZIP file,
	if ( !opaque.file.OpenRead( fileName, true, true ) )
	{
		WARN( "Couldn't open %s", fileName );
		return false;
	}

	int len = (int)opaque.file.GetLength();
	if ( len <= 0 )
	{
		WARN( "len = %i", len );
		return false;
	}
	if ( !opaque.view.Open( &opaque.file ) )
	{
		WARN( "View open failed" );
		return false;
	}
	if ( !opaque.view.MapView( 0, len ) )
	{
		WARN( "MapView failed" );
		return false;
	}

	opaque.data = opaque.view.GetFront();
	opaque.len = len;
	opaque.ptr = opaque.data;
	opaque.left = len;

	return true;
}

static unzFile open_opaque( zlib_mmap_opaque & zlib_opaque, const char * fileName )
{
	zlib_filefunc_def zlib_file_funcs =
	{
		mmap_fopen_file_func,
		mmap_fread_file_func,
		mmap_fwrite_file_func,
		mmap_ftell_file_func,
		mmap_fseek_file_func,
		mmap_fclose_file_func,
		mmap_ferror_file_func,
		&zlib_opaque
	};

	return unzOpen2( fileName, &zlib_file_funcs );
}

ModelFile * LoadModelFileFromMemory( const char * fileName,
		const void * buffer, int bufferLength,
		const ModelGlPrograms & programs,
		const MaterialParms & materialParms )
{
	// Open the .ModelFile file as a zip.
	LOG( "LoadModelFileFromMemory %s %i", fileName, bufferLength );

	zlib_mmap_opaque zlib_opaque;

	mem_set_opaque( zlib_opaque, (const unsigned char *)buffer, bufferLength );

	unzFile zfp = open_opaque( zlib_opaque, fileName );
	if ( !zfp )
	{
		return new ModelFile( fileName );
	}

	LOG( "LoadModelFileFromMemory zfp = %p", zfp );

	return LoadModelFile( zfp, fileName, (char *)buffer, bufferLength, programs, materialParms );
}

ModelFile * LoadModelFile( const char * fileName,
		const ModelGlPrograms & programs,
		const MaterialParms & materialParms )
{
	zlib_mmap_opaque zlib_opaque;

	LOG( "LoadModelFile %s", fileName );

	// Map and open the zip file
	if ( !mmap_open_opaque( fileName, zlib_opaque ) )
	{
		return new ModelFile( fileName );
	}

	unzFile zfp = open_opaque( zlib_opaque, fileName );
	if ( !zfp )
	{
		return new ModelFile( fileName );
	}

	return LoadModelFile( zfp, fileName, (char *)zlib_opaque.data, zlib_opaque.len, programs, materialParms );
}

#else	// !MEMORY_MAPPED

struct mzBuffer_t
{
	unsigned char * mzBufferBase;
	int				mzBufferLength;
	int				mzBufferPos;
};

static voidpf mz_open_file_func( voidpf opaque, const char* filename, int mode )
{
	mzBuffer_t * buffer = (mzBuffer_t *)opaque;
	buffer->mzBufferPos = 0;
	return (void *)1;
}

static uLong mz_read_file_func( voidpf opaque, voidpf stream, void* buf, uLong size )
{
	mzBuffer_t * buffer = (mzBuffer_t *)opaque;
	const int numCopyBytes = OVR::Alg::Min( buffer->mzBufferLength - buffer->mzBufferPos, (int)size );
	memcpy( buf, buffer->mzBufferBase + buffer->mzBufferPos, numCopyBytes );
	buffer->mzBufferPos += numCopyBytes;
	return numCopyBytes;
}

static uLong mz_write_file_func( voidpf opaque, voidpf stream, const void* buf, uLong size )
{
	return -1;
}

static int mz_close_file_func( voidpf opaque, voidpf stream )
{
	return 0;
}

static int mz_testerror_file_func( voidpf opaque, voidpf stream )
{
    int ret = -1;
    if ( stream != NULL )
    {
        ret = 0;
    }
    return ret;
}

static long mz_tell_file_func( voidpf opaque, voidpf stream )
{
	mzBuffer_t * buffer = (mzBuffer_t *)opaque;
	return buffer->mzBufferLength;
}

static long mz_seek_file_func( voidpf opaque, voidpf stream, uLong offset, int origin )
{
	mzBuffer_t * buffer = (mzBuffer_t *)opaque;
	switch ( origin )
	{
		case ZLIB_FILEFUNC_SEEK_CUR :
			buffer->mzBufferPos = OVR::Alg::Min( buffer->mzBufferLength, buffer->mzBufferPos + (int)offset );
			break;
		case ZLIB_FILEFUNC_SEEK_END :
			buffer->mzBufferPos = OVR::Alg::Min( buffer->mzBufferLength, buffer->mzBufferLength - (int)offset );
			break;
		case ZLIB_FILEFUNC_SEEK_SET :
			buffer->mzBufferPos = OVR::Alg::Min( buffer->mzBufferLength, (int)offset );
			break;
		default:
			return -1;
	}
	return 0;
}

ModelFile * LoadModelFileFromMemory( const char * fileName,
		const void * buffer, int bufferLength,
		const ModelGlPrograms & programs,
		const MaterialParms & materialParms )
{
	// Open the .ModelFile file as a zip.
	LOG( "LoadModelFileFromMemory %s %i", fileName, bufferLength );

	mzBuffer_t mzBuffer;
	mzBuffer.mzBufferBase = (unsigned char *)buffer;
	mzBuffer.mzBufferLength = bufferLength;
	mzBuffer.mzBufferPos = 0;

	zlib_filefunc_def filefunc;
	filefunc.zopen_file = mz_open_file_func;
	filefunc.zread_file = mz_read_file_func;
	filefunc.zwrite_file = mz_write_file_func;
	filefunc.ztell_file = mz_tell_file_func;
	filefunc.zseek_file = mz_seek_file_func;
	filefunc.zclose_file = mz_close_file_func;
	filefunc.zerror_file = mz_testerror_file_func;
	filefunc.opaque = &mzBuffer;

	unzFile zfp = unzOpen2( fileName, &filefunc );

	LOG( "OpenZipForMemory = %p", zfp );

	return LoadModelFile( zfp, fileName, NULL, 0, programs, materialParms );
}

ModelFile * LoadModelFile( const char * fileName,
		const ModelGlPrograms & programs, const MaterialParms & materialParms )
{
	// Open the .ModelFile file as a zip.
	unzFile zfp = unzOpen( fileName );
	return LoadModelFile( zfp, fileName, NULL, 0, programs, materialParms );
}

#endif	// MEMORY_MAPPED

} // namespace OVR
