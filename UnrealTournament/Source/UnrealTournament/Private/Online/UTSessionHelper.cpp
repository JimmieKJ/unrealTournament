// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTSessionHelper.h"
#include "UTGameInstance.h"
#include "UTOnlineGameSettings.h"
#include "Online/UTParty.h"
#include "Online/UTPartyGameState.h"
#include "Online/UTPartyBeaconClient.h"
#include "OnlineSubsystemUtils.h"
#include "PartyMemberState.h"

void BuildPartyMembers(UWorld* World, const FUniqueNetIdRepl& PartyLeader, TArray<FPlayerReservation>& PartyMembers)
{
	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		FPlayerReservation PlayerRes;

		PlayerRes.UniqueId = PartyLeader.GetUniqueNetId();
		TSharedPtr<FUserOnlineAccount> UserAcct = IdentityInt->GetUserAccount(*PlayerRes.UniqueId);
		if (UserAcct.IsValid())
		{
			PlayerRes.ValidationStr = UserAcct->GetAccessToken();
		}

		PartyMembers.Add(PlayerRes);

		UE_LOG(LogOnlineGame, Verbose, TEXT("Adding Party Leader UniqueId=%s Validation=%s"),
			*PlayerRes.UniqueId->ToDebugString(),
			*PlayerRes.ValidationStr);

		UUTGameInstance* UTGameInstance = Cast<UUTGameInstance>(World->GetGameInstance());
		if (UTGameInstance)
		{
			UUTParty* UTParties = UTGameInstance->GetParties();
			if (UTParties)
			{
				UPartyGameState* Party = UTParties->GetPersistentParty();
				if (Party)
				{
					TArray<UPartyMemberState*> AllPartyMembers;
					Party->GetAllPartyMembers(AllPartyMembers);

					for (const UPartyMemberState* PartyMember : AllPartyMembers)
					{
						if (PartyMember->UniqueId != PartyLeader)
						{
							PlayerRes.UniqueId = PartyMember->UniqueId;
							PlayerRes.ValidationStr.Empty();
							PartyMembers.Add(PlayerRes);

							UE_LOG(LogOnlineGame, Verbose, TEXT("Adding Party Member UniqueId=%s"),
								*PlayerRes.UniqueId->ToDebugString(),
								*PlayerRes.ValidationStr);
						}
					}
				}
			}
		}
	}
}

UUTSessionHelper::UUTSessionHelper(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	PartyBeaconClient(nullptr),
	CurrentLocalUserId(nullptr),
	CurrentSessionName(NAME_None),
	LastBeaconResponse(EPartyReservationResult::GeneralError),
	CurrentJoinState(EUTSessionHelperJoinState::NotJoining),
	CurrentJoinResult(EUTSessionHelperJoinResult::NoResult)
{
	BeaconClientClass = AUTPartyBeaconClient::StaticClass();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
	}
}

void UUTSessionHelper::Reset()
{
	CurrentLocalUserId = nullptr;
	CurrentSessionName = NAME_None;
	CurrentSessionToJoin = FOnlineSessionSearchResult();

	CurrentJoinState = EUTSessionHelperJoinState::NotJoining;
	CurrentJoinResult = EUTSessionHelperJoinResult::NoResult;
	LastBeaconResponse = EPartyReservationResult::NoResult;

	SessionHelperStateChange.Unbind();
	ReserveSessionCompleteDelegate.Unbind();
	JoinReservedSessionCompleteDelegate.Unbind();
}

UWorld* UUTSessionHelper::GetWorld() const
{
	check(GEngine);
	check(GEngine->GameViewport);
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	return GameInstance->GetWorld();
}

void UUTSessionHelper::DestroyClientBeacons()
{
	if (PartyBeaconClient)
	{
		PartyBeaconClient->OnReservationCountUpdate().Unbind();
		PartyBeaconClient->OnReservationRequestComplete().Unbind();
		PartyBeaconClient->OnHostConnectionFailure().Unbind();
		PartyBeaconClient->DestroyBeacon();
		PartyBeaconClient = nullptr;
	}
}

