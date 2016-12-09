// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** This class represents an APEX Destructible Actor. */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "DestructibleActor.generated.h"

class UDestructibleComponent;

/** Delegate for notification when fracture occurs */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActorFractureSignature, const FVector &, HitPoint, const FVector &, HitDirection);

UCLASS(hideCategories=(Input), showCategories=("Input|MouseInput", "Input|TouchInput"), ComponentWrapperClass)
class ENGINE_API ADestructibleActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/**
	 * The component which holds the skinned mesh and physics data for this actor.
	 */
private_subobject:
	DEPRECATED_FORGAME(4.6, "DestructibleComponent should not be accessed directly, please use GetDestructibleComponent() function instead. DestructibleComponent will soon be private and your code will not compile.")
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Destruction, meta = (ExposeFunctionCategories = "Destruction,Components|Destructible", AllowPrivateAccess = "true"))
	UDestructibleComponent* DestructibleComponent;
public:

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category=Navigation)
	uint32 bAffectNavigation : 1;

	UPROPERTY(BlueprintAssignable, Category = "Components|Destructible")
	FActorFractureSignature OnActorFracture;

	//~ Begin AActor Interface.
#if WITH_EDITOR
	virtual bool GetReferencedContentObjects( TArray<UObject*>& Objects ) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End AActor Interface.

	virtual void PostLoad() override;


public:
	/** Returns DestructibleComponent subobject **/
	UDestructibleComponent* GetDestructibleComponent() const;
};



