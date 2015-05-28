// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.cpp: Particle vertex factory implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "ParticleDefinitions.h"
#include "ParticleResources.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleSpriteUniformParameters,TEXT("SpriteVF"));

TGlobalResource<FNullDynamicParameterVertexBuffer> GNullDynamicParameterVertexBuffer;

/**
 * Shader parameters for the particle vertex factory.
 */
class FParticleSpriteVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
	}

	virtual void Serialize(FArchive& Ar) override
	{
	}
};

class FParticleSpriteVertexFactoryShaderParametersVS : public FParticleSpriteVertexFactoryShaderParameters
{
public:

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FParticleSpriteVertexFactory* SpriteVF = (FParticleSpriteVertexFactory*)VertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShaderRHI, Shader->GetUniformBufferParameter<FParticleSpriteUniformParameters>(), SpriteVF->GetSpriteUniformBuffer() );
	}
};

class FParticleSpriteVertexFactoryShaderParametersPS : public FParticleSpriteVertexFactoryShaderParameters
{
public:

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FParticleSpriteVertexFactory* SpriteVF = (FParticleSpriteVertexFactory*)VertexFactory;
		FPixelShaderRHIParamRef PixelShaderRHI = Shader->GetPixelShader();
		SetUniformBufferParameter(RHICmdList, PixelShaderRHI, Shader->GetUniformBufferParameter<FParticleSpriteUniformParameters>(), SpriteVF->GetSpriteUniformBuffer() );
	}
};

/**
 * The particle system vertex declaration resource type.
 */
class FParticleSpriteVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Constructor.
	FParticleSpriteVertexDeclaration(bool bInInstanced)
		: bInstanced(bInInstanced)
	{

	}

	// Destructor.
	virtual ~FParticleSpriteVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, int32& Offset)
	{
		uint32 InitialStride = sizeof(float) * 2;
		uint32 PerParticleStride = (sizeof(float) * 4) * 4;

		/** The stream to read the texture coordinates from. */
		check( Offset == 0 );
		uint32 Stride = bInstanced ? InitialStride : InitialStride + PerParticleStride;
		Elements.Add(FVertexElement(0, Offset, VET_Float2, 4, Stride, false));
		Offset += sizeof(float) * 2;

		/** The per-particle streams follow. */
		if(bInstanced) 
		{
			Offset = 0;
			// update stride
			Stride = PerParticleStride;
		}

		/** The stream to read the vertex position from. */
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float4, 0, Stride, bInstanced));
		Offset += sizeof(float) * 4;
		/** The stream to read the vertex old position from. */
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float4, 1, Stride, bInstanced));
		Offset += sizeof(float) * 4;
		/** The stream to read the vertex size/rot/subimage from. */
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float4, 2, Stride, bInstanced));
		Offset += sizeof(float) * 4;
		/** The stream to read the color from.					*/
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float4, 3, Stride, bInstanced));
		Offset += sizeof(float) * 4;

		/** The per-particle dynamic parameter stream */

		// The -V519 disables a warning from PVS-Studio's static analyzer. It noticed that offset is assigned
		// twice before being read. It is probably safer to leave the redundant assignments here to reduce
		// the chance of an error being introduced if this code is modified.
		Offset = 0;  //-V519
		Stride = sizeof(float) * 4;
		Elements.Add(FVertexElement(bInstanced ? 2 : 1, Offset, VET_Float4, 5, Stride, bInstanced));
		Offset += sizeof(float) * 4;
	}

	virtual void InitDynamicRHI()
	{
		FVertexDeclarationElementList Elements;
		int32	Offset = 0;

		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		// This is done in InitDynamicRHI instead of InitRHI to allow FParticleSpriteVertexFactory::InitRHI
		// to rely on it being initialized, since InitDynamicRHI is called before InitRHI.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseDynamicRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}

private:

	bool bInstanced;
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleSpriteVertexDeclaration> GParticleSpriteVertexDeclarationInstanced(true);
static TGlobalResource<FParticleSpriteVertexDeclaration> GParticleSpriteVertexDeclarationNonInstanced(false);

inline TGlobalResource<FParticleSpriteVertexDeclaration>& GetParticleSpriteVertexDeclaration(bool SupportsInstancing)
{
	if (SupportsInstancing)
	{
		return GParticleSpriteVertexDeclarationInstanced;
	}
	else
	{
		return GParticleSpriteVertexDeclarationNonInstanced;
	}
}

bool FParticleSpriteVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return Material->IsUsedWithParticleSprites() || Material->IsSpecialEngineMaterial();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FParticleSpriteVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

	// Set a define so we can tell in MaterialTemplate.usf when we are compiling a sprite vertex factory
	OutEnvironment.SetDefine(TEXT("PARTICLE_SPRITE_FACTORY"),TEXT("1"));
}

