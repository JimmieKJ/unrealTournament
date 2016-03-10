// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFRoundGame.h"
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
#include "UTShowdownGameMessage.h"
#include "UTShowdownRewardMessage.h"

AUTCTFRoundGame::AUTCTFRoundGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	GoalScore = 3;
	TimeLimit = 0;
	DisplayName = NSLOCTEXT("UTGameMode", "CTFR", "Round based CTF");
	RoundLives = 5;
	bPerPlayerLives = true;
	bNeedFiveKillsMessage = true;
	FlagCapScore = 2;
	RespawnWaitTime = 3.f;
	ForceRespawnTime = 5.f;
	bForceRespawn = true;
	bUseDash = false;
	bAsymmetricVictoryConditions = true;
	bCarryOwnFlag = true;
	bNoFlagReturn = true;
	bFirstRoundInitialized = false;
	ExtraHealth = 0;
}

void AUTCTFRoundGame::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &GoalScore, TEXT("GoalScore"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount, TEXT("BotFill"))));
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bBalanceTeams, TEXT("BalanceTeams"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &MercyScore, TEXT("MercyScore"))));
}

void AUTCTFRoundGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	TimeLimit = 0;
	if (GoalScore == 0)
	{
		GoalScore = 3;
	}

	// key options are ?Respawnwait=xx?RoundLives=xx?CTFMode=xx?Dash=xx?Asymm=xx?PerPlayerLives=xx
	RoundLives = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("RoundLives"), RoundLives));

	FString InOpt = UGameplayStatics::ParseOption(Options, TEXT("Dash"));
	bUseDash = EvalBoolOptions(InOpt, bUseDash);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("Asymm"));
	bAsymmetricVictoryConditions = EvalBoolOptions(InOpt, bAsymmetricVictoryConditions);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("OwnFlag"));
	bCarryOwnFlag = EvalBoolOptions(InOpt, bCarryOwnFlag);
	// FIXMESTEVE - if carry own flag, need option whether enemy flag needs to be home to score, and need base effect if not

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("FlagReturn"));
	bNoFlagReturn = EvalBoolOptions(InOpt, bNoFlagReturn);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("PerPlayerLives"));
	bPerPlayerLives = EvalBoolOptions(InOpt, bPerPlayerLives);

	ExtraHealth = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("XHealth"), ExtraHealth));
}

void AUTCTFRoundGame::SetPlayerDefaults(APawn* PlayerPawn)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(PlayerPawn);
	if (UTC != NULL)
	{
		UTC->HealthMax = UTC->GetClass()->GetDefaultObject<AUTCharacter>()->HealthMax + ExtraHealth;
		UTC->SuperHealthMax = UTC->GetClass()->GetDefaultObject<AUTCharacter>()->SuperHealthMax + ExtraHealth;
	}
	Super::SetPlayerDefaults(PlayerPawn);
}

bool AUTCTFRoundGame::CheckScore_Implementation(AUTPlayerState* Scorer)
{
	if (Scorer->Team != NULL)
	{
		if (GoalScore > 0 && Scorer->Team->Score >= GoalScore)
		{
			EndGame(Scorer, FName(TEXT("scorelimit")));
		}
	}
	return true;
}

void AUTCTFRoundGame::HandleFlagCapture(AUTPlayerState* Holder)
{
	CheckScore(Holder);
	if (UTGameState->IsMatchInProgress())
	{
		SetMatchState(MatchState::MatchIntermission);
	}
}

int32 AUTCTFRoundGame::IntermissionTeamToView(AUTPlayerController* PC)
{
	if (LastTeamToScore)
	{
		return LastTeamToScore->TeamIndex;
	}
	return Super::IntermissionTeamToView(PC);
}

void AUTCTFRoundGame::BuildServerResponseRules(FString& OutRules)
{
	OutRules += FString::Printf(TEXT("Goal Score\t%i\t"), GoalScore);

	AUTMutator* Mut = BaseMutator;
	while (Mut)
	{
		OutRules += FString::Printf(TEXT("Mutator\t%s\t"), *Mut->DisplayName.ToString());
		Mut = Mut->NextMutator;
	}
}

void AUTCTFRoundGame::StartMatch()
{
	if (!bFirstRoundInitialized)
	{
		InitRound();
		CTFGameState->CTFRound = 1;
		bFirstRoundInitialized = true;
	}
	Super::StartMatch();
}

void AUTCTFRoundGame::HandleExitingIntermission()
{
	InitRound();
	Super::HandleExitingIntermission();
}

