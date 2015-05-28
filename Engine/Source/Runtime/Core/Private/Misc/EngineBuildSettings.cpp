// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "EngineBuildSettings.h"


bool FEngineBuildSettings::IsInternalBuild()
{
	static bool bIsInternalBuild = FPaths::FileExists( FPaths::EngineDir() / TEXT("Build/NotForLicensees/EpicInternal.txt") );
	return bIsInternalBuild;
}


bool FEngineBuildSettings::IsPerforceBuild()
{
	static bool bIsPerforceBuild = FPaths::FileExists(FPaths::EngineDir() / TEXT("Build/PerforceBuild.txt"));
	return bIsPerforceBuild;
}


bool FEngineBuildSettings::IsSourceDistribution()
{
	return IsSourceDistribution( FPaths::RootDir() );
}


bool FEngineBuildSettings::IsSourceDistribution(const FString& RootDir)
{
	static bool bIsSourceDistribution = FPaths::FileExists(RootDir / TEXT("Engine/Build/SourceDistribution.txt"));
	return bIsSourceDistribution;
}
