//-----------------------------------------------------------------------------
// File:		LightPropagationVolume.h
//
// Summary:		Light Propagation Volumes implementation 
//
// Created:		2013-03-01
//
// Author:		mailto:benwood@microsoft.com
//
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

#include "UniformBuffer.h"

#define LPV_MULTIPLE_BOUNCES  1
#define LPV_VOLUME_TEXTURE    1
#define LPV_GV_VOLUME_TEXTURE 1

class FLpvWriteUniformBufferParameters;
struct FLpvBaseWriteShaderParams;
typedef TUniformBufferRef<FLpvWriteUniformBufferParameters> FLpvWriteUniformBufferRef;
typedef TUniformBuffer<FLpvWriteUniformBufferParameters> FLpvWriteUniformBuffer;
struct FRsmInfo;

#if LPV_VOLUME_TEXTURE
static const TCHAR* LpvVolumeTextureSRVNames[7] = { 
	TEXT("gLpv3DTexture0"),
	TEXT("gLpv3DTexture1"),
	TEXT("gLpv3DTexture2"),
	TEXT("gLpv3DTexture3"),
	TEXT("gLpv3DTexture4"),
	TEXT("gLpv3DTexture5"),
	TEXT("gLpv3DTexture6") };

static const TCHAR* LpvVolumeTextureUAVNames[7] = { 
	TEXT("gLpv3DTextureRW0"),
	TEXT("gLpv3DTextureRW1"),
	TEXT("gLpv3DTextureRW2"),
	TEXT("gLpv3DTextureRW3"),
	TEXT("gLpv3DTextureRW4"),
	TEXT("gLpv3DTextureRW5"),
	TEXT("gLpv3DTextureRW6") };
#endif

#if LPV_GV_VOLUME_TEXTURE
static const TCHAR* LpvGvVolumeTextureSRVNames[3] = { 
	TEXT("gGv3DTexture0"),
	TEXT("gGv3DTexture1"),
	TEXT("gGv3DTexture2") };

static const TCHAR* LpvGvVolumeTextureUAVNames[3] = { 
	TEXT("gGv3DTextureRW0"),
	TEXT("gGv3DTextureRW1"),
	TEXT("gGv3DTextureRW2") };
#endif

//
// LPV Read constant buffer
//
BEGIN_UNIFORM_BUFFER_STRUCT( FLpvReadUniformBufferParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FIntVector, mLpvGridOffset )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, LpvScale )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, OneOverLpvScale )
END_UNIFORM_BUFFER_STRUCT( FLpvReadUniformBufferParameters )


