// creates a short duration shootable particle of a given character (for teleports, i.e. translocator)
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponRedirector.h"

#include "UTAfterImageEffect.generated.h"

UCLASS(Blueprintable, NotPlaceable)
class UNREALTOURNAMENT_API AUTAfterImageEffect : public AUTWeaponRedirector
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_CharMesh)
	USkeletalMesh* CharMesh;
	UPROPERTY(BlueprintReadWrite, Replicated)
	FLinearColor TeamColor;

	/** on clients we may trigger the effect when either CharMesh or Instigator is replicated, so use this to avoid repeat calls to start the effect */
	bool bPlayedEffect;

	virtual void OnRep_Instigator() override;
	UFUNCTION()
	virtual void OnRep_CharMesh();

	/** called to start the effect going after needed data has been set. Needs to be implemented for the effect to work correctly. */
	UFUNCTION(BlueprintImplementableEvent)
	void StartEffect();

	virtual void InitFor_Implementation(APawn* InInstigator, const FRepCollisionShape& InCollision, UPrimitiveComponent* InBase, const FTransform& InDest) override;

	/** copies the visuals of Instigator's Mesh (or CharMesh, if Instigator is not available) onto the passed in component */
	UFUNCTION(BlueprintCallable, Category = Mesh)
	virtual void SetMeshProperties(USkeletalMeshComponent* DestComp);
};