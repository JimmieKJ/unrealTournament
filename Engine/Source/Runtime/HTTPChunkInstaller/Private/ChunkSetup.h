// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Helper class to find all pak files.
class FFileSearchVisitor : public IPlatformFile::FDirectoryVisitor
{
	FString			 FileWildcard;
	TArray<FString>& FoundFiles;
public:
	FFileSearchVisitor(FString InFileWildcard, TArray<FString>& InFoundFiles)
		: FileWildcard(InFileWildcard)
		, FoundFiles(InFoundFiles)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory == false)
		{
			FString Filename(FilenameOrDirectory);
			if (Filename.MatchesWildcard(FileWildcard))
			{
				FoundFiles.Add(Filename);
			}
		}
		return true;
	}
};

class IBuildPatchServicesModule;

class FChunkSetupTask : public FNonAbandonableTask, public IPlatformFile::FDirectoryVisitor
{
public:
	/** Input parameters */
	IBuildPatchServicesModule*	BPSModule;
	FString						InstallDir; // Intermediate directory where installed chunks may be waiting
	FString						ContentDir; // Directory where installed chunks need to live to be mounted
	const TArray<FString>*		CurrentMountPaks; // Array of already mounted Paks
	/** Output */
	TArray<FString>							MountedPaks;
	TMultiMap<uint32, IBuildManifestPtr>	InstalledChunks;
	/** Working */
	TArray<FString>		FoundPaks;
	TArray<FString>		FoundManifests;
	FFileSearchVisitor	PakVisitor;
	FFileSearchVisitor	ManifestVisitor;
	TArray<FString>		DirectoriesToRemove;
	uint32				Pass;

	FChunkSetupTask()
		: BPSModule(nullptr)
		, CurrentMountPaks(nullptr)
		, PakVisitor(TEXT("*.pak"), FoundPaks)
		, ManifestVisitor(TEXT("*.manifest"), FoundManifests)
	{}

	void Init(IBuildPatchServicesModule* InBPSModule, FString InInstallDir, FString InContentDir, const TArray<FString>& InCurrentMountedPaks)
	{
		BPSModule = InBPSModule;
		InstallDir = InInstallDir;
		ContentDir = InContentDir;
		CurrentMountPaks = &InCurrentMountedPaks;

		Pass = 0;
		InstalledChunks.Reset();
		MountedPaks.Reset();
		FoundManifests.Reset();
		FoundPaks.Reset();
		DirectoriesToRemove.Reset();
	}

	void DoWork()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.IterateDirectory(*InstallDir, *this);
		for (const auto& ToRemove : DirectoriesToRemove)
		{
			PlatformFile.DeleteDirectoryRecursively(*ToRemove);
		}
		++Pass;
		PlatformFile.IterateDirectory(*ContentDir, *this);
	}

	bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory)
		{
			return true;
		}
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		FoundManifests.Reset();
		PlatformFile.IterateDirectory(FilenameOrDirectory, ManifestVisitor);
		if (FoundManifests.Num() == 0)
		{
			return true;
		}
		// Only allow one manifest per folder, any more suggests corruption so mark the folder for delete
		if (FoundManifests.Num() > 1)
		{
			DirectoriesToRemove.Add(FilenameOrDirectory);
			return true;
		}
		// Load the manifest, so that can be classed as installed
		auto Manifest = BPSModule->LoadManifestFromFile(FoundManifests[0]);
		if (!Manifest.IsValid())
		{
			//Something is wrong, suggests corruption so mark the folder for delete
			DirectoriesToRemove.Add(FilenameOrDirectory);
			return true;
		}
		auto ChunkIDField = Manifest->GetCustomField("ChunkID");
		if (!ChunkIDField.IsValid())
		{
			//Something is wrong, suggests corruption so mark the folder for delete
			DirectoriesToRemove.Add(FilenameOrDirectory);
			return true;
		}
		auto ChunkPatchField = Manifest->GetCustomField("bIsPatch");
		bool bIsPatch = ChunkPatchField.IsValid() ? ChunkPatchField->AsString() == TEXT("true") : false;
		uint32 ChunkID = (uint32)ChunkIDField->AsInteger();

		if (Pass == 1)
		{
			InstalledChunks.AddUnique(ChunkID, Manifest);
		}
		else
		{
			FString ChunkFdrName = FString::Printf(TEXT("%s%d"), !bIsPatch ? TEXT("base") : TEXT("patch"), ChunkID);
			FString DestDir = *FPaths::Combine(*ContentDir, *ChunkFdrName);
			if (PlatformFile.DirectoryExists(*DestDir))
			{
				PlatformFile.DeleteDirectoryRecursively(*DestDir);
			}
			PlatformFile.CreateDirectoryTree(*DestDir);
			if (PlatformFile.CopyDirectoryTree(*DestDir, FilenameOrDirectory, true))
			{
				DirectoriesToRemove.Add(FilenameOrDirectory);
			}
		}

		return true;
	}

	static const TCHAR *Name()
	{
		return TEXT("FChunkSetup");
	}
};