/**
 * Uniform buffer parameters for LPV write shaders
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FLpvWriteUniformBufferParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FMatrix, mRsmToWorld )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, mLightColour )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, GeometryVolumeCaptureLightDirection )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, mEyePos )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FIntVector, mOldGridOffset ) 
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FIntVector, mLpvGridOffset )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, ClearMultiplier )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, LpvScale )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, OneOverLpvScale )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, WarpStrength )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, SecondaryOcclusionStrength )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, SecondaryBounceStrength )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, VplInjectionBias )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, GeometryVolumeInjectionBias )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, EmissiveInjectionMultiplier )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( int,	 PropagationIndex )
END_UNIFORM_BUFFER_STRUCT( FLpvWriteUniformBufferParameters )

// ----------------------------------------------------------------------------
// Shader params for base LPV write shaders
// ----------------------------------------------------------------------------
struct FLpvBaseWriteShaderParams
{
	FLpvWriteUniformBufferRef		UniformBuffer;
#if LPV_VOLUME_TEXTURE
	FTextureRHIParamRef				LpvBufferSRVs[7];
	FUnorderedAccessViewRHIParamRef LpvBufferUAVs[7];
#else
	FShaderResourceViewRHIParamRef	LpvBufferSRV;
	FUnorderedAccessViewRHIParamRef LpvBufferUAV;
#endif

	FShaderResourceViewRHIParamRef	VplListHeadBufferSRV;
	FUnorderedAccessViewRHIParamRef VplListHeadBufferUAV;
	FShaderResourceViewRHIParamRef	VplListBufferSRV;
	FUnorderedAccessViewRHIParamRef VplListBufferUAV;

#if LPV_GV_VOLUME_TEXTURE
	FTextureRHIParamRef				GvBufferSRVs[3];
	FUnorderedAccessViewRHIParamRef GvBufferUAVs[3];
#else
	FShaderResourceViewRHIParamRef	GvBufferSRV;
	FUnorderedAccessViewRHIParamRef GvBufferUAV;
#endif
	FShaderResourceViewRHIParamRef	GvListHeadBufferSRV;
	FUnorderedAccessViewRHIParamRef GvListHeadBufferUAV;
	FShaderResourceViewRHIParamRef	GvListBufferSRV;
	FUnorderedAccessViewRHIParamRef GvListBufferUAV;
};

class FLightPropagationVolume // @TODO: this should probably be derived from FRenderResource (with InitDynamicRHI etc)
{
public:
	FLightPropagationVolume();
	virtual ~FLightPropagationVolume();

	void InitSettings(FRHICommandList& RHICmdList, const FSceneView& View);

	void Clear(FRHICommandListImmediate& RHICmdList, FViewInfo& View);

	void SetVplInjectionConstants(
		const FProjectedShadowInfo&	ProjectedShadowInfo,
		const FLightSceneProxy* LightProxy );

	void InjectDirectionalLightRSM(
		FRHICommandListImmediate& RHICmdList,
		FViewInfo&					View,
		const FTexture2DRHIRef&		RsmDiffuseTex, 
		const FTexture2DRHIRef&		RsmNormalTex, 
		const FTexture2DRHIRef&		RsmDepthTex, 
		const FProjectedShadowInfo&	ProjectedShadowInfo,
		const FLinearColor&			LightColour );

	void InjectLightDirect(FRHICommandListImmediate& RHICmdList, const FLightSceneProxy& Light, const FViewInfo& View);

	void Propagate(FRHICommandListImmediate& RHICmdList, FViewInfo& View);

	void Visualise(FRHICommandList& RHICmdList, const FViewInfo& View) const;

	const FIntVector& GetGridOffset() const { return mGridOffset; }

	const FLpvReadUniformBufferParameters& GetReadUniformBufferParams()		{ return LpvReadUniformBufferParams; }
	const FLpvWriteUniformBufferParameters& GetWriteUniformBufferParams()	{ return *LpvWriteUniformBufferParams; }

	FLpvWriteUniformBufferRef GetWriteUniformBuffer() const				{ return (FLpvWriteUniformBufferRef)LpvWriteUniformBuffer; }

#if LPV_VOLUME_TEXTURE
	FTextureRHIParamRef GetLpvBufferSrv( int i )						{ return LpvVolumeTextures[ 1-mWriteBufferIndex ][i]->GetRenderTargetItem().ShaderResourceTexture; }
#else
	FShaderResourceViewRHIParamRef GetLpvBufferSrv()					{ return mLpvBuffers[ 1-mWriteBufferIndex ]->SRV; }
#endif

	FUnorderedAccessViewRHIParamRef GetVplListBufferUav()				{ return mVplListBuffer->UAV; }
	FUnorderedAccessViewRHIParamRef GetVplListHeadBufferUav()			{ return mVplListHeadBuffer->UAV; }

	FUnorderedAccessViewRHIParamRef GetGvListBufferUav()				{ return GvListBuffer->UAV; }
	FUnorderedAccessViewRHIParamRef GetGvListHeadBufferUav()			{ return GvListHeadBuffer->UAV; }


	const FBox&	GetBoundingBox()										{ return BoundingBox; }

//private:	// MM
	void GetShaderParams( FLpvBaseWriteShaderParams& OutParams) const;
public:

	void GetShadowInfo( const FProjectedShadowInfo& ProjectedShadowInfo, FRsmInfo& RsmInfoOut );

#if LPV_VOLUME_TEXTURE
	TRefCountPtr<IPooledRenderTarget>	LpvVolumeTextures[2][7];		// double buffered
#else
	FRWBufferStructured*				mLpvBuffers[2];					// Double buffered for propagation
#endif
	FRWBufferByteAddress*				mVplListHeadBuffer;
	FRWBufferStructured*				mVplListBuffer;

	FIntVector							mGridOffset;
	FIntVector							mOldGridOffset;

	FLpvWriteUniformBufferParameters*	LpvWriteUniformBufferParams;
	FLpvReadUniformBufferParameters     LpvReadUniformBufferParams;

	uint32								mInjectedLightCount;

	// Geometry volume
	FRWBufferByteAddress*				GvListHeadBuffer;
	FRWBufferStructured*				GvListBuffer;

#if LPV_VOLUME_TEXTURE || LPV_GV_VOLUME_TEXTURE
	FShaderResourceParameter			LpvVolumeTextureSampler;
#endif

#if LPV_GV_VOLUME_TEXTURE
	TRefCountPtr<IPooledRenderTarget>	GvVolumeTextures[3];		// 9 coeffs + RGB
#else
	FRWBuffer*							GvBuffer;			
#endif
	float								SecondaryOcclusionStrength;
	float								SecondaryBounceStrength;

	float								CubeSize;
	float								Strength;
	bool								bEnabled;

	uint32								mWriteBufferIndex;
	bool								bNeedsBufferClear;

	FBox								BoundingBox;
	bool								GeometryVolumeGenerated; 

	FLpvWriteUniformBuffer				LpvWriteUniformBuffer;

	bool								bInitialized;

	friend class FLpvVisualisePS;
};


// use for render thread only
bool UseLightPropagationVolumeRT(ERHIFeatureLevel::Type InFeatureLevel);
