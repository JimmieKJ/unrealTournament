// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRenderTargets.cpp: Scene render target implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "ReflectionEnvironment.h"
#include "LightPropagationVolume.h"
#include "SceneUtils.h"
#include "HdrCustomResolveShaders.h"
#include "Public/LightPropagationVolumeBlendable.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FGBufferResourceStruct,TEXT("GBuffers"));

static TAutoConsoleVariable<int32> CVarBasePassOutputsVelocityDebug(
	TEXT("r.BasePassOutputsVelocityDebug"),
	0,
	TEXT("Debug settings for Base Pass outputting velocity.\n")
	TEXT("0 - Regular rendering\n")
	TEXT("1 - Skip setting GBufferVelocity RT\n")
	TEXT("2 - Set Color Mask 0 for GBufferVelocity RT"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRSMResolution(
	TEXT("r.LPV.RSMResolution"),
	360,
	TEXT("Reflective Shadow Map resolution (used for LPV) - higher values result in less aliasing artifacts, at the cost of performance"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static int32 GBasePassOutputsVelocityDebug = 0;

/*-----------------------------------------------------------------------------
FSceneRenderTargets
-----------------------------------------------------------------------------*/

int32 GDownsampledOcclusionQueries = 0;
static FAutoConsoleVariableRef CVarDownsampledOcclusionQueries(
	TEXT("r.DownsampledOcclusionQueries"),
	GDownsampledOcclusionQueries,
	TEXT("Whether to issue occlusion queries to a downsampled depth buffer"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarSceneTargetsResizingMethod(
	TEXT("r.SceneRenderTargetResizeMethod"),
	0,
	TEXT("Control the scene render target resize method:\n")
	TEXT("(This value is only used in game mode and on windowing platforms.)\n")
	TEXT("0: Resize to match requested render size (Default) (Least memory use, can cause stalls when size changes e.g. ScreenPercentage)\n")
	TEXT("1: Fixed to screen resolution.\n")
	TEXT("2: Expands to encompass the largest requested render dimension. (Most memory use, least prone to allocation stalls.)"),	
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarOptimizeForUAVPerformance(
	TEXT("r.OptimizeForUAVPerformance"),
	0,
	TEXT("Allows to profile if hardware has a performance cost due to render target reuse (more info: search for GCNPerformanceTweets.pdf Tip 37)\n")
	TEXT("If we see a noticeable difference on some hardware we can add another option like -1 (meaning auto) and make it the new default.\n")
	TEXT("0: Optimize for GPU memory savings and reuse render targets (default)\n")
	TEXT("1: Optimize for GPU performance (might render faster but can require more GPU memory)"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarCustomDepth(
	TEXT("r.CustomDepth"),
	1,
	TEXT("0: feature is disabled\n")
	TEXT("1: feature is enabled, texture is created on demand\n")
	TEXT("2: feature is enabled, texture is not released until required (should be the project setting if the feature should not stall)\n")
	TEXT("3: feature is enabled, stencil writes are enabled, texture is not released until required (should be the project setting if the feature should not stall)"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarMobileMSAA(
	TEXT("r.MobileMSAA"),
	0,
	TEXT("Use MSAA instead of Temporal AA on mobile:\n")
	TEXT("1: Use Temporal AA (MSAA disabled)\n")
	TEXT("2: Use 2x MSAA (Temporal AA disabled)\n")
	TEXT("4: Use 4x MSAA (Temporal AA disabled)"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarGBufferFormat(
	TEXT("r.GBufferFormat"),
	1,
	TEXT("Defines the memory layout used for the GBuffer.\n")
	TEXT("(affects performance, mostly through bandwidth, quality of normals and material attributes).\n")
	TEXT(" 0: lower precision (8bit per component, for profiling)\n")
	TEXT(" 1: low precision (default)\n")
	TEXT(" 5: high precision"),
	ECVF_RenderThreadSafe);

/** The global render targets used for scene rendering. */
static TGlobalResource<FSceneRenderTargets> SceneRenderTargetsSingleton;

FSceneRenderTargets& FSceneRenderTargets::Get(FRHICommandList& RHICmdList)
{
	FSceneRenderTargets* SceneContext = (FSceneRenderTargets*)RHICmdList.GetRenderThreadContext(FRHICommandListBase::ERenderThreadContext::SceneRenderTargets);
	if (!SceneContext)
	{
		return SceneRenderTargetsSingleton;
	}
	check(!RHICmdList.IsImmediate());
	return *SceneContext;
}

FSceneRenderTargets& FSceneRenderTargets::Get(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread() && !RHICmdList.GetRenderThreadContext(FRHICommandListBase::ERenderThreadContext::SceneRenderTargets)
		&& !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local)); // if we are processing tasks on the local queue, it is assumed this are in support of async tasks, which cannot use the current state of the render targets. This can be relaxed if needed.
	return SceneRenderTargetsSingleton;
}

FSceneRenderTargets& FSceneRenderTargets::Get(FRHIAsyncComputeCommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread() && !RHICmdList.GetRenderThreadContext(FRHICommandListBase::ERenderThreadContext::SceneRenderTargets)
		&& !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local)); // if we are processing tasks on the local queue, it is assumed this are in support of async tasks, which cannot use the current state of the render targets. This can be relaxed if needed.
	return SceneRenderTargetsSingleton;
}

FSceneRenderTargets& FSceneRenderTargets::Get_Todo_PassContext()
{
	check(IsInRenderingThread()
		&& !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local)); // if we are processing tasks on the local queue, it is assumed this are in support of async tasks, which cannot use the current state of the render targets. This can be relaxed if needed.
	return SceneRenderTargetsSingleton;
}

FSceneRenderTargets& FSceneRenderTargets::Get_FrameConstantsOnly()
{
	return SceneRenderTargetsSingleton;
}

 FSceneRenderTargets* FSceneRenderTargets::CreateSnapshot(const FViewInfo& InView)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderTargets_CreateSnapshot);
	check(IsInRenderingThread() && FMemStack::Get().GetNumMarks() == 1); // we do not want this popped before the end of the scene and it better be the scene allocator
	FSceneRenderTargets* NewSnapshot = new (FMemStack::Get()) FSceneRenderTargets(InView, *this);
	check(NewSnapshot->bSnapshot);
	Snapshots.Add(NewSnapshot);
	return NewSnapshot;
}

void FSceneRenderTargets::SetSnapshotOnCmdList(FRHICommandList& TargetCmdList)
{
	check(bSnapshot);
	TargetCmdList.SetRenderThreadContext(this, FRHICommandListBase::ERenderThreadContext::SceneRenderTargets);
}

void FSceneRenderTargets::DestroyAllSnapshots()
{
	if (Snapshots.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderTargets_DestroyAllSnapshots);
		check(IsInRenderingThread());
		for (auto Snapshot : Snapshots)
		{
			Snapshot->~FSceneRenderTargets();
		}
		Snapshots.Reset();
		GRenderTargetPool.DestructSnapshots();
	}
}



template <size_t N>
static void SnapshotArray(TRefCountPtr<IPooledRenderTarget> (&Dest)[N], const TRefCountPtr<IPooledRenderTarget> (&Src)[N])
{
	for (int32 Index = 0; Index < N; Index++)
	{
		Dest[Index] = GRenderTargetPool.MakeSnapshot(Src[Index]);
	}
}

FSceneRenderTargets::FSceneRenderTargets(const FViewInfo& View, const FSceneRenderTargets& SnapshotSource)
	: LightAttenuation(GRenderTargetPool.MakeSnapshot(SnapshotSource.LightAttenuation))
	, LightAccumulation(GRenderTargetPool.MakeSnapshot(SnapshotSource.LightAccumulation))
	, DirectionalOcclusion(GRenderTargetPool.MakeSnapshot(SnapshotSource.DirectionalOcclusion))
	, SceneDepthZ(GRenderTargetPool.MakeSnapshot(SnapshotSource.SceneDepthZ))
	, LightingChannels(GRenderTargetPool.MakeSnapshot(SnapshotSource.LightingChannels))
	, NoMSAASceneDepthZ(GRenderTargetPool.MakeSnapshot(SnapshotSource.NoMSAASceneDepthZ))
	, SceneAlphaCopy(GRenderTargetPool.MakeSnapshot(SnapshotSource.SceneAlphaCopy))
	, AuxiliarySceneDepthZ(GRenderTargetPool.MakeSnapshot(SnapshotSource.AuxiliarySceneDepthZ))
	, SmallDepthZ(GRenderTargetPool.MakeSnapshot(SnapshotSource.SmallDepthZ))
	, GBufferA(GRenderTargetPool.MakeSnapshot(SnapshotSource.GBufferA))
	, GBufferB(GRenderTargetPool.MakeSnapshot(SnapshotSource.GBufferB))
	, GBufferC(GRenderTargetPool.MakeSnapshot(SnapshotSource.GBufferC))
	, GBufferD(GRenderTargetPool.MakeSnapshot(SnapshotSource.GBufferD))
	, GBufferE(GRenderTargetPool.MakeSnapshot(SnapshotSource.GBufferE))
	, GBufferVelocity(GRenderTargetPool.MakeSnapshot(SnapshotSource.GBufferVelocity))
	, DBufferA(GRenderTargetPool.MakeSnapshot(SnapshotSource.DBufferA))
	, DBufferB(GRenderTargetPool.MakeSnapshot(SnapshotSource.DBufferB))
	, DBufferC(GRenderTargetPool.MakeSnapshot(SnapshotSource.DBufferC))
	, ScreenSpaceAO(GRenderTargetPool.MakeSnapshot(SnapshotSource.ScreenSpaceAO))
	, QuadOverdrawBuffer(GRenderTargetPool.MakeSnapshot(SnapshotSource.QuadOverdrawBuffer))
	, CustomDepth(GRenderTargetPool.MakeSnapshot(SnapshotSource.CustomDepth))
	, CustomStencilSRV(SnapshotSource.CustomStencilSRV)
	, ShadowDepthZ(GRenderTargetPool.MakeSnapshot(SnapshotSource.ShadowDepthZ))
	, OptionalShadowDepthColor(GRenderTargetPool.MakeSnapshot(SnapshotSource.OptionalShadowDepthColor))
	, PreShadowCacheDepthZ(GRenderTargetPool.MakeSnapshot(SnapshotSource.PreShadowCacheDepthZ))
	, ReflectiveShadowMapNormal(GRenderTargetPool.MakeSnapshot(SnapshotSource.ReflectiveShadowMapNormal))
	, ReflectiveShadowMapDiffuse(GRenderTargetPool.MakeSnapshot(SnapshotSource.ReflectiveShadowMapDiffuse))
	, ReflectiveShadowMapDepth(GRenderTargetPool.MakeSnapshot(SnapshotSource.ReflectiveShadowMapDepth))
	, SkySHIrradianceMap(GRenderTargetPool.MakeSnapshot(SnapshotSource.SkySHIrradianceMap))
	, EditorPrimitivesColor(GRenderTargetPool.MakeSnapshot(SnapshotSource.EditorPrimitivesColor))
	, EditorPrimitivesDepth(GRenderTargetPool.MakeSnapshot(SnapshotSource.EditorPrimitivesDepth))
	, SeparateTranslucencyRT(SnapshotSource.SeparateTranslucencyRT)
	, SeparateTranslucencyDepthRT(SnapshotSource.SeparateTranslucencyDepthRT)
	, bScreenSpaceAOIsValid(SnapshotSource.bScreenSpaceAOIsValid)
	, bCustomDepthIsValid(SnapshotSource.bCustomDepthIsValid)
	, bPreshadowCacheNewlyAllocated(SnapshotSource.bPreshadowCacheNewlyAllocated)
	, GBufferRefCount(SnapshotSource.GBufferRefCount)
	, LargestDesiredSizeThisFrame(SnapshotSource.LargestDesiredSizeThisFrame)
	, LargestDesiredSizeLastFrame(SnapshotSource.LargestDesiredSizeLastFrame)
	, ThisFrameNumber(SnapshotSource.ThisFrameNumber)
	, bVelocityPass(SnapshotSource.bVelocityPass)
	, bSeparateTranslucencyPass(SnapshotSource.bSeparateTranslucencyPass)
	, GBufferResourcesUniformBuffer(SnapshotSource.GBufferResourcesUniformBuffer)
	, BufferSize(SnapshotSource.BufferSize)
	, SeparateTranslucencyBufferSize(SnapshotSource.SeparateTranslucencyBufferSize)
	, SeparateTranslucencyScale(SnapshotSource.SeparateTranslucencyScale)
	, SmallColorDepthDownsampleFactor(SnapshotSource.SmallColorDepthDownsampleFactor)
	, bLightAttenuationEnabled(SnapshotSource.bLightAttenuationEnabled)
	, bUseDownsizedOcclusionQueries(SnapshotSource.bUseDownsizedOcclusionQueries)
	, CurrentGBufferFormat(SnapshotSource.CurrentGBufferFormat)
	, CurrentSceneColorFormat(SnapshotSource.CurrentSceneColorFormat)
	, bAllowStaticLighting(SnapshotSource.bAllowStaticLighting)
	, CurrentMaxShadowResolution(SnapshotSource.CurrentMaxShadowResolution)
	, CurrentRSMResolution(SnapshotSource.CurrentRSMResolution)
	, CurrentTranslucencyLightingVolumeDim(SnapshotSource.CurrentTranslucencyLightingVolumeDim)
	, CurrentMobile32bpp(SnapshotSource.CurrentMobile32bpp)
	, CurrentMobileMSAA(SnapshotSource.CurrentMobileMSAA)
	, CurrentMinShadowResolution(SnapshotSource.CurrentMinShadowResolution)
	, bCurrentLightPropagationVolume(SnapshotSource.bCurrentLightPropagationVolume)
	, CurrentFeatureLevel(SnapshotSource.CurrentFeatureLevel)
	, CurrentShadingPath(SnapshotSource.CurrentShadingPath)
	, bAllocateVelocityGBuffer(SnapshotSource.bAllocateVelocityGBuffer)
	, bGBuffersFastCleared(SnapshotSource.bGBuffersFastCleared)	
	, bSceneDepthCleared(SnapshotSource.bSceneDepthCleared)	
	, bSnapshot(true)
	, QuadOverdrawIndex(SnapshotSource.QuadOverdrawIndex)
{
	SnapshotArray(SceneColor, SnapshotSource.SceneColor);
	SnapshotArray(TranslucencyShadowTransmission, SnapshotSource.TranslucencyShadowTransmission);
	SnapshotArray(CubeShadowDepthZ, SnapshotSource.CubeShadowDepthZ);
	SnapshotArray(ReflectionColorScratchCubemap, SnapshotSource.ReflectionColorScratchCubemap);
	SnapshotArray(DiffuseIrradianceScratchCubemap, SnapshotSource.DiffuseIrradianceScratchCubemap);
	SnapshotArray(ReflectionBrightness, SnapshotSource.ReflectionBrightness);
	SnapshotArray(TranslucencyLightingVolumeAmbient, SnapshotSource.TranslucencyLightingVolumeAmbient);
	SnapshotArray(TranslucencyLightingVolumeDirectional, SnapshotSource.TranslucencyLightingVolumeDirectional);
}


inline const TCHAR* GetReflectionBrightnessTargetName(uint32 Index)
{
	const TCHAR* Names[2] = 
	{ 
		TEXT("ReflectionBrightness0"), 
		TEXT("ReflectionBrightness1") 
	};
	check(Index < sizeof(Names));
	return Names[Index];
}

inline const TCHAR* GetSceneColorTargetName(FSceneRenderTargets::EShadingPath ShadingPath)
{
	const TCHAR* SceneColorNames[(uint32)FSceneRenderTargets::EShadingPath::Num] =
	{ 
		TEXT("SceneColorForward"), 
		TEXT("SceneColorDeferred") 
	};
	check((uint32)ShadingPath < sizeof(SceneColorNames));
	return SceneColorNames[(uint32)ShadingPath];
}

FIntPoint FSceneRenderTargets::ComputeDesiredSize(const FSceneViewFamily& ViewFamily)
{
	enum ESizingMethods { RequestedSize, ScreenRes, Grow, VisibleSizingMethodsCount };
	ESizingMethods SceneTargetsSizingMethod = Grow;

	bool bIsSceneCapture = false;
	bool bIsReflectionCapture = false;

	for (int32 ViewIndex = 0, ViewCount = ViewFamily.Views.Num(); ViewIndex < ViewCount; ++ViewIndex)
	{
		const FSceneView* View = ViewFamily.Views[ViewIndex];

		bIsSceneCapture |= View->bIsSceneCapture;
		bIsReflectionCapture |= View->bIsReflectionCapture;
	}

	if(!FPlatformProperties::SupportsWindowedMode())
	{
		// Force ScreenRes on non windowed platforms.
		SceneTargetsSizingMethod = RequestedSize;
	}
	else if (GIsEditor)
	{
		// Always grow scene render targets in the editor.
		SceneTargetsSizingMethod = Grow;
	}	
	else
	{
		// Otherwise use the setting specified by the console variable.
		SceneTargetsSizingMethod = (ESizingMethods) FMath::Clamp(CVarSceneTargetsResizingMethod.GetValueOnRenderThread(), 0, (int32)VisibleSizingMethodsCount);
	}

	FIntPoint DesiredBufferSize = FIntPoint::ZeroValue;

	switch (SceneTargetsSizingMethod)
	{
		case RequestedSize:
			DesiredBufferSize = FIntPoint(ViewFamily.FamilySizeX, ViewFamily.FamilySizeY);
			break;

		case ScreenRes:
			DesiredBufferSize = FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY);
			break;

		case Grow:
			DesiredBufferSize = FIntPoint(FMath::Max((uint32)GetBufferSizeXY().X, ViewFamily.FamilySizeX),
					FMath::Max((uint32)GetBufferSizeXY().Y, ViewFamily.FamilySizeY));
			break;

		default:
			checkNoEntry();
	}


	// we want to shrink the buffer but as we can have multiple scenecaptures per frame we have to delay that a frame to get all size requests
	{
		// this allows The BufferSize to not grow below the SceneCapture requests (happen before scene rendering, in the same frame with a Grow request)
		LargestDesiredSizeThisFrame = LargestDesiredSizeThisFrame.ComponentMax(DesiredBufferSize);

		uint32 FrameNumber = ViewFamily.FrameNumber;

		// this could be refined to be some time or multiple frame if we have SceneCaptures not running each frame any more
		if(ThisFrameNumber != FrameNumber)
		{
			// this allows the BufferSize to shrink each frame (in game)
			ThisFrameNumber = FrameNumber;
			LargestDesiredSizeLastFrame = LargestDesiredSizeThisFrame;
			LargestDesiredSizeThisFrame = FIntPoint(0, 0);
		}

		DesiredBufferSize = DesiredBufferSize.ComponentMax(LargestDesiredSizeLastFrame);
	}

	return DesiredBufferSize;
}
inline uint16 GetNumSceneColorMSAASamples(ERHIFeatureLevel::Type InFeatureLevel)
{
	if (InFeatureLevel > ERHIFeatureLevel::ES3_1 ||
		//@todo-rco: Fix when iOS OpenGL supports MSAA
		GShaderPlatformForFeatureLevel[InFeatureLevel] == SP_OPENGL_ES2_IOS)
	{
		return 1;
	}

	uint16 NumSamples = CVarMobileMSAA.GetValueOnRenderThread();
	if (NumSamples != 1 && NumSamples != 2 && NumSamples != 4)
	{
		NumSamples = 1;
	}
	return NumSamples;
}

void FSceneRenderTargets::Allocate(FRHICommandList& RHICmdList, const FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
	// ViewFamily setup wasn't complete
	check(ViewFamily.FrameNumber != UINT_MAX);

	// If feature level has changed, release all previously allocated targets to the pool. If feature level has changed but
	const auto NewFeatureLevel = ViewFamily.Scene->GetFeatureLevel();
	CurrentShadingPath = ViewFamily.Scene->ShouldUseDeferredRenderer() ? EShadingPath::Deferred : EShadingPath::Forward;

	FIntPoint DesiredBufferSize = ComputeDesiredSize(ViewFamily);
	check(DesiredBufferSize.X > 0 && DesiredBufferSize.Y > 0);
	QuantizeBufferSize(DesiredBufferSize.X, DesiredBufferSize.Y);

	int GBufferFormat = CVarGBufferFormat.GetValueOnRenderThread();

	int SceneColorFormat;
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SceneColorFormat"));

		SceneColorFormat = CVar->GetValueOnRenderThread();
	}
		
	bool bNewAllowStaticLighting;
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));

		bNewAllowStaticLighting = CVar->GetValueOnRenderThread() != 0;
	}

	bool bDownsampledOcclusionQueries = GDownsampledOcclusionQueries != 0;

	int32 MaxShadowResolution = GetCachedScalabilityCVars().MaxShadowResolution;

	int32 RSMResolution = FMath::Clamp(CVarRSMResolution.GetValueOnRenderThread(), 1, 2048);

	if (ViewFamily.Scene->ShouldUseDeferredRenderer() == false)
	{
		// ensure there is always enough space for forward renderer's tiled shadow maps
		// by reducing the shadow map resolution.
		int32 MaxShadowDepthBufferDim = FMath::Max(GMaxShadowDepthBufferSizeX, GMaxShadowDepthBufferSizeY);
		if (MaxShadowResolution * 2 >  MaxShadowDepthBufferDim)
		{
			MaxShadowResolution = MaxShadowDepthBufferDim / 2;
		}
	}

	int32 TranslucencyLightingVolumeDim = GTranslucencyLightingVolumeDim;

	uint32 Mobile32bpp = !IsMobileHDR() || IsMobileHDR32bpp();

	int32 MobileMSAA = GetNumSceneColorMSAASamples(NewFeatureLevel);

	bool bLightPropagationVolume = UseLightPropagationVolumeRT(NewFeatureLevel);

	uint32 MinShadowResolution;
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.MinResolution"));

		MinShadowResolution = CVar->GetValueOnRenderThread();
	}

	if( (BufferSize.X != DesiredBufferSize.X) ||
		(BufferSize.Y != DesiredBufferSize.Y) ||
		(CurrentGBufferFormat != GBufferFormat) ||
		(CurrentSceneColorFormat != SceneColorFormat) ||
		(bAllowStaticLighting != bNewAllowStaticLighting) ||
		(bUseDownsizedOcclusionQueries != bDownsampledOcclusionQueries) ||
		(CurrentMaxShadowResolution != MaxShadowResolution) ||
 		(CurrentRSMResolution != RSMResolution) ||
		(CurrentTranslucencyLightingVolumeDim != TranslucencyLightingVolumeDim) ||
		(CurrentMobile32bpp != Mobile32bpp) ||
		(CurrentMobileMSAA != MobileMSAA) ||
		(bCurrentLightPropagationVolume != bLightPropagationVolume) ||
		(CurrentMinShadowResolution != MinShadowResolution))
	{
		CurrentGBufferFormat = GBufferFormat;
		CurrentSceneColorFormat = SceneColorFormat;
		bAllowStaticLighting = bNewAllowStaticLighting;
		bUseDownsizedOcclusionQueries = bDownsampledOcclusionQueries;
		CurrentMaxShadowResolution = MaxShadowResolution;
		CurrentRSMResolution = RSMResolution;
		CurrentTranslucencyLightingVolumeDim = TranslucencyLightingVolumeDim;
		CurrentMobile32bpp = Mobile32bpp;
		CurrentMobileMSAA = MobileMSAA;
		CurrentMinShadowResolution = MinShadowResolution;
		bCurrentLightPropagationVolume = bLightPropagationVolume;

		// Reinitialize the render targets for the given size.
		SetBufferSize(DesiredBufferSize.X, DesiredBufferSize.Y);

		UE_LOG(LogRenderer, Log, TEXT("Reallocating scene render targets to support %ux%u (Frame:%u)."), BufferSize.X, BufferSize.Y, ViewFamily.FrameNumber);

		UpdateRHI();
	}

	// Do allocation of render targets if they aren't available for the current shading path
	CurrentFeatureLevel = NewFeatureLevel;
	AllocateRenderTargets(RHICmdList);
}

