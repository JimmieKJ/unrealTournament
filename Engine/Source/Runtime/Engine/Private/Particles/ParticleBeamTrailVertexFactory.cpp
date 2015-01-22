// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleBeamTrailVertexFactory.cpp: Particle vertex factory implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "ParticleDefinitions.h"
#include "ShaderParameterUtils.h"
#include "ParticleBeamTrailVertexFactory.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleBeamTrailUniformParameters,TEXT("BeamTrailVF"));

/**
 * Shader parameters for the beam/trail vertex factory.
 */
class FParticleBeamTrailVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
	}

	virtual void Serialize(FArchive& Ar) override
	{
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FParticleBeamTrailVertexFactory* BeamTrailVF = (FParticleBeamTrailVertexFactory*)VertexFactory;
		SetUniformBufferParameter(RHICmdList, Shader->GetVertexShader(), Shader->GetUniformBufferParameter<FParticleBeamTrailUniformParameters>(), BeamTrailVF->GetBeamTrailUniformBuffer() );
	}
};

///////////////////////////////////////////////////////////////////////////////
/**
 * The particle system beam trail vertex declaration resource type.
 */
class FParticleBeamTrailVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FParticleBeamTrailVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, int32& Offset)
	{
		uint16 Stride = sizeof(FParticleBeamTrailVertex);
		/** The stream to read the vertex position from. */
		Elements.Add(FVertexElement(0, Offset, VET_Float4, 0, Stride));
		Offset += sizeof(float) * 4;
		/** The stream to read the vertex old position from. */
		Elements.Add(FVertexElement(0, Offset, VET_Float3, 1, Stride));
		Offset += sizeof(float) * 4;
		/** The stream to read the vertex size/rot/subimage from. */
		Elements.Add(FVertexElement(0, Offset, VET_Float4, 2, Stride));
		Offset += sizeof(float) * 4;
		/** The stream to read the color from.					*/
		Elements.Add(FVertexElement(0, Offset, VET_Float4, 4, Stride));
		Offset += sizeof(float) * 4;
		/** The stream to read the texture coordinates from.	*/
		Elements.Add(FVertexElement(0, Offset, VET_Float4, 3, Stride));
		Offset += sizeof(float) * 4;
		
		/** Dynamic parameters come from a second stream */
		Elements.Add(FVertexElement(1, 0, VET_Float4, 5, sizeof(FVector4)));
	}

	virtual void InitDynamicRHI()
	{
		FVertexDeclarationElementList Elements;
		int32	Offset = 0;
		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		// This is done in InitDynamicRHI instead of InitRHI to allow FParticleBeamTrailVertexFactory::InitRHI
		// to rely on it being initialized, since InitDynamicRHI is called before InitRHI.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseDynamicRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleBeamTrailVertexDeclaration> GParticleBeamTrailVertexDeclaration;

///////////////////////////////////////////////////////////////////////////////

bool FParticleBeamTrailVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return Material->IsUsedWithBeamTrails() || Material->IsSpecialEngineMaterial();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FParticleBeamTrailVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("PARTICLE_BEAMTRAIL_FACTORY"),TEXT("1"));
}

/**
 *	Initialize the Render Hardware Interface for this vertex factory
 */
void FParticleBeamTrailVertexFactory::InitRHI()
{
	SetDeclaration(GParticleBeamTrailVertexDeclaration.VertexDeclarationRHI);

	FVertexStream* VertexStream = new(Streams) FVertexStream;
	VertexStream->VertexBuffer = 0;
	VertexStream->Stride = 0;
	VertexStream->Offset = 0;
	FVertexStream* DynamicParameterStream = new(Streams) FVertexStream;
	DynamicParameterStream->VertexBuffer = 0;
	DynamicParameterStream->Stride = 0;
	DynamicParameterStream->Offset = 0;
}

FVertexFactoryShaderParameters* FParticleBeamTrailVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FParticleBeamTrailVertexFactoryShaderParameters() : NULL;
}

void FParticleBeamTrailVertexFactory::SetVertexBuffer(const FVertexBuffer* InBuffer, uint32 StreamOffset, uint32 Stride)
{
	check(Streams.Num() == 2);
	FVertexStream& VertexStream = Streams[0];
	VertexStream.VertexBuffer = InBuffer;
	VertexStream.Stride = Stride;
	VertexStream.Offset = StreamOffset;
}

void FParticleBeamTrailVertexFactory::SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride)
{
	check(Streams.Num() == 2);
	FVertexStream& DynamicParameterStream = Streams[1];
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

///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleBeamTrailVertexFactory,"ParticleBeamTrailVertexFactory",true,false,true,false,false);
