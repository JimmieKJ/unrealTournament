// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class FBuildMergeManifests
{
public:
	static bool MergeManifests(const FString& ManifestFilePathA, const FString& ManifestFilePathB, const FString& ManifestFilePathC, const FString& NewVersionString, const FString& SelectionDetailFilePath);
};
