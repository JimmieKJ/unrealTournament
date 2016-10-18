// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "EditorExperimentalSettings.generated.h"


/**
 * Implements Editor settings for experimental features.
 */
UCLASS(config=EditorPerProjectUserSettings)
class UNREALED_API UEditorExperimentalSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Allows usage of the procedural foliage system */
	UPROPERTY(EditAnywhere, config, Category = Foliage, meta = (DisplayName = "Procedural Foliage"))
	bool bProceduralFoliage;

	/** Allows usage of the Localization Dashboard */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Localization Dashboard"))
	bool bEnableLocalizationDashboard;

	/** Allows usage of the Translation Picker */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Translation Picker"))
	bool bEnableTranslationPicker;

	/** The Blutility shelf holds editor utility Blueprints. Summon from the Workspace menu. */
	UPROPERTY(EditAnywhere, config, Category=Tools, meta=(DisplayName="Editor Utility Blueprints (Blutility)"))
	bool bEnableEditorUtilityBlueprints;

	/** When enabled, all details panels will be able to have properties marked as favorite that show in a top most category.  
	 * NOTE: Some customizations are not supported yet
	 */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Enable Details Panel Favorites"))
	bool bEnableFavoriteSystem;

	/** Enable being able to subclass components in blueprints */
	UPROPERTY(EditAnywhere, config, Category=Tools, meta=(ConfigRestartRequired=true))
	bool bBlueprintableComponents;
	
	/** Device output log window (currently implemented for Android only)*/
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Device Output Log"))
	bool bDeviceOutputLog;

	/** Specify which console-specific nomenclature to use for gamepad label text */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, meta=(DisplayName="Console for Gamepad Labels"))
	TEnumAsByte<EConsoleForGamepadLabels::Type> ConsoleForGamepadLabels;

	/** Allows for customization of toolbars and menus throughout the editor */
	UPROPERTY(config)
	bool bToolbarCustomization;

	/** Break on Exceptions allows you to trap Access Nones and other exceptional events in Blueprints. */
	UPROPERTY(EditAnywhere, config, Category=Blueprints, meta=(DisplayName="Blueprint Break on Exceptions"))
	bool bBreakOnExceptions;

	/** Enable experimental blueprint performance analysis tools. */
	UPROPERTY(EditAnywhere, config, Category=Blueprints, meta=(DisplayName="Blueprint Performance Analysis Tools"))
	bool bBlueprintPerformanceAnalysisTools;

	/** Enables the visual diff tool for widget blueprints. WARNING: changes to the widget hierarchy will not be detected */
	UPROPERTY(EditAnywhere, config, Category=Blueprints, meta=(DisplayName="Use the Diff Tool for Widget Blueprints"))
	bool bEnableWidgetVisualDiff;

	/** Enables the visual diff tool for anim blueprints. WARNING: changes to the Target Skeleton and Groups will not be detected */
	UPROPERTY(EditAnywhere, config, Category = Blueprints, meta = (DisplayName = "Use the Diff Tool for Animation Blueprints"))
	bool bEnableAnimVisualDiff;

	/** Enables "Find and Replace All" tool in the MyBlueprint window for variables */
	UPROPERTY(EditAnywhere, config, Category = Blueprints, meta = (DisplayName = "Find and Replace All References Tool"))
	bool bEnableFindAndReplaceReferences;

	/** Should arrows indicating data/execution flow be drawn halfway along wires? */
	UPROPERTY(/*EditAnywhere - deprecated (moved into UBlueprintEditorSettings), */config/*, Category=Blueprints, meta=(DisplayName="Draw midpoint arrows in Blueprints")*/)
	bool bDrawMidpointArrowsInBlueprints;

	/** Whether to show Audio Streaming options for SoundWaves (disabling will not stop all audio streaming) */
	UPROPERTY(EditAnywhere, config, Category=Audio)
	bool bShowAudioStreamingOptions;

	/** Allows ChunkIDs to be assigned to assets to via the content browser context menu. */
	UPROPERTY(EditAnywhere,config,Category=UserInterface,meta=(DisplayName="Allow ChunkID Assignments"))
	bool bContextMenuChunkAssignments;

	/** Disable cook in the editor */
	UPROPERTY(EditAnywhere, config, Category = Cooking, meta = (DisplayName = "Disable Cook In The Editor feature (cooks from launch on will be run in a separate process if disabled)", ConfigRestartRequired=true))
	bool bDisableCookInEditor;

	UPROPERTY(EditAnywhere, config, Category = Cooking, meta = (DisplayName = "Use multiple processes when cooking (only affects File -> Package)"))
	int32 MultiProcessCooking;

	/** Enables Environment Queries editor */
	UPROPERTY(EditAnywhere, config, Category = AI, meta = (DisplayName = "Environment Querying System"))
	bool bEQSEditor;

	/** This feature allows you to broadcast to a live streaming service directly from the editor.  This requires you to have a live streaming plugin installed. */
	UPROPERTY(EditAnywhere, config, Category=Tools)
	bool bLiveStreamingFromEditor;

	/** Enables Metal/High-end mobile rendering path preview on Desktop */
	UPROPERTY(EditAnywhere, config, Category = Rendering, meta = (DisplayName = "Enable Metal/Vulkan/High-end mobile Preview Rendering Level in editor"))
	bool bFeatureLevelES31Preview;

	/** Enable late joining in PIE */
	UPROPERTY(EditAnywhere, config, Category = PIE, meta = (DisplayName = "Allow late joining"))
	bool bAllowLateJoinInPIE;

	/** Allow Vulkan Preview */
	UPROPERTY(EditAnywhere, config, Category = PIE, meta = (DisplayName = "Allow Vulkan Mobile Preview"))
	bool bAllowVulkanPreview;

	/** Enable multithreaded lightmap encoding (decreases time taken to encode lightmaps) */
	UPROPERTY(EditAnywhere, config, Category = LightingBuilds, meta = (DisplayName = "Enable Multithreaded lightmap encoding"))
	bool bEnableMultithreadedLightmapEncoding;

	/** Enable multithreaded shadow map encoding (decreases time taken to encode shadow maps) */
	UPROPERTY(EditAnywhere, config, Category = LightingBuilds, meta = (DisplayName = "Enable Multithreaded shadowmap encoding"))
	bool bEnableMultithreadedShadowmapEncoding;
	
	/** Whether to use OpenCL to accelerate convex hull decomposition (uses GPU to decrease time taken to decompose meshes, currently only available on Mac OS X) */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Use OpenCL for convex hull decomposition"))
	bool bUseOpenCLForConvexHullDecomp;

	/** Enables a preview of the Unreal Editor in VR.  This adds a new tool bar button that allows you to toggle into "VR Mode" instantly.  This feature is still in development, but your feedback is appreciated! */
	UPROPERTY(EditAnywhere, config, Category=VR, meta = (DisplayName="Enable VR Editing"))
	bool bEnableVREditing;

	/**If true, wearing a Vive or Oculus Rift headset will automatically enter VR Editing mode if Enable VR Editing is true. */
	UPROPERTY(EditAnywhere, config, Category = VR, meta = (DisplayName = "Enable VR Mode Auto-Entry"))
	bool bEnableAutoVREditMode;

	/** Allows editing of potentially unsafe properties during PIE. Advanced use only - use with caution. */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Allow editing of potentially unsafe properties."))
	bool bAllowPotentiallyUnsafePropertyEditing;

	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UEditorExperimentalSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged( )
	{
		return SettingChangedEvent;
	}

protected:

	// UObject overrides

	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};
