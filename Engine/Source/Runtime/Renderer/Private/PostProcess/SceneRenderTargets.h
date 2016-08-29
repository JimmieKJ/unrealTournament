// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRenderTargets.h: Scene render target definitions.
=============================================================================*/

#pragma once

#include "ShaderParameters.h"
#include "RenderTargetPool.h"
#include "../SystemTextures.h"
#include "RHIStaticStates.h"

struct IPooledRenderTarget;

/** Number of cube map shadow depth surfaces that will be created and used for rendering one pass point light shadows. */
static const int32 NumCubeShadowDepthSurfaces = 5;

/** 
 * Allocate enough sets of translucent volume textures to cover all the cascades, 
 * And then one more which will be used as a scratch target when doing ping-pong operations like filtering. 
 */
static const int32 NumTranslucentVolumeRenderTargetSets = TVC_MAX + 1;

/** Forward declaration of console variable controlling translucent volume blur */
extern int32 GUseTranslucencyVolumeBlur;

/** Forward declaration of console variable controlling translucent lighting volume dimensions */
extern int32 GTranslucencyLightingVolumeDim;

/** Function to select the index of the volume texture that we will hold the final translucency lighting volume texture */
inline int SelectTranslucencyVolumeTarget(ETranslucencyVolumeCascade InCascade)
{
	if (GUseTranslucencyVolumeBlur)
	{
		switch (InCascade)
		{
		case TVC_Inner:
			{
				return 2;
			}
		case TVC_Outer:
			{
				return 0;
			}
		default:
			{
				// error
				check(false);
				return 0;
			}
		}
	}
	else
	{
		switch (InCascade)
		{
		case TVC_Inner:
			{
				return 0;
			}
		case TVC_Outer:
			{
				return 1;
			}
		default:
			{
				// error
				check(false);
				return 0;
			}
		}
	}
}

/** Number of surfaces used for translucent shadows. */
static const int32 NumTranslucencyShadowSurfaces = 2;

BEGIN_UNIFORM_BUFFER_STRUCT(FGBufferResourceStruct, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D,			GBufferATexture )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D,			GBufferBTexture )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D,			GBufferCTexture )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D,			GBufferDTexture )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D,			GBufferETexture )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D,			GBufferVelocityTexture )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D<float4>,	GBufferATextureNonMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D<float4>,	GBufferBTextureNonMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D<float4>,	GBufferCTextureNonMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D<float4>,	GBufferDTextureNonMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D<float4>,	GBufferETextureNonMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2D<float4>,	GBufferVelocityTextureNonMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2DMS<float4>,	GBufferATextureMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2DMS<float4>,	GBufferBTextureMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2DMS<float4>,	GBufferCTextureMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2DMS<float4>,	GBufferDTextureMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2DMS<float4>,	GBufferETextureMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE( Texture2DMS<float4>,	GBufferVelocityTextureMS )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER( SamplerState,			GBufferATextureSampler )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER( SamplerState,			GBufferBTextureSampler )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER( SamplerState,			GBufferCTextureSampler )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER( SamplerState,			GBufferDTextureSampler )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER( SamplerState,			GBufferETextureSampler )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER( SamplerState,			GBufferVelocityTextureSampler)
END_UNIFORM_BUFFER_STRUCT( FGBufferResourceStruct )

/*
* Stencil layout during basepass / deferred decals:
*		BIT ID    | USE
*		[0]       | sandbox bit (this is not actually output in the base pass, but in the decal passes)
*		[1]       | unallocated
*		[2]       | unallocated
*		[3]       | unallocated
*		[4]       | Lighting channels
*		[5]       | Lighting channels
*		[6]       | Lighting channels
*		[7]       | primitive receive decal bit
*
* After deferred decals, stencil is cleared to 0 and no longer packed in this way, to ensure use of fast hardware clears and HiStencil.
*/
#define STENCIL_SANDBOX_BIT_ID				0
#define STENCIL_LIGHTING_CHANNELS_BIT_ID	4
#define STENCIL_RECEIVE_DECAL_BIT_ID		7

// Outputs a compile-time constant stencil's bit mask ready to be used
// in TStaticDepthStencilState<> template parameter. It also takes care
// of masking the Value macro parameter to only keep the low significant
// bit to ensure to not overflow on other bits.
#define GET_STENCIL_BIT_MASK(BIT_NAME,Value) uint8((uint8(Value) & uint8(0x01)) << (STENCIL_##BIT_NAME##_BIT_ID))

