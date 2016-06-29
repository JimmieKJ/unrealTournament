// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTFlagRunGame.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRoleMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTCTFMajorMessage.h"
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
#include "UTShowdownGameMessage.h"
#include "UTShowdownRewardMessage.h"
#include "UTPlayerStart.h"
#include "UTSkullPickup.h"
#include "UTArmor.h"
#include "UTTimedPowerup.h"
#include "UTPlayerState.h"
#include "UTFlagRunHUD.h"
#include "UTGhostFlag.h"
#include "UTCTFRoundGameState.h"
#include "UTAsymCTFSquadAI.h"
#include "UTWeaponRedirector.h"
#include "UTWeaponLocker.h"

AUTFlagRunGame::AUTFlagRunGame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAsymmetricVictoryConditions = true;
	GoldBonusTime = 120;
	SilverBonusTime = 60;
	GoldScore = 3;
	SilverScore = 2;
	BronzeScore = 1;
	DefenseScore = 1;
	DisplayName = NSLOCTEXT("UTGameMode", "FLAGRUN", "Flag Run");
	bHideInUI = false;
	bWeaponStayActive = false;

	static ConstructorHelpers::FObjectFinder<UClass> AfterImageFinder(TEXT("Blueprint'/Game/RestrictedAssets/Weapons/Translocator/TransAfterImage.TransAfterImage_C'"));
	AfterImageType = AfterImageFinder.Object;
}

void AUTFlagRunGame::BroadcastVictoryConditions()
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC)
		{
			int32 MessageIndex = IsTeamOnOffense(PC->GetTeamNum()) ? 10 * PC->GetTeamNum() + 1 : 3;
			PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), MessageIndex);
		}
	}
}

void AUTFlagRunGame::InitFlags()
{
	Super::InitFlags();
	for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
	{
		if (Base)
		{
			Base->Capsule->SetCapsuleSize(160.f, 134.0f);
		}
	}
}

float AUTFlagRunGame::OverrideRespawnTime(TSubclassOf<AUTInventory> InventoryType)
{
	if (InventoryType == nullptr)
	{
		return 0.f;
	}
	AUTWeapon* WeaponDefault = Cast<AUTWeapon>(InventoryType.GetDefaultObject());
	return (WeaponDefault && !WeaponDefault->bMustBeHolstered) ? 20.f : InventoryType.GetDefaultObject()->RespawnTime;
}

