// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshParticleVertexFactory.h: Mesh particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "UniformBuffer.h"
//@todo - parallelrendering - remove once FOneFrameResource no longer needs to be referenced in header
#include "SceneManagement.h"

/**
 * Uniform buffer for mesh particle vertex factories.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FMeshParticleUniformParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, SubImageSize )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, TexCoordWeightA )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, TexCoordWeightB )
END_UNIFORM_BUFFER_STRUCT( FMeshParticleUniformParameters )
typedef TUniformBufferRef<FMeshParticleUniformParameters> FMeshParticleUniformBufferRef;

/**
 * Vertex factory for rendering instanced mesh particles with out dynamic parameter support.
 */
class FMeshParticleVertexFactory : public FParticleVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FMeshParticleVertexFactory);
public:

	struct DataType : public FVertexFactory::DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TArray<FVertexStreamComponent,TFixedAllocator<MAX_TEXCOORDS> > TextureCoordinates;

		/** The stream to read the vertex  color from. */
		FVertexStreamComponent VertexColorComponent;

		/** The stream to read the vertex  color from. */
		FVertexStreamComponent ParticleColorComponent;

		/** The stream to read the mesh transform from. */
		FVertexStreamComponent TransformComponent[3];

		/** The stream to read the particle velocity from */
		FVertexStreamComponent VelocityComponent;

		/** The stream to read SubUV parameters from.. */
		FVertexStreamComponent SubUVs;

		/** The stream to read SubUV lerp and the particle relative time from */
		FVertexStreamComponent SubUVLerpAndRelTime;

		/** Flag to mark as initialized */
		bool bInitialized;

		DataType()
		: bInitialized(false)
		{

		}
	};

	class FBatchParametersCPU : public FOneFrameResource
	{
	public:
		const struct FMeshParticleInstanceVertex* InstanceBuffer;
		const struct FMeshParticleInstanceVertexDynamicParameter* DynamicParameterBuffer;
	};

	/** Default constructor. */
	FMeshParticleVertexFactory(EParticleVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel, int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
		: FParticleVertexFactoryBase(InType, InFeatureLevel)
		, DynamicVertexStride(InDynamicVertexStride)
		, DynamicParameterVertexStride(InDynamicParameterVertexStride)
	{}

	FMeshParticleVertexFactory()
		: FParticleVertexFactoryBase(PVFT_MAX, ERHIFeatureLevel::Num)
		, DynamicVertexStride(-1)
		, DynamicParameterVertexStride(-1)
	{}

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);


	/**
	 * Modify compile environment to enable instancing
	 * @param OutEnvironment - shader compile environment to modify
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FParticleVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		const bool bInstanced = IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES3_1);

		// Set a define so we can tell in MaterialTemplate.usf when we are compiling a mesh particle vertex factory
		OutEnvironment.SetDefine(TEXT("PARTICLE_MESH_FACTORY"),TEXT("1"));

		// TODO: Set based on instancing.
		OutEnvironment.SetDefine(TEXT("PARTICLE_MESH_INSTANCED"),(uint32)(bInstanced ? 1 : 0));
	}

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	ENGINE_API void SetData(const DataType& InData);

	/**
	 * Set the uniform buffer for this vertex factory.
	 */
	FORCEINLINE void SetUniformBuffer( const FMeshParticleUniformBufferRef& InMeshParticleUniformBuffer )
	{
		MeshParticleUniformBuffer = InMeshParticleUniformBuffer;
	}

	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 */
	FORCEINLINE FUniformBufferRHIParamRef GetUniformBuffer()
	{
		return MeshParticleUniformBuffer;
	}

	/**
	 * Update the data strides (MUST HAPPEN BEFORE InitRHI is called)
	 */
	void SetStrides(int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
	{
		DynamicVertexStride = InDynamicVertexStride;
		DynamicParameterVertexStride = InDynamicParameterVertexStride;
	}
	
	 /**
	 * Set the source vertex buffer that contains particle instance data.
	 */
	void SetInstanceBuffer(const FVertexBuffer* InstanceBuffer, uint32 StreamOffset, uint32 Stride);

	/**
	 * Set the source vertex buffer that contains particle dynamic parameter data.
	 */
	void SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FMeshParticleVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;

	static bool SupportsTessellationShaders() { return true; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	DataType Data;
	
	/** Stride information for instanced mesh particles */
	int32 DynamicVertexStride;
	int32 DynamicParameterVertexStride;
	

	/** Uniform buffer with mesh particle paramters. */
	FUniformBufferRHIParamRef MeshParticleUniformBuffer;
};
