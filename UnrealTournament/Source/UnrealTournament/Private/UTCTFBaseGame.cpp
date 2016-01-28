// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFBaseGame.h"
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

AUTCTFBaseGame::AUTCTFBaseGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// By default, we do 2 team CTF
	NumTeams = 2;

	HUDClass = AUTHUD_CTF::StaticClass();
	GameStateClass = AUTCTFGameState::StaticClass();
	bUseTeamStarts = true;

	MapPrefix = TEXT("CTF");
	SquadType = AUTCTFSquadAI::StaticClass();
	CTFScoringClass = AUTCTFScoring::StaticClass();
	IntermissionDuration = 10.f;
	FlagCapScore = 1;

	//Add the translocator here for now :(
	TranslocatorObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Weapons/Translocator/BP_Translocator.BP_Translocator_C"));
}

void AUTCTFBaseGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	if (!TranslocatorObject.IsNull())
	{
		TSubclassOf<AUTWeapon> WeaponClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *TranslocatorObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
		DefaultInventory.Add(WeaponClass);
	}

	Super::InitGame(MapName, Options, ErrorMessage);
}

void AUTCTFBaseGame::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;
	CTFScoring = GetWorld()->SpawnActor<AUTCTFScoring>(CTFScoringClass, SpawnInfo);
	CTFScoring->CTFGameState = CTFGameState;
}

void AUTCTFBaseGame::InitGameState()
{
	Super::InitGameState();
	// Store a cached reference to the GameState
	CTFGameState = Cast<AUTCTFGameState>(GameState);
	CTFGameState->SetMaxNumberOfTeams(NumTeams);
}

void AUTCTFBaseGame::CheatScore()
{
	if ((GetNetMode() == NM_Standalone) && !bOfflineChallenge && !bBasicTrainingGame)
	{
		int32 ScoringTeam = (FMath::FRand() < 0.5f) ? 0 : 1;
		TArray<AController*> Members = Teams[ScoringTeam]->GetTeamMembers();
		if (Members.Num() > 0)
		{
			AUTPlayerState* Scorer = Cast<AUTPlayerState>(Members[FMath::RandHelper(Members.Num())]->PlayerState);
			if (FMath::FRand() < 0.5f)
			{
				FAssistTracker NewAssist;
				NewAssist.Holder = Cast<AUTPlayerState>(Members[FMath::RandHelper(Members.Num())]->PlayerState);
				NewAssist.TotalHeldTime = 0.5f;
				CTFGameState->FlagBases[ScoringTeam]->GetCarriedObject()->AssistTracking.Add(NewAssist);
			}
			if (FMath::FRand() < 0.5f)
			{
				CTFGameState->FlagBases[ScoringTeam]->GetCarriedObject()->HolderRescuers.Add(Members[FMath::RandHelper(Members.Num())]);
			}
			if (FMath::FRand() < 0.5f)
			{
				Cast<AUTPlayerState>(Members[FMath::RandHelper(Members.Num())]->PlayerState)->LastFlagReturnTime = GetWorld()->GetTimeSeconds() - 0.1f;
			}
			ScoreObject(CTFGameState->FlagBases[ScoringTeam]->GetCarriedObject(), Cast<AUTCharacter>(Cast<AController>(Scorer->GetOwner())->GetPawn()), Scorer, FName("FlagCapture"));
		}
	}
}

void AUTCTFBaseGame::AddCaptureEventToReplay(AUTPlayerState* Holder, AUTTeamInfo* Team)
{
	UDemoNetDriver* DemoNetDriver = GetWorld()->DemoNetDriver;
	if (DemoNetDriver != nullptr && DemoNetDriver->ServerConnection == nullptr)
	{
		TArray<uint8> Data;

		FString PlayerName = Holder ? *Holder->PlayerName : TEXT("None");
		PlayerName.ReplaceInline(TEXT(" "), TEXT("%20"));

		FString CapInfo = FString::Printf(TEXT("%s"), *PlayerName);

		FMemoryWriter MemoryWriter(Data);
		MemoryWriter.Serialize(TCHAR_TO_ANSI(*CapInfo), CapInfo.Len() + 1);

		FString MetaTag = FString::FromInt(Team->TeamIndex);

		DemoNetDriver->AddEvent(TEXT("FlagCaps"), MetaTag, Data);
	}
}