void FSceneRenderTargets::BeginRenderingSceneColor(FRHICommandList& RHICmdList, ESimpleRenderTargetMode RenderTargetMode/*=EUninitializedColorExistingDepth*/, FExclusiveDepthStencil DepthStencilAccess, bool bTransitionWritable)
{
	SCOPED_DRAW_EVENT(RHICmdList, BeginRenderingSceneColor);

	AllocSceneColor(RHICmdList);
	SetRenderTarget(RHICmdList, GetSceneColorSurface(), GetSceneDepthSurface(), RenderTargetMode, DepthStencilAccess, bTransitionWritable);
} 

int32 FSceneRenderTargets::GetGBufferRenderTargets(ERenderTargetLoadAction ColorLoadAction, FRHIRenderTargetView OutRenderTargets[MaxSimultaneousRenderTargets], int32& OutVelocityRTIndex)
{
	int32 MRTCount = 0;
	OutRenderTargets[MRTCount++] = FRHIRenderTargetView(GetSceneColorSurface(), 0, -1, ColorLoadAction, ERenderTargetStoreAction::EStore);
	OutRenderTargets[MRTCount++] = FRHIRenderTargetView(GBufferA->GetRenderTargetItem().TargetableTexture, 0, -1, ColorLoadAction, ERenderTargetStoreAction::EStore);
	OutRenderTargets[MRTCount++] = FRHIRenderTargetView(GBufferB->GetRenderTargetItem().TargetableTexture, 0, -1, ColorLoadAction, ERenderTargetStoreAction::EStore);
	OutRenderTargets[MRTCount++] = FRHIRenderTargetView(GBufferC->GetRenderTargetItem().TargetableTexture, 0, -1, ColorLoadAction, ERenderTargetStoreAction::EStore);

	// The velocity buffer needs to be bound before other optionnal rendertargets (when UseSelecUseSelectiveBasePassOutputs() is true).
	// Otherwise there is an issue on some AMD hardware where the target does not get updated. Seems to be related to the velocity buffer format as it works fine with other targets.
	if (bAllocateVelocityGBuffer)
	{
		OutVelocityRTIndex = MRTCount;
		check(OutVelocityRTIndex == 4); // As defined in BasePassPixelShader.usf
		OutRenderTargets[MRTCount++] = FRHIRenderTargetView(GBufferVelocity->GetRenderTargetItem().TargetableTexture, 0, -1, ColorLoadAction, ERenderTargetStoreAction::EStore);
	}
	else
	{
	OutVelocityRTIndex = -1;
	}

	OutRenderTargets[MRTCount++] = FRHIRenderTargetView(GBufferD->GetRenderTargetItem().TargetableTexture, 0, -1, ColorLoadAction, ERenderTargetStoreAction::EStore);


	if (bAllowStaticLighting)
	{
		check(MRTCount == (bAllocateVelocityGBuffer ? 6 : 5)); // As defined in BasePassPixelShader.usf
		OutRenderTargets[MRTCount++] = FRHIRenderTargetView(GBufferE->GetRenderTargetItem().TargetableTexture, 0, -1, ColorLoadAction, ERenderTargetStoreAction::EStore);
	}

	check(MRTCount <= MaxSimultaneousRenderTargets);
	return MRTCount;
}

void FSceneRenderTargets::BeginRenderingGBuffer(FRHICommandList& RHICmdList, ERenderTargetLoadAction ColorLoadAction, ERenderTargetLoadAction DepthLoadAction, bool bBindQuadOverdrawBuffers, const FLinearColor& ClearColor/*=(0,0,0,1)*/)
{
	SCOPED_DRAW_EVENT(RHICmdList, BeginRenderingSceneColor);

	if (IsSimpleDynamicLightingEnabled())
	{
		// in this non-standard case, just render to scene color with default mode
		BeginRenderingSceneColor(RHICmdList);
		return;
	}

	AllocSceneColor(RHICmdList);

	// Set the scene color surface as the render target, and the scene depth surface as the depth-stencil target.
	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		bool bClearColor = ColorLoadAction == ERenderTargetLoadAction::EClear;
		bool bClearDepth = DepthLoadAction == ERenderTargetLoadAction::EClear;

		//if the desired clear color doesn't match the bound hwclear value, or there isn't one at all (editor code)
		//then we need to fall back to a shader clear.
		const FTextureRHIRef& SceneColorTex = GetSceneColorSurface();
		bool bShaderClear = false;
		if (bClearColor)
		{			
			if (!SceneColorTex->HasClearValue() || (ClearColor != SceneColorTex->GetClearColor()))
			{
				ColorLoadAction = ERenderTargetLoadAction::ENoAction;				
				bShaderClear = true;
			}
			else
			{
				bGBuffersFastCleared = true;
			}
		}

		int32 VelocityRTIndex;
		FRHIRenderTargetView RenderTargets[MaxSimultaneousRenderTargets];
		int32 MRTCount = GetGBufferRenderTargets(ColorLoadAction, RenderTargets, VelocityRTIndex);

		//make sure our conditions for shader clear fallback are valid.
		check(RenderTargets[0].Texture == SceneColorTex);
		
		

		FRHIDepthRenderTargetView DepthView(GetSceneDepthSurface(), DepthLoadAction, ERenderTargetStoreAction::EStore);
		FRHISetRenderTargetsInfo Info(MRTCount, RenderTargets, DepthView);		

		if (bClearDepth)
		{
			bSceneDepthCleared = true;			
		}		

		if (bBindQuadOverdrawBuffers && RuntimeAllowDebugViewModeShader(CurrentFeatureLevel))
		{
			if (QuadOverdrawBuffer.IsValid() && QuadOverdrawBuffer->GetRenderTargetItem().UAV.IsValid())
			{
				QuadOverdrawIndex = 7; // As defined in QuadOverdraw.usf

				// Increase the rendertarget count in order to control the bound slot of the UAV.
				check(Info.NumColorRenderTargets <= QuadOverdrawIndex);
				Info.NumColorRenderTargets = QuadOverdrawIndex;
				Info.UnorderedAccessView[Info.NumUAVs++] = QuadOverdrawBuffer->GetRenderTargetItem().UAV;

				// Clear to default value
				const uint32 ClearValue[4] = { 0, 0, 0, 0 };
				RHICmdList.ClearUAV(QuadOverdrawBuffer->GetRenderTargetItem().UAV, ClearValue);
				RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToGfx, QuadOverdrawBuffer->GetRenderTargetItem().UAV);
			}
		}

		// set the render target
		RHICmdList.SetRenderTargetsAndClear(Info);
		if (bShaderClear)
		{
			FLinearColor ClearColors[MaxSimultaneousRenderTargets];
			ClearColors[0] = ClearColor;
			for (int32 i = 1; i < MRTCount; ++i)
			{
				ClearColors[i] = RenderTargets[i].Texture->GetClearColor();
			}
			//depth/stencil should have been handled by the fast clear.  only color for RT0 can get changed.
			RHICmdList.ClearMRT(true, MRTCount, ClearColors, false, 0.0f, false, 0, FIntRect());
		}

		//bind any clear data that won't be bound automatically by the preceding SetRenderTargetsAndClear
		bool bBindClearColor = !bClearColor && bGBuffersFastCleared;
		bool bBindClearDepth = !bClearDepth && bSceneDepthCleared;
		RHICmdList.BindClearMRTValues(bBindClearColor, bBindClearDepth, bBindClearDepth);
	}
}

