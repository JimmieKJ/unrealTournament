// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_BUILDPATCHGENERATION

class FBuildDataEnumeration
{
public:
	static bool EnumerateManifestData(FString ManifestFilePath, FString OutputFile);
};

#endif //WITH_BUILDPATCHGENERATION
