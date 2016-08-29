// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchServicesModule.cpp: Implements the FBuildPatchServicesModule class.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

using namespace BuildPatchConstants;

DEFINE_LOG_CATEGORY(LogBuildPatchServices);
IMPLEMENT_MODULE(FBuildPatchServicesModule, BuildPatchServices);

/* FBuildPatchInstallationInfo implementation
*****************************************************************************/

void FBuildPatchInstallationInfo::RegisterAppInstallation(IBuildManifestRef AppManifest, const FString& AppInstallDirectory)
{
	FBuildPatchAppManifestRef InternalRef = StaticCastSharedRef<FBuildPatchAppManifest>(AppManifest);
	AvailableInstallations.Add(AppInstallDirectory, InternalRef);
}

void FBuildPatchInstallationInfo::EnumerateProducibleChunks(const TArray< FGuid >& ChunksRequired, TArray< FGuid >& ChunksAvailable)
{
	for (auto AvailableInstallationsIt = AvailableInstallations.CreateConstIterator(); AvailableInstallationsIt; ++AvailableInstallationsIt)
	{
		const FBuildPatchAppManifest& AppManifest = *AvailableInstallationsIt.Value().Get();
		const FString& AppInstallDir = AvailableInstallationsIt.Key();
		TArray< FGuid > ChunksFromThisManifest;
		AppManifest.EnumerateProducibleChunks(AppInstallDir, ChunksRequired, ChunksFromThisManifest);
		for (auto ChunksFromThisManifestIt = ChunksFromThisManifest.CreateConstIterator(); ChunksFromThisManifestIt; ++ChunksFromThisManifestIt)
		{
			const FGuid& ChunkGuid = *ChunksFromThisManifestIt;
			if (ChunksAvailable.Contains(ChunkGuid) == false)
			{
				ChunksAvailable.Add(ChunkGuid);
				RecyclableChunks.Add(ChunkGuid, AvailableInstallationsIt.Value());
			}
		}
	}
}

FBuildPatchAppManifestPtr FBuildPatchInstallationInfo::GetManifestContainingChunk(const FGuid& ChunkGuid)
{
	return RecyclableChunks.FindRef(ChunkGuid);
}

const FString FBuildPatchInstallationInfo::GetManifestInstallDir(FBuildPatchAppManifestPtr AppManifest)
{
	for (auto AvailableInstallationsIt = AvailableInstallations.CreateConstIterator(); AvailableInstallationsIt; ++AvailableInstallationsIt)
	{
		if (AppManifest == AvailableInstallationsIt.Value())
		{
			return AvailableInstallationsIt.Key();
		}
	}
	return TEXT("");
}

/* FBuildPatchServicesModule implementation
 *****************************************************************************/

void FBuildPatchServicesModule::StartupModule()
{
	// Debug sanity checks
#if UE_BUILD_DEBUG
	TSet<FString> NoDupes;
	bool bWasDupe = false;
	check(ARRAY_COUNT(InstallErrorPrefixes::ErrorTypeStrings) == (uint64)EBuildPatchInstallError::NumInstallErrors);
	for (int32 Idx = 0; Idx < ARRAY_COUNT(InstallErrorPrefixes::ErrorTypeStrings); ++Idx)
	{
		NoDupes.Add(FString(InstallErrorPrefixes::ErrorTypeStrings[Idx]), &bWasDupe);
		check(bWasDupe == false);
	}
#endif

	// We need to initialize the lookup for our hashing functions
	FRollingHashConst::Init();

	// Set the local machine config filename
	LocalMachineConfigFile = FPaths::Combine(FPlatformProcess::ApplicationSettingsDir(), FApp::GetGameName(), TEXT("BuildPatchServicesLocal.ini"));

	// Fix up any legacy configuration data
	FixupLegacyConfig();

	// Check if the user has opted to force skip prerequisites install
	bool bForceSkipPrereqsCmdline = FParse::Param(FCommandLine::Get(), TEXT("skipbuildpatchprereq"));
	bool bForceSkipPrereqsConfig = false;
	GConfig->GetBool(TEXT("Portal.BuildPatch"), TEXT("skipbuildpatchprereq"), bForceSkipPrereqsConfig, GEngineIni);

	if (bForceSkipPrereqsCmdline)
	{
		GLog->Log(TEXT("BuildPatchServicesModule: Setup to skip prerequisites install via commandline."));
	}

	if (bForceSkipPrereqsConfig)
	{
		GLog->Log( TEXT("BuildPatchServicesModule: Setup to skip prerequisites install via config."));
	}
	
	bForceSkipPrereqs = bForceSkipPrereqsCmdline || bForceSkipPrereqsConfig;

	// Add our ticker
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker( FTickerDelegate::CreateRaw( this, &FBuildPatchServicesModule::Tick ) );

	// Register core PreExit
	FCoreDelegates::OnPreExit.AddRaw(this, &FBuildPatchServicesModule::PreExit);

	// Test the rolling hash algorithm
	check( CheckRollingHashAlgorithm() );
}