#define STENCIL_SANDBOX_MASK GET_STENCIL_BIT_MASK(SANDBOX,1)

#define STENCIL_LIGHTING_CHANNELS_MASK(Value) uint8((Value & 0x7) << STENCIL_LIGHTING_CHANNELS_BIT_ID)

/**
 * Encapsulates the render targets used for scene rendering.
 */
class RENDERER_API FSceneRenderTargets : public FRenderResource
{
public:
	/** Destructor. */
	virtual ~FSceneRenderTargets() {}

	/** Singletons. At the moment parallel tasks get their snapshot from the rhicmdlist */
	static FSceneRenderTargets& Get(FRHICommandList& RHICmdList);
	static FSceneRenderTargets& Get(FRHICommandListImmediate& RHICmdList);
	static FSceneRenderTargets& Get(FRHIAsyncComputeCommandListImmediate& RHICmdList);

	// this is a placeholder, the context should come from somewhere. This is very unsafe, please don't use it!
	static FSceneRenderTargets& Get_Todo_PassContext();
	// As above but relaxed checks and always gives the global FSceneRenderTargets. The intention here is that it is only used for constants that don't change during a frame. This is very unsafe, please don't use it!
	static FSceneRenderTargets& Get_FrameConstantsOnly();

	/** Create a snapshot on the scene allocator */
	FSceneRenderTargets* CreateSnapshot(const FViewInfo& InView);
	/** Set a snapshot on the TargetCmdList */
	void SetSnapshotOnCmdList(FRHICommandList& TargetCmdList);	
	/** Destruct all snapshots */
	void DestroyAllSnapshots();

	static TAutoConsoleVariable<int32> CVarSetSeperateTranslucencyEnabled;

protected:
	/** Constructor */
	FSceneRenderTargets(): 
		bScreenSpaceAOIsValid(false),
		bCustomDepthIsValid(false),
		GBufferRefCount(0),
		LargestDesiredSizeThisFrame( 0, 0 ),
		LargestDesiredSizeLastFrame( 0, 0 ),
		ThisFrameNumber( 0 ),
		bVelocityPass(false),
		bSeparateTranslucencyPass(false),
		BufferSize(0, 0),
		SeparateTranslucencyBufferSize(0, 0),
		SeparateTranslucencyScale(1),
		SmallColorDepthDownsampleFactor(2),
		bLightAttenuationEnabled(true),
		bUseDownsizedOcclusionQueries(true),
		CurrentGBufferFormat(0),
		CurrentSceneColorFormat(0),
		bAllowStaticLighting(true),
		CurrentMaxShadowResolution(0),
		CurrentRSMResolution(0),
		CurrentTranslucencyLightingVolumeDim(64),
		CurrentMobile32bpp(0),
		bCurrentLightPropagationVolume(false),
		CurrentFeatureLevel(ERHIFeatureLevel::Num),
		CurrentShadingPath(EShadingPath::Num),
		bAllocateVelocityGBuffer(false),
		bSnapshot(false),
		QuadOverdrawIndex(INDEX_NONE)
		{
		}
	/** Constructor that creates snapshot */
	FSceneRenderTargets(const FViewInfo& InView, const FSceneRenderTargets& SnapshotSource);
public:

	/**
	 * Checks that scene render targets are ready for rendering a view family of the given dimensions.
	 * If the allocated render targets are too small, they are reallocated.
	 */
	void Allocate(FRHICommandList& RHICmdList, const FSceneViewFamily& ViewFamily);

	/**
	 *
	 */
	void SetBufferSize(int32 InBufferSizeX, int32 InBufferSizeY);

	void SetSeparateTranslucencyBufferSize(bool bAnyViewWantsDownsampledSeparateTranslucency);

	void BeginRenderingGBuffer(FRHICommandList& RHICmdList, ERenderTargetLoadAction ColorLoadAction, ERenderTargetLoadAction DepthLoadAction, bool bBindQuadOverdrawBuffers, const FLinearColor& ClearColor = FLinearColor(0, 0, 0, 1));
	void FinishRenderingGBuffer(FRHICommandListImmediate& RHICmdList);

