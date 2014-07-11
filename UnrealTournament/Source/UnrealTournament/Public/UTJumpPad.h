// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTJumpPadRenderingComponent.h"
#include "UTJumpPad.generated.h"

/**
 * jump pad for launching characters
 */
UCLASS(Blueprintable)
class AUTJumpPad : public AActor, public INavLinkHostInterface, public INavRelevantActorInterface
{
	GENERATED_UCLASS_BODY()

	/** Static mesh for the Jump Pad */
	UPROPERTY(VisibleAnywhere, Category = JumpPad)
	TSubobjectPtr<class UStaticMeshComponent> Mesh;

	/** The Player will Jump when overlapping this box */
	UPROPERTY(VisibleAnywhere, Category = JumpPad)
	TSubobjectPtr<class UBoxComponent> TriggerBox;

	/** Sound to play when we jump*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = JumpPad)
	USoundBase* JumpSound;

	/** The destination of this Jump Pad */
	UPROPERTY(EditAnywhere, Category = JumpPad, BlueprintReadWrite, meta = (MakeEditWidget = ""))
	FVector JumpTarget;

	/** The length of air time it takes to reach the target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = JumpPad, meta = (ClampMin = "0.1"))
	float JumpTime;

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
	TSubobjectPtr<class UUTJumpPadRenderingComponent> JumpPadComp;
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR

	/** Overridden to launch PendingJumpActors */
	virtual void Tick(float DeltaTime) OVERRIDE;

	virtual void CheckForErrors() OVERRIDE;

	/** returns whether the given Actor can be launched by this jumppad */
	UFUNCTION(BlueprintNativeEvent)
	bool CanLaunch(AActor* TestActor);
	/** Launches the actor */
	UFUNCTION(BlueprintNativeEvent)
	void Launch(AActor* Actor);

	// BEGIN INavRelevantActorInterface
	virtual bool UpdateNavigationRelevancy() OVERRIDE;
	virtual bool GetNavigationRelevantData(struct FNavigationRelevantData& Data) const OVERRIDE;
	// END INavRelevantActorInterface

	// BEGIN INavLinkHostInterface
	virtual bool GetNavigationLinksClasses(TArray<TSubclassOf<class UNavLinkDefinition> >& OutClasses) const OVERRIDE;
	virtual bool GetNavigationLinksArray(TArray<FNavigationLink>& OutLink, TArray<FNavigationSegmentLink>& OutSegments) const OVERRIDE;
	// END INavLinkHostInterface

	/** The Nav link between the Jump pad and the Target */
	FNavigationLink PointLink;

	/** Actors we want to Jump next tick */
	TArray<AActor*> PendingJumpActors;

	/** Event when this actor overlaps another actor. */
	virtual void ReceiveActorBeginOverlap(class AActor* OtherActor);
};
