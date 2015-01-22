// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef PLACEHOLDER_DLC_WORK
#	define PLACEHOLDER_DLC_WORK 0
#endif

#include "HTTPChunkInstallerPrivatePCH.h"
#include "HTTPChunkInstaller.h"
#include "ChunkInstall.h"
#include "UniquePtr.h"
#include "LocalTitleFile.h"

DEFINE_LOG_CATEGORY(LogHTTPChunkInstaller);

// helper to grab the installer service
static IBuildPatchServicesModule* GetBuildPatchServices()
{
	static IBuildPatchServicesModule* BuildPatchServices = nullptr;
	if (BuildPatchServices == nullptr)
	{
		BuildPatchServices = &FModuleManager::LoadModuleChecked<IBuildPatchServicesModule>(TEXT("BuildPatchServices"));
	}
	return BuildPatchServices;
}

// Helper class to find all pak file manifests.
class FChunkSearchVisitor: public IPlatformFile::FDirectoryVisitor
{
public:
	TArray<FString>				PakManifests;

	FChunkSearchVisitor()
	{}

	virtual bool Visit(const TCHAR* FilenameOrDirectory,bool bIsDirectory)
	{
		if(bIsDirectory == false)
		{
			FString Filename(FilenameOrDirectory);
			if(FPaths::GetBaseFilename(Filename).MatchesWildcard("*.manifest"))
			{
				PakManifests.AddUnique(Filename);
			}
		}
		return true;
	}
};


FHTTPChunkInstall::FHTTPChunkInstall()
	: InstallingChunkID(-1)
	, InstallerState(ChunkInstallState::Setup)
	, InstallSpeed(EChunkInstallSpeed::Fast)
	, bDebugNoInstalledRequired(false)
{
	BPSModule = GetBuildPatchServices();

#if !UE_BUILD_SHIPPING
	const TCHAR* CmdLine = FCommandLine::Get();
	bool Result = FParse::Param(CmdLine,TEXT("Pak")) || FParse::Param(CmdLine,TEXT("Signedpak")) || FParse::Param(CmdLine,TEXT("Signed")) || FParse::Param(CmdLine, TEXT("TestChunkInstall"));
	if(!FPlatformProperties::RequiresCookedData() || !Result || FParse::Param(CmdLine,TEXT("NoPak")) || FParse::Param(CmdLine, TEXT("NoChunkInstall")))
	{		
		bDebugNoInstalledRequired = true;
	}
#endif

	// Grab the title file interface
	FString TitleFileSource;
	bool bValidTitleFileSource = GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("TitleFileSource"), TitleFileSource, GEngineIni);
	if (bValidTitleFileSource && TitleFileSource != TEXT("Local"))
	{
		OnlineTitleFile = Online::GetTitleFileInterface(*TitleFileSource);
	}
	else
	{
		FString LocalTileFileDirectory = FPaths::GameConfigDir();
		GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("LocalTitleFileDirectory"), LocalTileFileDirectory, GEngineIni);
		OnlineTitleFile = MakeShareable(new FLocalTitleFile(LocalTileFileDirectory));
	}
	CloudDir	= FPaths::Combine(*FPaths::GameContentDir()	, TEXT("Cloud"));
	StageDir	= FPaths::Combine(*FPaths::GameSavedDir()	, TEXT("Chunks"), TEXT("Staged"));
	InstallDir	= FPaths::Combine(*FPaths::GameSavedDir()	, TEXT("Chunks"), TEXT("Installed"));
	BackupDir	= FPaths::Combine(*FPaths::GameSavedDir()	, TEXT("Chunks"), TEXT("Backup"));
	CacheDir	= FPaths::Combine(*FPaths::GameSavedDir()	, TEXT("Chunks"), TEXT("Cache"));
	ContentDir  = FPaths::Combine(*FPaths::GameContentDir() , TEXT("Chunks"));

	FString TmpString1;
	FString TmpString2;
	if (GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("CloudDirectory"), TmpString1, GEngineIni))
	{
		CloudDir = TmpString1;
	}
	if ((GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("CloudProtocol"), TmpString1, GEngineIni)) && (GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("CloudDomain"), TmpString2, GEngineIni)))
	{
		CloudDir = FString::Printf(TEXT("%s://%s"), *TmpString1, *TmpString2);
	}
	if (GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("StageDirectory"), TmpString1, GEngineIni))
	{
		StageDir = TmpString1;
	}
	if (GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("InstallDirectory"), TmpString1, GEngineIni))
	{
		InstallDir = TmpString1;
	}
	if (GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("BackupDirectory"), TmpString1, GEngineIni))
	{
		BackupDir = TmpString1;
	}
	if (GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("ContentDirectory"), TmpString1, GEngineIni))
	{
		ContentDir = TmpString1;
	}

	bFirstRun = true;
}