	/**
	 * Sets the scene color target and restores its contents if necessary
	 */
	void BeginRenderingSceneColor(FRHICommandList& FRHICommandListImmediate, ESimpleRenderTargetMode RenderTargetMode = ESimpleRenderTargetMode::EUninitializedColorExistingDepth, FExclusiveDepthStencil DepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilWrite, bool bTransitionWritable = true);
	
	/**
	 * Called when finished rendering to the scene color surface
	 */
	void FinishRenderingSceneColor(FRHICommandListImmediate& RHICmdList, const FResolveRect& ResolveRect = FResolveRect());

	// @return true: call FinishRenderingCustomDepth after rendering, false: don't render it, feature is disabled
	bool BeginRenderingCustomDepth(FRHICommandListImmediate& RHICmdList, bool bPrimitives);
	// only call if BeginRenderingCustomDepth() returned true
	void FinishRenderingCustomDepth(FRHICommandListImmediate& RHICmdList, const FResolveRect& ResolveRect = FResolveRect());

	/**
	 * Resolve a previously rendered scene color surface.
	 */
	void ResolveSceneColor(FRHICommandList& RHICmdList, const FResolveRect& ResolveRect = FResolveRect());

	/** Binds the appropriate shadow depth cube map for rendering. */
	void BeginRenderingCubeShadowDepth(FRHICommandList& RHICmdList, int32 ShadowResolution);

	void BeginRenderingTranslucency(FRHICommandList& RHICmdList, const class FViewInfo& View, bool bFirstTimeThisFrame = true);
	void FinishRenderingTranslucency(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View);

	bool BeginRenderingSeparateTranslucency(FRHICommandList& RHICmdList, const FViewInfo& View, bool bFirstTimeThisFrame);
	void FinishRenderingSeparateTranslucency(FRHICommandList& RHICmdList, const FViewInfo& View);
	void FreeSeparateTranslucency()
	{
		SeparateTranslucencyRT.SafeRelease();
		check(!SeparateTranslucencyRT);
	}

	void FreeSeparateTranslucencyDepth()
	{
		if (SeparateTranslucencyDepthRT.GetReference())
		{
			SeparateTranslucencyDepthRT.SafeRelease();
		}
	}

	void ResolveSceneDepthTexture(FRHICommandList& RHICmdList);
	void ResolveSceneDepthToAuxiliaryTexture(FRHICommandList& RHICmdList);

	void BeginRenderingPrePass(FRHICommandList& RHICmdList, bool bPerformClear);
	void FinishRenderingPrePass(FRHICommandListImmediate& RHICmdList);

	void BeginRenderingSceneAlphaCopy(FRHICommandListImmediate& RHICmdList);
	void FinishRenderingSceneAlphaCopy(FRHICommandListImmediate& RHICmdList);

	void BeginRenderingLightAttenuation(FRHICommandList& RHICmdList, bool bClearToWhite = false);
	void FinishRenderingLightAttenuation(FRHICommandList& RHICmdList);

	void GetSeparateTranslucencyDimensions(FIntPoint& OutScaledSize, float& OutScale)
	{
		OutScaledSize = SeparateTranslucencyBufferSize;
		OutScale = SeparateTranslucencyScale;
	}

	TRefCountPtr<IPooledRenderTarget>& GetSeparateTranslucency(FRHICommandList& RHICmdList, FIntPoint Size);

	bool IsSeparateTranslucencyDepthValid()
	{
		return SeparateTranslucencyDepthRT != nullptr;
	}

	TRefCountPtr<IPooledRenderTarget>& GetSeparateTranslucencyDepth(FRHICommandList& RHICmdList, FIntPoint Size);

	const FTexture2DRHIRef& GetSeparateTranslucencyDepthSurface()
	{
		return (const FTexture2DRHIRef&)SeparateTranslucencyDepthRT->GetRenderTargetItem().TargetableTexture;
	}

	/**
	 * Cleans up editor primitive targets that we no longer need
	 */
	void CleanUpEditorPrimitiveTargets();

	/**
	 * Affects the render quality of the editor 3d objects. MSAA is needed if >1
	 * @return clamped to reasonable numbers
	 */
	int32 GetEditorMSAACompositingSampleCount() const;

	bool IsStaticLightingAllowed() const { return bAllowStaticLighting; }

