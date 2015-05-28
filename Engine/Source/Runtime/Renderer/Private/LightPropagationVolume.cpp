//-----------------------------------------------------------------------------
// File:		LightPropagationVolume.cpp
//
// Summary:		Light Propagation Volumes implementation 
//
// Created:		2013-03-01
//
// Author:		mailto:benwood@microsoft.com
//
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "LightPropagationVolume.h"
#include "UniformBuffer.h"
#include "SceneUtils.h"

static TAutoConsoleVariable<int32> CVarLightPropagationVolume(
	TEXT("r.LightPropagationVolume"),
	0,
	TEXT("Project setting of the work in progress feature LightPropgationVolume. Cannot be changed at runtime.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

// ----------------------------------------------------------------------------

static const uint32 LPV_GRIDRES = 32;
static float LPV_CENTRE_OFFSET = 10.0f;

#define LPV_AMBIENT_CUBE 0

static int GLpvEnabled = 0;

// ----------------------------------------------------------------------------

struct LPVBufferElement
{
#if LPV_AMBIENT_CUBE
	uint32 values[6];   // 12 halfs (24 bytes)
#else
	uint32 values[14];   // 28 halfs (56 bytes)
#endif
};

struct VplListEntry
{
	uint32	normalPacked;
	uint32	fluxPacked;
	int		nextIndex;
};

struct FRsmInfo
{
	FVector4 ShadowmapMinMax;
	FMatrix WorldToShadow;
	FMatrix ShadowToWorld;
	float AreaBrightnessMultiplier; 
};

// ----------------------------------------------------------------------------

/**
 * Uniform buffer parameters for LPV direct injection shaders
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FLpvDirectLightInjectParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, LightRadius )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, LightPosition )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, LightColor )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, LightFalloffExponent )
END_UNIFORM_BUFFER_STRUCT( FLpvDirectLightInjectParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLpvDirectLightInjectParameters,TEXT("LpvInject"));

typedef TUniformBufferRef<FLpvDirectLightInjectParameters> FDirectLightInjectBufferRef;

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLpvWriteUniformBufferParameters,TEXT("LpvWrite"));


// ----------------------------------------------------------------------------
// Base LPV Write Compute shader
// ----------------------------------------------------------------------------
class FLpvWriteShaderCSBase : public FGlobalShader
{
public:
	// Default constructor
	FLpvWriteShaderCSBase()	{	}

	/** Initialization constructor. */
	explicit FLpvWriteShaderCSBase( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
#if LPV_VOLUME_TEXTURE
		for ( int i = 0; i < 7; i++ )
		{
			LpvBufferSRVParameters[i].Bind(Initializer.ParameterMap, LpvVolumeTextureSRVNames[i] );
			LpvBufferUAVs[i].Bind(Initializer.ParameterMap, LpvVolumeTextureUAVNames[i] );
		}
#else
		LpvBufferSRV.Bind(Initializer.ParameterMap, TEXT("gLpvBuffer") );
		LpvBufferUAV.Bind(Initializer.ParameterMap, TEXT("gLpvBufferRW") );
#endif

#if LPV_VOLUME_TEXTURE || LPV_GV_VOLUME_TEXTURE
		LpvVolumeTextureSampler.Bind( Initializer.ParameterMap, TEXT("gLpv3DTextureSampler") );
#endif

		VplListHeadBufferSRV.Bind(Initializer.ParameterMap, TEXT("gVplListHeadBuffer") );
		VplListHeadBufferUAV.Bind(Initializer.ParameterMap, TEXT("RWVplListHeadBuffer") );
		VplListBufferSRV.Bind(Initializer.ParameterMap, TEXT("gVplListBuffer") );
		VplListBufferUAV.Bind(Initializer.ParameterMap, TEXT("RWVplListBuffer") );

#if LPV_GV_VOLUME_TEXTURE
		for ( int i = 0; i < 3; i++ )
		{
			GvBufferSRVParameters[i].Bind(Initializer.ParameterMap, LpvGvVolumeTextureSRVNames[i] );
			GvBufferUAVs[i].Bind(Initializer.ParameterMap, LpvGvVolumeTextureUAVNames[i] );
		}
#else
		GvBufferSRV.Bind(Initializer.ParameterMap, TEXT("gGvBuffer") );
		GvBufferUAV.Bind(Initializer.ParameterMap, TEXT("gGvBufferRW") );
#endif
		GvListBufferUAV.Bind(Initializer.ParameterMap, TEXT("RWGvListBuffer") );
		GvListHeadBufferUAV.Bind(Initializer.ParameterMap, TEXT("RWGvListHeadBuffer") );

		GvListBufferSRV.Bind(Initializer.ParameterMap, TEXT("gGvListBuffer") );
		GvListHeadBufferSRV.Bind(Initializer.ParameterMap, TEXT("gGvListHeadBuffer") );
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("LPV_AMBIENT_CUBE"),			(uint32)LPV_AMBIENT_CUBE );
		OutEnvironment.SetDefine( TEXT("LPV_MULTIPLE_BOUNCES"), (uint32)LPV_MULTIPLE_BOUNCES );
		OutEnvironment.SetDefine( TEXT("LPV_GV_VOLUME_TEXTURE"),	(uint32)LPV_GV_VOLUME_TEXTURE );
	}
	// Serialization
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );

#if LPV_VOLUME_TEXTURE
		for(int i = 0; i < 7; i++)
		{
			Ar << LpvBufferSRVParameters[i];
			Ar << LpvBufferUAVs[i];
		}
#else
		Ar << LpvBufferSRV;
		Ar << LpvBufferUAV;
#endif

#if LPV_VOLUME_TEXTURE || LPV_GV_VOLUME_TEXTURE
		Ar << LpvVolumeTextureSampler;
#endif

		Ar << VplListHeadBufferSRV;
		Ar << VplListHeadBufferUAV;
		Ar << VplListBufferSRV;
		Ar << VplListBufferUAV;
