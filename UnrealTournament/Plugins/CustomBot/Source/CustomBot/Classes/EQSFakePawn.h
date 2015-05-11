// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EnvironmentQuery/EQSTestingPawn.h"
#include "EQSFakePawn.generated.h"

class AUTCharacter;
struct FPropertyChangedEvent;

UCLASS(hidedropdown, hidecategories = (Advanced, Attachment, Collision, Mesh, Animation, Clothing, Physics, Rendering, Lighting, Activation, CharacterMovement, AgentPhysics, Avoidance, MovementComponent, Velocity, Shape, Camera, Input, Layers, SkeletalMesh, Optimization, Pawn, Replication, Actor))
class CUSTOMBOT_API AEQSFakePawn : public AEQSTestingPawn
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "EQS")
	AUTCharacter* Enemy;

	AEQSFakePawn(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	void UpdateEnemy();
};