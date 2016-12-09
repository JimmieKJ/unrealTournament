// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPartyGameState.h"
#include "UTGameInstance.h"
#include "UTParty.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineSubsystemUtils.h"

#define LOCTEXT_NAMESPACE "UTParties"

UUTParty::UUTParty(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	PartyClasses.Add(IOnlinePartySystem::GetPrimaryPartyTypeId(), UUTPartyGameState::StaticClass());

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
	}
}

UUTGameInstance* UUTParty::GetUTGameInstance() const
{
	return Cast<UUTGameInstance>(GetOuter());
}

UUTPartyGameState* UUTParty::GetUTPersistentParty() const
{
	return Cast<UUTPartyGameState>(GetPersistentParty());
}

void UUTParty::GetDefaultPersistentPartySettings(EPartyType& PartyType, bool& bLeaderFriendsOnly, bool& bLeaderInvitesOnly, bool& bAllowInvites)
{
	PartyType = EPartyType::Public;
	bLeaderFriendsOnly = false;
	bLeaderInvitesOnly = false;
	bAllowInvites = true;

	UUTGameInstance* UTGameInstance = GetUTGameInstance();
	check(UTGameInstance);
	/*
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(UTGameInstance->GetPrimaryPlayerController());
	if (UTPC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(FortPC->Player);
		if (LP != nullptr &&
			LP->AreClientSettingsInitialized())
		{
			LP->ClientSettingsRecord->GetPartyPrivacySettings(PartyType, bLeaderFriendsOnly, bLeaderInvitesOnly);
		}
	}
	*/
}

void UUTParty::UpdatePersistentPartyLeader(const FUniqueNetIdRepl& NewPartyLeader)
{
	UUTGameInstance* UTGameInstance = GetUTGameInstance();
	check(UTGameInstance);

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(UTGameInstance->GetPrimaryPlayerController());
	if (UTPC)
	{/*
		AUTLobbyBeaconClient* LobbyBeaconClient = UTGameInstance->GetLobbyBeaconClient();
		if (LobbyBeaconClient)
		{
			LobbyBeaconClient->SetPartyOwnerId(UTPC->PlayerState->UniqueId, NewPartyLeader);
		}
		*/
		//UTPC->ClientRegisterWithParty();
	}
}

void UUTParty::HandlePendingJoin()
{
	ensure(PendingPartyJoin.IsValid());

	TSharedPtr<FPendingPartyJoin> PendingPersistentPartyJoin = StaticCastSharedPtr<FPendingPartyJoin>(PendingPartyJoin);

	if (PendingPersistentPartyJoin->PartyDetails->IsValid())
	{
		UUTGameInstance* UTGameInstance = GetUTGameInstance();
		check(UTGameInstance);

		APlayerController* OwningController = Cast<APlayerController>(UTGameInstance->GetPrimaryPlayerController());
		if (ensure(OwningController))
		{
			// Handle things now
			ULocalPlayer* LP = Cast<ULocalPlayer>(OwningController->Player);
			check(LP);

			// join the party
			TSharedPtr<const FOnlinePartyId> CurrentPersistentPartyId = GetPersistentPartyId();
			if (CurrentPersistentPartyId.IsValid())
			{
				if (*CurrentPersistentPartyId != *PendingPersistentPartyJoin->PartyDetails->GetPartyId())
				{
					UPartyDelegates::FOnLeaveUPartyComplete LeavePartyCompletionDelegate;
					LeavePartyCompletionDelegate.BindUObject(this, &UUTParty::OnLeavePartyForNewJoin, PendingPartyJoin.ToSharedRef());
					LeavePersistentParty(*(PendingPersistentPartyJoin->LocalUserId), LeavePartyCompletionDelegate);
				}
				else
				{
					UE_LOG(LogParty, Warning, TEXT("Can't join party %s twice, ignoring."), *CurrentPersistentPartyId->ToString());
				}
			}
			else
			{
				JoinPersistentParty(*(PendingPersistentPartyJoin->LocalUserId), *PendingPersistentPartyJoin->PartyDetails, PendingPersistentPartyJoin->Delegate);
			}

			PendingPartyJoin.Reset();

			// All handling of invites should have been successful in some way (either processing now, or storing for later)
		}
		else
		{
			// Bad case with no controller, should never happen, trigger delegate for UI to handle failure
			PendingPersistentPartyJoin->Delegate.ExecuteIfBound(*(PendingPersistentPartyJoin->LocalUserId), EJoinPartyCompletionResult::UnknownClientFailure, 0);
		}
	}
	else
	{
		if (!PendingPersistentPartyJoin->PartyDetails->PartyJoinInfo->IsAcceptingMembers())
		{
			PendingPersistentPartyJoin->Delegate.ExecuteIfBound(*(PendingPersistentPartyJoin->LocalUserId), EJoinPartyCompletionResult::NotApproved, PendingPersistentPartyJoin->PartyDetails->PartyJoinInfo->GetNotAcceptingReason());
			PendingPartyJoin.Reset();
		}
		else
		{
			// Bad party join details, trigger delegate for UI to handle failure
			PendingPersistentPartyJoin->Delegate.ExecuteIfBound(*(PendingPersistentPartyJoin->LocalUserId), EJoinPartyCompletionResult::JoinInfoInvalid, 0);
			PendingPartyJoin.Reset();
		}
	}
}

