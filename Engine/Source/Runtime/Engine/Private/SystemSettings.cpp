// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScalabilityOptions.cpp: Unreal engine HW compat scalability system.
=============================================================================*/

#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "TargetPlatform.h"

DEFINE_LOG_CATEGORY_STATIC(LogSystemSettings, Log, All);

static TAutoConsoleVariable<int32> CVarUseMaxQualityMode(
	TEXT("r.MaxQualityMode"),
	0,
	TEXT("If set to 1, override certain system settings to highest quality regardless of performance impact"),
	ECVF_RenderThreadSafe);

/*-----------------------------------------------------------------------------
	FSystemSettings
-----------------------------------------------------------------------------*/

/** Global accessor */
FSystemSettings GSystemSettings;

/**
 * Helpers for writing to specific ini sections
 */
static const TCHAR* GIniSectionGame = TEXT("SystemSettings");
static const TCHAR* GIniSectionEditor = TEXT("SystemSettingsEditor");

static inline const TCHAR* GetSectionName(bool bIsEditor)
{
	// if we are cooking, look for an override on the commandline
	FString OverrideSubName;
	if (FParse::Value(FCommandLine::Get(), TEXT("-SystemSettings="), OverrideSubName))
	{
		// append it to SystemSettings, unless it starts with SystemSettings
		static TCHAR ReturnedOverrideName[256] = TEXT("SystemSettings");
		// cache how long the SystemSettings is
		static int32 SystemSettingsStrLen = FCString::Strlen(ReturnedOverrideName);
		if (FCString::Strnicmp(*OverrideSubName, TEXT("SystemSettings"), SystemSettingsStrLen) == 0)
		{
			FCString::Strcpy(ReturnedOverrideName + SystemSettingsStrLen, ARRAY_COUNT(ReturnedOverrideName), (*OverrideSubName + SystemSettingsStrLen));
		}
		else
		{
			FCString::Strcpy(ReturnedOverrideName + SystemSettingsStrLen, ARRAY_COUNT(ReturnedOverrideName), *OverrideSubName);
		}
		return ReturnedOverrideName;
	}

	// return the proper section for using editor or not
	return bIsEditor ? GIniSectionEditor : GIniSectionGame;
}

/**
 * ctor
 */
FSystemSettingsDataTextureDetail::FSystemSettingsDataTextureDetail()
{
}

/**
 * ctor
 */
FSystemSettingsData::FSystemSettingsData()
{
}

// implements a FKeyValueSink
class FSetCVarFromIni
{
public:
	static void OnEntry(const TCHAR *Key, const TCHAR* Value)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Key);

		if(CVar)
		{
			// Set if the variable exists.

			// to be more compatible with non console variable boolean system settings we allow verbal values

			if(FCString::Stricmp(Value, TEXT("True")) == 0
			|| FCString::Stricmp(Value, TEXT("Yes")) == 0
			|| FCString::Stricmp(Value, TEXT("On")) == 0)
			{
				CVar->Set(1, ECVF_SetBySystemSettingsIni);
			}
			else
			if(FCString::Stricmp(Value, TEXT("False")) == 0
			|| FCString::Stricmp(Value, TEXT("No")) == 0
			|| FCString::Stricmp(Value, TEXT("Off")) == 0)
			{
				CVar->Set(0, ECVF_SetBySystemSettingsIni);
			}
			else
			{
				CVar->Set(Value, ECVF_SetBySystemSettingsIni);
			}
		}
	}
};

/** Initializes and instance with the values from the given IniSection in the engine ini */
void FSystemSettingsData::LoadFromIni( const TCHAR* IniSection, const FString& IniFilename, bool bAllowMissingValues, bool* FoundValues )
{
	// first, look for a parent section to base off of
	FString BasedOnSection;
	if (GConfig->GetString(IniSection, TEXT("BasedOn"), BasedOnSection, IniFilename))
	{
		// recurse with the BasedOn section if it existed, always allowing for missing values
		LoadFromIni(*BasedOnSection, IniFilename, true, FoundValues);
	}

	// Read all console variables from .ini
	{
		GConfig->ForEachEntry(FKeyValueSink::CreateStatic(&FSetCVarFromIni::OnEntry), IniSection, IniFilename);

		IConsoleManager::Get().CallAllConsoleVariableSinks();
	}


	// Read the texture group LOD settings.
	TextureLODSettings.Initialize( IniFilename, IniSection );
}

