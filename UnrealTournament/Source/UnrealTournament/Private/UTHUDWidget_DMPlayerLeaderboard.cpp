// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_DMPlayerLeaderboard.h"

UUTHUDWidget_DMPlayerLeaderboard::UUTHUDWidget_DMPlayerLeaderboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Position=FVector2D(0.0f, 0.0f);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(1.0f, 0.0f);
	Origin=FVector2D(1.0f,0.0f);
	OwnerNameColor = FLinearColor::White;
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
		AUTPlayerState* OwnerPS = UTPlayerOwner->UTPlayerState;
		int32 MyIndex = UTHUDOwner->Leaderboard.Find(OwnerPS);
		float YPosition = 0;
		if (OwnerPS && UTHUDOwner->Leaderboard.Num() > 0)
		{
			for (int32 DrawIndex = 0; DrawIndex < 2; DrawIndex++)
			{
				DrawPlayer(YPosition, DrawIndex, DrawIndex == MyIndex);
			}

			if (MyIndex>1)
			{
				YPosition += 10;
				DrawPlayer(YPosition, MyIndex, true);
			}
			else
			{
				DrawPlayer(YPosition, 2, false);
			}
		}
	}
}

void UUTHUDWidget_DMPlayerLeaderboard::DrawPlayer(float& YPosition, int32 PlayerIndex, bool bIsOwner)
{
	if (PlayerIndex >= UTHUDOwner->Leaderboard.Num()) return;

	AUTPlayerState* PS = UTHUDOwner->Leaderboard[PlayerIndex];

	// Find the actual place for this player.

	int32 MyPlace = 0;
	for (int32 i=1;i<=PlayerIndex;i++)
	{
		if (UTHUDOwner->Leaderboard[i]->Score != UTHUDOwner->Leaderboard[i-1]->Score)
		{
			MyPlace++;
		}
	}

	MyPlace++;

	if (bIsOwner && PlayerIndex > 0)
	{
		// Look to see if we have to draw what is above the player...
		int32 SpreadToTop = UTHUDOwner->Leaderboard[0]->Score - PS->Score;
		
		// Draw the indicator
		RenderObj_TextureAt(SpreadBar[0], SpreadBar[0].GetWidth() * -1, YPosition, SpreadBar[0].GetWidth(), SpreadBar[0].GetHeight());
		RenderObj_TextureAt(SpreadBar[1], SpreadBar[1].GetWidth() * -1, YPosition, SpreadBar[1].GetWidth(), SpreadBar[1].GetHeight());

		RenderObj_TextureAt(UpIcon, (SpreadBar[0].GetWidth() * -1) + UpIcon.Position.X, YPosition + UpIcon.Position.Y, UpIcon.GetWidth(), UpIcon.GetHeight());

		SpreadTextTemplate.Text = FText::AsNumber(SpreadToTop);
		RenderObj_TextAt(SpreadTextTemplate,  SpreadTextTemplate.Position.X, YPosition + SpreadTextTemplate.Position.Y);
		YPosition += SpreadBar[0].GetHeight() * 1.2;
	}

	// Draw the player name

	RenderObj_TextureAt(Bar[0], BarWidth * -1, YPosition, BarWidth, Bar[0].GetHeight());
	RenderObj_TextureAt(Bar[1], BarWidth * -1, YPosition, BarWidth, Bar[1].GetHeight());
	NameTextTemplate.Text = FText::FromString(PS->PlayerName);

	NameTextTemplate.RenderColor = (bIsOwner) ? OwnerNameColor : GetClass()->GetDefaultObject<UUTHUDWidget_DMPlayerLeaderboard>()->NameTextTemplate.RenderColor;
	RenderObj_TextAt(NameTextTemplate, 0 + NameTextTemplate.Position.X, YPosition + NameTextTemplate.Position.Y);

	// Draw the place

	float XPosition = BarWidth * -1;

	RenderObj_TextureAt(Header[0], XPosition, YPosition, Header[0].GetWidth(), Header[0].GetHeight());
	RenderObj_TextureAt(Header[1], XPosition, YPosition, Header[1].GetWidth(), Header[1].GetHeight());
	PlaceTextTemplate.Text = FText::AsNumber(MyPlace);
	RenderObj_TextAt(PlaceTextTemplate, XPosition + PlaceTextTemplate.Position.X, YPosition + PlaceTextTemplate.Position.Y);

	YPosition += Header[0].GetHeight() * 1.2;

	if (bIsOwner && PlayerIndex < UTHUDOwner->Leaderboard.Num()-1)
	{
		// Look to see if we have to draw what is above the player...
		int32 SpreadToTop = PS->Score - UTHUDOwner->Leaderboard[PlayerIndex+1]->Score;
		
		// Draw the indicator
		RenderObj_TextureAt(SpreadBar[0], SpreadBar[0].GetWidth() * -1, YPosition, SpreadBar[0].GetWidth(), SpreadBar[0].GetHeight());
		RenderObj_TextureAt(SpreadBar[1], SpreadBar[1].GetWidth() * -1, YPosition, SpreadBar[1].GetWidth(), SpreadBar[1].GetHeight());

		RenderObj_TextureAt(DownIcon, (SpreadBar[0].GetWidth() * -1) + DownIcon.Position.X, YPosition + DownIcon.Position.Y, DownIcon.GetWidth(), DownIcon.GetHeight());

		SpreadTextTemplate.Text = FText::AsNumber(SpreadToTop);
		RenderObj_TextAt(SpreadTextTemplate, SpreadTextTemplate.Position.X, YPosition + SpreadTextTemplate.Position.Y);
		YPosition += SpreadBar[0].GetHeight() * 1.2;
	}
}