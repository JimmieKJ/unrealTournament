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
			GetWorld()->GetTimerManager().ClearTimer(IntermissionHandle);
			GetWorld()->GetTimerManager().ClearTimer(MatchSummaryHandle);

			if (TimerDelayForIntro > SMALL_NUMBER)
			{
				GetWorld()->GetTimerManager().SetTimer(IntroHandle, FTimerDelegate::CreateUObject(this, &AUTLineUpHelper::HandleIntro, LineUpTypes::Intro), TimerDelayForIntro, false);
			}
			else
			{
				HandleIntro(ZoneType);
			}
		}
		else if (ZoneType == LineUpTypes::Intermission)
		{
			GetWorld()->GetTimerManager().ClearTimer(IntroHandle);
			GetWorld()->GetTimerManager().ClearTimer(MatchSummaryHandle);

			if (TimerDelayForIntermission > SMALL_NUMBER)
			{
				GetWorld()->GetTimerManager().SetTimer(IntermissionHandle, FTimerDelegate::CreateUObject(this, &AUTLineUpHelper::HandleIntermission, LineUpTypes::Intermission), TimerDelayForIntermission, false);
			}
			else
			{
				HandleIntermission(ZoneType);
			}
		}
		else if (ZoneType == LineUpTypes::PostMatch)
		{
			GetWorld()->GetTimerManager().ClearTimer(IntroHandle);
			GetWorld()->GetTimerManager().ClearTimer(IntermissionHandle);
			
			if (TimerDelayForEndMatch > SMALL_NUMBER)
			{
				GetWorld()->GetTimerManager().SetTimer(MatchSummaryHandle, FTimerDelegate::CreateUObject(this, &AUTLineUpHelper::HandleEndMatchSummary, LineUpTypes::PostMatch), TimerDelayForEndMatch, false);
			}
			else
			{
				HandleEndMatchSummary(ZoneType);
			}
		}
	}
}

void AUTLineUpHelper::HandleIntro(LineUpTypes IntroType)
{
	bIsActive = true;
	MovePlayers(IntroType);
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

void AUTLineUpHelper::SpawnPlayerClones(LineUpTypes LineUpType)
{
	if (GetWorld())
	{
		AUTGameState* UTGS = Cast<AUTGameState>(GetWorld()->GameState);
		if ((UTGS != nullptr) && (UTGS->ShouldUseInGameSummary(LineUpType)))
		{
			for (int PlayerIndex = 0; PlayerIndex < UTGS->PlayerArray.Num(); ++PlayerIndex)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTGS->PlayerArray[PlayerIndex]);
				if (UTPS)
				{
					SpawnClone(UTPS, FTransform());
				}
			}

			SortPlayers();
			MovePreviewCharactersToLineUpSpawns(LineUpType);
		}
	}
}
			

void AUTLineUpHelper::DestroySpawnedClones()
{
	if (PlayerPreviewCharacters.Num() > 0)
	{
		for (int index = 0; index < PlayerPreviewCharacters.Num(); ++index)
		{
			PlayerPreviewCharacters[index]->Destroy();
		}
		PlayerPreviewCharacters.Empty();
	}

	if (PreviewWeapons.Num() > 0)
	{
		for (int index = 0; index < PreviewWeapons.Num(); ++index)
		{
			PreviewWeapons[index]->Destroy();
		}
		PreviewWeapons.Empty();
	}
}

void AUTLineUpHelper::HandleIntermission(LineUpTypes IntermissionType)
{
	bIsActive = true;
	MovePlayers(IntermissionType);
}

void AUTLineUpHelper::MovePlayers(LineUpTypes ZoneType)
{	
	bIsPlacingPlayers = true;

	if (GetWorld() && GetWorld()->GetAuthGameMode())
	{
		AUTGameMode* UTGM = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
		
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
			
			//Want to still update the animations and bones even though we have turned off the Pawn, so re-enable those.
			if (UTChar->GetMesh())
			{
				UTChar->GetMesh()->bPauseAnims = false;
				//UTChar->GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
			}
		}
	}

	bIsPlacingPlayers = false;
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
			if ((PreviewChar->GetTeamNum() == RedAndWinningTeamNumber) && (RedSpawns.Num() > RedIndex))
			{
				FTransform SpawnTransform = SpawnList->GetActorTransform();
				SpawnTransform = RedSpawns[RedIndex] * SpawnTransform;
				
				PreviewChar->TeleportTo(SpawnTransform.GetTranslation(), SpawnTransform.GetRotation().Rotator(),false, true);
				PreviewChar->Controller->SetControlRotation(SpawnTransform.GetRotation().Rotator());
				++RedIndex;
			}
			else if ((PreviewChar->GetTeamNum() == BlueAndLosingTeamNumber) && (BlueSpawns.Num() > BlueIndex))
			{
				FTransform SpawnTransform = SpawnList->GetActorTransform();
				SpawnTransform = BlueSpawns[BlueIndex] * SpawnTransform;

				PreviewChar->TeleportTo(SpawnTransform.GetTranslation(), SpawnTransform.GetRotation().Rotator(), false, true);
				PreviewChar->Controller->SetControlRotation(SpawnTransform.GetRotation().Rotator());
				++BlueIndex;
			}
			else if (FFASpawns.Num() > FFAIndex)
			{
				FTransform SpawnTransform = SpawnList->GetActorTransform();
				SpawnTransform = FFASpawns[FFAIndex] * SpawnTransform;

				PreviewChar->TeleportTo(SpawnTransform.GetTranslation(), SpawnTransform.GetRotation().Rotator(), false, true);
				PreviewChar->Controller->SetControlRotation(SpawnTransform.GetRotation().Rotator());
				++FFAIndex;
			}
			//If they are not part of the line up display... remove them
			else
			{
				PreviewsMarkedForDestroy.Add(PreviewChar);
			}
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
	if ((UTGS->GetMatchState() == MatchState::PlayerIntro) || ((UTGS->GetMatchState() == MatchState::MatchIntermission) && UTCTFRoundGameGS && (UTCTFRoundGameGS->CTFRound == 1) && (!CTFGM || CTFGM->FlagScorer == nullptr)))
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

