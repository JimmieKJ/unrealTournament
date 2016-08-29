// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectHash.cpp: Unreal object name hashes
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "Projects.h"
#include "PackageLocalizationManager.h"
#include "HAL/ThreadHeartBeat.h"

DEFINE_LOG_CATEGORY_STATIC(LogPackageName, Log, All);

FString FPackageName::AssetPackageExtension = TEXT(".uasset");
FString FPackageName::MapPackageExtension = TEXT(".umap");

/** Event that is triggered when a new content path is mounted */
FPackageName::FOnContentPathMountedEvent FPackageName::OnContentPathMountedEvent;

/** Event that is triggered when a content path is dismounted */
FPackageName::FOnContentPathDismountedEvent FPackageName::OnContentPathDismountedEvent;

namespace PackageNameConstants
{
	// Minimum theoretical package name length ("/A/B") is 4
	const int32 MinPackageNameLength = 4;
}

bool FPackageName::IsShortPackageName(const FString& PossiblyLongName)
{
	return !PossiblyLongName.Contains(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
}

bool FPackageName::IsShortPackageName(const FName PossiblyLongName)
{
	return IsShortPackageName(PossiblyLongName.ToString());
}

FString FPackageName::GetShortName(const FString& LongName)
{
	const int32 IndexOfLastSlash = LongName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return LongName.Mid(IndexOfLastSlash + 1);
}

FString FPackageName::GetShortName(const UPackage* Package)
{
	check(Package != NULL);
	return GetShortName(Package->GetName());
}

FString FPackageName::GetShortName(const FName& LongName)
{
	return GetShortName(LongName.ToString());
}

FString FPackageName::GetShortName(const TCHAR* LongName)
{
	return GetShortName(FString(LongName));
}


FName FPackageName::GetShortFName(const FString& LongName)
{
	const int32 IndexOfLastSlash = LongName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return FName(*LongName.Mid(IndexOfLastSlash + 1));
}

FName FPackageName::GetShortFName(const FName& LongName)
{
	return GetShortFName(LongName.ToString());
}

FName FPackageName::GetShortFName(const TCHAR* LongName)
{
	return GetShortFName(FString(LongName));
}

struct FPathPair
{
	// The virtual path (e.g., "/Engine/")
	const FString RootPath;

	// The physical relative path (e.g., "../../../Engine/Content/")
	const FString ContentPath;

	bool operator ==(const FPathPair& Other) const
	{
		return RootPath == Other.RootPath && ContentPath == Other.ContentPath;
	}

	// Construct a path pair
	FPathPair(const FString& InRootPath, const FString& InContentPath)
		: RootPath(InRootPath)
		, ContentPath(InContentPath)
	{
	}
};

struct FLongPackagePathsSingleton
{
	FString ConfigRootPath;
	FString EngineRootPath;
	FString GameRootPath;
	FString ScriptRootPath;
	FString TempRootPath;
	TArray<FString> MountPointRootPaths;

	FString EngineContentPath;
	FString ContentPathShort;
	FString EngineShadersPath;
	FString EngineShadersPathShort;
	FString GameContentPath;
	FString GameConfigPath;
	FString GameScriptPath;
	FString GameSavedPath;
	FString GameContentPathRebased;
	FString GameConfigPathRebased;
	FString GameScriptPathRebased;
	FString GameSavedPathRebased;

	//@TODO: Can probably consolidate these into a single array, if it weren't for EngineContentPathShort
	TArray<FPathPair> ContentRootToPath;
	TArray<FPathPair> ContentPathToRoot;

	// singleton
	static FLongPackagePathsSingleton& Get()
	{
		static FLongPackagePathsSingleton Singleton;
		return Singleton;
	}

	void GetValidLongPackageRoots(TArray<FString>& OutRoots, bool bIncludeReadOnlyRoots) const
	{
		OutRoots.Add(EngineRootPath);
		OutRoots.Add(GameRootPath);
		OutRoots += MountPointRootPaths;

		if (bIncludeReadOnlyRoots)
		{
			OutRoots.Add(ConfigRootPath);
			OutRoots.Add(ScriptRootPath);
			OutRoots.Add(TempRootPath);
		}
	}

	// This will insert a mount point at the head of the search chain (so it can overlap an existing mount point and win)
	void InsertMountPoint(const FString& RootPath, const FString& ContentPath)
	{
		// Make sure the content path is stored as a relative path, consistent with the other paths we have
		FString RelativeContentPath = IFileManager::Get().ConvertToRelativePath( *ContentPath );

		// Make sure the path ends in a trailing path separator.  We are expecting that in the InternalFilenameToLongPackageName code.
		if( !RelativeContentPath.EndsWith( TEXT( "/" ), ESearchCase::CaseSensitive ) )
		{
			RelativeContentPath += TEXT( "/" );
		}

		FPathPair Pair(RootPath, RelativeContentPath);
		ContentRootToPath.Insert(Pair, 0);
		ContentPathToRoot.Insert(Pair, 0);
		MountPointRootPaths.Add( RootPath );

		// Let subscribers know that a new content path was mounted
		FPackageName::OnContentPathMounted().Broadcast( RootPath, ContentPath );
	}

	// This will remove a previously inserted mount point
	void RemoveMountPoint(const FString& RootPath, const FString& ContentPath)
	{
		// Make sure the content path is stored as a relative path, consistent with the other paths we have
		FString RelativeContentPath = IFileManager::Get().ConvertToRelativePath(*ContentPath);

		// Make sure the path ends in a trailing path separator.  We are expecting that in the InternalFilenameToLongPackageName code.
		if (!RelativeContentPath.EndsWith(TEXT("/"), ESearchCase::CaseSensitive))
		{
			RelativeContentPath += TEXT("/");
		}

		if ( MountPointRootPaths.Remove(RootPath) > 0 )
		{
			FPathPair Pair(RootPath, RelativeContentPath);
			ContentRootToPath.Remove(Pair);
			ContentPathToRoot.Remove(Pair);
			MountPointRootPaths.Remove(RootPath);

			// Let subscribers know that a new content path was mounted
			FPackageName::OnContentPathDismounted().Broadcast( RootPath, ContentPath );
		}
	}

private:
	FLongPackagePathsSingleton()
	{
		ConfigRootPath = TEXT("/Config/");
		EngineRootPath = TEXT("/Engine/");
		GameRootPath   = TEXT("/Game/");
		ScriptRootPath = TEXT("/Script/");
		TempRootPath   = TEXT("/Temp/");

		EngineContentPath      = FPaths::EngineContentDir();
		ContentPathShort       = TEXT("../../Content/");
		EngineShadersPath      = FPaths::EngineDir() / TEXT("Shaders/");
		EngineShadersPathShort = TEXT("../../Shaders/");
		GameContentPath        = FPaths::GameContentDir();
		GameConfigPath         = FPaths::GameConfigDir();
		GameScriptPath         = FPaths::GameDir() / TEXT("Script/");
		GameSavedPath          = FPaths::GameSavedDir();

		FString RebasedGameDir = FString::Printf(TEXT("../../../%s/"), FApp::GetGameName());

		GameContentPathRebased = RebasedGameDir / TEXT("Content/");
		GameConfigPathRebased  = RebasedGameDir / TEXT("Config/");
		GameScriptPathRebased  = RebasedGameDir / TEXT("Script/");
		GameSavedPathRebased   = RebasedGameDir / TEXT("Saved/");
		
		ContentPathToRoot.Empty(11);
		ContentPathToRoot.Emplace(EngineRootPath, EngineContentPath);
#if IS_MONOLITHIC
		ContentPathToRoot.Emplace(GameRootPath,   ContentPathShort);
#else
		ContentPathToRoot.Emplace(EngineRootPath, ContentPathShort);
#endif
		ContentPathToRoot.Emplace(EngineRootPath, EngineShadersPath);
		ContentPathToRoot.Emplace(EngineRootPath, EngineShadersPathShort);
		ContentPathToRoot.Emplace(GameRootPath,   GameContentPath);
		ContentPathToRoot.Emplace(ScriptRootPath, GameScriptPath);
		ContentPathToRoot.Emplace(TempRootPath,   GameSavedPath);
		ContentPathToRoot.Emplace(GameRootPath,   GameContentPathRebased);
		ContentPathToRoot.Emplace(ScriptRootPath, GameScriptPathRebased);
		ContentPathToRoot.Emplace(TempRootPath,   GameSavedPathRebased);
		ContentPathToRoot.Emplace(ConfigRootPath, GameConfigPath);

		ContentRootToPath.Empty(9);
		ContentRootToPath.Emplace(EngineRootPath, EngineContentPath);
		ContentRootToPath.Emplace(EngineRootPath, EngineShadersPath);
		ContentRootToPath.Emplace(GameRootPath,   GameContentPath);
		ContentRootToPath.Emplace(ScriptRootPath, GameScriptPath);
		ContentRootToPath.Emplace(TempRootPath,   GameSavedPath);
		ContentRootToPath.Emplace(GameRootPath,   GameContentPathRebased);
		ContentRootToPath.Emplace(ScriptRootPath, GameScriptPathRebased);
		ContentRootToPath.Emplace(TempRootPath,   GameSavedPathRebased);
		ContentRootToPath.Emplace(ConfigRootPath, GameConfigPathRebased);

		// Allow the plugin manager to mount new content paths by exposing access through a delegate.  PluginManager is 
		// a Core class, but content path functionality is added at the CoreUObject level.
		IPluginManager::Get().SetRegisterMountPointDelegate( IPluginManager::FRegisterMountPointDelegate::CreateStatic( &FPackageName::RegisterMountPoint ) );
	}
};

FString FPackageName::InternalFilenameToLongPackageName(const FString& InFilename)
{
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	FString Filename = InFilename.Replace(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	// Convert to relative path if it's not already a long package name
	bool bIsValidLongPackageName = false;
	for (const auto& Pair : Paths.ContentRootToPath)
	{
		if (Filename.StartsWith(Pair.RootPath))
		{
			bIsValidLongPackageName = true;
			break;
		}
	}

	if (!bIsValidLongPackageName)
	{
		Filename = IFileManager::Get().ConvertToRelativePath(*Filename);
	}

	FString PackageName         = FPaths::GetBaseFilename(Filename);
	int32   PackageNameStartsAt = Filename.Len() - FPaths::GetCleanFilename(Filename).Len();
	FString Result              = Filename.Mid(0, PackageNameStartsAt + PackageName.Len());
	Result.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	for (const auto& Pair : Paths.ContentPathToRoot)
	{
		if (Result.StartsWith(Pair.ContentPath))
		{
			Result = Pair.RootPath + Result.Mid(Pair.ContentPath.Len());
			break;
		}
	}

	return Result;
}

bool FPackageName::TryConvertFilenameToLongPackageName(const FString& InFilename, FString& OutPackageName, FString* OutFailureReason)
{
	FString LongPackageName = InternalFilenameToLongPackageName(InFilename);

	// we don't support loading packages from outside of well defined places
	const bool bContainsDot = LongPackageName.Contains(TEXT("."), ESearchCase::CaseSensitive);
	const bool bContainsBackslash = LongPackageName.Contains(TEXT("\\"), ESearchCase::CaseSensitive);
	const bool bContainsColon = LongPackageName.Contains(TEXT(":"), ESearchCase::CaseSensitive);
	const bool bResult = !(bContainsDot || bContainsBackslash || bContainsColon);

	if (bResult)
	{
		OutPackageName = MoveTemp(LongPackageName);
	}
	else if (OutFailureReason != nullptr)
	{
		FString InvalidChars;
		if (bContainsDot)
		{
			InvalidChars += TEXT(".");
		}
		if (bContainsBackslash)
		{
			InvalidChars += TEXT("\\");
		}
		if (bContainsColon)
		{
			InvalidChars += TEXT(":");
		}
		*OutFailureReason = FString::Printf(TEXT("FilenameToLongPackageName failed to convert '%s'. Attempt result was '%s', but the path contains illegal characters '%s'"), *InFilename, *LongPackageName, *InvalidChars);
	}
	return bResult;
}

FString FPackageName::FilenameToLongPackageName(const FString& InFilename)
{
	FString FailureReason;
	FString Result;
	if (!TryConvertFilenameToLongPackageName(InFilename, Result, &FailureReason))
	{
		UE_LOG(LogPackageName, Fatal, TEXT("%s"), *FailureReason);
	}
	return Result;
}

bool FPackageName::TryConvertLongPackageNameToFilename(const FString& InLongPackageName, FString& OutFilename, const FString& InExtension)
{
	const auto& Paths = FLongPackagePathsSingleton::Get();
	for (const auto& Pair : Paths.ContentRootToPath)
	{
		if (InLongPackageName.StartsWith(Pair.RootPath))
		{
			OutFilename = Pair.ContentPath + InLongPackageName.Mid(Pair.RootPath.Len()) + InExtension;
			return true;
		}
	}

	// This is not a long package name or the root folder is not handled in the above cases
	return false;
}

FString FPackageName::LongPackageNameToFilename(const FString& InLongPackageName, const FString& InExtension)
{
	FString FailureReason;
	FString Result;
	if (!TryConvertLongPackageNameToFilename(InLongPackageName, Result, InExtension))
	{
		UE_LOG(LogPackageName, Fatal,TEXT("LongPackageNameToFilename failed to convert '%s'. Path does not map to any roots."), *InLongPackageName);
	}
	return Result;
}

FString FPackageName::GetLongPackagePath(const FString& InLongPackageName)
{
	const int32 LastSlashIdx = InLongPackageName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return LastSlashIdx >= 0 ? InLongPackageName.Left(LastSlashIdx) : InLongPackageName;
}

bool FPackageName::SplitLongPackageName(const FString& InLongPackageName, FString& OutPackageRoot, FString& OutPackagePath, FString& OutPackageName, const bool bStripRootLeadingSlash)
{
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();

	const bool bIncludeReadOnlyRoots = true;
	TArray<FString> ValidRoots;
	Paths.GetValidLongPackageRoots(ValidRoots, bIncludeReadOnlyRoots);

	// Check to see whether our package came from a valid root
	OutPackageRoot.Empty();
	for(auto RootIt = ValidRoots.CreateConstIterator(); RootIt; ++RootIt)
	{
		const FString& PackageRoot = *RootIt;
		if(InLongPackageName.StartsWith(PackageRoot))
		{
			OutPackageRoot = PackageRoot / "";
			break;
		}
	}

	if(OutPackageRoot.IsEmpty() || InLongPackageName.Len() <= OutPackageRoot.Len())
	{
		// Path is not part of a valid root, or the path given is too short to continue; splitting failed
		return false;
	}

	// Use the standard path functions to get the rest
	const FString RemainingPackageName = InLongPackageName.Mid(OutPackageRoot.Len());
	OutPackagePath = FPaths::GetPath(RemainingPackageName) / "";
	OutPackageName = FPaths::GetCleanFilename(RemainingPackageName);

	if(bStripRootLeadingSlash && OutPackageRoot.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		OutPackageRoot.RemoveAt(0);
	}

	return true;
}

FString FPackageName::GetLongPackageAssetName(const FString& InLongPackageName)
{
	const int32 LastSlashIdx = InLongPackageName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return LastSlashIdx >= 0 ? InLongPackageName.Mid(LastSlashIdx + 1) : InLongPackageName;
}

bool FPackageName::DoesPackageNameContainInvalidCharacters(const FString& InLongPackageName, FText* OutReason /*= NULL*/)
{
	// See if the name contains invalid characters.
	TCHAR CharString[] = { '\0', '\0' };
	FString MatchedInvalidChars;
	for (const TCHAR* InvalidCharacters = INVALID_LONGPACKAGE_CHARACTERS; *InvalidCharacters; ++InvalidCharacters)
	{
		CharString[0] = *InvalidCharacters;
		if (InLongPackageName.Contains(CharString, ESearchCase::CaseSensitive))
		{
			MatchedInvalidChars += *InvalidCharacters;
		}
	}
	if (MatchedInvalidChars.Len())
	{
		if (OutReason)
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("IllegalNameCharacters"), FText::FromString(MatchedInvalidChars) );
			*OutReason = FText::Format( NSLOCTEXT("Core", "PackageNameContainsInvalidCharacters", "Name may not contain the following characters: {IllegalNameCharacters}"), Args );
		}
		return true;
	}
	return false;
}