void FSceneRenderTargets::FinishRenderingGBuffer(FRHICommandListImmediate& RHICmdList)
{
	if (IsSimpleDynamicLightingEnabled())
	{
		// in this non-standard case, just render to scene color with default mode
		FinishRenderingSceneColor(RHICmdList, true);
		return;
	}

	int32 VelocityRTIndex;
	FRHIRenderTargetView RenderTargets[MaxSimultaneousRenderTargets];
	int32 NumMRTs = GetGBufferRenderTargets(ERenderTargetLoadAction::ELoad, RenderTargets, VelocityRTIndex);

	FResolveParams ResolveParams;
	for (int32 i = 0; i < NumMRTs; ++i)
	{
		 // When the basepass outputs to the velocity buffer, don't resolve it yet if selective outputs are enabled, as it will be resolved after the velocity pass.
		if (i != VelocityRTIndex || !UseSelectiveBasePassOutputs())
		{
		RHICmdList.CopyToResolveTarget(RenderTargets[i].Texture, RenderTargets[i].Texture, true, ResolveParams);
	}
	}
	FTextureRHIParamRef DepthSurface = GetSceneDepthSurface();
	RHICmdList.CopyToResolveTarget(DepthSurface, DepthSurface, true, ResolveParams);

	QuadOverdrawIndex = INDEX_NONE;
}

int32 FSceneRenderTargets::GetNumGBufferTargets() const
{
	int32 NumGBufferTargets = 1;

	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM4 && !IsSimpleDynamicLightingEnabled())
	{
		// This needs to match TBasePassPixelShaderBaseType::ModifyCompilationEnvironment()
		NumGBufferTargets = bAllowStaticLighting ? 6 : 5;

		if (bAllocateVelocityGBuffer)
		{
			++NumGBufferTargets;
		}
	}
	return NumGBufferTargets;
}

void FSceneRenderTargets::AllocSceneColor(FRHICommandList& RHICmdList)
{
	if (GetSceneColorForCurrentShadingPath())
	{
		// no work needed
		return;
	}

	// create SceneColor on demand so it can be shared with other pooled RT

	EPixelFormat SceneColorBufferFormat = GetSceneColorFormat();

	// Create the scene color.
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, SceneColorBufferFormat, FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable, false));

		Desc.Flags |= TexCreate_FastVRAM;

		int32 OptimizeForUAVPerformance = CVarOptimizeForUAVPerformance.GetValueOnRenderThread();

		// with TexCreate_UAV it would allow better sharing with later elements but it might come at a high cost:
		// GCNPerformanceTweets.pdf Tip 37: Warning: Causes additional synchronization between draw calls when using a render target allocated with this flag, use sparingly
		if (CurrentFeatureLevel >= ERHIFeatureLevel::SM5 && !OptimizeForUAVPerformance)
		{
			Desc.TargetableFlags |= TexCreate_UAV;
		}

		if (CurrentFeatureLevel < ERHIFeatureLevel::SM4)
		{
			Desc.NumSamples = GetNumSceneColorMSAASamples(CurrentFeatureLevel);
		}

		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, GetSceneColorForCurrentShadingPath(), GetSceneColorTargetName(CurrentShadingPath));
	}

	// otherwise we have a severe problem
	check(GetSceneColorForCurrentShadingPath());
}

void FSceneRenderTargets::AllocLightAttenuation(FRHICommandList& RHICmdList)
{
	if(LightAttenuation)
	{
		// no work needed
		return;
	}

	check(IsInRenderingThread());

	// create LightAttenuation on demand so it can be shared with other pooled RT

	// Create a texture to store the resolved light attenuation values, and a render-targetable surface to hold the unresolved light attenuation values.
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_B8G8R8A8, FClearValueBinding::White, TexCreate_None, TexCreate_RenderTargetable, false));
		Desc.Flags |= TexCreate_FastVRAM;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, LightAttenuation, TEXT("LightAttenuation"));

		// the channel assignment is documented in ShadowRendering.cpp (look for Light Attenuation channel assignment)
	}

	// otherwise we have a severe problem
	check(LightAttenuation);
}

TRefCountPtr<IPooledRenderTarget>& FSceneRenderTargets::GetReflectionBrightnessTarget()
{
	bool bSupportsR32Float = CurrentFeatureLevel > ERHIFeatureLevel::ES2;
	int32 ReflectionBrightnessIndex = bSupportsR32Float ? 0 : 1;
	return ReflectionBrightness[ReflectionBrightnessIndex];
}

void FSceneRenderTargets::ReleaseGBufferTargets()
{
	GBufferResourcesUniformBuffer.SafeRelease();
	GBufferA.SafeRelease();
	GBufferB.SafeRelease();
	GBufferC.SafeRelease();
	GBufferD.SafeRelease();
	GBufferE.SafeRelease();
	GBufferVelocity.SafeRelease();
}

void FSceneRenderTargets::PreallocGBufferTargets(bool bShouldRenderVelocities)
{
	if (GBasePassOutputsVelocityDebug == 1)
	{
		bAllocateVelocityGBuffer = false;
	}
	else
	{
		bAllocateVelocityGBuffer = bShouldRenderVelocities && FVelocityRendering::OutputsToGBuffer();
	}
}

void FSceneRenderTargets::AllocGBufferTargets(FRHICommandList& RHICmdList)
{
	// AdjustGBufferRefCount +1 doesn't match -1 (within the same frame)
	ensure(GBufferRefCount == 0);

	if (GBufferA)
	{
		// no work needed
		return;
	}

	// create GBuffer on demand so it can be shared with other pooled RT

	// good to see the quality loss due to precision in the gbuffer
	const bool bHighPrecisionGBuffers = (CurrentGBufferFormat >= 5);
	// good to profile the impact of non 8 bit formats
	const bool bEnforce8BitPerChannel = (CurrentGBufferFormat == 0);

	// Create the world-space normal g-buffer.
	{
		EPixelFormat NormalGBufferFormat = bHighPrecisionGBuffers ? PF_FloatRGBA : PF_A2B10G10R10;

		if(bEnforce8BitPerChannel)
		{
			NormalGBufferFormat = PF_B8G8R8A8;
		}

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, NormalGBufferFormat, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, GBufferA, TEXT("GBufferA"));
	}

	// Create the specular color and power g-buffer.
	{
		const EPixelFormat SpecularGBufferFormat = bHighPrecisionGBuffers ? PF_FloatRGBA : PF_B8G8R8A8;

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, SpecularGBufferFormat, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, GBufferB, TEXT("GBufferB"));
	}

	// Create the diffuse color g-buffer.
	{
		const EPixelFormat DiffuseGBufferFormat = bHighPrecisionGBuffers ? PF_FloatRGBA : PF_B8G8R8A8;
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, DiffuseGBufferFormat, FClearValueBinding::Transparent, TexCreate_SRGB, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, GBufferC, TEXT("GBufferC"));
	}

	// Create the mask g-buffer (e.g. SSAO, subsurface scattering, wet surface mask, skylight mask, ...).
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_B8G8R8A8, FClearValueBinding(FLinearColor(0, 1, 1, 1)), TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, GBufferD, TEXT("GBufferD"));
	}

	if (bAllowStaticLighting)
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_B8G8R8A8, FClearValueBinding(FLinearColor(1, 1, 1, 1)), TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, GBufferE, TEXT("GBufferE"));
	}

	GBasePassOutputsVelocityDebug = CVarBasePassOutputsVelocityDebug.GetValueOnRenderThread();
	if (bAllocateVelocityGBuffer)
	{
		FPooledRenderTargetDesc VelocityRTDesc = FVelocityRendering::GetRenderTargetDesc();
		GRenderTargetPool.FindFreeElement(RHICmdList, VelocityRTDesc, GBufferVelocity, TEXT("GBufferVelocity"));
	}

	// otherwise we have a severe problem
	check(GBufferA);

	// Create the required render targets if running Highend.
	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM4)
	{

		// Allocate the Gbuffer resource uniform buffer.
		const FSceneRenderTargetItem& GBufferAToUse = GBufferA ? GBufferA->GetRenderTargetItem() : GSystemTextures.BlackDummy->GetRenderTargetItem();
		const FSceneRenderTargetItem& GBufferBToUse = GBufferB ? GBufferB->GetRenderTargetItem() : GSystemTextures.BlackDummy->GetRenderTargetItem();
		const FSceneRenderTargetItem& GBufferCToUse = GBufferC ? GBufferC->GetRenderTargetItem() : GSystemTextures.BlackDummy->GetRenderTargetItem();
		const FSceneRenderTargetItem& GBufferDToUse = GBufferD ? GBufferD->GetRenderTargetItem() : GSystemTextures.BlackDummy->GetRenderTargetItem();
		const FSceneRenderTargetItem& GBufferEToUse = GBufferE ? GBufferE->GetRenderTargetItem() : GSystemTextures.BlackDummy->GetRenderTargetItem();
		const FSceneRenderTargetItem& GBufferVelocityToUse = GBufferVelocity ? GBufferVelocity->GetRenderTargetItem() : GSystemTextures.BlackDummy->GetRenderTargetItem();

		FGBufferResourceStruct GBufferResourceStruct;

		GBufferResourceStruct.GBufferATexture = GBufferAToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferBTexture = GBufferBToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferCTexture = GBufferCToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferDTexture = GBufferDToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferETexture = GBufferEToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferVelocityTexture = GBufferVelocityToUse.ShaderResourceTexture;

		GBufferResourceStruct.GBufferATextureNonMS = GBufferAToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferBTextureNonMS = GBufferBToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferCTextureNonMS = GBufferCToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferDTextureNonMS = GBufferDToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferETextureNonMS = GBufferEToUse.ShaderResourceTexture;
		GBufferResourceStruct.GBufferVelocityTextureNonMS = GBufferVelocityToUse.ShaderResourceTexture;

		GBufferResourceStruct.GBufferATextureMS = GBufferAToUse.TargetableTexture;
		GBufferResourceStruct.GBufferBTextureMS = GBufferBToUse.TargetableTexture;
		GBufferResourceStruct.GBufferCTextureMS = GBufferCToUse.TargetableTexture;
		GBufferResourceStruct.GBufferDTextureMS = GBufferDToUse.TargetableTexture;
		GBufferResourceStruct.GBufferETextureMS = GBufferEToUse.TargetableTexture;
		GBufferResourceStruct.GBufferVelocityTextureMS = GBufferVelocityToUse.TargetableTexture;

		GBufferResourceStruct.GBufferATextureSampler = TStaticSamplerState<>::GetRHI();
		GBufferResourceStruct.GBufferBTextureSampler = TStaticSamplerState<>::GetRHI();
		GBufferResourceStruct.GBufferCTextureSampler = TStaticSamplerState<>::GetRHI();
		GBufferResourceStruct.GBufferDTextureSampler = TStaticSamplerState<>::GetRHI();
		GBufferResourceStruct.GBufferETextureSampler = TStaticSamplerState<>::GetRHI();
		GBufferResourceStruct.GBufferVelocityTextureSampler = TStaticSamplerState<>::GetRHI();

		GBufferResourcesUniformBuffer = FGBufferResourceStruct::CreateUniformBuffer(GBufferResourceStruct, UniformBuffer_SingleFrame);
	}

	// so that 
	GBufferRefCount = 1;
}

const TRefCountPtr<IPooledRenderTarget>& FSceneRenderTargets::GetSceneColor() const
{
	if (!GetSceneColorForCurrentShadingPath())
	{
		// to avoid log/ensure spam
		static bool bFirst = true;
		if(bFirst)
		{
			bFirst = false;

			// the first called should be AllocSceneColor(), contact MartinM if that happens
			ensure(GetSceneColorForCurrentShadingPath());
		}

		return GSystemTextures.BlackDummy;
	}

	return GetSceneColorForCurrentShadingPath();
}

bool FSceneRenderTargets::IsSceneColorAllocated() const
{
	return GetSceneColorForCurrentShadingPath() != 0;
}

TRefCountPtr<IPooledRenderTarget>& FSceneRenderTargets::GetSceneColor()
{
	if (!GetSceneColorForCurrentShadingPath())
	{
		// to avoid log/ensure spam
		static bool bFirst = true;
		if(bFirst)
		{
			bFirst = false;

			// the first called should be AllocSceneColor(), contact MartinM if that happens
			ensure(GetSceneColorForCurrentShadingPath());
		}

		return GSystemTextures.BlackDummy;
	}

	return GetSceneColorForCurrentShadingPath();
}

void FSceneRenderTargets::SetSceneColor(IPooledRenderTarget* In)
{
	check(CurrentShadingPath < EShadingPath::Num);
	SceneColor[(int32)CurrentShadingPath] = In;
}

void FSceneRenderTargets::SetLightAttenuation(IPooledRenderTarget* In)
{
	LightAttenuation = In;
}

