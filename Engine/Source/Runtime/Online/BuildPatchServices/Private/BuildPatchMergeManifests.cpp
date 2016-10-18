// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchServicesPrivatePCH.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMergeManifests, Log, All);
DEFINE_LOG_CATEGORY(LogMergeManifests);

namespace MergeHelpers
{
	FBuildPatchAppManifestPtr LoadManifestFile(const FString& ManifestFilePath, FCriticalSection* UObjectAllocationLock)
	{
		check(UObjectAllocationLock != nullptr);
		UObjectAllocationLock->Lock();
		FBuildPatchAppManifestPtr Manifest = MakeShareable(new FBuildPatchAppManifest());
		UObjectAllocationLock->Unlock();
		if (Manifest->LoadFromFile(ManifestFilePath))
		{
			return Manifest;
		}
		return FBuildPatchAppManifestPtr();
	}

	bool CopyFileDataFromManifestToObject(const TSet<FString>& Filenames, const FBuildPatchAppManifestPtr& Manifest, UBuildPatchManifest* Object, const TCHAR* ManifestName)
	{
		bool bSuccess = true;
		for (const FString& Filename : Filenames)
		{
			check(Manifest.IsValid());
			check(Object != nullptr);
			const FFileManifestData* FileManifest = Manifest->GetFileManifest(Filename);
			if (FileManifest == nullptr)
			{
				UE_LOG(LogMergeManifests, Error, TEXT("Could not find file in %s: %s"), ManifestName, *Filename);
				bSuccess = false;
			}
			else
			{
				Object->FileManifestList.Add(*FileManifest);
			}
		}
		return bSuccess;
	}
}