void FSystemSettingsData::SaveToIni( const TCHAR* IniSection )
{
	// Save the texture group LOD settings.
	WriteTextureLODGroupsToIni( IniSection );

	GConfig->Flush( false, GEngineIni );
}

/**
 * Writes all texture group LOD settings to the specified ini.
 *
 * @param	IniSection			The .ini section to save to.
 */
void FSystemSettingsData::WriteTextureLODGroupsToIni(const TCHAR* IniSection)
{
#define WRITETEXTURELODGROUPTOINI(Group) WriteTextureLODGroupToIni(Group, TEXT(#Group), IniSection);
	FOREACH_ENUM_TEXTUREGROUP(WRITETEXTURELODGROUPTOINI)
#undef WRITETEXTURELODGROUPTOINI
}

/**
* Returns a string for the specified texture group LOD settings to the specified ini.
*
* @param	TextureGroupID		Index/enum of the group
* @param	GroupName			String representation of the texture group
*/
FString FSystemSettingsData::GetLODGroupString( TextureGroup TextureGroupID, const TCHAR* GroupName )
{
	const FExposedTextureLODSettings::FTextureLODGroup& Group = TextureLODSettings.GetTextureLODGroup(TextureGroupID);

	const int32 MinLODSize = 1 << Group.MinLODMipCount;
	const int32 MaxLODSize = 1 << Group.MaxLODMipCount;

	FName MinMagFilter = NAME_Aniso;
	FName MipFilter = NAME_Linear;
	switch(Group.Filter)
	{
		case SF_Point:
			MinMagFilter = NAME_Point;
			MipFilter = NAME_Point;
			break;
		case SF_Bilinear:
			MinMagFilter = NAME_Linear;
			MipFilter = NAME_Point;
			break;
		case SF_Trilinear:
			MinMagFilter = NAME_Linear;
			MipFilter = NAME_Linear;
			break;
		case SF_AnisotropicPoint:
			MinMagFilter = NAME_Aniso;
			MipFilter = NAME_Point;
			break;
		case SF_AnisotropicLinear:
			MinMagFilter = NAME_Aniso;
			MipFilter = NAME_Linear;
			break;
	}

	FString NumStreamedMipsText;
	if ( Group.NumStreamedMips >= 0 )
	{
		NumStreamedMipsText = FString::Printf( TEXT(",NumStreamedMips=%i"), Group.NumStreamedMips );
	}

	return FString::Printf( TEXT("(MinLODSize=%i,MaxLODSize=%i,LODBias=%i,MinMagFilter=%s,MipFilter=%s%s,MipGenSettings=%s)"),
		MinLODSize, MaxLODSize, Group.LODBias, *MinMagFilter.GetPlainNameString(), *MipFilter.GetPlainNameString(), *NumStreamedMipsText, UTexture::GetMipGenSettingsString(Group.MipGenSettings) );
}

/**
 * Writes the specified texture group LOD settings to the specified ini.
 *
 * @param	TextureGroupID		Index/enum of the group to parse
 * @param	GroupName			String representation of the texture group, to be used as the ini key.
 * @param	IniSection			The .ini section to save to.
 */
void FSystemSettingsData::WriteTextureLODGroupToIni(TextureGroup TextureGroupID, const TCHAR* GroupName, const TCHAR* IniSection)
{
	const FString Entry = GetLODGroupString( TextureGroupID, GroupName );
	GConfig->SetString( IniSection, GroupName, *Entry, GEngineIni );
}

/**
 * Dump helpers
 */
void FSystemSettingsData::DumpTextureLODGroups()
{
#define DUMPTEXTURELODGROUP(Group) DumpTextureLODGroup(Group, TEXT(#Group));
	FOREACH_ENUM_TEXTUREGROUP(DUMPTEXTURELODGROUP)
#undef DUMPTEXTURELODGROUP
}

