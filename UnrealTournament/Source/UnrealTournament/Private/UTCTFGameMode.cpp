// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
#include "Slate/Widgets/SUTTabWidget.h"
#include "Slate/SUWPlayerInfoDialog.h"
#include "StatNames.h"
#include "Engine/DemoNetDriver.h"

namespace MatchState
{
	const FName MatchEnteringHalftime = FName(TEXT("MatchEnteringHalftime"));
	const FName MatchIsAtHalftime = FName(TEXT("MatchIsAtHalftime"));
	const FName MatchExitingHalftime = FName(TEXT("MatchExitingHalftime"));
	const FName MatchEnteringSuddenDeath = FName(TEXT("MatchEnteringSuddenDeath"));
	const FName MatchIsInSuddenDeath = FName(TEXT("MatchIsInSuddenDeath"));
}

AUTCTFGameMode::AUTCTFGameMode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// By default, we do 2 team CTF
	NumTeams = 2;
	HalftimeDuration = 30.f;
	HUDClass = AUTHUD_CTF::StaticClass();
	GameStateClass = AUTCTFGameState::StaticClass();
	bAllowOvertime = true;
	OvertimeDuration = 5;
	AdvantageDuration = 5;
	bUseTeamStarts = true;
	bSuddenDeath = true;
	MercyScore = 5;
	GoalScore = 0;
	TimeLimit = 14;
	MapPrefix = TEXT("CTF");
	SquadType = AUTCTFSquadAI::StaticClass();
	CTFScoringClass = AUTCTFScoring::StaticClass();

	//Add the translocator here for now :(
	TranslocatorObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Weapons/Translocator/BP_Translocator.BP_Translocator_C"));
	
	DisplayName = NSLOCTEXT("UTGameMode", "CTF", "Capture the Flag");
}

void AUTCTFGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	if (!TranslocatorObject.IsNull())
	{
		TSubclassOf<AUTWeapon> WeaponClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *TranslocatorObject.ToStringReference().AssetLongPathname, NULL, LOAD_NoWarn));
		DefaultInventory.Add(WeaponClass);
	}

	Super::InitGame(MapName, Options, ErrorMessage);

	bSuddenDeath = EvalBoolOptions(ParseOption(Options, TEXT("SuddenDeath")), bSuddenDeath);

	// HalftimeDuration is in seconds and used in seconds,
	HalftimeDuration = FMath::Max(0, GetIntOption(Options, TEXT("HalftimeDuration"), HalftimeDuration));

	// OvertimeDuration is in minutes
	OvertimeDuration = FMath::Max(1, GetIntOption(Options, TEXT("OvertimeDuration"), OvertimeDuration));
	OvertimeDuration *= 60;

	// AdvantageDuration is in seconds 
	AdvantageDuration = FMath::Max(0, GetIntOption(Options, TEXT("AdvantageDuration"), AdvantageDuration));

	if (TimeLimit > 0)
	{
		TimeLimit = uint32(float(TimeLimit) * 0.5);
	}
}

void AUTCTFGameMode::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;
	CTFScoring = GetWorld()->SpawnActor<AUTCTFScoring>(CTFScoringClass, SpawnInfo);
	CTFScoring->CTFGameState = CTFGameState;
}

void AUTCTFGameMode::InitGameState()
{
	Super::InitGameState();
	// Store a cached reference to the GameState
	CTFGameState = Cast<AUTCTFGameState>(GameState);
	CTFGameState->SetMaxNumberOfTeams(NumTeams);
}

