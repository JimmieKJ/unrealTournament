// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTReplicatedEmitter.h"
#include "UTFlagReturnTrail.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagReturnTrail : public AUTReplicatedEmitter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FVector EndPoint;

	UPROPERTY()
		FVector StartPoint;

	UPROPERTY()
		uint8 TeamIndex;

	UPROPERTY()
		AActor* StartActor;

	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;

	virtual	void SetTeamIndex(uint8 NewValue);
};