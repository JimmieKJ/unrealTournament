// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/HUD.h"
#include "UTLobbyHUD.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "UTLobbyPC.h"


AUTLobbyHUD::AUTLobbyHUD(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void AUTLobbyHUD::PostRender()
{
	AHUD::PostRender();

	AUTLobbyGameState* GS = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GS)
	{
		float Y = 475;
		for (int32 i=0; i < GS->AvailableMatches.Num(); i++)
		{
			if (GS->AvailableMatches[i])
			{
				DrawString(FText::Format(NSLOCTEXT("UTLOBBYHUD","LobbyDebugA","Lobby {0} - {1}"), FText::AsNumber(i), FText::FromString(GS->AvailableMatches[i]->MatchStats)), 10,Y, ETextHorzPos::Left, ETextVertPos::Top, SmallFont, FLinearColor::White, 1.0, true);
				Y+= 20;
				for (int32 j=0; j < GS->AvailableMatches[i]->Players.Num(); j++)
				{
					FText Name = GS->AvailableMatches[i]->Players[i] ? FText::FromString(GS->AvailableMatches[i]->Players[j]->PlayerName) : NSLOCTEXT("Generic","None","None");
					DrawString(FText::Format(NSLOCTEXT("UTLOBBYHUD","LobbyDebugB","Player {0} - {1}"), FText::AsNumber(j), Name), 40,Y, ETextHorzPos::Left, ETextVertPos::Top, SmallFont, FLinearColor::White, 1.0, true);

					Y+= 20;
				}
			}
		}
	}

	return;
}