void AUTCTFGameMode::CheatScore()
{
	if (GetNetMode() == NM_Standalone)
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

void AUTCTFGameMode::ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	if (Holder != NULL && Holder->Team != NULL && !CTFGameState->HasMatchEnded() && !CTFGameState->IsMatchAtHalftime() && GetMatchState() != MatchState::MatchEnteringHalftime)
	{
		CTFScoring->ScoreObject(GameObject, HolderPawn, Holder, Reason, TimeLimit);

		if (BaseMutator != NULL)
		{
			BaseMutator->ScoreObject(GameObject, HolderPawn, Holder, Reason);
		}
		FindAndMarkHighScorer();

		if ( Reason == FName("SentHome") )
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
		else if (Reason == FName("FlagCapture"))
		{
			// Give the team a capture.
			Holder->Team->Score++;
			Holder->Team->ForceNetUpdate();
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
	}
}

void AUTCTFGameMode::AddCaptureEventToReplay(AUTPlayerState* Holder, AUTTeamInfo* Team)
{
	UDemoNetDriver* DemoNetDriver = GetWorld()->DemoNetDriver;
	if (DemoNetDriver != nullptr && DemoNetDriver->ServerConnection == nullptr)
	{
		TArray<uint8> Data;
		FString CapInfo = FString::Printf(TEXT("%s"), Holder ? *Holder->PlayerName : TEXT("None"));

		FMemoryWriter MemoryWriter(Data);
		MemoryWriter.Serialize(TCHAR_TO_ANSI(*CapInfo), CapInfo.Len() + 1);

		FString MetaTag = FString::FromInt(Team->TeamIndex);

		DemoNetDriver->AddEvent(TEXT("FlagCaps"), MetaTag, Data);
	}
}

void AUTCTFGameMode::AddReturnEventToReplay(AUTPlayerState* Returner, AUTTeamInfo* Team)
{
	UDemoNetDriver* DemoNetDriver = GetWorld()->DemoNetDriver;
	if (Returner && DemoNetDriver != nullptr && DemoNetDriver->ServerConnection == nullptr)
	{
		TArray<uint8> Data;
		FString ReturnInfo = FString::Printf(TEXT("%s"), Returner ? *Returner->PlayerName : TEXT("None"));

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

void AUTCTFGameMode::AddDeniedEventToReplay(APlayerState* KillerPlayerState, AUTPlayerState* Holder, AUTTeamInfo* Team)
{
	UDemoNetDriver* DemoNetDriver = GetWorld()->DemoNetDriver;
	if (DemoNetDriver != nullptr && DemoNetDriver->ServerConnection == nullptr)
	{
		TArray<uint8> Data;
		FString CapInfo = FString::Printf(TEXT("%s %s"), KillerPlayerState ? *KillerPlayerState->PlayerName : TEXT("None"), Holder ? *Holder->PlayerName : TEXT("None"));

		FMemoryWriter MemoryWriter(Data);
		MemoryWriter.Serialize(TCHAR_TO_ANSI(*CapInfo), CapInfo.Len() + 1);

		FString MetaTag = FString::FromInt(Team->TeamIndex);

		DemoNetDriver->AddEvent(TEXT("FlagDeny"), MetaTag, Data);
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
	if (HalftimeDuration <= 0 || TimeLimit == 0)
	{
		Super::CheckGameTime();
	}
	else if ( CTFGameState->IsMatchInProgress() )
	{
		// First look to see if we are in halftime. 
		if (CTFGameState->IsMatchAtHalftime())		
		{
			if (CTFGameState->RemainingTime <= 0)
			{
				SetMatchState(MatchState::MatchExitingHalftime);
			}
		}

		// If the match is in progress and we are not playing advantage, then enter the halftime/end of game logic depending on the half
		else if (CTFGameState->RemainingTime <= 0)
		{
			if (!CTFGameState->bPlayingAdvantage)
			{
				// If we are in Overtime - Keep battling until one team wins.  We might want to add half-time or bring sudden death back 
				if ( CTFGameState->IsMatchInOvertime() )
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
		SetMatchState(MatchState::MatchEnteringHalftime);
	}
}

void AUTCTFGameMode::HandleMatchHasStarted()
{
	if (!CTFGameState->bSecondHalf)
	{
		Super::HandleMatchHasStarted();
	}
}

void AUTCTFGameMode::HandleHalftime()
{
}

void AUTCTFGameMode::PlacePlayersAroundFlagBase(int32 TeamNum)
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
	for (AController* C : Members)
	{
		AUTCharacter* UTChar = C ? Cast<AUTCharacter>(C->GetPawn()) : NULL;
		if (UTChar && !UTChar->IsDead())
		{
			while (PlacementCounter < MaxPlayers)
			{
				FRotator AdjustmentAngle(0, AngleSlices * PlacementCounter, 0);

				PlacementCounter++;

				FVector PlacementLoc = FlagLoc + AdjustmentAngle.RotateVector(FVector(200, 0, 0));
				PlacementLoc.Z += UTChar->GetSimpleCollisionHalfHeight() * 1.1f;

				FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(UTChar->GetSimpleCollisionRadius(), UTChar->GetSimpleCollisionHalfHeight());
				static const FName NAME_FlagPlacement = FName(TEXT("FlagPlacement"));
				FCollisionQueryParams CapsuleParams(NAME_FlagPlacement, false, this);
				FCollisionResponseParams ResponseParam;
				TArray<FOverlapResult> Overlaps;
				bool bEncroached = GetWorld()->OverlapMultiByChannel(Overlaps, PlacementLoc, FQuat::Identity, ECC_Pawn, CapsuleShape, CapsuleParams, ResponseParam);
				if (!bEncroached)
				{
					UTChar->SetActorLocation(PlacementLoc);
					break;
				}
			}
		}

		if (PlacementCounter == 8)
		{
			break;
		}
	}
}

void AUTCTFGameMode::HandleEnteringHalftime()
{
	UTGameState->UpdateMatchHighlights();
	CTFGameState->ResetFlags();

	// Figure out who we should look at
	// Init targets
	TArray<AUTCharacter*> BestPlayers;
	int32 BestTeam = 0;
	int32 BestTeamScore = 0;
	for (int32 i=0;i<Teams.Num();i++)
	{
		BestPlayers.Add(NULL);
		PlacePlayersAroundFlagBase(i);

		if (i == BestTeam || Teams[i]->Score > BestTeamScore)
		{
			BestTeamScore = Teams[i]->Score;
			BestTeam = i;
		}
	}
	
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
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
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC != NULL)
		{
			PC->ClientHalftime();
			int32 TeamToWatch = (!PC->PlayerState->bOnlySpectator && (PC->GetTeamNum() < Teams.Num())) ? PC->GetTeamNum() : BestTeam;
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

	CTFGameState->bHalftime = true;
	CTFGameState->OnHalftimeChanged();
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
	else
	{
		CTFGameState->SetTimeLimit(HalftimeDuration);	// Reset the Game Clock for Halftime
	}

	SetMatchState(MatchState::MatchIsAtHalftime);
	BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 11, NULL, NULL, NULL);
}

void AUTCTFGameMode::DefaultTimer()
{
	Super::DefaultTimer();

	//If caster control is enabled. check to see if one caster is ready then start the timer
	if (GetMatchState() == MatchState::MatchIsAtHalftime && bCasterControl)
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

		if (bReady && CTFGameState->bStopGameClock == true)
		{
			CTFGameState->bStopGameClock = false;
			CTFGameState->SetTimeLimit(11);
		}
	}
}

void AUTCTFGameMode::HalftimeIsOver()
{
	SetMatchState(MatchState::MatchExitingHalftime);
}

void AUTCTFGameMode::HandleExitingHalftime()
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

	for (int32 i=0;i<PawnsToDestroy.Num();i++)
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
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator)
		{
			RestartPlayer(Controller);
		}
	}

	// Send all flags home..
	CTFGameState->ResetFlags();
	CTFGameState->bHalftime = false;
	CTFGameState->OnHalftimeChanged();
	if (CTFGameState->bSecondHalf)
	{
		SetMatchState(MatchState::MatchEnteringOvertime);
	}
	else
	{
		CTFGameState->bSecondHalf = true;
		CTFGameState->SetTimeLimit(TimeLimit);		// Reset the GameClock for the second time.
		SetMatchState(MatchState::InProgress);
	}
}

