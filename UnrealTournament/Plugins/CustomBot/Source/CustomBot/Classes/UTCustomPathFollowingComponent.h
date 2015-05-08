// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Navigation/PathFollowingComponent.h"
#include "UTCustomPathFollowingComponent.generated.h"

class UCharacterMovementComponent;

UCLASS()
class CUSTOMBOT_API UUTCustomPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

protected:
	/** cashes UPathFollowingComponent.MovementComponent cast to UCharacterMovementComponent*/
	UPROPERTY()
	UCharacterMovementComponent* CharacterMovementComp;

public:
	virtual void SetMovementComponent(UNavMovementComponent* MoveComp) override;

protected:
	virtual void FollowPathSegment(float DeltaTime) override;
};
