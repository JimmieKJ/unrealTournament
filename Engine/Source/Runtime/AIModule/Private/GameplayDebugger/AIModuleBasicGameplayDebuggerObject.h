// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Debug/GameplayDebuggerBaseObject.h"
#include "AIModuleBasicGameplayDebuggerObject.generated.h"

UCLASS()
class UAIModuleBasicGameplayDebuggerObject : public UGameplayDebuggerBaseObject
{
	GENERATED_UCLASS_BODY()

protected:
	struct FPathCorridorPolygons
	{
		TArray<FVector> Points;
		FColor Color;
	};
	
	UPROPERTY(Replicated)
	FString ControllerName;

	UPROPERTY(Replicated)
	FString PawnName;

	UPROPERTY(Replicated)
	FString PawnClass;

	UPROPERTY(Replicated)
	FString DebugIcon;

	UPROPERTY(Replicated)
	FString MovementBaseInfo;

	UPROPERTY(Replicated)
	FString MovementModeInfo;

	UPROPERTY(Replicated)
	FString PathFollowingInfo;

	UPROPERTY(Replicated)
	FString CurrentAITask;

	UPROPERTY(Replicated)
	FString CurrentAIState;

	UPROPERTY(Replicated)
	FString CurrentAIAssets;

	UPROPERTY(Replicated)
	FString GameplayTasksState;

	UPROPERTY(Replicated)
	FString NavDataInfo;

	UPROPERTY(Replicated)
	FString MontageInfo;

	UPROPERTY(Replicated)
	uint32 bIsUsingPathFollowing : 1;

	UPROPERTY(Replicated)
	uint32 bIsUsingCharacter : 1;

	UPROPERTY(Replicated)
	uint32 bIsUsingBehaviorTree : 1;

	/** Begin path replication data */
	UPROPERTY(Replicated)
	int32 NextPathPointIndex;

	UPROPERTY(Replicated)
	TArray<FVector> PathPoints;

	TArray<uint8> PathCorridorData;

	TArray<FPathCorridorPolygons> PathCorridorPolygons;
	/** End path replication data*/


public:
	virtual void CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor) override;

	virtual void DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor) override;

	FDebugRenderSceneProxy* CreateSceneProxy(const UPrimitiveComponent* InComponent, UWorld* World, AActor* SelectedActor) override;

protected:
	FNavPathWeakPtr CurrentPath;
	float LastStoredPathTimeStamp;

	void CollectBasicMovementData(APawn* MyPawn);
	void CollectBasicPathData(APawn* MyPawn);
	void CollectBasicBehaviorData(APawn* MyPawn);
	void CollectPathData(APawn* MyPawn);
	void CollectBasicAnimationData(APawn* MyPawn);

	void DrawBasicData(UGameplayDebuggerBaseObject::FPrintContext& DefaultContext, UGameplayDebuggerBaseObject::FPrintContext& OverHeadContext);
	void DrawOverHeadInformation(UGameplayDebuggerBaseObject::FPrintContext& DefaultContext, UGameplayDebuggerBaseObject::FPrintContext& OverHeadContext, FVector SelectedActorLoc);
	void DrawPath();

	void OnDataReplicationComplited(const TArray<uint8>& ReplicatedData) override;
};
