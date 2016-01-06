// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTTeamScoreboard.h"
#include "UTShowdownScoreboard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTShowdownScoreboard : public UUTTeamScoreboard
{
	GENERATED_UCLASS_BODY()

	public:
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset) override;
	virtual void Draw_Implementation(float RenderDelta) override;
};

