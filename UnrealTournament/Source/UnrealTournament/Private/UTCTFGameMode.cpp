// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFGameMode.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTCountDownMessage.h"
#include "UTPickup.h"
#include "UTGameMessage.h"
#include "UTMutator.h"
#include "UTCTFSquadAI.h"
#include "UTWorldSettings.h"
#include "Widgets/SUTTabWidget.h"
#include "Dialogs/SUTPlayerInfoDialog.h"
#include "StatNames.h"
#include "Engine/DemoNetDriver.h"
#include "UTCTFScoreboard.h"
#include "SNumericEntryBox.h"

AUTCTFGameMode::AUTCTFGameMode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	IntermissionDuration = 30.f;
	bAllowOvertime = true;
	AdvantageDuration = 5;
	MercyScore = 5;
	GoalScore = 0;
	TimeLimit = 14;
	QuickPlayersToStart = 8;

	DisplayName = NSLOCTEXT("UTGameMode", "CTF", "Capture the Flag");
}

void AUTCTFGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// IntermissionDuration is in seconds and used in seconds,
	IntermissionDuration = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("HalftimeDuration"), IntermissionDuration));
	if (bOfflineChallenge)
	{
		TimeLimit = 600;
	}
	if (TimeLimit > 0)
	{
		TimeLimit = uint32(float(TimeLimit) * 0.5f);
	}
}

float AUTCTFGameMode::GetTravelDelay()
{
	return Super::GetTravelDelay() + (CTFGameState ? FMath::Max(6.f, 1.f + CTFGameState->GetScoringPlays().Num()) : 6.f);
}

void AUTCTFGameMode::ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	Super::ScoreObject_Implementation(GameObject, HolderPawn, Holder, Reason);

	if (Holder != NULL && Holder->Team != NULL && !CTFGameState->HasMatchEnded() && !CTFGameState->IsMatchIntermission())
	{
		if (Reason == FName("SentHome"))
		{
			// if all flags are returned, end advantage time right away
			if (CTFGameState->bPlayingAdvantage)
			{
				bool bAllHome = true;
				for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
				{
					if (Base != NULL && Base->GetCarriedObjectState() != CarriedObjectState::Home)
					{
						bAllHome = false;
						break;
					}
				}
				if (bAllHome)
				{
					EndOfHalf();
				}
			}
		}
	}
}

void AUTCTFGameMode::HandleFlagCapture(AUTPlayerState* Holder)
{
	if (CTFGameState->IsMatchInOvertime())
	{
		EndGame(Holder, FName(TEXT("GoldenCap")));
	}
	else
	{
		CheckScore(Holder);
		// any cap ends advantage time immediately
		// warning: CheckScore() could have ended the match already
		if (CTFGameState->bPlayingAdvantage && !CTFGameState->HasMatchEnded())
		{
			EndOfHalf();
		}
	}
}

bool AUTCTFGameMode::CheckScore_Implementation(AUTPlayerState* Scorer)
{
	if (Scorer->Team != NULL)
	{
		if (GoalScore > 0 && Scorer->Team->Score >= GoalScore)
		{
			EndGame(Scorer, FName(TEXT("scorelimit")));
		}
		else if (MercyScore > 0)
		{
			int32 Spread = Scorer->Team->Score;
			for (AUTTeamInfo* OtherTeam : Teams)
			{
				if (OtherTeam != Scorer->Team)
				{
					Spread = FMath::Min<int32>(Spread, Scorer->Team->Score - OtherTeam->Score);
				}
			}
			if (Spread >= MercyScore)
			{
				EndGame(Scorer, FName(TEXT("MercyScore")));
			}
		}
	}

	return true;
}

