// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightFunctionRendering.cpp: Implementation for rendering light functions.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "LightRendering.h"
#include "SceneUtils.h"

/**
 * A vertex shader for projecting a light function onto the scene.
 */
class FLightFunctionVS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FLightFunctionVS,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsLightFunction() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FLightFunctionVS( )	{ }
	FLightFunctionVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		StencilingGeometryParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView* View, const FLightSceneInfo* LightSceneInfo )
	{
		FMaterialShader::SetParameters(RHICmdList, GetVertexShader(), *View);
		
		// Light functions are projected using a bounding sphere.
		// Calculate transform for bounding stencil sphere.
		FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();
		if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
		{
			LightBounds.Center = View->ViewMatrices.ViewOrigin;
		}

		FVector4 StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, LightBounds, View->ViewMatrices.PreViewTranslation);
		StencilingGeometryParameters.Set(RHICmdList, this, StencilingSpherePosAndScale);
	}

	// Begin FShader interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << StencilingGeometryParameters;
		return bShaderHasOutdatedParameters;
	}
	//  End FShader interface 

private:
	FStencilingGeometryShaderParameters StencilingGeometryParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FLightFunctionVS,TEXT("LightFunctionVertexShader"),TEXT("Main"),SF_Vertex);

/**
 * A pixel shader for projecting a light function onto the scene.
 */
class FLightFunctionPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FLightFunctionPS,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsLightFunction() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FLightFunctionPS() {}
	FLightFunctionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		ScreenToLight.Bind(Initializer.ParameterMap,TEXT("ScreenToLight"));
		LightFunctionParameters.Bind(Initializer.ParameterMap);
		LightFunctionParameters2.Bind(Initializer.ParameterMap,TEXT("LightFunctionParameters2"));
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView* View, const FLightSceneInfo* LightSceneInfo, const FMaterialRenderProxy* MaterialProxy, bool bRenderingPreviewShadowIndicator, float ShadowFadeFraction )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View->GetFeatureLevel()), *View, true, ESceneRenderTargetsMode::SetTextures);

		// Set the transform from screen space to light space.
		if ( ScreenToLight.IsBound() )
		{
			const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
			// Switch x and z so that z of the user specified scale affects the distance along the light direction
			const FVector InverseScale = FVector( 1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X );
			const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));	
			const FMatrix ScreenToLightValue = 
				FMatrix(
					FPlane(1,0,0,0),
					FPlane(0,1,0,0),
					FPlane(0,0,View->ViewMatrices.ProjMatrix.M[2][2],1),
					FPlane(0,0,View->ViewMatrices.ProjMatrix.M[3][2],0))
				* View->InvViewProjectionMatrix * WorldToLight;

			SetShaderValue(RHICmdList, ShaderRHI, ScreenToLight, ScreenToLightValue );
		}

		LightFunctionParameters.Set(RHICmdList, ShaderRHI, LightSceneInfo, ShadowFadeFraction);

		SetShaderValue(RHICmdList, ShaderRHI, LightFunctionParameters2, FVector(
			LightSceneInfo->Proxy->GetLightFunctionFadeDistance(), 
			LightSceneInfo->Proxy->GetLightFunctionDisabledBrightness(),
			bRenderingPreviewShadowIndicator ? 1.0f : 0.0f));

		DeferredParameters.Set(RHICmdList, ShaderRHI, *View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << ScreenToLight;
		Ar << LightFunctionParameters;
		Ar << LightFunctionParameters2;
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ScreenToLight;
	FLightFunctionSharedParameters LightFunctionParameters;
	FShaderParameter LightFunctionParameters2;
	FDeferredPixelShaderParameters DeferredParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FLightFunctionPS,TEXT("LightFunctionPixelShader"),TEXT("Main"),SF_Pixel);

/** Returns a fade fraction for a light function and a given view based on the appropriate fade settings. */
static float GetLightFunctionFadeFraction(const FViewInfo& View, FSphere LightBounds)
{
	extern float CalculateShadowFadeAlpha(float MaxUnclampedResolution, int32 ShadowFadeResolution, int32 MinShadowResolution);

	// Override the global settings with the light's settings if the light has them specified
	static auto CVarMinShadowResolution = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.MinResolution"));
	static auto CVarShadowFadeResolution = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.FadeResolution"));

	const uint32 MinShadowResolution = CVarMinShadowResolution->GetValueOnRenderThread();
	const int32 ShadowFadeResolution = CVarShadowFadeResolution->GetValueOnRenderThread();

	// Project the bounds onto the view
	const FVector4 ScreenPosition = View.WorldToScreen(LightBounds.Center);

	int32 SizeX = View.ViewRect.Width();
	int32 SizeY = View.ViewRect.Height();
	const float ScreenRadius = FMath::Max(
		SizeX / 2.0f * View.ViewMatrices.ProjMatrix.M[0][0],
		SizeY / 2.0f * View.ViewMatrices.ProjMatrix.M[1][1]) *
		LightBounds.W /
		FMath::Max(ScreenPosition.W, 1.0f);

	static auto CVarShadowTexelsPerPixel = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.TexelsPerPixel"));
	const float UnclampedResolution = ScreenRadius * CVarShadowTexelsPerPixel->GetValueOnRenderThread();
	const float ResolutionFadeAlpha = CalculateShadowFadeAlpha(UnclampedResolution, ShadowFadeResolution, MinShadowResolution);
	return ResolutionFadeAlpha;
}