#if LPV_GV_VOLUME_TEXTURE
		for(int i = 0; i < 3; i++)
		{
			Ar << GvBufferSRVParameters[i];
			Ar << GvBufferUAVs[i];
		}
#else
		Ar << GvBufferSRV;
		Ar << GvBufferUAV;
#endif
		Ar << GvListBufferUAV;
		Ar << GvListHeadBufferUAV;

		Ar << GvListBufferSRV;
		Ar << GvListHeadBufferSRV;

		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FLpvBaseWriteShaderParams& Params )
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FLpvWriteUniformBufferParameters>(), Params.UniformBuffer );
#if LPV_VOLUME_TEXTURE
		for(int i  =0; i < 7; i++)
		{
			if ( LpvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, LpvBufferSRVParameters[i].GetBaseIndex(), Params.LpvBufferSRVs[i]);
			}
			if ( LpvBufferUAVs[i].IsBound() )
			{
				RHICmdList.SetUAVParameter( ShaderRHI, LpvBufferUAVs[i].GetBaseIndex(), Params.LpvBufferUAVs[i] );
			}

			SetTextureParameter(RHICmdList, ShaderRHI, LpvBufferSRVParameters[i], LpvVolumeTextureSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), Params.LpvBufferSRVs[i] );
		}
#else
		if ( LpvBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, LpvBufferSRV.GetBaseIndex(), Params.LpvBufferSRV );
		}
		if ( LpvBufferUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter( ShaderRHI, LpvBufferUAV.GetBaseIndex(), Params.LpvBufferUAV );
		}
#endif
		if ( VplListHeadBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, VplListHeadBufferSRV.GetBaseIndex(), Params.VplListHeadBufferSRV );
		}
		if ( VplListHeadBufferUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter( ShaderRHI, VplListHeadBufferUAV.GetBaseIndex(), Params.VplListHeadBufferUAV );
		}
		if ( VplListBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, VplListBufferSRV.GetBaseIndex(), Params.VplListBufferSRV );
		}
		if ( VplListBufferUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter( ShaderRHI, VplListBufferUAV.GetBaseIndex(), Params.VplListBufferUAV ); 
		}
#if LPV_GV_VOLUME_TEXTURE
		for ( int i=0;i<3; i++ )
		{
			if ( GvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, GvBufferSRVParameters[i].GetBaseIndex(), Params.GvBufferSRVs[i]);
			}
			if ( GvBufferUAVs[i].IsBound() )
			{
				RHICmdList.SetUAVParameter( ShaderRHI, GvBufferUAVs[i].GetBaseIndex(), Params.GvBufferUAVs[i] );
			}

			SetTextureParameter(RHICmdList, ShaderRHI, GvBufferSRVParameters[i], LpvVolumeTextureSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), Params.GvBufferSRVs[i] );
		}
#else
		if ( GvBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, GvBufferSRV.GetBaseIndex(), Params.GvBufferSRV );
		}
		if ( GvBufferUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter( ShaderRHI, GvBufferUAV.GetBaseIndex(), Params.GvBufferUAV );
		}
#endif
		if(GvListBufferUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ShaderRHI, GvListBufferUAV.GetBaseIndex(), Params.GvListBufferUAV); 
		}
		if(GvListHeadBufferUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ShaderRHI, GvListHeadBufferUAV.GetBaseIndex(), Params.GvListHeadBufferUAV); 
		}
		if(GvListBufferSRV.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ShaderRHI, GvListBufferSRV.GetBaseIndex(), Params.GvListBufferSRV);
		}
		if(GvListHeadBufferSRV.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(ShaderRHI, GvListHeadBufferSRV.GetBaseIndex(), Params.GvListHeadBufferSRV);
		}
	}

	// Unbinds any buffers that have been bound.
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
#if LPV_VOLUME_TEXTURE
		for ( int i = 0; i < 7; i++ )
		{
			if ( LpvBufferSRVParameters[i].IsBound() )
		{
				RHICmdList.SetShaderTexture(ShaderRHI, LpvBufferSRVParameters[i].GetBaseIndex(), FTextureRHIParamRef());
		}
			if ( LpvBufferUAVs[i].IsBound() )
		{
				RHICmdList.SetUAVParameter( ShaderRHI, LpvBufferUAVs[i].GetBaseIndex(), FUnorderedAccessViewRHIParamRef() );
		}
	}
#else
		if ( LpvBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, LpvBufferSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}
		if ( LpvBufferUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter( ShaderRHI, LpvBufferUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef() );
		}
#endif
		if ( VplListHeadBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, VplListHeadBufferSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}
		if ( VplListHeadBufferUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter( ShaderRHI, VplListHeadBufferUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef() );
		}
		if ( VplListBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, VplListBufferSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}
		if ( VplListBufferUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter( ShaderRHI, VplListBufferUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef() );
		}
#if LPV_GV_VOLUME_TEXTURE
		for ( int i = 0; i < 3; i++ )
		{
			if ( GvBufferSRVParameters[i].IsBound() )
		{
				RHICmdList.SetShaderTexture(ShaderRHI, GvBufferSRVParameters[i].GetBaseIndex(), FTextureRHIParamRef());
		}
			if ( GvBufferUAVs[i].IsBound() )
		{
				RHICmdList.SetUAVParameter( ShaderRHI, GvBufferUAVs[i].GetBaseIndex(), FUnorderedAccessViewRHIParamRef() );
		}
		}
#else
		if ( GvBufferSRV.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter( ShaderRHI, GvBufferSRV.GetBaseIndex(), FShaderResourceViewRHIParamRef() );
		}
		if ( GvBufferUAV.IsBound() )
		{
			RHICmdList.SetUAVParameter( ShaderRHI, GvBufferUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef() );
		}
