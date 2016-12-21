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
#include "UTAssistMessage.h"
#include "UTLineUpHelper.h"
#include "UTLineUpZone.h"
#include "UTCharacter.h"
#include "StatNames.h"
#include "UTATypes.h"

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
	CTFScoringClass = AUTBaseScoring::StaticClass();
	IntermissionDuration = 10.f;
	FlagCapScore = 1;

	//Add the translocator here for now :(
	TranslocatorObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Weapons/Translocator/BP_Translocator.BP_Translocator_C"));
	bGameHasTranslocator = true;
}

void AUTCTFBaseGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	if (bGameHasTranslocator && !TranslocatorObject.IsNull())
	{
		TranslocatorClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *TranslocatorObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
		DefaultInventory.Add(TranslocatorClass);
	}

	Super::InitGame(MapName, Options, ErrorMessage);
}

void AUTCTFBaseGame::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;
	CTFScoring = GetWorld()->SpawnActor<AUTBaseScoring>(CTFScoringClass, SpawnInfo);
	CTFScoring->InitFor(this);
}

void AUTCTFBaseGame::InitGameState()
{
	Super::InitGameState();
	// Store a cached reference to the GameState
	CTFGameState = Cast<AUTCTFGameState>(GameState);
	CTFGameState->SetMaxNumberOfTeams(NumTeams);
}

int32 AUTCTFBaseGame::PickCheatWinTeam()
{
	return (FMath::FRand() < 0.5f) ? 0 : 1;
}

void AUTCTFBaseGame::CheatScore()
{
	if ((UE_BUILD_DEVELOPMENT || (GetNetMode() == NM_Standalone)) && !bOfflineChallenge && !bBasicTrainingGame)
	{
		int32 ScoringTeam = PickCheatWinTeam();
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
			TrackKillAssists(Killer, Other, KilledPawn, DamageType, AttackerPS, OtherPlayerState);
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
	if (FlagBase != NULL)
	{
		CTFGameState->CacheFlagBase(FlagBase);
	}
}

int32 AUTCTFBaseGame::GetFlagCapScore()
{
	return FlagCapScore;
}

void AUTCTFBaseGame::ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	if (Holder != NULL && Holder->Team != NULL && !CTFGameState->HasMatchEnded() && !CTFGameState->IsMatchIntermission())
	{
		int32 NewFlagCapScore = GetFlagCapScore();
		CTFScoring->ScoreObject(GameObject, HolderPawn, Holder, Reason, NewFlagCapScore);

		if (BaseMutator != NULL)
		{
			BaseMutator->ScoreObject(GameObject, HolderPawn, Holder, Reason);
		}
		FindAndMarkHighScorer();

		if (Reason == FName("FlagCapture"))
		{
			// Give the team a capture.
			int32 OldScore = Holder->Team->Score;
			Holder->Team->Score += NewFlagCapScore;
			Holder->Team->ForceNetUpdate();
			LastTeamToScore = Holder->Team;
			BroadcastCTFScore(Holder, Holder->Team, OldScore);
			AddCaptureEventToReplay(Holder, Holder->Team);
			if (Holder->FlagCaptures == 3)
			{
				BroadcastLocalized(this, UUTAssistMessage::StaticClass(), 5, Holder, NULL, Holder->Team);
			}

			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
				if (PC)
				{
					if (CTFGameState->FlagBases[Holder->Team->TeamIndex] != nullptr)
					{
						PC->UTClientPlaySound(CTFGameState->FlagBases[Holder->Team->TeamIndex]->FlagScoreRewardSound);
					}

					AUTPlayerState* PS = Cast<AUTPlayerState>((*Iterator)->PlayerState);
					if (PS && PS->bNeedsAssistAnnouncement)
					{
						PC->SendPersonalMessage(UUTAssistMessage::StaticClass(), 2, PS, Holder, NULL);
						PS->bNeedsAssistAnnouncement = false;
					}
				}
			}
			HandleFlagCapture(HolderPawn, Holder);
			if (IsMatchInProgress())
			{
				// tell bots about the cap
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS != NULL)
				{
					for (AUTTeamInfo* TeamIter : GS->Teams)
					{
						TeamIter->NotifyObjectiveEvent(GameObject->HomeBase, HolderPawn->Controller, FName(TEXT("FlagCap")));
					}
				}
			}
		}
	}
}