void AUTLineUpHelper::SpawnClone(AUTPlayerState* PS, const FTransform& Location)
{
	if (!GetWorld())
	{
		return;
	}

	AUTWeaponAttachment* PreviewWeapon = nullptr;
	UAnimationAsset* PlayerPreviewAnim = nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TSubclassOf<class APawn> DefaultPawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GetDefault<AUTGameMode>()->PlayerPawnObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));

	AUTCharacter* PlayerPreviewMesh = GetWorld()->SpawnActor<AUTCharacter>(DefaultPawnClass, Location.GetTranslation(), Location.Rotator(), SpawnParams);

	if (PlayerPreviewMesh)
	{
		// We need to get our tick functions registered, this seemed like best way to do it
		PlayerPreviewMesh->RegisterAllActorTickFunctions(true, true);

		PlayerPreviewMesh->PlayerState = PS; //PS needed for team colors
		PlayerPreviewMesh->DeactivateSpawnProtection();

		PlayerPreviewMesh->ApplyCharacterData(PS->GetSelectedCharacter());
		PlayerPreviewMesh->NotifyTeamChanged();

		PlayerPreviewMesh->SetHatClass(PS->HatClass);
		PlayerPreviewMesh->SetHatVariant(PS->HatVariant);
		PlayerPreviewMesh->SetEyewearClass(PS->EyewearClass);
		PlayerPreviewMesh->SetEyewearVariant(PS->EyewearVariant);

		int32 WeaponIndexToSpawn = 0;
		if (!PreviewWeapon)
		{
			UClass* PreviewAttachmentType = PS->FavoriteWeapon ? PS->FavoriteWeapon->GetDefaultObject<AUTWeapon>()->AttachmentType : NULL;
			if (!PreviewAttachmentType)
			{
				UClass* PreviewAttachments[6];
				PreviewAttachments[0] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/LinkGun/BP_LinkGun_Attach.BP_LinkGun_Attach_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[1] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/Sniper/BP_Sniper_Attach.BP_Sniper_Attach_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[2] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/RocketLauncher/BP_Rocket_Attachment.BP_Rocket_Attachment_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[3] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/ShockRifle/ShockAttachment.ShockAttachment_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[4] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/Flak/BP_Flak_Attach.BP_Flak_Attach_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[5] = PreviewAttachments[3];
				WeaponIndexToSpawn = WeaponIndex % 6;
				PreviewAttachmentType = PreviewAttachments[WeaponIndexToSpawn];
				WeaponIndex++;
			}
			if (PreviewAttachmentType != NULL)
			{
				FActorSpawnParameters WeaponSpawnParams;
				WeaponSpawnParams.Instigator = PlayerPreviewMesh;
				WeaponSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				WeaponSpawnParams.bNoFail = true;
			
				PreviewWeapon = GetWorld()->SpawnActor<AUTWeaponAttachment>(PreviewAttachmentType, FVector(0, 0, 0), FRotator(0, 0, 0), WeaponSpawnParams);
			}
		}
		if (PreviewWeapon)
		{
			if (PreviewWeapon->HasActorBegunPlay())
			{
				PreviewWeapon->AttachToOwner();
			}

			PreviewWeapons.Add(PreviewWeapon);
		}

		if (PS->IsFemale())
		{
			switch (WeaponIndexToSpawn)
			{
			case 1:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Sniper.MatchPoseFemale_Sniper"));
				break;
			case 2:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Flak_B.MatchPoseFemale_Flak_B"));
				break;
			case 4:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Flak.MatchPoseFemale_Flak"));
				break;
			case 0:
			case 3:
			case 5:
			default:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_ShockRifle.MatchPoseFemale_ShockRifle"));
			}
		}
		else
		{
			switch (WeaponIndexToSpawn)
			{
			case 1:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Sniper.MatchPose_Sniper"));
				break;
			case 2:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Flak_B.MatchPose_Flak_B"));
				break;
			case 4:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Flak.MatchPose_Flak"));
				break;
			case 0:
			case 3:
			case 5:
			default:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_ShockRifle.MatchPose_ShockRifle"));
			}
		}

		PreviewAnimations.AddUnique(PlayerPreviewAnim);

		PlayerPreviewMesh->GetMesh()->PlayAnimation(PlayerPreviewAnim, true);
		PlayerPreviewMesh->GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

		PlayerPreviewCharacters.Add(PlayerPreviewMesh);
	}
}

void AUTLineUpHelper::HandleEndMatchSummary(LineUpTypes SummaryType)
{
	bIsActive = true;
	MovePlayers(SummaryType);
}

void AUTLineUpHelper::SortPlayers()
{
	bool(*SortFunc)(const AUTCharacter&, const AUTCharacter&);
	SortFunc = [](const AUTCharacter& A, const AUTCharacter& B)
	{
		AUTPlayerState* PSA = Cast<AUTPlayerState>(A.PlayerState);
		AUTPlayerState* PSB = Cast<AUTPlayerState>(B.PlayerState);
		return !PSB || (PSA && (PSA->Score > PSB->Score));
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