bool FPackageName::IsValidLongPackageName(const FString& InLongPackageName, bool bIncludeReadOnlyRoots /*= false*/, FText* OutReason /*= NULL*/)
{
	// All package names must contain a leading slash, root, slash and name, at minimum theoretical length ("/A/B") is 4
	if (InLongPackageName.Len() < PackageNameConstants::MinPackageNameLength)
	{
		if (OutReason)
		{
			*OutReason = FText::Format(NSLOCTEXT("Core", "LongPackageNames_PathTooShort", "Path should be no less than {0} characters long."), FText::AsNumber(PackageNameConstants::MinPackageNameLength));
		}
		return false;
	}
	// Package names start with a leading slash.
	if (InLongPackageName[0] != '/')
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("Core", "LongPackageNames_PathWithNoStartingSlash", "Path should start with a '/'");
		}
		return false;
	}
	// Package names do not end with a trailing slash.
	if (InLongPackageName[InLongPackageName.Len() - 1] == '/')
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("Core", "LongPackageNames_PathWithTrailingSlash", "Path may not end with a '/'");
		}
		return false;
	}
	// Check for invalid characters
	if (DoesPackageNameContainInvalidCharacters(InLongPackageName, OutReason))
	{
		return false;
	}
	// Check valid roots
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	TArray<FString> ValidRoots;
	bool bValidRoot = false;
	Paths.GetValidLongPackageRoots(ValidRoots, bIncludeReadOnlyRoots);
	for (int32 RootIdx = 0; RootIdx < ValidRoots.Num(); ++RootIdx)
	{
		const FString& Root = ValidRoots[RootIdx];
		if (InLongPackageName.StartsWith(Root))
		{
			bValidRoot = true;
			break;
		}
	}
	if (!bValidRoot && OutReason)
	{
		if (ValidRoots.Num() == 0)
		{
			*OutReason = NSLOCTEXT("Core", "LongPackageNames_NoValidRoots", "No valid roots exist!");
		}
		else
		{
			FString ValidRootsString = TEXT("");
			if (ValidRoots.Num() == 1)
			{
				ValidRootsString = FString::Printf(TEXT("'%s'"), *ValidRoots[0]);
			}
			else
			{
				for (int32 RootIdx = 0; RootIdx < ValidRoots.Num(); ++RootIdx)
				{
					if (RootIdx < ValidRoots.Num() - 1)
					{
						ValidRootsString += FString::Printf(TEXT("'%s', "), *ValidRoots[RootIdx]);
					}
					else
					{
						ValidRootsString += FString::Printf(TEXT("or '%s'"), *ValidRoots[RootIdx]);
					}
				}
			}
			*OutReason = FText::Format( NSLOCTEXT("Core", "LongPackageNames_InvalidRoot", "Path does not start with a valid root. Path must begin with: {0}"), FText::FromString( ValidRootsString ) );
		}
	}
	return bValidRoot;
}