void FBuildPatchServicesModule::ShutdownModule()
{
	GWarn->Logf( TEXT( "BuildPatchServicesModule: Shutting Down" ) );

	checkf(BuildPatchInstallers.Num() == 0, TEXT("BuildPatchServicesModule: FATAL ERROR: Core PreExit not called, or installer created during shutdown!"));

	// Remove our ticker
	GLog->Log(ELogVerbosity::VeryVerbose, TEXT( "BuildPatchServicesModule: Removing Ticker" ) );
	FTicker::GetCoreTicker().RemoveTicker( TickDelegateHandle );

	GLog->Log(ELogVerbosity::VeryVerbose, TEXT( "BuildPatchServicesModule: Shutting down BuildPatchHTTP" ) );
	FBuildPatchHTTP::OnShutdown();
	GLog->Log(ELogVerbosity::VeryVerbose, TEXT( "BuildPatchServicesModule: Finished shutting down" ) );
}

IBuildManifestPtr FBuildPatchServicesModule::LoadManifestFromFile( const FString& Filename )
{
	FBuildPatchAppManifestRef Manifest = MakeShareable( new FBuildPatchAppManifest() );
	if( Manifest->LoadFromFile( Filename ) )
	{
		return Manifest;
	}
	else
	{
		return NULL;
	}
}

IBuildManifestPtr FBuildPatchServicesModule::MakeManifestFromData(const TArray<uint8>& ManifestData)
{
	FBuildPatchAppManifestRef Manifest = MakeShareable(new FBuildPatchAppManifest());
	if (Manifest->DeserializeFromData(ManifestData))
	{
		return Manifest;
	}
	return NULL;
}

IBuildManifestPtr FBuildPatchServicesModule::MakeManifestFromJSON( const FString& ManifestJSON )
{
	FBuildPatchAppManifestRef Manifest = MakeShareable( new FBuildPatchAppManifest() );
	if( Manifest->DeserializeFromJSON( ManifestJSON ) )
	{
		return Manifest;
	}
	return NULL;
}

bool FBuildPatchServicesModule::SaveManifestToFile(const FString& Filename, IBuildManifestRef Manifest, bool bUseBinary/* = true*/)
{
	return StaticCastSharedRef< FBuildPatchAppManifest >(Manifest)->SaveToFile(Filename, bUseBinary);
}

