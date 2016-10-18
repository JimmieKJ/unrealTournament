// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealNetwork.h"

#include "UTReplicatedEmitter.generated.h"

/** temporary one-shot emitter that is replicated to clients
 * this is used when we need to spawn an effect that can't be easily simulated or triggered on clients through a simple RepNotify
 * on dedicated servers, the effect will not play but the Actor should still be spawned and will stay alive long enough to send to current clients\
 * the emitter defaults to being based on its Owner, so that the Base can effectively be passed as part of the spawn parameters
 */
UCLASS(Blueprintable, Abstract, Meta=(ChildCanTick))
class UNREALTOURNAMENT_API AUTReplicatedEmitter : public AActor
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Emitter)
	UParticleSystemComponent* PSC;
	/** lifespan when in dedicated server mode - basically how long it's worthwhile to consider sending the effect to clients before it's obsolete */
	UPROPERTY(EditDefaultsOnly, Category = Emitter)
	float DedicatedServerLifeSpan;
	/** if set and emitter is given an Owner, find mesh component closest to that Actor's root and attach to it using BaseSocketName */
	UPROPERTY(EditDefaultsOnly, Category = Emitter)
	bool bAttachToOwnerMesh;
	/** the socket to attach to (if any) */
	UPROPERTY(EditDefaultsOnly, Category = Emitter)
	FName BaseSocketName;

	virtual void PostInitializeComponents() override
	{
		if (GetNetMode() == NM_DedicatedServer)
		{
			InitialLifeSpan = DedicatedServerLifeSpan;
		}
		Super::PostInitializeComponents();
	}

	virtual void RegisterAllComponents() override
	{
		// we do this via PostActorCreated() so we don't attach our component to ourselves just to reattach it later in spawning
	}

	virtual void PostActorCreated() override
	{
		// workaround for SpawnActor blueprint node not allowing setting Owner on spawn
		if (GetOwner() == NULL && Instigator != NULL)
		{
			SetOwner(Instigator);
		}

		if (GetOwner() != NULL && bAttachToOwnerMesh)
		{
			ACharacter* C = Cast<ACharacter>(GetOwner());			
			AttachToComponent((C != NULL) ? C->GetMesh() : GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale, BaseSocketName);
		}
		if (GetNetMode() != NM_DedicatedServer)
		{
			Super::RegisterAllComponents();
		}

		Super::PostActorCreated();
	}

	virtual void PreInitializeComponents() override
	{
		Super::PreInitializeComponents();

		// we can't do this above because if the blueprint does something with a BP or construction script component, the script would overwrite changes
		// this spot is after the construction script so should be OK
		if (RootComponent->GetAttachParent() != NULL)
		{
			OnAttachedTo(RootComponent->GetAttachParent());
		}
	}

	UFUNCTION()
	virtual void OnParticlesFinished(UParticleSystemComponent* FinishedPSC)
	{
		if (GetNetMode() != NM_DedicatedServer)
		{
			Destroy();
		}
	}

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic)
	void OnAttachedTo(USceneComponent* BaseComponent);

	virtual void OnRep_AttachmentReplication()
	{
		Super::OnRep_AttachmentReplication();

		if (RootComponent->GetAttachParent() != NULL)
		{
			OnAttachedTo(RootComponent->GetAttachParent());
		}
	}

};