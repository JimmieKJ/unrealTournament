// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
BuildPatchCompactifier.cpp: Implements the classes that clean up chunk and file
data that are no longer referenced by the manifests in a given cloud directory.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

#if WITH_BUILDPATCHGENERATION

namespace BuildDataCompactifierDefs
{
	const static uint32 TouchFailureThresholdPercentage = 10;
	const static int64 TimeStampTolerance = FTimespan::FromMinutes(1).GetTicks();
}

/* Constructors
*****************************************************************************/

FBuildDataCompactifier::FBuildDataCompactifier(const FString& InCloudDir, const bool bInPreview, const bool bInNoPatchDelete)
	: CloudDir(InCloudDir)
	, bPreview(bInPreview)
	, bNoPatchDelete(bInNoPatchDelete)
{

}

/* Public static methods
*****************************************************************************/

bool FBuildDataCompactifier::CompactifyCloudDirectory(const FString& CloudDir, const TArray<FString>& ManifestsToKeep, const float DataAgeThreshold, const bool bPreview, const bool bNoPatchDelete)
{
	FBuildDataCompactifier Compactifier(CloudDir, bPreview, bNoPatchDelete);
	return Compactifier.Compactify(ManifestsToKeep, DataAgeThreshold);
}

bool FBuildDataCompactifier::CompactifyCloudDirectory(const TArray<FString>& ManifestsToKeep, const float DataAgeThreshold, const bool bPreview, const bool bNoPatchDelete)
{
	return CompactifyCloudDirectory(FBuildPatchServicesModule::GetCloudDirectory(), ManifestsToKeep, DataAgeThreshold, bPreview, bNoPatchDelete);
}

/* Private methods
*****************************************************************************/

