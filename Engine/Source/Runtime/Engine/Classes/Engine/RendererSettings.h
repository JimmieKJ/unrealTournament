// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveFloat.h"
#include "UserInterfaceSettings.h"

#include "RendererSettings.generated.h"


/**
 * Enumerates ways to clear a scene.
 */
UENUM()
namespace EClearSceneOptions
{
	enum Type
	{
		NoClear = 0 UMETA(DisplayName="Do not clear",ToolTip="This option is fastest but can cause artifacts unless you render to every pixel. Make sure to use a skybox with this option!"),
		HardwareClear = 1 UMETA(DisplayName="Hardware clear",ToolTip="Perform a full hardware clear before rendering. Most projects should use this option."),
		QuadAtMaxZ = 2 UMETA(DisplayName="Clear at far plane",ToolTip="Draws a quad to perform the clear at the far plane, this is faster than a hardware clear on some GPUs."),
	};
}


/**
 * Enumerates available compositing sample counts.
 */
UENUM()
namespace ECompositingSampleCount
{
	enum Type
	{
		One = 1 UMETA(DisplayName="No MSAA"),
		Two = 2 UMETA(DisplayName="2x MSAA"),
		Four = 4 UMETA(DisplayName="4x MSAA"),
		Eight = 8 UMETA(DisplayName="8x MSAA"),
	};
}


/**
 * Enumerates available options for custom depth.
 */
UENUM()
namespace ECustomDepthStencil
{
	enum Type
	{
		Disabled = 0,
		Enabled = 1 UMETA(ToolTip="Depth buffer created immediately. Stencil disabled."),
		EnabledOnDemand = 2 UMETA(ToolTip="Depth buffer created on first use, can save memory but cause stalls. Stencil disabled."),
		EnabledWithStencil = 3 UMETA(ToolTip="Depth buffer created immediately. Stencil available for read/write."),
	};
}


/**
 * Enumerates available options for early Z-passes.
 */
UENUM()
namespace EEarlyZPass
{
	enum Type
	{
		None = 0 UMETA(DisplayName="None"),
		OpaqueOnly = 1 UMETA(DisplayName="Opaque meshes only"),
		OpaqueAndMasked = 2 UMETA(DisplayName="Opaque and masked meshes"),
		Auto = 3 UMETA(DisplayName="Decide automatically",ToolTip="Let the engine decide what to render in the early Z pass based on the features being used."),
	};
}

/** used by FPostProcessSettings AutoExposure*/
UENUM()
namespace EAutoExposureMethodUI
{
	enum Type
	{
		/** Not supported on mobile, requires compute shader to construct 64 bin histogram */
		AEM_Histogram  UMETA(DisplayName = "Auto Exposure Histogram"),
		/** Not supported on mobile, faster method that computes single value by downsampling */
		AEM_Basic      UMETA(DisplayName = "Auto Exposure Basic"),
		AEM_MAX,
	};
}

