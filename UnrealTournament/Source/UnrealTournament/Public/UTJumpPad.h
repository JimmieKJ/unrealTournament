// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPathBuilderInterface.h"
#include "UTJumpPadRenderingComponent.h"
#include "UTJumpPad.generated.h"

/**
 * jump pad for launching characters
 */
UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTJumpPad : public AActor, public IUTPathBuilderInterface, public INavRelevantInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = JumpPad)
	USceneComponent* SceneRoot;

	/** Static mesh for the Jump Pad */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = JumpPad)
	class UStaticMeshComponent* Mesh;

	/** The Player will Jump when overlapping this box */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = JumpPad)
	class UBoxComponent* TriggerBox;

	/** Sound to play when we jump*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = JumpPad)
	USoundBase* JumpSound;

	/** The destination of this Jump Pad */
	UPROPERTY(EditAnywhere, Category = JumpPad, BlueprintReadWrite, meta = (MakeEditWidget = ""))
	FVector JumpTarget;

	/** The length of air time it takes to reach the target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = JumpPad, meta = (ClampMin = "0.1"))
	float JumpTime;

	/** amount of jump time during which a Pawn's movement is restricted (no air control, etc) to avoid them unintentionally missing the expected destination */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = JumpPad)
	float RestrictedJumpTime;

	/** Keep the characters XY Velocity when jumping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = JumpPad)
	bool bMaintainVelocity;

	/** if set AI using this jump pad will not air control while moving upwards - use in edge cases where the AI's attempt to help their jump makes them clip some edge or outcropping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI, Meta = (DisplayName = "Reduce AI Air Control"))
	bool bAIReducedAirControl;

	/** cached AI flag for whether this jump pad boosts the user through water */
	UPROPERTY(BlueprintReadOnly, Category = AI)
	bool bJumpThroughWater;

	/** Get the Jump Pad Velocity from JumpActor's location */
	UFUNCTION(BlueprintCallable, Category = JumpPad)
	FVector CalculateJumpVelocity(AActor* JumpActor);

	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();

		// compatibility
		if (AuthoredGravityZ == 0.0f)
		{
			AuthoredGravityZ = GetDefault<UWorld>()->GetDefaultGravityZ();
		}
	}
protected:

	/** used to detect low grav mods */
	UPROPERTY()
	float AuthoredGravityZ;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	class UUTJumpPadRenderingComponent* JumpPadComp;
#endif

#if WITH_EDITOR
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void CheckForErrors() override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override
	{
		Super::PreSave(TargetPlatform);

		if (GIsEditor && !IsTemplate() && !IsRunningCommandlet())
		{
			AuthoredGravityZ = GetLocationGravityZ(GetWorld(), GetActorLocation(), TriggerBox->GetCollisionShape());
		}
	}
protected:
#endif // WITH_EDITOR

public:
	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const override
	{
		if (TriggerBox != NULL)
		{
			TriggerBox->CalcBoundingCylinder(CollisionRadius, CollisionHalfHeight);
		}
		else
		{
			Super::GetSimpleCollisionCylinder(CollisionRadius, CollisionHalfHeight);
		}
	}

	/** Overridden to launch PendingJumpActors */
	virtual void Tick(float DeltaTime) override;

	/** returns whether this jump pad is currently enabled and will launch actors that touch it and pass CanLaunch() */
	UFUNCTION(BlueprintNativeEvent)
	bool IsEnabled() const;
	/** returns whether the given Actor can be launched by this jumppad */
	UFUNCTION(BlueprintNativeEvent)
	bool CanLaunch(AActor* TestActor);
	/** Launches the actor */
	UFUNCTION(BlueprintNativeEvent)
	void Launch(AActor* Actor);

	/** AI flag - set if jump pad is usually inactive but can be temporarily enabled via switch or trigger
	 * need to rebuild paths after changing
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AI)
	bool bTemporaryActivation;

	/** Actors we want to Jump next tick */
	TArray<AActor*> PendingJumpActors;

	/** Event when this actor overlaps another actor. */
	UFUNCTION()
	virtual void TriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData);

	virtual bool IsDestinationOnly() const override
	{
		return !bTemporaryActivation;
	}

	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual FBox GetNavigationBounds() const override
	{
		return (TriggerBox != nullptr) ? TriggerBox->Bounds.GetBox() : FBox(0);
	}
};