bool FBuildDataCompactifier::Compactify(const TArray<FString>& ManifestsToKeep, const float DataAgeThreshold) const
{
	GLog->Logf(TEXT("Running Compactify on %s%s"), *CloudDir, bPreview ? TEXT(". Preview mode. NO action will be taken.") : bNoPatchDelete ? TEXT(". NoPatchDelete mode. NO patch data will be deleted.") : TEXT(""));
	if (ManifestsToKeep.Num() > 0)
	{
		GLog->Logf(TEXT("Preserving manifest files: %s"), *FString::Join(ManifestsToKeep, TEXT(", ")));
	}
	GLog->Logf(TEXT("Minimum age of deleted chunks: %.3f days"), DataAgeThreshold);

	// We'll work out the date of the oldest unreferenced file we'll keep
	FDateTime Cutoff = FDateTime::UtcNow() - FTimespan::FromDays(DataAgeThreshold);

	// We'll get ALL files first, so we can use the count to preallocate space within the data filenames array to save excessive reallocs
	TArray<FString> AllFiles;
	const bool bFindFiles = true;
	const bool bFindDirectories = false;
	IFileManager::Get().FindFilesRecursive(AllFiles, *CloudDir, TEXT("*.*"), bFindFiles, bFindDirectories);

	TSet<FGuid> ReferencedGuids; // The master list of *ALL* referenced chunk / file data Guids

	TArray<FString> ManifestFilenames;
	TArray<FString> DeletedManifestFilenames;
	TArray<FGuid> DataGuids; // The Guids associated with the data files from a single manifest
	int32 NumDataFiles = 0;
	uint64 ManifestBytesDeleted = 0;
	
	// Preallocate enough storage in DataGuids, to stop repeatedly expanding the allocation
	DataGuids.Reserve(AllFiles.Num());

	EnumerateManifests(ManifestFilenames);

	if (!DoAllManifestsExist(ManifestFilenames, ManifestsToKeep))
	{
		// At least one of the manifests we want to keep does not exist. This is an error condition
		GLog->Log(ELogVerbosity::Error, TEXT("Not all manifests to keep exist. Aborting operation"));
		return false;
	}

	if (!DeleteNonReferencedManifests(ManifestFilenames, ManifestsToKeep, DeletedManifestFilenames, ManifestBytesDeleted))
	{
		// An error occurred deleting one or more of the manifest files. This is an error condition
		GLog->Log(ELogVerbosity::Error, TEXT("Could not delete one or more manifest files. Aborting operation"));
		return false;
	}

	// If we don't have any manifest files, we'll treat that as an error condition
	if (ManifestFilenames.Num() == 0)
	{
		GLog->Log(ELogVerbosity::Warning, TEXT("Could not find any manifest files. Aborting operation."));
		return true;  // We're still going to return a success code, as this isn't a hard error
	}

	// Process each remaining manifest, and build up a list of all referenced files
	for (const auto& ManifestFilename : ManifestFilenames)
	{
		const FString ManifestPath = CloudDir / ManifestFilename;
		GLog->Logf(TEXT("Extracting chunk filenames from %s"), *ManifestFilename);

		// Load the manifest data from the manifest
		FBuildPatchAppManifest Manifest;
		if (Manifest.LoadFromFile(ManifestPath))
		{
			// Work out all data Guids referenced in the manifest, and add them to our list of files to keep
			DataGuids.Empty();
			Manifest.GetDataList(DataGuids);

			GLog->Logf(TEXT("Extracted %d chunks from %s. Unioning with %d existing chunks"), DataGuids.Num(), *ManifestFilename, NumDataFiles);
			NumDataFiles += DataGuids.Num();

			// We're going to keep all the Guids so we know which files to keep later
			ReferencedGuids.Append(DataGuids);
		}
		else
		{
			// We failed to read from the manifest file.  This is an error which should halt progress and return a non-zero exit code
			FString ErrorMessage = FString::Printf(TEXT("Could not parse manifest file %s"), *ManifestFilename);
			GLog->Log(ELogVerbosity::Error, *ErrorMessage);
			return false;
		}
	}

	GLog->Logf(TEXT("Compactify walking %s to touch referenced chunks, remove all aged unreferenced chunks and compute statistics."), *CloudDir);

	uint32 FilesProcessed = 0;
	uint32 FilesTouched = 0;
	uint32 FilesSkipped = 0;
	uint32 NonPatchFilesProcessed = 0;
	uint32 FilesDeleted = DeletedManifestFilenames.Num();
	uint64 BytesProcessed = 0;
	uint64 BytesTouched = 0;
	uint64 BytesSkipped = 0;
	uint64 NonPatchBytesProcessed = 0;
	uint64 BytesDeleted = ManifestBytesDeleted;
	uint64 CurrentFileSize;
	uint32 TouchFailureCount = 0;
	uint64 BytesFailedTouch = 0;
	FGuid FileGuid;
	FDateTime Now = FDateTime::UtcNow();
	const uint32 TouchFailureErrorThreshold = FMath::DivideAndRoundUp((uint32)AllFiles.Num(), BuildDataCompactifierDefs::TouchFailureThresholdPercentage);

	for (const auto& File : AllFiles)
	{
		CurrentFileSize = IFileManager::Get().FileSize(*File);
		if (CurrentFileSize >= 0)
		{
			++FilesProcessed;
			BytesProcessed += CurrentFileSize;

			if (!GetPatchDataGuid(File, FileGuid))
			{
				FString CleanFilename = FPaths::GetCleanFilename(File);
				if (!ManifestFilenames.Contains(CleanFilename) && !DeletedManifestFilenames.Contains(CleanFilename))
				{
					++NonPatchFilesProcessed;
					NonPatchBytesProcessed += CurrentFileSize;
				}
				continue;
			}

			if (ReferencedGuids.Contains(FileGuid))
			{
				// This is a valid, referenced file, so we need to touch it with the current date
				++FilesTouched;
				BytesTouched += CurrentFileSize;
				if (!bPreview)
				{
					if (!SafeSetTimeStamp(*File, Now))
					{
						++TouchFailureCount;
						BytesFailedTouch += CurrentFileSize;
						GLog->Logf(ELogVerbosity::Warning, TEXT("Failed to set timestamp on file %s"), *File);
						if (TouchFailureCount >= TouchFailureErrorThreshold)
						{
							GLog->Logf(ELogVerbosity::Error, TEXT("Failed to set timestamp on %u%% of files"), BuildDataCompactifierDefs::TouchFailureThresholdPercentage);
							return false;
						}
					}
				}
			}
			else if (IFileManager::Get().GetTimeStamp(*File) < Cutoff)
			{
				// This file is not referenced by any manifest, is a data file, and is older than we want to keep ...
				// Let's get rid of it!
				DeleteFile(File);
				++FilesDeleted;
				BytesDeleted += CurrentFileSize;
			}
			else
			{
				++FilesSkipped;
				BytesSkipped += CurrentFileSize;
			}
		}
		else
		{
			GLog->Logf(TEXT("Warning. Could not determine size of %s. Perhaps it has been removed by another process."), *File);
		}
	}

	FilesTouched -= TouchFailureCount;
	BytesTouched -= BytesFailedTouch;

	GLog->Logf(TEXT("Compactify of %s complete!"), *CloudDir);
	GLog->Logf(TEXT("Compactify found %u files totalling %s."), FilesProcessed, *HumanReadableSize(BytesProcessed));
	if (NonPatchFilesProcessed > 0)
	{
		GLog->Logf(TEXT("Of these, %u (totalling %s) were not chunk/manifest files."), NonPatchFilesProcessed, *HumanReadableSize(NonPatchBytesProcessed));
	}
	if (bNoPatchDelete)
	{
		GLog->Logf(TEXT("Compactify skipped deleting %u unreferenced chunk/manifest files (totalling %s) due to -nopatchdelete being specified."), FilesDeleted, *HumanReadableSize(BytesDeleted));
	}
	else
	{
		GLog->Logf(TEXT("Compactify deleted %u chunk/manifest files totalling %s."), FilesDeleted, *HumanReadableSize(BytesDeleted));
	}
	GLog->Logf(TEXT("Compactify touched %u chunk files totalling %s."), FilesTouched, *HumanReadableSize(BytesTouched));
	if (TouchFailureCount > 0)
	{
		GLog->Logf(TEXT("Compactify failed to touch %u files toatlling %s."), TouchFailureCount, *HumanReadableSize(BytesFailedTouch));
	}
	GLog->Logf(TEXT("Compactify skipped %u unreferenced chunk files (totalling %s) which have not yet aged out."), FilesSkipped, *HumanReadableSize(BytesSkipped));
	return true;
}