IBuildInstallerPtr FBuildPatchServicesModule::StartBuildInstall(IBuildManifestPtr CurrentManifest, IBuildManifestPtr InstallManifest, const FString& InstallDirectory, FBuildPatchBoolManifestDelegate OnCompleteDelegate, bool bIsRepair, TSet<FString> InstallTags)
{
	// Using a local bool for this check will improve the assert message that gets displayed
	const bool bIsCalledFromMainThread = IsInGameThread();
	check( bIsCalledFromMainThread );
	// Cast manifest parameters
	FBuildPatchAppManifestPtr CurrentManifestInternal = StaticCastSharedPtr< FBuildPatchAppManifest >( CurrentManifest );
	FBuildPatchAppManifestPtr InstallManifestInternal = StaticCastSharedPtr< FBuildPatchAppManifest >( InstallManifest );
	if( !InstallManifestInternal.IsValid() )
	{
		// We must have an install manifest to continue
		return NULL;
	}
	// Make directory
	IFileManager::Get().MakeDirectory( *InstallDirectory, true );
	if( !IFileManager::Get().DirectoryExists( *InstallDirectory ) )
	{
		return NULL;
	}
	// Make sure the http wrapper is already created
	FBuildPatchHTTP::Initialize();
	// Run the install thread
	FBuildPatchInstallerRef Installer = MakeShareable(new FBuildPatchInstaller(OnCompleteDelegate, CurrentManifestInternal, InstallManifestInternal.ToSharedRef(), InstallDirectory, GetStagingDirectory(), InstallationInfo, false, LocalMachineConfigFile, bIsRepair, bForceSkipPrereqs));
	Installer->SetRequiredInstallTags(InstallTags);
	Installer->StartInstallation();
	BuildPatchInstallers.Add(Installer);
	return Installer;
}

IBuildInstallerPtr FBuildPatchServicesModule::StartBuildInstallStageOnly(IBuildManifestPtr CurrentManifest, IBuildManifestPtr InstallManifest, const FString& InstallDirectory, FBuildPatchBoolManifestDelegate OnCompleteDelegate, bool bIsRepair, TSet<FString> InstallTags)
{
	// Using a local bool for this check will improve the assert message that gets displayed
	const bool bIsCalledFromMainThread = IsInGameThread();
	check( bIsCalledFromMainThread );
	// Cast manifest parameters
	FBuildPatchAppManifestPtr CurrentManifestInternal = StaticCastSharedPtr< FBuildPatchAppManifest >( CurrentManifest );
	FBuildPatchAppManifestPtr InstallManifestInternal = StaticCastSharedPtr< FBuildPatchAppManifest >( InstallManifest );
	if( !InstallManifestInternal.IsValid() )
	{
		// We must have an install manifest to continue
		return NULL;
	}
	// Make sure the http wrapper is already created
	FBuildPatchHTTP::Initialize();
	// Run the install thread
	FBuildPatchInstallerRef Installer = MakeShareable(new FBuildPatchInstaller(OnCompleteDelegate, CurrentManifestInternal, InstallManifestInternal.ToSharedRef(), InstallDirectory, GetStagingDirectory(), InstallationInfo, true, LocalMachineConfigFile, bIsRepair, bForceSkipPrereqs));
	Installer->SetRequiredInstallTags(InstallTags);
	Installer->StartInstallation();
	BuildPatchInstallers.Add(Installer);
	return Installer;
}

bool FBuildPatchServicesModule::Tick( float Delta )
{
	// Using a local bool for this check will improve the assert message that gets displayed
	// This one is unlikely to assert unless the FTicker's core tick is not ticked on the main thread for some reason
	const bool bIsCalledFromMainThread = IsInGameThread();
	check( bIsCalledFromMainThread );

	// Call complete delegate on each finished installer
	for (auto& Installer : BuildPatchInstallers)
	{
		if (Installer.IsValid() && Installer->IsComplete())
		{
			Installer->ExecuteCompleteDelegate();
			Installer.Reset();
		}
	}

	// Remove completed (invalids) from the list
	BuildPatchInstallers.RemoveAll([](const FBuildPatchInstallerPtr& Installer){ return Installer.IsValid() == false; });

	// More ticks
	return true;
}

bool FBuildPatchServicesModule::GenerateChunksManifestFromDirectory( const FBuildPatchSettings& Settings )
{
	return FBuildDataGenerator::GenerateChunksManifestFromDirectory( Settings );
}