void FPackageName::RegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	Paths.InsertMountPoint(RootPath, ContentPath);
}

void FPackageName::UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	Paths.RemoveMountPoint(RootPath, ContentPath);
}

FName FPackageName::GetPackageMountPoint(const FString& InPackagePath)
{
	FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	
	TArray<FString> MountPoints;
	Paths.GetValidLongPackageRoots(MountPoints, true);

	for (auto RootIt = MountPoints.CreateConstIterator(); RootIt; ++RootIt)
	{
		if (InPackagePath.StartsWith(*RootIt))
		{
			return FName(*RootIt->Mid(1, RootIt->Len() - 2));
		}
	}

	return FName();
}

FString FPackageName::ConvertToLongScriptPackageName(const TCHAR* InShortName)
{
	if (IsShortPackageName(FString(InShortName)))
	{
		return FString::Printf(TEXT("/Script/%s"), InShortName);
	}
	else
	{
		return InShortName;
	}
}

// Short to long script package name map.
static TMap<FName, FName> ScriptPackageNames;


// @todo: This stuff needs to be eliminated as soon as we can make sure that no legacy short package names
//        are in use when referencing class names in UObject module "class packages"
void FPackageName::RegisterShortPackageNamesForUObjectModules()
{
	// @todo: Ideally we'd only be processing UObject modules, not every module, but we have
	//        no way of knowing which modules may contain UObjects (without say, having UBT save a manifest.)
	// @todo: This stuff is a bomb waiting to explode.  Because short package names can
	//        take precedent over other object names, modules can reserve names for other types!
	TArray<FName> AllModuleNames;
	FModuleManager::Get().FindModules( TEXT( "*" ), AllModuleNames );
	for( TArray<FName>::TConstIterator ModuleNameIt( AllModuleNames ); ModuleNameIt; ++ModuleNameIt )
	{
		ScriptPackageNames.Add( *ModuleNameIt, *ConvertToLongScriptPackageName( *ModuleNameIt->ToString() ));
	}
}