void AUTCTFBaseGame::BroadcastCTFScore(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam, int32 OldScore)
{
	// find best competing score - assume this is called after scores are updated.
	int32 BestScore = 0;
	for (int32 i = 0; i < Teams.Num(); i++)
	{
		if ((Teams[i] != ScoringTeam) && (Teams[i]->Score >= BestScore))
		{
			BestScore = Teams[i]->Score;
		}
	}
	BroadcastLocalized(this, UUTCTFRewardMessage::StaticClass(), 3 + ScoringTeam->TeamIndex, ScoringPlayer, NULL, ScoringTeam);

	if ((OldScore > BestScore) && (OldScore <= BestScore + 2) && (ScoringTeam->Score > BestScore + 2))
	{
		BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 8, ScoringPlayer, NULL, ScoringTeam);
	}
	else if (ScoringTeam->Score >= ((MercyScore > 0) ? (BestScore + MercyScore - 1) : (BestScore + 4)))
	{
		BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), bHasBroadcastDominating ? 2 : 9, ScoringPlayer, NULL, ScoringTeam);
		bHasBroadcastDominating = true;
	}
	else
	{
		bHasBroadcastDominating = false; // since other team scored, need new reminder if mercy rule might be hit again
		BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 2, ScoringPlayer, NULL, ScoringTeam);
	}
}

void AUTCTFBaseGame::HandleFlagCapture(AUTCharacter* HolderPawn, AUTPlayerState* Holder)
{
	CheckScore(Holder);
}

void AUTCTFBaseGame::CheckGameTime()
{
	if (CTFGameState->IsMatchIntermission())
	{
		if (CTFGameState->GetRemainingTime() <= 0)
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

	if (!CTFGameState->LineUpHelper || !CTFGameState->LineUpHelper->bIsActive)
	{
		// Init targets
		for (int32 i = 0; i < Teams.Num(); i++)
		{
			PlacePlayersAroundFlagBase(i, i);
		}
	}
	
	UTGameState->PrepareForIntermission();

	// Tell the controllers to look at own team flag
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC != NULL)
		{
			PC->ClientPrepareForIntermission();
			int32 TeamToWatch = IntermissionTeamToView(PC);
			PC->SetViewTarget(CTFGameState->FlagBases[TeamToWatch]);
			PC->FlushPressedKeys();
		}
	}

	CTFGameState->bIsAtIntermission = true;
	CTFGameState->OnIntermissionChanged();
	CTFGameState->SetTimeLimit(IntermissionDuration);	// Reset the Game Clock for intermission
}

int32 AUTCTFBaseGame::IntermissionTeamToView(AUTPlayerController* PC)
{
	if (PC && !PC->PlayerState->bOnlySpectator && (PC->GetTeamNum() < Teams.Num()))
	{
		return PC->GetTeamNum();
	}

	return (Teams[1]->Score > Teams[0]->Score) ? 1 : 0;
}

void AUTCTFBaseGame::HandleExitingIntermission()
{
	RemoveAllPawns();

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* TestActor = *It;
		if (TestActor && !TestActor->IsPendingKill() && TestActor->IsA<AUTProjectile>())
		{
			TestActor->Destroy();
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
		AController* Controller = Iterator->Get();
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
	if (!GetWorld() || !CTFGameState)
	{
		return;
	}
	int32 WinnerTeamNum = Winner ? Winner->GetTeamNum() : (LastTeamToScore ? LastTeamToScore->TeamIndex : 0);
	AUTCTFFlagBase* WinningBase = NULL;
	WinningBase = CTFGameState->FlagBases[WinnerTeamNum];
	PlacePlayersAroundFlagBase(WinnerTeamNum, WinnerTeamNum);

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
				Controller->GameHasEnded(BaseToView, (Controller->UTPlayerState->Team && (Controller->UTPlayerState->Team->TeamIndex == WinnerTeamNum)));
			}
		}
	}
}

void AUTCTFBaseGame::UpdateSkillRating()
{
	if (bRankedSession)
	{
		ReportRankedMatchResults(GetRankedLeagueName());
	}
	else
	{
		ReportRankedMatchResults(NAME_CTFSkillRating.ToString());
	}
}

FString AUTCTFBaseGame::GetRankedLeagueName()
{
	return NAME_RankedCTFSkillRating.ToString();
}

bool AUTCTFBaseGame::SkipPlacement(AUTCharacter* UTChar)
{
	return false;
}

void AUTCTFBaseGame::RestartPlayer(AController* aPlayer)
{
	if ((!IsMatchInProgress() && bPlacingPlayersAtIntermission) || (GetMatchState() == MatchState::MatchIntermission))
	{
		// placing players during intermission
		if (bPlacingPlayersAtIntermission)
		{
			AGameMode::RestartPlayer(aPlayer);
		}
		return;
	}
	Super::RestartPlayer(aPlayer);
}

