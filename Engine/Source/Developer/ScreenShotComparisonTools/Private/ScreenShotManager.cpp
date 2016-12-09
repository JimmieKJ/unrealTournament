// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ScreenShotManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "Misc/EngineVersion.h"
#include "Misc/FilterCollection.h"
#include "AutomationWorkerMessages.h"
#include "Helpers/MessageEndpointBuilder.h"
#include "ScreenShotBaseNode.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

class FScreenshotComparisons
{
public:
	FString ApprovedFolder;
	FString UnapprovedFolder;

	TArray<FString> Existing;
	TArray<FString> New;
	TArray<FString> Missing;
};

FScreenShotManager::FScreenShotManager()
{
	ScreenshotUnapprovedFolder = FPaths::GameSavedDir() / TEXT("Automation/Incoming/");
	ScreenshotDeltaFolder = FPaths::GameSavedDir() / TEXT("Automation/Delta/");
	ScreenshotResultsFolder = FPaths::GameSavedDir() / TEXT("Automation/");
	ScreenshotApprovedFolder = FPaths::GameDir() / TEXT("Test/Screenshots/");
}

FString FScreenShotManager::GetLocalUnapprovedFolder() const
{
	return FPaths::ConvertRelativePathToFull(ScreenshotUnapprovedFolder);
}

FString FScreenShotManager::GetLocalApprovedFolder() const
{
	return FPaths::ConvertRelativePathToFull(ScreenshotApprovedFolder);
}

TSharedRef<FScreenshotComparisons> FScreenShotManager::GenerateFileList() const
{
	TSharedRef<FScreenshotComparisons> Comparisons = MakeShareable(new FScreenshotComparisons());

	Comparisons->ApprovedFolder = ScreenshotApprovedFolder;
	Comparisons->UnapprovedFolder = ScreenshotUnapprovedFolder;

	TArray<FString> UnapprovedShots;
	TArray<FString> ApprovedShots;

	{
		TArray<FString> UnapprovedShotList;
		IFileManager::Get().FindFilesRecursive(UnapprovedShotList, *ScreenshotUnapprovedFolder, TEXT("*.png"), true, false);

		TArray<FString> ApprovedShotList;
		IFileManager::Get().FindFilesRecursive(ApprovedShotList, *ScreenshotApprovedFolder, TEXT("*.png"), true, false);

		for ( FString UnapprovedPngFile : UnapprovedShotList )
		{
			FPaths::MakePathRelativeTo(UnapprovedPngFile, *ScreenshotUnapprovedFolder);
			UnapprovedShots.AddUnique(FPaths::GetPath(UnapprovedPngFile));
		}

		for ( FString ApprovedPngFile : ApprovedShotList )
		{
			FPaths::MakePathRelativeTo(ApprovedPngFile, *ScreenshotApprovedFolder);
			ApprovedShots.AddUnique(FPaths::GetPath(ApprovedPngFile));
		}
	}

	for ( int32 ShotIndex = 0; ShotIndex < UnapprovedShots.Num(); ShotIndex++ )
	{
		const FString& UnapprovedShot = UnapprovedShots[ShotIndex];

		// Look for an exact match first
		if ( ApprovedShots.Contains(UnapprovedShot) )
		{
			Comparisons->Existing.Add(UnapprovedShot);
			ApprovedShots.Remove(UnapprovedShot);
		}
		else
		{
			Comparisons->New.Add(UnapprovedShot);
		}

		UnapprovedShots.RemoveAt(ShotIndex);
		ShotIndex--;
	}

	for ( int32 ShotIndex = 0; ShotIndex < ApprovedShots.Num(); ShotIndex++ )
	{
		const FString& ApprovedShot = ApprovedShots[ShotIndex];

		Comparisons->Missing.Add(ApprovedShot);
	}

	return Comparisons;
}

void FScreenShotManager::CreateData()
{
	TSet<FString> TempPlatforms;

	// Clear existing data
	CachedPlatformList.Empty();
	ScreenShotDataArray.Empty();

	// Add platforms for searching
	CachedPlatformList.Add( MakeShareable(new FString("Any") ) );

	for ( FString Platform : TempPlatforms )
	{
		TSharedPtr<FString> PlatformName = MakeShareable(new FString(Platform));
		CachedPlatformList.Add(PlatformName);
	}

	SetFilter( nullptr );
}

TArray< TSharedPtr<FScreenShotDataItem> >& FScreenShotManager::GetScreenshotResults()
{
	return ScreenShotDataArray;
}


/* IScreenShotManager interface
 *****************************************************************************/

void FScreenShotManager::GenerateLists()
{
	// Create the screen shot data
	CreateData();
}

TArray<TSharedPtr<FString> >& FScreenShotManager::GetCachedPlatfomList()
{
	return CachedPlatformList;
}

void FScreenShotManager::RegisterScreenShotUpdate(const FOnScreenFilterChanged& InDelegate )
{
	// Set the callback delegate
	ScreenFilterChangedDelegate = InDelegate;
}

