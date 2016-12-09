// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchDiffManifests.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"
#include "Async/Future.h"
#include "Async/Async.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDiffManifests, Log, All);
DEFINE_LOG_CATEGORY(LogDiffManifests);

// For the output file we'll use pretty json in debug, otherwise condensed.
#if UE_BUILD_DEBUG
typedef  TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDiffJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDiffJsonWriterFactory;
#else
typedef  TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDiffJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDiffJsonWriterFactory;
#endif //UE_BUILD_DEBUG

namespace DiffHelpers
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
}

bool FBuildDiffManifests::DiffManifests(const FString& ManifestFilePathA, const FString& ManifestFilePathB, const FString& OutputFilePath)
{
	bool bSuccess = true;
	FCriticalSection UObjectAllocationLock;

	TFunction<FBuildPatchAppManifestPtr()> TaskManifestA = [&UObjectAllocationLock, &ManifestFilePathA]()
	{
		return DiffHelpers::LoadManifestFile(ManifestFilePathA, &UObjectAllocationLock);
	};
	TFunction<FBuildPatchAppManifestPtr()> TaskManifestB = [&UObjectAllocationLock, &ManifestFilePathB]()
	{
		return DiffHelpers::LoadManifestFile(ManifestFilePathB, &UObjectAllocationLock);
	};

	TFuture<FBuildPatchAppManifestPtr> FutureManifestA = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskManifestA));
	TFuture<FBuildPatchAppManifestPtr> FutureManifestB = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskManifestB));

	FBuildPatchAppManifestPtr ManifestA = FutureManifestA.Get();
	FBuildPatchAppManifestPtr ManifestB = FutureManifestB.Get();

	// Flush any logs collected by tasks
	GLog->FlushThreadedLogs();

	// We must have loaded our manifests
	if (ManifestA.IsValid() == false)
	{
		UE_LOG(LogDiffManifests, Error, TEXT("Could not load manifest %s"), *ManifestFilePathA);
		return false;
	}
	if (ManifestB.IsValid() == false)
	{
		UE_LOG(LogDiffManifests, Error, TEXT("Could not load manifest %s"), *ManifestFilePathB);
		return false;
	}

	int64 NewChunksCount = 0;
	int64 TotalChunkSize = 0;
	TArray<FString> NewChunkPaths;
	for (TPair<FGuid, FChunkInfoData*>& ChunkInfoIterator : ManifestB->ChunkInfoLookup)
	{
		FChunkInfoData& ChunkInfoRef = *ChunkInfoIterator.Value;
		if (ManifestA->ChunkInfoLookup.Contains(ChunkInfoRef.Guid) == false)
		{
			++NewChunksCount;
			TotalChunkSize += ChunkInfoRef.FileSize;
			NewChunkPaths.Add(FBuildPatchUtils::GetDataFilename(ManifestB.ToSharedRef(), TEXT("."), ChunkInfoRef.Guid));
			UE_LOG(LogDiffManifests, Verbose, TEXT("New chunk discovered: Size: %10lld, Path: %s"), ChunkInfoRef.FileSize, *NewChunkPaths.Last());
		}
	}

	UE_LOG(LogDiffManifests, Log, TEXT("New chunks:  %lld"), NewChunksCount);
	UE_LOG(LogDiffManifests, Log, TEXT("Total bytes: %lld"), TotalChunkSize);

	// Log download details
	FNumberFormattingOptions SizeFormattingOptions;
	SizeFormattingOptions.MaximumFractionalDigits = 3;
	SizeFormattingOptions.MinimumFractionalDigits = 3;

	TSet<FString> TagsA;
	TSet<FString> TagsB;
	ManifestA->GetFileTagList(TagsA);
	ManifestB->GetFileTagList(TagsB);
	int64 DownloadSizeA = ManifestA->GetDownloadSize(TagsA);
	int64 BuildSizeA = ManifestA->GetBuildSize(TagsA);
	int64 DownloadSizeB = ManifestB->GetDownloadSize(TagsB);
	int64 BuildSizeB = ManifestB->GetBuildSize(TagsB);
	int64 DeltaDownloadSize = ManifestB->GetDeltaDownloadSize(TagsB, ManifestA.ToSharedRef());

	UE_LOG(LogDiffManifests, Log, TEXT(""));
	UE_LOG(LogDiffManifests, Log, TEXT("%s %s:"), *ManifestA->GetAppName(), *ManifestA->GetVersionString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Download Size: %10s"), *FText::AsMemory(DownloadSizeA, &SizeFormattingOptions).ToString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Build Size:    %10s"), *FText::AsMemory(BuildSizeA, &SizeFormattingOptions).ToString())
	UE_LOG(LogDiffManifests, Log, TEXT("%s %s:"), *ManifestB->GetAppName(), *ManifestB->GetVersionString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Download Size: %10s"), *FText::AsMemory(DownloadSizeB, &SizeFormattingOptions).ToString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Build Size:    %10s"), *FText::AsMemory(BuildSizeB, &SizeFormattingOptions).ToString());
	UE_LOG(LogDiffManifests, Log, TEXT("%s %s -> %s %s:"), *ManifestA->GetAppName(), *ManifestA->GetVersionString(), *ManifestB->GetAppName(), *ManifestB->GetVersionString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Delta Size:    %10s"), *FText::AsMemory(DeltaDownloadSize, &SizeFormattingOptions).ToString());

	if (bSuccess && OutputFilePath.IsEmpty() == false)
	{
		FString JsonOutput;
		TSharedRef<FDiffJsonWriter> Writer = FDiffJsonWriterFactory::Create(&JsonOutput);
		Writer->WriteObjectStart();
		{
			Writer->WriteObjectStart(TEXT("ManifestA"));
			{
				Writer->WriteValue(TEXT("AppName"), ManifestA->GetAppName());
				Writer->WriteValue(TEXT("AppId"), static_cast<int32>(ManifestA->GetAppID()));
				Writer->WriteValue(TEXT("VersionString"), ManifestA->GetVersionString());
				Writer->WriteValue(TEXT("DownloadSize"), DownloadSizeA);
				Writer->WriteValue(TEXT("BuildSize"), BuildSizeA);
			}
			Writer->WriteObjectEnd();
			Writer->WriteObjectStart(TEXT("ManifestB"));
			{
				Writer->WriteValue(TEXT("AppName"), ManifestB->GetAppName());
				Writer->WriteValue(TEXT("AppId"), static_cast<int32>(ManifestB->GetAppID()));
				Writer->WriteValue(TEXT("VersionString"), ManifestB->GetVersionString());
				Writer->WriteValue(TEXT("DownloadSize"), DownloadSizeB);
				Writer->WriteValue(TEXT("BuildSize"), BuildSizeB);
			}
			Writer->WriteObjectEnd();
			Writer->WriteObjectStart(TEXT("Differential"));
			{
				Writer->WriteArrayStart(TEXT("NewChunkPaths"));
				{
					for (const FString& NewChunkPath : NewChunkPaths)
					{
						Writer->WriteValue(NewChunkPath);
					}
				}
				Writer->WriteArrayEnd();
				Writer->WriteValue(TEXT("TotalChunkSize"), TotalChunkSize);
				Writer->WriteValue(TEXT("DeltaDownloadSize"), DeltaDownloadSize);
			}
			Writer->WriteObjectEnd();
		}
		Writer->WriteObjectEnd();
		Writer->Close();
		bSuccess = FFileHelper::SaveStringToFile(JsonOutput, *OutputFilePath);
		if (!bSuccess)
		{
			UE_LOG(LogDiffManifests, Error, TEXT("Could not save output to %s"), *OutputFilePath);
		}
	}

	return bSuccess;
}
