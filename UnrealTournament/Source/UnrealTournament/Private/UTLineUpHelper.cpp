// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLineUpHelper.h"
#include "UTLineUpZone.h"
#include "Net/UnrealNetwork.h"
#include "UTWeaponAttachment.h"
#include "UTHUD.h"
#include "UTCTFGameState.h"
#include "UTCTFRoundGameState.h"
#include "UTCTFRoundGame.h"

AUTLineUpHelper::AUTLineUpHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPlacingPlayers = false;
	bAlwaysRelevant = true;

	TimerDelayForIntro = 0.f;
	TimerDelayForIntermission = 9.f;
	TimerDelayForEndMatch = 9.f;
}

void AUTLineUpHelper::HandleLineUp(LineUpTypes ZoneType)
{
	LastActiveType = ZoneType;

	if (GetWorld())
	{
		if (ZoneType == LineUpTypes::Intro)
		{
			HandleIntro(ZoneType);
		}
		else if (ZoneType == LineUpTypes::Intermission)
		{
			HandleIntermission(ZoneType);
		}
		else if (ZoneType == LineUpTypes::PostMatch)
		{
			HandleEndMatchSummary(ZoneType);
		}
	}
}

void AUTLineUpHelper::HandleIntro(LineUpTypes IntroType)
{
	bIsActive = true;
	MovePlayersDelayed(IntroType, IntroHandle, TimerDelayForIntro);
}

void AUTLineUpHelper::CleanUp()
{
	if (bIsActive)
	{
		if (SelectedCharacter.IsValid())
		{
			SelectedCharacter.Reset();
		}

		bIsActive = false;
		LastActiveType = LineUpTypes::Invalid;
		
		if (GetWorld())
		{
			for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* UTPC = Cast<AUTPlayerController>(*Iterator);
				if (UTPC)
				{
					UTPC->ClientSetActiveLineUp(false, LineUpTypes::Invalid);
				}
			}
		}

		DestroySpawnedClones();
	}
}
		
void AUTLineUpHelper::DestroySpawnedClones()
{
	if (PlayerPreviewCharacters.Num() > 0)
	{
		for (int index = 0; index < PlayerPreviewCharacters.Num(); ++index)
		{
			if (PlayerPreviewCharacters[index])
			{
				PlayerPreviewCharacters[index]->Destroy();
			}
		}
		PlayerPreviewCharacters.Empty();
	}

	if (PreviewWeapons.Num() > 0)
	{
		for (int index = 0; index < PreviewWeapons.Num(); ++index)
		{
			if (PreviewWeapons[index])
			{
				PreviewWeapons[index]->Destroy();
			}
		}
		PreviewWeapons.Empty();
	}
}

void AUTLineUpHelper::HandleIntermission(LineUpTypes IntermissionType)
{
	MovePlayersDelayed(IntermissionType, IntermissionHandle, TimerDelayForIntermission);
}

void AUTLineUpHelper::MovePlayersDelayed(LineUpTypes ZoneType, FTimerHandle& TimerHandleToStart, float TimeDelay)
{
	GetWorld()->GetTimerManager().ClearTimer(IntroHandle);
	GetWorld()->GetTimerManager().ClearTimer(IntermissionHandle);
	GetWorld()->GetTimerManager().ClearTimer(MatchSummaryHandle);
	
	AUTGameMode* UTGM = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());

	if ((TimeDelay > SMALL_NUMBER))
	{
		SetupDelayedLineUp();
		GetWorld()->GetTimerManager().SetTimer(TimerHandleToStart, FTimerDelegate::CreateUObject(this, &AUTLineUpHelper::MovePlayers, ZoneType), TimeDelay, false);
	}
	else
	{
		MovePlayers(ZoneType);
	}
}

void AUTLineUpHelper::SetupDelayedLineUp()
{
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(*Iterator);
		if (UTPC)
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(UTPC->GetPawn());
			if (UTChar)
			{
				UTChar->TurnOff();
				ForceCharacterAnimResetForLineUp(UTChar);
			}

			UTPC->FlushPressedKeys();	

			UTPC->ClientPrepareForLineUp();
		}
	}
}

