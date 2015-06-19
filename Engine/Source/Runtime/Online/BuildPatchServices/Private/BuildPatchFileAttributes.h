// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FBuildPatchFileAttributes
{
public:
	/**
	 * Applies the attributes to the files in the Staging directory, or installation directory. A force param can be set true in the
	 * case that all attributes should be set regardless of changing since previous build. This is helpful for fixing file attributes
	 * when they are suspected incorrect.
	 * @param bForce   Default: False. Pass in true to always set up attributes even if patch does not change them.
	 */
	virtual bool ApplyAttributes(bool bForce = false) = 0;
};

class FBuildPatchFileAttributesFactory
{
public:
	/**
	 * Creates a file attribute class that can be used to setup file attributes that are contained in the build manifest, optionally
	 * taking account of a staging directory where alternative files are.
	 * @param NewManifest           The manifest describing the build data.
	 * @param OldManifest           The manifest describing the build data.
	 * @param VerifyDirectory       The directory to analyze.
	 * @param StagedFileDirectory   A stage directory for updated files, ignored if empty string. If a file exists here, it will be
	 *                              checked instead of the one in VerifyDirectory.
	 * @return     Ref of an object that can be used to perform the operation.
	 */
	static TSharedRef<FBuildPatchFileAttributes> Create(const FBuildPatchAppManifestRef& NewManifest,
	                                                    const FBuildPatchAppManifestPtr& OldManifest,
	                                                    const FString& VerifyDirectory,
	                                                    const FString& StagedFileDirectory);
};