void UUTSessionHelper::HandleFailedReservation()
{
	CurrentJoinResult = EUTSessionHelperJoinResult::ReservationFailure;
	ReserveSessionCompleteDelegate.ExecuteIfBound(CurrentJoinResult);
}

void UUTSessionHelper::HandleWaitingOnGame()
{
	CurrentJoinResult = EUTSessionHelperJoinResult::ReservationSuccess;
	ReserveSessionCompleteDelegate.ExecuteIfBound(CurrentJoinResult);
}

void UUTSessionHelper::HandleJoiningSession()
{
	CurrentJoinResult = EUTSessionHelperJoinResult::JoinSessionSuccess;
	JoinReservedSessionCompleteDelegate.ExecuteIfBound(CurrentJoinResult);
}

void UUTSessionHelper::HandleFailedJoin()
{
	CurrentJoinResult = EUTSessionHelperJoinResult::JoinSessionFailure;
	JoinReservedSessionCompleteDelegate.ExecuteIfBound(CurrentJoinResult);
}

void UUTSessionHelper::ProcessJoinStateChange(EUTSessionHelperJoinState::Type NewState)
{
	if (NewState != CurrentJoinState)
	{
		UE_LOG(LogOnlineGame, Display, TEXT("Join state change %s -> %s"), EUTSessionHelperJoinState::ToString(CurrentJoinState), EUTSessionHelperJoinState::ToString(NewState));

		EUTSessionHelperJoinState::Type OldState = CurrentJoinState;
		CurrentJoinState = NewState;

		switch (CurrentJoinState)
		{
		case EUTSessionHelperJoinState::NotJoining:
			break;
		case EUTSessionHelperJoinState::RequestingReservation:
			break;
		case EUTSessionHelperJoinState::FailedReservation:
			HandleFailedReservation();
			break;
		case EUTSessionHelperJoinState::WaitingOnGame:
			HandleWaitingOnGame();
			break;
		case EUTSessionHelperJoinState::AttemptingJoin:
			break;
		case EUTSessionHelperJoinState::JoiningSession:
			HandleJoiningSession();
			break;
		case EUTSessionHelperJoinState::FailedJoin:
			HandleFailedJoin();
			break;
		default:
			break;
		}	

		SessionHelperStateChange.ExecuteIfBound(OldState, NewState);
	}
}

bool UUTSessionHelper::IsJoining()
{
	// All other states mean active joined or a failed state that require a Reset() before proceeding
	return CurrentJoinState != EUTSessionHelperJoinState::NotJoining;
}

void UUTSessionHelper::ReserveSession(TSharedPtr<const FUniqueNetId>& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& SessionToJoin, FOnUTReserveSessionComplete& InCompletionDelegate)
{
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ThisClass::ReserveSessionInternal);
	ReserveSessionCommon(LocalUserId, SessionName, SessionToJoin, InCompletionDelegate, TimerDelegate);
}

void UUTSessionHelper::ReserveEmptySession(TSharedPtr<const FUniqueNetId>& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& SessionToJoin, const FEmptyServerReservation& EmptyReservation, FOnUTReserveSessionComplete& InCompletionDelegate)
{
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ThisClass::ReserveEmptySessionInternal, EmptyReservation);
	ReserveSessionCommon(LocalUserId, SessionName, SessionToJoin, InCompletionDelegate, TimerDelegate);
}