void AUTCTFBaseGame::AddReturnEventToReplay(AUTPlayerState* Returner, AUTTeamInfo* Team)
{
	UDemoNetDriver* DemoNetDriver = GetWorld()->DemoNetDriver;
	if (Returner && DemoNetDriver != nullptr && DemoNetDriver->ServerConnection == nullptr)
	{
		TArray<uint8> Data;

		FString PlayerName = Returner ? *Returner->PlayerName : TEXT("None");
		PlayerName.ReplaceInline(TEXT(" "), TEXT("%20"));

		FString ReturnInfo = FString::Printf(TEXT("%s"), *PlayerName);

		FMemoryWriter MemoryWriter(Data);
		MemoryWriter.Serialize(TCHAR_TO_ANSI(*ReturnInfo), ReturnInfo.Len() + 1);

		FString MetaTag = Returner->StatsID;
		if (MetaTag.IsEmpty())
		{
			MetaTag = Returner->PlayerName;
		}
		DemoNetDriver->AddEvent(TEXT("FlagReturns"), MetaTag, Data);
	}
}

void AUTCTFBaseGame::AddDeniedEventToReplay(APlayerState* KillerPlayerState, AUTPlayerState* Holder, AUTTeamInfo* Team)
{
	UDemoNetDriver* DemoNetDriver = GetWorld()->DemoNetDriver;
	if (DemoNetDriver != nullptr && DemoNetDriver->ServerConnection == nullptr)
	{
		TArray<uint8> Data;

		FString PlayerName = KillerPlayerState ? *KillerPlayerState->PlayerName : TEXT("None");
		PlayerName.ReplaceInline(TEXT(" "), TEXT("%20"));

		FString HolderName = Holder ? *Holder->PlayerName : TEXT("None");
		HolderName.ReplaceInline(TEXT(" "), TEXT("%20"));

		FString DenyInfo = FString::Printf(TEXT("%s %s"), *PlayerName, *HolderName);

		FMemoryWriter MemoryWriter(Data);
		MemoryWriter.Serialize(TCHAR_TO_ANSI(*DenyInfo), DenyInfo.Len() + 1);

		FString MetaTag = FString::FromInt(Team->TeamIndex);

		DemoNetDriver->AddEvent(TEXT("FlagDeny"), MetaTag, Data);
	}
}

void AUTCTFBaseGame::ScoreDamage_Implementation(int32 DamageAmount, AUTPlayerState* Victim, AUTPlayerState* Attacker)
{
	Super::ScoreDamage_Implementation(DamageAmount, Victim, Attacker);
	CTFScoring->ScoreDamage(DamageAmount, Victim, Attacker);
}

void AUTCTFBaseGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	CTFScoring->ScoreKill(Killer, Other, KilledPawn, DamageType);
	if ((Killer != NULL && Killer != Other))
	{
		AddKillEventToReplay(Killer, Other, DamageType);

		AUTPlayerState* AttackerPS = Cast<AUTPlayerState>(Killer->PlayerState);
		if (AttackerPS != NULL)
		{
			if (!bFirstBloodOccurred)
			{
				BroadcastLocalized(this, UUTFirstBloodMessage::StaticClass(), 0, AttackerPS, NULL, NULL);
				bFirstBloodOccurred = true;
			}
			AUTPlayerState* OtherPlayerState = Other ? Cast<AUTPlayerState>(Other->PlayerState) : NULL;
			AttackerPS->IncrementKills(DamageType, true, OtherPlayerState);
		}
	}
	if (BaseMutator != NULL)
	{
		BaseMutator->ScoreKill(Killer, Other, DamageType);
	}
	FindAndMarkHighScorer();
}

void AUTCTFBaseGame::GameObjectiveInitialized(AUTGameObjective* Obj)
{
	AUTCTFFlagBase* FlagBase = Cast<AUTCTFFlagBase>(Obj);
	if (FlagBase != NULL && FlagBase->MyFlag)
	{
		CTFGameState->CacheFlagBase(FlagBase);
	}
}