#endif
		if(GvListBufferUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ShaderRHI, GvListBufferUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
		if(GvListHeadBufferUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ShaderRHI, GvListHeadBufferUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

protected:
#if LPV_VOLUME_TEXTURE
	FShaderResourceParameter LpvBufferSRVParameters[7];
	FShaderResourceParameter LpvBufferUAVs[7];
#else
	FShaderResourceParameter LpvBufferSRV;
	FShaderResourceParameter LpvBufferUAV;
#endif

#if LPV_VOLUME_TEXTURE || LPV_GV_VOLUME_TEXTURE
	FShaderResourceParameter LpvVolumeTextureSampler;
#endif

	FShaderResourceParameter VplListHeadBufferSRV;
	FShaderResourceParameter VplListHeadBufferUAV;
	FShaderResourceParameter VplListBufferSRV;
	FShaderResourceParameter VplListBufferUAV;
#if LPV_GV_VOLUME_TEXTURE
	FShaderResourceParameter GvBufferSRVParameters[3];
	FShaderResourceParameter GvBufferUAVs[3];
#else
	FShaderResourceParameter GvBufferSRV;
	FShaderResourceParameter GvBufferUAV;
#endif

	FShaderResourceParameter GvListBufferUAV;
	FShaderResourceParameter GvListHeadBufferUAV;

	FShaderResourceParameter GvListBufferSRV;
	FShaderResourceParameter GvListHeadBufferSRV;
};


// ----------------------------------------------------------------------------
// LPV clear Compute shader
// ----------------------------------------------------------------------------
class FLpvClearCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvClearCS,Global);

public:
	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvClearCS()	{	}

	explicit FLpvClearCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvClearCS,TEXT("LPVClear"),TEXT("CSClear"),SF_Compute);


// ----------------------------------------------------------------------------
// LPV clear geometryVolume Compute shader
// ----------------------------------------------------------------------------
class FLpvClearGeometryVolumeCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvClearGeometryVolumeCS,Global);

public:
	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvClearGeometryVolumeCS()	{	}

	explicit FLpvClearGeometryVolumeCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvClearGeometryVolumeCS,TEXT("LPVClear"),TEXT("CSClearGeometryVolume"),SF_Compute);


// ----------------------------------------------------------------------------
// LPV clear lists Compute shader
// ----------------------------------------------------------------------------
class FLpvClearListsCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvClearListsCS,Global);

public:
	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvClearListsCS()	{	}

	explicit FLpvClearListsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvClearListsCS,TEXT("LPVClearLists"),TEXT("CSClearLists"),SF_Compute);

// ----------------------------------------------------------------------------
// LPV generate VPL lists compute shader (for a directional light)
// ----------------------------------------------------------------------------
class FLpvInject_GenerateVplListsCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvInject_GenerateVplListsCS,Global);

public:
	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvInject_GenerateVplListsCS()	{	}

	explicit FLpvInject_GenerateVplListsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		
	{	
		RsmDiffuseTexture.Bind(		Initializer.ParameterMap, TEXT("gRsmFluxTex") );
		RsmNormalTexture.Bind(		Initializer.ParameterMap, TEXT("gRsmNormalTex") );
		RsmDepthTexture.Bind(		Initializer.ParameterMap, TEXT("gRsmDepthTex") );

		LinearTextureSampler.Bind(	Initializer.ParameterMap, TEXT("LinearSampler") );
		PointTextureSampler.Bind(	Initializer.ParameterMap, TEXT("PointSampler") );
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		FLpvBaseWriteShaderParams& BaseParams,
		FTextureRHIParamRef RsmDiffuseTextureRHI,
		FTextureRHIParamRef RsmNormalTextureRHI,
		FTextureRHIParamRef RsmDepthTextureRHI )
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FLpvWriteShaderCSBase::SetParameters(RHICmdList, BaseParams );

		FSamplerStateRHIParamRef SamplerStateLinear  = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		FSamplerStateRHIParamRef SamplerStatePoint   = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

		// FIXME: Why do we have to bind a samplerstate to a sampler here? Presumably this is for legacy reasons... 
		SetTextureParameter(RHICmdList, ShaderRHI, RsmDiffuseTexture, LinearTextureSampler, SamplerStateLinear, RsmDiffuseTextureRHI );
		SetTextureParameter(RHICmdList, ShaderRHI, RsmNormalTexture, LinearTextureSampler, SamplerStateLinear, RsmNormalTextureRHI );
		SetTextureParameter(RHICmdList, ShaderRHI, RsmDepthTexture, PointTextureSampler, SamplerStatePoint, RsmDepthTextureRHI );
	}

	// Unbinds any buffers that have been bound.
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FLpvWriteShaderCSBase::UnbindBuffers(RHICmdList);
	}


	virtual bool Serialize( FArchive& Ar ) override			
	{ 
		bool rv = FLpvWriteShaderCSBase::Serialize( Ar ); 
		Ar << RsmDiffuseTexture;
		Ar << RsmNormalTexture;
		Ar << RsmDepthTexture;
		Ar << LinearTextureSampler;
		Ar << PointTextureSampler;
		return rv;
	}
protected:
	FShaderResourceParameter RsmDiffuseTexture;
	FShaderResourceParameter RsmNormalTexture;
	FShaderResourceParameter RsmDepthTexture;

	FShaderResourceParameter LinearTextureSampler;
	FShaderResourceParameter PointTextureSampler;
};
IMPLEMENT_SHADER_TYPE(,FLpvInject_GenerateVplListsCS,TEXT("LPVInject_GenerateVplLists"),TEXT("CSGenerateVplLists_LightDirectional"),SF_Compute);


// ----------------------------------------------------------------------------
// LPV accumulate VPL lists compute shader
// ----------------------------------------------------------------------------
class FLpvInject_AccumulateVplListsCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvInject_AccumulateVplListsCS,Global);

public:
	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvInject_AccumulateVplListsCS()	{	}

	explicit FLpvInject_AccumulateVplListsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvInject_AccumulateVplListsCS,TEXT("LPVInject_AccumulateVplLists"),TEXT("CSAccumulateVplLists"),SF_Compute);


