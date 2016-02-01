// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamShowdownGame.h"
#include "UTTimerMessage.h"
#include "UTTeamShowdownGameMessage.h"
#include "UTHUD_Showdown.h"
#include "UTShowdownGameState.h"
#include "UTShowdownSquadAI.h"
#include "UTPickupAmmo.h"
#include "UTDroppedPickup.h"
#include "UTGameEngine.h"
#include "UTChallengeManager.h"
#include "UTDroppedAmmoBox.h"
#include "SlateGameResources.h"
#include "SUWindowsStyle.h"
#include "SNumericEntryBox.h"
#include "UTShowdownRewardMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTMutator.h"

AUTTeamShowdownGame::AUTTeamShowdownGame(const FObjectInitializer& OI)
	: Super(OI)
{
	TimeLimit = 3.0f; // per round
	GoalScore = 5;
	DisplayName = NSLOCTEXT("UTGameMode", "TeamShowdown", "Team Showdown");
	bAnnounceTeam = true;
	QuickPlayersToStart = 6;
}

void AUTTeamShowdownGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	int32 SavedBotFillCount = BotFillCount;

	Super::InitGame(MapName, Options, ErrorMessage);

	// skip Duel overrides we don't want
	if (bOfflineChallenge)
	{
		TWeakObjectPtr<UUTChallengeManager> ChallengeManager = Cast<UUTGameEngine>(GEngine)->GetChallengeManager();
		if (ChallengeManager.IsValid())
		{
			BotFillCount = ChallengeManager->GetNumPlayers(this);
		}
	}
	else if (UGameplayStatics::HasOption(Options, TEXT("Bots")))
	{
		BotFillCount = UGameplayStatics::GetIntOption(Options, TEXT("Bots"), SavedBotFillCount) + 1;
	}
	else
	{
		BotFillCount = UGameplayStatics::GetIntOption(Options, TEXT("BotFill"), SavedBotFillCount);
	}
	GameSession->MaxPlayers = UGameplayStatics::GetIntOption(Options, TEXT("MaxPlayers"), GameSession->GetClass()->GetDefaultObject<AGameSession>()->MaxPlayers);
}

bool AUTTeamShowdownGame::CheckRelevance_Implementation(AActor* Other)
{
	// weapons and ammo pickups are worth double ammo
	AUTWeapon* W = Cast<AUTWeapon>(Other);
	if (W != NULL)
	{
		W->Ammo = FMath::Min<int32>(W->MaxAmmo, W->Ammo * 1.5);
	}
	else
	{
		AUTPickupAmmo* AmmoPickup = Cast<AUTPickupAmmo>(Other);
		if (AmmoPickup != NULL)
		{
			AmmoPickup->Ammo.Amount *= 1.5;
		}
		else
		{
			// make dropped weapons last for the whole round
			AUTDroppedPickup* Drop = Cast<AUTDroppedPickup>(Other);
			if (Drop != NULL)
			{
				Drop->SetLifeSpan(0.0f);
			}
		}
	}
	return Super::CheckRelevance_Implementation(Other);
}

void AUTTeamShowdownGame::RestartPlayer(AController* aPlayer)
{
	Super::RestartPlayer(aPlayer);

	// go to spectating if dead and can't respawn
	if (!bAllowPlayerRespawns && IsMatchInProgress() && aPlayer->GetPawn() == NULL)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(aPlayer);
		if (PC != NULL)
		{
			PC->ChangeState(NAME_Spectating);
			PC->ClientGotoState(NAME_Spectating);

			AUTPlayerState* PS = Cast<AUTPlayerState>(PC->PlayerState);
			if (PS != NULL && PS->Team != NULL)
			{
				for (AController* Member : PS->Team->GetTeamMembers())
				{
					if (Member->GetPawn() != NULL)
					{
						PC->ServerViewPlayerState(Member->PlayerState);
						break;
					}
				}
			}
		}
	}
}

void AUTTeamShowdownGame::PlayEndOfMatchMessage()
{
	// individual winner, not team
	AUTTeamGameMode::PlayEndOfMatchMessage();
}

void AUTTeamShowdownGame::DiscardInventory(APawn* Other, AController* Killer)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(Other);
	if (UTC != NULL)
	{
		// discard all weapons instead of just the one
		FRotationMatrix RotMat(Other->GetActorRotation());
		FVector ThrowDirs[] = { RotMat.GetUnitAxis(EAxis::Y), -RotMat.GetUnitAxis(EAxis::Y), -RotMat.GetUnitAxis(EAxis::X) };

		int32 i = 0;
		for (TInventoryIterator<AUTWeapon> It(UTC); It; ++It)
		{
			if (*It != UTC->GetWeapon())
			{
				FVector FinalDir;
				if (i < ARRAY_COUNT(ThrowDirs))
				{
					FinalDir = ThrowDirs[i];
				}
				else
				{
					FinalDir = FMath::VRand();
					FinalDir.Z = 0.0f;
					FinalDir.Normalize();
				}
				It->DropFrom(UTC->GetActorLocation(), FinalDir * 1000.0f + FVector(0.0f, 0.0f, 250.0f));
				i++;
			}
		}
		// spawn ammo box with any unassigned ammo
		if (UTC->SavedAmmo.Num() > 0)
		{
			FVector FinalDir = FMath::VRand();
			FinalDir.Z = 0.0f;
			FinalDir.Normalize();
			FActorSpawnParameters Params;
			Params.Instigator = UTC;
			AUTDroppedAmmoBox* Pickup = GetWorld()->SpawnActor<AUTDroppedAmmoBox>(AUTDroppedAmmoBox::StaticClass(), UTC->GetActorLocation(), FinalDir.Rotation(), Params);
			if (Pickup != NULL)
			{
				Pickup->Movement->Velocity = FinalDir * 1000.0f + FVector(0.0f, 0.0f, 250.0f);
				Pickup->Ammo = UTC->SavedAmmo;
			}
		}
	}
	Super::DiscardInventory(Other, Killer);
}

void AUTTeamShowdownGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	if (GetMatchState() != MatchState::MatchIntermission && (TimeLimit <= 0 || UTGameState->RemainingTime > 0))
	{
		if (Killer != Other && Killer != NULL && UTGameState->OnSameTeam(Killer, Other))
		{
			// AUTGameMode doesn't handle team kills and AUTTeamDMGameMode would change the team score so we need to do it ourselves
			AUTPlayerState* KillerPS = Cast<AUTPlayerState>(Killer->PlayerState);
			if (KillerPS != NULL)
			{
				KillerPS->AdjustScore(-100);
			}
		}
		else
		{
			AUTPlayerState* OtherPlayerState = Other ? Cast<AUTPlayerState>(Other->PlayerState) : NULL;
			if ((Killer == Other) || (Killer == NULL))
			{
				// If it's a suicide, subtract a kill from the player...
				if (OtherPlayerState)
				{
					OtherPlayerState->AdjustScore(-100);
				}
			}
			else
			{
				OtherPlayerState->AdjustScore(-100);
				AUTPlayerState * KillerPlayerState = Cast<AUTPlayerState>(Killer->PlayerState);
				if (KillerPlayerState != NULL)
				{
					KillerPlayerState->AdjustScore(+100);
					KillerPlayerState->IncrementKills(DamageType, true, OtherPlayerState);
				}

				if (!bFirstBloodOccurred)
				{
					BroadcastLocalized(this, UUTFirstBloodMessage::StaticClass(), 0, KillerPlayerState, NULL, NULL);
					bFirstBloodOccurred = true;
				}
			}
		}
		if (Other != NULL)
		{
			AUTPlayerState* OtherPS = Cast<AUTPlayerState>(Other->PlayerState);
			if (OtherPS != NULL && OtherPS->Team != NULL)
			{
				OtherPS->SetOutOfLives(true);
				AUTPlayerState* KillerPS = (Killer != NULL && Killer != Other) ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
				AUTTeamInfo* KillerTeam = (KillerPS != NULL && KillerPS->Team != OtherPS->Team) ? KillerPS->Team : Teams[1 - FMath::Min<int32>(1, OtherPS->Team->TeamIndex)];

				int32 AliveCount = 0;
				AController* LastAlive = NULL;
				int32 TeamCount = 0;
				for (AController* C : OtherPS->Team->GetTeamMembers())
				{
					if (C != Other && C->GetPawn() != NULL && !C->GetPawn()->bTearOff)
					{
						LastAlive = C;
						AliveCount++;
					}
					TeamCount++;
				}
				if (AliveCount == 1)
				{
					for (AController* C : OtherPS->Team->GetTeamMembers())
					{
						AUTPlayerController* PC = Cast<AUTPlayerController>(C);
						if (PC && PC->GetPawn() != NULL && !PC->GetPawn()->bTearOff)
						{
							PC->ClientReceiveLocalizedMessage(UUTShowdownRewardMessage::StaticClass(), 1, PC->PlayerState, NULL, NULL);
						}
					}
					for (AController* C : KillerTeam->GetTeamMembers())
					{
						AUTPlayerController* PC = Cast<AUTPlayerController>(C);
						if (PC && PC->GetPawn() != NULL && !PC->GetPawn()->bTearOff)
						{
							PC->ClientReceiveLocalizedMessage(UUTShowdownRewardMessage::StaticClass(), 0, PC->PlayerState, NULL, NULL);
						}
					}
				}
				else if (AliveCount == 0)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(Killer);
					if (PC && (Killer != Other))
					{
						if (KillerPS && (KillerPS->RoundKills >= FMath::Min(3, TeamCount)))
						{
							PC->ClientReceiveLocalizedMessage(UUTShowdownRewardMessage::StaticClass(), 3, PC->PlayerState, NULL, NULL);
						}
						else
						{
							PC->ClientReceiveLocalizedMessage(UUTShowdownRewardMessage::StaticClass(), 3, PC->PlayerState, NULL, NULL);
						}
					}
					KillerTeam->Score += 1;
					if (LastRoundWinner == NULL)
					{
						LastRoundWinner = KillerTeam;
					}
					else if (LastRoundWinner != KillerTeam)
					{
						LastRoundWinner = NULL; // both teams got a point so nobody won
					}

					// this is delayed so mutual kills can happen
					SetTimerUFunc(this, FName(TEXT("StartIntermission")), 1.0f, false);
				}
			}
		}
		AddKillEventToReplay(Killer, Other, DamageType);
		if (BaseMutator != NULL)
		{
			BaseMutator->ScoreKill(Killer, Other, DamageType);
		}
		FindAndMarkHighScorer();
	}
}

