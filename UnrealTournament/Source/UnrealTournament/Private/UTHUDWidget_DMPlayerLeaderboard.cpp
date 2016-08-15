// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_DMPlayerLeaderboard.h"

UUTHUDWidget_DMPlayerLeaderboard::UUTHUDWidget_DMPlayerLeaderboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Position=FVector2D(0.0f, 0.0f);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(1.0f, 0.0f);
	Origin=FVector2D(1.0f,0.0f);
	OwnerNameColor = FLinearColor::White;
	SpreadText = NSLOCTEXT("DMPlayerLeaderboard", "PlusFormat", "+{0}");
}

void UUTHUDWidget_DMPlayerLeaderboard::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
}

/**
 *	We aren't going tor use DrawAllRenderObjects.  Instead we are going to have a nice little custom bit of drawing based on what weapon gropup this 
 *  is.
 **/
void UUTHUDWidget_DMPlayerLeaderboard::Draw_Implementation(float DeltaTime)
{
	if (UTPlayerOwner)
	{
		UTHUDOwner->CalcStanding();
		AUTPlayerState* OwnerPS = UTPlayerOwner->UTPlayerState;
		int32 MyIndex = UTHUDOwner->Leaderboard.Find(OwnerPS);
		float YPosition = 0;
		if (OwnerPS && UTHUDOwner->Leaderboard.Num() > 0)
		{
			for (int32 DrawIndex = 0; DrawIndex < 2; DrawIndex++)
			{
				DrawPlayer(YPosition, DrawIndex, OwnerPS);
			}

			if (MyIndex>1)
			{
				YPosition += 10;
				DrawPlayer(YPosition, MyIndex, OwnerPS);
			}
			else
			{
				DrawPlayer(YPosition, 2, OwnerPS);
			}
		}
	}
}


bool UUTHUDWidget_DMPlayerLeaderboard::ShouldDraw_Implementation(bool bShowScores)
{
	return Super::ShouldDraw_Implementation(bShowScores) && !UTHUDOwner->bDrawMinimap && UTGameState && UTGameState->HasMatchStarted();
}

void UUTHUDWidget_DMPlayerLeaderboard::DrawPlayer(float& YPosition, int32 PlayerIndex, AUTPlayerState* OwnerPS)
{
	if (PlayerIndex >= UTHUDOwner->Leaderboard.Num()) return;

	AUTPlayerState* PS = UTHUDOwner->Leaderboard[PlayerIndex];

	bool bIsOwner = PS == OwnerPS;

	// Find the actual place for this player.

	int32 MyPlace = 0;
	for (int32 i=0;i<=PlayerIndex;i++)
	{
		if (UTHUDOwner->Leaderboard[i]->Score > PS->Score)
		{
			MyPlace++;
		}
	}

	MyPlace++;

	// Draw the player name
	float XPosition = BarWidth * -1;
	RenderObj_TextureAt(Header[0], XPosition, YPosition, Header[0].GetWidth(), Header[0].GetHeight());
	RenderObj_TextureAt(Header[1], XPosition, YPosition, Header[1].GetWidth(), Header[1].GetHeight());
	RenderObj_TextureAt(Bar[0], XPosition, YPosition, BarWidth, Bar[0].GetHeight());
	RenderObj_TextureAt(Bar[1], XPosition, YPosition, BarWidth, Bar[1].GetHeight());

	float NameScale = bScaleByDesignedResolution ? RenderScale : 1.f;
	NameScale *= SpreadTextTemplate.TextScale;
	FString PlayerName = GetClampedName(PS, SpreadTextTemplate.Font, NameScale, 0.7f * BarWidth);
	NameTextTemplate.Text = FText::FromString(PlayerName);
	NameTextTemplate.RenderColor = (bIsOwner) ? OwnerNameColor : GetClass()->GetDefaultObject<UUTHUDWidget_DMPlayerLeaderboard>()->NameTextTemplate.RenderColor;
	RenderObj_TextAt(NameTextTemplate, XPosition + NameTextTemplate.Position.X, YPosition + NameTextTemplate.Position.Y);

	// Draw the Score
	PlaceTextTemplate.Text = FText::AsNumber(int32(PS->Score));
	RenderObj_TextAt(PlaceTextTemplate, 0 + PlaceTextTemplate.Position.X, YPosition + PlaceTextTemplate.Position.Y);

	if (PlayerIndex == 0)
	{
		// Look to see if we have to draw what is above the player...
		int32 Spread = 0;
		
		if (bIsOwner)
		{
			if (UTHUDOwner->Leaderboard.Num() > 1)
			{
				Spread = OwnerPS->Score - UTHUDOwner->Leaderboard[1]->Score;
			}
		}
		else
		{
			Spread = OwnerPS->Score - PS->Score;
		}
		
		XPosition -= Header[0].GetWidth();

		// Draw the indicator
		RenderObj_TextureAt(SpreadBar[0], XPosition + SpreadBar[0].Position.X, YPosition + SpreadBar[0].Position.Y, SpreadBar[0].GetWidth(), SpreadBar[0].GetHeight());
		RenderObj_TextureAt(SpreadBar[1], XPosition + SpreadBar[0].Position.X, YPosition + SpreadBar[0].Position.Y, SpreadBar[1].GetWidth(), SpreadBar[1].GetHeight());

		SpreadTextTemplate.Text = Spread > 0 ? FText::Format(SpreadText, FText::AsNumber(Spread)) : FText::AsNumber(Spread);
		RenderObj_TextAt(SpreadTextTemplate, XPosition + SpreadTextTemplate.Position.X, YPosition + SpreadTextTemplate.Position.Y);
	}

	YPosition += Header[0].GetHeight() * 1.2;
}