/**
 *	Initialize the Render Hardware Interface for this vertex factory
 */
void FParticleSpriteVertexFactory::InitRHI()
{
	InitStreams();
	SetDeclaration(GetParticleSpriteVertexDeclaration(RHISupportsInstancing(GetFeatureLevelShaderPlatform(GetFeatureLevel()))).VertexDeclarationRHI);
}

void FParticleSpriteVertexFactory::InitStreams()
{
    const bool bInstanced = RHISupportsInstancing(GetFeatureLevelShaderPlatform(GetFeatureLevel()));

	check(Streams.Num() == 0);
	if(bInstanced) 
	{
		FVertexStream* TexCoordStream = new(Streams) FVertexStream;
		TexCoordStream->VertexBuffer = &GParticleTexCoordVertexBuffer;
		TexCoordStream->Stride = sizeof(FVector2D);
		TexCoordStream->Offset = 0;
	}
	FVertexStream* InstanceStream = new(Streams) FVertexStream;
	InstanceStream->VertexBuffer = 0;
	InstanceStream->Stride = 0;
	InstanceStream->Offset = 0;
	FVertexStream* DynamicParameterStream = new(Streams) FVertexStream;
	DynamicParameterStream->VertexBuffer = 0;
	DynamicParameterStream->Stride = 0;
	DynamicParameterStream->Offset = 0;
}

void FParticleSpriteVertexFactory::SetInstanceBuffer(const FVertexBuffer* InInstanceBuffer, uint32 StreamOffset, uint32 Stride, bool bInstanced)
{
	check(Streams.Num() == (bInstanced ? 3 : 2));
	FVertexStream& InstanceStream = Streams[bInstanced ? 1 : 0];
	InstanceStream.VertexBuffer = InInstanceBuffer;
	InstanceStream.Stride = Stride;
	InstanceStream.Offset = StreamOffset;
}

void FParticleSpriteVertexFactory::SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride, bool bInstanced)
{
	check(Streams.Num() == (bInstanced ? 3 : 2));
	FVertexStream& DynamicParameterStream = Streams[bInstanced ? 2 : 1];
	if (InDynamicParameterBuffer)
	{
		DynamicParameterStream.VertexBuffer = InDynamicParameterBuffer;
		DynamicParameterStream.Stride = Stride;
		DynamicParameterStream.Offset = StreamOffset;
	}
	else
	{
		DynamicParameterStream.VertexBuffer = &GNullDynamicParameterVertexBuffer;
		DynamicParameterStream.Stride = 0;
		DynamicParameterStream.Offset = 0;
	}
}

FVertexFactoryShaderParameters* FParticleSpriteVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	if (ShaderFrequency == SF_Vertex)
	{
		return new FParticleSpriteVertexFactoryShaderParametersVS();
	}
	else if (ShaderFrequency == SF_Pixel)
	{
		return new FParticleSpriteVertexFactoryShaderParametersPS();
	}
	return NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleSpriteVertexFactory,"ParticleSpriteVertexFactory",true,false,true,false,false);