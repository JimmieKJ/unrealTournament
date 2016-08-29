// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRendering.cpp: Scene rendering.
=============================================================================*/

#include "RendererPrivate.h"
#include "Engine.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "VisualizeTexture.h"
#include "PostProcessEyeAdaptation.h"
#include "CompositionLighting.h"
#include "FXSystem.h"
#include "SceneViewExtension.h"
#include "PostProcessBusyWait.h"
#include "PostProcessCircleDOF.h"
#include "SceneUtils.h"
#include "AtmosphereRendering.h"
#include "Components/PlanarReflectionComponent.h"
#include "Matinee/MatineeActor.h"
#include "ComponentRecreateRenderStateContext.h"
#include "PostProcessSubsurface.h"

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

extern ENGINE_API FLightMap2D* GDebugSelectedLightmap;
extern ENGINE_API UPrimitiveComponent* GDebugSelectedComponent;

DECLARE_FLOAT_COUNTER_STAT(TEXT("Custom Depth"), Stat_GPU_CustomDepth, STATGROUP_GPU);

/**
 * Console variable controlling whether or not occlusion queries are allowed.
 */
static TAutoConsoleVariable<int32> CVarAllowOcclusionQueries(
	TEXT("r.AllowOcclusionQueries"),
	1,
	TEXT("If zero, occlusion queries will not be used to cull primitives."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarDemosaicVposOffset(
	TEXT("r.DemosaicVposOffset"),
	0.0f,
	TEXT("This offset is added to the rasterized position used for demosaic in the ES2 tonemapping shader. It exists to workaround driver bugs on some Android devices that have a half-pixel offset."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRefractionQuality(
	TEXT("r.RefractionQuality"),
	2,
	TEXT("Defines the distorion/refraction quality which allows to adjust for quality or performance.\n")
	TEXT("<=0: off (fastest)\n")
	TEXT("  1: low quality (not yet implemented)\n")
	TEXT("  2: normal quality (default)\n")
	TEXT("  3: high quality (e.g. color fringe, not yet implemented)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarInstancedStereo(
	TEXT("vr.InstancedStereo"),
	0,
	TEXT("0 to disable instanced stereo (default), 1 to enable."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMultiView(
	TEXT("vr.MultiView"),
	0,
	TEXT("0 to disable multi-view instanced stereo, 1 to enable.\n")
	TEXT("Currently only supported by the PS4 RHI."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<float> CVarGeneralPurposeTweak(
	TEXT("r.GeneralPurposeTweak"),
	1.0f,
	TEXT("Useful for low level shader development to get quick iteration time without having to change any c++ code.\n")
	TEXT("Value maps to Frame.GeneralPurposeTweak inside the shaders.\n")
	TEXT("Example usage: Multiplier on some value to tweak, toggle to switch between different algorithms (Default: 1.0)\n")
	TEXT("DON'T USE THIS FOR ANYTHING THAT IS CHECKED IN. Compiled out in SHIPPING to make cheating a bit harder."),
	ECVF_RenderThreadSafe);

// should be changed to BaseColor and Metallic, since some time now UE4 is not using DiffuseColor and SpecularColor any more
static TAutoConsoleVariable<float> CVarDiffuseColorMin(
	TEXT("r.DiffuseColor.Min"),
	0.0f,
	TEXT("Allows quick material test by remapping the diffuse color at 1 to a new value (0..1), Only for non shipping built!\n")
	TEXT("1: (default)"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<float> CVarDiffuseColorMax(
	TEXT("r.DiffuseColor.Max"),
	1.0f,
	TEXT("Allows quick material test by remapping the diffuse color at 1 to a new value (0..1), Only for non shipping built!\n")
	TEXT("1: (default)"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<float> CVarRoughnessMin(
	TEXT("r.Roughness.Min"),
	0.0f,
	TEXT("Allows quick material test by remapping the roughness at 0 to a new value (0..1), Only for non shipping built!\n")
	TEXT("0: (default)"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<float> CVarRoughnessMax(
	TEXT("r.Roughness.Max"),
	1.0f,
	TEXT("Allows quick material test by remapping the roughness at 1 to a new value (0..1), Only for non shipping built!\n")
	TEXT("1: (default)"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<int32> CVarDisplayInternals(
	TEXT("r.DisplayInternals"),
	0,
	TEXT("Allows to enable screen printouts that show the internals on the engine/renderer\n")
	TEXT("This is mostly useful to be able to reason why a screenshots looks different.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: enabled"),
	ECVF_RenderThreadSafe | ECVF_Cheat);
#endif

/**
 * Console variable controlling the maximum number of shadow cascades to render with.
 *   DO NOT READ ON THE RENDERING THREAD. Use FSceneView::MaxShadowCascades.
 */
static TAutoConsoleVariable<int32> CVarMaxShadowCascades(
	TEXT("r.Shadow.CSM.MaxCascades"),
	10,
	TEXT("The maximum number of cascades with which to render dynamic directional light shadows."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarTessellationAdaptivePixelsPerTriangle(
	TEXT("r.TessellationAdaptivePixelsPerTriangle"),
	48.0f,
	TEXT("Global tessellation factor multiplier"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarForwardShading(
	TEXT("r.ForwardShading"),
	0,
	TEXT("Whether to use forward shading on desktop platforms - requires Shader Model 5 hardware.\n")
	TEXT("Forward shading has lower constant cost, but fewer features supported. 0:off, 1:on\n")
	TEXT("This rendering path is a work in progress with many unimplemented features, notably only a single reflection capture is applied per object and no translucency dynamic shadow receiving."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarSupportSimpleForwardShading(
	TEXT("r.SupportSimpleForwardShading"),
	0,
	TEXT("Whether to compile the shaders to support r.SimpleForwardShading being enabled (PC only)."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarSimpleForwardShading(
	TEXT("r.SimpleForwardShading"),
	0,
	TEXT("Whether to use the simple forward shading base pass shaders which only support lightmaps + stationary directional light + stationary skylight\n")
	TEXT("All other lighting features are disabled when true.  This is useful for supporting very low end hardware, and is only supported on PC platforms.\n")
	TEXT("0:off, 1:on"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarNormalCurvatureToRoughnessBias(
	TEXT("r.NormalCurvatureToRoughnessBias"),
	0.0f,
	TEXT("Biases the roughness resulting from screen space normal changes for materials with NormalCurvatureToRoughness enabled.  Valid range [-1, 1]"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarNormalCurvatureToRoughnessScale(
	TEXT("r.NormalCurvatureToRoughnessScale"),
	1.0f,
	TEXT("Scales the roughness resulting from screen space normal changes for materials with NormalCurvatureToRoughness enabled.  Valid range [0, 2]"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

/*-----------------------------------------------------------------------------
	FParallelCommandListSet
-----------------------------------------------------------------------------*/


static TAutoConsoleVariable<int32> CVarRHICmdSpewParallelListBalance(
	TEXT("r.RHICmdSpewParallelListBalance"),
	0,
	TEXT("For debugging, spews the size of the parallel command lists. This stalls and otherwise wrecks performance.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: enabled (default)"));

static TAutoConsoleVariable<int32> CVarRHICmdBalanceParallelLists(
	TEXT("r.RHICmdBalanceParallelLists"),
	2,
	TEXT("Allows to enable a preprocess of the drawlists to try to balance the load equally among the command lists.\n")
	TEXT(" 0: off \n")
	TEXT(" 1: enabled")
	TEXT(" 2: experiemental, uses previous frame results (does not do anything in split screen etc)"));

static TAutoConsoleVariable<int32> CVarRHICmdMinCmdlistForParallelSubmit(
	TEXT("r.RHICmdMinCmdlistForParallelSubmit"),
	2,
	TEXT("Minimum number of parallel translate command lists to submit. If there are fewer than this number, they just run on the RHI thread and immediate context."));

static TAutoConsoleVariable<int32> CVarRHICmdMinDrawsPerParallelCmdList(
	TEXT("r.RHICmdMinDrawsPerParallelCmdList"),
	64,
	TEXT("The minimum number of draws per cmdlist. If the total number of draws is less than this, then no parallel work will be done at all. This can't always be honored or done correctly. More effective with RHICmdBalanceParallelLists."));

static FParallelCommandListSet* GOutstandingParallelCommandListSet = nullptr;

FParallelCommandListSet::FParallelCommandListSet(TStatId InExecuteStat, const FViewInfo& InView, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext)
	: View(InView)
	, ParentCmdList(InParentCmdList)
	, Snapshot(nullptr)
	, ExecuteStat(InExecuteStat)
	, NumAlloc(0)
	, bParallelExecute(GRHISupportsParallelRHIExecute && bInParallelExecute)
	, bCreateSceneContext(bInCreateSceneContext)
{
	Width = CVarRHICmdWidth.GetValueOnRenderThread();
	MinDrawsPerCommandList = CVarRHICmdMinDrawsPerParallelCmdList.GetValueOnRenderThread();
	bSpewBalance = !!CVarRHICmdSpewParallelListBalance.GetValueOnRenderThread();
	int32 IntBalance = CVarRHICmdBalanceParallelLists.GetValueOnRenderThread();
	bBalanceCommands = !!IntBalance;
	bBalanceCommandsWithLastFrame = IntBalance > 1;
	CommandLists.Reserve(Width * 8);
	Events.Reserve(Width * 8);
	NumDrawsIfKnown.Reserve(Width * 8);
	check(!GOutstandingParallelCommandListSet);
	GOutstandingParallelCommandListSet = this;
}

FRHICommandList* FParallelCommandListSet::AllocCommandList()
{
	NumAlloc++;
	return new FRHICommandList;
}

void FParallelCommandListSet::Dispatch(bool bHighPriority)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FParallelCommandListSet_Dispatch);
	check(IsInRenderingThread() && FMemStack::Get().GetNumMarks() == 1); // we do not want this popped before the end of the scene and it better be the scene allocator
	check(CommandLists.Num() == Events.Num());
	check(CommandLists.Num() == NumAlloc);
	if (bSpewBalance)
	{
		// finish them all
		for (auto& Event : Events)
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Event, ENamedThreads::RenderThread_Local);
		}
		// spew sizes
		int32 Index = 0;
		for (auto CmdList : CommandLists)
		{
			UE_LOG(LogTemp, Display, TEXT("CmdList %2d/%2d  : %8dKB"), Index, CommandLists.Num(), (CmdList->GetUsedMemory() + 1023) / 1024);
			Index++;
		}
	}
	bool bActuallyDoParallelTranslate = bParallelExecute && CommandLists.Num() >= CVarRHICmdMinCmdlistForParallelSubmit.GetValueOnRenderThread();
	if (bActuallyDoParallelTranslate)
	{
		int32 Total = 0;
		bool bIndeterminate = false;
		for (int32 Count : NumDrawsIfKnown)
		{
			if (Count < 0)
			{
				bIndeterminate = true;
				break; // can't determine how many are in this one; assume we should run parallel translate
			}
			Total += Count;
		}
		if (!bIndeterminate && Total < MinDrawsPerCommandList)
		{
			UE_CLOG(bSpewBalance, LogTemp, Display, TEXT("Disabling parallel translate because the number of draws is known to be small."));
			bActuallyDoParallelTranslate = false;
		}
	}

	if (bActuallyDoParallelTranslate)
	{
		UE_CLOG(bSpewBalance, LogTemp, Display, TEXT("%d cmdlists for parallel translate"), CommandLists.Num());
		check(GRHISupportsParallelRHIExecute);
		NumAlloc -= CommandLists.Num();
		ParentCmdList.QueueParallelAsyncCommandListSubmit(&Events[0], bHighPriority, &CommandLists[0], &NumDrawsIfKnown[0], CommandLists.Num(), (MinDrawsPerCommandList * 4) / 3, bSpewBalance);
		SetStateOnCommandList(ParentCmdList);
	}
	else
	{
		UE_CLOG(bSpewBalance, LogTemp, Display, TEXT("%d cmdlists (no parallel translate desired)"), CommandLists.Num());
		for (int32 Index = 0; Index < CommandLists.Num(); Index++)
		{
			ParentCmdList.QueueAsyncCommandListSubmit(Events[Index], CommandLists[Index]);
			NumAlloc--;
		}
	}
	CommandLists.Reset();
	Snapshot = nullptr;
	Events.Reset();
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FParallelCommandListSet_Dispatch_ServiceLocalQueue);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::RenderThread_Local);
}

FParallelCommandListSet::~FParallelCommandListSet()
{
	check(GOutstandingParallelCommandListSet == this);
	GOutstandingParallelCommandListSet = nullptr;

	check(IsInRenderingThread() && FMemStack::Get().GetNumMarks() == 1); // we do not want this popped before the end of the scene and it better be the scene allocator
	checkf(CommandLists.Num() == 0, TEXT("Derived class of FParallelCommandListSet did not call Dispatch in virtual destructor"));
	checkf(NumAlloc == 0, TEXT("Derived class of FParallelCommandListSet did not call Dispatch in virtual destructor"));
}

FRHICommandList* FParallelCommandListSet::NewParallelCommandList()
{
	FRHICommandList* Result = AllocCommandList();
	Result->ExecuteStat = ExecuteStat;
	SetStateOnCommandList(*Result);
	if (bCreateSceneContext)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(ParentCmdList);
		check(&SceneContext == &FSceneRenderTargets::Get_FrameConstantsOnly()); // the immediate should not have an overridden context
		if (!Snapshot)
		{
			Snapshot = SceneContext.CreateSnapshot(View);
		}
		Snapshot->SetSnapshotOnCmdList(*Result);
		check(&SceneContext != &FSceneRenderTargets::Get(*Result)); // the new commandlist should have a snapshot
	}
	return Result;
}

void FParallelCommandListSet::AddParallelCommandList(FRHICommandList* CmdList, FGraphEventRef& CompletionEvent, int32 InNumDrawsIfKnown)
{
	check(IsInRenderingThread() && FMemStack::Get().GetNumMarks() == 1); // we do not want this popped before the end of the scene and it better be the scene allocator
	check(CommandLists.Num() == Events.Num());
	CommandLists.Add(CmdList);
	Events.Add(CompletionEvent);
	NumDrawsIfKnown.Add(InNumDrawsIfKnown);
}

void FParallelCommandListSet::WaitForTasks()
{
	if (GOutstandingParallelCommandListSet)
	{
		GOutstandingParallelCommandListSet->WaitForTasksInternal();
	}
}

void FParallelCommandListSet::WaitForTasksInternal()
{
	check(IsInRenderingThread());
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FParallelCommandListSet_WaitForTasks);
	FGraphEventArray WaitOutstandingTasks;
	for (int32 Index = 0; Index < Events.Num(); Index++)
	{
		if (!Events[Index]->IsComplete())
		{
			WaitOutstandingTasks.Add(Events[Index]);
		}
	}
	if (WaitOutstandingTasks.Num())
	{
		check(!FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local));
		FTaskGraphInterface::Get().WaitUntilTasksComplete(WaitOutstandingTasks, ENamedThreads::RenderThread_Local);
	}
}



/*-----------------------------------------------------------------------------
	FViewInfo
-----------------------------------------------------------------------------*/

/** 
 * Initialization constructor. Passes all parameters to FSceneView constructor
 */
FViewInfo::FViewInfo(const FSceneViewInitOptions& InitOptions)
	:	FSceneView(InitOptions)
	,	IndividualOcclusionQueries((FSceneViewState*)InitOptions.SceneViewStateInterface, 1)
	,	GroupedOcclusionQueries((FSceneViewState*)InitOptions.SceneViewStateInterface, FOcclusionQueryBatcher::OccludedPrimitiveQueryBatchSize)
{
	Init();
}

/** 
 * Initialization constructor. 
 * @param InView - copy to init with
 */
FViewInfo::FViewInfo(const FSceneView* InView)
	:	FSceneView(*InView)
	,	IndividualOcclusionQueries((FSceneViewState*)InView->State,1)
	,	GroupedOcclusionQueries((FSceneViewState*)InView->State,FOcclusionQueryBatcher::OccludedPrimitiveQueryBatchSize)
	,	CustomVisibilityQuery(nullptr)
{
	Init();
}

void FViewInfo::Init()
{
	CachedViewUniformShaderParameters = NULL;
	bHasTranslucentViewMeshElements = 0;
	bPrevTransformsReset = false;
	bIgnoreExistingQueries = false;
	bDisableQuerySubmissions = false;
	bDisableDistanceBasedFadeTransitions = false;	
	ShadingModelMaskInView = 0;

	NumVisibleStaticMeshElements = 0;
	PrecomputedVisibilityData = 0;
	bSceneHasDecals = 0;

	bIsViewInfo = true;
	PrevViewProjMatrix.SetIdentity();
	PrevViewRotationProjMatrix.SetIdentity();
	
	bUsesGlobalDistanceField = false;
	bUsesLightingChannels = false;
	bTranslucentSurfaceLighting = false;

	ExponentialFogParameters = FVector4(0,1,1,0);
	ExponentialFogColor = FVector::ZeroVector;
	FogMaxOpacity = 1;
	ExponentialFogParameters3 = FVector2D(0, 0);

	bUseDirectionalInscattering = false;
	DirectionalInscatteringExponent = 0;
	DirectionalInscatteringStartDistance = 0;
	InscatteringLightDirection = FVector(0);
	DirectionalInscatteringColor = FLinearColor(ForceInit);

	for (int32 CascadeIndex = 0; CascadeIndex < TVC_MAX; CascadeIndex++)
	{
		TranslucencyLightingVolumeMin[CascadeIndex] = FVector(0);
		TranslucencyVolumeVoxelSize[CascadeIndex] = 0;
		TranslucencyLightingVolumeSize[CascadeIndex] = FVector(0);
	}

	const int32 MaxShadowCascadeCountUpperBound = GetFeatureLevel() >= ERHIFeatureLevel::SM4 ? 10 : MAX_MOBILE_SHADOWCASCADES;

	MaxShadowCascades = FMath::Clamp<int32>(CVarMaxShadowCascades.GetValueOnAnyThread(), 1, MaxShadowCascadeCountUpperBound);

	ShaderMap = GetGlobalShaderMap(FeatureLevel);

	ViewState = (FSceneViewState*)State;
	bIsSnapshot = false;

	bAllowStencilDither = false;

	ForwardLightingResources = &ForwardLightingResourcesStorage;
}

FViewInfo::~FViewInfo()
{
	for(int32 ResourceIndex = 0;ResourceIndex < DynamicResources.Num();ResourceIndex++)
	{
		DynamicResources[ResourceIndex]->ReleasePrimitiveResource();
	}
	if (CustomVisibilityQuery)
	{
		CustomVisibilityQuery->Release();
	}
}

void FViewInfo::SetupSkyIrradianceEnvironmentMapConstants(FVector4* OutSkyIrradianceEnvironmentMap) const
{
	FScene* Scene = nullptr;

	if (Family->Scene)
	{
		Scene = Family->Scene->GetRenderScene();
	}

	if (Scene 
		&& Scene->SkyLight 
		// Skylights with static lighting already had their diffuse contribution baked into lightmaps
		&& !Scene->SkyLight->bHasStaticLighting
		&& Family->EngineShowFlags.SkyLighting)
	{
		const FSHVectorRGB3& SkyIrradiance = Scene->SkyLight->IrradianceEnvironmentMap;

		const float SqrtPI = FMath::Sqrt(PI);
		const float Coefficient0 = 1.0f / (2 * SqrtPI);
		const float Coefficient1 = FMath::Sqrt(3) / (3 * SqrtPI);
		const float Coefficient2 = FMath::Sqrt(15) / (8 * SqrtPI);
		const float Coefficient3 = FMath::Sqrt(5) / (16 * SqrtPI);
		const float Coefficient4 = .5f * Coefficient2;

		// Pack the SH coefficients in a way that makes applying the lighting use the least shader instructions
		// This has the diffuse convolution coefficients baked in
		// See "Stupid Spherical Harmonics (SH) Tricks"
		OutSkyIrradianceEnvironmentMap[0].X = -Coefficient1 * SkyIrradiance.R.V[3];
		OutSkyIrradianceEnvironmentMap[0].Y = -Coefficient1 * SkyIrradiance.R.V[1];
		OutSkyIrradianceEnvironmentMap[0].Z = Coefficient1 * SkyIrradiance.R.V[2];
		OutSkyIrradianceEnvironmentMap[0].W = Coefficient0 * SkyIrradiance.R.V[0] - Coefficient3 * SkyIrradiance.R.V[6];

		OutSkyIrradianceEnvironmentMap[1].X = -Coefficient1 * SkyIrradiance.G.V[3];
		OutSkyIrradianceEnvironmentMap[1].Y = -Coefficient1 * SkyIrradiance.G.V[1];
		OutSkyIrradianceEnvironmentMap[1].Z = Coefficient1 * SkyIrradiance.G.V[2];
		OutSkyIrradianceEnvironmentMap[1].W = Coefficient0 * SkyIrradiance.G.V[0] - Coefficient3 * SkyIrradiance.G.V[6];

		OutSkyIrradianceEnvironmentMap[2].X = -Coefficient1 * SkyIrradiance.B.V[3];
		OutSkyIrradianceEnvironmentMap[2].Y = -Coefficient1 * SkyIrradiance.B.V[1];
		OutSkyIrradianceEnvironmentMap[2].Z = Coefficient1 * SkyIrradiance.B.V[2];
		OutSkyIrradianceEnvironmentMap[2].W = Coefficient0 * SkyIrradiance.B.V[0] - Coefficient3 * SkyIrradiance.B.V[6];

		OutSkyIrradianceEnvironmentMap[3].X = Coefficient2 * SkyIrradiance.R.V[4];
		OutSkyIrradianceEnvironmentMap[3].Y = -Coefficient2 * SkyIrradiance.R.V[5];
		OutSkyIrradianceEnvironmentMap[3].Z = 3 * Coefficient3 * SkyIrradiance.R.V[6];
		OutSkyIrradianceEnvironmentMap[3].W = -Coefficient2 * SkyIrradiance.R.V[7];

		OutSkyIrradianceEnvironmentMap[4].X = Coefficient2 * SkyIrradiance.G.V[4];
		OutSkyIrradianceEnvironmentMap[4].Y = -Coefficient2 * SkyIrradiance.G.V[5];
		OutSkyIrradianceEnvironmentMap[4].Z = 3 * Coefficient3 * SkyIrradiance.G.V[6];
		OutSkyIrradianceEnvironmentMap[4].W = -Coefficient2 * SkyIrradiance.G.V[7];

		OutSkyIrradianceEnvironmentMap[5].X = Coefficient2 * SkyIrradiance.B.V[4];
		OutSkyIrradianceEnvironmentMap[5].Y = -Coefficient2 * SkyIrradiance.B.V[5];
		OutSkyIrradianceEnvironmentMap[5].Z = 3 * Coefficient3 * SkyIrradiance.B.V[6];
		OutSkyIrradianceEnvironmentMap[5].W = -Coefficient2 * SkyIrradiance.B.V[7];

		OutSkyIrradianceEnvironmentMap[6].X = Coefficient4 * SkyIrradiance.R.V[8];
		OutSkyIrradianceEnvironmentMap[6].Y = Coefficient4 * SkyIrradiance.G.V[8];
		OutSkyIrradianceEnvironmentMap[6].Z = Coefficient4 * SkyIrradiance.B.V[8];
		OutSkyIrradianceEnvironmentMap[6].W = 1;
	}
	else
	{
		FMemory::Memzero(OutSkyIrradianceEnvironmentMap, sizeof(FVector4) * 7);
	}
}

void UpdateNoiseTextureParameters(FViewUniformShaderParameters& ViewUniformShaderParameters)
{
	if (GSystemTextures.PerlinNoiseGradient.GetReference())
	{
		ViewUniformShaderParameters.PerlinNoiseGradientTexture = (FTexture2DRHIRef&)GSystemTextures.PerlinNoiseGradient->GetRenderTargetItem().ShaderResourceTexture;
		SetBlack2DIfNull(ViewUniformShaderParameters.PerlinNoiseGradientTexture);
	}
	check(ViewUniformShaderParameters.PerlinNoiseGradientTexture);
	ViewUniformShaderParameters.PerlinNoiseGradientTextureSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	if (GSystemTextures.PerlinNoise3D.GetReference())
	{
		ViewUniformShaderParameters.PerlinNoise3DTexture = (FTexture3DRHIRef&)GSystemTextures.PerlinNoise3D->GetRenderTargetItem().ShaderResourceTexture;
		SetBlack3DIfNull(ViewUniformShaderParameters.PerlinNoise3DTexture);
	}
	check(ViewUniformShaderParameters.PerlinNoise3DTexture);
	ViewUniformShaderParameters.PerlinNoise3DTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
}

/** Creates the view's uniform buffers given a set of view transforms. */
void FViewInfo::SetupUniformBufferParameters(
	FSceneRenderTargets& SceneContext,
	const FMatrix& EffectiveTranslatedViewMatrix, 
	const FMatrix& EffectiveViewToTranslatedWorld, 
	FBox* OutTranslucentCascadeBoundsArray, 
	int32 NumTranslucentCascades,
	FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	check(Family);

	SetupViewRectUniformBufferParameters(
		SceneContext.GetBufferSizeXY(),
		ViewRect,
		ViewUniformShaderParameters);

	FVector4 LocalDiffuseOverrideParameter = DiffuseOverrideParameter;
	FVector2D LocalRoughnessOverrideParameter = RoughnessOverrideParameter;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		// assuming we have no color in the multipliers
		float MinValue = LocalDiffuseOverrideParameter.X;
		float MaxValue = MinValue + LocalDiffuseOverrideParameter.W;

		float NewMinValue = FMath::Max(MinValue, CVarDiffuseColorMin.GetValueOnRenderThread());
		float NewMaxValue = FMath::Min(MaxValue, CVarDiffuseColorMax.GetValueOnRenderThread());

		LocalDiffuseOverrideParameter.X = LocalDiffuseOverrideParameter.Y = LocalDiffuseOverrideParameter.Z = NewMinValue;
		LocalDiffuseOverrideParameter.W = NewMaxValue - NewMinValue;
	}
	{
		float MinValue = LocalRoughnessOverrideParameter.X;
		float MaxValue = MinValue + LocalRoughnessOverrideParameter.Y;

		float NewMinValue = FMath::Max(MinValue, CVarRoughnessMin.GetValueOnRenderThread());
		float NewMaxValue = FMath::Min(MaxValue, CVarRoughnessMax.GetValueOnRenderThread());

		LocalRoughnessOverrideParameter.X = NewMinValue;
		LocalRoughnessOverrideParameter.Y = NewMaxValue - NewMinValue;
	}

#endif

	const bool bIsUnlitView = !Family->EngineShowFlags.Lighting;

	// Create the view's uniform buffer.
	// TODO: We should use a view and previous view uniform buffer to avoid code duplication and keep consistency

	ViewUniformShaderParameters.TranslatedWorldToClip = ViewMatrices.TranslatedViewProjectionMatrix;
	ViewUniformShaderParameters.WorldToClip = ViewProjectionMatrix;
	ViewUniformShaderParameters.TranslatedWorldToView = EffectiveTranslatedViewMatrix;
	ViewUniformShaderParameters.ViewToTranslatedWorld = EffectiveViewToTranslatedWorld;
	ViewUniformShaderParameters.TranslatedWorldToCameraView = ViewMatrices.TranslatedViewMatrix;
	ViewUniformShaderParameters.CameraViewToTranslatedWorld = ViewUniformShaderParameters.TranslatedWorldToCameraView.Inverse();
	ViewUniformShaderParameters.ViewToClip = ViewMatrices.ProjMatrix;
	ViewUniformShaderParameters.ClipToView = ViewMatrices.GetInvProjMatrix();
	ViewUniformShaderParameters.ClipToTranslatedWorld = ViewMatrices.InvTranslatedViewProjectionMatrix;
	ViewUniformShaderParameters.ViewForward = EffectiveTranslatedViewMatrix.GetColumn(2);
	ViewUniformShaderParameters.ViewUp = EffectiveTranslatedViewMatrix.GetColumn(1);
	ViewUniformShaderParameters.ViewRight = EffectiveTranslatedViewMatrix.GetColumn(0);
	ViewUniformShaderParameters.InvDeviceZToWorldZTransform = InvDeviceZToWorldZTransform;
	ViewUniformShaderParameters.WorldViewOrigin = EffectiveViewToTranslatedWorld.TransformPosition(FVector(0)) - ViewMatrices.PreViewTranslation;
	ViewUniformShaderParameters.WorldCameraOrigin = ViewMatrices.ViewOrigin;
	ViewUniformShaderParameters.TranslatedWorldCameraOrigin = ViewMatrices.ViewOrigin + ViewMatrices.PreViewTranslation;
	ViewUniformShaderParameters.PreViewTranslation = ViewMatrices.PreViewTranslation;
	ViewUniformShaderParameters.PrevProjection = PrevViewMatrices.ProjMatrix;
	ViewUniformShaderParameters.PrevViewProj = PrevViewProjMatrix;
	ViewUniformShaderParameters.PrevViewRotationProj = PrevViewRotationProjMatrix;
	ViewUniformShaderParameters.PrevViewToClip = PrevViewMatrices.ProjMatrix;
	ViewUniformShaderParameters.PrevClipToView = PrevViewMatrices.GetInvProjMatrix();
	ViewUniformShaderParameters.PrevTranslatedWorldToClip = PrevViewMatrices.TranslatedViewProjectionMatrix;
	// EffectiveTranslatedViewMatrix != ViewMatrices.TranslatedViewMatrix in the shadow pass
	// and we don't have EffectiveTranslatedViewMatrix for the previous frame to set up PrevTranslatedWorldToView
	// but that is fine to set up PrevTranslatedWorldToView as same as PrevTranslatedWorldToCameraView
	// since the shadow pass doesn't require previous frame computation.
	ViewUniformShaderParameters.PrevTranslatedWorldToView = PrevViewMatrices.TranslatedViewMatrix;
	ViewUniformShaderParameters.PrevViewToTranslatedWorld = ViewUniformShaderParameters.PrevTranslatedWorldToView.Inverse();
	ViewUniformShaderParameters.PrevTranslatedWorldToCameraView = PrevViewMatrices.TranslatedViewMatrix;
	ViewUniformShaderParameters.PrevCameraViewToTranslatedWorld = ViewUniformShaderParameters.PrevTranslatedWorldToCameraView.Inverse();
	ViewUniformShaderParameters.PrevWorldCameraOrigin = PrevViewMatrices.ViewOrigin;
	// previous view world origin is going to be needed only in the base pass or shadow pass
	// therefore is same as previous camera world origin.
	ViewUniformShaderParameters.PrevWorldViewOrigin = ViewUniformShaderParameters.PrevWorldCameraOrigin;
	ViewUniformShaderParameters.PrevPreViewTranslation = PrevViewMatrices.PreViewTranslation;
	// can be optimized
	ViewUniformShaderParameters.PrevInvViewProj = PrevViewProjMatrix.Inverse();
	ViewUniformShaderParameters.GlobalClippingPlane = FVector4(GlobalClippingPlane.X, GlobalClippingPlane.Y, GlobalClippingPlane.Z, -GlobalClippingPlane.W);

	ViewUniformShaderParameters.FieldOfViewWideAngles = 2.f * ViewMatrices.GetHalfFieldOfViewPerAxis();
	ViewUniformShaderParameters.PrevFieldOfViewWideAngles = 2.f * PrevViewMatrices.GetHalfFieldOfViewPerAxis();
	ViewUniformShaderParameters.DiffuseOverrideParameter = LocalDiffuseOverrideParameter;
	ViewUniformShaderParameters.SpecularOverrideParameter = SpecularOverrideParameter;
	ViewUniformShaderParameters.NormalOverrideParameter = NormalOverrideParameter;
	ViewUniformShaderParameters.RoughnessOverrideParameter = LocalRoughnessOverrideParameter;
	ViewUniformShaderParameters.PrevFrameGameTime = Family->CurrentWorldTime - Family->DeltaWorldTime;
	ViewUniformShaderParameters.PrevFrameRealTime = Family->CurrentRealTime - Family->DeltaWorldTime;
	ViewUniformShaderParameters.WorldCameraMovementSinceLastFrame = ViewMatrices.ViewOrigin - PrevViewMatrices.ViewOrigin;
	ViewUniformShaderParameters.CullingSign = bReverseCulling ? -1.0f : 1.0f;
	ViewUniformShaderParameters.NearPlane = GNearClippingPlane;

	const bool bCheckerboardSubsurfaceRendering = FRCPassPostProcessSubsurface::RequiresCheckerboardSubsurfaceRendering( SceneContext.GetSceneColorFormat() );
	ViewUniformShaderParameters.bCheckerboardSubsurfaceProfileRendering = bCheckerboardSubsurfaceRendering ? 1.0f : 0.0f;

	ViewUniformShaderParameters.ScreenToWorld = FMatrix(
		FPlane(1,0,0,0),
		FPlane(0,1,0,0),
		FPlane(0,0,ProjectionMatrixUnadjustedForRHI.M[2][2],1),
		FPlane(0,0,ProjectionMatrixUnadjustedForRHI.M[3][2],0))
		* InvViewProjectionMatrix;

	ViewUniformShaderParameters.ScreenToTranslatedWorld = FMatrix(
		FPlane(1,0,0,0),
		FPlane(0,1,0,0),
		FPlane(0,0,ProjectionMatrixUnadjustedForRHI.M[2][2],1),
		FPlane(0,0,ProjectionMatrixUnadjustedForRHI.M[3][2],0))
		* ViewMatrices.InvTranslatedViewProjectionMatrix;

	ViewUniformShaderParameters.PrevScreenToTranslatedWorld = FMatrix(
		FPlane(1,0,0,0),
		FPlane(0,1,0,0),
		FPlane(0,0,ProjectionMatrixUnadjustedForRHI.M[2][2],1),
		FPlane(0,0,ProjectionMatrixUnadjustedForRHI.M[3][2],0))
		* PrevViewMatrices.InvTranslatedViewProjectionMatrix;

	FVector DeltaTranslation = PrevViewMatrices.PreViewTranslation - ViewMatrices.PreViewTranslation;
	FMatrix InvViewProj = ViewMatrices.GetInvProjNoAAMatrix() * ViewMatrices.TranslatedViewMatrix.GetTransposed();
	FMatrix PrevViewProj = FTranslationMatrix( DeltaTranslation ) * PrevViewMatrices.TranslatedViewMatrix * PrevViewMatrices.GetProjNoAAMatrix();

	ViewUniformShaderParameters.ClipToPrevClip = InvViewProj * PrevViewProj;

	// is getting clamped in the shader to a value larger than 0 (we don't want the triangles to disappear)
	ViewUniformShaderParameters.AdaptiveTessellationFactor = 0.0f;

	FScene* Scene = nullptr;

	if (Family->Scene)
	{
		Scene = Family->Scene->GetRenderScene();
	}	

	if (Scene)
	{		
		if (Scene->SimpleDirectionalLight)
		{			
			ViewUniformShaderParameters.DirectionalLightColor = Scene->SimpleDirectionalLight->Proxy->GetColor() / PI;
			ViewUniformShaderParameters.DirectionalLightDirection = -Scene->SimpleDirectionalLight->Proxy->GetDirection(); 
		}
		else
		{
			ViewUniformShaderParameters.DirectionalLightColor = FLinearColor::Black;
			ViewUniformShaderParameters.DirectionalLightDirection = FVector::ZeroVector;
		}

		// Atmospheric fog parameters
		if (ShouldRenderAtmosphere(*Family) && Scene->AtmosphericFog)
		{
			ViewUniformShaderParameters.AtmosphericFogSunPower = Scene->AtmosphericFog->SunMultiplier;
			ViewUniformShaderParameters.AtmosphericFogPower = Scene->AtmosphericFog->FogMultiplier;
			ViewUniformShaderParameters.AtmosphericFogDensityScale = Scene->AtmosphericFog->InvDensityMultiplier;
			ViewUniformShaderParameters.AtmosphericFogDensityOffset = Scene->AtmosphericFog->DensityOffset;
			ViewUniformShaderParameters.AtmosphericFogGroundOffset = Scene->AtmosphericFog->GroundOffset;
			ViewUniformShaderParameters.AtmosphericFogDistanceScale = Scene->AtmosphericFog->DistanceScale;
			ViewUniformShaderParameters.AtmosphericFogAltitudeScale = Scene->AtmosphericFog->AltitudeScale;
			ViewUniformShaderParameters.AtmosphericFogHeightScaleRayleigh = Scene->AtmosphericFog->RHeight;
			ViewUniformShaderParameters.AtmosphericFogStartDistance = Scene->AtmosphericFog->StartDistance;
			ViewUniformShaderParameters.AtmosphericFogDistanceOffset = Scene->AtmosphericFog->DistanceOffset;
			ViewUniformShaderParameters.AtmosphericFogSunDiscScale = Scene->AtmosphericFog->SunDiscScale;
			ViewUniformShaderParameters.AtmosphericFogSunColor = Scene->SunLight ? Scene->SunLight->Proxy->GetColor() : Scene->AtmosphericFog->DefaultSunColor;
			ViewUniformShaderParameters.AtmosphericFogSunDirection = Scene->SunLight ? -Scene->SunLight->Proxy->GetDirection() : -Scene->AtmosphericFog->DefaultSunDirection;
			ViewUniformShaderParameters.AtmosphericFogRenderMask = Scene->AtmosphericFog->RenderFlag & (EAtmosphereRenderFlag::E_DisableGroundScattering | EAtmosphereRenderFlag::E_DisableSunDisk);
			ViewUniformShaderParameters.AtmosphericFogInscatterAltitudeSampleNum = Scene->AtmosphericFog->InscatterAltitudeSampleNum;
		}
		else
		{
			ViewUniformShaderParameters.AtmosphericFogSunPower = 0.f;
			ViewUniformShaderParameters.AtmosphericFogPower = 0.f;
			ViewUniformShaderParameters.AtmosphericFogDensityScale = 0.f;
			ViewUniformShaderParameters.AtmosphericFogDensityOffset = 0.f;
			ViewUniformShaderParameters.AtmosphericFogGroundOffset = 0.f;
			ViewUniformShaderParameters.AtmosphericFogDistanceScale = 0.f;
			ViewUniformShaderParameters.AtmosphericFogAltitudeScale = 0.f;
			ViewUniformShaderParameters.AtmosphericFogHeightScaleRayleigh = 0.f;
			ViewUniformShaderParameters.AtmosphericFogStartDistance = 0.f;
			ViewUniformShaderParameters.AtmosphericFogDistanceOffset = 0.f;
			ViewUniformShaderParameters.AtmosphericFogSunDiscScale = 1.f;
			//Added check so atmospheric light color and vector can use a directional light without needing an atmospheric fog actor in the scene
			ViewUniformShaderParameters.AtmosphericFogSunColor = Scene->SunLight ? Scene->SunLight->Proxy->GetColor() : FLinearColor::Black;
			ViewUniformShaderParameters.AtmosphericFogSunDirection = Scene->SunLight ? -Scene->SunLight->Proxy->GetDirection() : FVector::ZeroVector;
			ViewUniformShaderParameters.AtmosphericFogRenderMask = EAtmosphereRenderFlag::E_EnableAll;
			ViewUniformShaderParameters.AtmosphericFogInscatterAltitudeSampleNum = 0;
		}
	}
	else
	{
		// Atmospheric fog parameters
		ViewUniformShaderParameters.AtmosphericFogSunPower = 0.f;
		ViewUniformShaderParameters.AtmosphericFogPower = 0.f;
		ViewUniformShaderParameters.AtmosphericFogDensityScale = 0.f;
		ViewUniformShaderParameters.AtmosphericFogDensityOffset = 0.f;
		ViewUniformShaderParameters.AtmosphericFogGroundOffset = 0.f;
		ViewUniformShaderParameters.AtmosphericFogDistanceScale = 0.f;
		ViewUniformShaderParameters.AtmosphericFogAltitudeScale = 0.f;
		ViewUniformShaderParameters.AtmosphericFogHeightScaleRayleigh = 0.f;
		ViewUniformShaderParameters.AtmosphericFogStartDistance = 0.f;
		ViewUniformShaderParameters.AtmosphericFogDistanceOffset = 0.f;
		ViewUniformShaderParameters.AtmosphericFogSunDiscScale = 1.f;
		ViewUniformShaderParameters.AtmosphericFogSunColor = FLinearColor::Black;
		ViewUniformShaderParameters.AtmosphericFogSunDirection = FVector::ZeroVector;
		ViewUniformShaderParameters.AtmosphericFogRenderMask = EAtmosphereRenderFlag::E_EnableAll;
		ViewUniformShaderParameters.AtmosphericFogInscatterAltitudeSampleNum = 0;
	}

	ViewUniformShaderParameters.AtmosphereTransmittanceTexture_UB = OrBlack2DIfNull(AtmosphereTransmittanceTexture);
	ViewUniformShaderParameters.AtmosphereIrradianceTexture_UB = OrBlack2DIfNull(AtmosphereIrradianceTexture);
	ViewUniformShaderParameters.AtmosphereInscatterTexture_UB = OrBlack3DIfNull(AtmosphereInscatterTexture);

	ViewUniformShaderParameters.AtmosphereTransmittanceTextureSampler_UB = TStaticSamplerState<SF_Bilinear>::GetRHI();
	ViewUniformShaderParameters.AtmosphereIrradianceTextureSampler_UB = TStaticSamplerState<SF_Bilinear>::GetRHI();
	ViewUniformShaderParameters.AtmosphereInscatterTextureSampler_UB = TStaticSamplerState<SF_Bilinear>::GetRHI();

	// Update the Texture Parameters used for noise
	UpdateNoiseTextureParameters(ViewUniformShaderParameters);

	SetupDefaultGlobalDistanceFieldUniformBufferParameters(ViewUniformShaderParameters);

	ViewUniformShaderParameters.UnlitViewmodeMask = bIsUnlitView ? 1 : 0;
	ViewUniformShaderParameters.OutOfBoundsMask = Family->EngineShowFlags.VisualizeOutOfBoundsPixels ? 1 : 0;

	ViewUniformShaderParameters.GameTime = Family->CurrentWorldTime;
	ViewUniformShaderParameters.RealTime = Family->CurrentRealTime;
	ViewUniformShaderParameters.Random = FMath::Rand();
	ViewUniformShaderParameters.FrameNumber = Family->FrameNumber;

	ViewUniformShaderParameters.CameraCut = bCameraCut ? 1 : 0;

	uint32 StateFrameIndexMod8 = 0;

	if(State)
	{
		ViewUniformShaderParameters.TemporalAAParams = FVector4(
			ViewState->GetCurrentTemporalAASampleIndex(), 
			ViewState->GetCurrentTemporalAASampleCount(),
			TemporalJitterPixelsX,
			TemporalJitterPixelsY);

		StateFrameIndexMod8 = ViewState->GetFrameIndexMod8();
	}
	else
	{
		ViewUniformShaderParameters.TemporalAAParams = FVector4(0, 1, 0, 0);
	}

	ViewUniformShaderParameters.StateFrameIndexMod8 = StateFrameIndexMod8;

	{
		// If rendering in stereo, the right eye uses the left eye's translucency lighting volume.
		const FViewInfo* PrimaryView = this;
		if (StereoPass == eSSP_RIGHT_EYE)
		{
			int32 ViewIndex = Family->Views.Find(this);
			if (Family->Views.IsValidIndex(ViewIndex) && Family->Views.IsValidIndex(ViewIndex - 1))
			{
				const FSceneView* LeftEyeView = Family->Views[ViewIndex - 1];
				if (LeftEyeView->bIsViewInfo && LeftEyeView->StereoPass == eSSP_LEFT_EYE)
				{
					PrimaryView = static_cast<const FViewInfo*>(LeftEyeView);
				}
			}
		}
		PrimaryView->CalcTranslucencyLightingVolumeBounds(OutTranslucentCascadeBoundsArray, NumTranslucentCascades);
	}

	for (int32 CascadeIndex = 0; CascadeIndex < NumTranslucentCascades; CascadeIndex++)
	{
		const float VolumeVoxelSize = (OutTranslucentCascadeBoundsArray[CascadeIndex].Max.X - OutTranslucentCascadeBoundsArray[CascadeIndex].Min.X) / GTranslucencyLightingVolumeDim;
		const FVector VolumeSize = OutTranslucentCascadeBoundsArray[CascadeIndex].Max - OutTranslucentCascadeBoundsArray[CascadeIndex].Min;
		ViewUniformShaderParameters.TranslucencyLightingVolumeMin[CascadeIndex] = FVector4(OutTranslucentCascadeBoundsArray[CascadeIndex].Min, 1.0f / GTranslucencyLightingVolumeDim);
		ViewUniformShaderParameters.TranslucencyLightingVolumeInvSize[CascadeIndex] = FVector4(FVector(1.0f) / VolumeSize, VolumeVoxelSize);
	}
	
	// The exposure scale is just a scalar but needs to be a float4 to workaround a driver bug on IOS.
	// After 4.2 we can put the workaround in the cross compiler.
	float ExposureScale = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue( *this );
	ViewUniformShaderParameters.ExposureScale = FVector4(ExposureScale, ExposureScale, ExposureScale, 1.0f);
	ViewUniformShaderParameters.DepthOfFieldFocalDistance = FinalPostProcessSettings.DepthOfFieldFocalDistance;
	ViewUniformShaderParameters.DepthOfFieldSensorWidth = FinalPostProcessSettings.DepthOfFieldSensorWidth;
	ViewUniformShaderParameters.DepthOfFieldFocalRegion = FinalPostProcessSettings.DepthOfFieldFocalRegion;
	// clamped to avoid div by 0 in shader
	ViewUniformShaderParameters.DepthOfFieldNearTransitionRegion = FMath::Max(0.01f, FinalPostProcessSettings.DepthOfFieldNearTransitionRegion);
	// clamped to avoid div by 0 in shader
	ViewUniformShaderParameters.DepthOfFieldFarTransitionRegion = FMath::Max(0.01f, FinalPostProcessSettings.DepthOfFieldFarTransitionRegion);
	ViewUniformShaderParameters.DepthOfFieldScale = FinalPostProcessSettings.DepthOfFieldScale;
	ViewUniformShaderParameters.DepthOfFieldFocalLength = 50.0f;

	ViewUniformShaderParameters.bSubsurfacePostprocessEnabled = GCompositionLighting.IsSubsurfacePostprocessRequired() ? 1.0f : 0.0f;

	{
		// This is the CVar default
		float Value = 1.0f;

		// Compiled out in SHIPPING to make cheating a bit harder.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		Value = CVarGeneralPurposeTweak.GetValueOnRenderThread();
#endif

		ViewUniformShaderParameters.GeneralPurposeTweak = Value;
	}

	ViewUniformShaderParameters.DemosaicVposOffset = 0.0f;
	{
		ViewUniformShaderParameters.DemosaicVposOffset = CVarDemosaicVposOffset.GetValueOnRenderThread();
	}

	ViewUniformShaderParameters.IndirectLightingColorScale = FVector(FinalPostProcessSettings.IndirectLightingColor.R * FinalPostProcessSettings.IndirectLightingIntensity,
		FinalPostProcessSettings.IndirectLightingColor.G * FinalPostProcessSettings.IndirectLightingIntensity,
		FinalPostProcessSettings.IndirectLightingColor.B * FinalPostProcessSettings.IndirectLightingIntensity);

	ViewUniformShaderParameters.NormalCurvatureToRoughnessScaleBias.X = FMath::Clamp(CVarNormalCurvatureToRoughnessScale.GetValueOnAnyThread(), 0.0f, 2.0f);
	ViewUniformShaderParameters.NormalCurvatureToRoughnessScaleBias.Y = FMath::Clamp(CVarNormalCurvatureToRoughnessBias.GetValueOnAnyThread(), -1.0f, 1.0f);

	ViewUniformShaderParameters.RenderingReflectionCaptureMask = bIsReflectionCapture ? 1.0f : 0.0f;

	ViewUniformShaderParameters.AmbientCubemapTint = FinalPostProcessSettings.AmbientCubemapTint;
	ViewUniformShaderParameters.AmbientCubemapIntensity = FinalPostProcessSettings.AmbientCubemapIntensity;

	{
		// Enables HDR encoding mode selection without recompile of all PC shaders during ES2 emulation.
		ViewUniformShaderParameters.HDR32bppEncodingMode = 0;
		if (IsMobileHDR32bpp())
		{
			ViewUniformShaderParameters.HDR32bppEncodingMode = IsMobileHDRMosaic() ? 1.0f : 2.0f;
		}
	}

	ViewUniformShaderParameters.CircleDOFParams = CircleDofHalfCoc(*this);

	if (Family->Scene)
	{
		Scene = Family->Scene->GetRenderScene();
	}

	ERHIFeatureLevel::Type RHIFeatureLevel = Scene == nullptr ? GMaxRHIFeatureLevel : Scene->GetFeatureLevel();

	if (Scene && Scene->SkyLight)
	{
		FSkyLightSceneProxy* SkyLight = Scene->SkyLight;

		ViewUniformShaderParameters.SkyLightColor = SkyLight->LightColor;

		bool bApplyPrecomputedBentNormalShadowing = 
			SkyLight->bCastShadows 
			&& SkyLight->bWantsStaticShadowing
			&& SkyLight->bPrecomputedLightingIsValid;

		ViewUniformShaderParameters.SkyLightParameters = bApplyPrecomputedBentNormalShadowing ? 1 : 0;
	}
	else
	{
		ViewUniformShaderParameters.SkyLightColor = FLinearColor::Black;
		ViewUniformShaderParameters.SkyLightParameters = 0;
	}

	// Make sure there's no padding since we're going to cast to FVector4*
	checkSlow(sizeof(ViewUniformShaderParameters.SkyIrradianceEnvironmentMap) == sizeof(FVector4)* 7);
	SetupSkyIrradianceEnvironmentMapConstants((FVector4*)&ViewUniformShaderParameters.SkyIrradianceEnvironmentMap);

	ViewUniformShaderParameters.MobilePreviewMode =
		(GIsEditor &&
		(RHIFeatureLevel == ERHIFeatureLevel::ES2 || RHIFeatureLevel == ERHIFeatureLevel::ES3_1) &&
		GMaxRHIFeatureLevel > ERHIFeatureLevel::ES3_1) ? 1.0f : 0.0f;

	// Padding between the left and right eye may be introduced by an HMD, which instanced stereo needs to account for.
	if ((Family != nullptr) && (StereoPass != eSSP_FULL) && (Family->Views.Num() > 1))
	{
		check(Family->Views.Num() == 2);
		const float FamilySizeX = static_cast<float>(Family->InstancedStereoWidth);
		const float EyePaddingSize = static_cast<float>(Family->Views[1]->ViewRect.Min.X - Family->Views[0]->ViewRect.Max.X);
		ViewUniformShaderParameters.HMDEyePaddingOffset = (FamilySizeX - EyePaddingSize) / FamilySizeX;
	}
	else
	{
		ViewUniformShaderParameters.HMDEyePaddingOffset = 1.0f;
	}

	ViewUniformShaderParameters.ReflectionCubemapMaxMip = FMath::FloorLog2(UReflectionCaptureComponent::GetReflectionCaptureSize_RenderThread());

	ViewUniformShaderParameters.ShowDecalsMask = Family->EngineShowFlags.Decals ? 1.0f : 0.0f;

	extern int32 GDistanceFieldAOSpecularOcclusionMode;
	ViewUniformShaderParameters.DistanceFieldAOSpecularOcclusionMode = GDistanceFieldAOSpecularOcclusionMode;

	extern FVector2D GetReflectionEnvironmentRoughnessMixingScaleBias();
	ViewUniformShaderParameters.ReflectionEnvironmentRoughnessMixingScaleBias = GetReflectionEnvironmentRoughnessMixingScaleBias();

	ViewUniformShaderParameters.StereoPassIndex = (StereoPass != eSSP_RIGHT_EYE) ? 0 : 1;
}

void FViewInfo::SetupViewRectUniformBufferParameters(
	FIntPoint BufferSize,
	FIntRect EffectiveViewRect,
	FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	checkfSlow(EffectiveViewRect.Area() > 0, TEXT("Invalid-size EffectiveViewRect passed to CreateUniformBufferParameters [%d * %d]."), EffectiveViewRect.Width(), EffectiveViewRect.Height());

	// Calculate the vector used by shaders to convert clip space coordinates to texture space.
	const float InvBufferSizeX = 1.0f / BufferSize.X;
	const float InvBufferSizeY = 1.0f / BufferSize.Y;
	// to bring NDC (-1..1, 1..-1) into 0..1 UV for BufferSize textures
	const FVector4 ScreenPositionScaleBias(
		EffectiveViewRect.Width() * InvBufferSizeX / +2.0f,
		EffectiveViewRect.Height() * InvBufferSizeY / (-2.0f * GProjectionSignY),
		(EffectiveViewRect.Height() / 2.0f + EffectiveViewRect.Min.Y) * InvBufferSizeY,
		(EffectiveViewRect.Width() / 2.0f + EffectiveViewRect.Min.X) * InvBufferSizeX
		);
	
	ViewUniformShaderParameters.ScreenPositionScaleBias = ScreenPositionScaleBias;

	ViewUniformShaderParameters.ViewRectMin = FVector4(EffectiveViewRect.Min.X, EffectiveViewRect.Min.Y, 0.0f, 0.0f);
	ViewUniformShaderParameters.ViewSizeAndInvSize = FVector4(EffectiveViewRect.Width(), EffectiveViewRect.Height(), 1.0f / float(EffectiveViewRect.Width()), 1.0f / float(EffectiveViewRect.Height()));
	ViewUniformShaderParameters.BufferSizeAndInvSize = FVector4(BufferSize.X, BufferSize.Y, InvBufferSizeX, InvBufferSizeY);

	FVector2D OneScenePixelUVSize = FVector2D(1.0f / BufferSize.X, 1.0f / BufferSize.Y);
	FVector4 SceneTexMinMax(	((float)EffectiveViewRect.Min.X / BufferSize.X), 
								((float)EffectiveViewRect.Min.Y / BufferSize.Y), 
								(((float)EffectiveViewRect.Max.X / BufferSize.X) - OneScenePixelUVSize.X) , 
								(((float)EffectiveViewRect.Max.Y / BufferSize.Y) - OneScenePixelUVSize.Y) );

	ViewUniformShaderParameters.SceneTextureMinMax = SceneTexMinMax;

	ViewUniformShaderParameters.MotionBlurNormalizedToPixel = FinalPostProcessSettings.MotionBlurMax * EffectiveViewRect.Width() / 100.0f;

	{
		// setup a matrix to transform float4(SvPosition.xyz,1) directly to TranslatedWorld (quality, performance as we don't need to convert or use interpolator)

		//	new_xy = (xy - ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

		//  transformed into one MAD:  new_xy = xy * ViewSizeAndInvSize.zw * float2(2,-2)      +       (-ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

		float Mx = 2.0f * ViewUniformShaderParameters.ViewSizeAndInvSize.Z;
		float My = -2.0f * ViewUniformShaderParameters.ViewSizeAndInvSize.W;
		float Ax = -1.0f - 2.0f * EffectiveViewRect.Min.X * ViewUniformShaderParameters.ViewSizeAndInvSize.Z;
		float Ay = 1.0f + 2.0f * EffectiveViewRect.Min.Y * ViewUniformShaderParameters.ViewSizeAndInvSize.W;

		// http://stackoverflow.com/questions/9010546/java-transformation-matrix-operations

		ViewUniformShaderParameters.SVPositionToTranslatedWorld = 
			FMatrix(FPlane(Mx,   0,  0,   0),
					FPlane( 0,  My,  0,   0),
					FPlane( 0,   0,  1,   0),
					FPlane(Ax,  Ay,  0,   1)) * ViewMatrices.InvTranslatedViewProjectionMatrix;
	}

	if(Family->EngineShowFlags.Tessellation)
	{
		// CVar setting is pixels/tri which is nice and intuitive.  But we want pixels/tessellated edge.  So use a heuristic.
		float TessellationAdaptivePixelsPerEdge = FMath::Sqrt(2.f * CVarTessellationAdaptivePixelsPerTriangle.GetValueOnRenderThread());

		ViewUniformShaderParameters.AdaptiveTessellationFactor = 0.5f * ViewMatrices.ProjMatrix.M[1][1] * float(EffectiveViewRect.Height()) / TessellationAdaptivePixelsPerEdge;
	}
}

void FViewInfo::InitRHIResources()
{
	FBox VolumeBounds[TVC_MAX];

	/** The view transform, starting from world-space points translated by -ViewOrigin. */
	FMatrix TranslatedViewMatrix = FTranslationMatrix(-ViewMatrices.PreViewTranslation) * ViewMatrices.ViewMatrix;

	check(IsInRenderingThread());

	CachedViewUniformShaderParameters = new FViewUniformShaderParameters();

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(FRHICommandListExecutor::GetImmediateCommandList());

	SetupUniformBufferParameters(
		SceneContext,
		TranslatedViewMatrix,
		InvViewMatrix * FTranslationMatrix(ViewMatrices.PreViewTranslation),
		VolumeBounds,
		TVC_MAX,
		*CachedViewUniformShaderParameters);

	ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(*CachedViewUniformShaderParameters, UniformBuffer_SingleFrame);

	for (int32 CascadeIndex = 0; CascadeIndex < TVC_MAX; CascadeIndex++)
	{
		TranslucencyLightingVolumeMin[CascadeIndex] = VolumeBounds[CascadeIndex].Min;
		TranslucencyVolumeVoxelSize[CascadeIndex] = (VolumeBounds[CascadeIndex].Max.X - VolumeBounds[CascadeIndex].Min.X) / GTranslucencyLightingVolumeDim;
		TranslucencyLightingVolumeSize[CascadeIndex] = VolumeBounds[CascadeIndex].Max - VolumeBounds[CascadeIndex].Min;
	}

	// Initialize the dynamic resources used by the view's FViewElementDrawer.
	for(int32 ResourceIndex = 0;ResourceIndex < DynamicResources.Num();ResourceIndex++)
	{
		DynamicResources[ResourceIndex]->InitPrimitiveResource();
	}
}

// These are not real view infos, just dumb memory blocks
static TArray<FViewInfo*> ViewInfoSnapshots;
// these are never freed, even at program shutdown
static TArray<FViewInfo*> FreeViewInfoSnapshots;

FViewInfo* FViewInfo::CreateSnapshot() const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FViewInfo_CreateSnapshot);

	check(IsInRenderingThread()); // we do not want this popped before the end of the scene and it better be the scene allocator
	FViewInfo* Result;
	if (FreeViewInfoSnapshots.Num())
	{
		Result = FreeViewInfoSnapshots.Pop(false);
	}
	else
	{
		Result = (FViewInfo*)FMemory::Malloc(sizeof(FViewInfo), ALIGNOF(FViewInfo));
	}
	FMemory::Memcpy(*Result, *this);

	// we want these to start null without a reference count, since we clear a ref later
	TUniformBufferRef<FViewUniformShaderParameters> NullViewUniformBuffer;
	FMemory::Memcpy(Result->ViewUniformBuffer, NullViewUniformBuffer); 

	TScopedPointer<FViewUniformShaderParameters> NullViewParameters;
	FMemory::Memcpy(Result->CachedViewUniformShaderParameters, NullViewParameters); 
	Result->bIsSnapshot = true;
	ViewInfoSnapshots.Add(Result);
	return Result;
}

void FViewInfo::DestroyAllSnapshots()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FViewInfo_DestroyAllSnapshots);

	check(IsInRenderingThread());
	// we will only keep double the number actually used, plus a few
	int32 NumToRemove = FreeViewInfoSnapshots.Num() - (ViewInfoSnapshots.Num() + 2);
	if (NumToRemove > 0)
	{
		for (int32 Index = 0; Index < NumToRemove; Index++)
		{
			FMemory::Free(FreeViewInfoSnapshots[Index]);
		}
		FreeViewInfoSnapshots.RemoveAt(0, NumToRemove, false);
	}
	for (FViewInfo* Snapshot : ViewInfoSnapshots)
	{
		Snapshot->ViewUniformBuffer.SafeRelease();
		Snapshot->CachedViewUniformShaderParameters.Reset();
		FreeViewInfoSnapshots.Add(Snapshot);
	}
	ViewInfoSnapshots.Reset();
}

FSceneViewState* FViewInfo::GetEffectiveViewState() const
{
	FSceneViewState* EffectiveViewState = ViewState;

	// When rendering in stereo we want to use the same exposure for both eyes.
	if (StereoPass == eSSP_RIGHT_EYE)
	{
		int32 ViewIndex = Family->Views.Find(this);
		if (Family->Views.IsValidIndex(ViewIndex))
		{
			// The left eye is always added before the right eye.
			ViewIndex = ViewIndex - 1;
			if (Family->Views.IsValidIndex(ViewIndex))
			{
				const FSceneView* PrimaryView = Family->Views[ViewIndex];
				if (PrimaryView->StereoPass == eSSP_LEFT_EYE)
				{
					EffectiveViewState = (FSceneViewState*)PrimaryView->State;
				}
			}
		}
	}

	return EffectiveViewState;
}

IPooledRenderTarget* FViewInfo::GetEyeAdaptation(FRHICommandList& RHICmdList) const
{
	return GetEyeAdaptationRT(RHICmdList);
}

IPooledRenderTarget* FViewInfo::GetEyeAdaptationRT(FRHICommandList& RHICmdList) const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	IPooledRenderTarget* result = NULL;
	if (EffectiveViewState)
	{
		result = EffectiveViewState->GetCurrentEyeAdaptationRT(RHICmdList);
	}
	return result;
}

IPooledRenderTarget* FViewInfo::GetLastEyeAdaptationRT(FRHICommandList& RHICmdList) const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	IPooledRenderTarget* result = NULL;
	if (EffectiveViewState)
	{
		result = EffectiveViewState->GetLastEyeAdaptationRT(RHICmdList);
	}
	return result;
}

void FViewInfo::SwapEyeAdaptationRTs() const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	if (EffectiveViewState)
	{
		EffectiveViewState->SwapEyeAdaptationRTs();
	}
}

bool FViewInfo::HasValidEyeAdaptation() const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();	

	if (EffectiveViewState)
	{
		return EffectiveViewState->HasValidEyeAdaptation();
	}
	return false;
}

void FViewInfo::SetValidEyeAdaptation() const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();	

	if (EffectiveViewState)
	{
		EffectiveViewState->SetValidEyeAdaptation();
	}
}

void FDisplayInternalsData::Setup(UWorld *World)
{
	DisplayInternalsCVarValue = 0;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DisplayInternalsCVarValue = CVarDisplayInternals.GetValueOnGameThread();

	if(IsValid())
	{
		MatineeTime = -1.0f;
		uint32 Count = 0;

		for (TObjectIterator<AMatineeActor> It; It; ++It)
		{
			AMatineeActor* MatineeActor = *It;

			if(MatineeActor->GetWorld() == World && MatineeActor->bIsPlaying)
			{
				MatineeTime = MatineeActor->InterpPosition;
				++Count;
			}
		}

		if(Count > 1)
		{
			MatineeTime = -2;
		}

		check(IsValid());
		
		extern ENGINE_API uint32 GStreamAllResourcesStillInFlight;
		NumPendingStreamingRequests = GStreamAllResourcesStillInFlight;
	}
#endif
}

void FSortedShadowMaps::Release()
{
	for (int32 AtlasIndex = 0; AtlasIndex < ShadowMapAtlases.Num(); AtlasIndex++)
	{
		ShadowMapAtlases[AtlasIndex].RenderTargets.Release();
	}

	for (int32 AtlasIndex = 0; AtlasIndex < RSMAtlases.Num(); AtlasIndex++)
	{
		RSMAtlases[AtlasIndex].RenderTargets.Release();
	}

	for (int32 AtlasIndex = 0; AtlasIndex < ShadowMapCubemaps.Num(); AtlasIndex++)
	{
		ShadowMapCubemaps[AtlasIndex].RenderTargets.Release();
	}

	PreshadowCache.RenderTargets.Release();
}

/*-----------------------------------------------------------------------------
	FSceneRenderer
-----------------------------------------------------------------------------*/

FSceneRenderer::FSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer)
:	Scene(InViewFamily->Scene ? InViewFamily->Scene->GetRenderScene() : NULL)
,	ViewFamily(*InViewFamily)
,	bUsedPrecomputedVisibility(false)
{
	check(Scene != NULL);

	check(IsInGameThread());
	ViewFamily.FrameNumber = GFrameNumber;

	// Copy the individual views.
	bool bAnyViewIsLocked = false;
	Views.Empty(InViewFamily->Views.Num());
	for(int32 ViewIndex = 0;ViewIndex < InViewFamily->Views.Num();ViewIndex++)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		for(int32 ViewIndex2 = 0;ViewIndex2 < InViewFamily->Views.Num();ViewIndex2++)
		{
			if (ViewIndex != ViewIndex2 && InViewFamily->Views[ViewIndex]->State != NULL)
			{
				// Verify that each view has a unique view state, as the occlusion query mechanism depends on it.
				check(InViewFamily->Views[ViewIndex]->State != InViewFamily->Views[ViewIndex2]->State);
			}
		}
#endif
		// Construct a FViewInfo with the FSceneView properties.
		FViewInfo* ViewInfo = new(Views) FViewInfo(InViewFamily->Views[ViewIndex]);
		ViewFamily.Views[ViewIndex] = ViewInfo;
		ViewInfo->Family = &ViewFamily;
		bAnyViewIsLocked |= ViewInfo->bIsLocked;

#if WITH_EDITOR
		// Should we allow the user to select translucent primitives?
		ViewInfo->bAllowTranslucentPrimitivesInHitProxy =
			GEngine->AllowSelectTranslucent() ||		// User preference enabled?
			!ViewInfo->IsPerspectiveProjection();		// Is orthographic view?
#endif

		// Batch the view's elements for later rendering.
		if(ViewInfo->Drawer)
		{
			FViewElementPDI ViewElementPDI(ViewInfo,HitProxyConsumer);
			ViewInfo->Drawer->Draw(ViewInfo,&ViewElementPDI);
		}
	}

	// If any viewpoint has been locked, set time to zero to avoid time-based
	// rendering differences in materials.
	if (bAnyViewIsLocked)
	{
		ViewFamily.CurrentRealTime = 0.0f;
		ViewFamily.CurrentWorldTime = 0.0f;
	}
	
	if(HitProxyConsumer)
	{
		// Set the hit proxies show flag.
		ViewFamily.EngineShowFlags.SetHitProxies(1);
	}

	// launch custom visibility queries for views
	if (GCustomCullingImpl)
	{
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			FViewInfo& ViewInfo = Views[ViewIndex];
			ViewInfo.CustomVisibilityQuery = GCustomCullingImpl->CreateQuery(ViewInfo);
		}
	}

	ViewFamily.ComputeFamilySize();

	// copy off the requests
	// (I apologize for the const_cast, but didn't seem worth refactoring just for the freezerendering command)
	bHasRequestedToggleFreeze = const_cast<FRenderTarget*>(InViewFamily->RenderTarget)->HasToggleFreezeCommand();

	FeatureLevel = Scene->GetFeatureLevel();
}

bool FSceneRenderer::DoOcclusionQueries(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return !IsMobilePlatform(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& CVarAllowOcclusionQueries.GetValueOnRenderThread() != 0;
}

FSceneRenderer::~FSceneRenderer()
{
	// To prevent keeping persistent references to single frame buffers, clear any such reference at this point.
	ClearPrimitiveSingleFramePrecomputedLightingBuffers();

	if(Scene)
	{
		// Destruct the projected shadow infos.
		for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
		{
			if( VisibleLightInfos.IsValidIndex(LightIt.GetIndex()) )
			{
				FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightIt.GetIndex()];
				for(int32 ShadowIndex = 0;ShadowIndex < VisibleLightInfo.MemStackProjectedShadows.Num();ShadowIndex++)
				{
					// FProjectedShadowInfo's in MemStackProjectedShadows were allocated on the rendering thread mem stack, 
					// Their memory will be freed when the stack is freed with no destructor call, so invoke the destructor explicitly
					VisibleLightInfo.MemStackProjectedShadows[ShadowIndex]->~FProjectedShadowInfo();
				}
			}
		}
	}

	// Manually release references to TRefCountPtrs that are allocated on the mem stack, which doesn't call dtors
	SortedShadowsForShadowDepthPass.Release();
}

/** 
* Finishes the view family rendering.
*/
void FSceneRenderer::RenderFinish(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, RenderFinish);

	if(FRCPassPostProcessBusyWait::IsPassRequired())
	{
		// mostly view independent but to be safe we use the first view
		FViewInfo& View = Views[0];

		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FRenderingCompositeOutputRef BusyWait;
		{
			// for debugging purpose, can be controlled by console variable
			FRenderingCompositePass* Node = CompositeContext.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBusyWait());
			BusyWait = FRenderingCompositeOutputRef(Node);
		}		
		
		if(BusyWait.IsValid())
		{
			CompositeContext.Process(BusyWait.GetPass(), TEXT("RenderFinish"));
		}
	}
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		bool bShowPrecomputedVisibilityWarning = false;
		static const auto* CVarPrecomputedVisibilityWarning = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PrecomputedVisibilityWarning"));
		if (CVarPrecomputedVisibilityWarning && CVarPrecomputedVisibilityWarning->GetValueOnRenderThread() == 1)
		{
			bShowPrecomputedVisibilityWarning = !bUsedPrecomputedVisibility;
		}

		bool bShowGlobalClipPlaneWarning = false;

		if (Scene->PlanarReflections.Num() > 0)
		{
			static const auto* CVarClipPlane = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowGlobalClipPlane"));
			if (CVarClipPlane && CVarClipPlane->GetValueOnRenderThread() == 0)
			{
				bShowGlobalClipPlaneWarning = true;
			}
		}
		
		extern int32 GDistanceFieldAO;
		bool bShowDFAODisabledWarning = !GDistanceFieldAO && (ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields || ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO);

		const bool bShowAtmosphericFogWarning = Scene->AtmosphericFog != nullptr && !Scene->ReadOnlyCVARCache.bEnableAtmosphericFog;
		const bool bShowSkylightWarning = (Scene->ShouldRenderSkylight_Internal(BLEND_Opaque) || Scene->ShouldRenderSkylight_Internal(BLEND_Translucent)) && !Scene->ReadOnlyCVARCache.bEnableStationarySkylight;
		const bool bShowPointLightWarning = UsedWholeScenePointLightNames.Num() > 0 && !Scene->ReadOnlyCVARCache.bEnablePointLightShadows;

		const bool bAnyWarning = bShowPrecomputedVisibilityWarning || bShowGlobalClipPlaneWarning || bShowAtmosphericFogWarning || bShowSkylightWarning || bShowPointLightWarning || bShowDFAODisabledWarning;

		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{	
			FViewInfo& View = Views[ViewIndex];
			if (!View.bIsReflectionCapture && !View.bIsSceneCapture )
			{
				// display a message saying we're frozen
				FSceneViewState* ViewState = (FSceneViewState*)View.State;
				bool bViewParentOrFrozen = ViewState && (ViewState->HasViewParent() || ViewState->bIsFrozen);
				bool bLocked = View.bIsLocked;
				if (bViewParentOrFrozen || bLocked || bAnyWarning)
				{
					SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

					FRenderTargetTemp TempRenderTarget(View);

					// create a temporary FCanvas object with the temp render target
					// so it can get the screen size
					int32 Y = 130;
					FCanvas Canvas(&TempRenderTarget, NULL, View.Family->CurrentRealTime, View.Family->CurrentWorldTime, View.Family->DeltaWorldTime, FeatureLevel);
					if (bViewParentOrFrozen)
					{
						const FText StateText =
							ViewState->bIsFrozen ?
							NSLOCTEXT("SceneRendering", "RenderingFrozen", "Rendering frozen...")
							:
							NSLOCTEXT("SceneRendering", "OcclusionChild", "Occlusion Child");
						Canvas.DrawShadowedText(10, Y, StateText, GetStatsFont(), FLinearColor(0.8, 1.0, 0.2, 1.0));
						Y += 14;
					}
					if (bShowPrecomputedVisibilityWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "NoPrecomputedVisibility", "NO PRECOMPUTED VISIBILITY");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bShowGlobalClipPlaneWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "NoGlobalClipPlane", "GLOBAL CLIP PLANE PROJECT SETTING NOT ENABLED");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bShowDFAODisabledWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "DFAODisabled", "Distance Field AO is disabled through scalability");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}

					if (bShowAtmosphericFogWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "AtmosphericFog", "PROJECT DOES NOT SUPPORT ATMOSPHERIC FOG");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;						
					}
					if (bShowSkylightWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "Skylight", "PROJECT DOES NOT SUPPORT STATIONARY SKYLIGHT OR MOVABLE SKYLIGHT IN SIMPLEFORWARD MODE: ");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bShowPointLightWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "PointLight", "PROJECT DOES NOT SUPPORT WHOLE SCENE POINT LIGHT SHADOWS: ");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;

						for (FName LightName : UsedWholeScenePointLightNames)
						{
							Canvas.DrawShadowedText(10, Y, FText::FromString(LightName.ToString()), GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
							Y += 14;
						}
					}

					if (bLocked)
					{
						static const FText Message = NSLOCTEXT("Renderer", "ViewLocked", "VIEW LOCKED");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(0.8, 1.0, 0.2, 1.0));
						Y += 14;
					}
					Canvas.Flush_RenderThread(RHICmdList);
				}
			}
		}
	}
	
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// Save the post-occlusion visibility stats for the frame and freezing info
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		INC_DWORD_STAT_BY(STAT_VisibleStaticMeshElements,View.NumVisibleStaticMeshElements);
		INC_DWORD_STAT_BY(STAT_VisibleDynamicPrimitives,View.VisibleDynamicPrimitives.Num());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// update freezing info
		FSceneViewState* ViewState = (FSceneViewState*)View.State;
		if (ViewState)
		{
			// if we're finished freezing, now we are frozen
			if (ViewState->bIsFreezing)
			{
				ViewState->bIsFreezing = false;
				ViewState->bIsFrozen = true;
				ViewState->bIsFrozenViewMatricesCached = true;
				ViewState->CachedViewMatrices = View.ViewMatrices;
			}

			// handle freeze toggle request
			if (bHasRequestedToggleFreeze)
			{
				// do we want to start freezing or stop?
				ViewState->bIsFreezing = !ViewState->bIsFrozen;
				ViewState->bIsFrozen = false;
				ViewState->bIsFrozenViewMatricesCached = false;
				ViewState->FrozenPrimitives.Empty();
			}
		}
#endif
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// clear the commands
	bHasRequestedToggleFreeze = false;

	if(ViewFamily.EngineShowFlags.OnScreenDebug)
	{
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			if(!View.IsPerspectiveProjection())
			{
				continue;
			}

			GRenderTargetPool.PresentContent(RHICmdList, View);
		}
	}
