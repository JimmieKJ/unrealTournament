// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "EngineBuildSettings.h"


bool FEngineBuildSettings::IsInternalBuild()
{
	return FPaths::FileExists( FPaths::EngineDir() / TEXT("Build/NotForLicensees/EpicInternal.txt") );
}


bool FEngineBuildSettings::IsPerforceBuild()
{
	return FPaths::FileExists( FPaths::EngineDir() / TEXT("Build/PerforceBuild.txt") );
}


bool FEngineBuildSettings::IsSourceDistribution()
{
	return IsSourceDistribution( FPaths::RootDir() );
}


bool FEngineBuildSettings::IsSourceDistribution(const FString& RootDir)
{
	return FPaths::FileExists( RootDir / TEXT("Engine/Build/SourceDistribution.txt") );
}
