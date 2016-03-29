// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "BlueprintNativeCodeGenModule.h"
#include "Commandlets/NativeCodeGenCommandlet.h"

DEFINE_LOG_CATEGORY_STATIC(LogNativeCodeGenCommandletCommandlet, Log, All);

UNativeCodeGenCommandlet::UNativeCodeGenCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UNativeCodeGenCommandlet::Main(const FString& Params)
{
	TArray<FString> Platforms, Switches;
	ParseCommandLine(*Params, Platforms, Switches);

	if (Platforms.Num() == 0)
	{
		UE_LOG(LogNativeCodeGenCommandletCommandlet, Warning, TEXT("Missing platforms argument, should be -run=NativeCodeGen platform1 platform2, eg -run=NativeCodeGen windowsnoeditor"));
		return 0;
	}

	TArray< TPair< FString, FString > > CodeGenTargets;
	{
		for (auto& Platform : Platforms)
		{
			// If you change this target path you must also update logic in CookCommand.Automation.CS. Passing a single directory around is cumbersome for testing, so I have hard coded it.
			CodeGenTargets.Push(TPairInitializer<FString, FString>(Platform, FString(FPaths::Combine(*FPaths::GameIntermediateDir(), *Platform))));
		}
	}

	IBlueprintNativeCodeGenModule::InitializeModuleForRerunDebugOnly(CodeGenTargets);

	return 0;
}
