// base class for objects that only allow certain team(s) through
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPathBuilderInterface.h"

#include "UTTeamPathBlocker.generated.h"

UCLASS(Abstract, Blueprintable)
class UNREALTOURNAMENT_API AUTTeamPathBlocker : public AActor, public IUTPathBuilderInterface
{
	GENERATED_BODY()
public:
	/** whether players of the passed in team are allowed through, used by the AI pathing code */
	UFUNCTION(BlueprintNativeEvent)
	bool IsAllowedThrough(uint8 TeamNum);

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData) override;

	virtual bool IsDestinationOnly() const
	{
		return true;
	}
};