// ----------------------------------------------------------------------------
// Compute shader to build a geometry volume
// ----------------------------------------------------------------------------
class FLpvBuildGeometryVolumeCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(FLpvBuildGeometryVolumeCS,Global);

public:
	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	FLpvBuildGeometryVolumeCS()	{	}

	explicit FLpvBuildGeometryVolumeCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(,FLpvBuildGeometryVolumeCS,TEXT("LPVBuildGeometryVolume"),TEXT("CSBuildGeometryVolume"),SF_Compute);



// ----------------------------------------------------------------------------
// LPV propagate compute shader
// ----------------------------------------------------------------------------
enum PropagateShaderFlags
{
	PROPAGATE_SECONDARY_OCCLUSION	= 0x01,
	PROPAGATE_MULTIPLE_BOUNCES		= 0x02,

	PROPAGATE_SECONDARY_OCCLUSION_AND_MULTIPLE_BOUNCES		= PROPAGATE_SECONDARY_OCCLUSION | PROPAGATE_MULTIPLE_BOUNCES,
};

template<uint32 ShaderFlags>
class TLpvPropagateCS : public FLpvWriteShaderCSBase
{
	DECLARE_SHADER_TYPE(TLpvPropagateCS,Global);

public:
	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		OutEnvironment.SetDefine(TEXT("LPV_SECONDARY_OCCLUSION"), (uint32)(ShaderFlags & PROPAGATE_SECONDARY_OCCLUSION ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("LPV_MULTIPLE_BOUNCES_ENABLED"), (uint32)(ShaderFlags & PROPAGATE_MULTIPLE_BOUNCES ? 1 : 0));
			
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	TLpvPropagateCS()	{	}

	explicit TLpvPropagateCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};

IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<0>,													TEXT("LPVPropagate"),TEXT("CSPropogate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_SECONDARY_OCCLUSION>,						TEXT("LPVPropagate"),TEXT("CSPropogate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_MULTIPLE_BOUNCES>,							TEXT("LPVPropagate"),TEXT("CSPropogate"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvPropagateCS<PROPAGATE_SECONDARY_OCCLUSION_AND_MULTIPLE_BOUNCES>,	TEXT("LPVPropagate"),TEXT("CSPropogate"),SF_Compute);

FLpvWriteShaderCSBase* GetPropagateShader(FViewInfo& View, bool bSecondaryOcclusion, bool bSecondaryBounces )
{
	FLpvWriteShaderCSBase* ShaderOut = NULL;
	if ( bSecondaryBounces )
	{
		if ( bSecondaryOcclusion ) 
			ShaderOut = (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_SECONDARY_OCCLUSION_AND_MULTIPLE_BOUNCES> >(View.ShaderMap);
		else 
			ShaderOut = (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_MULTIPLE_BOUNCES> >(View.ShaderMap);
	}
	else
	{
		if ( bSecondaryOcclusion ) 
			ShaderOut = (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<PROPAGATE_SECONDARY_OCCLUSION> >(View.ShaderMap);
		else 
			ShaderOut = (FLpvWriteShaderCSBase*)*TShaderMapRef<TLpvPropagateCS<0> >(View.ShaderMap);
	}
	return ShaderOut;
}

// ----------------------------------------------------------------------------
// Base injection compute shader
// ----------------------------------------------------------------------------
enum EInjectFlags
{
	INJECT_SHADOWED = 0x01,
};

class FLpvInjectShader_Base : public FLpvWriteShaderCSBase
{
public:
	void SetParameters(
		FRHICommandList& RHICmdList, 
		FLpvBaseWriteShaderParams& BaseParams,
		FDirectLightInjectBufferRef& InjectUniformBuffer )
	{
		FLpvWriteShaderCSBase::SetParameters(RHICmdList, BaseParams );
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FLpvDirectLightInjectParameters>(), InjectUniformBuffer );
	}

	FLpvInjectShader_Base()	{	}

	explicit FLpvInjectShader_Base( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvWriteShaderCSBase(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvWriteShaderCSBase::Serialize( Ar ); }
};

// ----------------------------------------------------------------------------
// Point light injection compute shader
// ----------------------------------------------------------------------------
template <uint32 InjectFlags>
class TLpvInject_PointLightCS : public FLpvInjectShader_Base
{
	DECLARE_SHADER_TYPE(TLpvInject_PointLightCS,Global);

public:
	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		OutEnvironment.SetDefine(TEXT("SHADOW_CASTING"), (uint32)(InjectFlags & INJECT_SHADOWED ? 1 : 0));
		FLpvWriteShaderCSBase::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	TLpvInject_PointLightCS()	{	}

	explicit TLpvInject_PointLightCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvInjectShader_Base(Initializer)		{	}

	virtual bool Serialize( FArchive& Ar ) override			{ return FLpvInjectShader_Base::Serialize( Ar ); }
};
IMPLEMENT_SHADER_TYPE(template<>,TLpvInject_PointLightCS<0>,TEXT("LPVDirectLightInject"),TEXT("CSPointLightInject_ListGenCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLpvInject_PointLightCS<INJECT_SHADOWED>,TEXT("LPVDirectLightInject"),TEXT("CSPointLightInject_ListGenCS"),SF_Compute);

// ----------------------------------------------------------------------------
// FLightPropagationVolume
// ----------------------------------------------------------------------------
FLightPropagationVolume::FLightPropagationVolume() :
	  mGridOffset( 0, 0, 0 )
	, mOldGridOffset( 0, 0, 0 )
	, mInjectedLightCount( 0 )
	, SecondaryOcclusionStrength( 0.0f )
	, SecondaryBounceStrength( 0.0f )
	, CubeSize( 5312.0f )
	, bEnabled( false )
	, mWriteBufferIndex( 0 )
	, GeometryVolumeGenerated( false )
{
	bNeedsBufferClear = true;

#if !LPV_VOLUME_TEXTURE
	for ( int i=0; i<2; i++ )
	{
		mLpvBuffers[i] = new FRWBufferStructured();
		mLpvBuffers[i]->Initialize( sizeof(LPVBufferElement), LPV_GRIDRES*LPV_GRIDRES*LPV_GRIDRES, BUF_FastVRAM, false, false );
	}
#endif
	// VPL List buffers
	mVplListBuffer = new FRWBufferStructured();
	mVplListBuffer->Initialize(sizeof(VplListEntry), 256 * 256 * LPV_GRIDRES, 0, true, false);
	mVplListHeadBuffer = new FRWBufferByteAddress();
	mVplListHeadBuffer->Initialize(LPV_GRIDRES*LPV_GRIDRES*LPV_GRIDRES * 4, BUF_ByteAddressBuffer);

	// Geometry volume buffers
#if !LPV_GV_VOLUME_TEXTURE
	GvBuffer = new FRWBuffer();

#if LPV_MULTIPLE_BOUNCES
	GvBuffer->Initialize( 4, LPV_GRIDRES*LPV_GRIDRES*LPV_GRIDRES*4, PF_A2B10G10R10, BUF_FastVRAM );
#else
	GvBuffer->Initialize( 4, LPV_GRIDRES*LPV_GRIDRES*LPV_GRIDRES*3, PF_A2B10G10R10, 0 );
#endif

#endif // !LPV_GV_VOLUME_TEXTURE

	GvListBuffer = new FRWBufferStructured();
	GvListBuffer->Initialize(sizeof(VplListEntry), 256 * 256 * LPV_GRIDRES, 0, true, false);
	GvListHeadBuffer = new FRWBufferByteAddress();
	GvListHeadBuffer->Initialize(LPV_GRIDRES*LPV_GRIDRES*LPV_GRIDRES * 4, BUF_ByteAddressBuffer);

	LpvWriteUniformBufferParams = new FLpvWriteUniformBufferParameters;
	FMemory::Memzero( LpvWriteUniformBufferParams, sizeof(FLpvWriteUniformBufferParameters) );

	bInitialized = false;
}


/**
* Destructor
*/
FLightPropagationVolume::~FLightPropagationVolume()
{
	LpvWriteUniformBuffer.ReleaseResource();

#if LPV_VOLUME_TEXTURE
	// Note: this is double-buffered!
	for ( int i = 0; i < 2; i++ )
	{
		for ( int j = 0; j < 7; j++ )
		{
			LpvVolumeTextures[i][j].SafeRelease();
		}
	}
#else
	for ( int i = 0; i < 2; i++ )
	{
		mLpvBuffers[i]->Release();
		delete mLpvBuffers[i];
	}
#endif 

	mVplListHeadBuffer->Release();
	delete mVplListHeadBuffer;

	mVplListBuffer->Release();
	delete mVplListBuffer;

#if LPV_GV_VOLUME_TEXTURE
	for ( int i = 0; i < 3; i++ )
	{
		GvVolumeTextures[i].SafeRelease();
	}
#else
	GvBuffer->Release();
	delete GvBuffer;
#endif

	GvListHeadBuffer->Release();
	delete GvListHeadBuffer;

	GvListBuffer->Release();
	delete GvListBuffer;

	delete LpvWriteUniformBufferParams;
}  

/**
* Sets up the LPV at the beginning of the frame
*/
void FLightPropagationVolume::InitSettings(FRHICommandList& RHICmdList, const FSceneView& View)
	{
	check(View.GetFeatureLevel() >= ERHIFeatureLevel::SM5);

#if LPV_VOLUME_TEXTURE
	if ( !bInitialized )
	{
		// @TODO: this should probably be derived from FRenderResource (with InitDynamicRHI etc)
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::CreateVolumeDesc(
			LPV_GRIDRES,
			LPV_GRIDRES,
			LPV_GRIDRES,
			PF_FloatRGBA,
			TexCreate_HideInVisualizeTexture,
			TexCreate_ShaderResource | TexCreate_UAV | TexCreate_FastVRAM,
			false,
			1));

		{
			const TCHAR* Names[] = { TEXT("LPV_A0"), TEXT("LPV_B0"), TEXT("LPV_A1"), TEXT("LPV_B1"), TEXT("LPV_A2"), TEXT("LPV_B2"), TEXT("LPV_A3"), TEXT("LPV_B3"), TEXT("LPV_A4"), TEXT("LPV_B4"), TEXT("LPV_A5"), TEXT("LPV_B5"), TEXT("LPV_A6"), TEXT("LPV_B6") };

			// Note: this is double-buffered!
			for ( int i = 0; i < 2; i++ )
			{
				for ( int j = 0; j < 7; j++ )
				{
					GRenderTargetPool.FindFreeElement(Desc, LpvVolumeTextures[i][j], Names[j * 2 + i] );
				}
			}
		}

#if LPV_GV_VOLUME_TEXTURE
		{
			const TCHAR* Names[] = { TEXT("LPV_GV0"), TEXT("LPV_GV1"), TEXT("LPV_GV2") };

			for(int i = 0; i < 3; i++)
			{
				GRenderTargetPool.FindFreeElement(Desc, GvVolumeTextures[i], Names[i]);
			}
		}
#endif
		bInitialized = true;
	}  
#endif


	// Copy the LPV postprocess settings
	Strength	 = View.FinalPostProcessSettings.LPVIntensity;
	bEnabled     = Strength > 0.0f;
	CubeSize	 = View.FinalPostProcessSettings.LPVSize;

	SecondaryOcclusionStrength = View.FinalPostProcessSettings.LPVSecondaryOcclusionIntensity;
	SecondaryBounceStrength = View.FinalPostProcessSettings.LPVSecondaryBounceIntensity;

	GeometryVolumeGenerated = false;

	if ( !bEnabled )
	{
		return;
	}

	float ClearMultiplier = 1.0f;

	// Clear the LPV for the first 2 frames (so both buffers are cleared initially)
	static int FrameCount = 0;
	if ( FrameCount < 2 )
	{
		bNeedsBufferClear = true;
	}
	FrameCount++;

	// Clear the UAVs if necessary
	if ( bNeedsBufferClear )
	{
#if !LPV_VOLUME_TEXTURE
		uint32 ClearValues[4] = {0};
		RHICmdList.ClearUAV( mVplListHeadBuffer->UAV, ClearValues );
#endif

		ClearMultiplier = 0.0f;
		bNeedsBufferClear = false;
	}

	mInjectedLightCount = 0;
	// Update the grid offset based on the camera
	{
		mOldGridOffset = mGridOffset;
		FVector CentrePos = View.ViewMatrices.ViewOrigin;
		FVector CameraAt = View.GetViewDirection();

		float LpvScale = CubeSize / float(LPV_GRIDRES);
		float OneOverLpvScale = 1.0f / LpvScale;

		CentrePos += CameraAt * (LPV_CENTRE_OFFSET*LpvScale);
		FVector HalfGridRes = FVector( LPV_GRIDRES/2, LPV_GRIDRES/2, LPV_GRIDRES/2 );
		FVector Offset = ( CentrePos * OneOverLpvScale - HalfGridRes ) * -1.0f;
		mGridOffset = FIntVector( Offset.X, Offset.Y, Offset.Z );

		LpvWriteUniformBufferParams->mOldGridOffset		= mOldGridOffset;
		LpvWriteUniformBufferParams->mLpvGridOffset		= mGridOffset;
		LpvWriteUniformBufferParams->mEyePos			= View.ViewMatrices.ViewOrigin;
		LpvWriteUniformBufferParams->ClearMultiplier	= ClearMultiplier;
		LpvWriteUniformBufferParams->LpvScale			= LpvScale;
		LpvWriteUniformBufferParams->OneOverLpvScale	= OneOverLpvScale;
		LpvWriteUniformBufferParams->SecondaryOcclusionStrength = SecondaryOcclusionStrength;
		LpvWriteUniformBufferParams->SecondaryBounceStrength = SecondaryBounceStrength;

		// Convert the bias values from LPV grid space to world space
		LpvWriteUniformBufferParams->GeometryVolumeInjectionBias	= View.FinalPostProcessSettings.LPVGeometryVolumeBias * LpvScale;
		LpvWriteUniformBufferParams->VplInjectionBias				= View.FinalPostProcessSettings.LPVVplInjectionBias * LpvScale;
		LpvWriteUniformBufferParams->PropagationIndex				= 0;
		LpvWriteUniformBufferParams->EmissiveInjectionMultiplier	= View.FinalPostProcessSettings.LPVEmissiveInjectionIntensity;


		LpvReadUniformBufferParams.mLpvGridOffset		= mGridOffset;
		LpvReadUniformBufferParams.LpvScale				= LpvScale;
		LpvReadUniformBufferParams.OneOverLpvScale		= OneOverLpvScale;

		LpvReadUniformBufferParams.LpvScale				= LpvScale;
		LpvReadUniformBufferParams.OneOverLpvScale		= OneOverLpvScale;

		// Compute the bounding box
		FVector Centre = ( FVector(mGridOffset.X, mGridOffset.Y, mGridOffset.Z ) + FVector(0.5f,0.5f,0.5f) - HalfGridRes ) * -LpvScale;
		FVector Extent = FVector(CubeSize,CubeSize,CubeSize) * 0.5f;
		BoundingBox = FBox( Centre-Extent, Centre+Extent );
	}
}

/**
* Clears the LPV
*/
void FLightPropagationVolume::Clear(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	if ( !bEnabled )
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, LpvClear);

	if ( !LpvWriteUniformBuffer.IsInitialized() )
	{
		LpvWriteUniformBuffer.InitResource();
	}
	LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

	// TODO: these could be run in parallel... 
	//RHIBatchComputeShaderBegin();
	// Clear the list buffers
	{
		TShaderMapRef<FLpvClearListsCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );
		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
		Shader->UnbindBuffers(RHICmdList);
	}

	// Clear the LPV (or fade, if REFINE_OVER_TIME is enabled)
	{
		TShaderMapRef<FLpvClearCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );
		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
		Shader->UnbindBuffers(RHICmdList);
	}

	// Clear the geometry volume if necessary
	if ( SecondaryOcclusionStrength > 0.001f )
	{
		TShaderMapRef<FLpvClearGeometryVolumeCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );
		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );
		Shader->UnbindBuffers(RHICmdList);
	}
	//RHIBatchComputeShaderEnd();

	RHICmdList.SetUAVParameter( FComputeShaderRHIRef(), 7, mVplListBuffer->UAV, 0 );
	RHICmdList.SetUAVParameter( FComputeShaderRHIRef(), 7, GvListBuffer->UAV, 0 );
	RHICmdList.SetUAVParameter( FComputeShaderRHIRef(), 7, FUnorderedAccessViewRHIParamRef(), 0 );
}

/**
* Gets shadow info
*/
void FLightPropagationVolume::GetShadowInfo( const FProjectedShadowInfo& ProjectedShadowInfo, FRsmInfo& RsmInfoOut )
{
	FIntPoint ShadowBufferResolution( ProjectedShadowInfo.ResolutionX, ProjectedShadowInfo.ResolutionY );
	RsmInfoOut.WorldToShadow = ProjectedShadowInfo.GetWorldToShadowMatrix(RsmInfoOut.ShadowmapMinMax, &ShadowBufferResolution );
	RsmInfoOut.ShadowToWorld = RsmInfoOut.WorldToShadow.InverseFast();

	// Determine the shadow area in world space, so we can scale the brightness if needed. 
	FVector ShadowUp    = FVector( 1.0f,0.0f,0.0f );
	FVector ShadowRight = FVector( 0.0f,1.0f,0.0f );
	FVector4 WorldUp = RsmInfoOut.ShadowToWorld.TransformVector( ShadowUp );
	FVector4 WorldRight = RsmInfoOut.ShadowToWorld.TransformVector( ShadowRight );
	float ShadowArea = WorldUp.Size3() * WorldRight.Size3();

	static float IdealCubeSizeMultiplier = 0.5f; 
	float IdealRsmArea = ( CubeSize * IdealCubeSizeMultiplier * CubeSize * IdealCubeSizeMultiplier );
	RsmInfoOut.AreaBrightnessMultiplier = ShadowArea/IdealRsmArea;
}


/**
* Injects a Directional light into the LPV
*/
void FLightPropagationVolume::SetVplInjectionConstants(
	const FProjectedShadowInfo&	ProjectedShadowInfo,
	const FLightSceneProxy*		LightProxy )
{
	FLinearColor LightColor = LightProxy->GetColor();
	FRsmInfo RsmInfo;
	GetShadowInfo( ProjectedShadowInfo, RsmInfo );

	float LpvStrength = 0.0f;
	if ( bEnabled )
	{
		LpvStrength = Strength;
	}
	LpvStrength *= RsmInfo.AreaBrightnessMultiplier;

	LpvStrength *= LightProxy->GetIndirectLightingScale();
	LpvWriteUniformBufferParams->mRsmToWorld = RsmInfo.ShadowToWorld;
	LpvWriteUniformBufferParams->mLightColour = FVector4( LightColor.R, LightColor.G, LightColor.B, LightColor.A ) * LpvStrength; 
	LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );
}