FName* FPackageName::FindScriptPackageName(FName InShortName)
{
	return ScriptPackageNames.Find(InShortName);
}

bool FPackageName::FindPackageFileWithoutExtension(const FString& InPackageFilename, FString& OutFilename)
{
	auto& FileManager = IFileManager::Get();

	static const FString* PackageExtensions[] =
	{
		&AssetPackageExtension,
		&MapPackageExtension
	};

	// Loop through all known extensions and check if the file exist.
	for (auto Extension : PackageExtensions)
	{
		FString   PackageFilename = InPackageFilename + *Extension;
		FDateTime Timestamp       = FileManager.GetTimeStamp(*PackageFilename);
		if (Timestamp != FDateTime::MinValue())
		{
			// The package exists so exit. From now on InPackageFilename can be equal to OutFilename so
			// don't attempt to use it anymore (case where &InPackageFilename == &OutFilename).
			OutFilename = MoveTemp(PackageFilename);
			return true;
		}
	}

	return false;
}

bool FPackageName::DoesPackageExist(const FString& LongPackageName, const FGuid* Guid /*= NULL*/, FString* OutFilename /*= NULL*/)
{
	bool bFoundFile = false;

	// Make sure passing filename as LongPackageName is supported.
	FString PackageName;
	FText Reason;

	if (!FPackageName::TryConvertFilenameToLongPackageName(LongPackageName, PackageName))
	{
		verify(!FPackageName::IsValidLongPackageName(PackageName, true, &Reason));
		UE_LOG(LogPackageName, Error, TEXT("Illegal call to DoesPackageExist: '%s' is not a standard unreal filename or a long path name. Reason: %s"), *LongPackageName, *Reason.ToString());
		ensureMsgf(false, TEXT("Illegal call to DoesPackageExist: '%s' is not a standard unreal filename or a long path name. Reason: %s"), *LongPackageName, *Reason.ToString());
		return false;
	}
	// Once we have the real Package Name, we can exit early if it's a script package - they exist only in memory.
	if (IsScriptPackage(PackageName))
	{
		return false;
	}

	if ( !FPackageName::IsValidLongPackageName( PackageName, true, &Reason ) )
	{
		UE_LOG(LogPackageName, Error, TEXT( "DoesPackageExist: DoesPackageExist FAILED: '%s' is not a standard unreal filename or a long path name. Reason: %s"), *LongPackageName, *Reason.ToString() );
		return false;
	}

	// Convert to filename (no extension yet).
	FString Filename = LongPackageNameToFilename(PackageName, TEXT(""));
	// Find the filename (with extension).
	bFoundFile = FindPackageFileWithoutExtension(Filename, Filename);

	// On consoles, we don't support package downloading, so no need to waste any extra cycles/disk io dealing with it
	if (!FPlatformProperties::RequiresCookedData() && bFoundFile && Guid != NULL)
	{
		// @todo: If we could get to list of linkers here, it would be faster to check
		// then to open the file and read it
		FArchive* PackageReader = IFileManager::Get().CreateFileReader(*Filename);
		// This had better open
		check(PackageReader != NULL);

		// Read in the package summary
		FPackageFileSummary Summary;
		*PackageReader << Summary;

		// Compare Guids
		if (Summary.Guid != *Guid)
		{
			bFoundFile = false;
		}

		// Close package file
		delete PackageReader;
	}

	if (bFoundFile && OutFilename)
	{
		*OutFilename = Filename;
	}
	return bFoundFile;
}

