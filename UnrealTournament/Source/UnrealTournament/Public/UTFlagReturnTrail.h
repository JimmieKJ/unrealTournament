// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTReplicatedEmitter.h"
#include "UTFlagReturnTrail.generated.h"

UCLASS(Abstract, Blueprintable, Meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTFlagReturnTrail : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	class AUTCarriedObject* Flag;

	/** set points for trail to follow, including start point */
	UFUNCTION(BlueprintImplementableEvent)
	void SetPoints(const TArray<FVector>& Points);

	UFUNCTION(BlueprintImplementableEvent)
	void SetTeam(AUTTeamInfo* Team);

	UFUNCTION(BlueprintImplementableEvent)
	void EndTrail();
};