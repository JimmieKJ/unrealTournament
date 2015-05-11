// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UTCustomBotBlueprintLib.generated.h"

class AUTCustomBot;

UCLASS()
class UUTCustomBotBlueprintLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(Category="CustomBot", BlueprintCallable, meta = (DefaultToSelf = "WorldContextObject", HidePin = "WorldContextObject"))
	static AUTCustomBot* AddCustomBot(UObject* WorldContextObject, TSubclassOf<AUTCustomBot> BotClass, FString BotName = TEXT(""), uint8 TeamNum = 255);
};
