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

UUTLineUpHelper::UUTLineUpHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPlacingPlayers = false;
}

void UUTLineUpHelper::HandleLineUp(UWorld* World, LineUpTypes ZoneType)
{
	if (ZoneType == LineUpTypes::Intro)
	{
		HandleIntro(World, ZoneType);
	}
	else if (ZoneType == LineUpTypes::Intermission)
	{
		HandleIntermission(World, ZoneType);
	}
	else if (ZoneType == LineUpTypes::PostMatch)
	{
		HandleEndMatchSummary(World, ZoneType);
	}
}

void UUTLineUpHelper::HandleIntro(UWorld* World, LineUpTypes IntroType)
{
	bIsActive = true;
	LastActiveType = IntroType;

	MovePlayers(World, IntroType);
}

void UUTLineUpHelper::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UUTLineUpHelper, bIsActive, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UUTLineUpHelper, LastActiveType, COND_OwnerOnly);
}

void UUTLineUpHelper::CleanUp()
{
	if (SelectedCharacter.IsValid())
	{
		SelectedCharacter.Reset();
	}

	bIsActive = false;
	DestroySpawnedClones();
}

void UUTLineUpHelper::SpawnPlayerClones(UWorld* World, LineUpTypes LineUpType)
{
	if (World != nullptr)
	{
		AUTGameState* UTGS = Cast<AUTGameState>(World->GameState);
		if ((UTGS != nullptr) && (UTGS->ShouldUseInGameSummary(LineUpType)))
		{
			for (int PlayerIndex = 0; PlayerIndex < UTGS->PlayerArray.Num(); ++PlayerIndex)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTGS->PlayerArray[PlayerIndex]);
				if (UTPS)
				{
					SpawnClone(World, UTPS, FTransform());
				}
			}

			SortPlayers();
			MovePreviewCharactersToLineUpSpawns(World, LineUpType);
		}
	}
}
			

void UUTLineUpHelper::DestroySpawnedClones()
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

void UUTLineUpHelper::HandleIntermission(UWorld* World, LineUpTypes IntermissionType)
{
	bIsActive = true;
	LastActiveType = IntermissionType;

	MovePlayers(World, IntermissionType);
}

void UUTLineUpHelper::MovePlayers(UWorld* World, LineUpTypes ZoneType)
{
	TArray<TArray<AController*>> PlayersToMove;

	bIsPlacingPlayers = true;

	if (World && World->GetAuthGameMode())
	{
		AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(World->GetAuthGameMode());
		AUTGameMode* UTGM = Cast<AUTGameMode>(World->GetAuthGameMode());

		if (TeamGM && TeamGM->Teams.Num() > 1)
		{
			PlayersToMove.SetNum(TeamGM->Teams.Num());

			for (int TeamIndex = 0; TeamIndex < TeamGM->Teams.Num(); ++TeamIndex)
			{
				const TArray<AController*> TeamMembers = TeamGM->Teams[TeamIndex]->GetTeamMembers();
				for (int PlayerIndex = 0; PlayerIndex < TeamMembers.Num(); ++PlayerIndex)
				{
					PlayersToMove[TeamIndex].Add(TeamMembers[PlayerIndex]);
				}
			}
		}
		else if (UTGM)
		{
			//All players stored on 1 "team"
			PlayersToMove.SetNum(1);

			for (FConstControllerIterator Iterator = World->GetControllerIterator(); Iterator; ++Iterator)
			{
				AController* Controller = Cast<AController>(*Iterator);
				if (Controller && PlayersToMove.Num() > 0)
				{
					PlayersToMove[0].Add(Controller);
				}
			}
		}


		for (int TeamIndex = 0; TeamIndex < PlayersToMove.Num(); ++TeamIndex)
		{
			// respawn dead pawns
			for (int ControllerIndex = 0; ControllerIndex < PlayersToMove[TeamIndex].Num(); ++ControllerIndex)
			{
				AController* C = PlayersToMove[TeamIndex][ControllerIndex];
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
							C->GetPawn()->TurnOff();
							UTChar = Cast<AUTCharacter>(C->GetPawn());
						}
					}

					if (UTChar && !UTChar->IsDead())
					{
						PlayerPreviewCharacters.Add(UTChar);
					}

					//Set camera to intermission cam
					AUTPlayerController* UTPC = Cast<AUTPlayerController>(C);
					if (UTPC)
					{
						UTPC->ClientSetLineUpCamera(World, ZoneType);
					}
				}
			}
		}
		
		SortPlayers();
		MovePreviewCharactersToLineUpSpawns(World, ZoneType);
	}

	bIsPlacingPlayers = false;
}

void UUTLineUpHelper::MovePreviewCharactersToLineUpSpawns(UWorld* World, LineUpTypes LineUpType)
{
 	AUTLineUpZone* SpawnList = GetAppropriateSpawnList(World, LineUpType);
	if (SpawnList && World)
	{
		AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(World->GetAuthGameMode());
		AUTCTFRoundGame* CTFGM = Cast<AUTCTFRoundGame>(World->GetAuthGameMode());

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

LineUpTypes UUTLineUpHelper::GetLineUpTypeToPlay(UWorld* World)
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

AUTLineUpZone* UUTLineUpHelper::GetAppropriateSpawnList(UWorld* World, LineUpTypes ZoneType)
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

static int32 WeaponIndex = 0;

void UUTLineUpHelper::SpawnClone(UWorld* World, AUTPlayerState* PS, const FTransform& Location)
{
	AUTWeaponAttachment* PreviewWeapon = nullptr;
	UAnimationAsset* PlayerPreviewAnim = nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TSubclassOf<class APawn> DefaultPawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GetDefault<AUTGameMode>()->PlayerPawnObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));

	AUTCharacter* PlayerPreviewMesh = World->SpawnActor<AUTCharacter>(DefaultPawnClass, Location.GetTranslation(), Location.Rotator(), SpawnParams);

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
			
				PreviewWeapon = World->SpawnActor<AUTWeaponAttachment>(PreviewAttachmentType, FVector(0, 0, 0), FRotator(0, 0, 0), WeaponSpawnParams);
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

void UUTLineUpHelper::HandleEndMatchSummary(UWorld* World, LineUpTypes SummaryType)
{
	LastActiveType = SummaryType;
	bIsActive = true;

	MovePlayers(World, SummaryType);
}

void UUTLineUpHelper::SortPlayers()
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

void UUTLineUpHelper::OnPlayerChange()
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

void UUTLineUpHelper::ClientUpdatePlayerClones_Implementation()
{
}