class FChunkMountTask : public FNonAbandonableTask, public IPlatformFile::FDirectoryVisitor
{
public:
	/** Input parameters */
	IBuildPatchServicesModule*	BPSModule;
	FString						ContentDir; // Directory where installed chunks need to live to be mounted
	const TArray<FString>*		CurrentMountPaks; // Array of already mounted Paks
	/** Output */
	TArray<FString>							MountedPaks;
	/** Working */
	TArray<FString>		FoundPaks;
	TArray<FString>		FoundManifests;
	FFileSearchVisitor	PakVisitor;
	FFileSearchVisitor	ManifestVisitor;

	FChunkMountTask()
		: BPSModule(nullptr)
		, CurrentMountPaks(nullptr)
		, PakVisitor(TEXT("*.pak"), FoundPaks)
		, ManifestVisitor(TEXT("*.manifest"), FoundManifests)
	{}

	void Init(IBuildPatchServicesModule* InBPSModule, FString InContentDir, const TArray<FString>& InCurrentMountedPaks)
	{
		BPSModule = InBPSModule;
		ContentDir = InContentDir;
		CurrentMountPaks = &InCurrentMountedPaks;

		MountedPaks.Reset();
		FoundManifests.Reset();
		FoundPaks.Reset();
	}

	void DoWork()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.IterateDirectory(*ContentDir, *this);
	}

	bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory)
		{
			return true;
		}
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		FoundManifests.Reset();
		PlatformFile.IterateDirectory(FilenameOrDirectory, ManifestVisitor);
		if (FoundManifests.Num() == 0)
		{
			return true;
		}
		// Only allow one manifest per folder, any more suggests corruption so ignore
		if (FoundManifests.Num() > 1)
		{
			return true;
		}
		// Load the manifest, so that can be classed as installed
		auto Manifest = BPSModule->LoadManifestFromFile(FoundManifests[0]);
		if (!Manifest.IsValid())
		{
			//Something is wrong, suggests corruption so ignore
			return true;
		}
		auto ChunkIDField = Manifest->GetCustomField("ChunkID");
		if (!ChunkIDField.IsValid())
		{
			//Something is wrong, suggests corruption so ignore
			return true;
		}

		FoundPaks.Reset();
		PlatformFile.IterateDirectoryRecursively(FilenameOrDirectory, PakVisitor);
		if (FoundPaks.Num() == 0)
		{
			return true;
		}
		auto PakReadOrderField = Manifest->GetCustomField("PakReadOrdering");
		uint32 PakReadOrder = PakReadOrderField.IsValid() ? (uint32)PakReadOrderField->AsInteger() : 0;
		for (const auto& PakPath : FoundPaks)
		{
			//Note: A side effect here is that any unmounted paks get mounted. This is desirable as
			// any previously installed chunks get mounted 
			if (!CurrentMountPaks->Contains(PakPath) && !MountedPaks.Contains(PakPath))
			{
				if (FCoreDelegates::OnMountPak.IsBound())
				{
					FCoreDelegates::OnMountPak.Execute(PakPath, PakReadOrder);
					MountedPaks.Add(PakPath);
					//Register the install
					BPSModule->RegisterAppInstallation(Manifest.ToSharedRef(), FilenameOrDirectory);
				}
			}
		}
		return true;
	}

	static const TCHAR *Name()
	{
		return TEXT("FChunkSetup");
	}
};