void AUTCTFBaseGame::ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	if (Holder != NULL && Holder->Team != NULL && !CTFGameState->HasMatchEnded() && !CTFGameState->IsMatchIntermission())
	{
		CTFScoring->ScoreObject(GameObject, HolderPawn, Holder, Reason, TimeLimit);

		if (BaseMutator != NULL)
		{
			BaseMutator->ScoreObject(GameObject, HolderPawn, Holder, Reason);
		}
		FindAndMarkHighScorer();

		if (Reason == FName("FlagCapture"))
		{
			// Give the team a capture.
			Holder->Team->Score += FlagCapScore;
			Holder->Team->ForceNetUpdate();
			LastTeamToScore = Holder->Team;
			BroadcastScoreUpdate(Holder, Holder->Team);
			AddCaptureEventToReplay(Holder, Holder->Team);
			if (Holder->FlagCaptures == 3)
			{
				BroadcastLocalized(this, UUTCTFRewardMessage::StaticClass(), 5, Holder, NULL, Holder->Team);
			}

			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
				if (PC)
				{
					PC->ClientPlaySound(CTFGameState->FlagBases[Holder->Team->TeamIndex]->FlagScoreRewardSound, 2.f);

					AUTPlayerState* PS = Cast<AUTPlayerState>((*Iterator)->PlayerState);
					if (PS && PS->bNeedsAssistAnnouncement)
					{
						PC->SendPersonalMessage(UUTCTFRewardMessage::StaticClass(), 2, PS, Holder, NULL);
						PS->bNeedsAssistAnnouncement = false;
					}
				}
			}
			HandleFlagCapture(Holder);
		}
	}
}

void AUTCTFBaseGame::HandleFlagCapture(AUTPlayerState* Holder)
{
	CheckScore(Holder);
}

void AUTCTFBaseGame::CheckGameTime()
{
	if (CTFGameState->IsMatchIntermission())
	{
		if (CTFGameState->RemainingTime <= 0)
		{
			SetMatchState(MatchState::MatchExitingIntermission);
		}
	}
}

void AUTCTFBaseGame::CallMatchStateChangeNotify()
{
	Super::CallMatchStateChangeNotify();

	if (MatchState == MatchState::MatchIntermission)
	{
		HandleMatchIntermission();
	}
	else if (MatchState == MatchState::MatchExitingIntermission)
	{
		HandleExitingIntermission();
	}
}

void AUTCTFBaseGame::HandleMatchIntermission()
{
	//UTGameState->UpdateMatchHighlights();
	CTFGameState->ResetFlags();

	// Figure out who we should look at
	// Init targets
	TArray<AUTCharacter*> BestPlayers;
	for (int32 i = 0; i<Teams.Num(); i++)
	{
		BestPlayers.Add(NULL);
		PlacePlayersAroundFlagBase(i);
	}

	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AController* Controller = *Iterator;
		if (Controller->GetPawn() != NULL && Controller->PlayerState != NULL)
		{
			AUTCharacter* Char = Cast<AUTCharacter>(Controller->GetPawn());
			if (Char != NULL)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(Controller->PlayerState);
				uint8 TeamNum = PS->GetTeamNum();
				if (TeamNum < BestPlayers.Num())
				{
					if (BestPlayers[TeamNum] == NULL || PS->Score > BestPlayers[TeamNum]->PlayerState->Score || Cast<AUTCTFFlag>(PS->CarriedObject) != NULL)
					{
						BestPlayers[TeamNum] = Char;
					}
				}
			}
		}
	}

	// Tell the controllers to look at own team flag
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC != NULL)
		{
			PC->ClientHalftime();
			int32 TeamToWatch = IntermissionTeamToView(PC);
			PC->SetViewTarget(CTFGameState->FlagBases[TeamToWatch]);
		}
	}

	// Freeze all of the pawns
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (*It && !Cast<ASpectatorPawn>((*It).Get()))
		{
			(*It)->TurnOff();
		}
	}

	CTFGameState->bIsAtIntermission = true;
	CTFGameState->OnIntermissionChanged();
	CTFGameState->SetTimeLimit(IntermissionDuration);	// Reset the Game Clock for intermission
}

int32 AUTCTFBaseGame::IntermissionTeamToView(AUTPlayerController* PC)
{
	if (!PC->PlayerState->bOnlySpectator && (PC->GetTeamNum() < Teams.Num()))
	{
		return PC->GetTeamNum();
	}

	return (Teams[1]->Score > Teams[0]->Score) ? 1 : 0;
}