AInfo* AUTTeamShowdownGame::GetTiebreakWinner(FName* WinReason) const
{
	TArray<int32> LivingPlayersPerTeam;
	TArray<int32> TotalHealthPerTeam;

	// end round; team with most players alive wins
	// if equal, most total health wins
	// if that's equal too, both teams get a point
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AUTCharacter* UTC = Cast<AUTCharacter>(It->Get());
		if (UTC != NULL && !UTC->IsDead())
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTC->PlayerState);
			if (PS != NULL && PS->Team != NULL)
			{
				LivingPlayersPerTeam.SetNumZeroed(FMath::Max<int32>(LivingPlayersPerTeam.Num(), PS->Team->TeamIndex + 1));
				TotalHealthPerTeam.SetNumZeroed(FMath::Max<int32>(TotalHealthPerTeam.Num(), PS->Team->TeamIndex + 1));
				LivingPlayersPerTeam[PS->Team->TeamIndex]++;
				TotalHealthPerTeam[PS->Team->TeamIndex] += UTC->Health;
			}
		}
	}

	int32 BestNumIndex = INDEX_NONE;
	int32 BestNum = 0;
	int32 BestHealthIndex = INDEX_NONE;
	int32 BestHealth = 0;
	for (int32 i = 0; i < LivingPlayersPerTeam.Num(); i++)
	{
		if (LivingPlayersPerTeam[i] > BestNum)
		{
			BestNumIndex = i;
			BestNum = LivingPlayersPerTeam[i];
		}
		else if (LivingPlayersPerTeam[i] == BestNum)
		{
			BestNumIndex = INDEX_NONE;
		}
		if (TotalHealthPerTeam[i] > BestHealth)
		{
			BestHealthIndex = i;
			BestHealth = TotalHealthPerTeam[i];
		}
		else if (TotalHealthPerTeam[i] == BestHealth)
		{
			BestHealthIndex = INDEX_NONE;
		}
	}

	if (BestNumIndex != INDEX_NONE)
	{
		if (WinReason != NULL)
		{
			*WinReason = FName(TEXT("Count"));
		}
		return Teams[BestNumIndex];
	}
	else if (BestHealthIndex != INDEX_NONE)
	{
		if (WinReason != NULL)
		{
			*WinReason = FName(TEXT("Health"));
		}
		return Teams[BestHealthIndex];
	}
	else
	{
		return NULL;
	}
}

void AUTTeamShowdownGame::ScoreExpiredRoundTime()
{
	FName WinReason = NAME_None;
	LastRoundWinner = Cast<AUTTeamInfo>(GetTiebreakWinner(&WinReason));
	if (LastRoundWinner == NULL)
	{
		for (AUTTeamInfo* Team : Teams)
		{
			Team->Score += 1.0f;
			Team->ForceNetUpdate();
		}
		BroadcastLocalized(NULL, UUTTeamShowdownGameMessage::StaticClass(), 2);
	}
	else
	{
		LastRoundWinner->Score += 1.0f;
		LastRoundWinner->ForceNetUpdate();
		BroadcastLocalized(NULL, UUTTeamShowdownGameMessage::StaticClass(), (WinReason == FName(TEXT("Count"))) ? 0 : 1, NULL, NULL, LastRoundWinner);
	}
}

void AUTTeamShowdownGame::GetGameURLOptions(const TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps, TArray<FString>& OptionsList, int32& DesiredPlayerCount)
{
	Super::GetGameURLOptions(MenuProps, OptionsList, DesiredPlayerCount);
	DesiredPlayerCount = BotFillCount;
}
void AUTTeamShowdownGame::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	Super::CreateGameURLOptions(MenuProps);
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount, TEXT("BotFill"))));
}
#if !UE_SERVER
void AUTTeamShowdownGame::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
	CreateGameURLOptions(ConfigProps);

	TSharedPtr< TAttributeProperty<int32> > CombatantsAttr = StaticCastSharedPtr<TAttributeProperty<int32>>(FindGameURLOption(ConfigProps, TEXT("BotFill")));

	if (CombatantsAttr.IsValid())
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
					.Text(NSLOCTEXT("UTGameMode", "NumCombatants", "Number of Combatants"))
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
						.Text(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
					) :
					StaticCastSharedRef<SWidget>(
						SNew(SNumericEntryBox<int32>)
						.Value(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
						.OnValueChanged(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
						.AllowSpin(true)
						.Delta(1)
						.MinValue(1)
						.MaxValue(32)
						.MinSliderValue(1)
						.MaxSliderValue(32)
						.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")

					)
				]
			]
		];
	}

	Super::CreateConfigWidgets(MenuSpace, bCreateReadOnly, ConfigProps);
}
#endif