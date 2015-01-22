// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FChunkInstallTask : public FNonAbandonableTask
{
public:
	/** Input parameters */
	FString							ManifestPath;
	FString							SrcDir;
	FString							DestDir;
	IBuildPatchServicesModule*		BPSModule;
	IBuildManifestPtr				BuildManifest;
	bool							bCopy;
	const TArray<FString>*			CurrentMountPaks;
	/** Output */
	TArray<FString>					MountedPaks;

	FChunkInstallTask()
	{}

	void Init(FString InManifestPath, FString InSrcDir, FString InDestDir, IBuildPatchServicesModule* InBPSModule, IBuildManifestRef InBuildManifest, const TArray<FString>& InCurrentMountedPaks, bool bInCopy)
	{
		ManifestPath = InManifestPath;
		SrcDir = InSrcDir;
		DestDir = InDestDir;
		BPSModule = InBPSModule;
		BuildManifest = InBuildManifest;
		CurrentMountPaks = &InCurrentMountedPaks;
		bCopy = bInCopy;

		MountedPaks.Reset();
	}

	void DoWork()
	{
		// Helper class to find all pak files.
		class FPakSearchVisitor : public IPlatformFile::FDirectoryVisitor
		{
			TArray<FString>& FoundPakFiles;
		public:
			FPakSearchVisitor(TArray<FString>& InFoundPakFiles)
				: FoundPakFiles(InFoundPakFiles)
			{}
			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
			{
				if (bIsDirectory == false)
				{
					FString Filename(FilenameOrDirectory);
					if (FPaths::GetExtension(Filename) == TEXT("pak"))
					{
						FoundPakFiles.Add(Filename);
					}
				}
				return true;
			}
		};

		check(CurrentMountPaks);

		BPSModule->SaveManifestToFile(ManifestPath, BuildManifest.ToSharedRef());
		if (bCopy)
		{
			TArray<FString> PakFiles;
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (PlatformFile.DirectoryExists(*DestDir))
			{
				PlatformFile.DeleteDirectoryRecursively(*DestDir);
			}
			PlatformFile.CreateDirectoryTree(*DestDir);
			if (PlatformFile.CopyDirectoryTree(*DestDir, *SrcDir, true))
			{
				PlatformFile.DeleteDirectoryRecursively(*SrcDir);
			}
			// Find all pak files.
			FPakSearchVisitor Visitor(PakFiles);
			PlatformFile.IterateDirectoryRecursively(*DestDir, Visitor);
			auto PakReadOrderField = BuildManifest->GetCustomField("PakReadOrdering");
			uint32 PakReadOrder = PakReadOrderField.IsValid() ? (uint32)PakReadOrderField->AsInteger() : 0;
			for (uint32 PakIndex = 0, PakCount = PakFiles.Num(); PakIndex < PakCount; ++PakIndex)
			{
				if (!CurrentMountPaks->Contains(PakFiles[PakIndex]) && !MountedPaks.Contains(PakFiles[PakIndex]))
				{
					if (FCoreDelegates::OnMountPak.IsBound())
					{
						FCoreDelegates::OnMountPak.Execute(PakFiles[PakIndex], PakReadOrder);
						MountedPaks.Add(PakFiles[PakIndex]);
					}
				}
			}
			//Register the install
			BPSModule->RegisterAppInstallation(BuildManifest.ToSharedRef(), DestDir);
		}
	}

	static const TCHAR *Name()
	{
		return TEXT("FChunkDescovery");
	}
};