void AUTCTFGameMode::CheckGameTime()
{
	Super::CheckGameTime();

	if ( CTFGameState->IsMatchInProgress() && (TimeLimit != 0) )
	{
		// If the match is in progress and we are not playing advantage, then enter the halftime/end of game logic depending on the half
		if (CTFGameState->RemainingTime <= 0)
		{
			if (!CTFGameState->IsMatchInOvertime() && CTFGameState->bSecondHalf && bAllowOvertime)
			{
				EndOfHalf();
			}
			if (!CTFGameState->bPlayingAdvantage)
			{
				// If we are in Overtime - Keep battling until one team wins. 
				if (CTFGameState->IsMatchInOvertime())
				{
					AUTTeamInfo* WinningTeam = CTFGameState->FindLeadingTeam();
					if ( WinningTeam != NULL )
					{
						// Match is over....
						AUTPlayerState* WinningPS = FindBestPlayerOnTeam(WinningTeam->GetTeamNum());
						EndGame(WinningPS, FName(TEXT("TimeLimit")));	
					}
				}
				// We are in normal time, so figure out what to do...
				else
				{
					// Look to see if we should be playing advantage
					if (!CTFGameState->bPlayingAdvantage)
					{
						int32 AdvantageTeam = TeamWithAdvantage();
						if (AdvantageTeam >= 0 && AdvantageTeam <= 1)
						{
							// A team has advantage.. so set the flags.
							CTFGameState->bPlayingAdvantage = true;
							CTFGameState->AdvantageTeamIndex = AdvantageTeam;
							CTFGameState->SetTimeLimit(60);
							RemainingAdvantageTime = AdvantageDuration;

							// Broadcast the Advantage Message....
							BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 6, NULL, NULL, CTFGameState->Teams[AdvantageTeam]);

							return;
						}
					}

					// If we still aren't playing advantage, handle halftime, etc.
					if (!CTFGameState->bPlayingAdvantage)
					{
						EndOfHalf();
					}
				}
			}
			// Advantage Time ran out
			else
			{
				EndOfHalf();
			}
		}
		// If we are playing advantage, we need to check to see if we should be playing advantage
		else if (CTFGameState->bPlayingAdvantage)
		{
			// Look to see if we should still be playing advantage
			if (!CheckAdvantage())
			{
				EndOfHalf();
			}
		}
	}
}

void AUTCTFGameMode::EndOfHalf()
{
	// Handle possible end of game at the end of the second half
	if (CTFGameState->bSecondHalf)
	{
		// Look to see if there is a winning team
		AUTTeamInfo* WinningTeam = CTFGameState->FindLeadingTeam();
		if (WinningTeam != NULL)
		{
			// Game Over... yeh!
			AUTPlayerState* WinningPS = FindBestPlayerOnTeam(WinningTeam->GetTeamNum());
			EndGame(WinningPS, FName(TEXT("TimeLimit")));
		}

		// Otherwise look to see if we should enter overtime
		else
		{
			if (bAllowOvertime)
			{
				CTFGameState->bPlayingAdvantage = false;
				SetMatchState(MatchState::MatchIsInOvertime);
			}
			else
			{
				// Match is over....
				EndGame(NULL, FName(TEXT("TimeLimit")));
			}
		}
	}

	// Time expired and noone has advantage so enter the second half.
	else
	{
		SetMatchState(MatchState::MatchIntermission);
	}
}

void AUTCTFGameMode::HandleMatchHasStarted()
{
	if (!CTFGameState->bSecondHalf)
	{
		Super::HandleMatchHasStarted();
	}
}

void AUTCTFGameMode::HandleMatchIntermission()
{
	Super::HandleMatchIntermission();

	CTFGameState->bPlayingAdvantage = false;
	CTFGameState->AdvantageTeamIndex = 255;

	if (bCasterControl)
	{
		//Reset all casters to "not ready"
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PS != nullptr && PS->bCaster)
			{
				PS->bReadyToPlay = false;
			}
		}
		CTFGameState->bStopGameClock = true;
		CTFGameState->SetTimeLimit(10);
	}

	BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 11, NULL, NULL, NULL);
}

