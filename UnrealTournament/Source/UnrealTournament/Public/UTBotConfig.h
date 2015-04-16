// contains bot character settings (skill modifiers, etc)
// this is a separate object so it can be saved and loaded independently from game settings
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBotConfig.generated.h"

UCLASS(NotBlueprintable, Config = Game)
class UNREALTOURNAMENT_API UUTBotConfig : public UObject
{
	GENERATED_BODY()
public:
	/** list of bot characters that may be added */
	UPROPERTY(GlobalConfig)
	TArray<struct FBotCharacter> BotCharacters;

	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();
		// make sure selection count didn't get saved
		for (FBotCharacter& Bot : BotCharacters)
		{
			Bot.SelectCount = 0;
		}
	}

	// currently we just save/load to .ini file, but these accessors added in case we change that (to profile, for example)
	void Save()
	{
		SaveConfig();
	}
	void Load()
	{
		LoadConfig();
	}
};