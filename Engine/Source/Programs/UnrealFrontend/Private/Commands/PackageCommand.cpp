// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealFrontendPrivatePCH.h"
#include "PackageCommand.h"
#include "TargetPlatform.h"


void FPackageCommand::Run( )
{
	FString SourceDir;
	FParse::Value(FCommandLine::Get(), TEXT("-SOURCEDIR="), SourceDir);

	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();

	if (TPM)
	{
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

		for (int32 Index = 0; Index < Platforms.Num(); ++Index)
		{
			if (Platforms[Index]->PackageBuild(SourceDir))
			{
			}
		}
	}
}
