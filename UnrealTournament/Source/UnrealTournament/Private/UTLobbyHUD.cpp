// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	if (LobbyDebugLevel > 0)
	{
		AUTLobbyGameState* GS = GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GS)
		{
			float Y = 455;
			for (int32 i=0; i < GS->AvailableMatches.Num(); i++)
			{
				if (GS->AvailableMatches[i])
				{
					DrawString(FText::Format(NSLOCTEXT("UTLOBBYHUD","LobbyDebugA","Lobby {0} - {1}"), FText::AsNumber(i), GS->AvailableMatches[i]->GetDebugInfo()), 10,Y, ETextHorzPos::Left, ETextVertPos::Top, SmallFont, FLinearColor::White, 1.0, true);
					Y+= 20;
					for (int32 j=0; j < GS->AvailableMatches[i]->Players.Num(); j++)
					{
						AUTLobbyPlayerState* PS = GS->AvailableMatches[i]->Players[j].Get();
						FText Name = PS ? FText::FromString(PS->PlayerName) : NSLOCTEXT("Generic","None","None");
						DrawString(FText::Format(NSLOCTEXT("UTLOBBYHUD","LobbyDebugB","Player {0} - {1}"), FText::AsNumber(j), Name), 40,Y, ETextHorzPos::Left, ETextVertPos::Top, SmallFont, FLinearColor::White, 1.0, true);
						Y+= 20;
					}
				}
			}
			if (LobbyDebugLevel > 1)
			{
				for (int32 i = 0; i < GS->ClientAvailableGameModes.Num(); i++)
				{
					DrawString(FText::Format(NSLOCTEXT("UTLOBBYHUD", "LobbyDebugC", " Available Game Mode {0} = {1}"), FText::AsNumber(i), FText::FromString(*GS->ClientAvailableGameModes[i]->DisplayName)), 10, Y, ETextHorzPos::Left, ETextVertPos::Top, SmallFont, FLinearColor::White, 1.0, true);
					Y+=20;
				}
				for (int32 i = 0; i < GS->ClientAvailableMaps.Num(); i++)
				{
					DrawString(FText::Format(NSLOCTEXT("UTLOBBYHUD", "LobbyDebugD", " Available Maps {0} = {1}"), FText::AsNumber(i), FText::FromString(*GS->ClientAvailableMaps[i]->MapName)), 10, Y, ETextHorzPos::Left, ETextVertPos::Top, SmallFont, FLinearColor::White, 1.0, true);
					Y += 20;
				}
			}
		}
	}
}