// this class is hit by weapons fire and can be used either to redirect it (e.g. teleport projectiles)
// or to take the damage and pass it on to its owner
// generally used for teleport "afterimage" effect
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponRedirector.generated.h"

/** replicatable version of FCollisionShape... */
USTRUCT()
struct FRepCollisionShape
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint8 ShapeType;
	/** shape dimensions, interpreted depending on shape type */
	UPROPERTY()
	FVector_NetQuantize10 Dimensions;

	FRepCollisionShape()
	{}
	FRepCollisionShape(const FCollisionShape& InShape)
	: ShapeType(uint8(InShape.ShapeType)), Dimensions(FVector::ZeroVector)
	{
		switch (InShape.ShapeType)
		{
			case ECollisionShape::Box:
				Dimensions = InShape.GetExtent();
				break;
			case ECollisionShape::Sphere:
				Dimensions.X = Dimensions.Y = Dimensions.Z = InShape.GetSphereRadius();
				break;
			case ECollisionShape::Capsule:
				Dimensions.X = InShape.GetCapsuleRadius();
				Dimensions.Y = InShape.GetCapsuleRadius();
				Dimensions.Z = InShape.GetCapsuleHalfHeight();
				break;
			default:
				break;
		}
	}
	FRepCollisionShape& operator==(const FCollisionShape& OtherShape)
	{
		*this = FRepCollisionShape(OtherShape);
		return *this;
	}

	inline FVector GetExtent() const
	{
		return Dimensions;
	}
	inline float GetSphereRadius() const
	{
		return Dimensions.X;
	}
	inline float GetCapsuleRadius() const
	{
		return Dimensions.X;
	}
	inline float GetCapsuleHalfHeight() const
	{
		return Dimensions.Z;
	}
};

UCLASS(Blueprintable, NotPlaceable)
class UNREALTOURNAMENT_API AUTWeaponRedirector : public AActor
{
	GENERATED_BODY()
public:
	AUTWeaponRedirector(const FObjectInitializer& OI)
	: Super(OI)
	{
		RootComponent = OI.CreateDefaultSubobject<USceneComponent>(this, FName(TEXT("Root")));
		bRedirectDamage = true;
		InitialLifeSpan = 0.15f;
		bReplicates = true;
	}

protected:
	UPROPERTY(ReplicatedUsing = UpdateCollisionShape, BlueprintReadOnly)
	FRepCollisionShape CollisionInfo;
	UPROPERTY()
	UPrimitiveComponent* CollisionComp;

public:
	/** exit point when teleporting impacting objects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	FTransform PortalDest;

	/** if set, redirect damage that we take to the instigator of the effect	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	bool bRedirectDamage;

	/** effect played when warping weapons fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UParticleSystem* WarpEffect;

	/** effect played when warping weapons fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TSubclassOf<class AUTReplicatedEmitter> DamageEffect;

	UFUNCTION(BlueprintNativeEvent, Category = Init)
	void InitFor(APawn* InInstigator, const FRepCollisionShape& InCollision, UPrimitiveComponent* InBase, const FTransform& InDest = FTransform::Identity);
	virtual void InitFor_Implementation(APawn* InInstigator, const FRepCollisionShape& InCollision, UPrimitiveComponent* InBase, const FTransform& InDest);

	UFUNCTION()
	virtual void UpdateCollisionShape();

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
};