void AUTLineUpHelper::MovePlayers(LineUpTypes ZoneType)
{	
	static const FName NAME_LineUpCam = FName(TEXT("LineUpCam"));
	bIsPlacingPlayers = true;
	bIsActive = true;

	if (GetWorld() && GetWorld()->GetAuthGameMode())
	{
		AUTGameMode* UTGM = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
		
		//Go through controllers and save current equipped wepaon as the FavoriteWeapon. Doing this before removing all pawns.
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* UTPC = Cast<AUTPlayerController>(*Iterator);
			if (UTPC)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPC->PlayerState);
				AUTCharacter* UTChar = Cast<AUTCharacter>(UTPC->GetPawn());
				if (UTPS && UTChar)
				{
					UTPS->FavoriteWeapon = UTChar->GetWeaponClass();
				}
			}
		}

		if (UTGM)
		{
			UTGM->RemoveAllPawns();
		}

		//Go through all controllers and spawn/respawn pawns
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AController* C = Cast<AController>(*Iterator);
			if (C)
			{
				AUTCharacter* UTChar = Cast<AUTCharacter>(C->GetPawn());
				if (!UTChar || UTChar->IsDead() || UTChar->IsRagdoll())
				{
					if (C->GetPawn())
					{
						C->UnPossess();
					}
					UTGM->RestartPlayer(C);
					if (C->GetPawn())
					{
						UTChar = Cast<AUTCharacter>(C->GetPawn());
					}
				}
					
				if (UTChar && !UTChar->IsDead())
				{
					PlayerPreviewCharacters.Add(UTChar);
				}
				
				AUTPlayerController* UTPC = Cast<AUTPlayerController>(C);
				if (UTPC)
				{
					//UTPC->SetCameraMode(NAME_LineUpCam);
					UTPC->ClientSetActiveLineUp(true, ZoneType);
				}
			}
		}

		SortPlayers();
		MovePreviewCharactersToLineUpSpawns(ZoneType);

		//Go back through characters now that they are moved and turn them off
		for (AUTCharacter* UTChar : PlayerPreviewCharacters)
		{
			UTChar->TurnOff();
			ForceCharacterAnimResetForLineUp(UTChar);
			SpawnPlayerWeapon(UTChar);
		}
	}

	bIsPlacingPlayers = false;
}

void AUTLineUpHelper::SpawnPlayerWeapon(AUTCharacter* UTChar)
{
	//If we already have a weapon attachment, keep that
	if (UTChar)
	{
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTChar->PlayerState);
		TSubclassOf<AUTWeapon> WeaponClass = NULL;

		if (UTPS)
		{
			WeaponClass = UTPS->FavoriteWeapon ? UTPS->FavoriteWeapon->GetDefaultObject<AUTWeapon>()->GetClass() : NULL;
		}

		//Default to Link Gun
		if (!WeaponClass)
		{
			WeaponClass = LoadClass<AUTWeapon>(NULL, TEXT("/Game/RestrictedAssets/Weapons/LinkGun/BP_LinkGun.BP_LinkGun_C"), NULL, LOAD_None, NULL);
		}
		
		//Remove all inventory so that when we add this weapon in, it is equipped.
		UTChar->DiscardAllInventory();
		
		FActorSpawnParameters WeaponSpawnParams;
		WeaponSpawnParams.Instigator = UTChar;
		WeaponSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		WeaponSpawnParams.bNoFail = true;

		AUTWeapon* PreviewWeapon = GetWorld()->SpawnActor<AUTWeapon>(WeaponClass, FVector(0, 0, 0), FRotator(0, 0, 0), WeaponSpawnParams);
		if (PreviewWeapon)
		{
			PreviewWeapons.Add(PreviewWeapon);
			
			PreviewWeapon->bAlwaysRelevant = true;
			PreviewWeapon->SetReplicates(true);
			UTChar->AddInventory(PreviewWeapon, true);
			
			//Bots will not auto-switch to new weapon
			AUTBot* BotController = Cast<AUTBot>(UTChar->Controller);
			if (BotController)
			{
				BotController->SwitchToBestWeapon();
			}
		}
	}
}


void AUTLineUpHelper::ForceCharacterAnimResetForLineUp(AUTCharacter* UTChar)
{
	if (UTChar && UTChar->GetMesh())
	{
		//Want to still update the animations and bones even though we have turned off the Pawn, so re-enable those.
		UTChar->GetMesh()->bPauseAnims = false;
		UTChar->GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	}
}