void AUTCTFRoundGame::InitRound()
{
	bFirstBloodOccurred = false;
	bNeedFiveKillsMessage = true;
	if (CTFGameState)
	{
		CTFGameState->CTFRound++;
		if (!bPerPlayerLives)
		{
			CTFGameState->RedLivesRemaining = RoundLives;
			CTFGameState->BlueLivesRemaining = RoundLives;
		}
		if (CTFGameState->FlagBases.Num() > 1)
		{
			CTFGameState->RedLivesRemaining += CTFGameState->FlagBases[0] ? CTFGameState->FlagBases[0]->RoundLivesAdjustment : 0;
			CTFGameState->BlueLivesRemaining += CTFGameState->FlagBases[1] ? CTFGameState->FlagBases[0]->RoundLivesAdjustment : 0;
		}
	}

	bRedToCap = !bRedToCap;
	for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
	{
		if (Base != NULL && Base->MyFlag)
		{
			AUTCarriedObject* Flag = Base->MyFlag;
			if (bAsymmetricVictoryConditions)
			{
				if (bRedToCap == (Flag->GetTeamNum() == 0))
				{
					Flag->bEnemyCanPickup = !bCarryOwnFlag;
					Flag->bFriendlyCanPickup = bCarryOwnFlag;
					Flag->bTeamPickupSendsHome = !Flag->bFriendlyCanPickup && !bNoFlagReturn;
					Flag->bEnemyPickupSendsHome = !Flag->bEnemyCanPickup && !bNoFlagReturn;
				}
				else
				{
					Flag->bEnemyCanPickup = false;
					Flag->bFriendlyCanPickup = false;
					Flag->bTeamPickupSendsHome = false;
					Flag->bEnemyPickupSendsHome = false;
				}
			}
			else
			{
				Flag->bEnemyCanPickup = !bCarryOwnFlag;
				Flag->bFriendlyCanPickup = bCarryOwnFlag;
				Flag->bTeamPickupSendsHome = !Flag->bFriendlyCanPickup && !bNoFlagReturn;
				Flag->bEnemyPickupSendsHome = !Flag->bEnemyCanPickup && !bNoFlagReturn;
			}
		}
	}

	if (bAsymmetricVictoryConditions)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC)
			{
				if (bRedToCap == (PC->GetTeamNum() == 0))
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFGameMessage::StaticClass(), 14);
				}
				else
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFGameMessage::StaticClass(), 15);
				}
			}
		}
	}
	else if (RoundLives > 0)
	{
		BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 16, NULL, NULL, NULL);
	}

	if (bPerPlayerLives)
	{
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PS && PS->bIsInactive)
			{
				PS->RemainingLives = 0;
				PS->SetOutOfLives(true);
			}
			else if (PS)
			{
				PS->RemainingLives = RoundLives;
				PS->SetOutOfLives(false);
			}
		}
	}
}

void AUTCTFRoundGame::RestartPlayer(AController* aPlayer)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(aPlayer->PlayerState);
	if (bPerPlayerLives && PS && PS->Team)
	{
		if (PS->bOutOfLives)
		{
			return;
		}
		PS->RemainingLives--;
		if ((PS->RemainingLives < 0) && (!bAsymmetricVictoryConditions || (bRedToCap == (PS->Team->TeamIndex == 0))))
		{
			// this player is out of lives
			PS->SetOutOfLives(true);
			for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
			{
				AUTPlayerState* OtherPS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
				if (OtherPS && (OtherPS->Team == PS->Team) && !OtherPS->bOutOfLives && !OtherPS->bIsInactive)
				{
					// found a live teammate, so round isn't over - notify about termination though
					BroadcastLocalized(NULL, UUTShowdownRewardMessage::StaticClass(), 3);
					return;
				}
			}
			BroadcastLocalized(NULL, UUTShowdownRewardMessage::StaticClass(), 4);
			ScoreOutOfLives((PS->Team->TeamIndex == 0) ? 1 : 0);
			return;
		}
	}
	Super::RestartPlayer(aPlayer);

	if (aPlayer->GetPawn() && !bPerPlayerLives && (RoundLives > 0) && PS && PS->Team && CTFGameState && CTFGameState->IsMatchInProgress())
	{
		if ((PS->Team->TeamIndex == 0) && (!bAsymmetricVictoryConditions || bRedToCap))
		{
			CTFGameState->RedLivesRemaining--;
			if (CTFGameState->RedLivesRemaining <= 0)
			{
				CTFGameState->RedLivesRemaining = 0;
				ScoreOutOfLives(1);
				return;
			}
			else if (bNeedFiveKillsMessage && (CTFGameState->RedLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
		else if ((PS->Team->TeamIndex == 1) && (!bAsymmetricVictoryConditions || !bRedToCap))
		{
			CTFGameState->BlueLivesRemaining--;
			if (CTFGameState->BlueLivesRemaining <= 0)
			{
				CTFGameState->BlueLivesRemaining = 0;
				ScoreOutOfLives(0);
				return;
			}
			else if (bNeedFiveKillsMessage && (CTFGameState->BlueLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
	}
}

void AUTCTFRoundGame::ScoreOutOfLives(int32 WinningTeamIndex)
{
	FindAndMarkHighScorer();
	AUTTeamInfo* WinningTeam = (Teams.Num() > WinningTeamIndex) ? Teams[WinningTeamIndex] : NULL;
	if (WinningTeam)
	{
		WinningTeam->Score++;
		WinningTeam->ForceNetUpdate();
		LastTeamToScore = WinningTeam;
		BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 3 + WinningTeam->TeamIndex);
		if (GoalScore > 0 && LastTeamToScore->Score >= GoalScore)
		{
			EndGame(NULL, FName(TEXT("scorelimit")));
		}
		else if (MercyScore > 0)
		{
			int32 Spread = LastTeamToScore->Score;
			for (AUTTeamInfo* OtherTeam : Teams)
			{
				if (OtherTeam != LastTeamToScore)
				{
					Spread = FMath::Min<int32>(Spread, LastTeamToScore->Score - OtherTeam->Score);
				}
			}
			if (Spread >= MercyScore)
			{
				EndGame(NULL, FName(TEXT("MercyScore")));
			}
		}
		if (UTGameState->IsMatchInProgress())
		{
			SetMatchState(MatchState::MatchIntermission);
		}
	}
}




