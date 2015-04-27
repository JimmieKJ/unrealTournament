// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "Net/UnrealNetwork.h"

AUTCTFGameState::AUTCTFGameState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bSecondHalf = false;
	bHalftime = false;
	HalftimeScoreDelay = 2.f;
}

void AUTCTFGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCTFGameState, bSecondHalf);
	DOREPLIFETIME(AUTCTFGameState, bHalftime);
	DOREPLIFETIME(AUTCTFGameState, FlagBases);
	DOREPLIFETIME(AUTCTFGameState, bPlayingAdvantage);
	DOREPLIFETIME(AUTCTFGameState, AdvantageTeamIndex);
	DOREPLIFETIME(AUTCTFGameState, ScoringPlays);
}

void AUTCTFGameState::SetMaxNumberOfTeams(int TeamCount)
{
	for (int32 TeamIdx = 0; TeamIdx < TeamCount; TeamIdx++)
	{
		FlagBases.Add(NULL);
	}
}

void AUTCTFGameState::CacheFlagBase(AUTCTFFlagBase* BaseToCache)
{
	if (BaseToCache->MyFlag != NULL)
	{
		uint8 TeamNum = BaseToCache->MyFlag->GetTeamNum();
		if (TeamNum < FlagBases.Num())
		{
			FlagBases[TeamNum] = BaseToCache;
		}
		else
		{
			UE_LOG(UT,Warning,TEXT("Found a Flag Base with TeamNum of %i that was unexpected (%i)"), TeamNum, FlagBases.Num());
		}
	}
	else
	{
		UE_LOG(UT,Warning,TEXT("Found a Flag Base (%s) without a flag"), *GetNameSafe(BaseToCache));
	}

}

AUTTeamInfo* AUTCTFGameState::FindLeadingTeam()
{
	AUTTeamInfo* WinningTeam = NULL;
	bool bTied;

	if (Teams.Num() > 0)
	{
		WinningTeam = Teams[0];
		bTied = false;
		for (int i=1;i<Teams.Num();i++)
		{
			if (Teams[i]->Score == WinningTeam->Score)
			{
				bTied = true;
			}
			else if (Teams[i]->Score > WinningTeam->Score)
			{
				WinningTeam = Teams[i];
				bTied = false;
			}
		}

		if (bTied) WinningTeam = NULL;
	}

	return WinningTeam;	

}

FName AUTCTFGameState::GetFlagState(uint8 TeamNum)
{
	if (TeamNum < FlagBases.Num() && FlagBases[TeamNum] != NULL)
	{
		return FlagBases[TeamNum]->GetFlagState();
	}

	return NAME_None;
}

AUTPlayerState* AUTCTFGameState::GetFlagHolder(uint8 TeamNum)
{
	if (TeamNum < FlagBases.Num() && FlagBases[TeamNum] != NULL)
	{
		return FlagBases[TeamNum]->GetCarriedObjectHolder();
	}

	return NULL;
}

AUTCTFFlagBase* AUTCTFGameState::GetFlagBase(uint8 TeamNum)
{
	if (TeamNum < FlagBases.Num())
	{
		return FlagBases[TeamNum];
	}

	return NULL;
}


void AUTCTFGameState::ResetFlags()
{
	for (int i=0; i < FlagBases.Num(); i++)
	{
		if (FlagBases[i] != NULL)
		{
			FlagBases[i]->RecallFlag();
		}
	}

}

bool AUTCTFGameState::IsMatchInProgress() const
{
	FName MatchState = GetMatchState();
	if (MatchState == MatchState::InProgress || MatchState == MatchState::MatchIsInOvertime || MatchState == MatchState::MatchIsAtHalftime ||
			MatchState == MatchState::MatchEnteringHalftime || MatchState == MatchState::MatchExitingHalftime ||
			MatchState == MatchState::MatchEnteringSuddenDeath || MatchState == MatchState::MatchIsInSuddenDeath)
	{
		return true;
	}

	return false;
}

bool AUTCTFGameState::IsMatchInOvertime() const
{
	FName MatchState = GetMatchState();
	if (MatchState == MatchState::MatchIsInOvertime ||	MatchState == MatchState::MatchEnteringSuddenDeath || MatchState == MatchState::MatchIsInSuddenDeath)
	{
		return true;
	}

	return false;
}


bool AUTCTFGameState::IsMatchAtHalftime() const
{
	FName MatchState = GetMatchState();
	if (MatchState == MatchState::MatchIsAtHalftime || MatchState == MatchState::MatchEnteringHalftime || MatchState == MatchState::MatchExitingHalftime)
	{
		return true;
	}

	return false;
}

bool AUTCTFGameState::IsMatchInSuddenDeath() const
{
	FName MatchState = GetMatchState();
	if (MatchState == MatchState::MatchEnteringSuddenDeath || MatchState == MatchState::MatchIsInSuddenDeath)
	{
		return true;
	}

	return false;
}

FName AUTCTFGameState::OverrideCameraStyle(APlayerController* PCOwner, FName CurrentCameraStyle)
{
	return (IsMatchAtHalftime() || HasMatchEnded()) ? FName(TEXT("FreeCam")) : Super::OverrideCameraStyle(PCOwner, CurrentCameraStyle);
}

void AUTCTFGameState::OnHalftimeChanged()
{
	if (bHalftime)
	{
		// delay toggling scoreboard
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFGameState::ToggleScoreboards, HalftimeScoreDelay);
	}
	else
	{
		ToggleScoreboards();
	}
}

void AUTCTFGameState::ToggleScoreboards()
{
	for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
		if (PC != NULL)
		{
			PC->ClientToggleScoreboard(bHalftime);
		}
	}
}

void AUTCTFGameState::AddScoringPlay(const FCTFScoringPlay& NewScoringPlay)
{
	if (Role == ROLE_Authority)
	{
		ScoringPlays.AddUnique(NewScoringPlay);
	}
}

FText AUTCTFGameState::GetGameStatusText()
{
	if (bPlayingAdvantage)
	{
		if (AdvantageTeamIndex == 0)
		{
			return NSLOCTEXT("UTCTFGameState", "RedAdvantage", "Red Advantage");
		}
		else
		{
			return NSLOCTEXT("UTCTFGameState", "BlueAdvantage", "Blue Advantage");
		}
	}
	else if (IsMatchAtHalftime()) 
	{
		return bSecondHalf ? NSLOCTEXT("UTCTFGameState", "PreOvertime", "Get Ready!") : NSLOCTEXT("UTCTFGameState", "HalfTime", "HalfTime");
	}
	else if (IsMatchInProgress())
	{
		if (IsMatchInOvertime())
		{
			return NSLOCTEXT("UTFGameState", "Overtime", "Overtime!");
		}

		return bSecondHalf ? NSLOCTEXT("UTCTFGameState", "SecondHalf", "Second Half") : NSLOCTEXT("UTCTFGameState", "FirstHalf", "First Half");
	}

	return Super::GetGameStatusText();
}