FHTTPChunkInstall::~FHTTPChunkInstall()
{
	if (InstallService.IsValid())
	{
		InstallService->CancelInstall();
		InstallService.Reset();
	}
}

bool FHTTPChunkInstall::Tick(float DeltaSeconds)
{
	switch (InstallerState)
	{
	case ChunkInstallState::Setup:
		{
			check(OnlineTitleFile.IsValid());
			OnlineTitleFile->AddOnEnumerateFilesCompleteDelegate(FOnEnumerateFilesCompleteDelegate::CreateRaw(this,&FHTTPChunkInstall::OSSEnumerateFilesComplete));
			OnlineTitleFile->AddOnReadFileCompleteDelegate(FOnReadFileCompleteDelegate::CreateRaw(this,&FHTTPChunkInstall::OSSReadFileComplete));
			ChunkSetupTask.GetTask().Init(BPSModule, InstallDir, ContentDir, MountedPaks);
			ChunkSetupTask.StartBackgroundTask();
			InstallerState = ChunkInstallState::SetupWait;
		} break;
	case ChunkInstallState::SetupWait:
		{
			if (ChunkSetupTask.IsDone())
			{
				for (auto It = ChunkSetupTask.GetTask().InstalledChunks.CreateConstIterator(); It; ++It)
				{
					UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding Chunk %d to installed manifests"), It.Key());
					InstalledManifests.Add(It.Key(), It.Value());
				}
				MountedPaks.Append(ChunkSetupTask.GetTask().MountedPaks);
				InstallerState = ChunkInstallState::QueryRemoteManifests;
			}
		} break;
	case ChunkInstallState::QueryRemoteManifests:
		{			
			//Now query the title file service for the chunk manifests. This should return the list of expected chunk manifests
			check(OnlineTitleFile.IsValid());
			if (InstallSpeed == EChunkInstallSpeed::Paused)
			{
				UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Skipping enumerating manifest files because system is paused"));
				InstallerState = ChunkInstallState::PostSetup;
				break;
			}
			OnlineTitleFile->ClearFiles();
			InstallerState = ChunkInstallState::RequestingTitleFiles;
			UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Enumerating manifest files"));
			OnlineTitleFile->EnumerateFiles();
		} break;
	case ChunkInstallState::SearchTitleFiles:
		{
			FString CleanName;
			TArray<FCloudFileHeader> FileList;
			TitleFilesToRead.Reset();
			RemoteManifests.Reset();
			OnlineTitleFile->GetFileList(FileList);
			for (int32 FileIndex = 0, FileCount = FileList.Num(); FileIndex < FileCount; ++FileIndex)
			{
				if (FileList[FileIndex].FileName.MatchesWildcard(TEXT("*.manifest")))
				{
					UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Found manifest %s"), *FileList[FileIndex].FileName);
					TitleFilesToRead.Add(FileList[FileIndex]);
				}
			}
			InstallerState = ChunkInstallState::ReadTitleFiles;
		} break;
	case ChunkInstallState::ReadTitleFiles:
		{
			if (TitleFilesToRead.Num() > 0 && InstallSpeed != EChunkInstallSpeed::Paused)
			{
				if (!IsDataInFileCache(TitleFilesToRead[0].Hash))
				{
					UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Reading manifest %s from remote source"), *TitleFilesToRead[0].FileName);
					OnlineTitleFile->ReadFile(TitleFilesToRead[0].DLName);
					InstallerState = ChunkInstallState::WaitingOnRead;
				}
				else
				{
					InstallerState = ChunkInstallState::ReadComplete;
				}
			}
			else
			{
				InstallerState = ChunkInstallState::PostSetup;
			}
		} break;
	case ChunkInstallState::ReadComplete:
		{
			FileContentBuffer.Reset();
			bool bReadOK = false;
			bool bAlreadyLoaded = ManifestsInMemory.Contains(TitleFilesToRead[0].Hash);
			if (!IsDataInFileCache(TitleFilesToRead[0].Hash))
			{
				bReadOK = OnlineTitleFile->GetFileContents(TitleFilesToRead[0].DLName, FileContentBuffer);
				if (bReadOK)
				{
					AddDataToFileCache(TitleFilesToRead[0].Hash, FileContentBuffer);
				}
			}
			else if (!bAlreadyLoaded)
			{
				bReadOK = GetDataFromFileCache(TitleFilesToRead[0].Hash, FileContentBuffer);
				if (!bReadOK)
				{
					RemoveDataFromFileCache(TitleFilesToRead[0].Hash);
				}
			}
			if (bReadOK)
			{
				if (!bAlreadyLoaded)
				{
					ParseTitleFileManifest(TitleFilesToRead[0].Hash);
				}
				// Even if the Parse failed remove the file from the list
				TitleFilesToRead.RemoveAt(0);
			}
			if (TitleFilesToRead.Num() == 0)
			{
				if (bFirstRun)
				{
					ChunkMountTask.GetTask().Init(BPSModule, ContentDir, MountedPaks);
					ChunkMountTask.StartBackgroundTask();
				}
				InstallerState = ChunkInstallState::PostSetup;
			}
			else
			{
				InstallerState = ChunkInstallState::ReadTitleFiles;
			}
		} break;
	case ChunkInstallState::PostSetup:
		{
			if (bFirstRun)
			{
				if (ChunkMountTask.IsDone())
				{
					MountedPaks.Append(ChunkMountTask.GetTask().MountedPaks);
					UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Completed First Run"));
					bFirstRun = false;
				}
			}
			else
			{
				InstallerState = ChunkInstallState::Idle;
			}
		} break;
	case ChunkInstallState::Idle: 
		{
			UpdatePendingInstallQueue();
		} break;
	case ChunkInstallState::CopyToContent:
		{
			if (!ChunkCopyInstall.IsDone() || !InstallService->IsComplete())
			{
				break;
			}
			check(InstallingChunkID != -1);
			if (InstallService.IsValid())
			{
				InstallService.Reset();
			}
			check(RemoteManifests.Find(InstallingChunkID));
			UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding Chunk %d to installed manifests"), InstallingChunkID);
			InstalledManifests.Add(InstallingChunkID, InstallingChunkManifest);
			UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Removing Chunk %d from remote manifests"), InstallingChunkID);
			RemoteManifests.Remove(InstallingChunkID, InstallingChunkManifest);
			MountedPaks.Append(ChunkCopyInstall.GetTask().MountedPaks);
			if (!RemoteManifests.Contains(InstallingChunkID))
			{
				// No more manifests relating to the chunk ID are left to install.
				// Inform any listeners that the install has been completed.
				FPlatformChunkInstallCompleteMultiDelegate* FoundDelegate = DelegateMap.Find(InstallingChunkID);
				if (FoundDelegate)
				{
					FoundDelegate->Broadcast(InstallingChunkID);
				}
			}
			InstallingChunkID = -1;
			InstallingChunkManifest.Reset();
			InstallerState = ChunkInstallState::Idle;
		} break;
	case ChunkInstallState::Installing:
	case ChunkInstallState::RequestingTitleFiles:
	case ChunkInstallState::WaitingOnRead:
	default:
		break;
	}

	return true;
}