#endif

	for(int32 ViewExt = 0; ViewExt < ViewFamily.ViewExtensions.Num(); ++ViewExt)
	{
		ViewFamily.ViewExtensions[ViewExt]->PostRenderViewFamily_RenderThread(RHICmdList, ViewFamily);
		for(int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex)
		{
			ViewFamily.ViewExtensions[ViewExt]->PostRenderView_RenderThread(RHICmdList, Views[ViewIndex]);
		}
	}

	// Notify the RHI we are done rendering a scene.
	RHICmdList.EndScene();
}

FSceneRenderer* FSceneRenderer::CreateSceneRenderer(const FSceneViewFamily* InViewFamily, FHitProxyConsumer* HitProxyConsumer)
{
	EShadingPath ShadingPath = InViewFamily->Scene->GetShadingPath();

	if (ShadingPath == EShadingPath::Deferred)
	{
		return new FDeferredShadingSceneRenderer(InViewFamily, HitProxyConsumer);
	}
	else 
	{
		check(ShadingPath == EShadingPath::Mobile);
		return new FMobileSceneRenderer(InViewFamily, HitProxyConsumer);
	}
}

void ServiceLocalQueue();

void FSceneRenderer::RenderCustomDepthPassAtLocation(FRHICommandListImmediate& RHICmdList, int32 Location)
{		
	extern TAutoConsoleVariable<int32> CVarCustomDepthOrder;
	int32 CustomDepthOrder = FMath::Clamp(CVarCustomDepthOrder.GetValueOnRenderThread(), 0, 1);

	if(CustomDepthOrder == Location)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_CustomDepthPass);
		RenderCustomDepthPass(RHICmdList);
		ServiceLocalQueue();
	}
}