void AUTCTFGameMode::DefaultTimer()
{
	Super::DefaultTimer();

	//If caster control is enabled. check to see if one caster is ready then start the timer
	if (GetMatchState() == MatchState::MatchIntermission && bCasterControl)
	{
		bool bReady = false;
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PS != nullptr && PS->bCaster && PS->bReadyToPlay)
			{
				bReady = true;
				break;
			}
		}

		if (bReady && CTFGameState->bStopGameClock)
		{
			CTFGameState->bStopGameClock = false;
			CTFGameState->SetTimeLimit(11);
		}
	}
	else if (CTFGameState->IsMatchInOvertime())
	{
		float OvertimeElapsed = CTFGameState->ElapsedTime - CTFGameState->OvertimeStartTime;
		if (OvertimeElapsed > TimeLimit)
		{
			// once overtime has gone too long, increase respawn delay
			RespawnWaitTime = 10.f;
			CTFGameState->RespawnWaitTime = RespawnWaitTime;
		}
	}
}

bool AUTCTFGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// Can't restart in overtime
	if (!CTFGameState->IsMatchInProgress() || CTFGameState->IsMatchIntermission() ||
			Player == NULL || Player->IsPendingKillPending())
	{
		return false;
	}
	
	// Ask the player controller if it's ready to restart as well
	return Player->CanRestartPlayer();
}

void AUTCTFGameMode::HandleExitingIntermission()
{
	const bool bWasSecondHalf = CTFGameState->bSecondHalf;

	// This needs to get set before HandleExitingIntermission as HandleMatchHasStarted wants to read this
	CTFGameState->bSecondHalf = true;

	Super::HandleExitingIntermission();

	if (bWasSecondHalf)
	{
		SetMatchState(MatchState::MatchEnteringOvertime);
		CTFGameState->SetTimeLimit(0);
	}
}

void AUTCTFGameMode::HandleEnteringOvertime()
{
	CTFGameState->SetTimeLimit(6000);
	SetMatchState(MatchState::MatchIsInOvertime);
	CTFGameState->bPlayingAdvantage = false;
}

void AUTCTFGameMode::HandleMatchInOvertime()
{
	BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 12, NULL, NULL, NULL);
	CTFGameState->bPlayingAdvantage = false;
}

uint8 AUTCTFGameMode::TeamWithAdvantage()
{
	AUTCTFFlag* Flags[2];
	if (CTFGameState == NULL || CTFGameState->FlagBases.Num() < 2 || CTFGameState->FlagBases[0] == NULL || CTFGameState->FlagBases[1] == NULL)
	{
		return false;	// Fix for crash when CTF transitions to a map without flags.
	}

	Flags[0] = CTFGameState->FlagBases[0]->MyFlag;
	Flags[1] = CTFGameState->FlagBases[1]->MyFlag;

	if ( (Flags[0]->ObjectState == Flags[1]->ObjectState) ||
		 (Flags[0]->ObjectState != CarriedObjectState::Held && Flags[1]->ObjectState != CarriedObjectState::Held))
	{
		// Both flags share the same state so noone has advantage
		return 255;
	}

	int8 AdvantageNum = Flags[0]->ObjectState == CarriedObjectState::Held ? 1 : 0;

	return AdvantageNum;
}

bool AUTCTFGameMode::CheckAdvantage()
{
	if (!CTFGameState || !CTFGameState->FlagBases[0] || !CTFGameState->FlagBases[1])
	{
		return false;
	}
	AUTCTFFlag* Flags[2];
	Flags[0] = CTFGameState->FlagBases[0]->MyFlag;
	Flags[1] = CTFGameState->FlagBases[1]->MyFlag;

	uint8 OtherTeam = 1 - CTFGameState->AdvantageTeamIndex;

	// The Flag was returned so advantage lost
	if (!Flags[0] || !Flags[1] || Flags[OtherTeam]->ObjectState == CarriedObjectState::Home)
	{
		return false;	
	}

	// If our flag is held, then look to see if our advantage is lost.. has to be held for 5 seconds.
	if (Flags[CTFGameState->AdvantageTeamIndex]->ObjectState == CarriedObjectState::Held)
	{
		//Starting the Advantage so play the "Losing advantage" announcement
		if (RemainingAdvantageTime == AdvantageDuration)
		{
			BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 7, NULL, NULL, CTFGameState->Teams[CTFGameState->AdvantageTeamIndex]);
		}
		const int32 AnnouncementLength = 2; //Time for the audio announcement to play out

		RemainingAdvantageTime--;
		if (RemainingAdvantageTime < 0)
		{
			return false;
		}
		//Play the countdown to match the placeholder announcement. Delay by the length of the audio
		else if (RemainingAdvantageTime < 10 && RemainingAdvantageTime < AdvantageDuration - AnnouncementLength)
		{
			BroadcastLocalized(this, UUTCountDownMessage::StaticClass(), RemainingAdvantageTime+1);
		}
	}
	else
	{
		RemainingAdvantageTime = AdvantageDuration;
	}
		
	return true;
}

