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
	, bFirstRun(true)
	, bSystemInitialised(false)
{

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
	if (!bSystemInitialised)
	{
		InitialiseSystem();
	}

	switch (InstallerState)
	{
	case ChunkInstallState::Setup:
		{
			check(OnlineTitleFile.IsValid());
			EnumFilesCompleteHandle = OnlineTitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(FOnEnumerateFilesCompleteDelegate::CreateRaw(this,&FHTTPChunkInstall::OSSEnumerateFilesComplete));
			ReadFileCompleteHandle = OnlineTitleFile->AddOnReadFileCompleteDelegate_Handle(FOnReadFileCompleteDelegate::CreateRaw(this,&FHTTPChunkInstall::OSSReadFileComplete));
			ChunkSetupTask.SetupWork(BPSModule, InstallDir, ContentDir, HoldingDir, MountedPaks);
			ChunkSetupTaskThread.Reset(FRunnableThread::Create(&ChunkSetupTask, TEXT("Chunk descovery thread")));
			InstallerState = ChunkInstallState::SetupWait;
		} break;
	case ChunkInstallState::SetupWait:
		{
			if (ChunkSetupTask.IsDone())
			{
				ChunkSetupTaskThread->WaitForCompletion();
				ChunkSetupTaskThread.Reset();
				for (auto It = ChunkSetupTask.InstalledChunks.CreateConstIterator(); It; ++It)
				{
					UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding Chunk %d to installed manifests"), It.Key());
					InstalledManifests.Add(It.Key(), It.Value());
				}
				for (auto It = ChunkSetupTask.HoldingChunks.CreateConstIterator(); It; ++It)
				{
					UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding Chunk %d to holding manifests"), It.Key());
					PrevInstallManifests.Add(It.Key(), It.Value());
				}
				MountedPaks.Append(ChunkSetupTask.MountedPaks);
				InstallerState = ChunkInstallState::QueryRemoteManifests;
			}
		} break;
	case ChunkInstallState::QueryRemoteManifests:
		{			
			//Now query the title file service for the chunk manifests. This should return the list of expected chunk manifests
			check(OnlineTitleFile.IsValid());
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
			ExpectedChunks.Empty();
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
					InstallerState = ChunkInstallState::WaitingOnRead;
					OnlineTitleFile->ReadFile(TitleFilesToRead[0].DLName);
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
					ChunkMountTask.SetupWork(BPSModule, ContentDir, MountedPaks, ExpectedChunks);
					ChunkMountTaskThread.Reset(FRunnableThread::Create(&ChunkMountTask, TEXT("Chunk mounting thread")));
				}
				InstallerState = ChunkInstallState::PostSetup;
			}
			else
			{
				InstallerState = ChunkInstallState::ReadTitleFiles;
			}
		} break;
	case ChunkInstallState::EnterOfflineMode:
		{
			for (auto It = InstalledManifests.CreateConstIterator(); It; ++It)
			{
				ExpectedChunks.Add(It.Key());
			}
			ChunkMountTask.SetupWork(BPSModule, ContentDir, MountedPaks, ExpectedChunks);
			ChunkMountTaskThread.Reset(FRunnableThread::Create(&ChunkMountTask, TEXT("Chunk mounting thread")));
			InstallerState = ChunkInstallState::PostSetup;
		} break;
	case ChunkInstallState::PostSetup:
		{
			if (bFirstRun)
			{
				if (ChunkMountTask.IsDone())
				{
					ChunkMountTaskThread->WaitForCompletion();
					ChunkMountTaskThread.Reset();
					MountedPaks.Append(ChunkMountTask.MountedPaks);
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
			ChunkCopyInstallThread.Reset();
			check(RemoteManifests.Find(InstallingChunkID));
			UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding Chunk %d to installed manifests"), InstallingChunkID);
			InstalledManifests.Add(InstallingChunkID, InstallingChunkManifest);
			UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Removing Chunk %d from remote manifests"), InstallingChunkID);
			RemoteManifests.Remove(InstallingChunkID, InstallingChunkManifest);
			MountedPaks.Append(ChunkCopyInstall.MountedPaks);
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
			EndInstall();

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
				BeginChunkInstall(NextChunk.ChunkID, ChunkManifest, FindPreviousInstallManifest(ChunkManifest));
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
					BeginChunkInstall(ChunkIDField->AsInteger(), ChunkManifest, FindPreviousInstallManifest(ChunkManifest));
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

	// Safe to assume Chunk0 is ready
	if (ChunkID == 0)
	{
		return EChunkLocation::BestLocation;
	}

	if (bFirstRun || !bSystemInitialised)
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

	// Safe to assume Chunk0 is ready
	if (ChunkID == 0)
	{
		return 100.f;
	}

	if (bFirstRun || !bSystemInitialised)
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
	InstallerState = bSuccess ? ChunkInstallState::SearchTitleFiles : ChunkInstallState::EnterOfflineMode;
}

void FHTTPChunkInstall::OSSReadFileComplete(bool bSuccess, const FString& Filename)
{
	InstallerState = bSuccess ? ChunkInstallState::ReadComplete : ChunkInstallState::EnterOfflineMode;
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
			EndInstall();
			return;
		}
		UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Chunk %d install complete, preparing to copy to content directory"), ChunkID);
		FString ManifestPath = FPaths::Combine(*InstallDir, *ChunkFdrName, *ManifestName);
		FString HoldingManifestPath = FPaths::Combine(*HoldingDir, *ChunkFdrName, *ManifestName);
		FString SrcDir = FPaths::Combine(*InstallDir, *ChunkFdrName);
		FString DestDir = FPaths::Combine(*ContentDir, *ChunkFdrName);
		bool bCopyDir = InstallDir != ContentDir; 
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
		ChunkCopyInstall.SetupWork(ManifestPath, HoldingManifestPath, SrcDir, DestDir, BPSModule, BuildManifest, MountedPaks, bCopyDir);
		UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Copying Chunk %d to content directory"), ChunkID);
		ChunkCopyInstallThread.Reset(FRunnableThread::Create(&ChunkCopyInstall, TEXT("Chunk Install Copy Thread")));
		InstallerState = ChunkInstallState::CopyToContent;
	}
	else
	{
		//Something bad has happened, return to the Idle state. We'll re-attempt the install
		EndInstall();
	}
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
	ExpectedChunks.Add(ChunkID);
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
						FString HoldingPath = FPaths::Combine(*HoldingDir, *ChunkFdrName, *ManifestName);
						IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
						PlatformFile.CreateDirectoryTree(*FPaths::Combine(*HoldingDir, *ChunkFdrName));
						PlatformFile.MoveFile(*HoldingPath, *ManifestPath);
					}
					UE_LOG(LogHTTPChunkInstaller, Log, TEXT("Adding Chunk %d to previous installed manifests"), ChunkID);
					PrevInstallManifests.Add(ChunkID, InstalledManifest);
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