void FSceneRenderer::RenderCustomDepthPass(FRHICommandListImmediate& RHICmdList)
{
	if(FeatureLevel < ERHIFeatureLevel::SM4)
	{
		// not yet supported on lower end platforms
		return;
	}

	// do we have primitives in this pass?
	bool bPrimitives = false;

	if(!Scene->World || (Scene->World->WorldType != EWorldType::Preview && Scene->World->WorldType != EWorldType::Inactive))
	{
		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			if(View.CustomDepthSet.NumPrims())
			{
				bPrimitives = true;
				break;
			}
		}
	}

	// Render CustomDepth
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	if (SceneContext.BeginRenderingCustomDepth(RHICmdList, bPrimitives))
	{
		SCOPED_DRAW_EVENT(RHICmdList, CustomDepth);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_CustomDepth);

		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			
			// seems this is set each draw call anyway
			RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			
			const bool bWriteCustomStencilValues = SceneContext.IsCustomDepthPassWritingStencil();

			if (!bWriteCustomStencilValues)
			{
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
			}

			View.CustomDepthSet.DrawPrims(RHICmdList, View, bWriteCustomStencilValues);
		}

		// resolve using the current ResolveParams 
		SceneContext.FinishRenderingCustomDepth(RHICmdList);
	}
}

