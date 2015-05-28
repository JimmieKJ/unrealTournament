// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchServicesModule.cpp: Implements the FBuildPatchServicesModule class.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

IMPLEMENT_MODULE( FBuildPatchServicesModule, BuildPatchServices );

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
	// We need to initialize the lookup for our hashing functions
	FRollingHashConst::Init();

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
	FTicker::GetCoreTicker().RemoveTicker( TickDelegateHandle );

	FBuildPatchHTTP::OnShutdown();
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

IBuildInstallerPtr FBuildPatchServicesModule::StartBuildInstall( IBuildManifestPtr CurrentManifest, IBuildManifestPtr InstallManifest, const FString& InstallDirectory, FBuildPatchBoolManifestDelegate OnCompleteDelegate )
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
	BuildPatchInstallers.Add( MakeShareable( new FBuildPatchInstaller( OnCompleteDelegate, CurrentManifestInternal, InstallManifestInternal.ToSharedRef(), InstallDirectory, GetStagingDirectory(), InstallationInfo, false ) ) );
	return BuildPatchInstallers.Top();
}

IBuildInstallerPtr FBuildPatchServicesModule::StartBuildInstallStageOnly(IBuildManifestPtr CurrentManifest, IBuildManifestPtr InstallManifest, const FString& InstallDirectory, FBuildPatchBoolManifestDelegate OnCompleteDelegate)
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
	BuildPatchInstallers.Add( MakeShareable( new FBuildPatchInstaller( OnCompleteDelegate, CurrentManifestInternal, InstallManifestInternal.ToSharedRef(), InstallDirectory, GetStagingDirectory(), InstallationInfo, true ) ) );
	return BuildPatchInstallers.Top();
}

bool FBuildPatchServicesModule::Tick( float Delta )
{
	// Using a local bool for this check will improve the assert message that gets displayed
	// This one is unlikely to assert unless the FTicker's core tick is not ticked on the main thread for some reason
	const bool bIsCalledFromMainThread = IsInGameThread();
	check( bIsCalledFromMainThread );

	// Call complete delegate on each finished installer
	for(auto InstallerIt = BuildPatchInstallers.CreateIterator(); InstallerIt; ++InstallerIt)
	{
		if( (*InstallerIt).IsValid() && (*InstallerIt)->IsComplete() )
		{
			(*InstallerIt)->ExecuteCompleteDelegate();
			(*InstallerIt).Reset();
		}
	}

	// Remove completed (invalids) from the list
	for(int32 BuildPatchInstallersIdx = 0; BuildPatchInstallersIdx < BuildPatchInstallers.Num(); ++BuildPatchInstallersIdx )
	{
		const FBuildPatchInstallerPtr* Installer = &BuildPatchInstallers[ BuildPatchInstallersIdx ];
		if( !Installer->IsValid() )
		{
			BuildPatchInstallers.RemoveAt( BuildPatchInstallersIdx-- );
		}
	}

	// More ticks
	return true;
}

#if WITH_BUILDPATCHGENERATION
bool FBuildPatchServicesModule::GenerateChunksManifestFromDirectory( const FBuildPatchSettings& Settings )
{
	return FBuildDataGenerator::GenerateChunksManifestFromDirectory( Settings );
}

bool FBuildPatchServicesModule::GenerateFilesManifestFromDirectory( const FBuildPatchSettings& Settings )
{
	return FBuildDataGenerator::GenerateFilesManifestFromDirectory( Settings );
}

bool FBuildPatchServicesModule::CompactifyCloudDirectory(const TArray<FString>& ManifestsToKeep, const float DataAgeThreshold, const ECompactifyMode::Type Mode)
{
	const bool bPreview = Mode == ECompactifyMode::Preview;
	const bool bNoPatchDelete = Mode == ECompactifyMode::NoPatchDelete;
	return FBuildDataCompactifier::CompactifyCloudDirectory(ManifestsToKeep, DataAgeThreshold, bPreview, bNoPatchDelete);
}

bool FBuildPatchServicesModule::EnumerateManifestData(FString ManifestFilePath, FString OutputFile, const bool bIncludeSizes)
{
	return FBuildDataEnumeration::EnumerateManifestData(MoveTemp(ManifestFilePath), MoveTemp(OutputFile), bIncludeSizes);
}

#endif //WITH_BUILDPATCHGENERATION

void FBuildPatchServicesModule::SetStagingDirectory( const FString& StagingDir )
{
	StagingDirectory = StagingDir;
}

void FBuildPatchServicesModule::SetCloudDirectory( const FString& CloudDir )
{
	CloudDirectory = CloudDir;
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

void FBuildPatchServicesModule::PreExit()
{
	// Set shutdown error so any running threads know to exit.
	FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::ApplicationClosing);

	// Release our ptr to analytics
	FBuildPatchAnalytics::SetAnalyticsProvider(NULL);
	FBuildPatchAnalytics::SetHttpTracker(nullptr);

	// Cleanup installers
	for (auto& BuildPatchInstaller : BuildPatchInstallers)
	{
		// Make sure it is not paused, this function only un-pauses when in error state
		BuildPatchInstaller->TogglePauseInstall();
		// We still have to manually wait for the thread as another system could hold a shared ptr
		// thus we would not be calling the destructor here
		BuildPatchInstaller->WaitForThread();
	}
	BuildPatchInstallers.Empty();
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

const FString& FBuildPatchServicesModule::GetCloudDirectory()
{
	// Default cloud directory
	if( CloudDirectory.IsEmpty() )
	{
		CloudDirectory = FPaths::CloudDir();
	}
	return CloudDirectory;
}

const FString& FBuildPatchServicesModule::GetBackupDirectory()
{
	// Default backup directory stays empty which simply doesn't backup
	return BackupDirectory;
}

/* Static variables
 *****************************************************************************/
FString FBuildPatchServicesModule::CloudDirectory;
FString FBuildPatchServicesModule::StagingDirectory;
FString FBuildPatchServicesModule::BackupDirectory;
