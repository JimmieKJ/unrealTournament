// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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

	virtual void Init(IEngineLoop* InEngineLoop) override
	{
		Super::Init(InEngineLoop);

		// remove the priority of "ConsoleVariables.ini" from console variables because it prevents the UI from working
		IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda([](const TCHAR*, IConsoleObject* Obj) { IConsoleVariable* Var = Obj->AsVariable(); if (Var != NULL) { Var->ClearFlags(ECVF_SetByConsoleVariablesIni); } }), TEXT(""));

		FModuleManager::Get().LoadModule("BlueprintContext");
		FModuleManager::Get().LoadModule("BlueprintContextEditor");
	}

	virtual FString BuildPlayWorldURL(const TCHAR* MapName, bool bSpectatorMode, FString AdditionalURLOptions) override
	{
		FString URL = Super::BuildPlayWorldURL(MapName, bSpectatorMode, AdditionalURLOptions);

		if (!URL.Contains(TEXT("Taunt")) && PlayWorld != NULL) // PlayWorld is NULL when doing "play standalone game" or "play on XXX" in the editor, because they launch a separate process
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
