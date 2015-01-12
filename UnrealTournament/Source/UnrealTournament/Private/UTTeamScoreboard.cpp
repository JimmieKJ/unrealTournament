// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTTeamScoreboard.h"

UUTTeamScoreboard::UUTTeamScoreboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UFont> HFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Huge.fntScoreboard_Huge'"));
	HugeFont = HFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> SFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Score.fntScoreboard_Score'"));
	ScoreFont = SFont.Object;
}

void UUTTeamScoreboard::DrawTeamPanel(float RenderDelta, float& YOffset)
{
	if (UTGameState == NULL || UTGameState->Teams.Num() < 2 || UTGameState->Teams[0] == NULL || UTGameState->Teams[1] == NULL) return;

	// Draw the front end End

	float FrontSize = 35;
	float EndSize = 16;
	float MiddleSize = 624 - FrontSize - EndSize;
	float XOffset = 0;

	// Draw the Background
	DrawTexture(TextureAtlas, XOffset, YOffset + 22, 35, 65, 0, 188, 36, 65, 1.0, FLinearColor::Red);
	DrawTexture(TextureAtlas, XOffset + FrontSize, YOffset + 22, MiddleSize, 65, 39,188,64,65, 1.0, FLinearColor::Red);
	DrawTexture(TextureAtlas, XOffset + FrontSize + MiddleSize, YOffset + 22, EndSize, 65, 39,188,64,65, 1.0, FLinearColor::Red);

	DrawText(NSLOCTEXT("UTTeamScoreboard","RedTeam","RED"), 36, YOffset + 40, HugeFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	DrawText(FText::AsNumber(UTGameState->Teams[0]->Score), 620, YOffset + 48, ScoreFont, false, FVector2D(0,0), FLinearColor::Black, true, FLinearColor::Black, 0.75, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);

	XOffset += 645;

	DrawTexture(TextureAtlas, XOffset + EndSize + MiddleSize, YOffset + 22, 35, 65, 196, 188, 36, 65 , 1.0, FLinearColor::Blue);
	DrawTexture(TextureAtlas, XOffset + EndSize, YOffset + 22, MiddleSize, 65, 130,188,64,65, 1.0, FLinearColor::Blue);
	DrawTexture(TextureAtlas, XOffset, YOffset + 22, EndSize, 65, 117, 188, 16, 65, 1.0, FLinearColor::Blue);

	DrawText(NSLOCTEXT("UTTeamScoreboard", "BlueTeam", "BLUE"), 1237, YOffset + 40, HugeFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
	DrawText(FText::AsNumber(UTGameState->Teams[1]->Score), 654, YOffset + 48, ScoreFont, false, FVector2D(0,0), FLinearColor::Black, true, FLinearColor::Black, 0.75, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);

	YOffset += 119;

}

void UUTTeamScoreboard::DrawPlayerScores(float RenderDelta, float& YOffset)
{
	if (UTGameState == nullptr)
	{
		return;
	}

	int32 NumSpectators = 0;
	int32 XOffset = 0;

	for (int8 Team = 0; Team < 2; Team++)
	{
		int32 Place = 1;
		float DrawOffset = YOffset;
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PlayerState)
			{
				if (!PlayerState->bOnlySpectator)
				{
					if (PlayerState->GetTeamNum() == Team)
					{
						DrawPlayer(Place, PlayerState, RenderDelta, XOffset, DrawOffset);
						Place++;
						DrawOffset += 40;
					}
				} 
				else
				{
					NumSpectators++;
				}
			}
		}
		XOffset = 645;
	}

	if (UTGameState->PlayerArray.Num() <= 28 && NumSpectators > 0)
	{
		FText SpectatorCount = FText::Format(NSLOCTEXT("UTScoreboard", "SpectatorFormat", "{0} spectators are watching this match"), FText::AsNumber(NumSpectators));
		DrawText(SpectatorCount, 635, 765, SmallFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Bottom);
	}
}