void UUTParty::AddPendingJoin(TSharedRef<const FUniqueNetId> InLocalUserId, TSharedRef<const FPartyDetails> InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InDelegate)
{
	PendingPartyJoin = MakeShareable(new FPendingPartyJoin(InLocalUserId, InPartyDetails, InDelegate));
}

void UUTParty::OnLeavePartyForNewJoin(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, TSharedRef<FPendingPartyJoin> InPendingPartyJoin)
{
	if (Result == ELeavePartyCompletionResult::Succeeded)
	{
		JoinPersistentParty(LocalUserId, *InPendingPartyJoin->PartyDetails, InPendingPartyJoin->Delegate);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Failed to leave existing party to join new party - %s"), ToString(Result));
		InPendingPartyJoin->Delegate.ExecuteIfBound(LocalUserId, EJoinPartyCompletionResult::UnknownClientFailure, 0);
	}
}

void UUTParty::ProcessInviteFromSearchResult(TSharedPtr< const FUniqueNetId > UserId, const FOnlineSessionSearchResult & InviteResult)
{
	if (UserId.IsValid())
	{
		UUTGameInstance* UTGameInstance = GetUTGameInstance();
		check(UTGameInstance);

		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(UTGameInstance->GetWorld());
		check(PartyInt.IsValid());

		FString JoinInfoJson;
		InviteResult.Session.SessionSettings.Get(SETTING_CUSTOM, JoinInfoJson);

		TSharedPtr<IOnlinePartyJoinInfo> JoinInfo = PartyInt->MakeJoinInfoFromJson(JoinInfoJson);

		if (JoinInfo.IsValid())
		{
			TSharedRef<const FPartyDetails> PartyDetails = MakeShareable(new FPartyDetails(JoinInfo.ToSharedRef(), true));
			PendingPartyJoin = MakeShareable(new FPendingPartyJoin(UserId.ToSharedRef(), PartyDetails, UPartyDelegates::FOnJoinUPartyComplete::CreateUObject(this, &ThisClass::OnJoinPersistentPartyFromInviteComplete)));
			HandlePendingJoin();
		}
	}
}

void UUTParty::OnJoinPersistentPartyFromInviteComplete(const FUniqueNetId& LocalUserId, const EJoinPartyCompletionResult Result, const int32 NotApprovedReason)
{
	FString PersistentPartyIdString = "Invalid";
	if (GetPersistentPartyId().IsValid())
	{
		PersistentPartyIdString = GetPersistentPartyId()->ToString();
	}
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnJoinPersistentPartyFromInviteComplete result %d for user %s with party ID %s"), (int32)Result, *LocalUserId.ToString(), *PersistentPartyIdString);

	OnPartyJoinComplete().Broadcast(LocalUserId, Result, NotApprovedReason);
}

void UUTParty::SetSession(const FOnlineSessionSearchResult& InSearchResult)
{
	if (GetUTPersistentParty())
	{
		GetUTPersistentParty()->SetSession(InSearchResult);
	}
}

#undef LOCTEXT_NAMESPACE