void AUTLineUpHelper::MovePreviewCharactersToLineUpSpawns(LineUpTypes LineUpType)
{
 	AUTLineUpZone* SpawnList = GetAppropriateSpawnList(GetWorld(), LineUpType);
	if (SpawnList)
	{
		AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(GetWorld()->GetAuthGameMode());
		AUTCTFRoundGame* CTFGM = Cast<AUTCTFRoundGame>(GetWorld()->GetAuthGameMode());

		AUTCTFGameState* CTFGS = Cast<AUTCTFGameState>(GetWorld()->GetGameState());

		const TArray<FTransform>& RedSpawns = SpawnList->RedAndWinningTeamSpawnLocations;
		const TArray<FTransform>& BlueSpawns = SpawnList->BlueAndLosingTeamSpawnLocations;
		const TArray<FTransform>& FFASpawns = SpawnList->FFATeamSpawnLocations;

		int RedIndex = 0;
		int BlueIndex = 0;
		int FFAIndex = 0;
		
		int RedAndWinningTeamNumber = 0;
		int BlueAndLosingTeamNumber = 1;

		//Spawn using Winning / Losing teams instead of team color based teams. This means the red list = winning team and blue list = losing team.
		if (TeamGM && TeamGM->UTGameState && (SpawnList->RedAndWinningTeamSpawnLocations.Num() > 0) && (LineUpType == LineUpTypes::PostMatch || LineUpType == LineUpTypes::Intermission))
		{
			if (TeamGM->UTGameState->WinningTeam)
			{		
				RedAndWinningTeamNumber = TeamGM->UTGameState->WinningTeam->GetTeamNum();
				BlueAndLosingTeamNumber = 1 - RedAndWinningTeamNumber;
			}
			else if (TeamGM->UTGameState->ScoringPlayerState)
			{
				RedAndWinningTeamNumber = TeamGM->UTGameState->ScoringPlayerState->GetTeamNum();
				BlueAndLosingTeamNumber = 1 - RedAndWinningTeamNumber;
			}
			else if (CTFGM && CTFGM->FlagScorer)
			{
				RedAndWinningTeamNumber = CTFGM->FlagScorer->GetTeamNum();
				BlueAndLosingTeamNumber = 1 - RedAndWinningTeamNumber;
			}
			else if (CTFGS && CTFGS->GetScoringPlays().Num() > 0)
			{
				const TArray<const FCTFScoringPlay>& ScoringPlays = CTFGS->GetScoringPlays();
				const FCTFScoringPlay& WinningPlay = ScoringPlays.Last();

				if (WinningPlay.Team)
				{
					RedAndWinningTeamNumber = WinningPlay.Team->GetTeamNum();
					BlueAndLosingTeamNumber = 1 - RedAndWinningTeamNumber;
				}
			}
		}

		TArray<AUTCharacter*> PreviewsMarkedForDestroy;
		for (AUTCharacter* PreviewChar : PlayerPreviewCharacters)
		{
			FTransform SpawnTransform = SpawnList->GetActorTransform();

			if ((PreviewChar->GetTeamNum() == RedAndWinningTeamNumber) && (RedSpawns.Num() > RedIndex))
			{
				SpawnTransform = RedSpawns[RedIndex] * SpawnTransform;
				++RedIndex;
			}
			else if ((PreviewChar->GetTeamNum() == BlueAndLosingTeamNumber) && (BlueSpawns.Num() > BlueIndex))
			{
				SpawnTransform = BlueSpawns[BlueIndex] * SpawnTransform;
				++BlueIndex;
			}
			else if (FFASpawns.Num() > FFAIndex)
			{
				SpawnTransform = FFASpawns[FFAIndex] * SpawnTransform;
				++FFAIndex;
			}
			//If they are not part of the line up display... remove them
			else
			{
				PreviewsMarkedForDestroy.Add(PreviewChar);
			}

			PreviewChar->bIsTranslocating = true; // Hack to get rid of teleport effect

			PreviewChar->TeleportTo(SpawnTransform.GetTranslation(), SpawnTransform.GetRotation().Rotator(), false, true);
			PreviewChar->Controller->SetControlRotation(SpawnTransform.GetRotation().Rotator());
			PreviewChar->ClientSetRotation(SpawnTransform.GetRotation().Rotator());

			PreviewChar->bIsTranslocating = false;
		}

		for (AUTCharacter* DestroyCharacter : PreviewsMarkedForDestroy)
		{
			AUTPlayerController* UTPC = Cast<AUTPlayerController>(DestroyCharacter->GetController());
			if (UTPC)
			{
				UTPC->UnPossess();
			}
			
			PlayerPreviewCharacters.Remove(DestroyCharacter);
			DestroyCharacter->Destroy();
		}
		PreviewsMarkedForDestroy.Empty();
	}
}