void AUTCTFBaseGame::HandleExitingIntermission()
{
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		// Detach all controllers from their pawns
		if ((*Iterator)->GetPawn() != NULL)
		{
			(*Iterator)->UnPossess();
		}
	}

	TArray<APawn*> PawnsToDestroy;
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (*It && !Cast<ASpectatorPawn>((*It).Get()))
		{
			PawnsToDestroy.Add(*It);
		}
	}

	for (int32 i = 0; i<PawnsToDestroy.Num(); i++)
	{
		APawn* Pawn = PawnsToDestroy[i];
		if (Pawn != NULL && !Pawn->IsPendingKill())
		{
			Pawn->Destroy();
		}
	}

	// swap sides, if desired
	AUTWorldSettings* Settings = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
	if (Settings != NULL && Settings->bAllowSideSwitching)
	{
		CTFGameState->ChangeTeamSides(1);
	}

	// reset everything
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->ImplementsInterface(UUTResetInterface::StaticClass()))
		{
			IUTResetInterface::Execute_Reset(*It);
		}
	}

	//now respawn all the players
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator)
		{
			RestartPlayer(Controller);
		}
	}

	// Send all flags home..
	CTFGameState->ResetFlags();
	CTFGameState->bIsAtIntermission = false;
	CTFGameState->OnIntermissionChanged();
	CTFGameState->SetTimeLimit(TimeLimit);		// Reset the GameClock for the second time.
	SetMatchState(MatchState::InProgress);
}

void AUTCTFBaseGame::EndGame(AUTPlayerState* Winner, FName Reason)
{
	// Dont ever end the game in PIE
	if (GetWorld()->WorldType == EWorldType::PIE) return;

	Super::EndGame(Winner, Reason);

	// Send all of the flags home...
	CTFGameState->ResetFlags();
}

void AUTCTFBaseGame::SetEndGameFocus(AUTPlayerState* Winner)
{
	int32 WinnerTeamNum = Winner ? Winner->GetTeamNum() : (LastTeamToScore ? LastTeamToScore->TeamIndex : 0);
	AUTCTFFlagBase* WinningBase = NULL;
	WinningBase = CTFGameState->FlagBases[WinnerTeamNum];
	PlacePlayersAroundFlagBase(WinnerTeamNum);

	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* Controller = Cast<AUTPlayerController>(*Iterator);
		if (Controller && Controller->UTPlayerState)
		{
			AUTCTFFlagBase* BaseToView = WinningBase;
			// If we don't have a winner, view my base
			if (BaseToView == NULL)
			{
				AUTTeamInfo* MyTeam = Controller->UTPlayerState->Team;
				if (MyTeam)
				{
					BaseToView = CTFGameState->FlagBases[MyTeam->GetTeamNum()];
				}
			}

			if (BaseToView)
			{
				Controller->GameHasEnded(BaseToView, Controller->UTPlayerState->Team->TeamIndex == WinnerTeamNum);
			}
		}
	}
}

