// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SystemSettings.cpp: Unreal engine HW compat scalability system.
=============================================================================*/

#pragma once

#include "TextureLODSettings.h"
#include "ShowFlags.h"

/*-----------------------------------------------------------------------------
	System settings and scalability options.
-----------------------------------------------------------------------------*/

/** Augments TextureLODSettings with access to TextureLODGroups. */
struct FExposedTextureLODSettings : public FTextureLODSettings
{
public:
	/** @return		A handle to the indexed LOD group. */
	FTextureLODGroup& GetTextureLODGroup(int32 GroupIndex)
	{
		check( GroupIndex >= 0 && GroupIndex < TEXTUREGROUP_MAX );
		return TextureLODGroups[GroupIndex];
	}
	/** @return		A handle to the indexed LOD group. */
	const FTextureLODGroup& GetTextureLODGroup(int32 GroupIndex) const
	{
		check( GroupIndex >= 0 && GroupIndex < TEXTUREGROUP_MAX );
		return TextureLODGroups[GroupIndex];
	}
};

/** Encapsulates all the system settings that make up Texture Detail friendly settings */
struct FSystemSettingsDataTextureDetail
{
	FSystemSettingsDataTextureDetail();

	friend bool operator==(const FSystemSettingsDataTextureDetail& LHS, const FSystemSettingsDataTextureDetail& RHS)
	{
		// WRH - 2007/09/22 - This relies on the fact that the various settings are binary comparable
		return FMemory::Memcmp(&LHS, &RHS, sizeof(LHS)) == 0;
	}
	friend bool operator!=(const FSystemSettingsDataTextureDetail& LHS, const FSystemSettingsDataTextureDetail& RHS)
	{
		return !(LHS==RHS);
	}

	/** Global texture LOD settings.									*/
	FExposedTextureLODSettings TextureLODSettings;
};

/** 
 * Struct that holds the actual data for the system settings. 
 *
 * Uses the insane derivation for backwards compatibility purposes only. Would be cleaner to use composition.
 */
struct FSystemSettingsData 
	: public FSystemSettingsDataTextureDetail
{
	/** ctor */
	FSystemSettingsData();

	/** loads settings from the given section in the given ini */
	void LoadFromIni(const TCHAR* IniSection, const FString& IniFilename = GEngineIni, bool bAllowMissingValues = true, bool* FoundValues=NULL);

	/** saves settings to the given section in the engine ini */
	void SaveToIni(const TCHAR* IniSection);
	
private:
	/**
	 * Returns a string for the specified texture group LOD settings to the specified ini.
	 *
	 * @param	TextureGroupID		Index/enum of the group
	 * @param	GroupName			String representation of the texture group
	 */
	FString GetLODGroupString( TextureGroup TextureGroupID, const TCHAR* GroupName );

	void WriteTextureLODGroupsToIni(const TCHAR* IniSection);
	void WriteTextureLODGroupToIni(TextureGroup TextureGroupID, const TCHAR* GroupName, const TCHAR* IniSection);

	void DumpTextureLODGroups();
	void DumpTextureLODGroup(TextureGroup TextureGroupID, const TCHAR* GroupName);
};


/**
 * Class that actually holds the current system settings
 *
 * Derive from FSystemSettingsData instead of holding one purely for backwards
 * compatibility reasons (each data element used to be a member of the class).
 */
class FSystemSettings : public FSystemSettingsData, public FExec, private FNoncopyable
{
public:
	/** Constructor, initializing all member variables. */
	FSystemSettings();

	/**
	 * Initializes system settings and included texture LOD settings.
	 *
	 * @param bSetupForEditor	Whether to initialize settings for Editor
	 */
	ENGINE_API void Initialize( bool bSetupForEditor );

	/**
	 * Sets new system settings (optionally writes out to the ini).
	 */
	ENGINE_API void ApplyNewSettings(const FSystemSettingsData& NewSettings, bool bWriteToIni);

	/** Applies setting overrides based on command line options. */
	void ApplyOverrides();
	
	// Begin Exec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	// End Exec Interface

	/** Mask where 1 bits mean we want to force the engine show flag to be off */
	const FEngineShowFlags& GetForce0Mask() const { return Force0Mask; }
	/** Mask where 1 bits mean we want to force the engine show flag to be on */
	const FEngineShowFlags& GetForce1Mask() const { return Force1Mask; }

private:
	/** Since System Settings is called into before GIsEditor is set, we must cache this value. */
	bool bIsEditor;

	/** Mask where 1 bits mean we want to force the engine show flag to be off */
	FEngineShowFlags Force0Mask;
	/** Mask where 1 bits mean we want to force the engine show flag to be on */
	FEngineShowFlags Force1Mask;

	/**
	 * Loads settings from the ini. (purposely override the inherited name so people can't accidentally call it.)
	 */
	void LoadFromIni();

	/**
	 * Saves current settings to the ini. (purposely override the inherited name so people can't accidentally call it.)
	 */
	void SaveToIni();

	/**
	 * Reads a single entry and parses it into the group array.
	 *
	 * @param	TextureGroupID		Index/enum of group to parse
	 * @param	MinLODSize			Minimum size, in pixels, below which the code won't bias.
	 * @param	MaxLODSize			Maximum size, in pixels, above which the code won't bias.
	 * @param	LODBias				Group LOD bias.
	 * @param	MipGenSettings		Defines how the the mip-map generation works, e.g. sharpening
	 */
	void SetTextureLODGroup(TextureGroup TextureGroupID, int MinLODSize, int32 MaxLODSize, int32 LODBias, TextureMipGenSettings MipGenSettings);

	/**
	 * Writes all texture group LOD settings to the specified ini.
	 *
	 * @param	IniFilename			The .ini file to save to.
	 * @param	IniSection			The .ini section to save to.
	 */
	void WriteTextureLODGroupsToIni(const TCHAR* IniSection);

	/**
	 * Writes the specified texture group LOD settings to the specified ini.
	 *
	 * @param	TextureGroupID		Index/enum of the group to parse
	 * @param	GroupName			String representation of the texture group, to be used as the ini key.
	 * @param	IniSection			The .ini section to save to.
	 */
	void WriteTextureLODGroupToIni(TextureGroup TextureGroupID, const TCHAR* GroupName, const TCHAR* IniSection);

	/**
	 * Helper for ApplyNewSettings when the engine is running. Applies the changes needed for the runtime system.
	 *
	 * We can assume the game is running if this code is called.
	 */
	void ApplySettingsAtRuntime(const FSystemSettingsData& NewSettings, bool bWriteToInii);

	/** 
	 * Makes System Settings take effect on the rendering thread 
	 */
	void RegisterShowFlagConsoleVariables();

	/** 
	 * Console variable sink - called when cvars have changed
	 */
	void CVarSink();
};

/**
 * Global system settings accessor
 */
extern ENGINE_API FSystemSettings GSystemSettings;
