// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VertexFactory.h"
#include "Components.h"

/*=============================================================================
	LocalVertexFactory.h: Local vertex factory definitions.
=============================================================================*/

/**
 * A vertex factory which simply transforms explicit vertex attributes from local to world space.
 */
class ENGINE_API FLocalVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLocalVertexFactory);
public:

	struct DataType : public FVertexFactory::DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TArray<FVertexStreamComponent,TFixedAllocator<MAX_STATIC_TEXCOORDS/2> > TextureCoordinates;

		/** The stream to read the shadow map texture coordinates from. */
		FVertexStreamComponent LightMapCoordinateComponent;

		/** The stream to read the vertex color from. */
		FVertexStreamComponent ColorComponent;
	};

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_SPEEDTREE_WIND"),TEXT("1"));
	}

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const DataType& InData);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FLocalVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;

	static bool SupportsTessellationShaders() { return true; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	DataType Data;
};

/**
 * Shader parameters for LocalVertexFactory.
 */
class FLocalVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override;
	// SpeedTree LOD parameter
	FShaderParameter LODParameter;
};
