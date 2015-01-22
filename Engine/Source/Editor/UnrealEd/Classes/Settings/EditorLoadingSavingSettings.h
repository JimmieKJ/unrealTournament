// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "EditorLoadingSavingSettings.generated.h"


/**
 * Implements the Level Editor's loading and saving settings.
 */
UCLASS(config=EditorUserSettings, autoexpandcategories=(AutoSave, AutoReimport, Blueprints))
class UNREALED_API UEditorLoadingSavingSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Whether to load a default example map at startup */
	UPROPERTY(EditAnywhere, config, Category=Startup)
	uint32 bLoadDefaultLevelAtStartup:1;

	/** Whether to restore previously open assets at startup */
	UPROPERTY(EditAnywhere, config, Category=Startup)
	uint32 bRestoreOpenAssetTabsOnRestart:1;

public:

	/** Directories being monitored for Auto Reimport */
	UPROPERTY(EditAnywhere, config, Category=AutoReimport, meta=(DisplayName="Auto Reimport Directories"))
	TArray<FString> AutoReimportDirectories;

	/**Automatically reimports textures when a change to source content is detected */
	UPROPERTY(EditAnywhere, config, Category=AutoReimport, meta=(DisplayName="Auto Reimport Textures"))
	bool bAutoReimportTextures;

	/**Automatically reimports CSV files when a change to source content is detected */
	UPROPERTY(EditAnywhere, config, Category=AutoReimport, meta=(DisplayName="Auto Reimport CSVData"))
	bool bAutoReimportCSV;

public:

	/** Whether to mark blueprints dirty if they are automatically migrated during loads */
	UPROPERTY(EditAnywhere, config, Category=Blueprints, meta=(DisplayName="Dirty Migrated Blueprints"))
	bool bDirtyMigratedBlueprints;

public:

	/** Whether to automatically save after a time interval */
	UPROPERTY(EditAnywhere, config, Category=AutoSave, meta=(DisplayName="Enable AutoSave"))
	uint32 bAutoSaveEnable:1;

	/** Whether to automatically save maps during an autosave */
	UPROPERTY(EditAnywhere, config, Category=AutoSave, meta=(DisplayName="Save Maps"))
	uint32 bAutoSaveMaps:1;

	/** Whether to automatically save content packages during an autosave */
	UPROPERTY(EditAnywhere, config, Category=AutoSave, meta=(DisplayName="Save Content"))
	uint32 bAutoSaveContent:1;

	/** The time interval after which to auto save */
	UPROPERTY(EditAnywhere, config, Category=AutoSave, meta=(DisplayName="Frequency in Minutes", ClampMin = "1"))
	int32 AutoSaveTimeMinutes;

	/** The number of seconds warning before an autosave*/
	UPROPERTY(EditAnywhere, config, Category=AutoSave, meta=(DisplayName="Warning in seconds", ClampMin = "0", UIMin = "0", UIMax = "20"))
	int32 AutoSaveWarningInSeconds;

public:

	/** Whether to automatically prompt for SCC checkout on asset modification */
	UPROPERTY(EditAnywhere, config, Category=SourceControl)
	uint32 bPromptForCheckoutOnAssetModification:1;

	/** Auto add files to source control */
	UPROPERTY(EditAnywhere, config, Category=SourceControl, meta=(DisplayName="Add New Files when Modified"))
	uint32 bSCCAutoAddNewFiles:1;

	/** Use global source control login settings, rather than per-project. Changing this will require you to login again */
	UPROPERTY(EditAnywhere, config, Category=SourceControl, meta=(DisplayName="Use Global Settings"))
	uint32 bSCCUseGlobalSettings:1;

	/** Specifies the file path to the tool to be used for diff'ing text files */
	UPROPERTY(EditAnywhere, config, Category=SourceControl, meta=(DisplayName="Tool for diff'ing text", FilePathFilter = "exe"))
	FFilePath TextDiffToolPath;

public:

	// @todo thomass: proper settings support for source control module
	void SccHackInitialize( );

public:

	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UEditorLoadingSavingSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged( ) { return SettingChangedEvent; }

protected:

	// UObject overrides

	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};