void FBuildDataCompactifier::DeleteFile(const FString& FilePath) const
{
	FString LogMsg = FString::Printf(TEXT("Deprecated data %s"), *FilePath);
	if (!bNoPatchDelete && !bPreview)
	{
		LogMsg = LogMsg.Append(TEXT(" ... deleted"));
		IFileManager::Get().Delete(*FilePath);
	}
	GLog->Logf(*LogMsg);
}

void FBuildDataCompactifier::EnumerateManifests(TArray<FString>& OutManifests) const
{
	// Get a list of all manifest filenames by using IFileManager
	FString FilePattern = CloudDir / TEXT("*.manifest");
	IFileManager::Get().FindFiles(OutManifests, *FilePattern, true, false);
}

bool FBuildDataCompactifier::DoAllManifestsExist(const TArray<FString>& AllManifests, const TArray<FString>& Manifests) const
{
	bool bSuccess = true;
	for (const auto& Manifest : Manifests)
	{
		if (!AllManifests.Contains(Manifest))
		{
			GLog->Logf(ELogVerbosity::Error, TEXT("Could not locate specified manifest file %s"), *Manifest);
			bSuccess = false;
		}
	}
	return bSuccess;
}

bool FBuildDataCompactifier::DeleteNonReferencedManifests(TArray<FString>& AllManifests, const TArray<FString>& ManifestsToKeep, TArray<FString>& DeletedManifests, uint64& BytesDeleted) const
{
	if (ManifestsToKeep.Num() == 0)
	{
		return true; // We don't need to delete anything, just return a success response
	}

	for (const auto& Manifest : AllManifests)
	{
		if (!ManifestsToKeep.Contains(Manifest))
		{
			FString LogMsg = FString::Printf(TEXT("Found deleteable manifest file %s"), *Manifest);
			FString ManifestPath = CloudDir / Manifest;

			DeletedManifests.Add(Manifest);
			BytesDeleted += IFileManager::Get().FileSize(*ManifestPath);

			if (!bPreview && !bNoPatchDelete)
			{
				IFileManager::Get().Delete(*ManifestPath);
				if (FPaths::FileExists(ManifestPath))
				{
					// Something went wrong ... the file still exists!
					GLog->Logf(ELogVerbosity::Error, TEXT("Compactify could not delete manifest file %s"), *Manifest);
					return false;
				}
				LogMsg.Append(TEXT(" ... deleted"));
			}
			GLog->Logf(*LogMsg);
		}
	}

	AllManifests = ManifestsToKeep;
	return true; // None of the manifests we need to get rid of still exist, we succeeded
}