void AUTCTFBaseGame::PlacePlayersAroundFlagBase(int32 TeamNum, int32 FlagTeamNum)
{
	if ((CTFGameState == NULL) || (FlagTeamNum >= CTFGameState->FlagBases.Num()) || (CTFGameState->FlagBases[FlagTeamNum] == NULL) || (CTFGameState->LineUpHelper != NULL && CTFGameState->LineUpHelper->bIsActive))
	{
		return;
	}

	TArray<AController*> Members = Teams[TeamNum]->GetTeamMembers();
	const int32 MaxPlayers = FMath::Min(8, Members.Num());

	FVector FlagLoc = CTFGameState->FlagBases[FlagTeamNum]->GetActorLocation();
	float AngleSlices = 360.0f / MaxPlayers;
	int32 PlacementCounter = 0;
	bPlacingPlayersAtIntermission = true;
	// respawn dead pawns
	for (AController* C : Members)
	{
		AUTPlayerState* PS = C ? Cast<AUTPlayerState>(C->PlayerState) : nullptr;
		if (PS && PS->Team && (PS->Team->TeamIndex == TeamNum))
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(C->GetPawn());
			if (!UTChar || UTChar->IsDead() || UTChar->IsRagdoll())
			{
				if (C->GetPawn())
				{
					C->UnPossess();
				}
				RestartPlayer(C);
				if (C->GetPawn())
				{
					C->GetPawn()->TurnOff();
				}
			}
		}
	}
	bPlacingPlayersAtIntermission = false;
	bool bSecondLevel = false;
	FVector PlacementOffset = FVector(200.f, 0.f, 0.f);
	float StartAngle = 0.f;
	for (AController* C : Members)
	{
		AUTCharacter* UTChar = C ? Cast<AUTCharacter>(C->GetPawn()) : NULL;
		if (UTChar && !UTChar->IsDead() && !SkipPlacement(UTChar))
		{
			while (PlacementCounter < MaxPlayers)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(UTChar->PlayerState);
				if (PS && PS->CarriedObject && PS->CarriedObject->HolderTrail)
				{
					PS->CarriedObject->HolderTrail->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				}
				FRotator AdjustmentAngle(0, StartAngle + AngleSlices * PlacementCounter, 0);
				FVector PlacementLoc = FlagLoc + AdjustmentAngle.RotateVector(PlacementOffset);
				PlacementLoc.Z += UTChar->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 1.1f;
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
				UTChar->TurnOff();
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
	return (PS->StartTime <  UTGameState->ElapsedTime) ? PS->Score * 60.f / (UTGameState->ElapsedTime - PS->StartTime) : 0.f;
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
	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "LargeArmorPickups", "Large Armor Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_ArmorVestCount)), StatList);
	NewPlayerInfoLine(BotLeftPane, NSLOCTEXT("AUTGameMode", "MediumArmorPickups", "Medium Armor Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_ArmorPadsCount)), StatList);

	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "UDamage", "UDamage Control"), MakeShareable(new TAttributeStat(PlayerState, NAME_UDamageTime, nullptr, ToTime)), StatList);
	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "Berserk", "Berserk Control"), MakeShareable(new TAttributeStat(PlayerState, NAME_BerserkTime, nullptr, ToTime)), StatList);
	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "Invisibility", "Invisibility Control"), MakeShareable(new TAttributeStat(PlayerState, NAME_InvisibilityTime, nullptr, ToTime)), StatList);

	BotRightPane->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(60.0f)];
	NewPlayerInfoLine(BotRightPane, NSLOCTEXT("AUTGameMode", "SmallArmorPickups", "Small Armor Pickups"), MakeShareable(new TAttributeStat(PlayerState, NAME_HelmetCount)), StatList);
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

uint8 AUTCTFBaseGame::GetNumMatchesFor(AUTPlayerState* PS, bool InbRankedSession) const
{
	return PS ? PS->CTFMatchesPlayed : 0;
}

int32 AUTCTFBaseGame::GetEloFor(AUTPlayerState* PS, bool InbRankedSession) const
{
	return PS ? PS->CTFRank : Super::GetEloFor(PS, InbRankedSession);
}

void AUTCTFBaseGame::SetEloFor(AUTPlayerState* PS, bool InbRankedSession, int32 NewEloValue, bool bIncrementMatchCount)
{
	if (PS)
	{
		PS->CTFRank = NewEloValue;
		if (bIncrementMatchCount && (PS->CTFMatchesPlayed < 255))
		{
			PS->CTFMatchesPlayed++;
		}
	}
}

int32 AUTCTFBaseGame::GetComSwitch(FName CommandTag, AActor* ContextActor, AUTPlayerController* InInstigator, UWorld* World)
{
	if (CommandTag == CommandTags::Distress)
	{
		return UNDER_HEAVY_ATTACK_SWITCH_INDEX;
	}

	return Super::GetComSwitch(CommandTag, ContextActor, InInstigator, World);
}
