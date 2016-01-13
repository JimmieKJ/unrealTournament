// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPathBuilderInterface.h"

#include "UTDefensePoint.generated.h"

/** marker for good spots AI should consider for defending the associated game objective */
UCLASS()
class UNREALTOURNAMENT_API AUTDefensePoint : public AActor, public IUTPathBuilderInterface
{
	GENERATED_BODY()
public:

	AUTDefensePoint(const FObjectInitializer& OI);

	/** objective this point defends */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite)
	class AUTGameObjective* Objective;
	/** base priority for this objective, final value may take into account bot preference and learning data */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BasePriority;
	/** if true, bot should stand near this point and shoot enemies from afar instead of using full combat behavior */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSniperSpot;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const override
	{
		CollisionRadius = GetDefault<AUTCharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
		CollisionHalfHeight = GetDefault<AUTCharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	}

	/** returns calculated priority of this defense point for selection by the passed in bot */
	UFUNCTION(BlueprintCallable, Category = AI)
	virtual int32 GetPriorityFor(class AUTBot* B) const;

	/** bot currently defending at this point */
	UPROPERTY(BlueprintReadOnly)
	AUTBot* CurrentUser;
protected:
	/** editor icon (don't make editoronly because it's the RootComponent - removing will cause transform to be lost) */
	UPROPERTY(VisibleAnywhere, Category = Sprite)
	UBillboardComponent* Icon;
#if WITH_EDITORONLY_DATA
	/** editor rotation indicator */
	UPROPERTY(VisibleAnywhere)
	UArrowComponent* EditorArrow;
#endif

	/** node defense point is on to grab navigation system data */
	UPROPERTY()
	const UUTPathNode* MyNode;
};