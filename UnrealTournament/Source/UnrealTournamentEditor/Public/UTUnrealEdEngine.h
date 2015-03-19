// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "Editor/UnrealEd/Classes/Editor/UnrealEdEngine.h"
#include "UTWorldSettings.h"
#include "UTMenuGameMode.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"

#include "UTUnrealEdEngine.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENTEDITOR_API UUTUnrealEdEngine : public UUnrealEdEngine
{
	GENERATED_UCLASS_BODY()

	UUTUnrealEdEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	UT_LOADMAP_DEFINITION()


	void StartDLCCookInEditor(const TArray<ITargetPlatform*> &TargetPlatforms, FString DLCName, FString BasedOnReleaseVersion)
	{
		UCookOnTheFlyServer::FCookByTheBookStartupOptions StartupOptions;
		StartupOptions.TargetPlatforms = TargetPlatforms;
		StartupOptions.DLCName = DLCName;
		StartupOptions.BasedOnReleaseVersion = BasedOnReleaseVersion;

		CookServer->ClearAllCookedData();
		CookServer->StartCookByTheBook(StartupOptions);
	}
};