/**
* Injects a Directional light into the LPV
*/
void FLightPropagationVolume::InjectDirectionalLightRSM(
	FRHICommandListImmediate& RHICmdList,
	FViewInfo&					View,
	const FTexture2DRHIRef&		RsmDiffuseTex, 
	const FTexture2DRHIRef&		RsmNormalTex, 
	const FTexture2DRHIRef&		RsmDepthTex, 
	const FProjectedShadowInfo&	ProjectedShadowInfo,
	const FLinearColor&			LightColour )
{
	const FLightSceneProxy* LightProxy = ProjectedShadowInfo.GetLightSceneInfo().Proxy;
	{
		SCOPED_DRAW_EVENT(RHICmdList, LpvInjectDirectionalLightRSM);

		SetVplInjectionConstants(ProjectedShadowInfo, LightProxy );

		TShaderMapRef<FLpvInject_GenerateVplListsCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		// Clear the list counter the first time this function is called in a frame
		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams, RsmDiffuseTex, RsmNormalTex, RsmDepthTex );

		DispatchComputeShader(RHICmdList, *Shader, 256/8, 256/8, 1 ); 

		Shader->UnbindBuffers(RHICmdList); 
	}

	// If this is the first directional light, build the geometry volume with it
	if ( !GeometryVolumeGenerated && SecondaryOcclusionStrength > 0.001f )
	{
		SCOPED_DRAW_EVENT(RHICmdList, LpvBuildGeometryVolume);
		GeometryVolumeGenerated = true;
		FVector LightDirection( 0.0f, 0.0f, 1.0f );
		if ( LightProxy )
		{
			LightDirection = LightProxy->GetDirection();
		}
		LpvWriteUniformBufferParams->GeometryVolumeCaptureLightDirection = LightDirection;

		TShaderMapRef<FLpvBuildGeometryVolumeCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams ); // FIXME: is this causing a stall? Double-buffer?

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );

		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );

		Shader->UnbindBuffers(RHICmdList);
	}

	mInjectedLightCount++;
}

