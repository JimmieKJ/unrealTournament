// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Debug/GameplayDebuggerBaseObject.h"
#include "BTGameplayDebuggerObject.generated.h"

UCLASS()
class UBTGameplayDebuggerObject : public UGameplayDebuggerBaseObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Replicated)
	FString BrainComponentName;

	UPROPERTY(Replicated)
	FString BrainComponentString;

	FString BlackboardString;

	virtual FString GetCategoryName() const override { return TEXT("BehaviorTree"); };
	virtual void CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor) override;
	virtual void DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor) override;
	virtual void OnDataReplicationComplited(const TArray<uint8>& ReplicatedData) override;
};
