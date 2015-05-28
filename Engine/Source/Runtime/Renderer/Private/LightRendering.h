// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightRendering.h: Light rendering declarations.
=============================================================================*/

#ifndef __LIGHTRENDERING_H__
#define __LIGHTRENDERING_H__

#include "UniformBuffer.h"

/** Uniform buffer for rendering deferred lights. */
BEGIN_UNIFORM_BUFFER_STRUCT(FDeferredLightUniformStruct,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,LightPosition)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,LightInvRadius)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,LightColor)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,LightFalloffExponent)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,NormalizedLightDirection)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D,SpotAngles)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,SourceRadius)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,SourceLength)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,MinRoughness)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D,DistanceFadeMAD)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,ShadowMapChannelMask)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,bShadowed)
END_UNIFORM_BUFFER_STRUCT(FDeferredLightUniformStruct)

extern float GMinScreenRadiusForLights;

template<typename ShaderRHIParamRef>
void SetDeferredLightParameters(
	FRHICommandList& RHICmdList, 
	const ShaderRHIParamRef ShaderRHI, 
	const TShaderUniformBufferParameter<FDeferredLightUniformStruct>& DeferredLightUniformBufferParameter, 
	const FLightSceneInfo* LightSceneInfo,
	const FSceneView& View)
{
	FVector4 LightPositionAndInvRadius;
	FVector4 LightColorAndFalloffExponent;
		
	FDeferredLightUniformStruct DeferredLightUniformsValue;

	// Get the light parameters
	LightSceneInfo->Proxy->GetParameters(
		LightPositionAndInvRadius,
		LightColorAndFalloffExponent,
		DeferredLightUniformsValue.NormalizedLightDirection,
		DeferredLightUniformsValue.SpotAngles,
		DeferredLightUniformsValue.SourceRadius,
		DeferredLightUniformsValue.SourceLength,
		DeferredLightUniformsValue.MinRoughness);
	
	DeferredLightUniformsValue.LightPosition = LightPositionAndInvRadius;
	DeferredLightUniformsValue.LightInvRadius = LightPositionAndInvRadius.W;
	DeferredLightUniformsValue.LightColor = LightColorAndFalloffExponent;
	DeferredLightUniformsValue.LightFalloffExponent = LightColorAndFalloffExponent.W;

	const FVector2D FadeParams = LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), LightSceneInfo->IsPrecomputedLightingValid());

	// use MAD for efficiency in the shader
	DeferredLightUniformsValue.DistanceFadeMAD = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);

	int32 ShadowMapChannel = LightSceneInfo->Proxy->GetShadowMapChannel();

	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

	if (!bAllowStaticLighting)
	{
		ShadowMapChannel = INDEX_NONE;
	}

	DeferredLightUniformsValue.ShadowMapChannelMask = FVector4(
		ShadowMapChannel == 0 ? 1 : 0,
		ShadowMapChannel == 1 ? 1 : 0,
		ShadowMapChannel == 2 ? 1 : 0,
		ShadowMapChannel == 3 ? 1 : 0);

	const bool bHasLightFunction = LightSceneInfo->Proxy->GetLightFunctionMaterial() != NULL;
	DeferredLightUniformsValue.bShadowed = LightSceneInfo->Proxy->CastsDynamicShadow() || LightSceneInfo->Proxy->CastsStaticShadow() || bHasLightFunction;

	if( LightSceneInfo->Proxy->IsInverseSquared() )
	{
		// Correction for lumen units
		DeferredLightUniformsValue.LightColor *= 16.0f;
	}

	// When rendering reflection captures, the direct lighting of the light is actually the indirect specular from the main view
	if (View.bIsReflectionCapture)
	{
		DeferredLightUniformsValue.LightColor *= LightSceneInfo->Proxy->GetIndirectLightingScale();
	}

	{
		// Distance fade
		FSphere Bounds = LightSceneInfo->Proxy->GetBoundingSphere();

		const float DistanceSquared = ( Bounds.Center - View.ViewMatrices.ViewOrigin ).SizeSquared();
		float Fade = FMath::Square( FMath::Min( 0.0002f, GMinScreenRadiusForLights / Bounds.W ) * View.LODDistanceFactor ) * DistanceSquared;
		Fade = FMath::Clamp( 6.0f - 6.0f * Fade, 0.0f, 1.0f );
		DeferredLightUniformsValue.LightColor *= Fade;
	}

	SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI,DeferredLightUniformBufferParameter,DeferredLightUniformsValue);
}

