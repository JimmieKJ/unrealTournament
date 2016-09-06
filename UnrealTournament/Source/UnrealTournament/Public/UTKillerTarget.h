// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTKillerTarget.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTKillerTarget : public AActor
{
	GENERATED_UCLASS_BODY()

		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Flag)
		USkeletalMeshComponent* Mesh;

	virtual void InitFor(class AUTCharacter* KillerPawn);

};

