// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTTeamInfo.h"
#include "UTTeamPlayerStart.h"
#include "Slate.h"

UUTTeamInterface::UUTTeamInterface(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

AUTTeamGameMode::AUTTeamGameMode(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	NumTeams = 2;
	bBalanceTeams = true;
	new(TeamColors) FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	new(TeamColors) FLinearColor(0.1f, 0.1f, 1.0f, 1.0f);
	new(TeamColors) FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	new(TeamColors) FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

	TeamNames.Add(NSLOCTEXT("UTTeamGameMode","Team0Name","Red"));
	TeamNames.Add(NSLOCTEXT("UTTeamGameMode","Team1Name","Blue"));
	TeamNames.Add(NSLOCTEXT("UTTeamGameMode","Team2Name","Gold"));
	TeamNames.Add(NSLOCTEXT("UTTeamGameMode","Team3Name","Green"));

	TeamMomentumPct = 0.3f;
	bTeamGame = true;

}

void AUTTeamGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (bAllowURLTeamCountOverride)
	{
		NumTeams = GetIntOption(Options, TEXT("NumTeams"), NumTeams);
	}
	NumTeams = FMath::Max<uint8>(NumTeams, 2);

	if (TeamClass == NULL)
	{
		TeamClass = AUTTeamInfo::StaticClass();
	}
	for (uint8 i = 0; i < NumTeams; i++)
	{
		AUTTeamInfo* NewTeam = GetWorld()->SpawnActor<AUTTeamInfo>(TeamClass);
		NewTeam->TeamIndex = i;
		if (TeamColors.IsValidIndex(i))
		{
			NewTeam->TeamColor = TeamColors[i];
		}

		if (TeamNames.IsValidIndex(i))
		{
			NewTeam->TeamName = TeamNames[i];
		}

		Teams.Add(NewTeam);
		checkSlow(Teams[i] == NewTeam);
	}

	// TDM never kills off players going in to overtime
	bOnlyTheStrongSurvive = false;
}

void AUTTeamGameMode::InitGameState()
{
	Super::InitGameState();
	Cast<AUTGameState>(GameState)->Teams = Teams;
}

APlayerController* AUTTeamGameMode::Login(class UPlayer* NewPlayer, const FString& Portal, const FString& Options, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	APlayerController* PC = Super::Login(NewPlayer, Portal, Options, UniqueId, ErrorMessage);

	if (PC != NULL && !PC->PlayerState->bOnlySpectator)
	{
		uint8 DesiredTeam = uint8(FMath::Clamp<int32>(GetIntOption(Options, TEXT("Team"), 255), 0, 255));
		ChangeTeam(PC, DesiredTeam, false);
	}

	return PC;
}

bool AUTTeamGameMode::ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast)
{
	if (Player == NULL)
	{
		return false;
	}
	else
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(Player->PlayerState);
		if (PS == NULL || PS->bOnlySpectator)
		{
			return false;
		}
		else
		{
			bool bForceTeam = false;
			if (!Teams.IsValidIndex(NewTeam))
			{
				bForceTeam = true;
			}
			else if (bBalanceTeams)
			{
				for (int32 i = 0; i < Teams.Num(); i++)
				{
					// don't allow switching to a team with more players, or equal players if the player is on a team now
					if (i != NewTeam && Teams[i]->GetSize() - ((PS->Team != NULL && PS->Team->TeamIndex == i) ? 1 : 0)  < Teams[NewTeam]->GetSize())
					{
						bForceTeam = true;
						break;
					}
				}
			}
			if (bForceTeam)
			{
				NewTeam = PickBalancedTeam(PS, NewTeam);
			}

			if (Teams.IsValidIndex(NewTeam) && (PS->Team == NULL || PS->Team->TeamIndex != NewTeam))
			{
				if (PS->Team != NULL)
				{
					PS->Team->RemoveFromTeam(Player);
				}
				Teams[NewTeam]->AddToTeam(Player);
				return true;
			}
			else
			{
				// temp logging to track down intermittent issue of not being able to change teams in reasonable situations
				UE_LOG(UT, Log, TEXT("Player %s denied from team change:"), *PS->PlayerName);
				for (int32 i = 0; i < Teams.Num(); i++)
				{
					UE_LOG(UT, Log, TEXT("Team (%i) size: %i"), i, Teams[i]->GetSize());
				}
				return false;
			}
		}
	}
}