void FHTTPChunkInstall::UpdatePendingInstallQueue()
{
	if (InstallingChunkID != -1
#if !UE_BUILD_SHIPPING
		|| bDebugNoInstalledRequired
#endif
	)
	{
		return;
	}

	check(!InstallService.IsValid());
	bool bPatch = false;
	while (PriorityQueue.Num() > 0 && InstallerState != ChunkInstallState::Installing)
	{
		const FChunkPrio& NextChunk = PriorityQueue[0];
		TArray<IBuildManifestPtr> FoundChunkManifests;
		RemoteManifests.MultiFind(NextChunk.ChunkID, FoundChunkManifests);
		if (FoundChunkManifests.Num() > 0)
		{
			auto ChunkManifest = FoundChunkManifests[0];
			auto ChunkIDField = ChunkManifest->GetCustomField("ChunkID");
			if (ChunkIDField.IsValid())
			{
				BeginChunkInstall(NextChunk.ChunkID, ChunkManifest);
			}
			else
			{
				PriorityQueue.RemoveAt(0);
			}
		}
		else
		{
			PriorityQueue.RemoveAt(0);
		}
	}
	if (InstallingChunkID == -1)
	{
		// Install the first available chunk
		for (auto It = RemoteManifests.CreateConstIterator(); It; ++It)
		{
			if (It)
			{
				IBuildManifestPtr ChunkManifest = It.Value();
				auto ChunkIDField = ChunkManifest->GetCustomField("ChunkID");
				if (ChunkIDField.IsValid())
				{
					BeginChunkInstall(ChunkIDField->AsInteger(), ChunkManifest);
					return;
				}
			}
		}
	}
}


