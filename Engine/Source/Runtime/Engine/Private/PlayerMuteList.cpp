// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "OnlineSubsystemUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerState.h"

static inline void AddIdToMuteList(TArray< TSharedRef<const FUniqueNetId> >& MuteList, const TSharedPtr<const FUniqueNetId>& UniqueIdToAdd)
{
	FUniqueNetIdMatcher UniqueIdToAddMatch(*UniqueIdToAdd);
	if (MuteList.IndexOfByPredicate(UniqueIdToAddMatch) == INDEX_NONE)
	{
		MuteList.Add(UniqueIdToAdd.ToSharedRef());
	}
}

static inline void RemoveIdFromMuteList(TArray< TSharedRef<const FUniqueNetId> >& MuteList, const TSharedPtr<const FUniqueNetId>& UniqueIdToRemove)
{
	FUniqueNetIdMatcher UniqueIdToRemoveMatch(*UniqueIdToRemove);
	int32 RemoveIndex = MuteList.IndexOfByPredicate(UniqueIdToRemoveMatch);
	if (RemoveIndex != INDEX_NONE)
	{
		MuteList.RemoveAtSwap(RemoveIndex);
	}
}

void FPlayerMuteList::ServerMutePlayer(APlayerController* OwningPC, const FUniqueNetIdRepl& MuteId)
{
	UWorld* World = OwningPC->GetWorld();

	const TSharedPtr<const FUniqueNetId>& PlayerIdToMute = MuteId.GetUniqueNetId();

	// Don't reprocess if they are already muted
	AddIdToMuteList(VoiceMuteList, PlayerIdToMute);

	// Add them to the packet filter list if not already on it
	AddIdToMuteList(VoicePacketFilter, PlayerIdToMute);

	// Replicate mute state to client
	OwningPC->ClientMutePlayer(MuteId);

	// Find the muted player's player controller so it can be notified
	APlayerController* OtherPC = GetPlayerControllerFromNetId(World, *PlayerIdToMute);
	if (OtherPC != NULL)
	{
		// Update their packet filter list too
		OtherPC->MuteList.ClientMutePlayer(OtherPC, OwningPC->PlayerState->UniqueId);

		// Tell the other PC to mute this one
		OtherPC->ClientMutePlayer(OwningPC->PlayerState->UniqueId);
	}
}

void FPlayerMuteList::ServerUnmutePlayer(APlayerController* OwningPC, const FUniqueNetIdRepl& UnmuteId)
{
	UWorld* World = OwningPC->GetWorld();

	const TSharedPtr<const FUniqueNetId>& PlayerIdToUnmute = UnmuteId.GetUniqueNetId();

	// If the player was found, remove them from our explicit list
	RemoveIdFromMuteList(VoiceMuteList, PlayerIdToUnmute);

	// Find the muted player's player controller so it can be notified
	APlayerController* OtherPC = GetPlayerControllerFromNetId(World, *PlayerIdToUnmute);
	if (OtherPC != NULL)
	{
		FUniqueNetIdMatcher PlayerIdToUnmuteMatch(*PlayerIdToUnmute);
		FUniqueNetIdMatcher OwningPlayerIdMatch(*OwningPC->PlayerState->UniqueId);

		// Make sure this player isn't muted for gameplay reasons
		if (GameplayVoiceMuteList.IndexOfByPredicate(PlayerIdToUnmuteMatch) == INDEX_NONE &&
			// And make sure they didn't mute us
			OtherPC->MuteList.VoiceMuteList.IndexOfByPredicate(OwningPlayerIdMatch) == INDEX_NONE)
		{
			OwningPC->ClientUnmutePlayer(UnmuteId);
		}

		// If the other player doesn't have this player muted
		if (OtherPC->MuteList.VoiceMuteList.IndexOfByPredicate(OwningPlayerIdMatch) == INDEX_NONE &&
			OtherPC->MuteList.GameplayVoiceMuteList.IndexOfByPredicate(OwningPlayerIdMatch) == INDEX_NONE)
		{
			// Remove them from the packet filter list
			RemoveIdFromMuteList(VoicePacketFilter, PlayerIdToUnmute);

			// If found, remove so packets flow to that client too
			RemoveIdFromMuteList(OtherPC->MuteList.VoicePacketFilter, OwningPC->PlayerState->UniqueId.GetUniqueNetId());

			// Tell the other PC to unmute this one
			OtherPC->ClientUnmutePlayer(OwningPC->PlayerState->UniqueId);
		}
	}
}

void FPlayerMuteList::ClientMutePlayer(APlayerController* OwningPC, const FUniqueNetIdRepl& MuteId)
{
	const TSharedPtr<const FUniqueNetId>& PlayerIdToMute = MuteId.GetUniqueNetId();

	// Add to the filter list on clients (used for peer to peer voice)
	AddIdToMuteList(VoicePacketFilter, PlayerIdToMute);

	// Use the local player to determine the controller id
	ULocalPlayer* LP = Cast<ULocalPlayer>(OwningPC->Player);
	if (LP != NULL)
	{
		UWorld* World = OwningPC->GetWorld();
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(World);
		if (VoiceInt.IsValid())
		{
			// Have the voice subsystem mute this player
			VoiceInt->MuteRemoteTalker(LP->GetControllerId(), *PlayerIdToMute, false);
		}
	}
}

