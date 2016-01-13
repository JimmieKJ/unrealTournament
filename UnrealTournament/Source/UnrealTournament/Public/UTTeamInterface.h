// implement this interface to allow game code to determine what team it is on
// Actors without this interface are assumed to be on no team (generally hostile to all things)
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamInterface.generated.h"

UINTERFACE(MinimalAPI)
class UUTTeamInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class UNREALTOURNAMENT_API IUTTeamInterface
{
	GENERATED_IINTERFACE_BODY()

	/** if this returns true then the Actor is considered to be on all teams simultaneously (OnSameTeam() will always return true) */
	virtual bool IsFriendlyToAll() const
	{
		return false;
	}

	/** version of GetTeamNum() for blueprints */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "GetTeamNum"))
	uint8 ScriptGetTeamNum();

	/** return team number the object is on, or 255 for no team */
	virtual uint8 GetTeamNum() const PURE_VIRTUAL(IUTTeamInterface::GetTeamNum, return 255;);

	/** called to update the object's team when swapping sides
	 * implementing this function isn't required, but if level objects don't and side swapping is enabled then
	 * they will probably end up incorrect
	 */
	UFUNCTION(BlueprintNativeEvent, Category = Team)
	void SetTeamForSideSwap(uint8 NewTeamNum);
};
