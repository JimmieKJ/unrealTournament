// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_Duel.h"
#include "UTTimedPowerup.h"
#include "UTPickupWeapon.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsStyle.h"
#include "SNumericEntryBox.h"
#include "UTDuelGame.h"
#include "StatNames.h"
#include "UTSpectatorPickupMessage.h"
#include "UTDuelSquadAI.h"
#include "UTDuelScoreboard.h"

AUTDuelGame::AUTDuelGame(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	HUDClass = AUTHUD_Duel::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "Duel", "Duel");
	PowerupDuration = 10.f;
	GoalScore = 0;
	TimeLimit = 10;
	bForceRespawn = true;
	bAnnounceTeam = false;
	bHighScorerPerTeamBasis = false;
	bHasRespawnChoices = true;
	bWeaponStayActive = false;
	bNoDefaultLeaderHat = true;
	XPMultiplier = 7.0f;
	SquadType = AUTDuelSquadAI::StaticClass();
}

void AUTDuelGame::InitGameState()
{
	Super::InitGameState();

	UTGameState = Cast<AUTGameState>(GameState);
	if (UTGameState != NULL)
	{
		UTGameState->bWeaponStay = false;
		UTGameState->bAllowTeamSwitches = false;
	}
}

bool AUTDuelGame::ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast)
{
	// don't allow team changes in Duel once have initial team
	if (Player == NULL)
	{
		return false;
	}
	else
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(Player->PlayerState);
		if (PS == NULL || PS->bOnlySpectator || PS->Team)
		{
			return false;
		}
	}
	return Super::ChangeTeam(Player, NewTeam, bBroadcast);
}

bool AUTDuelGame::CheckRelevance_Implementation(AActor* Other)
{
	AUTTimedPowerup* Powerup = Cast<AUTTimedPowerup>(Other);
	if (Powerup)
	{
		Powerup->TimeRemaining = PowerupDuration;
	}
	
	// @TODO FIXMESTEVE - don't check for weapon stay - once have deployable base class, remove all deployables from duel
	AUTPickupWeapon* PickupWeapon = Cast<AUTPickupWeapon>(Other);
	if (PickupWeapon != NULL && PickupWeapon->WeaponType != NULL && !PickupWeapon->WeaponType.GetDefaultObject()->bWeaponStay)
	{
		PickupWeapon->SetInventoryType(nullptr);
	}

	return Super::CheckRelevance_Implementation(Other);
}

void AUTDuelGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	GameSession->MaxPlayers = 2;
	BotFillCount = 2;
	bForceRespawn = true;
	bBalanceTeams = true;
}

void AUTDuelGame::SetPlayerDefaults(APawn* PlayerPawn)
{
	AUTCharacter* UTChar = Cast<AUTCharacter>(PlayerPawn);
	if (UTChar)
	{
		UTChar->MaxStackedArmor = 150.f;
	}
	Super::SetPlayerDefaults(PlayerPawn);
}

void AUTDuelGame::PlayEndOfMatchMessage()
{
	// individual winner, not team
	AUTGameMode::PlayEndOfMatchMessage();
}

void AUTDuelGame::GetGameURLOptions(const TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps, TArray<FString>& OptionsList, int32& DesiredPlayerCount)
{
	Super::GetGameURLOptions(MenuProps, OptionsList, DesiredPlayerCount);
	DesiredPlayerCount = 2;
}

void AUTDuelGame::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &TimeLimit, TEXT("TimeLimit"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &GoalScore, TEXT("GoalScore"))));
}

#if !UE_SERVER
void AUTDuelGame::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
	CreateGameURLOptions(ConfigProps);

	TSharedPtr< TAttributeProperty<int32> > TimeLimitAttr = StaticCastSharedPtr<TAttributeProperty<int32>>(FindGameURLOption(ConfigProps,TEXT("TimeLimit")));
	TSharedPtr< TAttributeProperty<int32> > GoalScoreAttr = StaticCastSharedPtr<TAttributeProperty<int32>>(FindGameURLOption(ConfigProps,TEXT("GoalScore")));

	if (GoalScoreAttr.IsValid())
	{
		MenuSpace->AddSlot()
		.Padding(0.0f,0.0f,0.0f,5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
					.Text(NSLOCTEXT("UTGameMode", "GoalScore", "Goal Score"))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(20.0f,0.0f,0.0f,0.0f)	
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(300)
				[
					bCreateReadOnly ?
					StaticCastSharedRef<SWidget>(
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(),"UT.Common.ButtonText.White")
						.Text(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
					) :
					StaticCastSharedRef<SWidget>(
						SNew(SNumericEntryBox<int32>)
						.Value(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
						.OnValueChanged(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
						.AllowSpin(true)
						.Delta(1)
						.MinValue(0)
						.MaxValue(999)
						.MinSliderValue(0)
						.MaxSliderValue(99)
						.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
					)
				]
			]
		];
	}
	if (TimeLimitAttr.IsValid())
	{
		MenuSpace->AddSlot()
		.Padding(0.0f,0.0f,0.0f,5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("UTGameMode", "TimeLimit", "Time Limit"))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(20.0f,0.0f,0.0f,0.0f)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(300)
				[
					bCreateReadOnly ?
					StaticCastSharedRef<SWidget>(
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
						.Text(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
					) :
					StaticCastSharedRef<SWidget>(
						SNew(SNumericEntryBox<int32>)
						.Value(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
						.OnValueChanged(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
						.AllowSpin(true)
						.Delta(1)
						.MinValue(0)
						.MaxValue(999)
						.MinSliderValue(0)
						.MaxSliderValue(60)
						.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
					)
				]
			]
		];
	}
}

#endif

void AUTDuelGame::UpdateSkillRating()
{
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS != nullptr && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(NAME_SkillRating, UTGameState->WinnerPlayerState == PS, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}

	for (int32 i = 0; i < InactivePlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(InactivePlayerArray[i]);
		if (PS != nullptr && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(NAME_SkillRating, UTGameState->WinnerPlayerState == PS, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}
}

void AUTDuelGame::FindAndMarkHighScorer()
{
	AUTGameMode::FindAndMarkHighScorer();
}

void AUTDuelGame::BroadcastSpectatorPickup(AUTPlayerState* PS, FName StatsName, UClass* PickupClass)
{
	if (PS != nullptr && PickupClass != nullptr && StatsName != NAME_None)
	{
		int32 PlayerNumPickups = (int32)PS->GetStatsValue(StatsName);
		int32 TotalPickups = (int32)UTGameState->GetStatsValue(StatsName);

		//Stats may not have been replicated to the client so pack them in the switch
		int32 Switch = TotalPickups << 16 | PlayerNumPickups;

		BroadcastSpectator(nullptr, UUTSpectatorPickupMessage::StaticClass(), Switch, PS, nullptr, PickupClass);
	}
}

uint8 AUTDuelGame::GetNumMatchesFor(AUTPlayerState* PS) const
{
	return PS ? PS->DuelMatchesPlayed : 0;
}

int32 AUTDuelGame::GetEloFor(AUTPlayerState* PS) const
{
	return PS ? PS->DuelRank : Super::GetEloFor(PS);
}

void AUTDuelGame::SetEloFor(AUTPlayerState* PS, int32 NewEloValue)
{
	if (PS)
	{
		PS->DuelRank = NewEloValue;
	}
}