/**
 * Rendering settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Rendering"))
class ENGINE_API URendererSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category=Mobile, meta=(
		ConsoleVariable="r.MobileHDR",DisplayName="Mobile HDR",
		ToolTip="If true, mobile renders in full HDR. Disable this setting for games that do not require lighting features for better performance on slow devices."))
	uint32 bMobileHDR:1;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (
		ConsoleVariable = "r.MobileNumDynamicPointLights", DisplayName = "Max Dynamic Point Lights", ClampMax = 4, 
		ToolTip = "The number of dynamic point lights to support on mobile devices. Setting this to 0 for games which do not require dynamic point lights will reduce the number of shaders generated. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
	uint32 MobileNumDynamicPointLights;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (
		ConsoleVariable = "r.MobileDynamicPointLightsUseStaticBranch", DisplayName = "Use Shared Dynamic Point Light Shaders",
		ToolTip = "If this setting is enabled, the same shader will be used for any number of dynamic point lights (up to the maximum specified above) hitting a surface. This is slightly slower but reduces the number of shaders generated. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
	uint32 bMobileDynamicPointLightsUseStaticBranch : 1;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (
		ConsoleVariable = "r.Mobile.EnableStaticAndCSMShadowReceivers", DisplayName = "Enable Combined Static and CSM Shadowing",
		ToolTip = "Allow primitives to receive both static and CSM shadows from a stationary light. Disabling will free a mobile texture sampler.",
		ConfigRestartRequired = true))
		uint32 bMobileEnableStaticAndCSMShadowReceivers : 1;

	UPROPERTY(config, EditAnywhere, Category = Materials, meta = (
		ConsoleVariable = "r.DiscardUnusedQuality", DisplayName = "Game Discards Unused Material Quality Levels",
		ToolTip = "When running in game mode, whether to keep shaders for all quality levels in memory or only those needed for the current quality level.\nUnchecked: Keep all quality levels in memory allowing a runtime quality level change. (default)\nChecked: Discard unused quality levels when loading content for the game, saving some memory."))
	uint32 bDiscardUnusedQualityLevels : 1;

	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.AllowOcclusionQueries",DisplayName="Occlusion Culling",
		ToolTip="Allows occluded meshes to be culled and no rendered."))
	uint32 bOcclusionCulling:1;

	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.MinScreenRadiusForLights",DisplayName="Min Screen Radius for Lights",
		ToolTip="Screen radius at which lights are culled. Larger values can improve performance but causes lights to pop off when they affect a small area of the screen."))
	float MinScreenRadiusForLights;

	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.MinScreenRadiusForDepthPrepass",DisplayName="Min Screen Radius for Early Z Pass",
		ToolTip="Screen radius at which objects are culled for the early Z pass. Larger values can improve performance but very large values can degrade performance if large occluders are not rendered."))
	float MinScreenRadiusForEarlyZPass;

	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.MinScreenRadiusForCSMDepth",DisplayName="Min Screen Radius for Cascaded Shadow Maps",
		ToolTip="Screen radius at which objects are culled for cascaded shadow map depth passes. Larger values can improve performance but can cause artifacts as objects stop casting shadows."))
	float MinScreenRadiusForCSMdepth;
	
	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.PrecomputedVisibilityWarning",DisplayName="Warn about no precomputed visibility",
		ToolTip="Displays a warning when no precomputed visibility data is available for the current camera location. This can be helpful if you are making a game that relies on precomputed visibility, e.g. a first person mobile game."))
	uint32 bPrecomputedVisibilityWarning:1;

	UPROPERTY(config, EditAnywhere, Category=Textures, meta=(
		ConsoleVariable="r.TextureStreaming",DisplayName="Texture Streaming",
		ToolTip="When enabled textures will stream in based on what is visible on screen."))
	uint32 bTextureStreaming:1;

	UPROPERTY(config, EditAnywhere, Category=Textures, meta=(
		ConsoleVariable="Compat.UseDXT5NormalMaps",DisplayName="Use DXT5 Normal Maps",
		ToolTip="Whether to use DXT5 for normal maps, otherwise BC5 will be used, which is not supported on all hardware. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bUseDXT5NormalMaps:1;

	UPROPERTY(config, EditAnywhere, Category = Materials, meta =(
		ConfigRestartRequired = true,
		ConsoleVariable = "r.ClearCoatNormal",
		ToolTip = "Use a separate normal map for the bottom layer of a clear coat material. This is a higher quality feature that is expensive."))
		uint32 bClearCoatEnableSecondNormal : 1;

	UPROPERTY(config, EditAnywhere, Category = Textures, meta = (
		ConsoleVariable = "r.ReflectionCaptureResolution", DisplayName = "Reflection Capture Resolution",
		ToolTip = "The cubemap resolution for all reflection capture probes. Must be power of 2. Note that for very high values the memory and performance impact may be severe."))
	int32 ReflectionCaptureResolution;

	UPROPERTY(config, EditAnywhere, Category=ForwardShading, meta=(
		ConsoleVariable="r.ForwardShading",
		DisplayName = "Forward Shading (experimental)",
		ToolTip="Whether to use forward shading on desktop platforms.  Requires Shader Model 5 hardware.  Forward shading has lower constant cost, but fewer features supported.  Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bForwardShading:1;

	UPROPERTY(config, EditAnywhere, Category=ForwardShading, meta=(
		ConsoleVariable="r.VertexFoggingForOpaque",
		ToolTip="Causes opaque materials to use per-vertex fogging, which costs less and integrates properly with MSAA.  Only supported with forward shading. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bVertexFoggingForOpaque:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.AllowStaticLighting",
		ToolTip="Whether to allow any static lighting to be generated and used, like lightmaps and shadowmaps. Games that only use dynamic lighting should set this to 0 to save some static lighting overhead. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bAllowStaticLighting:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.NormalMapsForStaticLighting",
		ToolTip="Whether to allow any static lighting to use normal maps for lighting computations."))
	uint32 bUseNormalMapsForStaticLighting:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.GenerateMeshDistanceFields",
		ToolTip="Whether to build distance fields of static meshes, needed for distance field AO, which is used to implement Movable SkyLight shadows, and ray traced distance field shadows on directional lights.  Enabling will increase mesh build times and memory usage.  Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bGenerateMeshDistanceFields:1;

	UPROPERTY(config, EditAnywhere, Category = Lighting, meta = (
		EditCondition = "bGenerateMeshDistanceFields",
		ConsoleVariable = "r.GenerateLandscapeGIData", DisplayName = "Generate Landscape Real-time GI Data",
		ToolTip = "Whether to generate a low-resolution base color texture for landscapes for rendering real-time global illumination.  This feature requires GenerateMeshDistanceFields is also enabled, and will increase mesh build times and memory usage."))
	uint32 bGenerateLandscapeGIData : 1;

	UPROPERTY(config, EditAnywhere, Category=Tessellation, meta=(
		ConsoleVariable="r.TessellationAdaptivePixelsPerTriangle",DisplayName="Adaptive pixels per triangle",
		ToolTip="When adaptive tessellation is enabled it will try to tessellate a mesh so that each triangle contains the specified number of pixels. The tessellation multiplier specified in the material can increase or decrease the amount of tessellation."))
	float TessellationAdaptivePixelsPerTriangle;

	UPROPERTY(config, EditAnywhere, Category=Translucency, meta=(
		ConsoleVariable="r.SeparateTranslucency",
		ToolTip="Allow translucency to be rendered to a separate render targeted and composited after depth of field. Prevents translucency from appearing out of focus."))
	uint32 bSeparateTranslucency:1;

	UPROPERTY(config, EditAnywhere, Category=Translucency, meta=(
		ConsoleVariable="r.TranslucentSortPolicy",DisplayName="Translucent Sort Policy",
		ToolTip="The sort mode for translucent primitives, affecting how they are ordered and how they change order as the camera moves."))
	TEnumAsByte<ETranslucentSortPolicy::Type> TranslucentSortPolicy;

	UPROPERTY(config, EditAnywhere, Category=Translucency, meta=(
		DisplayName="Translucent Sort Axis",
		ToolTip="The axis that sorting will occur along when Translucent Sort Policy is set to SortAlongAxis."))
	FVector TranslucentSortAxis;

	UPROPERTY(config, EditAnywhere, Category=Postprocessing, meta=(
		ConsoleVariable="r.CustomDepth",DisplayName="Custom Depth-Stencil Pass",
		ToolTip="Whether the custom depth pass for tagging primitives for postprocessing passes is enabled. Enabling it on demand can save memory but may cause a hitch the first time the feature is used."))
	TEnumAsByte<ECustomDepthStencil::Type> CustomDepthStencil;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.Bloom", DisplayName = "Bloom",
		ToolTip = "Whether the default for Bloom is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureBloom : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AmbientOcclusion", DisplayName = "Ambient Occlusion",
		ToolTip = "Whether the default for AmbientOcclusion is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureAmbientOcclusion : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AmbientOcclusionStaticFraction", DisplayName = "Ambient Occlusion Static Fraction (AO for baked lighting)",
		ToolTip = "Whether the default for AmbientOcclusionStaticFraction is enabled or not (only useful for baked lighting and if AO is on, allows to have SSAO affect baked lighting as well, costs performance, postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureAmbientOcclusionStaticFraction : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AutoExposure", DisplayName = "Auto Exposure",
		ToolTip = "Whether the default for AutoExposure is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureAutoExposure : 1;
	
	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AutoExposure.Method", DisplayName = "Auto Exposure",
		ToolTip = "The default method for AutoExposure(postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	TEnumAsByte<EAutoExposureMethodUI::Type> DefaultFeatureAutoExposure; 

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.MotionBlur", DisplayName = "Motion Blur",
		ToolTip = "Whether the default for MotionBlur is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureMotionBlur : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.LensFlare", DisplayName = "Lens Flares (Image based)",
		ToolTip = "Whether the default for LensFlare is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureLensFlare : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AntiAliasing", DisplayName = "Anti-Aliasing Method",
		ToolTip = "What anti-aliasing mode is used by default (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	TEnumAsByte<EAntiAliasingMethod> DefaultFeatureAntiAliasing;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.StencilForLODDither",DisplayName="Use Stencil for LOD Dither Fading",
		ToolTip="Whether to use stencil for LOD dither fading.  This saves GPU time in the base pass for materials with dither fading enabled, but forces a full prepass. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bStencilForLODDither:1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable="r.EarlyZPass",DisplayName="Early Z-pass",
		ToolTip="Whether to use a depth only pass to initialize Z culling for the base pass."))
	TEnumAsByte<EEarlyZPass::Type> EarlyZPass;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.EarlyZPassMovable",DisplayName="Movables in early Z-pass",
		ToolTip="Whether to render movable objects in the early Z pass. Need to reload the level!"))
	uint32 bEarlyZPassMovable:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.DBuffer",DisplayName="DBuffer Decals",
		ToolTip="Whether to accumulate decal properties to a buffer before the base pass.  DBuffer decals correctly affect lightmap and sky lighting, unlike regular deferred decals.  DBuffer enabled forces a full prepass.  Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bDBuffer:1;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.ClearSceneMethod",DisplayName="Clear Scene",
		ToolTip="Select how the g-buffer is cleared in game mode (only affects deferred shading)."))
	TEnumAsByte<EClearSceneOptions::Type> ClearSceneMethod;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.BasePassOutputsVelocity", DisplayName="Accurate velocities from Vertex Deformation",
		ToolTip="Enables materials with time-based World Position Offset and/or World Displacement to output accurate velocities. This incurs a performance cost. If this is disabled, those materials will not output velocities. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bBasePassOutputsVelocity:1;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.SelectiveBasePassOutputs", DisplayName="Selectively output to the GBuffer rendertargets",
		ToolTip="Enables not exporting to the GBuffer rendertargets that are not relevant. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bSelectiveBasePassOutputs:1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		DisplayName = "Enable Particle Cutouts by default",
		ToolTip = "When enabled, after changing the material on a Required particle module a Particle Cutout texture will be chosen automatically from the Opacity Mask texture if it exists, if not the Opacity Texture will be used if it exists."))
	uint32 bDefaultParticleCutouts : 1;

	UPROPERTY(config, EditAnywhere, Category = Lighting, meta = (
		ConsoleVariable = "r.AllowGlobalClipPlane", DisplayName = "Support global clip plane for Planar Reflections",
		ToolTip = "Whether to support the global clip plane needed for planar reflections.  Enabling this increases BasePass triangle cost by ~15% regardless of whether planar reflections are active. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
	uint32 bGlobalClipPlane : 1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.GBufferFormat", DisplayName = "GBuffer Format",
		ToolTip = "Selects which GBuffer format should be used. Affects performance primarily via how much GPU memory bandwidth used."))
	TEnumAsByte<EGBufferFormat::Type> GBufferFormat;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.MorphTarget.Mode", DisplayName = "Use GPU for computing morph targets",
		ToolTip = "Whether to use original CPU method (loop per morph then by vertex) or use a GPU-based method on Shader Model 5 hardware."))
	uint32 bUseGPUMorphTargets : 1;

	UPROPERTY(config, EditAnywhere, Category=VR, meta=(
		ConsoleVariable="vr.InstancedStereo", DisplayName="Instanced Stereo",
		ToolTip="Enable instanced stereo rendering (only available for D3D SM5 or PS4).",
		ConfigRestartRequired=true))
	uint32 bInstancedStereo:1;

	UPROPERTY(config, EditAnywhere, Category = VR, meta = (
		EditCondition = "bInstancedStereo",
		ConsoleVariable = "vr.MultiView", DisplayName = "Multi-View",
		ToolTip = "Enable multi-view for instanced stereo rendering (only available on the PS4).",
		ConfigRestartRequired = true))
	uint32 bMultiView : 1;

	UPROPERTY(config, EditAnywhere, Category=Editor, meta=(
		ConsoleVariable="r.WireframeCullThreshold",DisplayName="Wireframe Cull Threshold",
		ToolTip="Screen radius at which wireframe objects are culled. Larger values can improve performance when viewing a scene in wireframe."))
	float WireframeCullThreshold;

	/**
	"Stationary skylight requires permutations of the basepass shaders.  Disabling will reduce the number of shader permutations required per material. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportStationarySkylight", DisplayName = "Support Stationary Skylight",
		ConfigRestartRequired = true))
		uint32 bSupportStationarySkylight : 1;

	/**
	"Low quality lightmap requires permutations of the lightmap rendering shaders.  Disabling will reduce the number of shader permutations required per material. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportLowQualityLightmaps", DisplayName = "Support low quality lightmap shader permutations",
		ConfigRestartRequired = true))
		uint32 bSupportLowQualityLightmaps : 1;

	/**
	PointLight WholeSceneShadows requires many vertex and geometry shader permutations for cubemap rendering. Disabling will reduce the number of shader permutations required per material. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportPointLightWholeSceneShadows", DisplayName = "Support PointLight WholeSceneShadows",
		ConfigRestartRequired = true))
		uint32 bSupportPointLightWholeSceneShadows : 1;

	/** 
	"Atmospheric fog requires permutations of the basepass shaders.  Disabling will reduce the number of shader permutations required per material. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportAtmosphericFog", DisplayName = "Support Atmospheric Fog",	
		ConfigRestartRequired = true))
		uint32 bSupportAtmosphericFog : 1;

public:

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ End UObject Interface

private:
	void SanatizeReflectionCaptureResolution();
	
	UPROPERTY(config)
	TEnumAsByte<EUIScalingRule> UIScaleRule_DEPRECATED;

	UPROPERTY(config)
	FRuntimeFloatCurve UIScaleCurve_DEPRECATED;
};

UCLASS(config = Engine, globaluserconfig, meta = (DisplayName = "Rendering Overrides (Local)"))
class ENGINE_API URendererOverrideSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	/**
		"Enabling will locally override all ShaderPermutationReduction settings from the Renderer section to be enabled.  Saved to local user config only.."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportAllShaderPermutations", DisplayName = "Force all shader permutation support",
		ConfigRestartRequired = true))
		uint32 bSupportAllShaderPermutations : 1;

public:

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};