const TRefCountPtr<IPooledRenderTarget>& FSceneRenderTargets::GetLightAttenuation() const
{
	if(!LightAttenuation)
	{
		// to avoid log/ensure spam
		static bool bFirst = true;
		if(bFirst)
		{
			bFirst = false;

			// First we need to call AllocLightAttenuation(), contact MartinM if that happens
			ensure(LightAttenuation);
		}

		return GSystemTextures.WhiteDummy;
	}

	return LightAttenuation;
}

TRefCountPtr<IPooledRenderTarget>& FSceneRenderTargets::GetLightAttenuation()
{
	if(!LightAttenuation)
	{
		// to avoid log/ensure spam
		static bool bFirst = true;
		if(bFirst)
		{
			bFirst = false;

			// the first called should be AllocLightAttenuation(), contact MartinM if that happens
			ensure(LightAttenuation);
		}

		return GSystemTextures.WhiteDummy;
	}

	return LightAttenuation;
}

void FSceneRenderTargets::AdjustGBufferRefCount(FRHICommandList& RHICmdList, int Delta)
{ 
	
	if(Delta > 0 && GBufferRefCount == 0)
	{
		AllocGBufferTargets(RHICmdList);
	}
	else
	{
		GBufferRefCount += Delta;

		if (GBufferRefCount == 0)
		{
			ReleaseGBufferTargets();
		}
	}
}

void FSceneRenderTargets::FinishRenderingSceneColor(FRHICommandListImmediate& RHICmdList, bool bKeepChanges, const FResolveRect& ResolveRect)
{
	SCOPED_DRAW_EVENT(RHICmdList, FinishRenderingSceneColor);

	if(bKeepChanges)
	{
		ResolveSceneColor(RHICmdList);
	}
}

bool FSceneRenderTargets::BeginRenderingCustomDepth(FRHICommandListImmediate& RHICmdList, bool bPrimitives)
{
	IPooledRenderTarget* CustomDepthRenderTarget = RequestCustomDepth(RHICmdList, bPrimitives);

	if(CustomDepthRenderTarget)
	{
		SCOPED_DRAW_EVENT(RHICmdList, BeginRenderingCustomDepth);
		
		FRHIDepthRenderTargetView DepthView(CustomDepthRenderTarget->GetRenderTargetItem().ShaderResourceTexture);
		FRHISetRenderTargetsInfo Info(0, nullptr, DepthView);
		Info.bClearStencil = IsCustomDepthPassWritingStencil();
		check(DepthView.Texture->GetStencilClearValue() == 0);

		RHICmdList.SetRenderTargetsAndClear(Info);

		return true;
	}

	return false;
}

void FSceneRenderTargets::FinishRenderingCustomDepth(FRHICommandListImmediate& RHICmdList, const FResolveRect& ResolveRect)
{
	SCOPED_DRAW_EVENT(RHICmdList, FinishRenderingCustomDepth);

	RHICmdList.CopyToResolveTarget(CustomDepth->GetRenderTargetItem().TargetableTexture, CustomDepth->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams(ResolveRect));

	bCustomDepthIsValid = true;
}

/**
* Saves a previously rendered scene color target
*/
void FSceneRenderTargets::ResolveSceneColor(FRHICommandList& RHICmdList, const FResolveRect& ResolveRect)
{
	SCOPED_DRAW_EVENT(RHICmdList, ResolveSceneColor);

	auto& CurrentSceneColor = GetSceneColor();

	uint32 samples = CurrentSceneColor->GetDesc().NumSamples;

	const EShaderPlatform CurrentShaderPlatform = GShaderPlatformForFeatureLevel[CurrentFeatureLevel];
	if (samples <= 1 || CurrentShaderPlatform == SP_METAL || IsVulkanMobilePlatform(CurrentShaderPlatform))
	{
		RHICmdList.CopyToResolveTarget(GetSceneColorSurface(), GetSceneColorTexture(), true, FResolveParams(ResolveRect));
	}
	else 
	{
		// Custom shader based color resolve for HDR color to emulate mobile.
		SetRenderTarget(RHICmdList, GetSceneColorTexture(), FTextureRHIParamRef());
		
		if(ResolveRect.IsValid())
		{
			RHICmdList.SetScissorRect(true, ResolveRect.X1, ResolveRect.Y1, ResolveRect.X2, ResolveRect.Y2);
		}
		else
		{
			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		}

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		auto ShaderMap = GetGlobalShaderMap(CurrentFeatureLevel);
		TShaderMapRef<FHdrCustomResolveVS> VertexShader(ShaderMap);

		if(samples == 2)
		{
			TShaderMapRef<FHdrCustomResolve2xPS> PixelShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(RHICmdList, CurrentFeatureLevel, BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);
			PixelShader->SetParameters(RHICmdList, CurrentSceneColor->GetRenderTargetItem().TargetableTexture);
			RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
		}
		else if(samples == 4)
		{
			TShaderMapRef<FHdrCustomResolve4xPS> PixelShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(RHICmdList, CurrentFeatureLevel, BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);
			PixelShader->SetParameters(RHICmdList, CurrentSceneColor->GetRenderTargetItem().TargetableTexture);
			RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
		}
		else if(samples == 8)
		{
			TShaderMapRef<FHdrCustomResolve8xPS> PixelShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(RHICmdList, CurrentFeatureLevel, BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);
			PixelShader->SetParameters(RHICmdList, CurrentSceneColor->GetRenderTargetItem().TargetableTexture);
			RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
		}
		else
		{
			// Everything other than 2,4,8 samples is not implemented.
			check(0);
		}
	}
}

/** Resolves the GBuffer targets so that their resolved textures can be sampled. */
void FSceneRenderTargets::ResolveGBufferSurfaces(FRHICommandList& RHICmdList, const FResolveRect& ResolveRect)
{
	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ResolveGBufferSurfaces);

		RHICmdList.CopyToResolveTarget(GBufferA->GetRenderTargetItem().TargetableTexture, GBufferA->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams(ResolveRect));
		RHICmdList.CopyToResolveTarget(GBufferB->GetRenderTargetItem().TargetableTexture, GBufferB->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams(ResolveRect));
		RHICmdList.CopyToResolveTarget(GBufferC->GetRenderTargetItem().TargetableTexture, GBufferC->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams(ResolveRect));
		RHICmdList.CopyToResolveTarget(GBufferD->GetRenderTargetItem().TargetableTexture, GBufferD->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams(ResolveRect));
		
		if (bAllowStaticLighting)
		{
			RHICmdList.CopyToResolveTarget(GBufferE->GetRenderTargetItem().TargetableTexture, GBufferE->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams(ResolveRect));
		}

		if (bAllocateVelocityGBuffer)
		{
			RHICmdList.CopyToResolveTarget(GBufferVelocity->GetRenderTargetItem().TargetableTexture, GBufferVelocity->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams(ResolveRect));
		}
	}
}

void FSceneRenderTargets::BeginRenderingPrePass(FRHICommandList& RHICmdList, bool bPerformClear)
{
	SCOPED_DRAW_EVENT(RHICmdList, BeginRenderingPrePass);

	FTextureRHIRef ColorTarget;
	FTexture2DRHIRef DepthTarget = GetSceneDepthSurface();
	
	if (bPerformClear)
	{				
		FRHIRenderTargetView ColorView(ColorTarget, 0, -1, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction);
		FRHIDepthRenderTargetView DepthView(DepthTarget, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);

		// Clear the depth buffer.
		// Note, this is a reversed Z depth surface, so 0.0f is the far plane.
		FRHISetRenderTargetsInfo Info(1, &ColorView, DepthView);
	
		RHICmdList.SetRenderTargetsAndClear(Info);
		bSceneDepthCleared = true;	
	}
	else
	{
		// Set the scene depth surface and a DUMMY buffer as color buffer
		// (as long as it's the same dimension as the depth buffer),	
		SetRenderTarget(RHICmdList, FTextureRHIRef(), DepthTarget);
		RHICmdList.BindClearMRTValues(false, true, true);
	}
}

void FSceneRenderTargets::FinishRenderingPrePass(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, FinishRenderingPrePass);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneDepthZ);
}

void FSceneRenderTargets::BeginRenderingShadowDepth(FRHICommandList& RHICmdList, bool bClear)
{
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, ShadowDepthZ);

	//All of the shadow passes are written using 'clear stencil after', so keep stencil as-is
	FRHISetRenderTargetsInfo Info(0, nullptr, FRHIDepthRenderTargetView(GetShadowDepthZSurface(), 
		bClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, 
		ERenderTargetStoreAction::EStore, 
		ERenderTargetLoadAction::ELoad, 
		ERenderTargetStoreAction::EStore));

	check(Info.DepthStencilRenderTarget.Texture->GetDepthClearValue() == 1.0f);	
	Info.ColorRenderTarget[0].StoreAction = ERenderTargetStoreAction::ENoAction;

	if (!GSupportsDepthRenderTargetWithoutColorRenderTarget)
	{
		Info.NumColorRenderTargets = 1;
		Info.ColorRenderTarget[0].Texture = GetOptionalShadowDepthColorSurface();
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, Info.ColorRenderTarget[0].Texture);
	}
	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, Info.DepthStencilRenderTarget.Texture);
	RHICmdList.SetRenderTargetsAndClear(Info);
}

void FSceneRenderTargets::BeginRenderingCubeShadowDepth(FRHICommandList& RHICmdList, int32 ShadowResolution)
{
	SCOPED_DRAW_EVENT(RHICmdList, BeginRenderingCubeShadowDepth);
	SetRenderTarget(RHICmdList, FTextureRHIRef(), GetCubeShadowDepthZSurface(ShadowResolution), true);
}

void FSceneRenderTargets::FinishRenderingShadowDepth(FRHICommandList& RHICmdList, const FResolveRect& ResolveRect)
{
	// Resolve the shadow depth z surface.
	RHICmdList.CopyToResolveTarget(GetShadowDepthZSurface(), GetShadowDepthZTexture(), false, FResolveParams(ResolveRect));
}

void FSceneRenderTargets::BeginRenderingReflectiveShadowMap(FRHICommandList& RHICmdList, FLightPropagationVolume* Lpv)
{
	FTextureRHIParamRef RenderTargets[2];
	RenderTargets[0] = GetReflectiveShadowMapNormalSurface();
	RenderTargets[1] = GetReflectiveShadowMapDiffuseSurface();

	// Hook up the geometry volume UAVs
	FUnorderedAccessViewRHIParamRef Uavs[4];
	Uavs[0] = Lpv->GetGvListBufferUav();
	Uavs[1] = Lpv->GetGvListHeadBufferUav();
	Uavs[2] = Lpv->GetVplListBufferUav();
	Uavs[3] = Lpv->GetVplListHeadBufferUav();
	
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToGfx, Uavs, ARRAY_COUNT(Uavs));
	SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, GetReflectiveShadowMapDepthSurface(), 4, Uavs);
}

void FSceneRenderTargets::FinishRenderingReflectiveShadowMap(FRHICommandList& RHICmdList, FLightPropagationVolume* Lpv, const FResolveRect& ResolveRect)
{
	// Resolve the shadow depth z surface.
	RHICmdList.CopyToResolveTarget(GetReflectiveShadowMapDepthSurface(), GetReflectiveShadowMapDepthTexture(), false, FResolveParams(ResolveRect));
	RHICmdList.CopyToResolveTarget(GetReflectiveShadowMapDiffuseSurface(), GetReflectiveShadowMapDiffuseTexture(), false, FResolveParams(ResolveRect));
	RHICmdList.CopyToResolveTarget(GetReflectiveShadowMapNormalSurface(), GetReflectiveShadowMapNormalTexture(), false, FResolveParams(ResolveRect));

	FUnorderedAccessViewRHIParamRef UavsToReadable[2];
	UavsToReadable[0] = Lpv->GetGvListBufferUav();
	UavsToReadable[1] = Lpv->GetGvListHeadBufferUav();	
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, UavsToReadable, ARRAY_COUNT(UavsToReadable));

	// Unset render targets
	FTextureRHIParamRef RenderTargets[2] = {NULL};
	FUnorderedAccessViewRHIParamRef Uavs[2] = {NULL};
	SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, FTextureRHIParamRef(), 2, Uavs);
}

void FSceneRenderTargets::FinishRenderingCubeShadowDepth(FRHICommandList& RHICmdList, int32 ShadowResolution)
{
	SCOPED_DRAW_EVENT(RHICmdList, FinishRenderingCubeShadowDepth);
	RHICmdList.CopyToResolveTarget(GetCubeShadowDepthZSurface(ShadowResolution), GetCubeShadowDepthZTexture(ShadowResolution), false, FResolveParams(FResolveRect(), CubeFace_PosX, -1, -1, 0));
}

void FSceneRenderTargets::BeginRenderingSceneAlphaCopy(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, BeginRenderingSceneAlphaCopy);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneAlphaCopy);
	SetRenderTarget(RHICmdList, GetSceneAlphaCopySurface(), 0);
}

void FSceneRenderTargets::FinishRenderingSceneAlphaCopy(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, FinishRenderingSceneAlphaCopy);
	RHICmdList.CopyToResolveTarget(GetSceneAlphaCopySurface(), SceneAlphaCopy->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams(FResolveRect()));
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneAlphaCopy);
}


void FSceneRenderTargets::BeginRenderingLightAttenuation(FRHICommandList& RHICmdList, bool bClearToWhite)
{
	SCOPED_DRAW_EVENT(RHICmdList, BeginRenderingLightAttenuation);

	AllocLightAttenuation(RHICmdList);

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GetLightAttenuation());

	// Set the light attenuation surface as the render target, and the scene depth buffer as the depth-stencil surface.
	SetRenderTarget(RHICmdList, GetLightAttenuationSurface(), GetSceneDepthSurface(), bClearToWhite ? ESimpleRenderTargetMode::EClearColorExistingDepth : ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);
}

void FSceneRenderTargets::FinishRenderingLightAttenuation(FRHICommandList& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, FinishRenderingLightAttenuation);

	// Resolve the light attenuation surface.
	RHICmdList.CopyToResolveTarget(GetLightAttenuationSurface(), LightAttenuation->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams(FResolveRect()));
	
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GetLightAttenuation());
}

void FSceneRenderTargets::BeginRenderingTranslucency(FRHICommandList& RHICmdList, const FViewInfo& View, bool bFirstTimeThisFrame)
{
	// Use the scene color buffer.
	BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

	if (bFirstTimeThisFrame)
	{
		// Clear the stencil buffer for ResponsiveAA
		RHICmdList.Clear(false, FLinearColor::White, false, (float)ERHIZBuffer::FarPlane, true, 0, FIntRect());
	}
		
	// viewport to match view size
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
}

void FSceneRenderTargets::FinishRenderingTranslucency(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View)
{
	FinishRenderingSceneColor(RHICmdList, true);
}