void FScreenShotManager::SetFilter( TSharedPtr< ScreenShotFilterCollection > InFilter )
{
	// Update the UI
	ScreenFilterChangedDelegate.ExecuteIfBound();
}

/* IScreenShotManager event handlers
 *****************************************************************************/

TFuture<void> FScreenShotManager::CompareScreensotsAsync()
{
	return Async<void>(EAsyncExecution::Thread, [&] () { CompareScreensots(); });
}

TFuture<FImageComparisonResult> FScreenShotManager::CompareScreensotAsync(FString RelativeImagePath)
{
	return Async<FImageComparisonResult>(EAsyncExecution::Thread, [&] () { return CompareScreensot(RelativeImagePath); });
}

void FScreenShotManager::CompareScreensots()
{
	TSharedRef<FScreenshotComparisons> Comparisons = GenerateFileList();

	IFileManager::Get().DeleteDirectory(*ScreenshotDeltaFolder, /*RequireExists =*/false, /*Tree =*/true);

	FComparisonResults Results;
	Results.ApprovedPath = TEXT("Approved");
	Results.IncomingPath = TEXT("Incoming");
	Results.DeltaPath = TEXT("Delta");

	for ( const FString& New : Comparisons->New )
	{
		Results.Added.Add(New);
	}

	for ( const FString& Missing : Comparisons->Missing )
	{
		Results.Missing.Add(Missing);
	}

	for ( const FString& ExistingGroup : Comparisons->Existing )
	{
		FString TestUnapprovedFolder = FPaths::Combine(ScreenshotUnapprovedFolder, ExistingGroup);

		TArray<FString> UnapprovedDeviceShots;
		IFileManager::Get().FindFilesRecursive(UnapprovedDeviceShots, *TestUnapprovedFolder, TEXT("*.png"), true, false);

		check(UnapprovedDeviceShots.Num() > 0);

		for ( const FString& UnapprovedShot : UnapprovedDeviceShots )
		{
			FString RelativeUnapprovedShot = UnapprovedShot;
			FPaths::MakePathRelativeTo(RelativeUnapprovedShot, *ScreenshotUnapprovedFolder);

			const FImageComparisonResult ShotResult = CompareScreensot(RelativeUnapprovedShot);
			Results.Comparisons.Add(ShotResult);
		}
	}

	SaveComparisonResults(Results);
}

FImageComparisonResult FScreenShotManager::CompareScreensot(FString ExistingImage)
{
	FString Existing = FPaths::GetPath(ExistingImage);

	FImageComparer Comparer;
	Comparer.ImageRootA = ScreenshotApprovedFolder;
	Comparer.ImageRootB = ScreenshotUnapprovedFolder;
	Comparer.DeltaDirectory = ScreenshotDeltaFolder;

	// If the metadata for the screenshot does not provide tolerance rules, use these instead.
	FImageTolerance DefaultTolerance = FImageTolerance::DefaultIgnoreLess;
	DefaultTolerance.IgnoreAntiAliasing = true;

	FString TestApprovedFolder = FPaths::Combine(ScreenshotApprovedFolder, Existing);
	FString TestUnapprovedFolder =  FPaths::Combine(ScreenshotUnapprovedFolder, Existing);

	TArray<FString> ApprovedDeviceShots;
	IFileManager::Get().FindFilesRecursive(ApprovedDeviceShots, *TestApprovedFolder, TEXT("*.png"), true, false);

	if ( ApprovedDeviceShots.Num() == 0 )
	{
		return FImageComparisonResult();
	}

	// Look for an exact adapter match so we're comparing apples to apples.
	
	FString UnapprovedFileName = FPaths::GetCleanFilename(ExistingImage);

	FString* ExistingApprovedAdapter = ApprovedDeviceShots.FindByPredicate([&UnapprovedFileName] (const FString& ApprovedPath) {
		FString ApprovedFileName = FPaths::GetCleanFilename(ApprovedPath);
		return ApprovedFileName == UnapprovedFileName;
	});

	FString ExistingApproved;

	if ( ExistingApprovedAdapter )
	{
		ExistingApproved = FPaths::GetCleanFilename(*ExistingApprovedAdapter);
	}
	else
	{
		ExistingApproved = FPaths::GetCleanFilename(ApprovedDeviceShots[0]);
		// TODO No good, this should find closest resolution, maybe some other stuff as well?
	}

	FString ApprovedFullPath = FPaths::Combine(TestApprovedFolder, ExistingApproved);
	FString UnapprovedFullPath = FPaths::Combine(TestUnapprovedFolder, UnapprovedFileName);

	// Always read the metadata file from the unapproved location, as we may have introduced new comparison rules.
	FString MetadataFile = FPaths::ChangeExtension(UnapprovedFullPath, ".json");

	FImageTolerance Tolerance = DefaultTolerance;

	FString Json;
	if ( FFileHelper::LoadFileToString(Json, *MetadataFile) )
	{
		FAutomationScreenshotMetadata Metadata;
		if ( FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Metadata, 0, 0) )
		{
			if ( Metadata.bHasComparisonRules )
			{
				Tolerance.Red = Metadata.ToleranceRed;
				Tolerance.Green = Metadata.ToleranceGreen;
				Tolerance.Blue = Metadata.ToleranceBlue;
				Tolerance.Alpha = Metadata.ToleranceAlpha;
				Tolerance.MinBrightness = Metadata.ToleranceMinBrightness;
				Tolerance.MaxBrightness = Metadata.ToleranceMaxBrightness;
				Tolerance.IgnoreAntiAliasing = Metadata.bIgnoreAntiAliasing;
				Tolerance.IgnoreColors = Metadata.bIgnoreColors;
				Tolerance.MaximumLocalError = Metadata.MaximumLocalError;
				Tolerance.MaximumGlobalError = Metadata.MaximumGlobalError;
			}
		}
	}

	return Comparer.Compare(ApprovedFullPath, UnapprovedFullPath, Tolerance);
}