void FSceneRenderer::OnStartFrame()
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get_Todo_PassContext();

	GRenderTargetPool.VisualizeTexture.OnStartFrame(Views[0]);
	CompositionGraph_OnStartFrame();
	SceneContext.bScreenSpaceAOIsValid = false;
	SceneContext.bCustomDepthIsValid = false;

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{	
		FSceneView& View = Views[ViewIndex];
		FSceneViewStateInterface* State = View.State;

		if(State)
		{
			State->OnStartFrame(View, ViewFamily);
		}
	}
}

bool FSceneRenderer::ShouldCompositeEditorPrimitives(const FViewInfo& View)
{
	// If the show flag is enabled
	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		return false;
	}

	if (View.Family->EngineShowFlags.VisualizeHDR || View.Family->UseDebugViewPS())
	{
		// certain visualize modes get obstructed too much
		return false;
	}

	if (GIsEditor && View.Family->EngineShowFlags.Wireframe)
	{
		// In Editor we want wire frame view modes to be in MSAA
		return true;
	}

	// Any elements that needed compositing were drawn then compositing should be done
	if (View.ViewMeshElements.Num() || View.TopViewMeshElements.Num() || View.BatchedViewElements.HasPrimsToDraw() || View.TopBatchedViewElements.HasPrimsToDraw() || View.VisibleEditorPrimitives.Num())
	{
		return true;
	}

	return false;
}

void FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer)
{
	// we are about to destroy things that are being used for async tasks, so we wait here for them.
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DeleteSceneRenderer_WaitForTasks);
		RHICmdList.ImmediateFlush(EImmediateFlushType::WaitForOutstandingTasksOnly);
	}
	FViewInfo::DestroyAllSnapshots(); // this destroys viewinfo snapshots
	FSceneRenderTargets::Get(RHICmdList).DestroyAllSnapshots(); // this will destroy the render target snapshots
	static const IConsoleVariable* AsyncDispatch	= IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdAsyncRHIThreadDispatch"));

	if (AsyncDispatch->GetInt() == 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DeleteSceneRenderer_Dispatch);
		RHICmdList.ImmediateFlush(EImmediateFlushType::WaitForDispatchToRHIThread); // we want to make sure this all gets to the rhi thread this frame and doesn't hang around
	}
	// Delete the scene renderer.
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DeleteSceneRenderer);
		delete SceneRenderer;
	}
}


void FSceneRenderer::UpdatePrimitivePrecomputedLightingBuffers()
{
	// Use a bit array to prevent primitives from being updated more than once.
	FSceneBitArray UpdatedPrimitiveMap;
	UpdatedPrimitiveMap.Init(false, Scene->Primitives.Num());

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];

		for (int32 Index = 0; Index < View.DirtyPrecomputedLightingBufferPrimitives.Num(); ++Index)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = View.DirtyPrecomputedLightingBufferPrimitives[Index];

			FBitReference bInserted = UpdatedPrimitiveMap[PrimitiveSceneInfo->GetIndex()];
			if (!bInserted)
			{
				PrimitiveSceneInfo->UpdatePrecomputedLightingBuffer();
				bInserted = true;
			}
			else
			{
				// This will prevent clearing it twice.
				View.DirtyPrecomputedLightingBufferPrimitives[Index] = nullptr;
			}
		}
	}
}

