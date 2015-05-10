// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CustomBotPrivatePCH.h"
#include "UTPlayerState.h"
#include "UTCustomBotBlueprintLib.h"

AUTCustomBot* UUTCustomBotBlueprintLib::AddCustomBot(UObject* WorldContextObject, TSubclassOf<AUTCustomBot> BotClass, FString BotName, uint8 TeamNum)
{
	UWorld* World = WorldContextObject->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}	

	AUTCustomBot* NewBot = World->SpawnActor<AUTCustomBot>(BotClass);
	if (NewBot != nullptr)
	{
		check(NewBot->PlayerState);
		
		if (BotName.IsEmpty())
		{
			static int32 NameCount = 0;
			BotName = FString(TEXT("CustomBot")) + ((NameCount > 0) ? FString::Printf(TEXT("_%i"), NameCount) : TEXT(""));
			++NameCount;
		}
		NewBot->PlayerState->SetPlayerName(BotName);

		AUTPlayerState* PS = Cast<AUTPlayerState>(NewBot->PlayerState);
		if (PS != nullptr)
		{
			PS->bReadyToPlay = true;
		}

		AUTGameMode* GameMode = World->GetAuthGameMode<AUTGameMode>();

		if (GameMode)
		{
			GameMode->BotFillCount++;
			NewBot->InitializeSkill(GameMode->GameDifficulty);
			GameMode->NumBots++;
			GameMode->ChangeTeam(NewBot, TeamNum);
			GameMode->GenericPlayerInitialization(NewBot);
		}
	}
	return NewBot;
}