void FPlayerMuteList::ClientUnmutePlayer(APlayerController* OwningPC, const FUniqueNetIdRepl& UnmuteId)
{
	const TSharedPtr<const FUniqueNetId>& PlayerIdToUnmute = UnmuteId.GetUniqueNetId();

	// It's safe to remove them from the filter list on clients (used for peer to peer voice)
	RemoveIdFromMuteList(VoicePacketFilter, PlayerIdToUnmute);

	// Use the local player to determine the controller id
	ULocalPlayer* LP = Cast<ULocalPlayer>(OwningPC->Player);
	if (LP != NULL)
	{
		UWorld* World = OwningPC->GetWorld();
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(World);
		if (VoiceInt.IsValid())
		{
			// Have the voice subsystem mute this player
			VoiceInt->UnmuteRemoteTalker(LP->GetControllerId(), *PlayerIdToUnmute, false);
		}
	}
}

void FPlayerMuteList::GameplayMutePlayer(APlayerController* OwningPC, const FUniqueNetIdRepl& MuteId)
{
	const TSharedPtr<const FUniqueNetId>& PlayerIdToMute = MuteId.GetUniqueNetId();

	// Don't add if already muted
	AddIdToMuteList(GameplayVoiceMuteList, PlayerIdToMute);

	// Add to the filter list, if missing
	AddIdToMuteList(VoicePacketFilter, PlayerIdToMute);

	// Now process on the client
	OwningPC->ClientMutePlayer(MuteId);
}

void FPlayerMuteList::GameplayUnmutePlayer(APlayerController* OwningPC, const FUniqueNetIdRepl& UnmuteId)
{
	UWorld* World = OwningPC->GetWorld();

	const TSharedPtr<const FUniqueNetId>& PlayerIdToUnmute = UnmuteId.GetUniqueNetId();

	// Remove from the gameplay mute list
	RemoveIdFromMuteList(GameplayVoiceMuteList, PlayerIdToUnmute);

	// Find the muted player's player controller so it can be notified
	APlayerController* OtherPC = GetPlayerControllerFromNetId(World, *PlayerIdToUnmute);
	if (OtherPC != NULL)
	{
		FUniqueNetIdMatcher PlayerIdToUnmuteMatch(*PlayerIdToUnmute);
		FUniqueNetIdMatcher OwningPlayerIdMatch(*OwningPC->PlayerState->UniqueId);

		// Make sure this player isn't explicitly muted
		if (VoiceMuteList.IndexOfByPredicate(PlayerIdToUnmuteMatch) == INDEX_NONE &&
			// And make sure they didn't mute us
			OtherPC->MuteList.VoiceMuteList.IndexOfByPredicate(OwningPlayerIdMatch) == INDEX_NONE)
		{
			RemoveIdFromMuteList(VoicePacketFilter, PlayerIdToUnmute);

			// Now process on the client
			OwningPC->ClientUnmutePlayer(UnmuteId);	
		}
	}
}

bool FPlayerMuteList::IsPlayerMuted(const FUniqueNetId& PlayerId)
{
	FUniqueNetIdMatcher PlayerIdMatch(PlayerId);
	return VoicePacketFilter.IndexOfByPredicate(PlayerIdMatch) != INDEX_NONE;
}

FString DumpMutelistState(UWorld* World)
{
	FString Output = TEXT("Muting state\n");

	if (World)
	{
		for(FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = *Iterator;
			Output += FString::Printf(TEXT("Player: %s\n"), PlayerController->PlayerState ? *PlayerController->PlayerState->PlayerName : TEXT("NONAME"));
			Output += FString::Printf(TEXT("VoiceChannel: %d\n"), PlayerController->MuteList.VoiceChannelIdx);
			Output += FString::Printf(TEXT("Handshake: %s\n"), PlayerController->MuteList.bHasVoiceHandshakeCompleted ? TEXT("true") : TEXT("false"));
			
			Output += FString(TEXT("System mutes:\n"));
			for (int32 idx=0; idx < PlayerController->MuteList.VoiceMuteList.Num(); idx++)
			{
				Output += FString::Printf(TEXT("%s\n"), *PlayerController->MuteList.VoiceMuteList[idx]->ToString());
			}

			Output += FString(TEXT("Gameplay mutes:\n"));
			for (int32 idx=0; idx < PlayerController->MuteList.GameplayVoiceMuteList.Num(); idx++)
			{
				Output += FString::Printf(TEXT("%s\n"), *PlayerController->MuteList.GameplayVoiceMuteList[idx]->ToString());
			}

			Output += FString(TEXT("Filter:\n"));
			for (int32 idx=0; idx < PlayerController->MuteList.VoicePacketFilter.Num(); idx++)
			{
				Output += FString::Printf(TEXT("%s\n"), *PlayerController->MuteList.VoicePacketFilter[idx]->ToString());
			}

			Output += TEXT("\n");
		}
	}

	return Output;
}



