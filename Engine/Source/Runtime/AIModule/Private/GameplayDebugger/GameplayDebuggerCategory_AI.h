// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#if WITH_GAMEPLAY_DEBUGGER

#pragma once
#include "GameplayDebuggerCategory.h"

class FGameplayDebuggerCategory_AI : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_AI();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void OnDataPackReplicated(int32 DataPackId) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;
	virtual FDebugRenderSceneProxy* CreateSceneProxy(const UPrimitiveComponent* InComponent) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	void CollectPathData(AAIController* DebugAI);
	void DrawPath(UWorld* World);
	void DrawPawnIcons(UWorld* World, AActor* DebugActor, APawn* SkipPawn, FGameplayDebuggerCanvasContext& CanvasContext);
	void DrawOverheadInfo(AActor& DebugActor, FGameplayDebuggerCanvasContext& CanvasContext);

	struct FRepData
	{
		FString ControllerName;
		FString PawnName;
		FString MovementBaseInfo;
		FString MovementModeInfo;
		FString PathFollowingInfo;
		int32 NextPathPointIndex;
		FString CurrentAITask;
		FString CurrentAIState;
		FString CurrentAIAssets;
		FString NavDataInfo;
		FString MontageInfo;
		uint32 bHasController : 1;
		uint32 bIsUsingPathFollowing : 1;
		uint32 bIsUsingCharacter : 1;
		uint32 bIsUsingBehaviorTree : 1;

		FRepData() : NextPathPointIndex(0), bHasController(false), bIsUsingPathFollowing(false), bIsUsingCharacter(false), bIsUsingBehaviorTree(false) {}
		void Serialize(FArchive& Ar);
	};
	FRepData DataPack;

	struct FRepDataPath
	{
		struct FPoly
		{
			TArray<FVector> Points;
			FColor Color;
		};

		TArray<FPoly> PathCorridor;
		TArray<FVector> PathPoints;

		void Serialize(FArchive& Ar);
	};	
	FRepDataPath PathDataPack;
	int32 PathDataPackId;

private:
	// do NOT read from this pointer, it's only for validating if path has changed and can be invalid!
	FNavigationPath* RawLastPath;
};

#endif // WITH_GAMEPLAY_DEBUGGER
