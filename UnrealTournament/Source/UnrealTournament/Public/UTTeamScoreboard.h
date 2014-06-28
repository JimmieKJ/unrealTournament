// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTTeamScoreboard.generated.h"

UCLASS()
class UUTTeamScoreboard : public UUTScoreboard
{
	GENERATED_UCLASS_BODY()

public:

	virtual void DrawScoreboard(float RenderDelta);

protected:

	virtual void DrawPlayers(float RenderDelta, float X, float Y, float ClipX, float ClipY, int32 TeamFilter = -1);


};