TFuture<void> FScreenShotManager::ExportScreensotsAsync(FString ExportPath)
{
	return Async<void>(EAsyncExecution::Thread, [&] () { ExportComparisonResults(ExportPath); });
}

bool FScreenShotManager::ExportComparisonResults(FString RootExportFolder)
{
	FPaths::NormalizeDirectoryName(RootExportFolder);

	if ( RootExportFolder.IsEmpty() )
	{
		RootExportFolder = GetDefaultExportDirectory();
	}

	IFileManager::Get().DeleteDirectory(*RootExportFolder, /*RequireExists =*/false, /*Tree =*/true);
	if ( !IFileManager::Get().MakeDirectory(*RootExportFolder, /*Tree =*/true) )
	{
		return false;
	}

	// Wait for file operations to complete.
	FPlatformProcess::Sleep(1.0f);

	FString ExportDirectory = RootExportFolder / FString::FromInt(FEngineVersion::Current().GetChangelist());
	FString ExportDirectory_ComparisonManifest = ExportDirectory / TEXT("ComparisonResults.json");
	FString ExportDirectory_Approved = ExportDirectory / TEXT("Approved");
	FString ExportDirectory_Incoming = ExportDirectory / TEXT("Incoming");
	FString ExportDirectory_Delta = ExportDirectory / TEXT("Delta");

	// Copy the comparison manifest
	IFileManager::Get().Copy(*ExportDirectory_ComparisonManifest, *GetComparisonResultsJsonFilename(), true, true);

	TArray<FString> FilesToCopy;

	// Copy the ground truth images
	CopyDirectory(ExportDirectory_Approved, ScreenshotApprovedFolder);

	// Copy the incoming unapproved images
	CopyDirectory(ExportDirectory_Incoming, ScreenshotUnapprovedFolder);

	// Copy the delta images
	CopyDirectory(ExportDirectory_Delta, ScreenshotDeltaFolder);

	return true;
}

TSharedPtr<FComparisonResults> FScreenShotManager::ImportScreensots(FString ImportPath)
{
	FString ManifestPath = ImportPath / TEXT("ComparisonResults.json");

	FString JsonString;

	if ( FFileHelper::LoadFileToString(JsonString, *ManifestPath) )
	{
		TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);

		TSharedPtr<FJsonObject> JsonComparisons;
		if ( !FJsonSerializer::Deserialize(JsonReader, JsonComparisons) )
		{
			return nullptr;
		}

		FComparisonResults* ImportedResults = new FComparisonResults();

		if ( FJsonObjectConverter::JsonObjectToUStruct(JsonComparisons.ToSharedRef(), ImportedResults, 0, 0) )
		{
			return MakeShareable(ImportedResults);
		}

		delete ImportedResults;
	}

	return nullptr;
}

FString FScreenShotManager::GetDefaultExportDirectory() const
{
	return FPaths::GameSavedDir() / TEXT("Exported");
}

FString FScreenShotManager::GetComparisonResultsJsonFilename() const
{
	return ScreenshotResultsFolder / TEXT("ComparisonResults.json");
}

void FScreenShotManager::SaveComparisonResults(const FComparisonResults& Results) const
{
	FString Json;
	if ( FJsonObjectConverter::UStructToJsonObjectString(Results, Json) )
	{
		FFileHelper::SaveStringToFile(Json, *GetComparisonResultsJsonFilename(), FFileHelper::EEncodingOptions::ForceUTF8);
	}
}

void FScreenShotManager::CopyDirectory(const FString& DestDir, const FString& SrcDir)
{
	TArray<FString> FilesToCopy;

	IFileManager::Get().FindFilesRecursive(FilesToCopy, *SrcDir, TEXT("*"), /*Files=*/true, /*Directories=*/true);
	for ( const FString& File : FilesToCopy )
	{
		const FString& SourceFilePath = File;
		FString DestFilePath = DestDir / SourceFilePath.RightChop(SrcDir.Len());

		IFileManager::Get().Copy(*DestFilePath, *File, true, true);
	}
}