bool FPackageName::SearchForPackageOnDisk(const FString& PackageName, FString* OutLongPackageName, FString* OutFilename)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FPackageName::SearchForPackageOnDisk"), STAT_PackageName_SearchForPackageOnDisk, STATGROUP_LoadTime);
	
	// This function may take a long time to complete, so suspend heartbeat measure while we're here
	FSlowHeartBeatScope SlowHeartBeatScope;

	bool bResult = false;
	double StartTime = FPlatformTime::Seconds();
	if (FPackageName::IsShortPackageName(PackageName) == false)
	{
		// If this is long package name, revert to using DoesPackageExist because it's a lot faster.
		FString Filename;
		if (DoesPackageExist(PackageName, NULL, &Filename))
		{
			if (OutLongPackageName)
			{
				*OutLongPackageName = PackageName;
			}
			if (OutFilename)
			{
				*OutFilename = Filename;
			}
			bResult = true;
		}
	}
	else
	{
		// Attempt to find package by its short name by searching in the known content paths.
		TArray<FString> Paths;		
		{
			TArray<FString> RootContentPaths;
			FPackageName::QueryRootContentPaths( RootContentPaths );
			for( TArray<FString>::TConstIterator RootPathIt( RootContentPaths ); RootPathIt; ++RootPathIt )
			{
				const FString& RootPath = *RootPathIt;
				const FString& ContentFolder = FPackageName::LongPackageNameToFilename(RootPath, TEXT(""));
				Paths.Add( ContentFolder );
			}
		}

		const FString PackageWildcard = (PackageName.Find(TEXT("."), ESearchCase::CaseSensitive) != INDEX_NONE ? PackageName : PackageName + TEXT(".*"));
		TArray<FString> Results;

		for (int32 PathIndex = 0; PathIndex < Paths.Num() && !bResult; ++PathIndex)
		{
			// Search directly on disk. Very slow!
			IFileManager::Get().FindFilesRecursive(Results, *Paths[PathIndex], *PackageWildcard, true, false);

			for (int32 FileIndex = 0; FileIndex < Results.Num(); ++FileIndex)
			{			
				FString Filename(Results[FileIndex]);
				if (IsPackageFilename(Results[FileIndex]))
				{
					// Convert to long package name.
					FString LongPackageName;
					if (TryConvertFilenameToLongPackageName(Filename, LongPackageName))
					{
						if (OutLongPackageName)
						{
							if (bResult)
							{
								UE_LOG(LogPackageName, Warning, TEXT("Found ambiguous long package name for '%s'. Returning '%s', but could also be '%s'."), *PackageName, **OutLongPackageName, *LongPackageName );
							}
							else
							{
								*OutLongPackageName = LongPackageName;
							}
						}
						if (OutFilename)
						{
							FPaths::MakeStandardFilename(Filename);
							if (bResult)
							{
								UE_LOG(LogPackageName, Warning, TEXT("Found ambiguous file name for '%s'. Returning '%s', but could also be '%s'."), *PackageName, **OutFilename, *Filename);
							}
							else
							{
								*OutFilename = Filename;
							}
						}
						bResult = true;
					}
				}
			}
		}
	}
	float ThisTime = FPlatformTime::Seconds() - StartTime;

	if ( bResult )
	{
		UE_LOG(LogPackageName, Log, TEXT("SearchForPackageOnDisk took %7.3fs to resolve %s."), ThisTime, *PackageName);
	}
	else
	{
		UE_LOG(LogPackageName, Log, TEXT("SearchForPackageOnDisk took %7.3fs, but failed to resolve %s."), ThisTime, *PackageName);
	}

	return bResult;
}

