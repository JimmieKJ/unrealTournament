// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Debug/GameplayDebuggerBaseObject.h"
#include "AbilitiesGameplayDebuggerObject.generated.h"

UCLASS()
class UAbilitiesGameplayDebuggerObject : public UGameplayDebuggerBaseObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(Replicated)
	FString AbilityInfo;

	UPROPERTY(Replicated)
	uint32 bIsUsingAbilities : 1;

	virtual void CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor) override;

	virtual void DrawCollectedData(APlayerController* MyPC, AActor* SelectedActor) override;

};
