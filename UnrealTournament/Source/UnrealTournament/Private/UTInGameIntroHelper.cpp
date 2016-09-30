// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTInGameIntroHelper.h"
#include "UTInGameIntroZone.h"
#include "Net/UnrealNetwork.h"
#include "UTWeaponAttachment.h"
#include "UTHUD.h"
#include "UTCTFGameState.h"
#include "UTCTFRoundGameState.h"

UUTInGameIntroHelper::UUTInGameIntroHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUTInGameIntroHelper::HandleIntro(UWorld* World, InGameIntroZoneTypes IntroType)
{
	SpawnPlayerClones(World, IntroType);

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(World->GetFirstPlayerController());
	if (UTPC)
	{
		UTPC->ClientSetIntroCamera(World, IntroType);
	}

	bIsActive = true;
	LastActiveType = IntroType;
}

void UUTInGameIntroHelper::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UUTInGameIntroHelper, bIsActive, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UUTInGameIntroHelper, LastActiveType, COND_OwnerOnly);
}

void UUTInGameIntroHelper::CleanUp()
{
	if (SelectedCharacter.IsValid())
	{
		SelectedCharacter.Reset();
	}

	bIsActive = false;
	DestroySpawnedClones();
}

void UUTInGameIntroHelper::SpawnPlayerClones(UWorld* World, InGameIntroZoneTypes IntroType)
{
	if (World != nullptr)
	{
		AUTGameState* UTGS = Cast<AUTGameState>(World->GameState);
		if ((UTGS != nullptr) && (UTGS->ShouldUseInGameSummary(IntroType)))
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

			for (TActorIterator<AUTInGameIntroZone> It(World); It; ++It)
			{
				if (It->ZoneType == IntroType)
				{
					const TArray<FTransform>& RedSpawns = It->RedTeamSpawnLocations;
					const TArray<FTransform>& BlueSpawns = It->BlueTeamSpawnLocations;
					const TArray<FTransform>& FFASpawns = It->FFATeamSpawnLocations;

					int RedIndex = 0;
					int BlueIndex = 0;
					int FFAIndex = 0;

					for (AUTCharacter* PreviewChar : PlayerPreviewCharacters)
					{
						if ((PreviewChar->GetTeamNum() == 0) && (RedSpawns.Num() > RedIndex))
						{
							PreviewChar->TeleportTo(RedSpawns[RedIndex].GetLocation(), RedSpawns[RedIndex].Rotator());
							++RedIndex;
						}
						else if ((PreviewChar->GetTeamNum() == 1) && (BlueSpawns.Num() > RedIndex))
						{
							PreviewChar->TeleportTo(BlueSpawns[BlueIndex].GetLocation(), BlueSpawns[BlueIndex].Rotator());
							++BlueIndex;
						}
						else if (FFASpawns.Num() > FFAIndex)
						{
							PreviewChar->TeleportTo(FFASpawns[FFAIndex].GetLocation(), FFASpawns[FFAIndex].Rotator());
							++FFAIndex;
						}
					}

					return;
				}
			}
		}
	}
}
			

void UUTInGameIntroHelper::DestroySpawnedClones()
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

void UUTInGameIntroHelper::HandleIntermission(UWorld* World, InGameIntroZoneTypes IntermissionType)
{
	bIsActive = true;
	LastActiveType = IntermissionType;

	MovePlayers(World, IntermissionType);
}

void UUTInGameIntroHelper::MovePlayers(UWorld* World, InGameIntroZoneTypes ZoneType)
{
	TArray<TArray<AController*>> PlayersToMove;

	if (World && World->GetAuthGameMode())
	{
		AUTTeamGameMode* TeamGM = Cast<AUTTeamGameMode>(World->GetAuthGameMode());
		AUTGameMode* UTGM = Cast<AUTGameMode>(World->GetAuthGameMode());

		if (TeamGM)
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
						UTPC->ClientSetIntroCamera(World, ZoneType);
					}
				}
			}
		}
		
		SortPlayers();

		AUTInGameIntroZone* SpawnListToUse = GetAppropriateSpawnList(World, ZoneType);
		if (SpawnListToUse)
		{
			int RedIndex = 0;
			int BlueIndex = 0;
			int FFAIndex = 0;

			TArray<FTransform>& RedSpawns = SpawnListToUse->RedTeamSpawnLocations;
			TArray<FTransform>& BlueSpawns = SpawnListToUse->BlueTeamSpawnLocations;
			TArray<FTransform>& FFASpawns = SpawnListToUse->FFATeamSpawnLocations;

			for (AUTCharacter* UTChar : PlayerPreviewCharacters)
			{
				if ((UTChar->GetTeamNum() == 0) && (RedSpawns.Num() > RedIndex))
				{
					UTChar->TeleportTo(RedSpawns[RedIndex].GetLocation(), RedSpawns[RedIndex].Rotator());
					++RedIndex;
				}
				else if ((UTChar->GetTeamNum() == 1) && (BlueSpawns.Num() > BlueIndex))
				{
					UTChar->TeleportTo(BlueSpawns[BlueIndex].GetLocation(), BlueSpawns[BlueIndex].Rotator());
					++BlueIndex;
				}
				else if (FFASpawns.Num() > FFAIndex)
				{
					UTChar->TeleportTo(FFASpawns[FFAIndex].GetLocation(), FFASpawns[FFAIndex].Rotator());
					++FFAIndex;
				}
			}
		}
	}
}