void FSceneRenderer::ClearPrimitiveSingleFramePrecomputedLightingBuffers()
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];

		for (int32 Index = 0; Index < View.DirtyPrecomputedLightingBufferPrimitives.Num(); ++Index)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = View.DirtyPrecomputedLightingBufferPrimitives[Index];
	
			if (PrimitiveSceneInfo) // Could be null if it was a duplicate.
			{
				PrimitiveSceneInfo->ClearPrecomputedLightingBuffer(true);
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	FRendererModule
-----------------------------------------------------------------------------*/

/**
 * Helper function performing actual work in render thread.
 *
 * @param SceneRenderer	Scene renderer to use for rendering.
 */
static void RenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer)
{
	FMemMark MemStackMark(FMemStack::Get());

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);

	for( int ViewExt = 0; ViewExt < SceneRenderer->ViewFamily.ViewExtensions.Num(); ViewExt++ )
	{
		SceneRenderer->ViewFamily.ViewExtensions[ViewExt]->PreRenderViewFamily_RenderThread(RHICmdList, SceneRenderer->ViewFamily);
		for( int ViewIndex = 0; ViewIndex < SceneRenderer->ViewFamily.Views.Num(); ViewIndex++ )
		{
			SceneRenderer->ViewFamily.ViewExtensions[ViewExt]->PreRenderView_RenderThread(RHICmdList, SceneRenderer->Views[ViewIndex]);
		}
	}

	if(SceneRenderer->ViewFamily.EngineShowFlags.OnScreenDebug)
	{
		GRenderTargetPool.SetEventRecordingActive(true);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_TotalSceneRenderingTime);
		
		GPU_STATS_UPDATE(RHICmdList);
		
		if(SceneRenderer->ViewFamily.EngineShowFlags.HitProxies)
		{
			// Render the scene's hit proxies.
			SceneRenderer->RenderHitProxies(RHICmdList);
		}
		else
		{
			// Render the scene.
			SceneRenderer->Render(RHICmdList);
		}

		// Only reset per-frame scene state once all views have processed their frame, including those in planar reflections
		SceneRenderer->Scene->DistanceFieldSceneData.PrimitiveModifiedBounds.Reset();