/**
* Propagates light in the LPV 
*/
void FLightPropagationVolume::Propagate(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	if ( !bEnabled )
	{
		return;
	}

	// Inject the VPLs into the LPV
	if ( mInjectedLightCount )
	{
		SCOPED_DRAW_EVENT(RHICmdList, LpvAccumulateVplLists);
		mWriteBufferIndex = 1-mWriteBufferIndex; // Swap buffers with each iteration

		TShaderMapRef<FLpvInject_AccumulateVplListsCS> Shader(View.ShaderMap);
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );
		Shader->SetParameters(RHICmdList, ShaderParams );

		DispatchComputeShader(RHICmdList, *Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );

		Shader->UnbindBuffers(RHICmdList);
	}

	// Propagate lighting, ping-ponging between the two buffers
	{
		static int PropagationStep = 0;

		SCOPED_DRAW_EVENT(RHICmdList, LpvPropagate);
		static int numIterations = 3;
		for ( int i=0; i<numIterations; i++ )
		{
			// Disable secondary occlusion on the first iteration
			bool bSecondaryOcclusion	= (SecondaryOcclusionStrength > 0.001f); 
			bool bSecondaryBounces		= (SecondaryBounceStrength > 0.001f); 
			mWriteBufferIndex = 1-mWriteBufferIndex; // Swap buffers with each iteration
			FLpvWriteShaderCSBase* Shader = GetPropagateShader(View, bSecondaryOcclusion, bSecondaryBounces);
			RHICmdList.SetComputeShader(Shader->GetComputeShader());

			LpvWriteUniformBufferParams->PropagationIndex = PropagationStep;
			PropagationStep++;

			LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

			FLpvBaseWriteShaderParams ShaderParams;
			GetShaderParams( ShaderParams );
			Shader->SetParameters(RHICmdList, ShaderParams );

			DispatchComputeShader(RHICmdList, Shader, LPV_GRIDRES/4, LPV_GRIDRES/4, LPV_GRIDRES/4 );

			Shader->UnbindBuffers(RHICmdList); 
		}
	}

	// Swap buffers
	mWriteBufferIndex = 1-mWriteBufferIndex; 
}