void AUTCTFBaseGame::UpdateSkillRating()
{
	for (int32 PlayerIdx = 0; PlayerIdx < UTGameState->PlayerArray.Num(); PlayerIdx++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
		if (PS && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(NAME_CTFSkillRating, PS->Team == UTGameState->WinningTeam, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}

	for (int32 PlayerIdx = 0; PlayerIdx < InactivePlayerArray.Num(); PlayerIdx++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(InactivePlayerArray[PlayerIdx]);
		if (PS && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(NAME_CTFSkillRating, PS->Team == UTGameState->WinningTeam, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}
}

void AUTCTFBaseGame::PlacePlayersAroundFlagBase(int32 TeamNum)
{
	if ((CTFGameState == NULL) || (TeamNum >= CTFGameState->FlagBases.Num()) || (CTFGameState->FlagBases[TeamNum] == NULL))
	{
		return;
	}

	TArray<AController*> Members = Teams[TeamNum]->GetTeamMembers();
	const int32 MaxPlayers = FMath::Min(8, Members.Num());

	FVector FlagLoc = CTFGameState->FlagBases[TeamNum]->GetActorLocation();
	float AngleSlices = 360.0f / MaxPlayers;
	int32 PlacementCounter = 0;

	// respawn dead pawns
	for (AController* C : Members)
	{
		if (C)
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(C->GetPawn());
			if (!UTChar || UTChar->IsDead())
			{
				if (C->GetPawn())
				{
					C->UnPossess();
				}
				RestartPlayer(C);
			}
		}
	}

	bool bSecondLevel = false;
	FVector PlacementOffset = FVector(200.f, 0.f, 0.f);
	float StartAngle = 0.f;
	for (AController* C : Members)
	{
		AUTCharacter* UTChar = C ? Cast<AUTCharacter>(C->GetPawn()) : NULL;
		if (UTChar && !UTChar->IsDead())
		{
			while (PlacementCounter < MaxPlayers)
			{
				FRotator AdjustmentAngle(0, StartAngle + AngleSlices * PlacementCounter, 0);
				FVector PlacementLoc = FlagLoc + AdjustmentAngle.RotateVector(PlacementOffset);
				PlacementLoc.Z += UTChar->GetSimpleCollisionHalfHeight() * 1.1f;
				PlacementCounter++;
				if ((PlacementCounter == 8) && !bSecondLevel)
				{
					bSecondLevel = true;
					PlacementOffset = FVector(300.f, 0.f, 0.f);
					StartAngle = 0.5f * AngleSlices;
					PlacementCounter = 0;
				}
				UTChar->bIsTranslocating = true; // hack to get rid of teleport effect
				if (UTChar->TeleportTo(PlacementLoc, UTChar->GetActorRotation()))
				{
					break;
				}
				UTChar->bIsTranslocating = false;
			}
			if (PlacementCounter == 8)
			{
				break;
			}
		}
	}
}

#if !UE_SERVER
void AUTCTFBaseGame::BuildScoreInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<TAttributeStat> >& StatList)
{
	TAttributeStat::StatValueTextFunc TwoDecimal = [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> FText
	{
		return FText::FromString(FString::Printf(TEXT("%8.2f"), Stat->GetValue()));
	};

	TSharedPtr<SVerticalBox> TopLeftPane;
	TSharedPtr<SVerticalBox> TopRightPane;
	TSharedPtr<SVerticalBox> BotLeftPane;
	TSharedPtr<SVerticalBox> BotRightPane;
	TSharedPtr<SHorizontalBox> TopBox;
	TSharedPtr<SHorizontalBox> BotBox;
	BuildPaneHelper(TopBox, TopLeftPane, TopRightPane);
	BuildPaneHelper(BotBox, BotLeftPane, BotRightPane);

	//4x4 panes
	TSharedPtr<SVerticalBox> MainBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			TopBox.ToSharedRef()
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BotBox.ToSharedRef()
		];

	TabWidget->AddTab(NSLOCTEXT("AUTGameMode", "Score", "Score"), MainBox);

	NewPlayerInfoLine(TopLeftPane, NSLOCTEXT("AUTGameMode", "Kills", "Kills"), MakeShareable(new TAttributeStat(PlayerState, NAME_None, [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> float { return PS->Kills;	})), StatList);
	NewPlayerInfoLine(TopLeftPane, NSLOCTEXT("AUTCTFGameMode", "RegKills", " - Regular Kills"), MakeShareable(new TAttributeStat(PlayerState, NAME_None, [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> float
	{
		return PS->Kills - PS->GetStatsValue(NAME_FCKills) - PS->GetStatsValue(NAME_FlagSupportKills);
	})), StatList);
	NewPlayerInfoLine(TopLeftPane, NSLOCTEXT("AUTCTFGameMode", "FlagSupportKills", " - FC Support Kills"), MakeShareable(new TAttributeStat(PlayerState, NAME_FlagSupportKills)), StatList);
	NewPlayerInfoLine(TopLeftPane, NSLOCTEXT("AUTCTFGameMode", "EnemyFCKills", " - Enemy FC Kills"), MakeShareable(new TAttributeStat(PlayerState, NAME_FCKillPoints)), StatList);
	NewPlayerInfoLine(TopLeftPane, NSLOCTEXT("AUTCTFGameMode", "EnemyFCDamage", "Enemy FC Damage Bonus"), MakeShareable(new TAttributeStat(PlayerState, NAME_EnemyFCDamage)), StatList);
	NewPlayerInfoLine(TopLeftPane, NSLOCTEXT("AUTGameMode", "Deaths", "Deaths"), MakeShareable(new TAttributeStat(PlayerState, NAME_None, [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> float { return PS->Deaths; })), StatList);
	/*NewPlayerInfoLine(TopLeftPane, NSLOCTEXT("AUTGameMode", "ScorePM", "Score Per Minute"), MakeShareable(new TAttributeStat(PlayerState, NAME_None, [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> float
	{
	return (PS->StartTime <  PS->GetWorld()->GameState->ElapsedTime) ? PS->Score * 60.f / (PS->GetWorld()->GameState->ElapsedTime - PS->StartTime) : 0.f;
	}, TwoDecimal)), StatList);*/
	NewPlayerInfoLine(TopLeftPane, NSLOCTEXT("AUTGameMode", "KDRatio", "K/D Ratio"), MakeShareable(new TAttributeStat(PlayerState, NAME_None, [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> float
	{
		return (PS->Deaths > 0) ? float(PS->Kills) / PS->Deaths : 0.f;
	}, TwoDecimal)), StatList);



	TAttributeStat::StatValueTextFunc ToTime = [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> FText
	{
		int32 Seconds = (int32)Stat->GetValue();
		int32 Mins = Seconds / 60;
		Seconds -= Mins * 60;
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Mins, Seconds));
	};

	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "UDamagePickups", "UDamage Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_UDamageCount)), StatList);
	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "BerserkPickups", "Berserk Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_BerserkCount)), StatList);
	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "InvisibilityPickups", "Invisibility Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_InvisibilityCount)), StatList);
	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "KegPickups", "Keg Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_KegCount)), StatList);
	BotLeftPane->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(30.0f)];
	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "BeltPickups", "Shield Belt Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_ShieldBeltCount)), StatList);
	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "VestPickups", "Armor Vest Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_ArmorVestCount)), StatList);
	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "PadPickups", "Thigh Pad Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_ArmorPadsCount)), StatList);

	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "UDamage", "UDamage Control"), MakeShareable(new TAttributeStat(PlayerState, NAME_UDamageTime, nullptr, ToTime)), StatList);
	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "Berserk", "Berserk Control"), MakeShareable(new TAttributeStat(PlayerState, NAME_BerserkTime, nullptr, ToTime)), StatList);
	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "Invisibility", "Invisibility Control"), MakeShareable(new TAttributeStat(PlayerState, NAME_InvisibilityTime, nullptr, ToTime)), StatList);

	BotRightPane->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(60.0f)];
	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "HelmetPickups", "Helmet Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_HelmetCount)), StatList);
	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "JumpBootJumps", "JumpBoot Jumps"), MakeShareable(new TAttributeStat(PlayerState, NAME_BootJumps)), StatList);

	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "FlagCaps", "Flag Captures"), MakeShareable(new TAttributeStat(PlayerState, NAME_None, [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> float { return PS->FlagCaptures; })), StatList);
	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "FlagReturns", "Flag Returns"), MakeShareable(new TAttributeStat(PlayerState, NAME_None, [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> float { return PS->FlagReturns; })), StatList);
	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "FlagAssists", "Assists"), MakeShareable(new TAttributeStat(PlayerState, NAME_None, [](const AUTPlayerState* PS, const TAttributeStat* Stat) -> float { return PS->Assists; })), StatList);
	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "CarryAssists", " - Carry Assists"), MakeShareable(new TAttributeStat(PlayerState, NAME_CarryAssist)), StatList);
	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "ReturnAssists", " - Return Assists"), MakeShareable(new TAttributeStat(PlayerState, NAME_ReturnAssist)), StatList);
	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "DefendAssists", " - Support Assists"), MakeShareable(new TAttributeStat(PlayerState, NAME_DefendAssist)), StatList);
	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "FlagGrabs", "Flag Grabs"), MakeShareable(new TAttributeStat(PlayerState, NAME_FlagGrabs)), StatList);
	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "FlagHeldTime", "Flag Held Time"), MakeShareable(new TAttributeStat(PlayerState, NAME_FlagHeldTime, nullptr, ToTime)), StatList);
	NewPlayerInfoLine(TopRightPane, NSLOCTEXT("AUTCTFGameMode", "FlagDenialTime", "Flag Denial Time"), MakeShareable(new TAttributeStat(PlayerState, NAME_FlagHeldDenyTime, nullptr, ToTime)), StatList);
}
#endif

void AUTCTFBaseGame::SetRedScore(int32 NewScore)
{
	if (!bOfflineChallenge && !bBasicTrainingGame)
	{
		Teams[0]->Score = NewScore;
	}
}

void AUTCTFBaseGame::SetBlueScore(int32 NewScore)
{
	if (!bOfflineChallenge && !bBasicTrainingGame)
	{
		Teams[1]->Score = NewScore;
	}
}