uint8 AUTTeamGameMode::PickBalancedTeam(AUTPlayerState* PS, uint8 RequestedTeam)
{
	TArray< AUTTeamInfo*, TInlineAllocator<4> > BestTeams;
	int32 BestSize = -1;

	for (int32 i = 0; i < Teams.Num(); i++)
	{
		int32 TestSize = Teams[i]->GetSize();
		if (Teams[i] == PS->Team)
		{
			// player will be leaving this team so count it's size as post-departure
			TestSize--;
		}
		if (BestTeams.Num() == 0 || TestSize < BestSize)
		{
			BestTeams.Empty();
			BestTeams.Add(Teams[i]);
			BestSize = TestSize;
		}
		else if (TestSize == BestSize)
		{
			BestTeams.Add(Teams[i]);
		}
	}

	for (int32 i = 0; i < BestTeams.Num(); i++)
	{
		if (BestTeams[i]->TeamIndex == RequestedTeam)
		{
			return RequestedTeam;
		}
	}

	return BestTeams[FMath::RandHelper(BestTeams.Num())]->TeamIndex;
}

void AUTTeamGameMode::ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser)
{
	if (InstigatedBy != NULL && InstigatedBy != Injured->Controller && Cast<AUTGameState>(GameState)->OnSameTeam(Injured, InstigatedBy))
	{
		Damage *= TeamDamagePct;
		Momentum *= TeamMomentumPct;
	}
	Super::ModifyDamage_Implementation(Damage, Momentum, Injured, InstigatedBy, HitInfo, DamageCauser);
}

float AUTTeamGameMode::RatePlayerStart(APlayerStart* P, AController* Player)
{
	float Result = Super::RatePlayerStart(P, Player);
	if (bUseTeamStarts && Player != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(Player->PlayerState);
		if (PS != NULL && PS->Team != NULL && (Cast<AUTTeamPlayerStart>(P) == NULL || ((AUTTeamPlayerStart*)P)->TeamNum != PS->Team->TeamIndex))
		{
			// return low positive rating so it can be used as a last resort
			Result *= 0.05;
		}
	}
	return Result;
}

bool AUTTeamGameMode::CheckScore(AUTPlayerState* Scorer)
{
	AUTTeamInfo* WinningTeam = NULL;
	for (int32 i = 0; i < Teams.Num(); i++)
	{
		if (Teams[i]->Score >= GoalScore)
		{
			WinningTeam = Teams[i];
			break;
		}
	}

	if (WinningTeam != NULL)
	{
		AUTPlayerState* BestPlayer = FindBestPlayerOnTeam(WinningTeam->GetTeamNum());
		if (BestPlayer == NULL) BestPlayer = Scorer;
		EndGame(BestPlayer, TEXT("fraglimit")); 
		return true;
	}
	else
	{
		return false;
	}
}

void AUTTeamGameMode::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
#if !UE_SERVER
	Super::CreateConfigWidgets(MenuSpace, ConfigProps);

	TSharedPtr< TAttributePropertyBool > BalanceTeamsAttr = MakeShareable(new TAttributePropertyBool(this, &bBalanceTeams));
	ConfigProps.Add(BalanceTeamsAttr);

	MenuSpace->AddSlot()
	.Padding(0.0f, 5.0f, 0.0f, 5.0f)
	.AutoHeight()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Center)
	[
		SNew(SCheckBox)
		.IsChecked(BalanceTeamsAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
		.OnCheckStateChanged(BalanceTeamsAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
		.Type(ESlateCheckBoxType::CheckBox)
		.Content()
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(NSLOCTEXT("UTTeamGameMode", "BalanceTeams", "Balance Teams").ToString())
		]
	];
#endif
}

AUTPlayerState* AUTTeamGameMode::FindBestPlayerOnTeam(int TeamNumToTest)
{
	AUTPlayerState* Best = NULL;
	for (int i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS != NULL && PS->GetTeamNum() == TeamNumToTest && (Best == NULL || Best->Score < PS->Score))
		{
			Best = PS;
		}
	}
	return Best;
}

