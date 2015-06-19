// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameplayAbility.h"
#include "Abilities/GameplayAbilityWorldReticle.h"
#include "GameplayAbilityTargetActor.generated.h"

UENUM(BlueprintType)
namespace ETargetAbilitySelfSelection
{
	enum Type
	{
		TASS_Permit			UMETA(DisplayName = "Allow self-selection"),
		TASS_Forbid			UMETA(DisplayName = "Forbid self-selection"),
		TASS_Require 		UMETA(DisplayName = "Force self-selection (add to final data)")
	};
}

/** TargetActors are spawned to assist with ability targeting. They are spawned by ability tasks and create/determine the outgoing targeting data passed from one task to another. */
UCLASS(Blueprintable, abstract, notplaceable)
class GAMEPLAYABILITIES_API AGameplayAbilityTargetActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	void Destroyed() override;

	/** The TargetData this class produces can be entirely generated on the server. We don't require the client to send us full or partial TargetData (possibly just a 'confirm') */
	UPROPERTY(EditAnywhere, Category=Advanced)
	bool ShouldProduceTargetDataOnServer;

	/** Describes where the targeting action starts, usually the player character or a socket on the player character. */
	//UPROPERTY(BlueprintReadOnly, meta=(ExposeOnSpawn=true), Category=Targeting)
	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true), Replicated, Category = Targeting)
	FGameplayAbilityTargetingLocationInfo StartLocation;
	
	/** Initialize and begin targeting logic  */
	virtual void StartTargeting(UGameplayAbility* Ability);

	virtual bool IsConfirmTargetingAllowed();

	/** Requesting targeting data, but not necessarily stopping/destroying the task. Useful for external target data requests. */
	virtual void ConfirmTargetingAndContinue();

	/** Outside code is saying 'stop and just give me what you have.' Returns true if the ability accepts this and can be forgotten. */
	UFUNCTION()
	virtual void ConfirmTargeting();

	/** Outside code is saying 'stop everything and just forget about it' */
	UFUNCTION()
	virtual void CancelTargeting();

	virtual void BindToConfirmCancelInputs();

	virtual bool ShouldProduceTargetData() const;

	/** Replicated target data was received from a client. Possibly sanitize/verify. return true if data is good and we should broadcast it as valid data. */
	virtual bool OnReplicatedTargetDataReceived(FGameplayAbilityTargetDataHandle& Data) const;

	/** Accessor for checking, before instantiating, if this TargetActor will replicate. */
	bool GetReplicates() const;

	// ------------------------------
	
	FAbilityTargetData	TargetDataReadyDelegate;
	FAbilityTargetData	CanceledDelegate;

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	APlayerController* MasterPC;

	UPROPERTY()
	UGameplayAbility* OwningAbility;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = Targeting)
	bool bDestroyOnConfirmation;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = Targeting)
	AActor* SourceActor;

	/** Parameters for world reticle. Usage of these parameters is dependent on the reticle. */
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = Targeting)
	FWorldReticleParameters ReticleParams;

	/** Reticle that will appear on top of acquired targets. Reticles will be spawned/despawned as targets are acquired/lost. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
	TSubclassOf<AGameplayAbilityWorldReticle> ReticleClass;		//Using a special class for replication purposes.

	UPROPERTY(BlueprintReadWrite, Replicated, meta = (ExposeOnSpawn = true), Category = Targeting)
	FGameplayTargetDataFilterHandle Filter;

	/** Draw the debug information (if applicable) for this targeting actor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, meta = (ExposeOnSpawn = true), Category = Targeting)
	bool bDebug;

	FDelegateHandle GenericConfirmHandle;
	FDelegateHandle GenericCancelHandle;
};