int32 AUTFlagRunGame::GetComSwitch(FName CommandTag, AActor* ContextActor, AUTPlayerController* Instigator, UWorld* World)
{
	if (World == nullptr) return INDEX_NONE;

	AUTCTFGameState* UTCTFGameState = World->GetGameState<AUTCTFGameState>();

	if (Instigator == nullptr || UTCTFGameState == nullptr) 
	{
		return Super::GetComSwitch(CommandTag, ContextActor, Instigator, World);
	}

	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(Instigator->PlayerState);
	AUTCharacter* ContextCharacter = ContextActor != nullptr ? Cast<AUTCharacter>(ContextActor) : nullptr;
	AUTPlayerState* ContextPlayerState = ContextCharacter != nullptr ? Cast<AUTPlayerState>(ContextCharacter->PlayerState) : nullptr;
	
	uint8 OffensiveTeamNum = UTCTFGameState->bRedToCap ? 0 : 1;

	if (ContextCharacter)
	{
		bool bContextOnSameTeam = ContextCharacter != nullptr ? World->GetGameState<AUTGameState>()->OnSameTeam(Instigator, ContextCharacter) : false;
		bool bContextIsFlagCarrier = ContextPlayerState != nullptr && ContextPlayerState->CarriedObject != nullptr;

		if (bContextIsFlagCarrier)
		{
			if ( bContextOnSameTeam )
			{
				if ( CommandTag == CommandTags::Intent )
				{
					return GOT_YOUR_BACK_SWITCH_INDEX;
				}

				else if (CommandTag == CommandTags::Attack)
				{
					return GOING_IN_SWITCH_INDEX;
				}

				else if (CommandTag == CommandTags::Defend)
				{
					return ATTACK_THEIR_BASE_SWITCH_INDEX;
				}
			}
			else
			{
				if (CommandTag == CommandTags::Intent)
				{
					return ENEMY_FC_HERE_SWITCH_INDEX;
				}
				else if (CommandTag == CommandTags::Attack)
				{
					return GET_FLAG_BACK_SWITCH_INDEX;
				}
				else if (CommandTag == CommandTags::Defend)
				{
					return BASE_UNDER_ATTACK_SWITCH_INDEX;
				}
			}
		}
	}

	AUTCharacter* InstCharacter = Cast<AUTCharacter>(Instigator->GetCharacter());
	if (InstCharacter != nullptr && !InstCharacter->IsDead())
	{
		// We aren't dead, look to see if we have the flag...
			
		if (UTPlayerState->CarriedObject != nullptr)
		{
			if (CommandTag == CommandTags::Intent)			
			{
				return GOT_FLAG_SWITCH_INDEX;
			}
			if (CommandTag == CommandTags::Attack)			
			{
				return ATTACK_THEIR_BASE_SWITCH_INDEX;
			}
			if (CommandTag == CommandTags::Defend)			
			{
				return DEFEND_FLAG_CARRIER_SWITCH_INDEX;
			}
		}
	}

	if (CommandTag == CommandTags::Intent)
	{
		// Look to see if I'm on offense or defense...

		if (Instigator->GetTeamNum() == OffensiveTeamNum)
		{
			return ATTACK_THEIR_BASE_SWITCH_INDEX;
		}
		else
		{
			return AREA_SECURE_SWITCH_INDEX;
		}
	}

	if (CommandTag == CommandTags::Attack)
	{
		// Look to see if I'm on offense or defense...

		if (Instigator->GetTeamNum() == OffensiveTeamNum)
		{
			return ATTACK_THEIR_BASE_SWITCH_INDEX;
		}
		else
		{
			return ON_OFFENSE_SWITCH_INDEX;
		}
	}

	if (CommandTag == CommandTags::Defend)
	{
		// Look to see if I'm on offense or defense...

		if (Instigator->GetTeamNum() == OffensiveTeamNum)
		{
			return ON_DEFENSE_SWITCH_INDEX;
		}
		else
		{
			return SPREAD_OUT_SWITCH_INDEX;
		}
	}

	if (CommandTag == CommandTags::Distress)
	{
		return UNDER_HEAVY_ATTACK_SWITCH_INDEX;  
	}

	return Super::GetComSwitch(CommandTag, ContextActor, Instigator, World);
}

void AUTFlagRunGame::HandleRallyRequest(AUTPlayerController* RequestingPC)
{
	AUTCharacter* UTCharacter = RequestingPC->GetUTCharacter();
	AUTPlayerState* UTPlayerState = RequestingPC->UTPlayerState;

	// if can rally, teleport with transloc effect, set last rally time
	AUTCTFGameState* GS = GetWorld()->GetGameState<AUTCTFGameState>();
	AUTTeamInfo* Team = UTPlayerState ? UTPlayerState->Team : nullptr;
	if (Team && UTPlayerState->bCanRally && UTCharacter && GS && IsMatchInProgress() && !GS->IsMatchIntermission() && ((Team->TeamIndex == 0) ? GS->bRedCanRally : GS->bBlueCanRally) && GS->FlagBases.IsValidIndex(Team->TeamIndex) && GS->FlagBases[Team->TeamIndex] != nullptr)
	{
		if (UTCharacter->GetCarriedObject())
		{
			// requesting rally
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
				if (PC && GS->OnSameTeam(RequestingPC, PC) && PC->UTPlayerState && PC->UTPlayerState->bCanRally)
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFMajorMessage::StaticClass(), 22, UTPlayerState);
				}
			}
			UTPlayerState->AnnounceStatus(StatusMessage::NeedBackup);
			UTPlayerState->NextRallyTime = GetWorld()->GetTimeSeconds() + 6.f;
			RallyRequestTime = GetWorld()->GetTimeSeconds();
			return;
		}
		else if ((Team->TeamIndex == 0) == GS->bRedToCap)
		{
			// rally to flag carrier
			AUTCTFFlag* Flag = Cast<AUTCTFFlag>(GS->FlagBases[GS->bRedToCap ? 0 : 1]->GetCarriedObject());
			AUTCharacter* FlagCarrier = Flag ? Flag->HoldingPawn : nullptr;
			if (FlagCarrier != nullptr)
			{
				if (bDelayedRally)
				{
					RequestingPC->BeginRallyTo(FlagCarrier, 1.f);
				}
				else
				{
					CompleteRallyRequest(RequestingPC);
				}
			}
		}
		else
		{
			CompleteRallyRequest(RequestingPC);
		}
	}
}