bool FSceneRenderTargets::BeginRenderingSeparateTranslucency(FRHICommandList& RHICmdList, const FViewInfo& View, bool bFirstTimeThisFrame)
{
	bSeparateTranslucencyPass = true;
	if(IsSeparateTranslucencyActive(View))
	{
		FIntPoint ScaledSize;
		float Scale = 1.0f;
		GetSeparateTranslucencyDimensions(ScaledSize, Scale);

		SCOPED_DRAW_EVENT(RHICmdList, BeginSeparateTranslucency);

		TRefCountPtr<IPooledRenderTarget>* SeparateTranslucency;
		if (bSnapshot)
		{
			check(SeparateTranslucencyRT.GetReference());
			SeparateTranslucency = &SeparateTranslucencyRT;
		}
		else
		{
			SeparateTranslucency = &GetSeparateTranslucency(RHICmdList, ScaledSize);
		}
		const FTexture2DRHIRef &SeparateTranslucencyDepth = Scale < 1.0f ? (const FTexture2DRHIRef&)GetSeparateTranslucencyDepth(RHICmdList, GetBufferSizeXY())->GetRenderTargetItem().TargetableTexture : GetSceneDepthSurface();

		check((*SeparateTranslucency)->GetRenderTargetItem().TargetableTexture->GetClearColor() == FLinearColor::Black);
		// clear the render target the first time, re-use afterwards
		SetRenderTarget(RHICmdList, (*SeparateTranslucency)->GetRenderTargetItem().TargetableTexture, SeparateTranslucencyDepth, 
			bFirstTimeThisFrame ? ESimpleRenderTargetMode::EClearColorExistingDepth : ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

		
		if (!bFirstTimeThisFrame)
		{
			// Clear the stencil buffer for ResponsiveAA
			RHICmdList.BindClearMRTValues(true, false, true);
		}

		RHICmdList.SetViewport(View.ViewRect.Min.X * Scale, View.ViewRect.Min.Y * Scale, 0.0f, View.ViewRect.Max.X * Scale, View.ViewRect.Max.Y * Scale, 1.0f);
		return true;
	}

	return false;
}

void FSceneRenderTargets::FinishRenderingSeparateTranslucency(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	if(IsSeparateTranslucencyActive(View))
	{
		SCOPED_DRAW_EVENT(RHICmdList, FinishSeparateTranslucency);

		TRefCountPtr<IPooledRenderTarget>* SeparateTranslucency;
		TRefCountPtr<IPooledRenderTarget>* SeparateTranslucencyDepth;
		if (bSnapshot)
		{
			check(SeparateTranslucencyRT.GetReference());
			SeparateTranslucency = &SeparateTranslucencyRT;
			SeparateTranslucencyDepth = &SeparateTranslucencyDepthRT;
		}
		else
		{
			FIntPoint ScaledSize;
			float Scale = 1.0f;
			GetSeparateTranslucencyDimensions(ScaledSize, Scale);
			SeparateTranslucency = &GetSeparateTranslucency(RHICmdList, ScaledSize);
			SeparateTranslucencyDepth = &GetSeparateTranslucencyDepth(RHICmdList, GetBufferSizeXY());
		}

		RHICmdList.CopyToResolveTarget((*SeparateTranslucency)->GetRenderTargetItem().TargetableTexture, (*SeparateTranslucency)->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams());
		RHICmdList.CopyToResolveTarget((*SeparateTranslucencyDepth)->GetRenderTargetItem().TargetableTexture, (*SeparateTranslucencyDepth)->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams());
	}

	bSeparateTranslucencyPass = false;
}

void FSceneRenderTargets::ResolveSceneDepthTexture(FRHICommandList& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, ResolveSceneDepthTexture);

	RHICmdList.CopyToResolveTarget(GetSceneDepthSurface(), GetSceneDepthTexture(), true, FResolveParams());
}

void FSceneRenderTargets::ResolveSceneDepthToAuxiliaryTexture(FRHICommandList& RHICmdList)
{
	// Resolve the scene depth to an auxiliary texture when SM3/SM4 is in use. This needs to happen so the auxiliary texture can be bound as a shader parameter
	// while the primary scene depth texture can be bound as the target. Simultaneously binding a single DepthStencil resource as a parameter and target
	// is unsupported in d3d feature level 10.
	if(!GSupportsDepthFetchDuringDepthTest)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ResolveSceneDepthToAuxiliaryTexture);

		RHICmdList.CopyToResolveTarget(GetSceneDepthSurface(), GetAuxiliarySceneDepthTexture(), true, FResolveParams());
	}
}

void FSceneRenderTargets::CleanUpEditorPrimitiveTargets()
{
	EditorPrimitivesDepth.SafeRelease();
	EditorPrimitivesColor.SafeRelease();
}

int32 FSceneRenderTargets::GetEditorMSAACompositingSampleCount() const
{
	int32 Value = 1;

	// only supported on SM5 yet (SM4 doesn't have MSAA sample load functionality which makes it harder to implement)
	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM5 && GRHISupportsMSAADepthSampleAccess)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MSAA.CompositingSampleCount"));

		Value = CVar->GetValueOnRenderThread();

		if(Value <= 1)
		{
			Value = 1;
		}
		else if(Value <= 2)
		{
			Value = 2;
		}
		else if(Value <= 4)
		{
			Value = 4;
		}
		else
		{
			Value = 8;
		}
	}

	return Value;
}

const FTexture2DRHIRef& FSceneRenderTargets::GetEditorPrimitivesColor(FRHICommandList& RHICmdList)
{
	const bool bIsValid = IsValidRef(EditorPrimitivesColor);

	if( !bIsValid || EditorPrimitivesColor->GetDesc().NumSamples != GetEditorMSAACompositingSampleCount() )
	{
		// If the target is does not match the MSAA settings it needs to be recreated
		InitEditorPrimitivesColor(RHICmdList);
	}

	return (const FTexture2DRHIRef&)EditorPrimitivesColor->GetRenderTargetItem().TargetableTexture;
}


const FTexture2DRHIRef& FSceneRenderTargets::GetEditorPrimitivesDepth(FRHICommandList& RHICmdList)
{
	const bool bIsValid = IsValidRef(EditorPrimitivesDepth);

	if (!bIsValid || (CurrentFeatureLevel >= ERHIFeatureLevel::SM5 && EditorPrimitivesDepth->GetDesc().NumSamples != GetEditorMSAACompositingSampleCount()) )
	{
		// If the target is does not match the MSAA settings it needs to be recreated
		InitEditorPrimitivesDepth(RHICmdList);
	}

	return (const FTexture2DRHIRef&)EditorPrimitivesDepth->GetRenderTargetItem().TargetableTexture;
}

TAutoConsoleVariable<int32> FSceneRenderTargets::CVarSetSeperateTranslucencyEnabled(
	TEXT("r.SeparateTranslucency"),
	1,
	TEXT("Allows to disable the separate translucency feature (all translucency is rendered in separate RT and composited\n")
	TEXT("after DOF, if not specified otherwise in the material).\n")
	TEXT(" 0: off (translucency is affected by depth of field)\n")
	TEXT(" 1: on costs GPU performance and memory but keeps translucency unaffected by Depth of Field. (default)"),
	ECVF_RenderThreadSafe);

bool FSceneRenderTargets::IsSeparateTranslucencyActive(const FViewInfo& View) const
{	
	int32 Value = FSceneRenderTargets::CVarSetSeperateTranslucencyEnabled.GetValueOnRenderThread();

	return (Value != 0) && CurrentFeatureLevel >= ERHIFeatureLevel::SM4
		&& (View.Family->EngineShowFlags.PostProcessing || View.Family->EngineShowFlags.ShaderComplexity)
		&& View.Family->EngineShowFlags.SeparateTranslucency;
}

void FSceneRenderTargets::InitEditorPrimitivesColor(FRHICommandList& RHICmdList)
{
	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, 
		PF_B8G8R8A8,
		FClearValueBinding::Transparent,
		TexCreate_None, 
		TexCreate_ShaderResource | TexCreate_RenderTargetable,
		false));

	Desc.NumSamples = GetEditorMSAACompositingSampleCount();

	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, EditorPrimitivesColor, TEXT("EditorPrimitivesColor"));
}

void FSceneRenderTargets::InitEditorPrimitivesDepth(FRHICommandList& RHICmdList)
{
	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, 
		PF_DepthStencil,
		FClearValueBinding::DepthFar,
		TexCreate_None, 
		TexCreate_ShaderResource | TexCreate_DepthStencilTargetable,
		false));

	Desc.NumSamples = GetEditorMSAACompositingSampleCount();

	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, EditorPrimitivesDepth, TEXT("EditorPrimitivesDepth"));
}

void FSceneRenderTargets::QuantizeBufferSize(int32& InOutBufferSizeX, int32& InOutBufferSizeY)
{
	// ensure sizes are dividable by DividableBy to get post processing effects with lower resolution working well
	const uint32 DividableBy = 4;

	const uint32 Mask = ~(DividableBy - 1);
	InOutBufferSizeX = (InOutBufferSizeX + DividableBy - 1) & Mask;
	InOutBufferSizeY = (InOutBufferSizeY + DividableBy - 1) & Mask;
}

void FSceneRenderTargets::SetBufferSize(int32 InBufferSizeX, int32 InBufferSizeY)
{
	QuantizeBufferSize(InBufferSizeX, InBufferSizeY);
	BufferSize.X = InBufferSizeX;
	BufferSize.Y = InBufferSizeY;
}

void FSceneRenderTargets::SetSeparateTranslucencyBufferSize(bool bAnyViewWantsDownsampledSeparateTranslucency)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.SeparateTranslucencyScreenPercentage"));
	const float CVarScale = FMath::Clamp(CVar->GetValueOnRenderThread() / 100.0f, 0.0f, 100.0f);
	float EffectiveScale = CVarScale;

	// 'r.SeparateTranslucencyScreenPercentage' CVar wins over automatic downsampling
	if (FMath::Abs(CVarScale - 1.0f) < .001f && bAnyViewWantsDownsampledSeparateTranslucency)
	{
		EffectiveScale = .5f;
	}

	int32 ScaledX = GetBufferSizeXY().X * EffectiveScale;
	int32 ScaledY = GetBufferSizeXY().Y * EffectiveScale;
	SeparateTranslucencyBufferSize = FIntPoint(FMath::Max(ScaledX, 1), FMath::Max(ScaledY, 1));
	SeparateTranslucencyScale = EffectiveScale;
}

void FSceneRenderTargets::AllocateForwardShadingPathRenderTargets(FRHICommandList& RHICmdList)
{
	// on ES2 we don't do on demand allocation of SceneColor yet (in non ES2 it's released in the Tonemapper Process())
	AllocSceneColor(RHICmdList);
	AllocateCommonDepthTargets(RHICmdList);
	AllocateReflectionTargets(RHICmdList);
	AllocateDebugViewModeTargets(RHICmdList);

	EPixelFormat Format = GetSceneColor()->GetDesc().Format;

#if PLATFORM_HTML5
	// For 64-bit ES2 without framebuffer fetch, create extra render target for copy of alpha channel.
	if((Format == PF_FloatRGBA) && (GSupportsShaderFramebufferFetch == false)) 
	{
		// creating a PF_R16F (a true one-channel renderable fp texture) is only supported on GL if EXT_texture_rg is available.  It's present
		// on iOS, but not in WebGL or Android.
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneAlphaCopy, TEXT("SceneAlphaCopy"));
	}
	else
#endif
	{
		SceneAlphaCopy = GSystemTextures.MaxFP16Depth;
	}
}

void FSceneRenderTargets::AllocateForwardShadingShadowDepthTarget(FRHICommandListImmediate& RHICmdList, const FIntPoint& ShadowBufferResolution)
{
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ShadowBufferResolution, PF_ShadowDepth, FClearValueBinding::DepthOne, TexCreate_None, TexCreate_DepthStencilTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ShadowDepthZ, TEXT("ShadowDepthZ"));
	}

	if (!GSupportsDepthRenderTargetWithoutColorRenderTarget)
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ShadowBufferResolution, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OptionalShadowDepthColor, TEXT("OptionalShadowDepthColor"));
	}
}

// for easier use of "VisualizeTexture"
static TCHAR* const GetVolumeName(uint32 Id, bool bDirectional)
{
	// (TCHAR*) for non VisualStudio
	switch(Id)
	{
		case 0: return bDirectional ? (TCHAR*)TEXT("TranslucentVolumeDir0") : (TCHAR*)TEXT("TranslucentVolume0");
		case 1: return bDirectional ? (TCHAR*)TEXT("TranslucentVolumeDir1") : (TCHAR*)TEXT("TranslucentVolume1");
		case 2: return bDirectional ? (TCHAR*)TEXT("TranslucentVolumeDir2") : (TCHAR*)TEXT("TranslucentVolume2");

		default:
			check(0);
	}
	return (TCHAR*)TEXT("InvalidName");
}


// for easier use of "VisualizeTexture"
static TCHAR* const GetTranslucencyShadowTransmissionName(uint32 Id)
{
	// (TCHAR*) for non VisualStudio
	switch(Id)
	{
		case 0: return (TCHAR*)TEXT("TranslucencyShadowTransmission0");
		case 1: return (TCHAR*)TEXT("TranslucencyShadowTransmission1");

		default:
			check(0);
	}
	return (TCHAR*)TEXT("InvalidName");
}

