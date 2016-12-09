// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BuildPatchManifest.h"
#include "Interfaces/IBuildPatchServicesModule.h"

// A delegate returning a bool. Used to pass a paused state
DECLARE_DELEGATE_RetVal(bool, FBuildPatchBoolRetDelegate);

class FBuildPatchVerification
{
public:
	/**
	 * Verifies a local directory structure against a given manifest.
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param OutDatedFiles    OUT  The array of files that do not match or are locally missing.
	 * @param TimeSpentPaused  OUT  The amount of time we were paused for, in seconds.
	 * @return    true if no file errors occurred AND the verification was successful
	 */
	virtual bool VerifyAgainstDirectory(TArray<FString>& OutDatedFiles, double& TimeSpentPaused) = 0;
};

class FBuildPatchVerificationFactory
{
public:
	/**
	 * Creates a verification class that will verify a local directory structure against a given manifest, optionally 
	 * taking account of a staging directory where alternative files are used.
	 * NOTE: This function is blocking and will not return until finished. Don't run on a UI thread.
	 * @param VerifyMode            The verify mode to run.
	 * @param TouchedFiles          The set of files that were touched by the installation.
	 * @param InstallTags           The install tags, will be used when verifying all files.
	 * @param Manifest              The manifest describing the build data.
	 * @param VerifyDirectory       The directory to analyze.
	 * @param StagedFileDirectory   A stage directory for updated files, ignored if empty string. If a file exists here, it will be checked instead of the one in VerifyDirectory.
	 * @param ProgressDelegate      Delegate to receive progress updates in the form of a float range 0.0f to 1.0f.
	 * @param ShouldPauseDelegate   Delegate that returns a bool, which if true will pause the process.
	 * @return     Ref of an object that can be used to perform the operation.
	 */
	static TSharedRef<FBuildPatchVerification> Create(
		EVerifyMode VerifyMode,
		TSet<FString> TouchedFiles,
		TSet<FString> InstallTags,
		FBuildPatchAppManifestRef Manifest,
		FString VerifyDirectory,
		FString StagedFileDirectory,
		const FBuildPatchFloatDelegate& ProgressDelegate,
		const FBuildPatchBoolRetDelegate& ShouldPauseDelegate);
};