/**
 * Dump helpers
 */
void FSystemSettingsData::DumpTextureLODGroup(TextureGroup TextureGroupID, const TCHAR* GroupName)
{
	const FString Entry = GetLODGroupString( TextureGroupID, GroupName );
	UE_LOG(LogSystemSettings, Log, TEXT("\t%s: %s"), GroupName, *Entry );
}

/**
 * Constructor, initializing all member variables.
 */
FSystemSettings::FSystemSettings() :
	bIsEditor( false ),
	Force0Mask(ESFIM_All0),
	Force1Mask(ESFIM_All0)
{
	// there should only ever be one of these
	static int32 InstanceCount = 0;
	++InstanceCount;
	check(InstanceCount == 1);
}

void FSystemSettings::RegisterShowFlagConsoleVariables()
{
	struct FIterSink
	{
		FIterSink(FSystemSettings& InThis) : This(InThis)
		{
		}

		bool OnEngineShowFlag(uint32 InIndex, const FString& InName)
		{
			uint32 ShowFlagIndex = FEngineShowFlags::FindIndexByName(*InName);

			// internal error
			check(ShowFlagIndex != -1);

			// Note: showflags cvars should not be used as options because they are considered as cheat cvars which cannot be altered by the user
			IConsoleManager::Get().RegisterConsoleVariableBitRef(*(FString("ShowFlag.") + InName), 
				*InName,
				ShowFlagIndex,
				(uint8*)&This.Force0Mask,
				(uint8*)&This.Force1Mask,
				TEXT("ShowFlag 0:force off, 1:force on, 2 or other value to not apply any adjustments"),
				ECVF_Cheat);

			return true;
		}

		FSystemSettings& This;
	};
		
	FIterSink Sink(*this);

	FEngineShowFlags::IterateAllFlags(Sink);
}

/**
 * a few tests to make sure this platform behaves as we expect it
 * if not: ExtractBoolFromBitfield and SetBoolInBitField needs to be modified
 */
static void TestBitFieldFunctions()
{
#if DO_CHECK
	struct FTestMask
	{
		// 48 bits should be enough for testing endianess 
		uint32 B00:1; uint32 B01:1; uint32 B02:1; uint32 B03:1; uint32 B04:1; uint32 B05:1; uint32 B06:1; uint32 B07:1; uint32 B08:1; uint32 B09:1; uint32 B0a:1; uint32 B0b:1; uint32 B0c:1; uint32 B0d:1; uint32 B0e:1; uint32 B0f:1;
		uint32 B10:1; uint32 B11:1; uint32 B12:1; uint32 B13:1; uint32 B14:1; uint32 B15:1; uint32 B16:1; uint32 B17:1; uint32 B18:1; uint32 B19:1; uint32 B1a:1; uint32 B1b:1; uint32 B1c:1; uint32 B1d:1; uint32 B1e:1; uint32 B1f:1;
		uint32 B20:1; uint32 B21:1; uint32 B22:1; uint32 B23:1; uint32 B24:1; uint32 B25:1; uint32 B26:1; uint32 B27:1; uint32 B28:1; uint32 B29:1; uint32 B2a:1; uint32 B2b:1; uint32 B2c:1; uint32 B2d:1; uint32 B2e:1; uint32 B2f:1;
	};

	FTestMask Mask;
	uint8* Mem = (uint8*)&Mask;

	// test cases
	{
		memset(&Mask, 0, sizeof(FTestMask));
		Mask.B00 = 1;
		check(FMath::ExtractBoolFromBitfield(Mem, 0x00) == true);
		FMath::SetBoolInBitField(Mem, 0x00, false);
		check(!Mask.B00);
	}
	{
		memset(&Mask, 0, sizeof(FTestMask));
		Mask.B11 = 1;
		check(FMath::ExtractBoolFromBitfield(Mem, 0x11) == true);
		FMath::SetBoolInBitField(Mem, 0x11, false);
		check(!Mask.B11);
	}
	{
		memset(&Mask, 0, sizeof(FTestMask));
		Mask.B22 = 1;
		check(FMath::ExtractBoolFromBitfield(Mem, 0x22) == true);
		FMath::SetBoolInBitField(Mem, 0x22, false);
		check(!Mask.B11);
	}
#endif
}

