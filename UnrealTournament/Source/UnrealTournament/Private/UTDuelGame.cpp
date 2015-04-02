// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUD_TeamDM.h"
#include "UTTimedPowerup.h"
#include "UTPickupWeapon.h"
#include "UTDuelGame.h"
#include "Slate/Panels/SUDuelSettings.h"


AUTDuelGame::AUTDuelGame(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	HUDClass = AUTHUD_DM::StaticClass();

	HUDClass = AUTHUD_TeamDM::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "Duel", "Duel");
	PowerupDuration = 10.f;
	GoalScore = 0;
	TimeLimit = 15.f;
	MaxReadyWaitTime = 60;
	bForceRespawn = true;
	bAnnounceTeam = false;
	bHighScorerPerTeamBasis = false;
	bHasRespawnChoices = true;
	bWeaponStayActive = false;
	bOnlyTheStrongSurvive = false;
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
		PickupWeapon->bDisplayRespawnTimer = false;
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

#if !UE_SERVER
void AUTDuelGame::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
	TSharedPtr< TAttributeProperty<int32> > TimeLimitAttr = MakeShareable(new TAttributeProperty<int32>(this, &TimeLimit, TEXT("TimeLimit")));
	ConfigProps.Add(TimeLimitAttr);
	TSharedPtr< TAttributeProperty<int32> > GoalScoreAttr = MakeShareable(new TAttributeProperty<int32>(this, &GoalScore, TEXT("GoalScore")));
	ConfigProps.Add(GoalScoreAttr);
	TSharedPtr< TAttributeProperty<float> > BotSkillAttr = MakeShareable(new TAttributeProperty<float>(this, &GameDifficulty, TEXT("Difficulty")));
	ConfigProps.Add(BotSkillAttr);

	// TODO: BotSkill should be a list box with the usual items; this is a simple placeholder
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
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "BotSkill", "Bot Skill"))
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
					.Text(BotSkillAttr.ToSharedRef(), &TAttributeProperty<float>::GetAsText)
				) :
				StaticCastSharedRef<SWidget>(
					SNew(SNumericEntryBox<float>)
					.LabelPadding(FMargin(10.0f, 0.0f))
					.Value(BotSkillAttr.ToSharedRef(), &TAttributeProperty<float>::GetOptional)
					.OnValueChanged(BotSkillAttr.ToSharedRef(), &TAttributeProperty<float>::Set)
					.AllowSpin(true)
					.Delta(1)
					.MinValue(0)
					.MaxValue(7)
					.MinSliderValue(0)
					.MaxSliderValue(7)
					.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
				)
			]
		]
	];
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

#endif

void AUTDuelGame::UpdateLobbyMatchStats()
{
	Super::UpdateLobbyMatchStats();
	if (LobbyBeacon)
	{
		FString MatchStats = FString::Printf(TEXT("ElpasedTime=%i"), GetWorld()->GetGameState()->ElapsedTime);
		LobbyBeacon->UpdateMatch(MatchStats);
	}
}

void AUTDuelGame::UpdateSkillRating()
{
	for (int i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS != nullptr && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(FName(TEXT("SkillRating")), UTGameState->WinnerPlayerState == PS);
		}
	}
}

void AUTDuelGame::UpdateLobbyBadge()
{
	AUTPlayerState* RedPlayer = NULL;
	AUTPlayerState* BluePlayer = NULL;
	AUTPlayerState* P;

	for (int i=0;i<UTGameState->PlayerArray.Num();i++)
	{
		P = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (P && !P->bIsSpectator && !P->bOnlySpectator)
		{
			if (P->GetTeamNum() == 0) RedPlayer = P;
			else if (P->GetTeamNum() == 1) BluePlayer = P;
		}
	}

	if (RedPlayer && BluePlayer)
	{
		FString Update = TEXT("");
		if (RedPlayer->Score >= BluePlayer->Score)
		{
			Update = FString::Printf(TEXT("<UWindows.Standard.MatchBadge.Header>%s</>\n\n<UWindows.Standard.MatchBadge.Red>%s</>\n<UWindows.Standard.MatchBadge.Red>(%i)</>\n<UWindows.Standard.MatchBadge.Small>-vs-</>\n<UWindows.Standard.MatchBadge.Blue>%s></>\n<UWindows.Standard.MatchBadge.Blue>(%i)</>\n"), *DisplayName.ToString(), 
				*RedPlayer->PlayerName, 	int(RedPlayer->Score),
				*BluePlayer->PlayerName,	int(BluePlayer->Score));
		}
		else
		{
			Update = FString::Printf(TEXT("<UWindows.Standard.MatchBadge.Header>%s</>\n\n<UWindows.Standard.MatchBadge.Blue>%s</>\n<<UWindows.Standard.MatchBadge.Blue>(%i)</>\n<UWindows.Standard.MatchBadge.Small>-vs-</>\n<UWindows.Standard.MatchBadge.Red>%s</>\n<UWindows.Standard.MatchBadge.Red>(%i)</>\n"), *DisplayName.ToString(), 
				*BluePlayer->PlayerName, 	int(RedPlayer->Score),
				*RedPlayer->PlayerName,	int(BluePlayer->Score));
		}

		if (ensure(LobbyBeacon))
		{
			LobbyBeacon->Lobby_UpdateBadge(LobbyInstanceID, Update);
		}


	}


}