void UUTSessionHelper::ReserveSessionCommon(TSharedPtr<const FUniqueNetId>& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& SessionToJoin, FOnUTReserveSessionComplete& InCompletionDelegate, FTimerDelegate& TimerDelegate)
{
	// Detect reuse and reset
	if (CurrentJoinState == EUTSessionHelperJoinState::FailedReservation || 
		CurrentJoinState == EUTSessionHelperJoinState::FailedJoin ||
		CurrentJoinState == EUTSessionHelperJoinState::JoiningSession)
	{
		Reset();
	}

	if (LocalUserId.IsValid() && SessionToJoin.IsValid() && !IsJoining())
	{
		UWorld* World = GetWorld();
		check(World);

		IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World);
		if (OnlineSub)
		{
			Reset();

			CurrentLocalUserId = LocalUserId;
			CurrentSessionName = SessionName;
			CurrentSessionToJoin = SessionToJoin;
			ReserveSessionCompleteDelegate = InCompletionDelegate;

			ProcessJoinStateChange(EUTSessionHelperJoinState::RequestingReservation);

			if (PartyBeaconClient)
			{
				DestroyClientBeacons();

				FTimerHandle ReserveSessionInternalTimerHandle;
				World->GetTimerManager().SetTimer(ReserveSessionInternalTimerHandle, TimerDelegate, NEXT_RESERVATION_REQUEST_DELAY, false);
			}
			else
			{
				TimerDelegate.Execute();
			}
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("No online subsystem."));
			ProcessJoinStateChange(EUTSessionHelperJoinState::FailedReservation);
		}
	}
	else
	{
		// Call the input delegate directly here because we don't want to interrupt what is already going on
		UE_LOG(LogOnlineGame, Verbose, TEXT("Already called JoinSession, ignoring."));
		InCompletionDelegate.ExecuteIfBound(EUTSessionHelperJoinResult::ReservationFailure);
	}
}

void UUTSessionHelper::ReserveSessionInternal()
{
	UWorld* World = GetWorld();
	check(World);

	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		PartyBeaconClient = World->SpawnActor<AUTPartyBeaconClient>(BeaconClientClass);
		UE_LOG(LogOnlineGame, Verbose, TEXT("Creating client reservation beacon %s."), *PartyBeaconClient->GetName());

		FUniqueNetIdRepl PartyLeader(CurrentLocalUserId);
		TArray<FPlayerReservation> PartyMembers;
		BuildPartyMembers(World, PartyLeader, PartyMembers);

		PartyBeaconClient->OnReservationCountUpdate().BindUObject(this, &ThisClass::OnReservationCountUpdate);
		PartyBeaconClient->OnReservationRequestComplete().BindUObject(this, &ThisClass::OnReservationRequestComplete);
		PartyBeaconClient->OnHostConnectionFailure().BindUObject(this, &ThisClass::OnHostConnectionFailure);
		PartyBeaconClient->RequestReservation(CurrentSessionToJoin, PartyLeader, PartyMembers);
	}
	else
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("No online subsystem."));
		ProcessJoinStateChange(EUTSessionHelperJoinState::FailedReservation);
	}
}

void UUTSessionHelper::ReserveEmptySessionInternal(FEmptyServerReservation ReservationData)
{
	UWorld* World = GetWorld();

	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		PartyBeaconClient = World->SpawnActor<AUTPartyBeaconClient>(BeaconClientClass);
		UE_LOG(LogOnlineGame, Verbose, TEXT("Creating empty reservation beacon %s."), *PartyBeaconClient->GetName());

		FUniqueNetIdRepl PartyLeader(CurrentLocalUserId);
		TArray<FPlayerReservation> PartyMembers;
		BuildPartyMembers(World, PartyLeader, PartyMembers);

		PartyBeaconClient->OnReservationCountUpdate().BindUObject(this, &ThisClass::OnReservationCountUpdate);
		PartyBeaconClient->OnReservationRequestComplete().BindUObject(this, &ThisClass::OnReservationRequestComplete);
		PartyBeaconClient->OnHostConnectionFailure().BindUObject(this, &ThisClass::OnHostConnectionFailure);

		PartyBeaconClient->EmptyServerReservationRequest(CurrentSessionToJoin, ReservationData, PartyLeader, PartyMembers);
	}
	else
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("No online subsystem."));
		ProcessJoinStateChange(EUTSessionHelperJoinState::FailedReservation);
	}
}

