// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTReplicatedEmitter.h"
#include "UTFlagReturnTrail.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagReturnTrail : public AUTReplicatedEmitter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Replicated)
	FVector EndPoint;

	UPROPERTY(ReplicatedUsing=OnReceivedTeamIndex)
		uint8 TeamIndex;

	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;

	UFUNCTION()
	virtual	void OnReceivedTeamIndex();

	virtual	void SetTeamIndex(uint8 NewValue);
};