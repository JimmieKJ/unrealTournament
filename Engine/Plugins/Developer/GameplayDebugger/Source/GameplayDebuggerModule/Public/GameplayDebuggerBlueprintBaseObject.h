// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Debug/GameplayDebuggerBaseObject.h"
#include "GameplayDebuggerBlueprintBaseObject.generated.h"

UCLASS(Blueprintable, Transient, Abstract, EarlyAccessPreview)
class UGameplayDebuggerBlueprintBaseObject : public UGameplayDebuggerBaseObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "GameplayDebugger", meta = (DisplayName = "CategoryName"))
	FString GameplayDebuggerCategoryName;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameplayDebugger", meta = (DisplayName = "CollectDataToReplicateOnServer"))
	void CollectDataToReplicateOnServerEvent(APlayerController* PlayerController, AActor *SelectedActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameplayDebugger", meta = (DisplayName = "DrawCollectedDataOnClient"))
	void DrawCollectedDataEvent(APlayerController* PlayerController, AActor* SelectedActor = nullptr);

	virtual FString GetCategoryName() const override;

	virtual void DrawCollectedData(APlayerController* PlayerController, AActor* SelectedActor = nullptr) override;

#if WITH_EDITOR
	// Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void CollectDataToReplicate(APlayerController* PlayerController, AActor *SelectedActor) override { CollectDataToReplicateOnServerEvent(PlayerController, SelectedActor); }
};
