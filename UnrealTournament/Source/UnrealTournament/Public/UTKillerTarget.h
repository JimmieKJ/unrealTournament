// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTKillerTarget.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTKillerTarget : public AActor
{
	GENERATED_UCLASS_BODY()

		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Flag)
		USkeletalMeshComponent* Mesh;

	UPROPERTY()
		bool bHasTicked;

	UPROPERTY()
		class AUTPlayerController* Watcher;

	virtual void InitFor(class AUTCharacter* KillerPawn, AUTPlayerController* InWatcher);
	virtual void Tick(float DeltaTime) override;
};