#if STATS
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderViewFamily_RenderThread_MemStats);

			// Update scene memory stats that couldn't be tracked continuously
			SET_MEMORY_STAT(STAT_StaticDrawListMemory, FStaticMeshDrawListBase::TotalBytesUsed);
			SET_MEMORY_STAT(STAT_RenderingSceneMemory, SceneRenderer->Scene->GetSizeBytes());

			SIZE_T ViewStateMemory = 0;
			for (int32 ViewIndex = 0; ViewIndex < SceneRenderer->Views.Num(); ViewIndex++)
			{
				if (SceneRenderer->Views[ViewIndex].State)
				{
					ViewStateMemory += SceneRenderer->Views[ViewIndex].State->GetSizeBytes();
				}
			}
			SET_MEMORY_STAT(STAT_ViewStateMemory, ViewStateMemory);
			SET_MEMORY_STAT(STAT_RenderingMemStackMemory, FMemStack::Get().GetByteCount());
			SET_MEMORY_STAT(STAT_LightInteractionMemory, FLightPrimitiveInteraction::GetMemoryPoolSize());
		}
#endif

		GRenderTargetPool.SetEventRecordingActive(false);

		FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(RHICmdList, SceneRenderer);

	}

#if STATS
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderViewFamily_RenderThread_RHIGetGPUFrameCycles);
	if (FPlatformProperties::SupportsWindowedMode() == false)
	{
		/** Update STATS with the total GPU time taken to render the last frame. */
		SET_CYCLE_COUNTER(STAT_TotalGPUFrameTime, RHIGetGPUFrameCycles());
	}
#endif
}

void OnChangeSimpleForwardShading(IConsoleVariable* Var)
{
	static const auto SupportSimpleForwardShadingCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportSimpleForwardShading"));
	static const auto SimpleForwardShadingCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SimpleForwardShading"));

	if (SimpleForwardShadingCVar->GetValueOnAnyThread() != 0)
	{
		if (SupportSimpleForwardShadingCVar->GetValueOnAnyThread() == 0)
		{
			UE_LOG(LogRenderer, Warning, TEXT("r.SimpleForwardShading ignored as r.SupportSimpleForwardShading is not enabled"));
		}
		else if (!PlatformSupportsSimpleForwardShading(GMaxRHIShaderPlatform))
		{
			UE_LOG(LogRenderer, Warning, TEXT("r.SimpleForwardShading ignored, only supported on PC shader platforms.  Current shader platform %s"), *LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform).ToString());
		}
	}

	// Propgate cvar change to static draw lists
	FGlobalComponentRecreateRenderStateContext Context;
}

void OnChangeCVarRequiringRecreateRenderState(IConsoleVariable* Var)
{
	// Propgate cvar change to static draw lists
	FGlobalComponentRecreateRenderStateContext Context;
}

FRendererModule::FRendererModule()
{
	CVarSimpleForwardShading.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeSimpleForwardShading));

	static auto CVarEarlyZPass = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EarlyZPass"));
	CVarEarlyZPass->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeCVarRequiringRecreateRenderState));

	static auto CVarEarlyZPassMovable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EarlyZPassMovable"));
	CVarEarlyZPassMovable->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeCVarRequiringRecreateRenderState));
}

void FRendererModule::CreateAndInitSingleView(FRHICommandListImmediate& RHICmdList, class FSceneViewFamily* ViewFamily, const struct FSceneViewInitOptions* ViewInitOptions)
{
	// Create and add the new view
	FViewInfo* NewView = new FViewInfo(*ViewInitOptions);
	ViewFamily->Views.Add(NewView);
	SetRenderTarget(RHICmdList, ViewFamily->RenderTarget->GetRenderTargetTexture(), nullptr, ESimpleRenderTargetMode::EClearColorExistingDepth);
	FViewInfo* View = (FViewInfo*)ViewFamily->Views[0];
	View->InitRHIResources();
}