bool FBuildPatchServicesModule::CompactifyCloudDirectory(float DataAgeThreshold, ECompactifyMode::Type Mode)
{
	const bool bPreview = Mode == ECompactifyMode::Preview;
	return FBuildDataCompactifier::CompactifyCloudDirectory(DataAgeThreshold, bPreview);
}

bool FBuildPatchServicesModule::EnumerateManifestData(const FString& ManifestFilePath, const FString& OutputFile, bool bIncludeSizes)
{
	return FBuildDataEnumeration::EnumerateManifestData(ManifestFilePath, OutputFile, bIncludeSizes);
}

bool FBuildPatchServicesModule::MergeManifests(const FString& ManifestFilePathA, const FString& ManifestFilePathB, const FString& ManifestFilePathC, const FString& NewVersionString, const FString& SelectionDetailFilePath)
{
	return FBuildMergeManifests::MergeManifests(ManifestFilePathA, ManifestFilePathB, ManifestFilePathC, NewVersionString, SelectionDetailFilePath);
}

void FBuildPatchServicesModule::SetStagingDirectory( const FString& StagingDir )
{
	StagingDirectory = StagingDir;
}

void FBuildPatchServicesModule::SetCloudDirectory(FString CloudDir)
{
	TArray<FString> CloudDirs;
	CloudDirs.Add(MoveTemp(CloudDir));
	SetCloudDirectories(MoveTemp(CloudDirs));
}

void FBuildPatchServicesModule::SetCloudDirectories(TArray<FString> CloudDirs)
{
	check(IsInGameThread());
	CloudDirectories = MoveTemp(CloudDirs);
	NormalizeCloudPaths(CloudDirectories);
}

void FBuildPatchServicesModule::NormalizeCloudPaths(TArray<FString>& InOutCloudPaths)
{
	for (FString& CloudPath : InOutCloudPaths)
	{
		// Ensure that we remove any double-slash characters apart from:
		//   1. A double slash following the URI schema
		//   2. A double slash at the start of the path, indicating a network share
		CloudPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		bool bIsNetworkPath = CloudPath.StartsWith(TEXT("//"));
		CloudPath.ReplaceInline(TEXT("://"), TEXT(":////"));
		CloudPath.ReplaceInline(TEXT("//"), TEXT("/"));
		if (bIsNetworkPath)
		{
			CloudPath.InsertAt(0, TEXT("/"));
		}
	}
}

void FBuildPatchServicesModule::SetBackupDirectory( const FString& BackupDir )
{
	BackupDirectory = BackupDir;
}

void FBuildPatchServicesModule::SetAnalyticsProvider( TSharedPtr< IAnalyticsProvider > AnalyticsProvider )
{
	FBuildPatchAnalytics::SetAnalyticsProvider( AnalyticsProvider );
}

void FBuildPatchServicesModule::SetHttpTracker( TSharedPtr< FHttpServiceTracker > HttpTracker )
{
	FBuildPatchAnalytics::SetHttpTracker( HttpTracker );
}

void FBuildPatchServicesModule::RegisterAppInstallation(IBuildManifestRef AppManifest, const FString AppInstallDirectory)
{
	InstallationInfo.RegisterAppInstallation(AppManifest, AppInstallDirectory);
}

void FBuildPatchServicesModule::CancelAllInstallers(bool WaitForThreads)
{
	// Using a local bool for this check will improve the assert message that gets displayed
	const bool bIsCalledFromMainThread = IsInGameThread();
	check(bIsCalledFromMainThread);

	// Loop each installer, cancel it, and optionally wait to make completion delegate call
	for (auto& Installer : BuildPatchInstallers)
	{
		if (Installer.IsValid())
		{
			Installer->CancelInstall();
			if (WaitForThreads)
			{
				Installer->WaitForThread();
				Installer->ExecuteCompleteDelegate();
				Installer.Reset();
			}
		}
	}

	// Remove completed (invalids) from the list
	BuildPatchInstallers.RemoveAll([](const FBuildPatchInstallerPtr& Installer){ return Installer.IsValid() == false; });
}

