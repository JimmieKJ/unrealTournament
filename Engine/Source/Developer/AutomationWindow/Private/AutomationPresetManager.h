// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of FAutomationTestPreset. */
typedef TSharedPtr<class FAutomationTestPreset> AutomationPresetPtr;

/** Type definition for shared references to instances of FAutomationTestPreset. */
typedef TSharedRef<class FAutomationTestPreset> AutomationPresetRef;

//Increment this number if the Preset serialization changes.
#define AUTOMATION_PRESETVERSION 1


/**
 * Class that holds preset data for the automation window
 */
class FAutomationTestPreset
{
public:

	/** Default constructor. */
	FAutomationTestPreset() :
		Id(FGuid::NewGuid())
	{ }

	/** Destructor. */
	virtual ~FAutomationTestPreset( ) { }

	/**
	 * Gets the GUid for this preset.
	 *
	 * @return The preset's unique identifier.
	 * @see GetPresetName
	 */
	const FGuid GetID()
	{
		return Id;
	}

	/**
	 * Gets the name for this preset.
	 *
	 * @return The preset's name.
	 * @see GetID, SetPresetName
	 */
	const FText& GetPresetName( ) const
	{
		return PresetName;
	}

	/**
	 * Sets the name for this preset.
	 *
	 * @param InPresetName The name to set.
	 * @see GetPresetName
	 */
	void SetPresetName(const FText& InPresetName)
	{
		PresetName = InPresetName;
	}

	/**
	 * Returns the list of enabled tests.
	 *
	 * @return Collection of tests.
	 * @see SetEnabledTests
	 */
	const TArray<FString>& GetEnabledTests() const
	{
		return EnabledTests;
	}

	/**
	 * Sets the list of enabled tests.
	 *
	 * @param NewEnableDtests The collection of tests to enable.
	 * @see GetEnabledTests
	 */
	void SetEnabledTests(TArray<FString> NewEnabledTests)
	{
		EnabledTests = NewEnabledTests;
	}

	/**
	 * Handles saving / loading the preset data to an archive (file).
	 *
	 * @param Archive The archive to serialize from/to.
	 */
	virtual bool Serialize( FArchive& Archive )
	{
		int32 Version = AUTOMATION_PRESETVERSION;

		Archive	<< Version;

		if (Version != AUTOMATION_PRESETVERSION)
		{
			return false;
		}

		Archive << Id;
		Archive << PresetName;
		Archive << EnabledTests;

		return true;
	}

private:

	/** A unique ID for this preset. (Used for the filename because the PresetName may have invalid characters). */
	FGuid Id;

	/** The name of this preset. */
	FText PresetName;

	/** The list of enabled test names. */
	TArray<FString> EnabledTests;
};


class FAutomationTestPresetManager
{
public:

	/** Default constructor. */
	FAutomationTestPresetManager();

	/** Virtual destructor. */
	virtual ~FAutomationTestPresetManager() { }

	/**
	 * Creates a new empty preset.
	 */
	virtual AutomationPresetRef AddNewPreset( );

	/**
	 * Creates a new preset with the given name and enabled tests.
	 * @param PresetName The name of the new preset.
	 * @param SelectedTests The list of enabled tests.
	 */
	virtual AutomationPresetRef AddNewPreset( const FText& PresetName, const TArray<FString>& SelectedTests );

	/**
	 * Returns a reference to the list that holds the presets.
	 */
	virtual TArray<AutomationPresetPtr>& GetAllPresets( );

	/**
	 * Removes the selected preset from the preset list and deletes the file on disk.
	 *
	 * @param Preset The preset to remove.
	 * @see LoadPresets, SavePreset
	 */
	void RemovePreset( const AutomationPresetRef Preset );

	/**
	 * Saves the passed in preset to disk.
	 *
	 * @param Preset The preset to save.
	 * @see RemovePreset, LoadPresets
	 */
	void SavePreset( const AutomationPresetRef Preset);

	/**
	 * Load all Presets from disk.
	 *
	 * @see RemovePreset, SavePreset
	 */
	void LoadPresets( );

protected:

	/**
	 * Creates a new preset and loads it with data from the archive.
	 *
	 * @param Archive The stream to read the preset data from.
	 */
	AutomationPresetPtr LoadPreset( FArchive& Archive );

	/**
	 * Writes the Preset data to the archive.
	 *
	 * @param Preset The preset to serialize.
	 * @param Archive The stream to write the preset data to.
	 */
	void SavePreset( const AutomationPresetRef Preset, FArchive& Archive );

	/**
	 * Gets the folder in which preset files are stored.
	 *
	 * @return The folder path.
	 */
	static FString GetPresetFolder( )
	{
		return FPaths::EngineSavedDir() / TEXT("Automation");
	}

private:

	/** Holds the collection of automation presets. */
	TArray<AutomationPresetPtr> Presets;
};
