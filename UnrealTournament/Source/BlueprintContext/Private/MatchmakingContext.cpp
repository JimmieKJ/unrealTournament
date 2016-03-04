// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContext.h"

#include "UTMatchmaking.h"
#include "UTLocalPlayer.h"
#include "UTPartyGameState.h"
#include "MatchmakingContext.h"
#include "PartyContext.h"
#include "BlueprintContextLibrary.h"

#define LOCTEXT_NAMESPACE "UTMatchmakingContext"

REGISTER_CONTEXT(UMatchmakingContext);

UMatchmakingContext::UMatchmakingContext()
{
}

void UMatchmakingContext::Initialize()
{
	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	if (GameInstance)
	{
		UUTMatchmaking* UTMatchmaking = GameInstance->GetMatchmaking();
		if (UTMatchmaking)
		{
			UTMatchmaking->OnMatchmakingStarted().AddUObject(this, &ThisClass::OnMatchmakingStartedInternal);
			UTMatchmaking->OnMatchmakingStateChange().AddUObject(this, &ThisClass::OnMatchmakingStateChangeInternal);
			UTMatchmaking->OnMatchmakingComplete().AddUObject(this, &ThisClass::OnMatchmakingCompleteInternal);
			UTMatchmaking->OnPartyStateChange().AddUObject(this, &ThisClass::OnPartyStateChangeInternal);
		}
	}
}

void UMatchmakingContext::Finalize()
{
	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	if (GameInstance)
	{
		UUTMatchmaking* UTMatchmaking = GameInstance->GetMatchmaking();
		if (UTMatchmaking)
		{
			UTMatchmaking->OnMatchmakingStarted().RemoveAll(this);
			UTMatchmaking->OnMatchmakingStateChange().RemoveAll(this);
			UTMatchmaking->OnMatchmakingComplete().RemoveAll(this);
			UTMatchmaking->OnPartyStateChange().RemoveAll(this);
		}
	}
}

void UMatchmakingContext::OnMatchmakingStartedInternal()
{
	OnMatchmakingStarted.Broadcast();

	UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
	if (LocalPlayer)
	{
		LocalPlayer->ShowMatchmakingDialog();
	}
}

void UMatchmakingContext::OnMatchmakingStateChangeInternal(EMatchmakingState::Type OldState, EMatchmakingState::Type NewState, const FMMAttemptState& MMState)
{
	OnMatchmakingStateChange.Broadcast(OldState, NewState);
}

void UMatchmakingContext::OnMatchmakingCompleteInternal(EMatchmakingCompleteResult EndResult)
{
	OnMatchmakingComplete.Broadcast(EndResult);
}

void UMatchmakingContext::OnPartyStateChangeInternal(EUTPartyState NewPartyState)
{
	if (NewPartyState == EUTPartyState::Matchmaking)
	{
		UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
		if (LocalPlayer)
		{
			LocalPlayer->ShowMatchmakingDialog();
		}
	}
	else if (NewPartyState == EUTPartyState::Menus || NewPartyState == EUTPartyState::TravelToServer)
	{
		UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
		if (LocalPlayer)
		{
			LocalPlayer->HideMatchmakingDialog();
		}
	}
	else if (NewPartyState == EUTPartyState::CustomMatch)
	{
		UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
		if (LocalPlayer)
		{
			LocalPlayer->HideMatchmakingDialog();
		}
	}
}

void UMatchmakingContext::StartMatchmaking(int32 InPlaylistId)
{
	UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
	if (LocalPlayer)
	{
		if (!LocalPlayer->IsPartyLeader())
		{
			// Show error dialog
			return;
		}

		UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
		if (GameInstance && GameInstance->GetPlaylistManager())
		{
			int32 TeamCount = 0;
			int32 TeamSize = 0;
			int32 MaxPartySize = 0;
			if (!GameInstance->GetPlaylistManager()->GetMaxTeamInfoForPlaylist(InPlaylistId, TeamCount, TeamSize, MaxPartySize))
			{
				// Playlist probably doesn't exist :/ Show an error dialog
				return;
			}

			UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(LocalPlayer->GetWorld(), UPartyContext::StaticClass()));
			if (PartyContext)
			{
				if (MaxPartySize < PartyContext->GetPartySize())
				{
					// Show error dialog
					return; 
				}
			}
			
		}


		LocalPlayer->StartMatchmaking(InPlaylistId);
	}
}

#undef LOCTEXT_NAMESPACE