	/**
	 * Gets the editor primitives color target/shader resource.  This may recreate the target
	 * if the msaa settings dont match
	 */
	const FTexture2DRHIRef& GetEditorPrimitivesColor(FRHICommandList& RHICmdList);

	/**
	 * Gets the editor primitives depth target/shader resource.  This may recreate the target
	 * if the msaa settings dont match
	 */
	const FTexture2DRHIRef& GetEditorPrimitivesDepth(FRHICommandList& RHICmdList);


	// FRenderResource interface.
	virtual void ReleaseDynamicRHI() override;

	// Texture Accessors -----------

	const FTextureRHIRef& GetSceneColorTexture() const;
	const FTexture2DRHIRef& GetSceneAlphaCopyTexture() const { return (const FTexture2DRHIRef&)SceneAlphaCopy->GetRenderTargetItem().ShaderResourceTexture; }
	bool HasSceneAlphaCopyTexture() const { return SceneAlphaCopy.GetReference() != 0; }
	const FTexture2DRHIRef& GetSceneDepthTexture() const { return (const FTexture2DRHIRef&)SceneDepthZ->GetRenderTargetItem().ShaderResourceTexture; }
	const FTexture2DRHIRef& GetAuxiliarySceneDepthTexture() const
	{ 
		check(!GSupportsDepthFetchDuringDepthTest);
		return (const FTexture2DRHIRef&)AuxiliarySceneDepthZ->GetRenderTargetItem().ShaderResourceTexture; 
	}

	const FTexture2DRHIRef* GetActualDepthTexture() const;
	const FTexture2DRHIRef& GetGBufferATexture() const { return (const FTexture2DRHIRef&)GBufferA->GetRenderTargetItem().ShaderResourceTexture; }
	const FTexture2DRHIRef& GetGBufferBTexture() const { return (const FTexture2DRHIRef&)GBufferB->GetRenderTargetItem().ShaderResourceTexture; }
	const FTexture2DRHIRef& GetGBufferCTexture() const { return (const FTexture2DRHIRef&)GBufferC->GetRenderTargetItem().ShaderResourceTexture; }
	const FTexture2DRHIRef& GetGBufferDTexture() const { return (const FTexture2DRHIRef&)GBufferD->GetRenderTargetItem().ShaderResourceTexture; }
	const FTexture2DRHIRef& GetGBufferETexture() const { return (const FTexture2DRHIRef&)GBufferE->GetRenderTargetItem().ShaderResourceTexture; }
	const FTexture2DRHIRef& GetGBufferVelocityTexture() const { return (const FTexture2DRHIRef&)GBufferVelocity->GetRenderTargetItem().ShaderResourceTexture; }

	/** 
	* Allows substitution of a 1x1 white texture in place of the light attenuation buffer when it is not needed;
	* this improves shader performance and removes the need for redundant Clears
	*/
	void SetLightAttenuationMode(bool bEnabled) { bLightAttenuationEnabled = bEnabled; }
	const FTextureRHIRef& GetEffectiveLightAttenuationTexture(bool bReceiveDynamicShadows) const 
	{
		if( bLightAttenuationEnabled && bReceiveDynamicShadows )
		{
			return GetLightAttenuationTexture();
		}
		else
		{
			return GWhiteTexture->TextureRHI;
		}
	}
	const FTextureRHIRef& GetLightAttenuationTexture() const
	{
		return *(FTextureRHIRef*)&GetLightAttenuation()->GetRenderTargetItem().ShaderResourceTexture;
	}

	const FTextureRHIRef& GetSceneColorSurface() const;
	const FTexture2DRHIRef& GetSceneAlphaCopySurface() const						{ return (const FTexture2DRHIRef&)SceneAlphaCopy->GetRenderTargetItem().TargetableTexture; }
	const FTexture2DRHIRef& GetSceneDepthSurface() const							{ return (const FTexture2DRHIRef&)SceneDepthZ->GetRenderTargetItem().TargetableTexture; }
	const FTexture2DRHIRef& GetSmallDepthSurface() const							{ return (const FTexture2DRHIRef&)SmallDepthZ->GetRenderTargetItem().TargetableTexture; }
	const FTexture2DRHIRef& GetOptionalShadowDepthColorSurface(FRHICommandList& RHICmdList, int32 Width, int32 Height) const;
	const FTexture2DRHIRef& GetLightAttenuationSurface() const					{ return (const FTexture2DRHIRef&)GetLightAttenuation()->GetRenderTargetItem().TargetableTexture; }
	const FTexture2DRHIRef& GetAuxiliarySceneDepthSurface() const 
	{	
		check(!GSupportsDepthFetchDuringDepthTest); 
		return (const FTexture2DRHIRef&)AuxiliarySceneDepthZ->GetRenderTargetItem().TargetableTexture; 
	}

