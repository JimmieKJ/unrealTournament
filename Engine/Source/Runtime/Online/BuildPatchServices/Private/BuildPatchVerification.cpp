// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchVerification.h"
#include "HAL/FileManager.h"
#include "BuildPatchError.h"
#include "BuildPatchUtil.h"

class FBuildPatchVerificationImpl : public FBuildPatchVerification
{
public:
	FBuildPatchVerificationImpl(EVerifyMode InVerifyMode, TSet<FString> InTouchedFiles, TSet<FString> InInstallTags, FBuildPatchAppManifestRef InManifest, FString InVerifyDirectory, FString InStagedFileDirectory, const FBuildPatchFloatDelegate& InProgressDelegate, const FBuildPatchBoolRetDelegate& InShouldPauseDelegate);

	virtual ~FBuildPatchVerificationImpl(){}

	virtual bool VerifyAgainstDirectory(TArray<FString>& OutDatedFiles, double& TimeSpentPaused) override;

private:
	EVerifyMode VerifyMode;
	TSet<FString> RequiredFiles;
	TSet<FString> InstallTags;
	FBuildPatchAppManifestRef Manifest;
	const FString VerifyDirectory;
	const FString StagedFileDirectory;
	FBuildPatchFloatDelegate ProgressDelegate;
	FBuildPatchBoolRetDelegate ShouldPauseDelegate;
	double CurrentBuildPercentage;
	double CurrentFileWeight;

private:
	void PerFileProgress(float Progress);
	FString SelectFullFilePath(const FString& BuildFile);
	bool VerfiyFileSha(const FString& BuildFile, double& TimeSpentPaused);
	bool VerfiyFileSize(const FString& BuildFile, double& TimeSpentPaused);
};

FBuildPatchVerificationImpl::FBuildPatchVerificationImpl(EVerifyMode InVerifyMode, TSet<FString> InTouchedFiles, TSet<FString> InInstallTags, FBuildPatchAppManifestRef InManifest, FString InVerifyDirectory, FString InStagedFileDirectory, const FBuildPatchFloatDelegate& InProgressDelegate, const FBuildPatchBoolRetDelegate& InShouldPauseDelegate)
	: VerifyMode(InVerifyMode)
	, RequiredFiles(MoveTemp(InTouchedFiles))
	, InstallTags(MoveTemp(InInstallTags))
	, Manifest(MoveTemp(InManifest))
	, VerifyDirectory(MoveTemp(InVerifyDirectory))
	, StagedFileDirectory(MoveTemp(InStagedFileDirectory))
	, ProgressDelegate(InProgressDelegate)
	, ShouldPauseDelegate(InShouldPauseDelegate)
	, CurrentBuildPercentage(0)
	, CurrentFileWeight(0)
{}

bool FBuildPatchVerificationImpl::VerifyAgainstDirectory(TArray<FString>& OutDatedFiles, double& TimeSpentPaused)
{
	bool bAllCorrect = true;
	OutDatedFiles.Empty();
	TimeSpentPaused = 0;
	if (VerifyMode == EVerifyMode::FileSizeCheckAllFiles || VerifyMode == EVerifyMode::ShaVerifyAllFiles)
	{
		Manifest->GetTaggedFileList(InstallTags, RequiredFiles);
	}

	// Setup progress tracking
	double TotalBuildSizeDouble = Manifest->GetFileSize(RequiredFiles);
	double ProcessedBytes = 0;
	CurrentBuildPercentage = 0;

	// Select verify function
	bool bVerifySha = VerifyMode == EVerifyMode::ShaVerifyAllFiles || VerifyMode == EVerifyMode::ShaVerifyTouchedFiles;

	// For all files in the manifest, check that they produce the correct SHA1 hash, adding any that don't to the list
	for (const FString& BuildFile : RequiredFiles)
	{
		// Break if quitting
		if (FBuildPatchInstallError::HasFatalError())
		{
			break;
		}

		// Get file details
		int64 BuildFileSize = Manifest->GetFileSize(BuildFile);

		// Verify the file
		CurrentFileWeight = BuildFileSize / TotalBuildSizeDouble;
		bool bFileOk = bVerifySha ? VerfiyFileSha(BuildFile, TimeSpentPaused) : VerfiyFileSize(BuildFile, TimeSpentPaused);
		if (bFileOk == false)
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
	FString FullFilePath;
	if (StagedFileDirectory.IsEmpty() == false)
	{
		FullFilePath = StagedFileDirectory / BuildFile;
		if (IFileManager::Get().FileSize(*FullFilePath) != -1)
		{
			return FullFilePath;
		}
	}
	FullFilePath = VerifyDirectory / BuildFile;
	return FullFilePath;
}

bool FBuildPatchVerificationImpl::VerfiyFileSha(const FString& BuildFile, double& TimeSpentPaused)
{
	FSHAHashData BuildFileHash;
	bool bFoundHash = Manifest->GetFileHash(BuildFile, BuildFileHash);
	checkf(bFoundHash, TEXT("Missing file hash from manifest."))
	return FBuildPatchUtils::VerifyFile(
		SelectFullFilePath(BuildFile),
		BuildFileHash,
		BuildFileHash,
		FBuildPatchFloatDelegate::CreateRaw(this, &FBuildPatchVerificationImpl::PerFileProgress),
		ShouldPauseDelegate,
		TimeSpentPaused) != 0;
}

bool FBuildPatchVerificationImpl::VerfiyFileSize(const FString& BuildFile, double& TimeSpentPaused)
{
	// Pause if necessary
	const double PrePauseTime = FPlatformTime::Seconds();
	double PostPauseTime = PrePauseTime;
	bool bShouldPause = ShouldPauseDelegate.IsBound() && ShouldPauseDelegate.Execute();
	while (bShouldPause && !FBuildPatchInstallError::HasFatalError())
	{
		FPlatformProcess::Sleep(0.1f);
		bShouldPause = ShouldPauseDelegate.Execute();
		PostPauseTime = FPlatformTime::Seconds();
	}
	// Count up pause time
	TimeSpentPaused += PostPauseTime - PrePauseTime;
	PerFileProgress(0.0f);
	int64 FileSize = IFileManager::Get().FileSize(*SelectFullFilePath(BuildFile));
	PerFileProgress(1.0f);
	return FileSize == Manifest->GetFileSize(BuildFile);
}

TSharedRef<FBuildPatchVerification> FBuildPatchVerificationFactory::Create(EVerifyMode VerifyMode, TSet<FString> TouchedFiles, TSet<FString> InstallTags, FBuildPatchAppManifestRef Manifest, FString VerifyDirectory, FString StagedFileDirectory, const FBuildPatchFloatDelegate& ProgressDelegate, const FBuildPatchBoolRetDelegate& ShouldPauseDelegate)
{
	return MakeShareable(new FBuildPatchVerificationImpl(VerifyMode, TouchedFiles, InstallTags, Manifest, VerifyDirectory, StagedFileDirectory, ProgressDelegate, ShouldPauseDelegate));
}