void FBuildPatchServicesModule::PreExit()
{
	// Set shutdown error so any running threads know to exit.
	FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::ApplicationClosing, ApplicationClosedErrorCodes::ApplicationClosed);

	// Cleanup installers
	CancelAllInstallers(true);
	BuildPatchInstallers.Empty();

	// Release our ptr to analytics
	FBuildPatchAnalytics::SetAnalyticsProvider(NULL);
	FBuildPatchAnalytics::SetHttpTracker(nullptr);
}

void FBuildPatchServicesModule::FixupLegacyConfig()
{
	// Check for old prerequisite installation values to bring in from user configuration
	TArray<FString> OldInstalledPrereqs;
	if (GConfig->GetArray(TEXT("Portal.BuildPatch"), TEXT("InstalledPrereqs"), OldInstalledPrereqs, GEngineIni) && OldInstalledPrereqs.Num() > 0)
	{
		bool bShouldSaveOut = false;
		TArray<FString> InstalledPrereqs;
		if (GConfig->GetArray(TEXT("Portal.BuildPatch"), TEXT("InstalledPrereqs"), InstalledPrereqs, LocalMachineConfigFile) && InstalledPrereqs.Num() > 0)
		{
			// Add old values to the new array
			for (const FString& OldInstalledPrereq : OldInstalledPrereqs)
			{
				int32 PrevNum = InstalledPrereqs.Num();
				bool bAlreadyInArray = InstalledPrereqs.AddUnique(OldInstalledPrereq) < PrevNum;
				bShouldSaveOut = bShouldSaveOut || !bAlreadyInArray;
			}
		}
		else
		{
			// Just use the old array
			InstalledPrereqs = MoveTemp(OldInstalledPrereqs);
			bShouldSaveOut = true;
		}
		// If we added extra then save new config
		if (bShouldSaveOut)
		{
			GConfig->SetArray(TEXT("Portal.BuildPatch"), TEXT("InstalledPrereqs"), InstalledPrereqs, LocalMachineConfigFile);
		}
		// Clear out the old config
		GConfig->RemoveKey(TEXT("Portal.BuildPatch"), TEXT("InstalledPrereqs"), GEngineIni);
	}
}

const FString& FBuildPatchServicesModule::GetStagingDirectory()
{
	// Default staging directory
	if( StagingDirectory.IsEmpty() )
	{
		StagingDirectory = FPaths::GameDir() + TEXT( "BuildStaging/" );
	}
	return StagingDirectory;
}

FString FBuildPatchServicesModule::GetCloudDirectory(int32 CloudIdx)
{
	FString RtnValue;
	if (CloudDirectories.Num())
	{
		RtnValue = CloudDirectories[CloudIdx % CloudDirectories.Num()];
	}
	else
	{
		// Default cloud directory
		RtnValue = FPaths::CloudDir();
	}
	return RtnValue;
}

TArray<FString> FBuildPatchServicesModule::GetCloudDirectories()
{
	TArray<FString> RtnValue;
	if (CloudDirectories.Num() > 0)
	{
		RtnValue = CloudDirectories;
	}
	else
	{
		// Singular function controls the default when none provided
		RtnValue.Add(GetCloudDirectory(0));
	}
	return RtnValue;
}

const FString& FBuildPatchServicesModule::GetBackupDirectory()
{
	// Default backup directory stays empty which simply doesn't backup
	return BackupDirectory;
}

/* Static variables
 *****************************************************************************/
TArray<FString> FBuildPatchServicesModule::CloudDirectories;
FString FBuildPatchServicesModule::StagingDirectory;
FString FBuildPatchServicesModule::BackupDirectory;