	const FTexture2DRHIRef& GetDirectionalOcclusionTexture() const 
	{	
		return (const FTexture2DRHIRef&)DirectionalOcclusion->GetRenderTargetItem().TargetableTexture; 
	}

	IPooledRenderTarget* GetGBufferVelocityRT();

	int32 GetQuadOverdrawIndex() const { return QuadOverdrawIndex; }

	// @return can be 0 if the feature is disabled
	IPooledRenderTarget* RequestCustomDepth(FRHICommandListImmediate& RHICmdList, bool bPrimitives);

	bool IsCustomDepthPassWritingStencil() const;

	// ---

	/** */
	bool UseDownsizedOcclusionQueries() const { return bUseDownsizedOcclusionQueries; }

	// ---

	/** Get the current translucent ambient lighting volume texture. Can vary depending on whether volume filtering is enabled */
	IPooledRenderTarget* GetTranslucencyVolumeAmbient(ETranslucencyVolumeCascade Cascade) { return TranslucencyLightingVolumeAmbient[SelectTranslucencyVolumeTarget(Cascade)].GetReference(); }

	/** Get the current translucent directional lighting volume texture. Can vary depending on whether volume filtering is enabled */
	IPooledRenderTarget* GetTranslucencyVolumeDirectional(ETranslucencyVolumeCascade Cascade) { return TranslucencyLightingVolumeDirectional[SelectTranslucencyVolumeTarget(Cascade)].GetReference(); }

	// ---
	/** Get the uniform buffer containing GBuffer resources. */
	FUniformBufferRHIParamRef GetGBufferResourcesUniformBuffer() const 
	{ 
		// if this triggers you need to make sure the GBuffer is not getting released before (using AdjustGBufferRefCount(1) and AdjustGBufferRefCount(-1))
		// Maybe You use a SceneTexture material expression that should set MaterialCompilationOutput.bNeedsGBuffer
		check(IsValidRef(GBufferResourcesUniformBuffer));

		return GBufferResourcesUniformBuffer; 
	}
	/** Returns the size of most screen space render targets e.g. SceneColor, SceneDepth, GBuffer, ... might be different from final RT or output Size because of ScreenPercentage use. */
	FIntPoint GetBufferSizeXY() const { return BufferSize; }
	/** */
	uint32 GetSmallColorDepthDownsampleFactor() const { return SmallColorDepthDownsampleFactor; }
	/** Returns an index in the range [0, NumCubeShadowDepthSurfaces) given an input resolution. */
	int32 GetCubeShadowDepthZIndex(int32 ShadowResolution) const;
	/** Returns the appropriate resolution for a given cube shadow index. */
	int32 GetCubeShadowDepthZResolution(int32 ShadowIndex) const;
	/** Returns the size of the shadow depth buffer, taking into account platform limitations and game specific resolution limits. */
	FIntPoint GetShadowDepthTextureResolution() const;
	// @return >= 1x1 <= GMaxShadowDepthBufferSizeX x GMaxShadowDepthBufferSizeY
	FIntPoint GetPreShadowCacheTextureResolution() const;
	FIntPoint GetTranslucentShadowDepthTextureResolution() const;
	int32 GetTranslucentShadowDownsampleFactor() const { return 2; }

	/** Returns the size of the RSM buffer, taking into account platform limitations and game specific resolution limits. */
	int32 GetReflectiveShadowMapResolution() const;

	int32 GetNumGBufferTargets() const;

	// ---

	// needs to be called between AllocSceneColor() and ReleaseSceneColor()
	const TRefCountPtr<IPooledRenderTarget>& GetSceneColor() const;

	TRefCountPtr<IPooledRenderTarget>& GetSceneColor();

	EPixelFormat GetSceneColorFormat() const;

	// changes depending at which part of the frame this is called
	bool IsSceneColorAllocated() const;

	void SetSceneColor(IPooledRenderTarget* In);

	// ---

	void SetLightAttenuation(IPooledRenderTarget* In);