/**
* Helper function to generate shader params
*/
void FLightPropagationVolume::GetShaderParams( FLpvBaseWriteShaderParams& OutParams ) const
{
#if LPV_VOLUME_TEXTURE
	for(int i = 0; i < 7; ++i)
	{
		OutParams.LpvBufferSRVs[i] = LpvVolumeTextures[ 1 - mWriteBufferIndex ][i]->GetRenderTargetItem().ShaderResourceTexture;
		OutParams.LpvBufferUAVs[i] = LpvVolumeTextures[ mWriteBufferIndex ][i]->GetRenderTargetItem().UAV;
	}
#else
	OutParams.LpvBufferSRV = mLpvBuffers[ 1-mWriteBufferIndex ]->SRV;	// Double-buffered
	OutParams.LpvBufferUAV = mLpvBuffers[ mWriteBufferIndex ]->UAV;		// Double-buffered
#endif

	OutParams.VplListBufferSRV = mVplListBuffer->SRV;
	OutParams.VplListBufferUAV = mVplListBuffer->UAV;
	OutParams.VplListHeadBufferSRV = mVplListHeadBuffer->SRV;
	OutParams.VplListHeadBufferUAV = mVplListHeadBuffer->UAV;

#if LPV_GV_VOLUME_TEXTURE
	for(int i = 0; i < 3; ++i)
	{
		OutParams.GvBufferSRVs[i] = GvVolumeTextures[i]->GetRenderTargetItem().ShaderResourceTexture;
		OutParams.GvBufferUAVs[i] = GvVolumeTextures[i]->GetRenderTargetItem().UAV;
	}
#else
	OutParams.GvBufferSRV = GvBuffer->SRV;
	OutParams.GvBufferUAV = GvBuffer->UAV;
#endif
	OutParams.GvListBufferSRV = GvListBuffer->SRV;
	OutParams.GvListBufferUAV = GvListBuffer->UAV;
	OutParams.GvListHeadBufferSRV = GvListHeadBuffer->SRV;
	OutParams.GvListHeadBufferUAV = GvListHeadBuffer->UAV;

	OutParams.UniformBuffer = LpvWriteUniformBuffer;
}

