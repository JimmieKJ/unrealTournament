// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Debug/GameplayDebuggerBaseObject.h"
#include "Debug/DebugDrawService.h"
#include "PerceptionGameplayDebuggerObject.generated.h"

UCLASS()
class UPerceptionGameplayDebuggerObject : public UGameplayDebuggerBaseObject
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(Replicated)
	FVector SensingComponentLocation;

	UPROPERTY(Replicated)
	FString PerceptionLegend;

	UPROPERTY(Replicated)
	float DistanceFromPlayer;

	UPROPERTY(Replicated)
	float DistanceFromSensor;

	float LastDataCaptureTime;

public:
	virtual FString GetCategoryName() const override { return TEXT("Perception"); };
	virtual void CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor) override;
	virtual void DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor) override;
};