	// needs to be called between AllocSceneColor() and SetSceneColor(0)
	const TRefCountPtr<IPooledRenderTarget>& GetLightAttenuation() const;

	TRefCountPtr<IPooledRenderTarget>& GetLightAttenuation();

	// ---

	// allows to release the GBuffer once post process materials no longer need it
	// @param 1: add a reference, -1: remove a reference
	void AdjustGBufferRefCount(FRHICommandList& RHICmdList, int Delta);

	//
	void PreallocGBufferTargets(bool bShouldRenderVelocities);
	void GetGBufferADesc(FPooledRenderTargetDesc& Desc) const;
	void AllocGBufferTargets(FRHICommandList& RHICmdList);

	void AllocLightAttenuation(FRHICommandList& RHICmdList);

	void AllocateReflectionTargets(FRHICommandList& RHICmdList, int32 TargetSize);

	void AllocateLightingChannelTexture(FRHICommandList& RHICmdList);

	void AllocateDebugViewModeTargets(FRHICommandList& RHICmdList);

	TRefCountPtr<IPooledRenderTarget>& GetReflectionBrightnessTarget();

	/**
	 * Takes the requested buffer size and quantizes it to an appropriate size for the rest of the
	 * rendering pipeline. Currently ensures that sizes are multiples of 8 so that they can safely
	 * be halved in size several times.
	 */
	static void QuantizeBufferSize(int32& InOutBufferSizeX, int32& InOutBufferSizeY);

	bool IsSeparateTranslucencyActive(const FViewInfo& View) const;

	bool IsSeparateTranslucencyPass()
	{
		return bSeparateTranslucencyPass;
	}
	
	// Can be called when the Scene Color content is no longer needed. As we create SceneColor on demand we can make sure it is created with the right format.
	// (as a call to SetSceneColor() can override it with a different format)
	void ReleaseSceneColor();
	
	ERHIFeatureLevel::Type GetCurrentFeatureLevel() const { return CurrentFeatureLevel; }

private: // Get...() methods instead of direct access

	// 0 before BeginRenderingSceneColor and after tone mapping in deferred shading
	// Permanently allocated for forward shading
	TRefCountPtr<IPooledRenderTarget> SceneColor[(int32)EShadingPath::Num];
	// Light Attenuation is a low precision scratch pad matching the size of the scene color buffer used by many passes.
	TRefCountPtr<IPooledRenderTarget> LightAttenuation;
public:
	// Light Accumulation is a high precision scratch pad matching the size of the scene color buffer used by many passes.
	TRefCountPtr<IPooledRenderTarget> LightAccumulation;

	// Reflection Environment: Bringing back light accumulation buffer to apply indirect reflections
	TRefCountPtr<IPooledRenderTarget> DirectionalOcclusion;
	//
	TRefCountPtr<IPooledRenderTarget> SceneDepthZ;
	TRefCountPtr<FRHIShaderResourceView> SceneStencilSRV;
	TRefCountPtr<IPooledRenderTarget> LightingChannels;
	// Mobile without frame buffer fetch (to get depth from alpha).
	TRefCountPtr<IPooledRenderTarget> SceneAlphaCopy;
	// Auxiliary scene depth target. The scene depth is resolved to this surface when targeting SM4. 
	TRefCountPtr<IPooledRenderTarget> AuxiliarySceneDepthZ;
	// Quarter-sized version of the scene depths
	TRefCountPtr<IPooledRenderTarget> SmallDepthZ;

	// GBuffer: Geometry Buffer rendered in base pass for deferred shading, only available between AllocGBufferTargets() and FreeGBufferTargets()
	TRefCountPtr<IPooledRenderTarget> GBufferA;
	TRefCountPtr<IPooledRenderTarget> GBufferB;
	TRefCountPtr<IPooledRenderTarget> GBufferC;
	TRefCountPtr<IPooledRenderTarget> GBufferD;
	TRefCountPtr<IPooledRenderTarget> GBufferE;

	TRefCountPtr<IPooledRenderTarget> GBufferVelocity;

	// DBuffer: For decals before base pass (only temporarily available after early z pass and until base pass)
	TRefCountPtr<IPooledRenderTarget> DBufferA;
	TRefCountPtr<IPooledRenderTarget> DBufferB;
	TRefCountPtr<IPooledRenderTarget> DBufferC;
	TRefCountPtr<IPooledRenderTarget> DBufferMask;