void FLightPropagationVolume::InjectLightDirect(FRHICommandListImmediate& RHICmdList, const FLightSceneProxy& Light, const FViewInfo& View)
{
	if(!bEnabled)
	{
		return;
	}

	if(Light.GetLightType() != LightType_Point)
	{
		//@TODO - Only point lights supported right now
		return;
	}

	// A geometry volume is required for direct light injection. This currently requires a directional light to be injected
	//@TODO: Add support for generating a GV when there's no directional light
	if ( GeometryVolumeGenerated )
	{
		// Inject the VPLs into the LPV
		SCOPED_DRAW_EVENT(RHICmdList, LpvDirectLightInjection);

		FLpvDirectLightInjectParameters InjectUniformBufferParams;
		InjectUniformBufferParams.LightColor = Light.GetColor() * Light.GetIndirectLightingScale();
		InjectUniformBufferParams.LightPosition = Light.GetPosition();
		InjectUniformBufferParams.LightRadius = Light.GetRadius();
		InjectUniformBufferParams.LightFalloffExponent = 0.0f;

		FLpvInjectShader_Base* Shader = nullptr;
		if(Light.GetLightType() == LightType_Point)
		{
			// Todo:  hard coded - should be use inverse squared ones only?
			InjectUniformBufferParams.LightFalloffExponent = 8.0f;

			if ( Light.CastsStaticShadow() || Light.CastsDynamicShadow() )
			{
				Shader = (FLpvInjectShader_Base*)*TShaderMapRef<TLpvInject_PointLightCS<INJECT_SHADOWED> >(View.ShaderMap);
			}
			else
			{
				Shader = (FLpvInjectShader_Base*)*TShaderMapRef<TLpvInject_PointLightCS<0> >(View.ShaderMap);
			}
		}
		RHICmdList.SetComputeShader(Shader->GetComputeShader());

		FDirectLightInjectBufferRef InjectUniformBuffer = FDirectLightInjectBufferRef::CreateUniformBufferImmediate(InjectUniformBufferParams, UniformBuffer_SingleDraw);

		mWriteBufferIndex = 1 - mWriteBufferIndex; // Swap buffers with each iteration

		FLpvBaseWriteShaderParams ShaderParams;
		GetShaderParams( ShaderParams );

		LpvWriteUniformBuffer.SetContents( *LpvWriteUniformBufferParams );

		Shader->SetParameters(RHICmdList, ShaderParams, InjectUniformBuffer );
		DispatchComputeShader(RHICmdList, Shader, LPV_GRIDRES / 4, LPV_GRIDRES / 4, LPV_GRIDRES / 4 );
		Shader->UnbindBuffers(RHICmdList); 
	}

}


// use for render thread only
bool UseLightPropagationVolumeRT(ERHIFeatureLevel::Type InFeatureLevel)
{
	if (InFeatureLevel < ERHIFeatureLevel::SM5)
	{
		return false;
	}

	int32 Value = CVarLightPropagationVolume.GetValueOnRenderThread();

	return Value != 0;
}


// ----------------------------------------------------------------------------
// FSceneViewState
// ----------------------------------------------------------------------------
void FSceneViewState::CreateLightPropagationVolumeIfNeeded(ERHIFeatureLevel::Type InFeatureLevel)
{
	check(IsInRenderingThread());

	if(LightPropagationVolume)
	{
		// not needed
		return;
	}

	bool bLPV = UseLightPropagationVolumeRT(InFeatureLevel);

	//@todo-rco: Remove this when reenabling for OpenGL
	if (bLPV && !IsOpenGLPlatform(GShaderPlatformForFeatureLevel[InFeatureLevel]))
	{
		LightPropagationVolume = new FLightPropagationVolume();
	}
}

void FSceneViewState::DestroyLightPropagationVolume()
{
	if ( LightPropagationVolume ) 
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DeleteLPV,
			FLightPropagationVolume*, LightPropagationVolume, LightPropagationVolume,
		{
			delete LightPropagationVolume;
		}
		);
	}
	LightPropagationVolume = NULL;
}