void UUTSessionHelper::SkipReservation(TSharedPtr<const FUniqueNetId>& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& SessionToJoin, FOnUTReserveSessionComplete& InCompletionDelegate)
{
	Reset();

	if (LocalUserId.IsValid() && SessionToJoin.IsValid() && !IsJoining())
	{
		UWorld* World = GetWorld();
		check(World);

		IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World);
		if (OnlineSub)
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Skipping reservation."));
			CurrentLocalUserId = LocalUserId;
			CurrentSessionName = SessionName;
			CurrentSessionToJoin = SessionToJoin;
			ReserveSessionCompleteDelegate = InCompletionDelegate;
			
			LastBeaconResponse = EPartyReservationResult::ReservationAccepted;
			ProcessJoinStateChange(EUTSessionHelperJoinState::WaitingOnGame);
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("No online subsystem."));
			ProcessJoinStateChange(EUTSessionHelperJoinState::FailedReservation);
		}
	}
	else
	{
		// Call the input delegate directly here because we don't want to interrupt what is already going on
		UE_LOG(LogOnlineGame, Verbose, TEXT("Already called JoinSession, ignoring."));
		InCompletionDelegate.ExecuteIfBound(EUTSessionHelperJoinResult::ReservationFailure);
	}
}

void UUTSessionHelper::OnReservationRequestComplete(EPartyReservationResult::Type ReservationResult)
{
	DestroyClientBeacons();

	LastBeaconResponse = ReservationResult;

	if (CurrentJoinState == EUTSessionHelperJoinState::RequestingReservation)
	{
		UWorld* World = GetWorld();
		check(World);

		if (ReservationResult == EPartyReservationResult::ReservationAccepted ||
			ReservationResult == EPartyReservationResult::ReservationDuplicate)
		{
			ProcessJoinStateChange(EUTSessionHelperJoinState::WaitingOnGame);
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Reservation request rejected with %s"), EPartyReservationResult::ToString(ReservationResult));
			ProcessJoinStateChange(EUTSessionHelperJoinState::FailedReservation);
		}
	}
	else
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("Reservation request complete while not in a joining state."));
	}
}

void UUTSessionHelper::OnReservationCountUpdate(int32 NumRemaining)
{
}

void UUTSessionHelper::OnHostConnectionFailure()
{
	OnReservationRequestComplete(EPartyReservationResult::RequestTimedOut);
}

void UUTSessionHelper::CancelReservation()
{
	if (PartyBeaconClient)
	{
		PartyBeaconClient->CancelReservation();
		DestroyClientBeacons();
	}
	else
	{
		OnReservationRequestComplete(EPartyReservationResult::ReservationRequestCanceled);
	}
}

void UUTSessionHelper::JoinReservedSession(FOnUTJoinSessionComplete& InCompletionDelegate)
{
	UWorld* World = GetWorld();
	check(World);

	if (CurrentJoinState == EUTSessionHelperJoinState::WaitingOnGame)
	{
		JoinReservedSessionCompleteDelegate = InCompletionDelegate;

		IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
		if (SessionInt.IsValid() && CurrentLocalUserId.IsValid() && CurrentSessionName != NAME_None && CurrentSessionToJoin.IsValid())
		{
			ProcessJoinStateChange(EUTSessionHelperJoinState::AttemptingJoin);

			FOnJoinSessionCompleteDelegate CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnJoinReservedSessionComplete);
			JoinReservedSessionCompleteDelegateHandle = SessionInt->AddOnJoinSessionCompleteDelegate_Handle(CompletionDelegate);
			SessionInt->JoinSession(*CurrentLocalUserId, CurrentSessionName, CurrentSessionToJoin);
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Bad reservation data at join session."));
			ProcessJoinStateChange(EUTSessionHelperJoinState::FailedJoin);
		}
	}
	else
	{
		// In a bad state here, but call the delegate to proceed
		UE_LOG(LogOnlineGame, Verbose, TEXT("Already called JoinSession, ignoring."));
		InCompletionDelegate.ExecuteIfBound(EUTSessionHelperJoinResult::JoinSessionFailure);
	}
}

void UUTSessionHelper::OnJoinReservedSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnJoinSessionComplete %s bSuccess: %d"), *SessionName.ToString(), (int32)Result);

	UWorld* World = GetWorld();
	check(World);

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnJoinSessionCompleteDelegate_Handle(JoinReservedSessionCompleteDelegateHandle);
	}

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		ProcessJoinStateChange(EUTSessionHelperJoinState::JoiningSession);
	}
	else
	{
		ProcessJoinStateChange(EUTSessionHelperJoinState::FailedJoin);
	}
}