	// for AmbientOcclusion, only valid for a short time during the frame to allow reuse
	TRefCountPtr<IPooledRenderTarget> ScreenSpaceAO;
	// for shader/quad complexity, the temporary quad descriptors and complexity.
	TRefCountPtr<IPooledRenderTarget> QuadOverdrawBuffer;
	// used by the CustomDepth material feature, is allocated on demand or if r.CustomDepth is 2
	TRefCountPtr<IPooledRenderTarget> CustomDepth;
	// used by the CustomDepth material feature for stencil
	TRefCountPtr<FRHIShaderResourceView> CustomStencilSRV;
	// optional in case this RHI requires a color render target (adjust up if necessary)
	TRefCountPtr<IPooledRenderTarget> OptionalShadowDepthColor[4];

	/** 2 scratch cubemaps used for filtering reflections. */
	TRefCountPtr<IPooledRenderTarget> ReflectionColorScratchCubemap[2];

	/** Temporary storage during SH irradiance map generation. */
	TRefCountPtr<IPooledRenderTarget> DiffuseIrradianceScratchCubemap[2];

	/** Temporary storage during SH irradiance map generation. */
	TRefCountPtr<IPooledRenderTarget> SkySHIrradianceMap;

	/** Volume textures used for lighting translucency. */
	TRefCountPtr<IPooledRenderTarget> TranslucencyLightingVolumeAmbient[NumTranslucentVolumeRenderTargetSets];
	TRefCountPtr<IPooledRenderTarget> TranslucencyLightingVolumeDirectional[NumTranslucentVolumeRenderTargetSets];

	/** Color and opacity for editor primitives (i.e editor gizmos). */
	TRefCountPtr<IPooledRenderTarget> EditorPrimitivesColor;

	/** Depth for editor primitives */
	TRefCountPtr<IPooledRenderTarget> EditorPrimitivesDepth;

	/** ONLY for snapshots!!! this is a copy of the SeparateTranslucencyRT from the view state. */
	TRefCountPtr<IPooledRenderTarget> SeparateTranslucencyRT;
	TRefCountPtr<IPooledRenderTarget> SeparateTranslucencyDepthRT;

	// todo: free ScreenSpaceAO so pool can reuse
	bool bScreenSpaceAOIsValid;

	// todo: free ScreenSpaceAO so pool can reuse
	bool bCustomDepthIsValid;

private:
	/** used by AdjustGBufferRefCount */
	int32 GBufferRefCount;

	/** as we might get multiple BufferSize requests each frame for SceneCaptures and we want to avoid reallocations we can only go as low as the largest request */
	FIntPoint LargestDesiredSizeThisFrame;
	FIntPoint LargestDesiredSizeLastFrame;
	/** to detect when LargestDesiredSizeThisFrame is outdated */
	uint32 ThisFrameNumber;

	bool bVelocityPass;
	bool bSeparateTranslucencyPass;
	/** CAUTION: When adding new data, make sure you copy it in the snapshot constructor! **/

	/**
	 * Initializes the editor primitive color render target
	 */
	void InitEditorPrimitivesColor(FRHICommandList& RHICmdList);

	/**
	 * Initializes the editor primitive depth buffer
	 */
	void InitEditorPrimitivesDepth(FRHICommandList& RHICmdList);

	/** Allocates render targets for use with the mobile path. */
	void AllocateMobileRenderTargets(FRHICommandList& RHICmdList);

public:
	/** Allocates render targets for use with the deferred shading path. */
	// Temporarily Public to call from DefferedShaderRenderer to attempt recovery from a crash until cause is found.
	void AllocateDeferredShadingPathRenderTargets(FRHICommandList& RHICmdList);
private:

	/** Allocates render targets for use with the current shading path. */
	void AllocateRenderTargets(FRHICommandList& RHICmdList);

	/** Allocates common depth render targets that are used by both mobile and deferred rendering paths */
	void AllocateCommonDepthTargets(FRHICommandList& RHICmdList);

	/** Determine the appropriate render target dimensions. */
	FIntPoint ComputeDesiredSize(const FSceneViewFamily& ViewFamily);

	void AllocSceneColor(FRHICommandList& RHICmdList);

	// internal method, used by AdjustGBufferRefCount()
	void ReleaseGBufferTargets();