void AUTCTFGameMode::BuildServerResponseRules(FString& OutRules)
{
	OutRules += FString::Printf(TEXT("Goal Score\t%i\t"), GoalScore);
	OutRules += FString::Printf(TEXT("Time Limit\t%i\t"), TimeLimit);

	if (TimeLimit > 0)
	{
		if (IntermissionDuration <= 0)
		{
			OutRules += FString::Printf(TEXT("No Halftime\tTrue\t"));
		}
		else
		{
			OutRules += FString::Printf(TEXT("Halftime\tTrue\t"));
			OutRules += FString::Printf(TEXT("Halftime Duration\t%i\t"), IntermissionDuration);
		}
	}

	AUTMutator* Mut = BaseMutator;
	while (Mut)
	{
		OutRules += FString::Printf(TEXT("Mutator\t%s\t"), *Mut->DisplayName.ToString());
		Mut = Mut->NextMutator;
	}
}

void AUTCTFGameMode::SetRemainingTime(int32 RemainingSeconds)
{
	if (bOfflineChallenge || bBasicTrainingGame)
	{
		return;
	}
	if (RemainingSeconds > TimeLimit)
	{
		// still in first half;
		UTGameState->RemainingTime = RemainingSeconds - TimeLimit;
	}
	else
	{
		UTGameState->RemainingTime = 1;
		TimeLimit = RemainingSeconds;
		IntermissionDuration = 5;
	}
}

void AUTCTFGameMode::GetGood()
{
#if !(UE_BUILD_SHIPPING)
	if (GetNetMode() == NM_Standalone)
	{
		Super::GetGood();
		IntermissionDuration = 5;
		Teams[0]->Score = 1;
		Teams[1]->Score = 9;
	}
#endif
}

void AUTCTFGameMode::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	Super::CreateGameURLOptions(MenuProps);
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &MercyScore, TEXT("MercyScore"))));
}

#if !UE_SERVER
void AUTCTFGameMode::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
	Super::CreateConfigWidgets(MenuSpace,bCreateReadOnly,ConfigProps);
	TSharedPtr< TAttributeProperty<int32> > MercyScoreAttr = StaticCastSharedPtr<TAttributeProperty<int32>>(FindGameURLOption(ConfigProps,TEXT("MercyScore")));

	// FIXME: temp 'ReadOnly' handling by creating new widgets; ideally there would just be a 'disabled' or 'read only' state in Slate...
	if (MercyScoreAttr.IsValid())
	{
		MenuSpace->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.Padding(0.0f,0.0f,0.0f,5.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("UTCTFGameMode", "MercyScore", "Mercy Score"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(20.0f,0.0f,0.0f,0.0f)
			[
				SNew(SBox)
				.WidthOverride(300)
				[
					bCreateReadOnly ?
					StaticCastSharedRef<SWidget>(
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
						.Text(MercyScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
					) :
					StaticCastSharedRef<SWidget>(
						SNew(SNumericEntryBox<int32>)
						.Value(MercyScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
						.OnValueChanged(MercyScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
						.AllowSpin(true)
						.Delta(1)
						.MinValue(0)
						.MaxValue(32)
						.MinSliderValue(0)
						.MaxSliderValue(32)
						.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
					)
				]
			]
		];
	}

}

#endif