void AUTCTFGameMode::GameObjectiveInitialized(AUTGameObjective* Obj)
{
	AUTCTFFlagBase* FlagBase = Cast<AUTCTFFlagBase>(Obj);
	if (FlagBase != NULL && FlagBase->MyFlag)
	{
		CTFGameState->CacheFlagBase(FlagBase);
	}
}

bool AUTCTFGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// Can't restart in overtime
	if (!CTFGameState->IsMatchInProgress() || CTFGameState->IsMatchAtHalftime() || CTFGameState->IsMatchInSuddenDeath()|| 
			Player == NULL || Player->IsPendingKillPending())
	{
		return false;
	}
	
	// Ask the player controller if it's ready to restart as well
	return Player->CanRestartPlayer();
}

void AUTCTFGameMode::ScoreDamage_Implementation(int32 DamageAmount, AController* Victim, AController* Attacker)
{
	Super::ScoreDamage_Implementation(DamageAmount, Victim, Attacker);
	CTFScoring->ScoreDamage(DamageAmount, Victim, Attacker);
}

void AUTCTFGameMode::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
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

bool AUTCTFGameMode::IsMatchInSuddenDeath()
{
	return CTFGameState->IsMatchInSuddenDeath();
}

void AUTCTFGameMode::CallMatchStateChangeNotify()
{
	Super::CallMatchStateChangeNotify();

	if (MatchState == MatchState::MatchEnteringHalftime)
	{
		HandleEnteringHalftime();
	}
	else if (MatchState == MatchState::MatchIsAtHalftime)
	{
		HandleHalftime();
	}
	else if (MatchState == MatchState::MatchExitingHalftime)
	{
		HandleExitingHalftime();
	}
	else if (MatchState == MatchState::MatchEnteringSuddenDeath)
	{
		HandleEnteringSuddenDeath();
	}
	else if (MatchState == MatchState::MatchIsInSuddenDeath)
	{
		HandleSuddenDeath();
	}
}

