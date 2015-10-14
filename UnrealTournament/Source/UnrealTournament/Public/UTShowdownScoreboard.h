// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTTeamScoreboard.h"

#include "UTShowdownScoreboard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTShowdownScoreboard : public UUTTeamScoreboard
{
	GENERATED_BODY()
public:
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
	{
		Super::DrawPlayer(Index, PlayerState, RenderDelta, XOffset, YOffset);

		// strikeout players that are dead
		AUTShowdownGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTShowdownGameState>();
		if (GS != NULL && GS->IsMatchInProgress())
		{
			AUTCharacter* UTC = PlayerState->GetUTCharacter();
			if (UTC == NULL || UTC->IsDead())
			{
				float BaseWidth = ((Size.X * 0.5f) - CenterBuffer);
				float Height = 16.0f;
				DrawTexture(GetCanvas()->DefaultTexture, XOffset + BaseWidth * 0.05f, YOffset + 8.0f, BaseWidth * 0.9f, Height, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, FLinearColor::Red);
			}
		}
	}
};
