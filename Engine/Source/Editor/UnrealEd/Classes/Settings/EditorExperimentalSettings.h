// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorExperimentalSettings.h: Declares the UEditorExperimentalSettings class.
=============================================================================*/

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

	/** Enable Single Layout BP Editor. */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = ( DisplayName = "Single Layout Blueprint Editor" ))
	bool bUnifiedBlueprintEditor;

	/** Enable being able to subclass components in blueprints */
	UPROPERTY(EditAnywhere, config, Category=Tools, meta=(ConfigRestartRequired=true))
	bool bBlueprintableComponents;

	/** The Messaging Debugger provides a visual utility for debugging the messaging system. */
	UPROPERTY(EditAnywhere, config, Category=Tools, meta=(DisplayName="Messaging Debugger"))
	bool bMessagingDebugger;

	/** Allows to use actor merging utilities (Simplygon Proxy LOD, Grouping by Materials)*/
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Actor Merging"))
	bool bActorMerging;

	/** Specify which console-specific nomenclature to use for gamepad label text */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, meta=(DisplayName="Console for Gamepad Labels"))
	TEnumAsByte<EConsoleForGamepadLabels::Type> ConsoleForGamepadLabels;

	/** Allows for customization of toolbars and menus throughout the editor */
	UPROPERTY(config)
	bool bToolbarCustomization;

	/** Break on Exceptions allows you to trap Access Nones and other exceptional events in Blueprints. */
	UPROPERTY(EditAnywhere, config, Category=Blueprints, meta=(DisplayName="Blueprint Break on Exceptions"))
	bool bBreakOnExceptions;

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

	/** Enable -iterate for launch on */
	UPROPERTY(EditAnywhere, config, Category = Cooking, meta = (DisplayName = "Iterative cooking for builds launched form the editor (launch on)"))
	bool bIterativeCookingForLaunchOn;

	/** Enables Environment Queries editor */
	UPROPERTY(EditAnywhere, config, Category = AI, meta = (DisplayName = "Environment Querying System"))
	bool bEQSEditor;

	/** Enables Environment Queries editor */
	UPROPERTY(EditAnywhere, config, Category = Rendering, meta = (DisplayName = "Enable Metal/High-end mobile rendering preview"))
	bool bFeatureLevelES31Preview;

	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UEditorExperimentalSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged( ) { return SettingChangedEvent; }

protected:

	// UObject overrides

	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};