void AUTFlagRunGame::CompleteRallyRequest(AUTPlayerController* RequestingPC)
{
	AUTCharacter* UTCharacter = RequestingPC->GetUTCharacter();
	AUTPlayerState* UTPlayerState = RequestingPC->UTPlayerState;

	// if can rally, teleport with transloc effect, set last rally time
	AUTCTFGameState* GS = GetWorld()->GetGameState<AUTCTFGameState>();
	AUTTeamInfo* Team = UTPlayerState ? UTPlayerState->Team : nullptr;
	if (!IsMatchInProgress() || (GS && GS->IsMatchIntermission()))
	{
		return;
	}

	if (Team && UTCharacter && GS && GS->FlagBases.IsValidIndex(Team->TeamIndex) && GS->FlagBases[Team->TeamIndex] != nullptr)
	{
		FVector WarpLocation = FVector::ZeroVector;
		FRotator WarpRotation(0.0f, UTCharacter->GetActorRotation().Yaw, 0.0f);
		FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(UTCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), UTCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		FHitResult Hit;
		float SweepRadius = UTCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
		float RallyDelay = 10.f;
		if ((Team->TeamIndex == 0) == GS->bRedToCap)
		{
			// rally to flag carrier
			AUTCTFFlag* Flag = Cast<AUTCTFFlag>(GS->FlagBases[GS->bRedToCap ? 0 : 1]->GetCarriedObject());
			AUTCharacter* FlagCarrier = Flag ? Flag->HoldingPawn : nullptr;
			if (FlagCarrier == nullptr)
			{
				return;
			}
			FVector CarrierLocation = FlagCarrier->GetActorLocation() + FVector(0.f, 0.f, UTCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
			ECollisionChannel SavedObjectType = UTCharacter->GetCapsuleComponent()->GetCollisionObjectType();
			UTCharacter->GetCapsuleComponent()->SetCollisionObjectType(COLLISION_TELEPORTING_OBJECT);
			float Offset = 4.f * UTCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
			for (int32 i = 0; i < 4; i++)
			{
				WarpLocation = CarrierLocation + FVector(Offset * ((i % 2 == 0) ? 1.f : -1.f), Offset * ((i > 1) ? 1.f : -1.f), 0.f);
				if (GetWorld()->FindTeleportSpot(UTCharacter, WarpLocation, WarpRotation) && !GetWorld()->LineTraceTestByChannel(CarrierLocation, WarpLocation, COLLISION_TELEPORTING_OBJECT, FCollisionQueryParams(FName(TEXT("Translocator")), false), WorldResponseParams))
				{
					break;
				}
			}
			UTCharacter->GetCapsuleComponent()->SetCollisionObjectType(SavedObjectType);
			FRotator DesiredRotation = (FlagCarrier->GetActorLocation() - WarpLocation).Rotation();
			WarpRotation.Yaw = DesiredRotation.Yaw;
			RallyDelay = 20.f;
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
				if (PC)
				{
					if (GS->OnSameTeam(RequestingPC, PC))
					{
						if (GetWorld()->GetTimeSeconds() - RallyRequestTime < 6.f)
						{
							PC->ClientReceiveLocalizedMessage(UTPlayerState->GetCharacterVoiceClass(), ACKNOWLEDGE_SWITCH_INDEX, UTPlayerState, PC->PlayerState, NULL);
						}
					}
					else
					{
						PC->ClientReceiveLocalizedMessage(UUTCTFMajorMessage::StaticClass(), 24, UTPlayerState);
					}
				}
			}
		}
		else
		{
			// defenders rally back to flag, once
			WarpLocation = GS->FlagBases[Team->TeamIndex]->GetActorLocation() + FVector(0.f, 0.f, UTCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
			FVector EndTrace = WarpLocation - FVector(0.0f, 0.0f, PlayerCapsule.GetCapsuleHalfHeight());
			bool bHitFloor = GetWorld()->SweepSingleByChannel(Hit, WarpLocation, EndTrace, FQuat::Identity, UTCharacter->GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeSphere(SweepRadius), FCollisionQueryParams(FName(TEXT("Translocation")), false, UTCharacter), UTCharacter->GetCapsuleComponent()->GetCollisionResponseToChannels());
			if (bHitFloor)
			{
				// need to move teleport destination up, unless close to ceiling
				FVector NewLocation = Hit.Location + FVector(0.0f, 0.0f, PlayerCapsule.GetCapsuleHalfHeight());
				bool bHitCeiling = GetWorld()->SweepSingleByChannel(Hit, WarpLocation, NewLocation, FQuat::Identity, UTCharacter->GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeSphere(SweepRadius), FCollisionQueryParams(FName(TEXT("Translocation")), false, UTCharacter), UTCharacter->GetCapsuleComponent()->GetCollisionResponseToChannels());
				if (!bHitCeiling)
				{
					WarpLocation = NewLocation;
				}
			}
			WarpRotation = GS->FlagBases[Team->TeamIndex]->GetActorRotation();
			RallyDelay = 500.f;
			UTPlayerState->AnnounceStatus(StatusMessage::ImOnDefense);
		}

		// teleport
		UPrimitiveComponent* SavedPlayerBase = UTCharacter->GetMovementBase();
		FTransform SavedPlayerTransform = UTCharacter->GetTransform();
		if (UTCharacter->TeleportTo(WarpLocation, WarpRotation))
		{
			UTCharacter->FaceRotation(WarpRotation, 0.0f);
			RequestingPC->SetControlRotation(WarpRotation);
			if (UTCharacter->UTCharacterMovement && UTCharacter->UTCharacterMovement->bIsFloorSliding)
			{
				float VelZ = UTCharacter->UTCharacterMovement->Velocity.Z;
				UTCharacter->UTCharacterMovement->Velocity *= 0.9f;
				UTCharacter->UTCharacterMovement->Velocity.Z = VelZ;
			}
			UTPlayerState->NextRallyTime = GetWorld()->GetTimeSeconds() + RallyDelay;

			// spawn effects
			FActorSpawnParameters SpawnParams;
			SpawnParams.Instigator = UTCharacter;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Owner = UTCharacter;
			if (AfterImageType != NULL)
			{
				AUTWeaponRedirector* AfterImage = GetWorld()->SpawnActor<AUTWeaponRedirector>(AfterImageType, SavedPlayerTransform.GetLocation(), SavedPlayerTransform.GetRotation().Rotator(), SpawnParams);
				if (AfterImage != NULL)
				{
					AfterImage->InitFor(UTCharacter, FRepCollisionShape(PlayerCapsule), SavedPlayerBase, UTCharacter->GetTransform());
				}
			}
		}
	}

}
