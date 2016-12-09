#include "UnrealTournament.h"

#include "UTAnalytics.h"
#include "UTGameInstance.h"
#include "UTGameState.h"
#include "UTHeartBeatManager.h"
#include "UTParty.h"
#include "UTPartyGameState.h"
#include "UTPlayerController.h"
#include "UTMenuGameMode.h"
#include "UTLobbyGameState.h"

UUTHeartbeatManager::UUTHeartbeatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UUTHeartbeatManager::StartManager(AUTPlayerController* UTPlayerController)
{
	if (!UTPlayerController)
	{
		return;
	}

	UTPC = UTPlayerController;

	//Do per minute events and wipe the minute stat periods
	UTPC->GetWorldTimerManager().SetTimer(DoMinuteEventsTimerHandle, this, &UUTHeartbeatManager::DoMinuteEvents, 60.0f, true);

	//Do per five second events and wipe the minute stat periods
	UTPC->GetWorldTimerManager().SetTimer(DoFiveSecondEventsTimerHandle, this, &UUTHeartbeatManager::DoFiveSecondEvents, 5.0f, true);
}

void UUTHeartbeatManager::StopManager()
{
	if (UTPC)
	{
		UTPC->GetWorldTimerManager().ClearAllTimersForObject(this);
	}
}

void UUTHeartbeatManager::DoMinuteEvents()
{
	if (UTPC == nullptr)
	{
		return;
	}

	if (UTPC->Role == ROLE_Authority)
	{
		DoMinuteEventsServer();
	}
	if (UTPC->IsLocalPlayerController())
	{
		DoMinuteEventsLocal();
	}
}

void UUTHeartbeatManager::DoFiveSecondEvents()
{

}

void UUTHeartbeatManager::DoMinuteEventsServer()
{

}

void UUTHeartbeatManager::DoMinuteEventsLocal()
{
	SendPlayerContextLocationPerMinute();
}

void UUTHeartbeatManager::SendPlayerContextLocationPerMinute()
{
	if (UTPC != NULL && UTPC->GetWorld())
	{
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPC->PlayerState);
		if (UTPS)
		{
			UUTGameInstance* GameInstance = Cast<UUTGameInstance>(UTPC->GetGameInstance());
			if (GameInstance && GameInstance->GetParties())
			{
				// Check to see if we are in a social party
				int32 NumPartyMembers = 0;
				if (UUTPartyGameState* SocialParty = Cast<UUTPartyGameState>(GameInstance->GetParties()->GetPersistentParty()))
				{
					TArray<UPartyMemberState*> PartyMembers;
					SocialParty->GetAllPartyMembers(PartyMembers);

					NumPartyMembers = PartyMembers.Num();
				}
							
				FString PlayerConxtextLocation;
				AUTGameState* UTGS = Cast<AUTGameState>(UTPS->GetWorld()->GetGameState());
				if (UTGS)
				{
					PlayerConxtextLocation = TEXT("Match");

					AUTMenuGameMode* MenuGM = UTPS->GetWorld()->GetAuthGameMode<AUTMenuGameMode>();
					if (MenuGM)
					{
						PlayerConxtextLocation = TEXT("Menu");
					}

					AUTLobbyGameState* LobbyGS = Cast<AUTLobbyGameState>(UTGS);
					if (LobbyGS)
					{
						PlayerConxtextLocation = TEXT("Lobby");
					}

					if (UTPC->GetWorld()->DemoNetDriver && UTPC->IsLocalController())
					{
						PlayerConxtextLocation = TEXT("Replay");
					}
				}

				FUTAnalytics::FireEvent_PlayerContextLocationPerMinute(UTPC, PlayerConxtextLocation, NumPartyMembers);
			}
		}
	}
}