LineUpTypes AUTLineUpHelper::GetLineUpTypeToPlay(UWorld* World)
{
	LineUpTypes ReturnZoneType = LineUpTypes::Invalid;

	AUTGameState* UTGS = Cast<AUTGameState>(World->GetGameState());
	if (UTGS == nullptr)
	{
		return ReturnZoneType;
	}

	AUTCTFGameState* UTCTFGS = Cast<AUTCTFGameState>(UTGS);
	AUTCTFRoundGameState* UTCTFRoundGameGS = Cast<AUTCTFRoundGameState>(UTGS);

	AUTCTFRoundGame* CTFGM = Cast<AUTCTFRoundGame>(World->GetAuthGameMode());

	//The first intermission of CTF Round Game is actually an intro
	if ((UTGS->GetMatchState() == MatchState::PlayerIntro) || ((UTGS->GetMatchState() == MatchState::MatchIntermission) && UTCTFRoundGameGS && (UTCTFRoundGameGS->CTFRound == 1) && (!UTCTFRoundGameGS || UTCTFRoundGameGS->GetScoringPlays().Num() == 0)))
	{
		if (UTGS->ShouldUseInGameSummary(LineUpTypes::Intro))
		{
			ReturnZoneType = LineUpTypes::Intro;
		}
	}

	else if (UTGS->GetMatchState() == MatchState::MatchIntermission)
	{
		if (UTGS->ShouldUseInGameSummary(LineUpTypes::Intermission))
		{
			ReturnZoneType = LineUpTypes::Intermission;
		}
	}

	else if (UTGS->GetMatchState() == MatchState::WaitingPostMatch)
	{
		if (UTGS->ShouldUseInGameSummary(LineUpTypes::PostMatch))
		{
			ReturnZoneType = LineUpTypes::PostMatch;
		}
	}

	return ReturnZoneType;
}

AUTLineUpZone* AUTLineUpHelper::GetAppropriateSpawnList(UWorld* World, LineUpTypes ZoneType)
{
	AUTLineUpZone* FoundPotentialMatch = nullptr;

	if (World && ZoneType != LineUpTypes::Invalid)
	{
		AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(World->GetAuthGameMode());

		for (TActorIterator<AUTLineUpZone> It(World); It; ++It)
		{
			if (It->ZoneType == ZoneType)
			{
				//Found a perfect match, so return it right away. Perfect match because this is a team spawn point in a team game
				if (It->bIsTeamSpawnList && TeamGM &&  TeamGM->Teams.Num() > 2)
				{
					return *It;
				}
				//Found a perfect match, so return it right away. Perfect match because this is not a team spawn point in a non-team game
				else if (!It->bIsTeamSpawnList && (TeamGM == nullptr))
				{
					return *It;
				}

				//Imperfect match, try and find a perfect match before returning it
				FoundPotentialMatch = *It;
			}
		}
	}

	return FoundPotentialMatch;
}

AActor* AUTLineUpHelper::GetCameraActorForLineUp(UWorld* World, LineUpTypes ZoneType)
{
	AActor* FoundCamera = nullptr;

	AUTLineUpZone* SpawnPointList = AUTLineUpHelper::GetAppropriateSpawnList(World, ZoneType);
	if (SpawnPointList)
	{
		FoundCamera = SpawnPointList->Camera;
	}

	return FoundCamera;
}

static int32 WeaponIndex = 0;

void AUTLineUpHelper::HandleEndMatchSummary(LineUpTypes SummaryType)
{
	MovePlayersDelayed(SummaryType, MatchSummaryHandle, TimerDelayForEndMatch);
}

void AUTLineUpHelper::SortPlayers()
{
	bool(*SortFunc)(const AUTCharacter&, const AUTCharacter&);
	SortFunc = [](const AUTCharacter& A, const AUTCharacter& B)
	{
		AUTPlayerState* PSA = Cast<AUTPlayerState>(A.PlayerState);
		AUTPlayerState* PSB = Cast<AUTPlayerState>(B.PlayerState);

		AUTCTFFlag* AUTFlagA = nullptr;
		AUTCTFFlag* AUTFlagB = nullptr;
		if (PSA)
		{
			AUTFlagA = Cast<AUTCTFFlag>(PSA->CarriedObject);
		}
		if (PSB)
		{
			AUTFlagB = Cast<AUTCTFFlag>(PSB->CarriedObject);
		}

		return !PSB || (AUTFlagA) || (PSA && (PSA->Score > PSB->Score) && !AUTFlagB);
	};
	PlayerPreviewCharacters.Sort(SortFunc);
}

void AUTLineUpHelper::OnPlayerChange()
{
	if (GetWorld())
	{
		AUTGameMode* UTGM = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
		if (UTGM && UTGM->UTGameState)
		{
			if (UTGM->UTGameState->GetMatchState() == MatchState::WaitingToStart)
			{
				ClientUpdatePlayerClones();
			}
		}
	}
}

void AUTLineUpHelper::ClientUpdatePlayerClones()
{

}