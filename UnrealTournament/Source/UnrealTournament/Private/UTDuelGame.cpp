// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUD_TeamDM.h"
#include "UTTimedPowerup.h"
#include "UTPickupWeapon.h"
#include "UTWeap_Redeemer.h"
#include "UTDuelGame.h"


AUTDuelGame::AUTDuelGame(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HUDClass = AUTHUD_DM::StaticClass();

	HUDClass = AUTHUD_TeamDM::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "Duel", "Duel");
	PowerupDuration = 10.f;
	GoalScore = 0;
	TimeLimit = 15.f;
	bPlayersMustBeReady = true;
	bForceRespawn = true;
}

void AUTDuelGame::InitGameState()
{
	Super::InitGameState();

	UTGameState = Cast<AUTGameState>(GameState);
	if (UTGameState != NULL)
	{
		UTGameState->bWeaponStay = false;
	}
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
		PickupWeapon->WeaponType = nullptr;
	}

	return Super::CheckRelevance_Implementation(Other);
}

void AUTDuelGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	GameSession->MaxPlayers = 2;
	BotFillCount = FMath::Min<int32>(BotFillCount, 2);
	bForceRespawn = true;
}

void AUTDuelGame::PlayEndOfMatchMessage()
{
	// individual winner, not team
	AUTGameMode::PlayEndOfMatchMessage();
}

void AUTDuelGame::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
#if !UE_SERVER
	TSharedPtr< TAttributeProperty<int32> > TimeLimitAttr = MakeShareable(new TAttributeProperty<int32>(this, &TimeLimit));
	ConfigProps.Add(TimeLimitAttr);
	TSharedPtr< TAttributeProperty<int32> > GoalScoreAttr = MakeShareable(new TAttributeProperty<int32>(this, &GoalScore));
	ConfigProps.Add(GoalScoreAttr);
	TSharedPtr< TAttributePropertyBool > ForceRespawnAttr = MakeShareable(new TAttributePropertyBool(this, &bForceRespawn));
	ConfigProps.Add(ForceRespawnAttr);
	TSharedPtr< TAttributeProperty<int32> > CombatantsAttr = MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount));
	ConfigProps.Add(CombatantsAttr);
	TSharedPtr< TAttributeProperty<float> > BotSkillAttr = MakeShareable(new TAttributeProperty<float>(this, &GameDifficulty));
	ConfigProps.Add(BotSkillAttr);

	MenuSpace->AddSlot()
		.Padding(10.0f, 5.0f, 10.0f, 5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(200.0f)
			.Content()
			[
				SNew(SNumericEntryBox<int32>)
				.LabelPadding(FMargin(10.0f, 0.0f))
				.Value(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
				.OnValueChanged(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
				.AllowSpin(true)
				.Delta(1)
				.MinValue(1)
				.MaxValue(32)
				.MinSliderValue(1)
				.MaxSliderValue(32)
				.Label()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Black)
					.Text(NSLOCTEXT("UTGameMode", "NumCombatants", "Number of Combatants"))
				]
			]
		];
	// TODO: BotSkill should be a list box with the usual items; this is a simple placeholder
	MenuSpace->AddSlot()
		.Padding(10.0f, 5.0f, 10.0f, 5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(200.0f)
			.Content()
			[
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
				.Label()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Black)
					.Text(NSLOCTEXT("UTGameMode", "BotSkill", "Bot Skill"))
				]
			]
		];
	MenuSpace->AddSlot()
		.Padding(10.0f, 5.0f, 10.0f, 5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(150.0f)
			.Content()
			[
				SNew(SNumericEntryBox<int32>)
				.LabelPadding(FMargin(10.0f, 0.0f))
				.Value(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
				.OnValueChanged(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
				.AllowSpin(true)
				.Delta(1)
				.MinValue(0)
				.MaxValue(999)
				.MinSliderValue(0)
				.MaxSliderValue(99)
				.Label()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Black)
					.Text(NSLOCTEXT("UTGameMode", "GoalScore", "Goal Score"))
				]
			]
		];
	MenuSpace->AddSlot()
		.Padding(10.0f, 5.0f, 10.0f, 5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(150.0f)
			.Content()
			[
				SNew(SNumericEntryBox<int32>)
				.LabelPadding(FMargin(10.0f, 0.0f))
				.Value(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
				.OnValueChanged(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
				.AllowSpin(true)
				.Delta(1)
				.MinValue(0)
				.MaxValue(999)
				.MinSliderValue(0)
				.MaxSliderValue(60)
				.Label()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Black)
					.Text(NSLOCTEXT("UTGameMode", "TimeLimit", "Time Limit"))
				]
			]
		];
#endif
}