EChunkLocation::Type FHTTPChunkInstall::GetChunkLocation(uint32 ChunkID)
{
#if !UE_BUILD_SHIPPING
	if(bDebugNoInstalledRequired)
	{
		return EChunkLocation::BestLocation;
	}
#endif

	if (bFirstRun)
	{
		/** Still waiting on setup to finish, report that nothing is installed yet... */
		return EChunkLocation::NotAvailable;
	}
	TArray<IBuildManifestPtr> FoundManifests;
	RemoteManifests.MultiFind(ChunkID, FoundManifests);
	if (FoundManifests.Num() > 0)
	{
		return EChunkLocation::NotAvailable;
	}

	InstalledManifests.MultiFind(ChunkID, FoundManifests);
	if (FoundManifests.Num() > 0)
	{
		return EChunkLocation::BestLocation;
	}

	return EChunkLocation::DoesNotExist;
}


float FHTTPChunkInstall::GetChunkProgress(uint32 ChunkID,EChunkProgressReportingType::Type ReportType)
{
#if !UE_BUILD_SHIPPING
	if (bDebugNoInstalledRequired)
	{
		return 100.f;
	}
#endif

	if (bFirstRun)
	{
		/** Still waiting on setup to finish, report that nothing is installed yet... */
		return 0.f;
	}
	TArray<IBuildManifestPtr> FoundManifests;
	RemoteManifests.MultiFind(ChunkID, FoundManifests);
	if (FoundManifests.Num() > 0)
	{
		float Progress = 0;
		if (InstallingChunkID == ChunkID && InstallService.IsValid())
		{
			Progress = InstallService->GetUpdateProgress();
		}
		return Progress / FoundManifests.Num();
	}

	InstalledManifests.MultiFind(ChunkID, FoundManifests);
	if (FoundManifests.Num() > 0)
	{
		return 100.f;
	}

	return 0.f;
}

void FHTTPChunkInstall::OSSEnumerateFilesComplete(bool bSuccess)
{
	InstallerState = bSuccess ? ChunkInstallState::SearchTitleFiles : ChunkInstallState::QueryRemoteManifests;
}

void FHTTPChunkInstall::OSSReadFileComplete(bool bSuccess, const FString& Filename)
{
	InstallerState = bSuccess ? ChunkInstallState::ReadComplete : ChunkInstallState::ReadTitleFiles;
}