/**
 * Initializes system settings and included texture LOD settings.
 *
 * @param bSetupForEditor	Whether to initialize settings for Editor
 */
void FSystemSettings::Initialize( bool bSetupForEditor )
{
	TestBitFieldFunctions();

	RegisterShowFlagConsoleVariables();

	// Load the settings that will be the default for every other compat level, and the editor, and the other split screen levels.
	FSystemSettingsData DefaultSettings;
	DefaultSettings.LoadFromIni(GetSectionName(false), GEngineIni, false);

	bIsEditor = bSetupForEditor;

	(FSystemSettingsData&)(*this) = DefaultSettings;
	LoadFromIni();

	ApplyOverrides();

	IConsoleManager::Get().RegisterConsoleVariableSink_Handle(FConsoleCommandDelegate::CreateRaw(this, &FSystemSettings::CVarSink));

	// intialize a critical texture streaming value used by texture loading, etc
	int32 MinTextureResidentMipCount = 1;
	verify(GConfig->GetInt(TEXT("TextureStreaming"), TEXT("MinTextureResidentMipCount"), MinTextureResidentMipCount, GEngineIni));
	UTexture2D::SetMinTextureResidentMipCount(MinTextureResidentMipCount);
}

void FSystemSettings::CVarSink()
{
	ApplyOverrides();
}

bool FSystemSettings::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// no longer needed, we have the "Scalability" console command
	return false;
}

void FSystemSettings::SetTextureLODGroup(TextureGroup TextureGroupID, int32 MinLODSize, int32 MaxLODSize, int32 LODBias, TextureMipGenSettings MipGenSettings)
{
	TextureLODSettings.GetTextureLODGroup(TextureGroupID).MinLODMipCount	= FMath::CeilLogTwo( MinLODSize );
	TextureLODSettings.GetTextureLODGroup(TextureGroupID).MaxLODMipCount	= FMath::CeilLogTwo( MaxLODSize );
	TextureLODSettings.GetTextureLODGroup(TextureGroupID).LODBias			= LODBias;
	TextureLODSettings.GetTextureLODGroup(TextureGroupID).MipGenSettings	= MipGenSettings;
}

/*-----------------------------------------------------------------------------
	Resolution.
-----------------------------------------------------------------------------*/

/**
 * Overriden function that selects the proper ini section to write to
 */
void FSystemSettings::LoadFromIni()
{
	FSystemSettingsData::LoadFromIni(GetSectionName(bIsEditor));
}

void FSystemSettings::SaveToIni()
{
	// don't write changes in the editor
	if (bIsEditor)
	{
		UE_LOG(LogSystemSettings, Log, TEXT("Can't save system settings to ini in an editor mode"));
		return;
	}
	FSystemSettingsData::SaveToIni(GetSectionName(bIsEditor));
}

/**
 * Helper for ApplyNewSettings when the engine is running. Applies the changes needed for the runtime system.
 *
 * We can assume the game is running if this code is called.
 */