void AUTCTFGameMode::HandleEnteringOvertime()
{
	CTFGameState->SetTimeLimit(OvertimeDuration);
	SetMatchState(MatchState::MatchIsInOvertime);
	CTFGameState->bPlayingAdvantage = false;
}

void AUTCTFGameMode::HandleEnteringSuddenDeath()
{
	BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 12, NULL, NULL, NULL);
	CTFGameState->bPlayingAdvantage = false;
}

void AUTCTFGameMode::HandleSuddenDeath()
{
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
		//Starting the Advantage so play the "Loosing advantage" announcement
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
	OutRules += FString::Printf(TEXT("Forced Respawn\t%s\t"), bForceRespawn ?  TEXT("True") : TEXT("False"));

	if (TimeLimit > 0)
	{
		if (HalftimeDuration <= 0)
		{
			OutRules += FString::Printf(TEXT("No Halftime\tTrue\t"));
		}
		else
		{
			OutRules += FString::Printf(TEXT("Halftime\tTrue\t"));
			OutRules += FString::Printf(TEXT("Halftime Duration\t%i\t"), HalftimeDuration);
		}
	}

	OutRules += FString::Printf(TEXT("Allow Overtime\t%s\t"), bAllowOvertime ? TEXT("True") : TEXT("False"));
	if (bAllowOvertime)
	{
		OutRules += FString::Printf(TEXT("Overtime Duration\t%i\t"), OvertimeDuration);;
	}

	if (bSuddenDeath)
	{
		OutRules += FString::Printf(TEXT("Sudden Death\tTrue\t"));
	}

	AUTMutator* Mut = BaseMutator;
	while (Mut)
	{
		OutRules += FString::Printf(TEXT("Mutator\t%s\t"), *Mut->DisplayName.ToString());
		Mut = Mut->NextMutator;
	}
}
void AUTCTFGameMode::EndGame(AUTPlayerState* Winner, FName Reason )
{
	// Dont ever end the game in PIE
	if (GetWorld()->WorldType == EWorldType::PIE) return;

	Super::EndGame(Winner, Reason);

	// Send all of the flags home...
	CTFGameState->ResetFlags();
}


void AUTCTFGameMode::SetEndGameFocus(AUTPlayerState* Winner)
{
	AUTCTFFlagBase* WinningBase = NULL;
	if (Winner != NULL)
	{
		WinningBase = CTFGameState->FlagBases[Winner->GetTeamNum()];

		PlacePlayersAroundFlagBase(Winner->GetTeamNum());
	}

	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
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
				Controller->GameHasEnded(BaseToView , Controller->UTPlayerState->Team == Winner->Team );
			}
		}
	}
}

void AUTCTFGameMode::UpdateSkillRating()
{
	for (int32 PlayerIdx = 0; PlayerIdx < UTGameState->PlayerArray.Num(); PlayerIdx++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
		if (PS && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(FName(TEXT("CTFSkillRating")), PS->Team == UTGameState->WinningTeam, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}

	for (int32 PlayerIdx = 0; PlayerIdx < InactivePlayerArray.Num(); PlayerIdx++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(InactivePlayerArray[PlayerIdx]);
		if (PS && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(FName(TEXT("CTFSkillRating")), PS->Team == UTGameState->WinningTeam, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}
}

#if !UE_SERVER
void AUTCTFGameMode::BuildScoreInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<TAttributeStat> >& StatList)
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