template<typename ShaderRHIParamRef>
void SetSimpleDeferredLightParameters(
	FRHICommandList& RHICmdList, 
	const ShaderRHIParamRef ShaderRHI, 
	const TShaderUniformBufferParameter<FDeferredLightUniformStruct>& DeferredLightUniformBufferParameter, 
	const FSimpleLightEntry& SimpleLight,
	const FSimpleLightPerViewEntry &SimpleLightPerViewData,
	const FSceneView& View)
{
	FDeferredLightUniformStruct DeferredLightUniformsValue;
	DeferredLightUniformsValue.LightPosition = SimpleLightPerViewData.Position;
	DeferredLightUniformsValue.LightInvRadius = 1.0f / FMath::Max(SimpleLight.Radius, KINDA_SMALL_NUMBER);
	DeferredLightUniformsValue.LightColor = SimpleLight.Color;
	DeferredLightUniformsValue.LightFalloffExponent = SimpleLight.Exponent;
	DeferredLightUniformsValue.NormalizedLightDirection = FVector(1, 0, 0);
	DeferredLightUniformsValue.MinRoughness = 0.08f;
	DeferredLightUniformsValue.SpotAngles = FVector2D(-2, 1);
	DeferredLightUniformsValue.DistanceFadeMAD = FVector2D(0, 0);
	DeferredLightUniformsValue.ShadowMapChannelMask = FVector4(0, 0, 0, 0);
	DeferredLightUniformsValue.SourceRadius = 0;
	DeferredLightUniformsValue.SourceLength = 0;

	SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, DeferredLightUniformBufferParameter, DeferredLightUniformsValue);
}

/** Shader parameters needed to render a light function. */
class FLightFunctionSharedParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		LightFunctionParameters.Bind(ParameterMap,TEXT("LightFunctionParameters"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FLightSceneInfo* LightSceneInfo, float ShadowFadeFraction) const
	{
		const bool bIsSpotLight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
		const bool bIsPointLight = LightSceneInfo->Proxy->GetLightType() == LightType_Point;
		const float TanOuterAngle = bIsSpotLight ? FMath::Tan(LightSceneInfo->Proxy->GetOuterConeAngle()) : 1.0f;

		SetShaderValue( 
			RHICmdList, 
			ShaderRHI, 
			LightFunctionParameters, 
			FVector4(TanOuterAngle, ShadowFadeFraction, bIsSpotLight ? 1.0f : 0.0f, bIsPointLight ? 1.0f : 0.0f));
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FLightFunctionSharedParameters& P)
	{
		Ar << P.LightFunctionParameters;
		return Ar;
	}

private:

	FShaderParameter LightFunctionParameters;
};

/** A vertex shader for rendering the light in a deferred pass. */
template<bool bRadialLight>
class TDeferredLightVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDeferredLightVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	TDeferredLightVS()	{}
	TDeferredLightVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		StencilingGeometryParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FLightSceneInfo* LightSceneInfo)
	{
		FGlobalShader::SetParameters(RHICmdList, GetVertexShader(),View);
		StencilingGeometryParameters.Set(RHICmdList, this, View, LightSceneInfo);
	}

	void SetSimpleLightParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FSphere& LightBounds)
	{
		FGlobalShader::SetParameters(RHICmdList, GetVertexShader(),View);

		FVector4 StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, LightBounds, View.ViewMatrices.PreViewTranslation);
		StencilingGeometryParameters.Set(RHICmdList, this, StencilingSpherePosAndScale);
	}

	// Begin FShader Interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StencilingGeometryParameters;
		return bShaderHasOutdatedParameters;
	}
	// End FShader Interface
private:
	FStencilingGeometryShaderParameters StencilingGeometryParameters;
};

extern void SetBoundingGeometryRasterizerAndDepthState(FRHICommandList& RHICmdList, const FViewInfo& View, const FSphere& LightBounds);

#endif
