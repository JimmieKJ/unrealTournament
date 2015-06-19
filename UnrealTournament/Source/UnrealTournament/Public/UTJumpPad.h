// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPathBuilderInterface.h"
#include "UTJumpPadRenderingComponent.h"
#include "UTJumpPad.generated.h"

/**
 * jump pad for launching characters
 */
UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTJumpPad : public AActor, public IUTPathBuilderInterface
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

	/** Area type for this Navigation link */
	UPROPERTY(EditAnywhere, Category = JumpPad)
	TSubclassOf<class UNavArea> AreaClass;

	/** Get the Jump Pad Velocity from JumpActor's location */
	UFUNCTION(BlueprintCallable, Category = JumpPad)
	FVector CalculateJumpVelocity(AActor* JumpActor);

protected:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	class UUTJumpPadRenderingComponent* JumpPadComp;
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void CheckForErrors() override;
#endif // WITH_EDITOR

	/** Overridden to launch PendingJumpActors */
	virtual void Tick(float DeltaTime) override;

	/** returns whether the given Actor can be launched by this jumppad */
	UFUNCTION(BlueprintNativeEvent)
	bool CanLaunch(AActor* TestActor);
	/** Launches the actor */
	UFUNCTION(BlueprintNativeEvent)
	void Launch(AActor* Actor);
	
	/** Actors we want to Jump next tick */
	TArray<AActor*> PendingJumpActors;

	/** Event when this actor overlaps another actor. */
	UFUNCTION()
	virtual void TriggerBeginOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData);

	virtual bool IsDestinationOnly() const override
	{
		return true;
	}
};
