// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class FBuildDataEnumeration
{
public:
	static bool EnumerateManifestData(const FString& ManifestFilePath, const FString& OutputFile, bool bIncludeSizes);
};
