// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTeamScoreboard.h"
#include "UTCTFScoreboard.generated.h"

UCLASS()
class UUTCTFScoreboard : public UUTTeamScoreboard
{
	GENERATED_UCLASS_BODY()

	virtual void DrawScoreboard(float RenderDelta);


protected:
	virtual void DrawPlayers(float RenderDelta, float X, float Y, float ClipX, float ClipY, int32 TeamFilter = -1);

};