void FRendererModule::BeginRenderingViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily)
{
	UWorld* World = nullptr; 
	check(ViewFamily->Scene);

	FScene* const Scene = ViewFamily->Scene->GetRenderScene();
	if (Scene)
	{
		World = Scene->GetWorld();
		if (World)
		{
			//guarantee that all render proxies are up to date before kicking off a BeginRenderViewFamily.
			World->SendAllEndOfFrameUpdates();
		}
	}

	// Flush the canvas first.
	Canvas->Flush_GameThread();

	// Increment FrameNumber before render the scene. Wrapping around is no problem.
	// This is the only spot we change GFrameNumber, other places can only read.
	++GFrameNumber;

	// this is passes to the render thread, better access that than GFrameNumberRenderThread
	ViewFamily->FrameNumber = GFrameNumber;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		extern TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> GetRendererViewExtension();

		ViewFamily->ViewExtensions.Add(GetRendererViewExtension());
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)


	for (int ViewExt = 0; ViewExt < ViewFamily->ViewExtensions.Num(); ViewExt++)
	{
		ViewFamily->ViewExtensions[ViewExt]->BeginRenderViewFamily(*ViewFamily);
	}
	
	if (Scene)
	{		
		// Set the world's "needs full lighting rebuild" flag if the scene has any uncached static lighting interactions.
		if(World)
		{
			// Note: reading NumUncachedStaticLightingInteractions on the game thread here which is written to by the rendering thread
			// This is reliable because the RT uses interlocked mechanisms to update it
			World->SetMapNeedsLightingFullyRebuilt(Scene->NumUncachedStaticLightingInteractions);
		}
	
		// Construct the scene renderer.  This copies the view family attributes into its own structures.
		FSceneRenderer* SceneRenderer = FSceneRenderer::CreateSceneRenderer(ViewFamily, Canvas->GetHitProxyConsumer());

		if (!SceneRenderer->ViewFamily.EngineShowFlags.HitProxies)
		{
			USceneCaptureComponent2D::UpdateDeferredCaptures(Scene);
			USceneCaptureComponentCube::UpdateDeferredCaptures(Scene);

			for (int32 ReflectionIndex = 0; ReflectionIndex < SceneRenderer->Scene->PlanarReflections_GameThread.Num(); ReflectionIndex++)
			{
				UPlanarReflectionComponent* ReflectionComponent = SceneRenderer->Scene->PlanarReflections_GameThread[ReflectionIndex];
				SceneRenderer->Scene->UpdatePlanarReflectionContents(ReflectionComponent, *SceneRenderer);
			}
		}

		SceneRenderer->ViewFamily.DisplayInternalsData.Setup(World);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDrawSceneCommand,
			FSceneRenderer*,SceneRenderer,SceneRenderer,
		{
			RenderViewFamily_RenderThread(RHICmdList, SceneRenderer);
			FlushPendingDeleteRHIResources_RenderThread();
		});
	}
}

void FRendererModule::UpdateMapNeedsLightingFullyRebuiltState(UWorld* World)
{
	World->SetMapNeedsLightingFullyRebuilt(World->Scene->GetRenderScene()->NumUncachedStaticLightingInteractions);
}

void FRendererModule::DrawRectangle(
		FRHICommandList& RHICmdList,
		float X,
		float Y,
		float SizeX,
		float SizeY,
		float U,
		float V,
		float SizeU,
		float SizeV,
		FIntPoint TargetSize,
		FIntPoint TextureSize,
		class FShader* VertexShader,
		EDrawRectangleFlags Flags
		)
{
	::DrawRectangle( RHICmdList, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, TargetSize, TextureSize, VertexShader, Flags );
}


TGlobalResource<FFilterVertexDeclaration>& FRendererModule::GetFilterVertexDeclaration()
{
	return GFilterVertexDeclaration;
}

void FRendererModule::RegisterPostOpaqueRenderDelegate(const FPostOpaqueRenderDelegate& InPostOpaqueRenderDelegate)
{
	this->PostOpaqueRenderDelegate = InPostOpaqueRenderDelegate;
}

void FRendererModule::RegisterOverlayRenderDelegate(const FPostOpaqueRenderDelegate& InOverlayRenderDelegate)
{
	this->OverlayRenderDelegate = InOverlayRenderDelegate;
}

void FRendererModule::RenderPostOpaqueExtensions(const FSceneView& View, FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext)
{
	check(IsInRenderingThread());
	FPostOpaqueRenderParameters RenderParameters;
	RenderParameters.ViewMatrix = View.ViewMatrices.ViewMatrix;
	RenderParameters.ProjMatrix = View.ViewMatrices.ProjMatrix;
	RenderParameters.DepthTexture = SceneContext.GetSceneDepthSurface()->GetTexture2D();
	RenderParameters.SmallDepthTexture = SceneContext.GetSmallDepthSurface()->GetTexture2D();

	RenderParameters.ViewportRect = View.ViewRect;
	RenderParameters.RHICmdList = &RHICmdList;

	RenderParameters.Uid = (void*)(&View);
	PostOpaqueRenderDelegate.ExecuteIfBound(RenderParameters);
}

void FRendererModule::RenderOverlayExtensions(const FSceneView& View, FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext)
{
	check(IsInRenderingThread());
	FPostOpaqueRenderParameters RenderParameters;
	RenderParameters.ViewMatrix = View.ViewMatrices.ViewMatrix;
	RenderParameters.ProjMatrix = View.ViewMatrices.ProjMatrix;
	RenderParameters.DepthTexture = SceneContext.GetSceneDepthSurface()->GetTexture2D();
	RenderParameters.SmallDepthTexture = SceneContext.GetSmallDepthSurface()->GetTexture2D();

	RenderParameters.ViewportRect = View.ViewRect;
	RenderParameters.RHICmdList = &RHICmdList;

	RenderParameters.Uid=(void*)(&View);
	OverlayRenderDelegate.ExecuteIfBound(RenderParameters);
}


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

class FConsoleVariableAutoCompleteVisitor 
{
public:
	// @param Name must not be 0
	// @param CVar must not be 0
	static void OnConsoleVariable(const TCHAR *Name, IConsoleObject* CObj, uint32& Crc)
	{
		IConsoleVariable* CVar = CObj->AsVariable();
		if(CVar)
		{
			if(CObj->TestFlags(ECVF_Scalability) || CObj->TestFlags(ECVF_ScalabilityGroup))
			{
				// float should work on int32 as well
				float Value = CVar->GetFloat();
				Crc = FCrc::MemCrc32(&Value, sizeof(Value), Crc);
			}
		}
	}
};
static uint32 ComputeScalabilityCVarHash()
{
	uint32 Ret = 0;

	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateStatic< uint32& >(&FConsoleVariableAutoCompleteVisitor::OnConsoleVariable, Ret));

	return Ret;
}

static void DisplayInternals(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	auto Family = InView.Family;
	// if r.DisplayInternals != 0
	if(Family->EngineShowFlags.OnScreenDebug && Family->DisplayInternalsData.IsValid())
	{
		// could be 0
		auto State = InView.State;

		FCanvas Canvas((FRenderTarget*)Family->RenderTarget, NULL, Family->CurrentRealTime, Family->CurrentWorldTime, Family->DeltaWorldTime, InView.GetFeatureLevel());
		Canvas.SetRenderTargetRect(FIntRect(0, 0, Family->RenderTarget->GetSizeXY().X, Family->RenderTarget->GetSizeXY().Y));

		SetRenderTarget(RHICmdList, Family->RenderTarget->GetRenderTargetTexture(), FTextureRHIRef());

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		// further down to not intersect with "LIGHTING NEEDS TO BE REBUILT"
		FVector2D Pos(30, 140);
		const int32 FontSizeY = 14;

		// dark background
		const uint32 BackgroundHeight = 30;
		Canvas.DrawTile(Pos.X - 4, Pos.Y - 4, 500 + 8, FontSizeY * BackgroundHeight + 8, 0, 0, 1, 1, FLinearColor(0,0,0,0.6f), 0, true);

		UFont* Font = GEngine->GetSmallFont();
		FCanvasTextItem SmallTextItem( Pos, FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White );

		SmallTextItem.SetColor(FLinearColor::White);
		SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("r.DisplayInternals = %d"), Family->DisplayInternalsData.DisplayInternalsCVarValue));
		Canvas.DrawItem(SmallTextItem, Pos);
		SmallTextItem.SetColor(FLinearColor::Gray);
		Pos.Y += 2 * FontSizeY;

#define CANVAS_HEADER(txt) \
		{ \
			SmallTextItem.SetColor(FLinearColor::Gray); \
			SmallTextItem.Text = FText::FromString(txt); \
			Canvas.DrawItem(SmallTextItem, Pos); \
			Pos.Y += FontSizeY; \
		}
#define CANVAS_LINE(bHighlight, txt, ... ) \
		{ \
			SmallTextItem.SetColor(bHighlight ? FLinearColor::Red : FLinearColor::Gray); \
			SmallTextItem.Text = FText::FromString(FString::Printf(txt, __VA_ARGS__)); \
			Canvas.DrawItem(SmallTextItem, Pos); \
			Pos.Y += FontSizeY; \
		}

		CANVAS_HEADER(TEXT("command line options:"))
		{
			bool bHighlight = !(FApp::UseFixedTimeStep() && FApp::bUseFixedSeed);
			CANVAS_LINE(bHighlight, TEXT("  -UseFixedTimeStep: %u"), FApp::UseFixedTimeStep())
			CANVAS_LINE(bHighlight, TEXT("  -FixedSeed: %u"), FApp::bUseFixedSeed)
			CANVAS_LINE(false, TEXT("  -gABC= (changelist): %d"), GetChangeListNumberForPerfTesting())
		}

		CANVAS_HEADER(TEXT("Global:"))
		CANVAS_LINE(false, TEXT("  FrameNumberRT: %u"), GFrameNumberRenderThread)
		CANVAS_LINE(false, TEXT("  Scalability CVar Hash: %x (use console command \"Scalability\")"), ComputeScalabilityCVarHash())
		//not really useful as it is non deterministic and should not be used for rendering features:  CANVAS_LINE(false, TEXT("  FrameNumberRT: %u"), GFrameNumberRenderThread)
		CANVAS_LINE(false, TEXT("  FrameCounter: %u"), GFrameCounter)
		CANVAS_LINE(false, TEXT("  rand()/SRand: %x/%x"), FMath::Rand(), FMath::GetRandSeed())
		{
			bool bHighlight = Family->DisplayInternalsData.NumPendingStreamingRequests != 0;
			CANVAS_LINE(bHighlight, TEXT("  FStreamAllResourcesLatentCommand: %d"), bHighlight)
		}
		{
			static auto* Var = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Streaming.FramesForFullUpdate"));
			int32 Value = Var->GetValueOnRenderThread();
			bool bHighlight = Value != 0;
			CANVAS_LINE(bHighlight, TEXT("  r.Streaming.FramesForFullUpdate: %u%s"), Value, bHighlight ? TEXT(" (should be 0)") : TEXT(""));
		}

		if(State)
		{
			CANVAS_HEADER(TEXT("State:"))
			CANVAS_LINE(false, TEXT("  TemporalAASample: %u"), State->GetCurrentTemporalAASampleIndex())
			CANVAS_LINE(false, TEXT("  FrameIndexMod8: %u"), State->GetFrameIndexMod8())
			CANVAS_LINE(false, TEXT("  LODTransition: %.2f"), State->GetTemporalLODTransition())
		}

		CANVAS_HEADER(TEXT("Family:"))
		CANVAS_LINE(false, TEXT("  Time (Real/World/DeltaWorld): %.2f/%.2f/%.2f"), Family->CurrentRealTime, Family->CurrentWorldTime, Family->DeltaWorldTime)
		CANVAS_LINE(false, TEXT("  MatineeTime: %f"), Family->DisplayInternalsData.MatineeTime)
		CANVAS_LINE(false, TEXT("  FrameNumber: %u"), Family->FrameNumber)
		CANVAS_LINE(false, TEXT("  ExposureSettings: %s"), *Family->ExposureSettings.ToString())
		CANVAS_LINE(false, TEXT("  GammaCorrection: %.2f"), Family->GammaCorrection)

		CANVAS_HEADER(TEXT("View:"))
		CANVAS_LINE(false, TEXT("  TemporalJitter: %.2f/%.2f"), InView.TemporalJitterPixelsX, InView.TemporalJitterPixelsY)
		CANVAS_LINE(false, TEXT("  ViewProjectionMatrix Hash: %x"), InView.ViewProjectionMatrix.ComputeHash())
		CANVAS_LINE(false, TEXT("  ViewLocation: %s"), *InView.ViewLocation.ToString())
		CANVAS_LINE(false, TEXT("  ViewRotation: %s"), *InView.ViewRotation.ToString())
		CANVAS_LINE(false, TEXT("  ViewRect: %s"), *InView.ViewRect.ToString())

		FViewInfo& ViewInfo = (FViewInfo&)InView;

		CANVAS_LINE(false, TEXT("  DynMeshElements/TranslPrim: %d/%d"), ViewInfo.DynamicMeshElements.Num(), ViewInfo.TranslucentPrimSet.NumPrims())

#undef CANVAS_LINE
#undef CANVAS_HEADER

		Canvas.Flush_RenderThread(RHICmdList);
	}
#endif
}

TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> GetRendererViewExtension()
{
	class FRendererViewExtension : public ISceneViewExtension
	{
	public:
		virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) {}
		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) {}
		virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {}
		virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {}
		virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {}
		virtual int32 GetPriority() const { return 0; }
		virtual void PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
		{
			DisplayInternals(RHICmdList, InView);
		}
	};
	TSharedPtr<FRendererViewExtension, ESPMode::ThreadSafe> ptr(new FRendererViewExtension);
	return StaticCastSharedPtr<ISceneViewExtension>(ptr);
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)