// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
namespace ECustomDepth
{
	enum Type
	{
		Disabled = 0,
		Enabled = 1,
		EnabledOnDemand = 2,
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


/** used by FPostProcessSettings Anti-aliasing */
UENUM()
namespace EAntiAliasingMethodUI
{
	enum Type
	{
		AAM_None UMETA(DisplayName = "None"),
		AAM_FXAA UMETA(DisplayName = "FXAA"),
		AAM_TemporalAA UMETA(DisplayName = "TemporalAA"),
		AAM_MAX,
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
		ConsoleVariable="r.MinScreenRadiusForDepthPrepass",DisplayName="Min Screen Radius for Cascaded Shadow Maps",
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
		ConsoleVariable = "r.GenerateLandscapeGIData", DisplayName = "Generate Landscape Real-time GI Data",
		ToolTip = "Whether to generate a low-resolution base color texture for landscapes for rendering real-time global illumination.  This feature requires GenerateMeshDistanceFields is also enabled, and will increase mesh build times and memory usage."))
		uint32 bGenerateLandscapeGIData : 1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.Shadow.DistanceFieldPenumbraSize",
		ToolTip="Controls the size of the uniform penumbra produced by static shadowing."))
	float DistanceFieldPenumbraSize;

	UPROPERTY(config, EditAnywhere, Category=Tessellation, meta=(
		ConsoleVariable="r.TessellationAdaptivePixelsPerTriangle",DisplayName="Adaptive pixels per triangle",
		ToolTip="When adaptive tessellation is enabled it will try to tessellate a mesh so that each triangle contains the specified number of pixels. The tessellation multiplier specified in the material can increase or decrease the amount of tessellation."))
	float TessellationAdaptivePixelsPerTriangle;

	UPROPERTY(config, EditAnywhere, Category=Postprocessing, meta=(
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
		ConsoleVariable="r.CustomDepth",DisplayName="Custom Depth Pass",
		ToolTip="Whether the custom depth pass for tagging primitives for postprocessing passes is enabled. Enabling it on demand can save memory but may cause a hitch the first time the feature is used."))
	TEnumAsByte<ECustomDepth::Type> CustomDepth;

	UPROPERTY(config, EditAnywhere, Category = DefaultPostprocessingSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.Bloom", DisplayName = "Bloom",
		ToolTip = "Whether the default for Bloom is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureBloom : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultPostprocessingSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AmbientOcclusion", DisplayName = "Ambient Occlusion",
		ToolTip = "Whether the default for AmbientOcclusion is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureAmbientOcclusion : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultPostprocessingSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AmbientOcclusionStaticFraction", DisplayName = "Ambient Occlusion Static Fraction (AO for baked lighting)",
		ToolTip = "Whether the default for AmbientOcclusionStaticFraction is enabled or not (only useful for baked lighting and if AO is on, allows to have SSAO affect baked lighting as well, costs performance, postprocess volume/camera/game setting can still override and enable or disable it independently)"))
		uint32 bDefaultFeatureAmbientOcclusionStaticFraction : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultPostprocessingSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AutoExposure", DisplayName = "Auto Exposure",
		ToolTip = "Whether the default for AutoExposure is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureAutoExposure : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultPostprocessingSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.MotionBlur", DisplayName = "Motion Blur",
		ToolTip = "Whether the default for MotionBlur is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureMotionBlur : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultPostprocessingSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.LensFlare", DisplayName = "Lens Flares (Image based)",
		ToolTip = "Whether the default for LensFlare is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureLensFlare : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultPostprocessingSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AntiAliasing", DisplayName = "Anti-Aliasing Method",
		ToolTip = "What anti-aliasing mode is used by default (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	TEnumAsByte<EAntiAliasingMethodUI::Type> DefaultFeatureAntiAliasing;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable="r.EarlyZPass",DisplayName="Early Z-pass",
		ToolTip="Whether to use a depth only pass to initialize Z culling for the base pass. Need to reload the level!"))
	TEnumAsByte<EEarlyZPass::Type> EarlyZPass;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.EarlyZPassMovable",DisplayName="Movables in early Z-pass",
		ToolTip="Whether to render movable objects in the early Z pass. Need to reload the level!"))
	uint32 bEarlyZPassMovable:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.DBuffer",DisplayName="DBuffer Decals",
		ToolTip="Experimental decal feature (see r.DBuffer, ideally combined with 'Movables in early Z-pass' and 'Early Z-pass')"))
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

	UPROPERTY(config, EditAnywhere, Category=Editor, meta=(
		ConsoleVariable="r.WireframeCullThreshold",DisplayName="Wireframe Cull Threshold",
		ToolTip="Screen radius at which wireframe objects are culled. Larger values can improve performance when viewing a scene in wireframe."))
	float WireframeCullThreshold;

public:

	// Begin UObject interface

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// End UObject interface

private:
	
	UPROPERTY(config)
	TEnumAsByte<EUIScalingRule> UIScaleRule_DEPRECATED;

	UPROPERTY(config)
	FRuntimeFloatCurve UIScaleCurve_DEPRECATED;
};