bool FPackageName::TryConvertShortPackagePathToLongInObjectPath(const FString& ObjectPath, FString& ConvertedObjectPath)
{
	FString PackagePath;
	FString ObjectName;

	int32 DotPosition = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive);
	if (DotPosition != INDEX_NONE)
	{
		PackagePath = ObjectPath.Mid(0, DotPosition);
		ObjectName = ObjectPath.Mid(DotPosition + 1);
	}
	else
	{
		PackagePath = ObjectPath;
	}

	FString LongPackagePath;
	if (!SearchForPackageOnDisk(PackagePath, &LongPackagePath))
	{
		return false;
	}

	ConvertedObjectPath = FString::Printf(TEXT("%s.%s"), *LongPackagePath, *ObjectName);
	return true;
}

FString FPackageName::GetNormalizedObjectPath(const FString& ObjectPath)
{
	if (!ObjectPath.IsEmpty() && FPackageName::IsShortPackageName(ObjectPath))
	{
		FString LongPath;

		UE_LOG(LogPackageName, Warning, TEXT("String asset reference \"%s\" is in short form, which is unsupported and -- even if valid -- resolving it will be really slow. Please consider resaving package in order to speed-up loading."), *ObjectPath);

		if (!FPackageName::TryConvertShortPackagePathToLongInObjectPath(ObjectPath, LongPath))
		{
			UE_LOG(LogPackageName, Warning, TEXT("String asset reference \"%s\" could not be resolved."), *ObjectPath);
		}

		return LongPath;
	}
	else
	{
		return ObjectPath;
	}
}