	// release all allocated targets to the pool
	void ReleaseAllTargets();

	/** Get the current scene color target based on our current shading path. Will return a null ptr if there is no valid scene color target  */
	const TRefCountPtr<IPooledRenderTarget>& GetSceneColorForCurrentShadingPath() const { check(CurrentShadingPath < EShadingPath::Num); return SceneColor[(int32)CurrentShadingPath]; }
	TRefCountPtr<IPooledRenderTarget>& GetSceneColorForCurrentShadingPath() { check(CurrentShadingPath < EShadingPath::Num); return SceneColor[(int32)CurrentShadingPath]; }

	/** Determine whether the render targets for a particular shading path have been allocated */
	bool AreShadingPathRenderTargetsAllocated(EShadingPath InShadingPath) const;

	/** Determine whether the render targets for any shading path have been allocated */
	bool AreAnyShadingPathRenderTargetsAllocated() const 
	{ 
		return AreShadingPathRenderTargetsAllocated(EShadingPath::Deferred) 
			|| AreShadingPathRenderTargetsAllocated(EShadingPath::Mobile); 
	}

	/** Gets all GBuffers to use.  Returns the number actually used. */
	int32 GetGBufferRenderTargets(ERenderTargetLoadAction ColorLoadAction, FRHIRenderTargetView OutRenderTargets[MaxSimultaneousRenderTargets], int32& OutVelocityRTIndex);

	/** Uniform buffer containing GBuffer resources. */
	FUniformBufferRHIRef GBufferResourcesUniformBuffer;
	/** size of the back buffer, in editor this has to be >= than the biggest view port */
	FIntPoint BufferSize;
	FIntPoint SeparateTranslucencyBufferSize;
	float SeparateTranslucencyScale;
	/** e.g. 2 */
	uint32 SmallColorDepthDownsampleFactor;
	/** if true we use the light attenuation buffer otherwise the 1x1 white texture is used */
	bool bLightAttenuationEnabled;
	/** Whether to use SmallDepthZ for occlusion queries. */
	bool bUseDownsizedOcclusionQueries;
	/** To detect a change of the CVar r.GBufferFormat */
	int32 CurrentGBufferFormat;
	/** To detect a change of the CVar r.SceneColorFormat */
	int32 CurrentSceneColorFormat;
	/** Whether render targets were allocated with static lighting allowed. */
	bool bAllowStaticLighting;
	/** To detect a change of the CVar r.Shadow.MaxResolution */
	int32 CurrentMaxShadowResolution;
	/** To detect a change of the CVar r.Shadow.RsmResolution*/
	int32 CurrentRSMResolution;
	/** To detect a change of the CVar r.TranslucencyLightingVolumeDim */
	int32 CurrentTranslucencyLightingVolumeDim;
	/** To detect a change of the CVar r.MobileHDR / r.MobileHDR32bppMode */
	int32 CurrentMobile32bpp;
	/** To detect a change of the CVar r.MobileMSAA or r.MSAA */
	int32 CurrentMSAACount;
	/** To detect a change of the CVar r.Shadow.MinResolution */
	int32 CurrentMinShadowResolution;
	/** To detect a change of the CVar r.LightPropagationVolume */
	bool bCurrentLightPropagationVolume;
	/** Feature level we were initialized for */
	ERHIFeatureLevel::Type CurrentFeatureLevel;
	/** Shading path that we are currently drawing through. Set when calling Allocate at the start of a scene render. */
	EShadingPath CurrentShadingPath;

	// Set this per frame since there might be cases where we don't need an extra GBuffer
	bool bAllocateVelocityGBuffer;

	/** Helpers to track gbuffer state on platforms that need to propagate clear information across parallel rendering boundaries. */
	bool bGBuffersFastCleared;	

	/** Helpers to track scenedepth state on platforms that need to propagate clear information across parallel rendering boundaries. */
	bool bSceneDepthCleared;

	/** true is this is a snapshot on the scene allocator */
	bool bSnapshot;

	/** Helpers to track the bound index of the quad overdraw UAV. Needed because UAVs overlap RTs in DX11 */
	int32 QuadOverdrawIndex;

	/** All outstanding snapshots */
	TArray<FSceneRenderTargets*> Snapshots;

	/** CAUTION: When adding new data, make sure you copy it in the snapshot constructor! **/

};