/**
* Used by RenderLights to figure out if light functions need to be rendered to the attenuation buffer.
*
* @param LightSceneInfo Represents the current light
* @return true if anything got rendered
*/
bool FDeferredShadingSceneRenderer::CheckForLightFunction( const FLightSceneInfo* LightSceneInfo ) const
{
	// NOTE: The extra check is necessary because there could be something wrong with the material.
	if( LightSceneInfo->Proxy->GetLightFunctionMaterial() && 
		LightSceneInfo->Proxy->GetLightFunctionMaterial()->GetMaterial(Scene->GetFeatureLevel())->IsLightFunction())
	{
		FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();
		for (int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
			{
				LightBounds.Center = View.ViewMatrices.ViewOrigin;
			}

			if(View.VisibleLightInfos[LightSceneInfo->Id].bInViewFrustum
				// Only draw the light function if it hasn't completely faded out
				&& GetLightFunctionFadeFraction(View, LightBounds) > 1.0f / 256.0f)
			{
				return true;
			}
		}
	}
	return false;
}

/**
 * Used by RenderLights to render a light function to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 */
bool FDeferredShadingSceneRenderer::RenderLightFunction(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bLightAttenuationCleared)
{
	if (ViewFamily.EngineShowFlags.LightFunctions)
	{
		return RenderLightFunctionForMaterial(RHICmdList, LightSceneInfo, LightSceneInfo->Proxy->GetLightFunctionMaterial(), bLightAttenuationCleared, false);
	}
	
	return false;
}

bool FDeferredShadingSceneRenderer::RenderPreviewShadowsIndicator(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bLightAttenuationCleared)
{
	if (GEngine->PreviewShadowsIndicatorMaterial)
	{
		return RenderLightFunctionForMaterial(RHICmdList, LightSceneInfo, GEngine->PreviewShadowsIndicatorMaterial->GetRenderProxy(false), bLightAttenuationCleared, true);
	}

	return false;
}

bool FDeferredShadingSceneRenderer::RenderLightFunctionForMaterial(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FMaterialRenderProxy* MaterialProxy, bool bLightAttenuationCleared, bool bRenderingPreviewShadowsIndicator)
{
	bool bRenderedLightFunction = false;

	if (MaterialProxy && MaterialProxy->GetMaterial(Scene->GetFeatureLevel())->IsLightFunction())
	{
		GSceneRenderTargets.BeginRenderingLightAttenuation(RHICmdList);

		bRenderedLightFunction = true;

		const FMaterial* Material = MaterialProxy->GetMaterial(Scene->GetFeatureLevel());
		SCOPED_DRAW_EVENTF(RHICmdList, LightFunction, TEXT("LightFunction Material=%s"), *Material->GetFriendlyName());

		const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();
		FLightFunctionVS* VertexShader = MaterialShaderMap->GetShader<FLightFunctionVS>();
		FLightFunctionPS* PixelShader = MaterialShaderMap->GetShader<FLightFunctionPS>();

		FLocalBoundShaderState LightFunctionBoundShaderState = RHICmdList.BuildLocalBoundShaderState(GetVertexDeclarationFVector4(), VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), PixelShader->GetPixelShader(), FGeometryShaderRHIRef());

		FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

		// Render to the light attenuation buffer for all views.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			const FViewInfo& View = Views[ViewIndex];

			if (View.VisibleLightInfos[LightSceneInfo->Id].bInViewFrustum)
			{
				if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
				{
					LightBounds.Center = View.ViewMatrices.ViewOrigin;
				}

				const float FadeAlpha = GetLightFunctionFadeFraction(View, LightBounds);
				// Don't draw the light function if it has completely faded out
				if (FadeAlpha < 1.0f / 256.0f)
				{
					if( !bLightAttenuationCleared )
					{
						LightSceneInfo->Proxy->SetScissorRect(RHICmdList, View);
						RHICmdList.Clear(true, FLinearColor::White, false, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect());
					}
				}
				else
				{
					// Set the device viewport for the view.
					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

					// Set the states to modulate the light function with the render target.
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

					if( bLightAttenuationCleared )
					{
						if (bRenderingPreviewShadowsIndicator)
						{
							RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA,BO_Max,BF_One,BF_One,BO_Max,BF_One,BF_One>::GetRHI());
						}
						else
						{
							// Light attenuation buffer has been remapped. 
							// Light function shadows now write to the blue channel.

							// Use modulated blending to BA since light functions are combined in the same buffer as normal shadows
							RHICmdList.SetBlendState(TStaticBlendState<CW_BA,BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI());
						}
					}
					else
					{
						RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
					}

					if (((FVector)View.ViewMatrices.ViewOrigin - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f))
					{
						// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light function geometry
						RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
					}
					else
					{
						// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light function geometry
						RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());
						RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI());
					}

					// Set the light's scissor rectangle.
					LightSceneInfo->Proxy->SetScissorRect(RHICmdList, View);

					// Render a bounding light sphere.
					RHICmdList.SetLocalBoundShaderState(LightFunctionBoundShaderState);
					VertexShader->SetParameters(RHICmdList, &View, LightSceneInfo);
					PixelShader->SetParameters(RHICmdList, &View, LightSceneInfo, MaterialProxy, bRenderingPreviewShadowsIndicator, FadeAlpha);

					// Project the light function using a sphere around the light
					//@todo - could use a cone for spotlights
					StencilingGeometry::DrawSphere(RHICmdList);
				}
			}
		}

		// Restore states.
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

		if (bRenderedLightFunction)
		{
			// Restore stencil buffer to all 0's which is the assumed default state
			RHICmdList.Clear(false,FColor(0,0,0),false,(float)ERHIZBuffer::FarPlane,true,0, FIntRect());
		}
	}
	return bRenderedLightFunction;
}
