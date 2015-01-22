// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SystemTextures.cpp: System textures implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "AtmosphereTextures.h"

void FAtmosphereTextures::InitDynamicRHI()
{
	check(PrecomputeParams != NULL);
	// Allocate atmosphere precompute textures...
	{
		// todo: Expose
		// Transmittance
		FIntPoint GTransmittanceTexSize(PrecomputeParams->TransmittanceTexWidth, PrecomputeParams->TransmittanceTexHeight);
		FPooledRenderTargetDesc TransmittanceDesc(FPooledRenderTargetDesc::Create2DDesc(GTransmittanceTexSize, PF_FloatRGBA, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(TransmittanceDesc, AtmosphereTransmittance, TEXT("AtmosphereTransmittance"));

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		SetRenderTarget(RHICmdList, AtmosphereTransmittance->GetRenderTargetItem().TargetableTexture, FTextureRHIRef());
		RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 0, false, 0, FIntRect());
		RHICmdList.CopyToResolveTarget(AtmosphereTransmittance->GetRenderTargetItem().TargetableTexture, AtmosphereTransmittance->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams());

		// Irradiance
		FIntPoint GIrradianceTexSize(PrecomputeParams->IrradianceTexWidth, PrecomputeParams->IrradianceTexHeight);
		FPooledRenderTargetDesc IrradianceDesc(FPooledRenderTargetDesc::Create2DDesc(GIrradianceTexSize, PF_FloatRGBA, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(IrradianceDesc, AtmosphereIrradiance, TEXT("AtmosphereIrradiance"));

		SetRenderTarget(RHICmdList, AtmosphereIrradiance->GetRenderTargetItem().TargetableTexture, FTextureRHIRef());
		RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 0, false, 0, FIntRect());
		RHICmdList.CopyToResolveTarget(AtmosphereIrradiance->GetRenderTargetItem().TargetableTexture, AtmosphereIrradiance->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams());

		// DeltaE
		GRenderTargetPool.FindFreeElement(IrradianceDesc, AtmosphereDeltaE, TEXT("AtmosphereDeltaE"));

		// 3D Texture
		// Inscatter
		FPooledRenderTargetDesc InscatterDesc(FPooledRenderTargetDesc::CreateVolumeDesc(PrecomputeParams->InscatterMuSNum * PrecomputeParams->InscatterNuNum, PrecomputeParams->InscatterMuNum, PrecomputeParams->InscatterAltitudeSampleNum, PF_FloatRGBA, TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(InscatterDesc, AtmosphereInscatter, TEXT("AtmosphereInscatter"));

		// DeltaSR
		GRenderTargetPool.FindFreeElement(InscatterDesc, AtmosphereDeltaSR, TEXT("AtmosphereDeltaSR"));

		// DeltaSM
		GRenderTargetPool.FindFreeElement(InscatterDesc, AtmosphereDeltaSM, TEXT("AtmosphereDeltaSM"));

		// DeltaJ
		GRenderTargetPool.FindFreeElement(InscatterDesc, AtmosphereDeltaJ, TEXT("AtmosphereDeltaJ"));
	}
}

void FAtmosphereTextures::ReleaseDynamicRHI()
{
	AtmosphereTransmittance.SafeRelease();
	AtmosphereIrradiance.SafeRelease();
	AtmosphereDeltaE.SafeRelease();

	AtmosphereInscatter.SafeRelease();
	AtmosphereDeltaSR.SafeRelease();
	AtmosphereDeltaSM.SafeRelease();
	AtmosphereDeltaJ.SafeRelease();

	GRenderTargetPool.FreeUnusedResources();
}


void FAtmosphereShaderTextureParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	TransmittanceTexture.Bind(ParameterMap,TEXT("AtmosphereTransmittanceTexture"));
	TransmittanceTextureSampler.Bind(ParameterMap,TEXT("AtmosphereTransmittanceTextureSampler"));
	IrradianceTexture.Bind(ParameterMap,TEXT("AtmosphereIrradianceTexture"));
	IrradianceTextureSampler.Bind(ParameterMap,TEXT("AtmosphereIrradianceTextureSampler"));
	InscatterTexture.Bind(ParameterMap,TEXT("AtmosphereInscatterTexture"));
	InscatterTextureSampler.Bind(ParameterMap,TEXT("AtmosphereInscatterTextureSampler"));
}

template< typename ShaderRHIParamRef >
void FAtmosphereShaderTextureParameters::Set( FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View ) const
{
	if (TransmittanceTexture.IsBound() || IrradianceTexture.IsBound() || InscatterTexture.IsBound())
	{
		SetTextureParameter(RHICmdList, ShaderRHI, TransmittanceTexture, TransmittanceTextureSampler, 
			TStaticSamplerState<SF_Bilinear>::GetRHI(), View.AtmosphereTransmittanceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, IrradianceTexture, IrradianceTextureSampler, 
			TStaticSamplerState<SF_Bilinear>::GetRHI(), View.AtmosphereIrradianceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, InscatterTexture, InscatterTextureSampler, 
			TStaticSamplerState<SF_Bilinear>::GetRHI(), View.AtmosphereInscatterTexture);
	}
}

#define IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( ShaderRHIParamRef ) \
	template void FAtmosphereShaderTextureParameters::Set< ShaderRHIParamRef >( FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View ) const;

IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FVertexShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FHullShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FDomainShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FGeometryShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FPixelShaderRHIParamRef );
IMPLEMENT_ATMOSPHERE_TEXTURE_PARAM_SET( FComputeShaderRHIParamRef );

FArchive& operator<<(FArchive& Ar,FAtmosphereShaderTextureParameters& Parameters)
{
	Ar << Parameters.TransmittanceTexture;
	Ar << Parameters.TransmittanceTextureSampler;
	Ar << Parameters.IrradianceTexture;
	Ar << Parameters.IrradianceTextureSampler;
	Ar << Parameters.InscatterTexture;
	Ar << Parameters.InscatterTextureSampler;
	return Ar;
}