FDelegateHandle FHTTPChunkInstall::SetChunkInstallDelgate(uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate)
{
	FPlatformChunkInstallCompleteMultiDelegate* FoundDelegate = DelegateMap.Find(ChunkID);
	if (FoundDelegate)
	{
		return FoundDelegate->Add(Delegate);
	}
	else
	{
		FPlatformChunkInstallCompleteMultiDelegate MC;
		auto RetVal = MC.Add(Delegate);
		DelegateMap.Add(ChunkID, MC);
		return RetVal;
	}
	return FDelegateHandle();
}

void FHTTPChunkInstall::RemoveChunkInstallDelgate(uint32 ChunkID, FDelegateHandle Delegate)
{
	FPlatformChunkInstallCompleteMultiDelegate* FoundDelegate = DelegateMap.Find(ChunkID);
	if (!FoundDelegate)
	{
		return;
	}
	FoundDelegate->Remove(Delegate);
}

void FHTTPChunkInstall::BeginChunkInstall(uint32 ChunkID,IBuildManifestPtr ChunkManifest, IBuildManifestPtr PrevInstallChunkManifest)
{
	check(ChunkManifest->GetCustomField("ChunkID").IsValid());
	InstallingChunkID = ChunkID;
	check(ChunkID > 0 && ChunkID < 100);
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
	InstallService = BPSModule->StartBuildInstall(PrevInstallChunkManifest,ChunkManifest,ChunkInstallDir,FBuildPatchBoolManifestDelegate::CreateRaw(this,&FHTTPChunkInstall::OSSInstallComplete));
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

void FHTTPChunkInstall::InitialiseSystem()
{
	BPSModule = GetBuildPatchServices();

#if !UE_BUILD_SHIPPING
	const TCHAR* CmdLine = FCommandLine::Get();
	if (!FPlatformProperties::RequiresCookedData() || FParse::Param(CmdLine, TEXT("NoPak")) || FParse::Param(CmdLine, TEXT("NoChunkInstall")))
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
		auto bGetConfigDir = GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("LocalTitleFileDirectory"), LocalTileFileDirectory, GEngineIni);
		OnlineTitleFile = MakeShareable(new FLocalTitleFile(LocalTileFileDirectory));
#if !UE_BUILD_SHIPPING
		bDebugNoInstalledRequired = !bGetConfigDir;
#endif
	}
	CloudDir = FPaths::Combine(*FPaths::GameContentDir(), TEXT("Cloud"));
	StageDir = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Chunks"), TEXT("Staged"));
	InstallDir = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Chunks"), TEXT("Installed")); // By default this should match ContentDir
	BackupDir = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Chunks"), TEXT("Backup"));
	CacheDir = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Chunks"), TEXT("Cache"));
	HoldingDir = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Chunks"), TEXT("Hold"));
	ContentDir = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Chunks"), TEXT("Installed")); // By default this should match InstallDir

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
	if (GConfig->GetString(TEXT("HTTPChunkInstall"), TEXT("HoldingDirectory"), TmpString1, GEngineIni))
	{
		HoldingDir = TmpString1;
	}

	bFirstRun = true;
	bSystemInitialised = true;
}

IBuildManifestPtr FHTTPChunkInstall::FindPreviousInstallManifest(const IBuildManifestPtr& ChunkManifest)
{
	auto ChunkIDField = ChunkManifest->GetCustomField("ChunkID");
	if (!ChunkIDField.IsValid())
	{
		return IBuildManifestPtr();
	}
	auto ChunkID = ChunkIDField->AsInteger();
	TArray<IBuildManifestPtr> FoundManifests;
	PrevInstallManifests.MultiFind(ChunkID, FoundManifests);
	return FoundManifests.Num() == 0 ? IBuildManifestPtr() : FoundManifests[0];
}

void FHTTPChunkInstall::EndInstall()
{
	if (InstallService.IsValid())
	{
		//InstallService->CancelInstall();
		InstallService.Reset();
	}
	InstallingChunkID = -1;
	InstallingChunkManifest.Reset();
	InstallerState = ChunkInstallState::Idle;
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