FString FPackageName::GetDelegateResolvedPackagePath(const FString& InSourcePackagePath)
{
	bool WasResolved = false;

	// If the path is /Game/Path/Foo.Foo only worry about resolving the /Game/Path/Foo
	FString PathName = InSourcePackagePath;
	FString ObjectName;
	PathName.Split(TEXT("."), &PathName, &ObjectName);

	for (auto Delegate : FCoreDelegates::PackageNameResolvers)
	{
		FString ResolvedPath;
		if (Delegate.Execute(PathName, ResolvedPath))
		{
			UE_LOG(LogPackageName, Display, TEXT("Package '%s' was resolved to '%s'"), *PathName, *ResolvedPath);
			PathName = ResolvedPath;
			WasResolved = true;
		}
	}

	if (WasResolved)
	{
		// If package was passed in with an object, add that back on by deriving it from the package name
		if (ObjectName.Len())
		{
			PathName.Split(TEXT("/"), nullptr, &ObjectName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

			PathName += TEXT(".");
			PathName += ObjectName;
		}

		return PathName;
	}

	return InSourcePackagePath;
}

FString FPackageName::GetLocalizedPackagePath(const FString& InSourcePackagePath, const FString& InCultureName)
{
	const FName LocalizedPackageName = FPackageLocalizationManager::Get().FindLocalizedPackageNameForCulture(*InSourcePackagePath, InCultureName);
	return (LocalizedPackageName.IsNone()) ? InSourcePackagePath : LocalizedPackageName.ToString();
}

FString FPackageName::PackageFromPath(const TCHAR* InPathName)
{
	FString PackageName;
	if (FPackageName::TryConvertFilenameToLongPackageName(InPathName, PackageName))
	{
		return PackageName;
	}
	else
	{
		// Not a valid package filename
		return InPathName;
	}
}

bool FPackageName::IsPackageExtension( const TCHAR* Ext )
{
	if (*Ext != TEXT('.'))
	{
		return AssetPackageExtension.EndsWith(Ext) || MapPackageExtension.EndsWith(Ext);
	}
	else
	{
		return AssetPackageExtension == Ext || MapPackageExtension == Ext;
	}
}

bool FPackageName::FindPackagesInDirectory( TArray<FString>& OutPackages, const FString& RootDir )
{
	// Find all files in RootDir, then filter by extension (we have two package extensions so they can't
	// be included in the search wildcard.
	TArray<FString> AllFiles;
	IFileManager::Get().FindFilesRecursive(AllFiles, *RootDir, TEXT("*.*"), true, false);
	// Keep track if any package has been found. Can't rely only on OutPackages.Num() > 0 as it may not be empty.
	const int32 PreviousPackagesCount = OutPackages.Num();
	for (int32 FileIndex = 0; FileIndex < AllFiles.Num(); FileIndex++)
	{
		FString& Filename = AllFiles[FileIndex];
		if (IsPackageFilename(Filename))
		{
			OutPackages.Add(Filename);
		}
	}
	return OutPackages.Num() > PreviousPackagesCount;
}

void FPackageName::IteratePackagesInDirectory(const FString& RootDir, const FPackageNameVisitor& Callback)
{
	class FPackageVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		const FPackageNameVisitor& Callback;
		explicit FPackageVisitor(const FPackageNameVisitor& InCallback)
			: Callback(InCallback)
		{
		}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			bool Result = true;
			if (!bIsDirectory && IsPackageFilename(FilenameOrDirectory))
			{
				Result = Callback(FilenameOrDirectory);
			}
			return Result;
		}
	};

	FPackageVisitor PackageVisitor(Callback);
	IFileManager::Get().IterateDirectoryRecursively(*RootDir, PackageVisitor);
}

