// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerState.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyMatchInfo.h"
#include "Net/UnrealNetwork.h"

AUTLobbyMatchInfo::AUTLobbyMatchInfo(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP
	.DoNotCreateDefaultSubobject(TEXT("Sprite")))
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;

	// Note: this is very important to set to false. Though all replication infos are spawned at run time, during seamless travel
	// they are held on to and brought over into the new world. In ULevel::InitializeNetworkActors, these PlayerStates may be treated as map/startup actors
	// and given static NetGUIDs. This also causes their deletions to be recorded and sent to new clients, which if unlucky due to name conflicts,
	// may end up deleting the new PlayerStates they had just spaned.
	bNetLoadOnClient = false;

}

void AUTLobbyMatchInfo::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTLobbyMatchInfo, OwnersPlayerState);
	DOREPLIFETIME(AUTLobbyMatchInfo, bPrivateMatch);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchDescription);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchOptions);
	DOREPLIFETIME(AUTLobbyMatchInfo, NumPlayers);
}

void AUTLobbyMatchInfo::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
}

void AUTLobbyMatchInfo::OnRep_MatchOptions()
{
	// TODO: Add code to pass to the UI once I have a UI
	UE_LOG(UT, Log, TEXT("Match Options Changed: %s"), *MatchOptions);
}

void AUTLobbyMatchInfo::BroadcastMatchMessage(AUTLobbyPlayerState* SenderPS, const FString& Message)
{
	for (int32 i = 0; i < LobbyGameState->PlayerArray.Num(); i++)
	{
		AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(LobbyGameState->PlayerArray[i]);
		if (PS && PS->CurrentMatch == this)
		{
			PS->ClientRecieveChat(ChatDestinations::CurrentMatch, SenderPS, Message); 
		}
	}
}