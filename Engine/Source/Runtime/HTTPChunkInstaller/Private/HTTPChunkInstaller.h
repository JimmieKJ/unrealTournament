// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatformChunkInstall.h"
#include "UniquePtr.h"
#include "BuildPatchServices.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystem.h"
#include "ChunkInstall.h"
#include "ChunkSetup.h"

/**
* HTTP based implementation of chunk based install
**/
class HTTPCHUNKINSTALLER_API FHTTPChunkInstall : public IPlatformChunkInstall, public FTickerObjectBase
{
public:
	FHTTPChunkInstall();
	~FHTTPChunkInstall();

	virtual EChunkLocation::Type GetChunkLocation( uint32 ChunkID ) override;

	virtual bool GetProgressReportingTypeSupported(EChunkProgressReportingType::Type ReportType) override
	{
		if (ReportType == EChunkProgressReportingType::PercentageComplete)
		{
			return true;
		}

		return false;
	}

	virtual float GetChunkProgress( uint32 ChunkID, EChunkProgressReportingType::Type ReportType ) override;

	virtual EChunkInstallSpeed::Type GetInstallSpeed() override
	{
		return InstallSpeed;
	}

	virtual bool SetInstallSpeed( EChunkInstallSpeed::Type InInstallSpeed ) override
	{
		if (InstallSpeed != InInstallSpeed)
		{
			InstallSpeed = InInstallSpeed;
			if ((InstallSpeed == EChunkInstallSpeed::Paused && !InstallService->IsPaused()) || (InstallSpeed != EChunkInstallSpeed::Paused && InstallService->IsPaused()))
			{
				InstallService->TogglePauseInstall();
			}
		}
		return true;
	}
	
	virtual bool PrioritizeChunk( uint32 ChunkID, EChunkPriority::Type Priority ) override;
	virtual bool SetChunkInstallDelgate(uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate) override;
	virtual void RemoveChunkInstallDelgate(uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate);

	virtual bool DebugStartNextChunk()
	{
		/** Unless paused we are always installing! */
		return false;
	}

	bool Tick(float DeltaSeconds) override;

private:

	bool AddDataToFileCache(const FString& ManifestHash,const TArray<uint8>& Data);
	bool IsDataInFileCache(const FString& ManifestHash);
	bool GetDataFromFileCache(const FString& ManifestHash,TArray<uint8>& Data);
	bool RemoveDataFromFileCache(const FString& ManifestHash);
	void UpdatePendingInstallQueue();
	void BeginChunkInstall(uint32 NextChunkID,IBuildManifestPtr ChunkManifest);
	void ParseTitleFileManifest(const FString& ManifestFileHash);
	bool BuildChunkFolderName(IBuildManifestRef Manifest, FString& ChunkFdrName, FString& ManifestName, uint32& ChunkID, bool& bIsPatch);
	void OSSEnumerateFilesComplete(bool bSuccess);
	void OSSReadFileComplete(bool bSuccess, const FString& Filename);
	void OSSInstallComplete(bool, IBuildManifestRef);

	DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformChunkInstallCompleteMultiDelegate, uint32);

	enum struct ChunkInstallState
	{
		Setup,
		SetupWait,
		QueryRemoteManifests,
		MoveInstalledChunks,
		RequestingTitleFiles,
		SearchTitleFiles,
		ReadTitleFiles,
		WaitingOnRead,
		ReadComplete,
		PostSetup,
		Idle,
		Installing,
		CopyToContent,
	};

	struct FChunkPrio
	{
		FChunkPrio(uint32 InChunkID, EChunkPriority::Type InChunkPrio)
			: ChunkID(InChunkID)
			, ChunkPrio(InChunkPrio)
		{

		}

		uint32					ChunkID;
		EChunkPriority::Type	ChunkPrio;

		bool operator == (const FChunkPrio& RHS) const
		{
			return ChunkID == RHS.ChunkID;
		}

		bool operator < (const FChunkPrio& RHS) const
		{
			return ChunkPrio < RHS.ChunkPrio;
		}
	};

	FAsyncTask<FChunkInstallTask>								ChunkCopyInstall;
	FAsyncTask<FChunkSetupTask>									ChunkSetupTask;
	FAsyncTask<FChunkMountTask>									ChunkMountTask;
	TMultiMap<uint32, IBuildManifestPtr>						InstalledManifests;
	TMultiMap<uint32, IBuildManifestPtr>						RemoteManifests;
	TMap<uint32, FPlatformChunkInstallCompleteMultiDelegate>	DelegateMap;
	TSet<FString>												ManifestsInMemory;
	TArray<FCloudFileHeader>									TitleFilesToRead;
	TArray<uint8>												FileContentBuffer;
	TArray<FChunkPrio>											PriorityQueue;
	TArray<FString>												MountedPaks;
	IOnlineTitleFilePtr											OnlineTitleFile;
	IBuildInstallerPtr											InstallService;
	IBuildManifestPtr											InstallingChunkManifest;
	FString														CloudDir;
	FString														StageDir;
	FString														InstallDir;
	FString														BackupDir;
	FString														ContentDir;
	FString														CacheDir;
	IBuildPatchServicesModule*									BPSModule;
	uint32														InstallingChunkID;
	ChunkInstallState											InstallerState;
	EChunkInstallSpeed::Type									InstallSpeed;
	bool														bFirstRun;
#if !UE_BUILD_SHIPPING
	bool														bDebugNoInstalledRequired;
#endif
};