bool FBuildMergeManifests::MergeManifests(const FString& ManifestFilePathA, const FString& ManifestFilePathB, const FString& ManifestFilePathC, const FString& NewVersionString, const FString& SelectionDetailFilePath)
{
	bool bSuccess = true;
	FCriticalSection UObjectAllocationLock;

	TFunction<FBuildPatchAppManifestPtr()> TaskManifestA = [&UObjectAllocationLock, &ManifestFilePathA]()
	{
		return MergeHelpers::LoadManifestFile(ManifestFilePathA, &UObjectAllocationLock);
	};
	TFunction<FBuildPatchAppManifestPtr()> TaskManifestB = [&UObjectAllocationLock, &ManifestFilePathB]()
	{
		return MergeHelpers::LoadManifestFile(ManifestFilePathB, &UObjectAllocationLock);
	};
	typedef TPair<TSet<FString>,TSet<FString>> TStringSetPair;
	FThreadSafeBool bSelectionDetailSuccess = false;
	TFunction<TStringSetPair()> TaskSelectionInfo = [&SelectionDetailFilePath, &bSelectionDetailSuccess]()
	{
		bSelectionDetailSuccess = true;
		TStringSetPair StringSetPair;
		if(SelectionDetailFilePath.IsEmpty() == false)
		{
			FString SelectionDetailFileData;
			bSelectionDetailSuccess = FFileHelper::LoadFileToString(SelectionDetailFileData, *SelectionDetailFilePath);
			if (bSelectionDetailSuccess)
			{
				TArray<FString> SelectionDetailLines;
				SelectionDetailFileData.ParseIntoArrayLines(SelectionDetailLines);
				for (int32 LineIdx = 0; LineIdx < SelectionDetailLines.Num(); ++LineIdx)
				{
					FString Filename, Source;
					SelectionDetailLines[LineIdx].Split(TEXT("\t"), &Filename, &Source, ESearchCase::CaseSensitive);
					Filename = Filename.Trim().TrimTrailing().TrimQuotes();
					FPaths::NormalizeDirectoryName(Filename);
					Source = Source.Trim().TrimTrailing().TrimQuotes();
					if (Source == TEXT("A"))
					{
						StringSetPair.Key.Add(Filename);
					}
					else if (Source == TEXT("B"))
					{
						StringSetPair.Value.Add(Filename);
					}
					else
					{
						UE_LOG(LogMergeManifests, Error, TEXT("Could not parse line %d from %s"), LineIdx + 1, *SelectionDetailFilePath);
						bSelectionDetailSuccess = false;
					}
				}
			}
			else
			{
				UE_LOG(LogMergeManifests, Error, TEXT("Could not load selection detail file %s"), *SelectionDetailFilePath);
			}
		}
		return MoveTemp(StringSetPair);
	};

	TFuture<FBuildPatchAppManifestPtr> FutureManifestA = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskManifestA));
	TFuture<FBuildPatchAppManifestPtr> FutureManifestB = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskManifestB));
	TFuture<TStringSetPair> FutureSelectionDetail = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskSelectionInfo));

	FBuildPatchAppManifestPtr ManifestA = FutureManifestA.Get();
	FBuildPatchAppManifestPtr ManifestB = FutureManifestB.Get();
	TStringSetPair SelectionDetail = FutureSelectionDetail.Get();

	// Flush any logs collected by tasks
	GLog->FlushThreadedLogs();

	// We must have loaded our manifests
	if (ManifestA.IsValid() == false)
	{
		UE_LOG(LogMergeManifests, Error, TEXT("Could not load manifest %s"), *ManifestFilePathA);
		return false;
	}
	if (ManifestB.IsValid() == false)
	{
		UE_LOG(LogMergeManifests, Error, TEXT("Could not load manifest %s"), *ManifestFilePathB);
		return false;
	}

	// Check if the selection detail had an error
	if (bSelectionDetailSuccess == false)
	{
		return false;
	}

	// If we have no selection detail, then we take the union of all files, preferring the version from B
	if (SelectionDetail.Key.Num() == 0 && SelectionDetail.Value.Num() == 0)
	{
		TSet<FString> ManifestFilesA(ManifestA->GetBuildFileList());
		SelectionDetail.Value.Append(ManifestB->GetBuildFileList());
		SelectionDetail.Key = ManifestFilesA.Difference(SelectionDetail.Value);
	}

	// Create the new manifest
	FBuildPatchAppManifest MergedManifest;

	// Helpful pointers
	UBuildPatchManifest* DataA = ManifestA->Data;
	UBuildPatchManifest* DataB = ManifestB->Data;
	UBuildPatchManifest* DataC = MergedManifest.Data;

	// Copy basic info from B
	DataC->ManifestFileVersion = DataB->ManifestFileVersion;
	DataC->bIsFileData = DataB->bIsFileData;
	DataC->AppID = DataB->AppID;
	DataC->AppName = DataB->AppName;
	DataC->LaunchExe = DataB->LaunchExe;
	DataC->LaunchCommand = DataB->LaunchCommand;
	DataC->PrereqName = DataB->PrereqName;
	DataC->PrereqPath = DataB->PrereqPath;
	DataC->PrereqArgs = DataB->PrereqArgs;
	DataC->CustomFields = DataB->CustomFields;

	// Set the new version string
	DataC->BuildVersion = NewVersionString;

	// Copy the file manifests required from A
	bSuccess = MergeHelpers::CopyFileDataFromManifestToObject(SelectionDetail.Key, ManifestA, DataC, TEXT("ManifestA")) && bSuccess;

	// Copy the file manifests required from B
	bSuccess = MergeHelpers::CopyFileDataFromManifestToObject(SelectionDetail.Value, ManifestB, DataC, TEXT("ManifestB")) && bSuccess;

	// Sort the file manifests before entering chunk info
	DataC->FileManifestList.Sort();

	// Fill out the chunk list in order of reference
	TSet<FGuid> ReferencedChunks;
	for (const FFileManifestData& FileManifest: DataC->FileManifestList)
	{
		for (const FChunkPartData& FileChunkPart : FileManifest.FileChunkParts)
		{
			if (ReferencedChunks.Contains(FileChunkPart.Guid) == false)
			{
				ReferencedChunks.Add(FileChunkPart.Guid);

				// Find the chunk info
				FChunkInfoData** ChunkInfo = ManifestB->ChunkInfoLookup.Find(FileChunkPart.Guid);
				if (ChunkInfo == nullptr)
				{
					ChunkInfo = ManifestA->ChunkInfoLookup.Find(FileChunkPart.Guid);
				}
				if (ChunkInfo == nullptr)
				{
					UE_LOG(LogMergeManifests, Error, TEXT("Failed to copy chunk meta for %s used by %s. Possible damaged manifest file as input."), *FileChunkPart.Guid.ToString(), *FileManifest.Filename);
					bSuccess = false;
				}
				else
				{
					DataC->ChunkList.Add(**ChunkInfo);
				}
			}
		}
	}

	// Save the new manifest out if we didn't register a failure
	if (bSuccess)
	{
		MergedManifest.InitLookups();
		if (!MergedManifest.SaveToFile(ManifestFilePathC, false))
		{
			UE_LOG(LogMergeManifests, Error, TEXT("Failed to save new manifest %s"), *ManifestFilePathC);
			bSuccess = false;
		}
	}
	else
	{
		UE_LOG(LogMergeManifests, Error, TEXT("Not saving new manifest due to previous errors."));
	}

	return bSuccess;
}
