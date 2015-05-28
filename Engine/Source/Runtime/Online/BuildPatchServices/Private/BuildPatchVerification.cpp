// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchServicesPrivatePCH.h"

class FBuildPatchVerificationImpl : public FBuildPatchVerification
{
public:
	FBuildPatchVerificationImpl(const FBuildPatchAppManifestRef& Manifest, const FBuildPatchFloatDelegate& ProgressDelegate, const FBuildPatchBoolRetDelegate& ShouldPauseDelegate, const FString& VerifyDirectory, const FString& StagedFileDirectory, bool bUseStageDirectory);

	virtual ~FBuildPatchVerificationImpl(){}

	virtual bool VerifyAgainstDirectory(TArray<FString>& OutDatedFiles, double& TimeSpentPaused) override;

private:
	FBuildPatchAppManifestRef Manifest;
	FBuildPatchFloatDelegate ProgressDelegate;
	FBuildPatchBoolRetDelegate ShouldPauseDelegate;
	const FString VerifyDirectory;
	const FString StagedFileDirectory;
	const bool bUseStageDirectory;
	double CurrentBuildPercentage;
	double CurrentFileWeight;

	void PerFileProgress(float Progress);
	FString SelectFullFilePath(const FString& BuildFile);
};

FBuildPatchVerificationImpl::FBuildPatchVerificationImpl(const FBuildPatchAppManifestRef& InManifest, const FBuildPatchFloatDelegate& InProgressDelegate, const FBuildPatchBoolRetDelegate& InShouldPauseDelegate, const FString& InVerifyDirectory, const FString& InStagedFileDirectory, bool bInUseStageDirectory)
	: Manifest(InManifest)
	, ProgressDelegate(InProgressDelegate)
	, ShouldPauseDelegate(InShouldPauseDelegate)
	, VerifyDirectory(InVerifyDirectory)
	, StagedFileDirectory(InStagedFileDirectory)
	, bUseStageDirectory(bInUseStageDirectory)
	, CurrentBuildPercentage(0)
	, CurrentFileWeight(0)
{}

bool FBuildPatchVerificationImpl::VerifyAgainstDirectory(TArray<FString>& OutDatedFiles, double& TimeSpentPaused)
{
	bool bAllCorrect = true;
	OutDatedFiles.Empty();
	TimeSpentPaused = 0;

	// Setup progress tracking
	double TotalBuildSizeDouble = Manifest->GetBuildSize();
	double ProcessedBytes = 0;
	CurrentBuildPercentage = 0;

	// For all files in the manifest, check that they produce the correct SHA1 hash, adding any that don't to the list
	TArray<FString> BuildFiles;
	Manifest->GetFileList(BuildFiles);
	for (const FString& BuildFile : BuildFiles)
	{
		// Get file details
		int64 BuildFileSize = Manifest->GetFileSize(BuildFile);
		FSHAHashData BuildFileHash;
		bool bFoundHash = Manifest->GetFileHash(BuildFile, BuildFileHash);
		check(bFoundHash);

		// Chose the file to check
		FString FullFilename = SelectFullFilePath(BuildFile);

		// Verify the file
		CurrentFileWeight = BuildFileSize / TotalBuildSizeDouble;
		if (FBuildPatchUtils::VerifyFile(FullFilename, BuildFileHash, BuildFileHash, FBuildPatchFloatDelegate::CreateRaw(this, &FBuildPatchVerificationImpl::PerFileProgress), ShouldPauseDelegate, TimeSpentPaused) == 0)
		{
			bAllCorrect = false;
			OutDatedFiles.Add(BuildFile);
		}
		ProcessedBytes += BuildFileSize;
		CurrentBuildPercentage = ProcessedBytes / TotalBuildSizeDouble;
	}

	return bAllCorrect && !FBuildPatchInstallError::HasFatalError();
}

void FBuildPatchVerificationImpl::PerFileProgress(float Progress)
{
	ProgressDelegate.ExecuteIfBound(CurrentBuildPercentage + (Progress * CurrentFileWeight));
}

FString FBuildPatchVerificationImpl::SelectFullFilePath(const FString& BuildFile)
{
	if (bUseStageDirectory)
	{
		FString StagedFilename = StagedFileDirectory / BuildFile;
		if (IFileManager::Get().FileSize(*StagedFilename) != -1)
		{
			return MoveTemp(StagedFilename);
		}
	}
	FString InstallFilename = VerifyDirectory / BuildFile;
	return MoveTemp(InstallFilename);
}

TSharedRef<FBuildPatchVerification> FBuildPatchVerificationFactory::Create(const FBuildPatchAppManifestRef& Manifest, const FBuildPatchFloatDelegate& ProgressDelegate, const FBuildPatchBoolRetDelegate& ShouldPauseDelegate, const FString& VerifyDirectory, const FString& StagedFileDirectory)
{
	return MakeShareable(new FBuildPatchVerificationImpl(Manifest, ProgressDelegate, ShouldPauseDelegate, VerifyDirectory, StagedFileDirectory, !StagedFileDirectory.IsEmpty()));
}