void FHTTPChunkInstall::OSSInstallComplete(bool bSuccess, IBuildManifestRef BuildManifest)
{
	if (bSuccess)
	{
		// Completed OK. Write the manifest. If the chunk doesn't exist, copy to the content dir.
		// Otherwise, writing the manifest will prompt a copy on next start of the game
		FString ManifestName;
		FString ChunkFdrName;
		uint32 ChunkID;
		bool bIsPatch;
		if (!BuildChunkFolderName(BuildManifest, ChunkFdrName, ManifestName, ChunkID, bIsPatch))
		{
			//Something bad has happened, bail
			InstallerState = ChunkInstallState::Idle;
			return;
		}
		UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Chunk %d install complete, preparing to copy to content directory"), ChunkID);
		FString ManifestPath = FPaths::Combine(*InstallDir, *ChunkFdrName, *ManifestName);
		FString SrcDir = FPaths::Combine(*InstallDir, *ChunkFdrName);
		FString DestDir = FPaths::Combine(*ContentDir, *ChunkFdrName);
		bool bCopyDir = true; 
		TArray<IBuildManifestPtr> FoundManifests;
		InstalledManifests.MultiFind(ChunkID, FoundManifests);
		for (const auto& It : FoundManifests)
		{
			auto FoundPatchField = It->GetCustomField("bIsPatch");
			bool bFoundPatch = FoundPatchField.IsValid() ? FoundPatchField->AsString() == TEXT("true") : false;
			if (bFoundPatch == bIsPatch)
			{
				bCopyDir = false;
			}
		}
		ChunkCopyInstall.GetTask().Init(ManifestPath, SrcDir, DestDir, BPSModule, BuildManifest, MountedPaks, bCopyDir);
		UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Copying Chunk %d to content directory"), ChunkID);
		ChunkCopyInstall.StartBackgroundTask();
	}
	InstallerState = ChunkInstallState::CopyToContent;
}

