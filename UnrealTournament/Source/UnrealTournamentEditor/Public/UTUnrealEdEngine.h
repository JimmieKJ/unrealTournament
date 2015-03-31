// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "Editor/UnrealEd/Classes/Editor/UnrealEdEngine.h"
#include "UTWorldSettings.h"
#include "UTMenuGameMode.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "UTLocalPlayer.h"

#include "UTUnrealEdEngine.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENTEDITOR_API UUTUnrealEdEngine : public UUnrealEdEngine
{
	GENERATED_UCLASS_BODY()

	UUTUnrealEdEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	UT_LOADMAP_DEFINITION()

	virtual FString BuildPlayWorldURL(const TCHAR* MapName, bool bSpectatorMode, FString AdditionalURLOptions) override
	{
		FString URL = Super::BuildPlayWorldURL(MapName, bSpectatorMode, AdditionalURLOptions);

		if (!URL.Contains(TEXT("Taunt")))
		{
			UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(GEngine->GetFirstGamePlayer(PlayWorld));
			if (LP)
			{
				URL += TEXT("?Taunt=");
				URL += LP->GetDefaultURLOption(TEXT("Taunt"));
			}
		}

		return URL;
	}

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