void FPackageName::IteratePackagesInDirectory(const FString& RootDir, const FPackageNameStatVisitor& Callback)
{
	class FPackageStatVisitor : public IPlatformFile::FDirectoryStatVisitor
	{
	public:
		const FPackageNameStatVisitor& Callback;
		explicit FPackageStatVisitor(const FPackageNameStatVisitor& InCallback)
			: Callback(InCallback)
		{
		}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, const FFileStatData& StatData) override
		{
			bool Result = true;
			if (!StatData.bIsDirectory && IsPackageFilename(FilenameOrDirectory))
			{
				Result = Callback(FilenameOrDirectory, StatData);
			}
			return Result;
		}
	};

	FPackageStatVisitor PackageVisitor(Callback);
	IFileManager::Get().IterateDirectoryStatRecursively(*RootDir, PackageVisitor);
}

void FPackageName::QueryRootContentPaths( TArray<FString>& OutRootContentPaths )
{
	const FLongPackagePathsSingleton& Paths = FLongPackagePathsSingleton::Get();
	const bool bIncludeReadOnlyRoots = false;	// Don't include Script or Temp paths
	Paths.GetValidLongPackageRoots( OutRootContentPaths, bIncludeReadOnlyRoots );
}

void FPackageName::EnsureContentPathsAreRegistered()
{
	FLongPackagePathsSingleton::Get();
}

bool FPackageName::ParseExportTextPath(const FString& InExportTextPath, FString* OutClassName, FString* OutObjectPath)
{
	if (InExportTextPath.Split(TEXT("'"), OutClassName, OutObjectPath, ESearchCase::CaseSensitive))
	{
		if ( OutObjectPath )
		{
			FString& OutObjectPathRef = *OutObjectPath;
			if ( OutObjectPathRef.EndsWith(TEXT("'"), ESearchCase::CaseSensitive) )
			{
				OutObjectPathRef = OutObjectPathRef.LeftChop(1);
			}
		}

		return true;
	}
	
	return false;
}

FString FPackageName::ExportTextPathToObjectPath(const FString& InExportTextPath)
{
	FString ObjectPath;
	if ( FPackageName::ParseExportTextPath(InExportTextPath, NULL, &ObjectPath) )
	{
		return ObjectPath;
	}
	
	// Could not parse the export text path. Could already be an object path, just return it back.
	return InExportTextPath;
}

FString FPackageName::ObjectPathToPackageName(const FString& InObjectPath)
{
	// Check for package delimiter
	int32 ObjectDelimiterIdx;
	if (InObjectPath.FindChar('.', ObjectDelimiterIdx))
	{
		return InObjectPath.Mid(0, ObjectDelimiterIdx);
	}

	// No object delimiter. The path must refer to the package name directly.
	return InObjectPath;
}

FString FPackageName::ObjectPathToObjectName(const FString& InObjectPath)
{
	// Check for a subobject
	int32 SubObjectDelimiterIdx;
	if ( InObjectPath.FindChar(':', SubObjectDelimiterIdx) )
	{
		return InObjectPath.Mid(SubObjectDelimiterIdx + 1);
	}

	// Check for a top level object
	int32 ObjectDelimiterIdx;
	if ( InObjectPath.FindChar('.', ObjectDelimiterIdx) )
	{
		return InObjectPath.Mid(ObjectDelimiterIdx + 1);
	}

	// No object or subobject delimiters. The path must refer to the object name directly (i.e. a package).
	return InObjectPath;
}

bool FPackageName::IsScriptPackage(const FString& InPackageName)
{
	return InPackageName.StartsWith(FLongPackagePathsSingleton::Get().ScriptRootPath);
}

bool FPackageName::IsLocalizedPackage(const FString& InPackageName)
{
	// Minimum valid package name length is "/A/L10N"
	if (InPackageName.Len() < 7)
	{
		return false;
	}

	const TCHAR* CurChar = *InPackageName;

	// Must start with a slash
	if (*CurChar++ != TEXT('/'))
	{
		return false;
	}

	// Find the end of the first part of the path, eg /Game/
	while (*CurChar && *CurChar++ != TEXT('/')) {}
	if (!*CurChar)
	{
		// Found end-of-string
		return false;
	}

	// Are we part of the L10N folder?
	return FCString::Strnicmp(CurChar, TEXT("L10N/"), 5) == 0	// StartsWith "L10N/"
		|| FCString::Stricmp(CurChar, TEXT("L10N")) == 0;		// Is "L10N"
}