void FHTTPChunkInstall::ParseTitleFileManifest(const FString& ManifestFileHash)
{
#if !UE_BUILD_SHIPPING
	if (bDebugNoInstalledRequired)
	{
		// Forces the installer to think that no remote manifests exist, so nothing needs to be installed.
		return;
	}
#endif
	FString JsonBuffer;
	FFileHelper::BufferToString(JsonBuffer, FileContentBuffer.GetData(), FileContentBuffer.Num());
	auto RemoteManifest = BPSModule->MakeManifestFromJSON(JsonBuffer);
	if (!RemoteManifest.IsValid())
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("Manifest was invalid"));
		return;
	}
	auto RemoteChunkIDField = RemoteManifest->GetCustomField("ChunkID");
	if (!RemoteChunkIDField.IsValid())
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("Manifest ChunkID was invalid or missing"));
		return;
	}
	//Compare to installed manifests and add to the remote if it needs to be installed.
	uint32 ChunkID = (uint32)RemoteChunkIDField->AsInteger();
	TArray<IBuildManifestPtr> FoundManifests;
	InstalledManifests.MultiFind(ChunkID, FoundManifests);
	uint32 FoundCount = FoundManifests.Num();
	if (FoundCount > 0)
	{
		auto RemotePatchManifest = RemoteManifest->GetCustomField("bIsPatch");
		auto RemoteVersion = RemoteManifest->GetVersionString();
		bool bRemoteIsPatch = RemotePatchManifest.IsValid() ? RemotePatchManifest->AsString() == TEXT("true") : false;
		for (uint32 FoundIndex = 0; FoundIndex < FoundCount; ++FoundIndex)
		{
			const auto& InstalledManifest = FoundManifests[FoundIndex];
			auto InstalledVersion = InstalledManifest->GetVersionString();
			auto InstallPatchManifest = InstalledManifest->GetCustomField("bIsPatch");
			bool bInstallIsPatch = InstallPatchManifest.IsValid() ? InstallPatchManifest->AsString() == TEXT("true") : false;
			if (InstalledVersion != RemoteVersion && bInstallIsPatch == bRemoteIsPatch)
			{
				UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding Chunk %d to remote manifests"), ChunkID);
				RemoteManifests.Add(ChunkID, RemoteManifest);
				if(!ManifestFileHash.IsEmpty())
				{
					ManifestsInMemory.Add(ManifestFileHash);
				}
				//Remove from the installed map
				if (bFirstRun)
				{
					// Prevent the paks from being mounted by removing the manifest file
					FString ChunkFdrName;
					FString ManifestName;
					uint32 ChunkID;
					bool bIsPatch;
					if (BuildChunkFolderName(InstalledManifest.ToSharedRef(), ChunkFdrName, ManifestName, ChunkID, bIsPatch))
					{
						FString ManifestPath = FPaths::Combine(*ContentDir, *ChunkFdrName, *ManifestName);
						IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
						PlatformFile.DeleteFile(*ManifestPath);
					}
					UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Removing Chunk %d from installed manifests"), ChunkID);
					InstalledManifests.Remove(ChunkID, InstalledManifest);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding Chunk %d to remote manifests"), ChunkID);
		RemoteManifests.Add(ChunkID, RemoteManifest);
		if (!ManifestFileHash.IsEmpty())
		{
			ManifestsInMemory.Add(ManifestFileHash);
		}
	}
}

bool FHTTPChunkInstall::BuildChunkFolderName(IBuildManifestRef Manifest, FString& ChunkFdrName, FString& ManifestName, uint32& ChunkID, bool& bIsPatch)
{
	auto ChunkIDField = Manifest->GetCustomField("ChunkID");
	auto ChunkPatchField = Manifest->GetCustomField("bIsPatch");

	if (!ChunkIDField.IsValid())
	{
		return false;
	}
	ChunkID = ChunkIDField->AsInteger();
	bIsPatch = ChunkPatchField.IsValid() ? ChunkPatchField->AsString() == TEXT("true") : false;
	ManifestName = FString::Printf(TEXT("chunk_%u"), ChunkID);
	if (bIsPatch)
	{
		ManifestName += TEXT("_patch");
	}
	ManifestName += TEXT(".manifest");
	ChunkFdrName = FString::Printf(TEXT("%s%d"), !bIsPatch ? TEXT("base") : TEXT("patch"), ChunkID);
	return true;
}

bool FHTTPChunkInstall::PrioritizeChunk(uint32 ChunkID, EChunkPriority::Type Priority)
{	
	int32 FoundIndex;
	PriorityQueue.Find(FChunkPrio(ChunkID, Priority), FoundIndex);
	if (FoundIndex != INDEX_NONE)
	{
		PriorityQueue.RemoveAt(FoundIndex);
	}
	// Low priority is assumed if the chunk ID doesn't exist in the queue
	if (Priority != EChunkPriority::Low)
	{
		PriorityQueue.AddUnique(FChunkPrio(ChunkID, Priority));
		PriorityQueue.Sort();
	}
	return true;
}

bool FHTTPChunkInstall::SetChunkInstallDelgate(uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate)
{
	FPlatformChunkInstallCompleteMultiDelegate* FoundDelegate = DelegateMap.Find(ChunkID);
	if (FoundDelegate)
	{
		FoundDelegate->Add(Delegate);
	}
	else
	{
		FPlatformChunkInstallCompleteMultiDelegate MC;
		MC.Add(Delegate);
		DelegateMap.Add(ChunkID, MC);
	}
	return false;
}

void FHTTPChunkInstall::RemoveChunkInstallDelgate(uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate)
{
	FPlatformChunkInstallCompleteMultiDelegate* FoundDelegate = DelegateMap.Find(ChunkID);
	if (!FoundDelegate)
	{
		return;
	}
	FoundDelegate->Remove(Delegate);
}

void FHTTPChunkInstall::BeginChunkInstall(uint32 ChunkID,IBuildManifestPtr ChunkManifest)
{
	check(ChunkManifest->GetCustomField("ChunkID").IsValid());
	InstallingChunkID = ChunkID;
	InstallingChunkManifest = ChunkManifest;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	auto PatchField = ChunkManifest->GetCustomField("bIsPatch");
	bool bIsPatch = PatchField.IsValid() ? PatchField->AsString() == TEXT("true") : false;
	auto ChunkFolderName = FString::Printf(TEXT("%s%d"),!bIsPatch ? TEXT("base") : TEXT("patch"), InstallingChunkID);
	auto ChunkInstallDir = FPaths::Combine(*InstallDir,*ChunkFolderName);
	auto ChunkStageDir = FPaths::Combine(*StageDir,*ChunkFolderName);
	if(!PlatformFile.DirectoryExists(*ChunkStageDir))
	{
		PlatformFile.CreateDirectoryTree(*ChunkStageDir);
	}
	if(!PlatformFile.DirectoryExists(*ChunkInstallDir))
	{
		PlatformFile.CreateDirectoryTree(*ChunkInstallDir);
	}
	BPSModule->SetCloudDirectory(CloudDir);
	BPSModule->SetStagingDirectory(ChunkStageDir);
	UE_LOG(LogHTTPChunkInstaller,Log,TEXT("Starting Chunk %d install"),InstallingChunkID);
	InstallService = BPSModule->StartBuildInstall(nullptr,ChunkManifest,ChunkInstallDir,FBuildPatchBoolManifestDelegate::CreateRaw(this,&FHTTPChunkInstall::OSSInstallComplete));
	if(InstallSpeed == EChunkInstallSpeed::Paused && !InstallService->IsPaused())
	{
		InstallService->TogglePauseInstall();
	}
	InstallerState = ChunkInstallState::Installing;
}

/**
 * Note: the following cache functions are synchronous and may need to become asynchronous...
 */
bool FHTTPChunkInstall::AddDataToFileCache(const FString& ManifestHash,const TArray<uint8>& Data)
{
	if (ManifestHash.IsEmpty())
	{
		return false;
	}
	UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding data hash %s to file cache"), *ManifestHash);
	return FFileHelper::SaveArrayToFile(Data, *FPaths::Combine(*CacheDir, *ManifestHash));
}

bool FHTTPChunkInstall::IsDataInFileCache(const FString& ManifestHash)
{
	if(ManifestHash.IsEmpty())
	{
		return false;
	}
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return PlatformFile.FileExists(*FPaths::Combine(*CacheDir, *ManifestHash));
}

bool FHTTPChunkInstall::GetDataFromFileCache(const FString& ManifestHash, TArray<uint8>& Data)
{
	if(ManifestHash.IsEmpty())
	{
		return false;
	}
	UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Reading data hash %s from file cache"), *ManifestHash);
	return FFileHelper::LoadFileToArray(Data, *FPaths::Combine(*CacheDir, *ManifestHash));
}

bool FHTTPChunkInstall::RemoveDataFromFileCache(const FString& ManifestHash)
{
	if(ManifestHash.IsEmpty())
	{
		return false;
	}
	UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Removing data hash %s from file cache"), *ManifestHash);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	auto ManifestPath = FPaths::Combine(*CacheDir, *ManifestHash);
	if (PlatformFile.FileExists(*ManifestPath))
	{
		return PlatformFile.DeleteFile(*ManifestPath);
	}
	return false;
}

/**
 * Module for the HTTP Chunk Installer
 */
class FHTTPChunkInstallerModule : public IPlatformChunkInstallModule
{
public:
	TUniquePtr<IPlatformChunkInstall> ChunkInstaller;

	FHTTPChunkInstallerModule()
		: ChunkInstaller(new FHTTPChunkInstall())
	{

	}

	virtual IPlatformChunkInstall* GetPlatformChunkInstall()
	{
		return ChunkInstaller.Get();
	}
};

IMPLEMENT_MODULE(FHTTPChunkInstallerModule, HTTPChunkInstaller);