// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "VertexFactory.h"
#include "UniformBuffer.h"

/**
 * Enum identifying the type of a particle vertex factory.
 */
enum EParticleVertexFactoryType
{
	PVFT_Sprite,
	PVFT_BeamTrail,
	PVFT_Mesh,
	PVFT_MAX
};

/**
 * Base class for particle vertex factories.
 */
class FParticleVertexFactoryBase : public FVertexFactory
{
public:

	/** Default constructor. */
	explicit FParticleVertexFactoryBase( EParticleVertexFactoryType Type, ERHIFeatureLevel::Type InFeatureLevel )
		: FVertexFactory(InFeatureLevel)
		, ParticleFactoryType(Type)
		, bInUse(false)
	{
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment) 
	{
		FVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PARTICLE_FACTORY"),TEXT("1"));
	}

	/** Return the vertex factory type */
	FORCEINLINE EParticleVertexFactoryType GetParticleFactoryType() const
	{
		return ParticleFactoryType;
	}

	inline void SetParticleFactoryType(EParticleVertexFactoryType InType)
	{
		ParticleFactoryType = InType;
	}

	/** Specify whether the factory is in use or not. */
	FORCEINLINE void SetInUse( bool bInInUse )
	{
		bInUse = bInInUse;
	}

	/** Return the vertex factory type */
	FORCEINLINE bool GetInUse() const
	{ 
		return bInUse;
	}

	ERHIFeatureLevel::Type GetFeatureLevel() const { check(HasValidFeatureLevel());  return FRenderResource::GetFeatureLevel(); }

private:

	/** The type of the vertex factory. */
	EParticleVertexFactoryType ParticleFactoryType;

	/** Whether the vertex factory is in use. */
	bool bInUse;
};

/**
 * Uniform buffer for particle sprite vertex factories.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FParticleSpriteUniformParameters, ENGINE_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, AxisLockRight, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, AxisLockUp, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, TangentSelector, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, NormalsSphereCenter, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, NormalsCylinderUnitDirection, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, SubImageSize, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, MacroUVParameters )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, RotationScale, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, RotationBias, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, NormalsType, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, InvDeltaSeconds, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector2D, PivotOffset, EShaderPrecisionModifier::Half )
END_UNIFORM_BUFFER_STRUCT( FParticleSpriteUniformParameters )
typedef TUniformBufferRef<FParticleSpriteUniformParameters> FParticleSpriteUniformBufferRef;

/**
 * Vertex factory for rendering particle sprites.
 */
class ENGINE_API FParticleSpriteVertexFactory : public FParticleVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticleSpriteVertexFactory);

public:

	/** Default constructor. */
	FParticleSpriteVertexFactory( EParticleVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel )
		: FParticleVertexFactoryBase(InType, InFeatureLevel),
		NumVertsInInstanceBuffer(0),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr)
	{}

	FParticleSpriteVertexFactory() 
		: FParticleVertexFactoryBase(PVFT_MAX, ERHIFeatureLevel::Num),
		NumVertsInInstanceBuffer(0),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr)
	{}

	// FRenderResource interface.
	virtual void InitRHI() override;

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * Set the source vertex buffer that contains particle instance data.
	 */
	void SetInstanceBuffer(const FVertexBuffer* InInstanceBuffer, uint32 StreamOffset, uint32 Stride, bool bInstanced);

	void SetTexCoordBuffer(const FVertexBuffer* InTexCoordBuffer);

	inline void SetNumVertsInInstanceBuffer(int32 InNumVertsInInstanceBuffer)
	{
		NumVertsInInstanceBuffer = InNumVertsInInstanceBuffer;
	}

	/**
	 * Set the source vertex buffer that contains particle dynamic parameter data.
	 */
	void SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride, bool bInstanced);

	/**
	 * Set the uniform buffer for this vertex factory.
	 */
	FORCEINLINE void SetSpriteUniformBuffer( const FParticleSpriteUniformBufferRef& InSpriteUniformBuffer )
	{
		SpriteUniformBuffer = InSpriteUniformBuffer;
	}

	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 */
	FORCEINLINE FUniformBufferRHIParamRef GetSpriteUniformBuffer()
	{
		return SpriteUniformBuffer;
	}

	void SetCutoutParameters(int32 InNumCutoutVerticesPerFrame, FShaderResourceViewRHIParamRef InCutoutGeometrySRV)
	{
		NumCutoutVerticesPerFrame = InNumCutoutVerticesPerFrame;
		CutoutGeometrySRV = InCutoutGeometrySRV;
	}

	inline int32 GetNumCutoutVerticesPerFrame() const { return NumCutoutVerticesPerFrame; }
	inline FShaderResourceViewRHIParamRef GetCutoutGeometrySRV() const { return CutoutGeometrySRV; }

	/**
	 * Construct shader parameters for this type of vertex factory.
	 */
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/** Initialize streams for this vertex factory. */
	void InitStreams();

private:

	int32 NumVertsInInstanceBuffer;

	/** Uniform buffer with sprite paramters. */
	FUniformBufferRHIParamRef SpriteUniformBuffer;

	int32 NumCutoutVerticesPerFrame;
	FShaderResourceViewRHIParamRef CutoutGeometrySRV;
};