void FSystemSettings::ApplySettingsAtRuntime(const FSystemSettingsData& NewSettings, bool bWriteToIni)
{
	// Some of these settings are shared between threads, so we
	// must flush the rendering thread before changing anything.
	FlushRenderingCommands();

	// Track settings we might have to put back
	FExposedTextureLODSettings InMemoryTextureLODSettings = TextureLODSettings;

	// Read settings from .ini.  This is necessary because settings which need to wait for a restart
	// will be on disk but may not be in memory.  Therefore, we read from disk before capturing old
	// values to revert to.
	LoadFromIni();

	// see what settings are actually changing.
	// Ugly casts because system settings is multi-inherited from all the consituent classes for backwards compatibility

	// Texture Detail
	bool bTextureDetailChanged = (const FSystemSettingsDataTextureDetail&)(*this) != (const FSystemSettingsDataTextureDetail&)NewSettings;

	// Make a copy of the existing settings so we can compare for changes
	FSystemSettingsData OldData = *this;

	// Set new settings. Would look prettier if we didn't derive from the Data class...
	(FSystemSettingsData&)(*this) = NewSettings;

	// apply any runtime changes that need to be made
	bool bUpdateTextureStreamingSucceeded = false;
	if (bTextureDetailChanged)
	{
		bUpdateTextureStreamingSucceeded = UTexture::ForceUpdateTextureStreaming();
	}

	// If requested, save the settings to ini.
	if ( bWriteToIni )
	{
		SaveToIni();
	}

	// If texture detail settings couldn't be applied because we're loading seekfree,
	// revert the new settings to their previous in-memory values.
	if ( bTextureDetailChanged && !bUpdateTextureStreamingSucceeded )
	{
		TextureLODSettings = InMemoryTextureLODSettings;
	}
}

/**
 * Sets new system settings (optionally writes out to the ini). 
 */
void FSystemSettings::ApplyNewSettings( const FSystemSettingsData& NewSettings, bool bWriteToIni )
{
	// we can set any setting before the engine is initialized so don't bother restoring values.
	bool bEngineIsInitialized = GEngine != NULL;

	// if the engine is running, there are certain values we can't set immediately
	if (bEngineIsInitialized)
	{
		// apply settings to the runtime system.
		ApplySettingsAtRuntime(NewSettings, bWriteToIni);

		ApplyOverrides();
	}
	else
	{
		// if the engine is not initialized we don't need to worry about all the deferred settings etc. 
		// as we do above.
		// Set new settings. Would look prettier if we didn't derive from the Data class...
		(FSystemSettingsData&)(*this) = NewSettings;

		// If requested, save the settings to ini.
		if ( bWriteToIni )
		{
			SaveToIni();
		}

		ApplyOverrides();
	}
}

void FSystemSettings::ApplyOverrides()
{
	EConsoleVariableFlags SetBy = ECVF_SetByMask;

	if (FPlatformProperties::SupportsWindowedMode())
	{
		if (CVarUseMaxQualityMode.GetValueOnGameThread() != 0)
		{
			SetBy = (EConsoleVariableFlags)(CVarUseMaxQualityMode.AsVariable()->GetFlags() & ECVF_SetByMask);
		}

		if (FParse::Param(FCommandLine::Get(),TEXT("MAXQUALITYMODE")))
		{
			SetBy = ECVF_SetByCommandline;
		}
	}

	if (SetBy != ECVF_SetByMask)
	{
		// Modify various system settings to get the best quality regardless of performance impact
		{
			static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MinResolution"));
			CVar->Set(16, SetBy);
		}

		// Disable shadow fading out over distance
		{
			static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.FadeResolution"));
			CVar->Set(1, SetBy);
		}

		// Increase minimum preshadow resolution
		{
			static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MinPreShadowResolution"));
			CVar->Set(16, SetBy);
		}

		// Disable preshadow fading out over distance
		{
			static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.PreShadowFadeResolution"));
			CVar->Set(1, SetBy);
		}

		// Increase shadow texel density
		{
			static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.TexelsPerPixel"));
			CVar->Set(4.0f, SetBy);
		}

		// Don't downsample preshadows
		{
			static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.PreShadowResolutionFactor"));
			CVar->Set(1.0f, SetBy);
		}

		for (int32 GroupIndex = 0; GroupIndex < TEXTUREGROUP_MAX; GroupIndex++)
		{
			FTextureLODSettings::FTextureLODGroup& CurrentGroup = TextureLODSettings.GetTextureLODGroup(GroupIndex);
			// Use the best quality texture filtering
			CurrentGroup.Filter = SF_AnisotropicLinear;
			// Raise texture max sizes to 4096
			CurrentGroup.MinLODMipCount = 12;
			CurrentGroup.MaxLODMipCount = 12;
			CurrentGroup.LODBias = -1000;
		}
	}
}

