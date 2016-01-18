// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Debug/GameplayDebuggerBaseObject.h"
#include "NavMeshGameplayDebuggerObject.generated.h"

UCLASS()
class UNavMeshGameplayDebuggerObject : public UGameplayDebuggerBaseObject
{
	GENERATED_UCLASS_BODY()

	virtual FString GetCategoryName() const override { return TEXT("NavMesh"); };
	virtual void CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor) override;
	virtual void DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor) override;
	virtual class FDebugRenderSceneProxy* CreateSceneProxy(const UPrimitiveComponent* InComponent, UWorld* World, AActor* SelectedActor) override;

	void ServerCollectNavmeshData(const AActor *SelectedActor);

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerDiscardNavmeshData();

protected:
	float LastDataCaptureTime;

	void PrepareNavMeshData(struct FNavMeshSceneProxyData*, const UPrimitiveComponent* InComponent, UWorld* World) const;

	virtual void OnDataReplicationComplited(const TArray<uint8>& ReplicatedData) override;

#if WITH_RECAST
	ARecastNavMesh* GetNavData(const AActor *SelectedActor);
#endif

};