void FSceneRenderTargets::AllocateReflectionTargets(FRHICommandList& RHICmdList)
{	
	if (GSupportsRenderTargetFormat_PF_FloatRGBA)
	{
		// Reflection targets are shared between both forward and deferred shading paths. If we have already allocated for one and are now allocating for the other,
		// we can skip these targets.
		bool bSharedReflectionTargetsAllocated = ReflectionColorScratchCubemap[0] != nullptr;

		if (!bSharedReflectionTargetsAllocated)
		{
			extern ENGINE_API int32 GReflectionCaptureSize;
			const int32 NumReflectionCaptureMips = FMath::CeilLogTwo(GReflectionCaptureSize) + 1;

			// We write to these cubemap faces individually during filtering
			uint32 CubeTexFlags = TexCreate_TargetArraySlicesIndependently;

			{
				// Create scratch cubemaps for filtering passes
				FPooledRenderTargetDesc Desc2(FPooledRenderTargetDesc::CreateCubemapDesc(GReflectionCaptureSize, PF_FloatRGBA, FClearValueBinding::None, CubeTexFlags, TexCreate_RenderTargetable, false, 1, NumReflectionCaptureMips));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc2, ReflectionColorScratchCubemap[0], TEXT("ReflectionColorScratchCubemap0"));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc2, ReflectionColorScratchCubemap[1], TEXT("ReflectionColorScratchCubemap1"));
			}

			extern int32 GDiffuseIrradianceCubemapSize;
			const int32 NumDiffuseIrradianceMips = FMath::CeilLogTwo(GDiffuseIrradianceCubemapSize) + 1;

			{
				FPooledRenderTargetDesc Desc2(FPooledRenderTargetDesc::CreateCubemapDesc(GDiffuseIrradianceCubemapSize, PF_FloatRGBA, FClearValueBinding::None, CubeTexFlags, TexCreate_RenderTargetable, false, 1, NumDiffuseIrradianceMips));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc2, DiffuseIrradianceScratchCubemap[0], TEXT("DiffuseIrradianceScratchCubemap0"));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc2, DiffuseIrradianceScratchCubemap[1], TEXT("DiffuseIrradianceScratchCubemap1"));
			}

			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(FSHVector3::MaxSHBasis, 1), PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SkySHIrradianceMap, TEXT("SkySHIrradianceMap"));
			}
		}

		auto& ReflectionBrightnessTarget = GetReflectionBrightnessTarget();
		if (!ReflectionBrightnessTarget)
		{
			bool bSupportsR32Float = CurrentFeatureLevel > ERHIFeatureLevel::ES2;
			//@todo: FScene::UpdateSkyCaptureContents() is called before FSceneRenderTargets::AllocateRenderTargets()
			// so CurrentFeatureLevel == ERHIFeatureLevel::Num and that crashes OpenGL ES2 as R32F is not valid
			if (CurrentFeatureLevel == ERHIFeatureLevel::Num)
			{
				bSupportsR32Float = GMaxRHIFeatureLevel > ERHIFeatureLevel::ES2;
			}
			int32 ReflectionBrightnessIndex = bSupportsR32Float ? 0 : 1;
			EPixelFormat BrightnessFormat = bSupportsR32Float ? PF_R32_FLOAT : PF_FloatRGBA;
			FPooledRenderTargetDesc Desc3(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(1, 1), BrightnessFormat, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc3, ReflectionBrightnessTarget, GetReflectionBrightnessTargetName(ReflectionBrightnessIndex));
		}
	}
}

void FSceneRenderTargets::AllocateDebugViewModeTargets(FRHICommandList& RHICmdList)
{
	// If the shader/quad complexity shader need a quad overdraw buffer to be bind, allocate it.
	if (RuntimeAllowDebugViewModeShader(CurrentFeatureLevel) && AllowDebugViewmodes())
	{
		FIntPoint QuadOverdrawSize;
		QuadOverdrawSize.X = 2 * FMath::Max<uint32>((BufferSize.X + 1) / 2, 1); // The size is time 2 since left side is QuadDescriptor, and right side QuadComplexity.
		QuadOverdrawSize.Y = FMath::Max<uint32>((BufferSize.Y + 1) / 2, 1);

		FPooledRenderTargetDesc QuadOverdrawDesc = FPooledRenderTargetDesc::Create2DDesc(
			QuadOverdrawSize, 
			PF_R32_UINT,
			FClearValueBinding::None,
			0,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV,
			false
			);

		GRenderTargetPool.FindFreeElement(RHICmdList, QuadOverdrawDesc, QuadOverdrawBuffer, TEXT("QuadOverdrawBuffer"));
	}
}

void FSceneRenderTargets::AllocateCommonDepthTargets(FRHICommandList& RHICmdList)
{
	if (!SceneDepthZ)
	{
		// Create a texture to store the resolved scene depth, and a render-targetable surface to hold the unresolved scene depth.
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_DepthStencil, FClearValueBinding::DepthFar, TexCreate_None, TexCreate_DepthStencilTargetable, false));
		Desc.NumSamples = GetNumSceneColorMSAASamples(CurrentFeatureLevel);
		Desc.Flags |= TexCreate_FastVRAM;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneDepthZ, TEXT("SceneDepthZ"));
		SceneStencilSRV = RHICreateShaderResourceView((FTexture2DRHIRef&)SceneDepthZ->GetRenderTargetItem().TargetableTexture, 0, 1, PF_X24_G8);

		// create non-MSAA version for hit proxies on PC
		const EShaderPlatform CurrentShaderPlatform = GShaderPlatformForFeatureLevel[CurrentFeatureLevel];
		if (Desc.NumSamples > 1 && CurrentShaderPlatform != SP_METAL && !IsVulkanMobilePlatform(CurrentShaderPlatform))
		{
			Desc.NumSamples = 1;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, NoMSAASceneDepthZ, TEXT("NoMSAASceneDepthZ"));
		}
		else
		{
			NoMSAASceneDepthZ = SceneDepthZ;
		}
	}

	// When targeting DX Feature Level 10, create an auxiliary texture to store the resolved scene depth, and a render-targetable surface to hold the unresolved scene depth.
	if (!AuxiliarySceneDepthZ && !GSupportsDepthFetchDuringDepthTest)
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_DepthStencil, FClearValueBinding::DepthFar, TexCreate_None, TexCreate_DepthStencilTargetable, false));
		Desc.AutoWritable = false;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, AuxiliarySceneDepthZ, TEXT("AuxiliarySceneDepthZ"));
	}
}

void FSceneRenderTargets::AllocateLightingChannelTexture(FRHICommandList& RHICmdList)
{
	if (!LightingChannels)
	{
		// Only need 3 bits for lighting channels
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_R16_UINT, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, LightingChannels, TEXT("LightingChannels"));
	}
}

void FSceneRenderTargets::AllocateDeferredShadingPathRenderTargets(FRHICommandList& RHICmdList)
{
	AllocateCommonDepthTargets(RHICmdList);

	// Create a quarter-sized version of the scene depth.
	{
		FIntPoint SmallDepthZSize(FMath::Max<uint32>(BufferSize.X / SmallColorDepthDownsampleFactor, 1), FMath::Max<uint32>(BufferSize.Y / SmallColorDepthDownsampleFactor, 1));

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SmallDepthZSize, PF_DepthStencil, FClearValueBinding::None, TexCreate_None, TexCreate_DepthStencilTargetable, true));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SmallDepthZ, TEXT("SmallDepthZ"));
	}

	// Set up quarter size scene color shared texture
	const FIntPoint ShadowBufferResolution = GetShadowDepthTextureResolution();

	const FIntPoint TranslucentShadowBufferResolution = GetTranslucentShadowDepthTextureResolution();

	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		for (int32 SurfaceIndex = 0; SurfaceIndex < NumTranslucencyShadowSurfaces; SurfaceIndex++)
		{
			if (!TranslucencyShadowTransmission[SurfaceIndex])
			{
				// Using PF_FloatRGBA because Fourier coefficients used by Fourier opacity maps have a large range and can be negative
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(TranslucentShadowBufferResolution, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TranslucencyShadowTransmission[SurfaceIndex], GetTranslucencyShadowTransmissionName(SurfaceIndex));
			}
		}
	}

	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		// Create several shadow depth cube maps with different resolutions, to handle different sized shadows on the screen
		for (int32 SurfaceIndex = 0; SurfaceIndex < NumCubeShadowDepthSurfaces; SurfaceIndex++)
		{
			const int32 SurfaceResolution = GetCubeShadowDepthZResolution(SurfaceIndex);

			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::CreateCubemapDesc(SurfaceResolution, PF_ShadowDepth, FClearValueBinding::None, TexCreate_None, TexCreate_DepthStencilTargetable | TexCreate_NoFastClear, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, CubeShadowDepthZ[SurfaceIndex], TEXT("CubeShadowDepthZ[]"));
		}
	}

	//create the shadow depth texture and/or surface
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ShadowBufferResolution, PF_ShadowDepth, FClearValueBinding::DepthOne, TexCreate_None, TexCreate_DepthStencilTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ShadowDepthZ, TEXT("ShadowDepthZ"));
	}
		
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(GetPreShadowCacheTextureResolution(), PF_ShadowDepth, FClearValueBinding::None, TexCreate_None, TexCreate_DepthStencilTargetable, false));
		Desc.AutoWritable = false;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, PreShadowCacheDepthZ, TEXT("PreShadowCacheDepthZ"));
		// Mark the preshadow cache as newly allocated, so the cache will know to update
		bPreshadowCacheNewlyAllocated = true;
	}

	// Create the required render targets if running Highend.
	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		// Create the screen space ambient occlusion buffer
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_G8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));

			if (CurrentFeatureLevel >= ERHIFeatureLevel::SM5)
			{
				// UAV is only needed to support "r.AmbientOcclusion.Compute"
				// todo: ideally this should be only UAV or RT, not both
				Desc.TargetableFlags |= TexCreate_UAV;
			}
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ScreenSpaceAO, TEXT("ScreenSpaceAO"));
		}
		
		{
			for (int32 RTSetIndex = 0; RTSetIndex < NumTranslucentVolumeRenderTargetSets; RTSetIndex++)
			{
				GRenderTargetPool.FindFreeElement(
					RHICmdList,
					FPooledRenderTargetDesc(FPooledRenderTargetDesc::CreateVolumeDesc(
						GTranslucencyLightingVolumeDim,
						GTranslucencyLightingVolumeDim,
						GTranslucencyLightingVolumeDim,
						PF_FloatRGBA,
						FClearValueBinding::None,
						0,
						TexCreate_ShaderResource | TexCreate_RenderTargetable,
						false,
						1,
						false)),
					TranslucencyLightingVolumeAmbient[RTSetIndex],
					GetVolumeName(RTSetIndex, false)
					);

				GRenderTargetPool.FindFreeElement(
					RHICmdList,
					FPooledRenderTargetDesc(FPooledRenderTargetDesc::CreateVolumeDesc(
						GTranslucencyLightingVolumeDim,
						GTranslucencyLightingVolumeDim,
						GTranslucencyLightingVolumeDim,
						PF_FloatRGBA,
						FClearValueBinding::None,
						0,
						TexCreate_ShaderResource | TexCreate_RenderTargetable,
						false,
						1,
						false)),
					TranslucencyLightingVolumeDirectional[RTSetIndex],
					GetVolumeName(RTSetIndex, true)
					);
			}
		}
	}

	// LPV : Dynamic directional occlusion for diffuse and specular
	if(UseLightPropagationVolumeRT(CurrentFeatureLevel))
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_R8G8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DirectionalOcclusion, TEXT("DirectionalOcclusion"));
	}

	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, GetSceneColorFormat(), FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable, false));
		if (CurrentFeatureLevel >= ERHIFeatureLevel::SM5)
		{
			Desc.TargetableFlags |= TexCreate_UAV;
		}
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, LightAccumulation, TEXT("LightAccumulation"));
	}

	AllocateReflectionTargets(RHICmdList);
	AllocateDebugViewModeTargets(RHICmdList);

	if (CurrentFeatureLevel >= ERHIFeatureLevel::SM5)
	{
		// Create the reflective shadow map textures for LightPropagationVolume feature
		if(bCurrentLightPropagationVolume)
		{
			int32 Res = GetReflectiveShadowMapResolution();
			FIntPoint Extent = FIntPoint(Res, Res);

			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Extent, PF_R8G8B8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ReflectiveShadowMapNormal, TEXT("RSMNormal"));
			}

			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Extent, PF_FloatR11G11B10, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ReflectiveShadowMapDiffuse, TEXT("RSMDiffuse"));
			}

			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Extent, PF_DepthStencil, FClearValueBinding::None, TexCreate_None, TexCreate_DepthStencilTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ReflectiveShadowMapDepth, TEXT("RSMDepth"));
			}
		}
	}

	if (bAllocateVelocityGBuffer)
	{
		FPooledRenderTargetDesc VelocityRTDesc = FVelocityRendering::GetRenderTargetDesc();
		GRenderTargetPool.FindFreeElement(RHICmdList, VelocityRTDesc, GBufferVelocity, TEXT("GBufferVelocity"));
	}
}

EPixelFormat FSceneRenderTargets::GetSceneColorFormat() const
{
	EPixelFormat SceneColorBufferFormat = PF_FloatRGBA;

	if (CurrentFeatureLevel < ERHIFeatureLevel::SM4)
	{
		// Potentially allocate an alpha channel in th -fe scene color texture to store the resolved scene depth.
		SceneColorBufferFormat = GSupportsRenderTargetFormat_PF_FloatRGBA ? PF_FloatRGBA : PF_B8G8R8A8;
		if (!IsMobileHDR() || IsMobileHDR32bpp()) 
		{
			SceneColorBufferFormat = PF_B8G8R8A8;
		}
	}
	else
    {
	    switch(CurrentSceneColorFormat)
	    {
		    case 0:
			    SceneColorBufferFormat = PF_R8G8B8A8; break;
		    case 1:
			    SceneColorBufferFormat = PF_A2B10G10R10; break;
		    case 2:	
			    SceneColorBufferFormat = PF_FloatR11G11B10; break;
		    case 3:	
			    SceneColorBufferFormat = PF_FloatRGB; break;
		    case 4:
			    // default
			    break;
		    case 5:
			    SceneColorBufferFormat = PF_A32B32G32R32F; break;
	    }
    
		// Fallback in case the scene color selected isn't supported.
	    if (!GPixelFormats[SceneColorBufferFormat].Supported)
	    {
		    SceneColorBufferFormat = PF_FloatRGBA;
	    }

	}

	return SceneColorBufferFormat;
}

void FSceneRenderTargets::AllocateRenderTargets(FRHICommandList& RHICmdList)
{
	if (BufferSize.X > 0 && BufferSize.Y > 0 && !AreShadingPathRenderTargetsAllocated(CurrentShadingPath))
	{
		if ((EShadingPath)CurrentShadingPath == EShadingPath::Forward)
		{
			AllocateForwardShadingPathRenderTargets(RHICmdList);
		}
		else
		{
			AllocateDeferredShadingPathRenderTargets(RHICmdList);
		}
	}
}

void FSceneRenderTargets::ReleaseSceneColor()
{
	for (auto i = 0; i < (int32)EShadingPath::Num; ++i)
	{
		SceneColor[i].SafeRelease();
	}
}