InGameIntroZoneTypes UUTInGameIntroHelper::GetIntroTypeToPlay(UWorld* World)
{
	InGameIntroZoneTypes ReturnZoneType = InGameIntroZoneTypes::Invalid;

	AUTGameState* UTGS = Cast<AUTGameState>(World->GetGameState());
	if (UTGS == nullptr)
	{
		return ReturnZoneType;
	}

	AUTCTFGameState* UTCTFGS = Cast<AUTCTFGameState>(UTGS);
	AUTCTFRoundGameState* UTCTFRoundGameGS = Cast<AUTCTFRoundGameState>(UTGS);

	if (UTGS->GetMatchState() == MatchState::PlayerIntro)
	{
		if (UTGS->ShouldUseInGameSummary(InGameIntroZoneTypes::Intro))
		{
			ReturnZoneType = InGameIntroZoneTypes::Intro;
		}
	}

	else if (UTGS->GetMatchState() == MatchState::MatchIntermission)
	{
		if (UTGS->Teams.Num() > 0)
		{
			int TeamToWatch = 255;
			if (UTCTFRoundGameGS)
			{
				TeamToWatch = (UTCTFRoundGameGS->Teams[0] > UTCTFRoundGameGS->Teams[1]) ? 0 : 1;
			}
			else if (UTCTFGS && UTCTFGS->WinnerPlayerState)
			{
				TeamToWatch = UTCTFGS->WinnerPlayerState->GetTeamNum();
			}
			
			//Fallback on basic intermission instead of team-specific in case team-specific fails
			InGameIntroZoneTypes IntermissionType = InGameIntroZoneTypes::Intermission;

			if (TeamToWatch == 0)
			{
				IntermissionType = InGameIntroZoneTypes::Intermission_RedWin;
			}
			else if (TeamToWatch == 1)
			{
				IntermissionType = InGameIntroZoneTypes::Intermission_BlueWin;
			}

			//Check that we can actually use the team intermission
			if (UTGS->ShouldUseInGameSummary(IntermissionType))
			{
				ReturnZoneType = IntermissionType;
			}
		}
		else
		{
			if (UTGS->ShouldUseInGameSummary(InGameIntroZoneTypes::Intermission))
			{
				ReturnZoneType = InGameIntroZoneTypes::Intermission;
			}
		}
	}

	else if (UTGS->GetMatchState() == MatchState::WaitingPostMatch)
	{
		if (UTGS->Teams.Num() > 0)
		{
			int TeamToWatch = 255;
			if (UTCTFRoundGameGS)
			{
				TeamToWatch = (UTCTFRoundGameGS->Teams[0] > UTCTFRoundGameGS->Teams[1]) ? 0 : 1;
			}
			else if (UTCTFGS && UTCTFGS->WinnerPlayerState)
			{
				TeamToWatch = UTCTFGS->WinnerPlayerState->GetTeamNum();
			}

			//Fallback on basic intermission instead of team-specific in case team-specific fails
			InGameIntroZoneTypes IntermissionType = InGameIntroZoneTypes::PostMatch;
			
			if (TeamToWatch == 0)
			{
				IntermissionType = InGameIntroZoneTypes::PostMatch_RedWin;
			}
			else if (TeamToWatch == 1)
			{
				IntermissionType = InGameIntroZoneTypes::PostMatch_BlueWin;
			}

			//Check that we can actually use the team intermission
			if (UTGS->ShouldUseInGameSummary(IntermissionType))
			{
				ReturnZoneType = IntermissionType;
			}
		}
		else
		{
			if (UTGS->ShouldUseInGameSummary(InGameIntroZoneTypes::PostMatch))
			{
				ReturnZoneType = InGameIntroZoneTypes::PostMatch;
			}
		}
	}

	return ReturnZoneType;
}

AUTInGameIntroZone* UUTInGameIntroHelper::GetAppropriateSpawnList(UWorld* World, InGameIntroZoneTypes IntermissionType)
{
	for (TActorIterator<AUTInGameIntroZone> It(World); It; ++It)
	{
		if (It->ZoneType == IntermissionType)
		{
			return *It;
		}
	}

	return nullptr;
}

static int32 WeaponIndex = 0;

void UUTInGameIntroHelper::SpawnClone(UWorld* World, AUTPlayerState* PS, const FTransform& Location)
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
			PreviewWeapon->AttachToOwner();
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

void UUTInGameIntroHelper::HandleEndMatchSummary(UWorld* World, InGameIntroZoneTypes SummaryType)
{
	LastActiveType = SummaryType;
	bIsActive = true;

	MovePlayers(World, SummaryType);
}

void UUTInGameIntroHelper::SortPlayers()
{
	bool(*SortFunc)(const AUTCharacter&, const AUTCharacter&);
	SortFunc = [](const AUTCharacter& A, const AUTCharacter& B)
	{
		AUTPlayerState* PSA = Cast<AUTPlayerState>(A.PlayerState);
		AUTPlayerState* PSB = Cast<AUTPlayerState>(B.PlayerState);
		return !PSB || (PSA && (PSA->MatchHighlightScore > PSB->MatchHighlightScore));
	};
	PlayerPreviewCharacters.Sort(SortFunc);
}