bool FBuildDataCompactifier::GetPatchDataGuid(const FString& FilePath, FGuid& OutGuid) const
{
	FString Extension = FPaths::GetExtension(FilePath);
	if (Extension != TEXT("chunk") && Extension != TEXT("file"))
	{
		// Our filename doesn't have one of the allowed file extensions, so it's not patch data
		return false;
	}

	FString BaseFileName = FPaths::GetBaseFilename(FilePath);
	if (BaseFileName.Contains(TEXT("_")))
	{
		FString Right;
		BaseFileName.Split(TEXT("_"), NULL, &Right);
		if (Right.Len() != 32)
		{
			return false; // Valid data files which contain an _ in their filename must have 32 characters to the right of the underscore
		}
	}
	else
	{
		if (BaseFileName.Len() != 32)
		{
			return false; // Valid data files whose names do not contain underscores must be 32 characters long
		}
	}

	return FBuildPatchUtils::GetGUIDFromFilename(FilePath, OutGuid);
}

FString FBuildDataCompactifier::HumanReadableSize(uint64 NumBytes, uint8 DecimalPlaces, bool bUseBase10) const
{
	static const TCHAR* const Suffixes[2][7] = { { TEXT("Bytes"), TEXT("kB"), TEXT("MB"), TEXT("GB"), TEXT("TB"), TEXT("PB"), TEXT("EB") },
	                                             { TEXT("Bytes"), TEXT("KiB"), TEXT("MiB"), TEXT("GiB"), TEXT("TiB"), TEXT("PiB"), TEXT("EiB") }};

	float DataSize = (float)NumBytes;

	const int32 Base = bUseBase10 ? 1000 : 1024;
	int32 Index = FMath::FloorToInt(FMath::LogX(Base, NumBytes));
	Index = FMath::Clamp<int32>(Index, 0, ARRAY_COUNT(Suffixes[0]) - 1);

	DecimalPlaces = FMath::Clamp<int32>(DecimalPlaces, 0, Index*3);

	return FString::Printf(TEXT("%.*f %s"), DecimalPlaces, DataSize / (FMath::Pow(Base, Index)), Suffixes[bUseBase10 ? 0 : 1][Index]);
}

bool FBuildDataCompactifier::SafeSetTimeStamp(const FString& FilePath, FDateTime TimeStamp) const
{
	bool bSuccess = false;
	IFileManager& FileMan = IFileManager::Get();
	bSuccess = FileMan.SetTimeStamp(*FilePath, TimeStamp);
	if (bSuccess)
	{
		FDateTime ActualTimeStamp = FileMan.GetTimeStamp(*FilePath);
		int64 TickVariance = FMath::Abs(ActualTimeStamp.GetTicks() - TimeStamp.GetTicks());
		bSuccess = TickVariance < BuildDataCompactifierDefs::TimeStampTolerance;
	}
	return bSuccess;
}
#endif