void FSceneRenderTargets::ReleaseAllTargets()
{
	ReleaseGBufferTargets();

	ReleaseSceneColor();

	SceneAlphaCopy.SafeRelease();
	SceneDepthZ.SafeRelease();
	SceneStencilSRV.SafeRelease();
	LightingChannels.SafeRelease();
	NoMSAASceneDepthZ.SafeRelease();
	AuxiliarySceneDepthZ.SafeRelease();
	SmallDepthZ.SafeRelease();
	DBufferA.SafeRelease();
	DBufferB.SafeRelease();
	DBufferC.SafeRelease();
	ScreenSpaceAO.SafeRelease();
	QuadOverdrawBuffer.SafeRelease();
	LightAttenuation.SafeRelease();
	LightAccumulation.SafeRelease();
	DirectionalOcclusion.SafeRelease();
	CustomDepth.SafeRelease();
	CustomStencilSRV.SafeRelease();
	ReflectiveShadowMapNormal.SafeRelease();
	ReflectiveShadowMapDiffuse.SafeRelease();
	ReflectiveShadowMapDepth.SafeRelease();

	for (int32 SurfaceIndex = 0; SurfaceIndex < NumTranslucencyShadowSurfaces; SurfaceIndex++)
	{
		TranslucencyShadowTransmission[SurfaceIndex].SafeRelease();
	}

	ShadowDepthZ.SafeRelease();
	OptionalShadowDepthColor.SafeRelease();

	PreShadowCacheDepthZ.SafeRelease();
	
	for(int32 Index = 0; Index < NumCubeShadowDepthSurfaces; ++Index)
	{
		CubeShadowDepthZ[Index].SafeRelease();
	}

	for (int32 i = 0; i < ARRAY_COUNT(ReflectionColorScratchCubemap); i++)
	{
		ReflectionColorScratchCubemap[i].SafeRelease();
	}

	ReflectionBrightness[0].SafeRelease();
	ReflectionBrightness[1].SafeRelease();

	for (int32 i = 0; i < ARRAY_COUNT(DiffuseIrradianceScratchCubemap); i++)
	{
		DiffuseIrradianceScratchCubemap[i].SafeRelease();
	}

	SkySHIrradianceMap.SafeRelease();

	for (int32 RTSetIndex = 0; RTSetIndex < NumTranslucentVolumeRenderTargetSets; RTSetIndex++)
	{
		TranslucencyLightingVolumeAmbient[RTSetIndex].SafeRelease();
		TranslucencyLightingVolumeDirectional[RTSetIndex].SafeRelease();
	}

	EditorPrimitivesColor.SafeRelease();
	EditorPrimitivesDepth.SafeRelease();
}

void FSceneRenderTargets::ReleaseDynamicRHI()
{
	ReleaseAllTargets();
	GRenderTargetPool.FreeUnusedResources();
}

/** Returns the size of the shadow depth buffer, taking into account platform limitations and game specific resolution limits. */
FIntPoint FSceneRenderTargets::GetShadowDepthTextureResolution() const
{
	int32 MaxShadowRes = CurrentMaxShadowResolution;
	const FIntPoint ShadowBufferResolution(
			FMath::Clamp(MaxShadowRes,1,GMaxShadowDepthBufferSizeX),
			FMath::Clamp(MaxShadowRes,1,GMaxShadowDepthBufferSizeY));
	
	return ShadowBufferResolution;
}

int32 FSceneRenderTargets::GetReflectiveShadowMapResolution() const
{
	check(IsInRenderingThread());

	return CurrentRSMResolution;
}

FIntPoint FSceneRenderTargets::GetPreShadowCacheTextureResolution() const
{
	const FIntPoint ShadowDepthResolution = GetShadowDepthTextureResolution();
	// Higher numbers increase cache hit rate but also memory usage
	const int32 ExpandFactor = 2;

	static auto CVarPreShadowResolutionFactor = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.PreShadowResolutionFactor"));

	float Factor = CVarPreShadowResolutionFactor->GetValueOnRenderThread();

	FIntPoint Ret;

	Ret.X = FMath::Clamp(FMath::TruncToInt(ShadowDepthResolution.X * Factor) * ExpandFactor, 1, GMaxShadowDepthBufferSizeX);
	Ret.Y = FMath::Clamp(FMath::TruncToInt(ShadowDepthResolution.Y * Factor) * ExpandFactor, 1, GMaxShadowDepthBufferSizeY);

	return Ret;
}

FIntPoint FSceneRenderTargets::GetTranslucentShadowDepthTextureResolution() const
{
	FIntPoint ShadowDepthResolution = GetShadowDepthTextureResolution();

	int32 Factor = GetTranslucentShadowDownsampleFactor();

	ShadowDepthResolution.X = FMath::Clamp(ShadowDepthResolution.X / Factor, 1, GMaxShadowDepthBufferSizeX);
	ShadowDepthResolution.Y = FMath::Clamp(ShadowDepthResolution.Y / Factor, 1, GMaxShadowDepthBufferSizeY);

	return ShadowDepthResolution;
}

const FTextureRHIRef& FSceneRenderTargets::GetSceneColorSurface() const							
{
	if (!GetSceneColorForCurrentShadingPath())
	{
		return GBlackTexture->TextureRHI;
	}

	return (const FTextureRHIRef&)GetSceneColor()->GetRenderTargetItem().TargetableTexture;
}

const FTextureRHIRef& FSceneRenderTargets::GetSceneColorTexture() const
{
	if (!GetSceneColorForCurrentShadingPath())
	{
		return GBlackTexture->TextureRHI;
	}

	return (const FTextureRHIRef&)GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture; 
}

const FTexture2DRHIRef* FSceneRenderTargets::GetActualDepthTexture() const
{
	const FTexture2DRHIRef* DepthTexture = NULL;
	if((CurrentFeatureLevel >= ERHIFeatureLevel::SM4) || IsPCPlatform(GShaderPlatformForFeatureLevel[CurrentFeatureLevel]))
	{
		if(GSupportsDepthFetchDuringDepthTest)
		{
			DepthTexture = &GetSceneDepthTexture();
		}
		else
		{
			DepthTexture = &GetAuxiliarySceneDepthSurface();
		}
	}
	else if (IsMobilePlatform(GShaderPlatformForFeatureLevel[CurrentFeatureLevel]))
	{
		// TODO: avoid depth texture fetch when shader needs fragment previous depth and device supports framebuffer fetch

		//bool bSceneDepthInAlpha = (GetSceneColor()->GetDesc().Format == PF_FloatRGBA);
		//bool bOnChipDepthFetch = (GSupportsShaderDepthStencilFetch || (bSceneDepthInAlpha && GSupportsShaderFramebufferFetch));
		//
		//if (bOnChipDepthFetch)
		//{
		//	DepthTexture = (const FTexture2DRHIRef*)(&GSystemTextures.DepthDummy->GetRenderTargetItem().ShaderResourceTexture);
		//}
		//else
		{
			DepthTexture = &GetSceneDepthTexture();
		}
	}

	check(DepthTexture != NULL);

	return DepthTexture;
}


IPooledRenderTarget* FSceneRenderTargets::GetGBufferVelocityRT()
{
	if (!bAllocateVelocityGBuffer)
	{
		return nullptr;
	}
	
	return GBufferVelocity;
}

IPooledRenderTarget* FSceneRenderTargets::RequestCustomDepth(FRHICommandListImmediate& RHICmdList, bool bPrimitives)
{
	int Value = CVarCustomDepth.GetValueOnRenderThread();

	if((Value == 1  && bPrimitives) || Value == 2 || IsCustomDepthPassWritingStencil())
	{
		if (!CustomStencilSRV.GetReference() || !CustomDepth.GetReference() || BufferSize != CustomDepth->GetDesc().Extent)
		{
			// Todo: Could check if writes stencil here and create min viable target
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_DepthStencil, FClearValueBinding::DepthFar, TexCreate_None, TexCreate_DepthStencilTargetable, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, CustomDepth, TEXT("CustomDepth"));
			CustomStencilSRV = RHICreateShaderResourceView((FTexture2DRHIRef&)CustomDepth->GetRenderTargetItem().TargetableTexture, 0, 1, PF_X24_G8);
		}
		return CustomDepth;
	}

	return 0;
}

bool FSceneRenderTargets::IsCustomDepthPassWritingStencil() const
{
	return (CVarCustomDepth.GetValueOnRenderThread() == 3);
}

/** Returns an index in the range [0, NumCubeShadowDepthSurfaces) given an input resolution. */
int32 FSceneRenderTargets::GetCubeShadowDepthZIndex(int32 ShadowResolution) const
{
	static auto CVarMinShadowResolution = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.MinResolution"));
	FIntPoint ObjectShadowBufferResolution = GetShadowDepthTextureResolution();

	// Use a lower resolution because cubemaps use a lot of memory
	ObjectShadowBufferResolution.X /= 2;
	ObjectShadowBufferResolution.Y /= 2;
	const int32 SurfaceSizes[NumCubeShadowDepthSurfaces] =
	{
		ObjectShadowBufferResolution.X,
		ObjectShadowBufferResolution.X / 2,
		ObjectShadowBufferResolution.X / 4,
		ObjectShadowBufferResolution.X / 8,
		CVarMinShadowResolution->GetValueOnRenderThread()
	};

	for (int32 SearchIndex = 0; SearchIndex < NumCubeShadowDepthSurfaces; SearchIndex++)
	{
		if (ShadowResolution >= SurfaceSizes[SearchIndex])
		{
			return SearchIndex;
		}
	}

	check(0);
	return 0;
}

/** Returns the appropriate resolution for a given cube shadow index. */
int32 FSceneRenderTargets::GetCubeShadowDepthZResolution(int32 ShadowIndex) const
{
	checkSlow(ShadowIndex >= 0 && ShadowIndex < NumCubeShadowDepthSurfaces);

	static auto CVarMinShadowResolution = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.MinResolution"));
	FIntPoint ObjectShadowBufferResolution = GetShadowDepthTextureResolution();

	// Use a lower resolution because cubemaps use a lot of memory
	ObjectShadowBufferResolution.X = FMath::Max(ObjectShadowBufferResolution.X / 2, 1);
	ObjectShadowBufferResolution.Y = FMath::Max(ObjectShadowBufferResolution.Y / 2, 1);
	const int32 SurfaceSizes[NumCubeShadowDepthSurfaces] =
	{
		ObjectShadowBufferResolution.X,
		FMath::Max(ObjectShadowBufferResolution.X / 2, 1),
		FMath::Max(ObjectShadowBufferResolution.X / 4, 1),
		FMath::Max(ObjectShadowBufferResolution.X / 8, 1),
		CVarMinShadowResolution->GetValueOnRenderThread()
	};
	return SurfaceSizes[ShadowIndex];
}

bool FSceneRenderTargets::AreShadingPathRenderTargetsAllocated(EShadingPath InShadingPath) const
{
	switch (InShadingPath)
	{
	case EShadingPath::Forward:
		{
			return (SceneColor[(int32)EShadingPath::Forward] != nullptr);
		}
	case EShadingPath::Deferred:
		{
			return (ShadowDepthZ != nullptr);
		}
	default:
		{
			checkNoEntry();
			return false;
		}
	}
}

/*-----------------------------------------------------------------------------
FSceneTextureShaderParameters
-----------------------------------------------------------------------------*/

//
void FSceneTextureShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	// only used if Material has an expression that requires SceneColorTexture
	SceneColorTextureParameter.Bind(ParameterMap,TEXT("SceneColorTexture"));
	SceneColorTextureParameterSampler.Bind(ParameterMap,TEXT("SceneColorTextureSampler"));
	// only used if Material has an expression that requires SceneDepthTexture
	SceneDepthTextureParameter.Bind(ParameterMap,TEXT("SceneDepthTexture"));
	SceneDepthTextureParameterSampler.Bind(ParameterMap,TEXT("SceneDepthTextureSampler"));
	// Only used if Material has an expression that requires SceneAlphaCopyTexture
	SceneAlphaCopyTextureParameter.Bind(ParameterMap,TEXT("SceneAlphaCopyTexture"));
	SceneAlphaCopyTextureParameterSampler.Bind(ParameterMap,TEXT("SceneAlphaCopyTextureSampler"));
	//
	SceneDepthTextureNonMS.Bind(ParameterMap,TEXT("SceneDepthTextureNonMS"));
	SceneColorSurfaceParameter.Bind(ParameterMap,TEXT("SceneColorSurface"));
	// only used if Material has an expression that requires SceneColorTextureMSAA
	SceneDepthSurfaceParameter.Bind(ParameterMap,TEXT("SceneDepthSurface"));
	DirectionalOcclusionSampler.Bind(ParameterMap, TEXT("DirectionalOcclusionSampler"));
	DirectionalOcclusionTexture.Bind(ParameterMap, TEXT("DirectionalOcclusionTexture"));
}

template< typename ShaderRHIParamRef, typename TRHICmdList >
void FSceneTextureShaderParameters::Set(
	TRHICmdList& RHICmdList,
	const ShaderRHIParamRef& ShaderRHI,
	const FSceneView& View,
	ESceneRenderTargetsMode::Type TextureMode,
	ESamplerFilter ColorFilter ) const
{
	if (TextureMode == ESceneRenderTargetsMode::SetTextures)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// optimization possible: TShaderRHIParamRef is no param Ref
		if (SceneColorTextureParameter.IsBound())
		{
			FSamplerStateRHIRef Filter;
			switch ( ColorFilter )
			{
			case SF_Bilinear:
				Filter = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
				break;
			case SF_Trilinear:
				Filter = TStaticSamplerState<SF_Trilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
				break;
			case SF_AnisotropicPoint:
				Filter = TStaticSamplerState<SF_AnisotropicPoint,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
				break;
			case SF_AnisotropicLinear:
				Filter = TStaticSamplerState<SF_AnisotropicLinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
				break;
			case SF_Point:
			default:
				Filter = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
				break;
			}

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				SceneColorTextureParameter,
				SceneColorTextureParameterSampler,
				Filter,
				SceneContext.GetSceneColorTexture()
				);
		}

		if (SceneAlphaCopyTextureParameter.IsBound() && SceneContext.HasSceneAlphaCopyTexture())
		{
			FSamplerStateRHIRef Filter;
			Filter = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				SceneAlphaCopyTextureParameter,
				SceneAlphaCopyTextureParameterSampler,
				Filter,
				SceneContext.GetSceneAlphaCopyTexture()
				);
		}

		if(SceneDepthTextureParameter.IsBound() || SceneDepthTextureParameterSampler.IsBound())
		{
			const FTexture2DRHIRef* DepthTexture = SceneContext.GetActualDepthTexture();

			if (SceneContext.IsSeparateTranslucencyPass() && SceneContext.IsSeparateTranslucencyDepthValid())
			{
				FIntPoint OutScaledSize;
				float OutScale;
				SceneContext.GetSeparateTranslucencyDimensions(OutScaledSize, OutScale);

				if (OutScale < 1.0f)
				{
					DepthTexture = &SceneContext.GetSeparateTranslucencyDepthSurface();
				}
			}

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				SceneDepthTextureParameter,
				SceneDepthTextureParameterSampler,
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				*DepthTexture
				);
		}

		const auto FeatureLevel = View.GetFeatureLevel();

		if (FeatureLevel >= ERHIFeatureLevel::SM5)
		{
			SetTextureParameter(RHICmdList, ShaderRHI, SceneColorSurfaceParameter, SceneContext.GetSceneColorSurface());
		}
		if (FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			if(GSupportsDepthFetchDuringDepthTest)
			{
				if(SceneDepthSurfaceParameter.IsBound())
				{
					SetTextureParameter(RHICmdList, ShaderRHI, SceneDepthSurfaceParameter, SceneContext.GetSceneDepthSurface());
				}
				if(SceneDepthTextureNonMS.IsBound())
				{
					SetTextureParameter(RHICmdList, ShaderRHI, SceneDepthTextureNonMS, SceneContext.GetSceneDepthTexture());
				}
			}
			else
			{
				if(SceneDepthSurfaceParameter.IsBound())
				{
					SetTextureParameter(RHICmdList, ShaderRHI, SceneDepthSurfaceParameter, SceneContext.GetAuxiliarySceneDepthSurface());
				}
				if(SceneDepthTextureNonMS.IsBound())
				{
					SetTextureParameter(RHICmdList, ShaderRHI, SceneDepthTextureNonMS, SceneContext.GetAuxiliarySceneDepthSurface());
				}
			}
		}
	}
	else if (TextureMode == ESceneRenderTargetsMode::DontSet)
	{
		// Verify that none of these were bound if we were told not to set them
		ensure(!SceneColorTextureParameter.IsBound()
			&& !SceneDepthTextureParameter.IsBound()
			&& !SceneColorSurfaceParameter.IsBound()
			&& !SceneDepthSurfaceParameter.IsBound()
			&& !SceneDepthTextureNonMS.IsBound());
	}
	else if (TextureMode == ESceneRenderTargetsMode::DontSetIgnoreBoundByEditorCompositing)
	{
		// Verify that none of these were bound if we were told not to set them
		// ignore SceneDepthTextureNonMS
		ensure(!SceneColorTextureParameter.IsBound()
			&& !SceneDepthTextureParameter.IsBound()
			&& !SceneColorSurfaceParameter.IsBound()
			&& !SceneDepthSurfaceParameter.IsBound());
	}

	if( DirectionalOcclusionSampler.IsBound() )
	{
		bool bDirectionalOcclusion = false;
		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		if(ViewState && ViewState->GetLightPropagationVolume(View.GetFeatureLevel()))
		{
			const FLightPropagationVolumeSettings& LPVSettings = View.FinalPostProcessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();

			bDirectionalOcclusion = LPVSettings.LPVIntensity > 0.0f && LPVSettings.LPVDirectionalOcclusionIntensity > 0.0001f;
		}

		FTextureRHIParamRef DirectionalOcclusion = nullptr;
		if( bDirectionalOcclusion )
		{
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
			DirectionalOcclusion = SceneContext.GetDirectionalOcclusionTexture();
		}
		else
		{
			DirectionalOcclusion = GWhiteTexture->TextureRHI;
		}

		FSamplerStateRHIRef Filter;
		Filter = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DirectionalOcclusionTexture,
			DirectionalOcclusionSampler,
			Filter,
			DirectionalOcclusion
			);
	}
}

