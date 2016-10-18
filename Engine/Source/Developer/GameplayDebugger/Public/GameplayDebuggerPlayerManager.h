// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "GameplayDebuggerPlayerManager.generated.h"

class AGameplayDebuggerCategoryReplicator;
class UGameplayDebuggerLocalController;

USTRUCT()
struct FGameplayDebuggerPlayerData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UGameplayDebuggerLocalController* Controller;

	UPROPERTY()
	UInputComponent* InputComponent;

	UPROPERTY()
	AGameplayDebuggerCategoryReplicator* Replicator;
};

UCLASS(NotBlueprintable, NotBlueprintType, notplaceable, noteditinlinenew, hidedropdown, Transient)
class AGameplayDebuggerPlayerManager : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
		
	void UpdateAuthReplicators();
	void RegisterReplicator(AGameplayDebuggerCategoryReplicator& Replicator);
	void RefreshInputBindings(AGameplayDebuggerCategoryReplicator& Replicator);

	AGameplayDebuggerCategoryReplicator* GetReplicator(const APlayerController& OwnerPC) const;
	UInputComponent* GetInputComponent(const APlayerController& OwnerPC) const;
	
	static AGameplayDebuggerPlayerManager& GetCurrent(UWorld* World);

protected:

	UPROPERTY()
	TArray<FGameplayDebuggerPlayerData> PlayerData;

	UPROPERTY()
	TArray<AGameplayDebuggerCategoryReplicator*> PendingRegistrations;

	uint32 bHasAuthority : 1;
	uint32 bIsLocal : 1;
	uint32 bInitialized : 1;
};