#define IMPLEMENT_SCENE_TEXTURE_PARAM_SET( ShaderRHIParamRef ) \
	template void FSceneTextureShaderParameters::Set< ShaderRHIParamRef >( \
		FRHICommandList& RHICmdList,				\
		const ShaderRHIParamRef& ShaderRHI,			\
		const FSceneView& View,						\
		ESceneRenderTargetsMode::Type TextureMode,	\
		ESamplerFilter ColorFilter					\
	) const;

IMPLEMENT_SCENE_TEXTURE_PARAM_SET( FVertexShaderRHIParamRef );
IMPLEMENT_SCENE_TEXTURE_PARAM_SET( FHullShaderRHIParamRef );
IMPLEMENT_SCENE_TEXTURE_PARAM_SET( FDomainShaderRHIParamRef );
IMPLEMENT_SCENE_TEXTURE_PARAM_SET( FGeometryShaderRHIParamRef );
IMPLEMENT_SCENE_TEXTURE_PARAM_SET( FPixelShaderRHIParamRef );
IMPLEMENT_SCENE_TEXTURE_PARAM_SET( FComputeShaderRHIParamRef );

FArchive& operator<<(FArchive& Ar,FSceneTextureShaderParameters& Parameters)
{
	Ar << Parameters.SceneColorTextureParameter;
	Ar << Parameters.SceneColorTextureParameterSampler;
	Ar << Parameters.SceneAlphaCopyTextureParameter;
	Ar << Parameters.SceneAlphaCopyTextureParameterSampler;
	Ar << Parameters.SceneColorSurfaceParameter;
	Ar << Parameters.SceneDepthTextureParameter;
	Ar << Parameters.SceneDepthTextureParameterSampler;
	Ar << Parameters.SceneDepthSurfaceParameter;
	Ar << Parameters.SceneDepthTextureNonMS;
	Ar << Parameters.DirectionalOcclusionSampler;
	Ar << Parameters.DirectionalOcclusionTexture;
	return Ar;
}

// Note this is not just for Deferred rendering, it also applies to mobile forward rendering.
void FDeferredPixelShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	SceneTextureParameters.Bind(ParameterMap);
	
	GBufferResources.Bind(ParameterMap,TEXT("GBuffers"));
	DBufferATextureMS.Bind(ParameterMap,TEXT("DBufferATextureMS"));
	DBufferBTextureMS.Bind(ParameterMap,TEXT("DBufferBTextureMS"));
	DBufferCTextureMS.Bind(ParameterMap,TEXT("DBufferCTextureMS"));
	ScreenSpaceAOTextureMS.Bind(ParameterMap,TEXT("ScreenSpaceAOTextureMS"));
	DBufferATextureNonMS.Bind(ParameterMap,TEXT("DBufferATextureNonMS"));
	DBufferBTextureNonMS.Bind(ParameterMap,TEXT("DBufferBTextureNonMS"));
	DBufferCTextureNonMS.Bind(ParameterMap,TEXT("DBufferCTextureNonMS"));
	ScreenSpaceAOTextureNonMS.Bind(ParameterMap,TEXT("ScreenSpaceAOTextureNonMS"));
	CustomDepthTextureNonMS.Bind(ParameterMap,TEXT("CustomDepthTextureNonMS"));
	DBufferATexture.Bind(ParameterMap,TEXT("DBufferATexture"));
	DBufferATextureSampler.Bind(ParameterMap,TEXT("DBufferATextureSampler"));
	DBufferBTexture.Bind(ParameterMap,TEXT("DBufferBTexture"));
	DBufferBTextureSampler.Bind(ParameterMap,TEXT("DBufferBTextureSampler"));
	DBufferCTexture.Bind(ParameterMap,TEXT("DBufferCTexture"));
	DBufferCTextureSampler.Bind(ParameterMap,TEXT("DBufferCTextureSampler"));
	ScreenSpaceAOTexture.Bind(ParameterMap,TEXT("ScreenSpaceAOTexture"));
	ScreenSpaceAOTextureSampler.Bind(ParameterMap,TEXT("ScreenSpaceAOTextureSampler"));
	CustomDepthTexture.Bind(ParameterMap,TEXT("CustomDepthTexture"));
	CustomDepthTextureSampler.Bind(ParameterMap,TEXT("CustomDepthTextureSampler"));
	CustomStencilTexture.Bind(ParameterMap,TEXT("CustomStencilTexture"));
}

bool IsDBufferEnabled();

template< typename ShaderRHIParamRef, typename TRHICmdList >
void FDeferredPixelShaderParameters::Set(TRHICmdList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View, ESceneRenderTargetsMode::Type TextureMode) const
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	// This is needed on PC ES2 for SceneAlphaCopy, probably should be refactored for performance.
	SceneTextureParameters.Set(RHICmdList, ShaderRHI, View, TextureMode, SF_Point);

	// if() is purely an optimization and could be removed
	if(IsDBufferEnabled())
	{
		IPooledRenderTarget* DBufferA = SceneContext.DBufferA ? SceneContext.DBufferA : GSystemTextures.BlackDummy;
		IPooledRenderTarget* DBufferB = SceneContext.DBufferB ? SceneContext.DBufferB : GSystemTextures.BlackDummy;
		IPooledRenderTarget* DBufferC = SceneContext.DBufferC ? SceneContext.DBufferC : GSystemTextures.BlackDummy;

		// todo: optimize out when not needed
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferATexture, DBufferATextureSampler, TStaticSamplerState<>::GetRHI(), DBufferA->GetRenderTargetItem().ShaderResourceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferBTexture, DBufferBTextureSampler, TStaticSamplerState<>::GetRHI(), DBufferB->GetRenderTargetItem().ShaderResourceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferCTexture, DBufferCTextureSampler, TStaticSamplerState<>::GetRHI(), DBufferC->GetRenderTargetItem().ShaderResourceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferATextureMS, DBufferA->GetRenderTargetItem().TargetableTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferBTextureMS, DBufferB->GetRenderTargetItem().TargetableTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferCTextureMS, DBufferC->GetRenderTargetItem().TargetableTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferATextureNonMS, DBufferA->GetRenderTargetItem().ShaderResourceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferBTextureNonMS, DBufferB->GetRenderTargetItem().ShaderResourceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, DBufferCTextureNonMS, DBufferC->GetRenderTargetItem().ShaderResourceTexture);
	}

	const auto FeatureLevel = View.GetFeatureLevel();

	if (TextureMode == ESceneRenderTargetsMode::SetTextures && FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		// if there is no ambient occlusion it's better to have white there
		IPooledRenderTarget* ScreenSpaceAO = SceneContext.ScreenSpaceAO;
		if(!SceneContext.bScreenSpaceAOIsValid)
		{
			ScreenSpaceAO = GSystemTextures.WhiteDummy;
		}

		// if there is no custom depth it's better to have the far distance there
		IPooledRenderTarget* CustomDepth = SceneContext.bCustomDepthIsValid ? SceneContext.CustomDepth.GetReference() : 0;
		if(!CustomDepth)
		{
			CustomDepth = GSystemTextures.BlackDummy;
		}

		if (FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			if (GBufferResources.IsBound())
			{
				SetUniformBufferParameter(RHICmdList, ShaderRHI, GBufferResources, SceneContext.GetGBufferResourcesUniformBuffer());
			}

			SetTextureParameter(RHICmdList, ShaderRHI, ScreenSpaceAOTexture, ScreenSpaceAOTextureSampler, TStaticSamplerState<>::GetRHI(), ScreenSpaceAO->GetRenderTargetItem().ShaderResourceTexture);
			SetTextureParameter(RHICmdList, ShaderRHI, ScreenSpaceAOTextureMS, ScreenSpaceAO->GetRenderTargetItem().TargetableTexture);
			SetTextureParameter(RHICmdList, ShaderRHI, ScreenSpaceAOTextureNonMS, ScreenSpaceAO->GetRenderTargetItem().ShaderResourceTexture);

			SetTextureParameter(RHICmdList, ShaderRHI, CustomDepthTexture, CustomDepthTextureSampler, TStaticSamplerState<>::GetRHI(), CustomDepth->GetRenderTargetItem().ShaderResourceTexture);
			SetTextureParameter(RHICmdList, ShaderRHI, CustomDepthTextureNonMS, CustomDepth->GetRenderTargetItem().ShaderResourceTexture);

			if (CustomStencilTexture.IsBound())
			{
				if (SceneContext.bCustomDepthIsValid && SceneContext.CustomStencilSRV.GetReference())
				{
					SetSRVParameter(RHICmdList, ShaderRHI, CustomStencilTexture, SceneContext.CustomStencilSRV);
				}
				else
				{
					SetTextureParameter(RHICmdList, ShaderRHI, CustomStencilTexture, GSystemTextures.BlackDummy->GetRenderTargetItem().ShaderResourceTexture);
				}
			}
		}
	}
	else if (TextureMode == ESceneRenderTargetsMode::DontSet ||
		TextureMode == ESceneRenderTargetsMode::DontSetIgnoreBoundByEditorCompositing)
	{
		// Verify that none of these are actually bound
		checkSlow(!GBufferResources.IsBound());
	}
}

#define IMPLEMENT_DEFERRED_PARAMETERS_SET( ShaderRHIParamRef, TRHICmdList ) \
	template void FDeferredPixelShaderParameters::Set< ShaderRHIParamRef, TRHICmdList >(\
		TRHICmdList& RHICmdList,				\
		const ShaderRHIParamRef ShaderRHI,			\
		const FSceneView& View,						\
		ESceneRenderTargetsMode::Type TextureMode	\
	) const;

IMPLEMENT_DEFERRED_PARAMETERS_SET( FVertexShaderRHIParamRef, FRHICommandList );
IMPLEMENT_DEFERRED_PARAMETERS_SET( FHullShaderRHIParamRef, FRHICommandList );
IMPLEMENT_DEFERRED_PARAMETERS_SET( FDomainShaderRHIParamRef, FRHICommandList );
IMPLEMENT_DEFERRED_PARAMETERS_SET( FGeometryShaderRHIParamRef, FRHICommandList );
IMPLEMENT_DEFERRED_PARAMETERS_SET( FPixelShaderRHIParamRef, FRHICommandList );
IMPLEMENT_DEFERRED_PARAMETERS_SET( FComputeShaderRHIParamRef, FRHICommandList );

IMPLEMENT_DEFERRED_PARAMETERS_SET(FVertexShaderRHIParamRef, FRHICommandListImmediate);
IMPLEMENT_DEFERRED_PARAMETERS_SET(FHullShaderRHIParamRef, FRHICommandListImmediate);
IMPLEMENT_DEFERRED_PARAMETERS_SET(FDomainShaderRHIParamRef, FRHICommandListImmediate);
IMPLEMENT_DEFERRED_PARAMETERS_SET(FGeometryShaderRHIParamRef, FRHICommandListImmediate);
IMPLEMENT_DEFERRED_PARAMETERS_SET(FPixelShaderRHIParamRef, FRHICommandListImmediate);
IMPLEMENT_DEFERRED_PARAMETERS_SET(FComputeShaderRHIParamRef, FRHICommandListImmediate);
IMPLEMENT_DEFERRED_PARAMETERS_SET(FComputeShaderRHIParamRef, FRHIAsyncComputeCommandListImmediate );

FArchive& operator<<(FArchive& Ar,FDeferredPixelShaderParameters& Parameters)
{
	Ar << Parameters.SceneTextureParameters;

	Ar << Parameters.GBufferResources;
	Ar << Parameters.DBufferATextureMS;
	Ar << Parameters.DBufferBTextureMS;
	Ar << Parameters.DBufferCTextureMS;
	Ar << Parameters.ScreenSpaceAOTextureMS;
	Ar << Parameters.DBufferATextureNonMS;
	Ar << Parameters.DBufferBTextureNonMS;
	Ar << Parameters.DBufferCTextureNonMS;
	Ar << Parameters.ScreenSpaceAOTextureNonMS;
	Ar << Parameters.CustomDepthTextureNonMS;
	Ar << Parameters.DBufferATexture;
	Ar << Parameters.DBufferATextureSampler;
	Ar << Parameters.DBufferBTexture;
	Ar << Parameters.DBufferBTextureSampler;
	Ar << Parameters.DBufferCTexture;
	Ar << Parameters.DBufferCTextureSampler;
	Ar << Parameters.ScreenSpaceAOTexture;
	Ar << Parameters.ScreenSpaceAOTextureSampler;
	Ar << Parameters.CustomDepthTexture;
	Ar << Parameters.CustomDepthTextureSampler;
	Ar << Parameters.CustomStencilTexture;

	return Ar;
}
