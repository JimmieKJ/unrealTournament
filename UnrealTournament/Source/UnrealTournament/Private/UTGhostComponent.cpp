// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGhostComponent.h"


UUTGhostComponent::UUTGhostComponent()
{
	bWantsBeginPlay = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UUTGhostComponent::BeginPlay()
{
	Super::BeginPlay();

	UTOwner = Cast<AUTCharacter>(GetOwner());
}

void UUTGhostComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );


	if (bGhostPlaying)
	{
		float CurrentTime = GetWorld()->TimeSeconds - GhostStartTime;

		//TODOTIM: All these gross loops should be unrolled into one based class loop

		while (GhostMoves.IsValidIndex(GhostMoveIndex) && GhostMoves[GhostMoveIndex].Time < CurrentTime)
		{
			//Copy over the RepMovement so SimulatedMove can play it
			UTOwner->UTReplicatedMovement = GhostMoves[GhostMoveIndex].RepMovement;
			UTOwner->OnRep_UTReplicatedMovement();

			//Set the movement flags
			if (UTOwner->UTCharacterMovement != nullptr)
			{
				UTOwner->UTCharacterMovement->UpdateFromCompressedFlags(GhostMoves[GhostMoveIndex].CompressedFlags);
				UTOwner->OnRepFloorSliding();

				UTOwner->bIsCrouched = GhostMoves[GhostMoveIndex].bIsCrouched;
				UTOwner->OnRep_IsCrouched();
				UTOwner->bApplyWallSlide = GhostMoves[GhostMoveIndex].bApplyWallSlide;

			}

			//Rotate the controller so weapon fire is in the right direction
			if (UTOwner->GetController() != nullptr)
			{
				UTOwner->GetController()->SetControlRotation(UTOwner->UTReplicatedMovement.Rotation);
			}

			GhostMoveIndex++;
		}

		while (GhostEvents.IsValidIndex(GhostEventIndex) && GhostEvents[GhostEventIndex].Time < CurrentTime)
		{
			UTOwner->MovementEvent = GhostEvents[GhostEventIndex].MovementEvent;
			UTOwner->MovementEventReplicated();

			GhostEventIndex++;
		}

		while (GhostInputs.IsValidIndex(GhostInputIndex) && GhostInputs[GhostInputIndex].Time < CurrentTime)
		{
			if (UTOwner->Weapon != nullptr)
			{
				for (int32 FireBit = 0; FireBit < sizeof(uint8) * 8; FireBit++)
				{
					uint8 OldBit = (GhostFireFlags >> FireBit) & 1;
					uint8 NewBit = (GhostInputs[GhostInputIndex].FireFlags >> FireBit) & 1;
					if (NewBit != OldBit)
					{
						if (NewBit == 1)
						{
							UTOwner->Weapon->BeginFiringSequence(FireBit, true);
						}
						else
						{
							UTOwner->Weapon->EndFiringSequence(FireBit);
						}
					}
				}
				GhostFireFlags = GhostInputs[GhostInputIndex].FireFlags;
			}
			GhostInputIndex++;
		}

		while (GhostWeapons.IsValidIndex(GhostWeaponIndex) && GhostWeapons[GhostWeaponIndex].Time < CurrentTime)
		{
			TSubclassOf<AUTWeapon> NewWeaponClass = GhostWeapons[GhostWeaponIndex].WeaponClass;
			if (NewWeaponClass != nullptr)
			{
				//AUTInventory* Existing = P->FindInventoryType(NewWeaponClass, true);
				//if (Existing == NULL || !Existing->StackPickup(NULL))
				//{
				FActorSpawnParameters Params;
				Params.bNoCollisionFail = true;
				Params.Instigator = UTOwner;
				AUTWeapon* NewWeapon = GetWorld()->SpawnActor<AUTWeapon>(NewWeaponClass, UTOwner->GetActorLocation(), UTOwner->GetActorRotation(), Params);

				//Give infinite ammo and instant switch speed to avoid any syncing issues
				for (int32 i = 0; i < NewWeapon->AmmoCost.Num(); i++)
				{
					NewWeapon->AmmoCost[i] = 0;
				}
				NewWeapon->BringUpTime = 0.0f;
				NewWeapon->PutDownTime = 0.0f;

				UTOwner->AddInventory(NewWeapon, true);
				UTOwner->SwitchWeapon(NewWeapon);
				//}
			}
			GhostWeaponIndex++;
		}

		if (!GhostMoves.IsValidIndex(GhostMoveIndex) &&
			!GhostEvents.IsValidIndex(GhostEventIndex) &&
			!GhostInputs.IsValidIndex(GhostInputIndex) &&
			!GhostWeapons.IsValidIndex(GhostWeaponIndex))
		{
			GhostStopPlaying();
		}
	}
	else if (bGhostRecording)
	{
		GhostMove();
	}
}



void UUTGhostComponent::GhostStartRecording()
{
	if (UTOwner->Role == ROLE_Authority && !bGhostRecording)
	{
		GhostMoves.Empty();
		GhostEvents.Empty();
		GhostInputs.Empty();
		GhostWeapons.Empty();
		bGhostRecording = true;
		GhostStartTime = GetWorld()->TimeSeconds;
		GhostFireFlags = 0;
		PrimaryComponentTick.SetTickFunctionEnable(true);

		if (UTOwner->Weapon != nullptr)
		{
			GhostWeapons.Add(FGhostWeapon(GetWorld()->TimeSeconds - GhostStartTime, UTOwner->Weapon->GetClass()));
		}
	}
}

void UUTGhostComponent::GhostStopRecording()
{
	if (bGhostRecording && UTOwner->Role == ROLE_Authority)
	{
		bGhostRecording = false;
		PrimaryComponentTick.SetTickFunctionEnable(false);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		//Save the Samples to the editor actor if PIE
		if (GetWorld()->WorldType == EWorldType::PIE && UTOwner->Role == ROLE_Authority)
		{
			auto Worlds = GEngine->GetWorldContexts();
			for (FWorldContext& WorldCtx : Worlds)
			{
				if (WorldCtx.WorldType == EWorldType::Editor && WorldCtx.World() != nullptr)
				{
					int32 NumLevels = WorldCtx.World()->GetNumLevels();
					for (int32 i = 0; i < NumLevels; i++)
					{
						ULevel* Level = WorldCtx.World()->GetLevel(i);
						for (AActor* Actor : Level->Actors)
						{
							AUTCharacter* LevelCharacter = Cast<AUTCharacter>(Actor);
							if (LevelCharacter != nullptr && LevelCharacter->GetName() == UTOwner->GetName())
							{
								//TODOTIM: Look into making ghost data into asset BP's that can be viewed in content browser
								//so they can be assigned to characters that are spawned on the fly
								LevelCharacter->GhostComponent->GhostMoves = GhostMoves;
								LevelCharacter->GhostComponent->GhostEvents = GhostEvents;
								LevelCharacter->GhostComponent->GhostInputs = GhostInputs;
								LevelCharacter->GhostComponent->GhostWeapons = GhostWeapons;
							}
						}
					}
				}
			}
		}
#endif
	}
}

void UUTGhostComponent::GhostStartPlaying()
{
	if (!bGhostRecording && !bGhostPlaying && GhostMoves.Num() > 0)
	{
		bGhostPlaying = true;
		GhostStartTime = GetWorld()->TimeSeconds;
		GhostMoveIndex = 0;
		GhostEventIndex = 0;
		GhostInputIndex = 0;
		GhostWeaponIndex = 0;
		GhostFireFlags = 0;

		//Simulate this char
		OldRole = UTOwner->Role;
		UTOwner->Role = ROLE_SimulatedProxy;

		PrimaryComponentTick.SetTickFunctionEnable(true);
	}
}

void UUTGhostComponent::GhostStopPlaying()
{
	if (bGhostPlaying)
	{
		bGhostPlaying = false;
		UTOwner->Role = OldRole;

		GhostMoveToStart();
		PrimaryComponentTick.SetTickFunctionEnable(false);
	}
}

void UUTGhostComponent::GhostMoveToStart()
{
	if (GhostMoves.Num() > 0)
	{
		UTOwner->SetActorLocation(GhostMoves[0].RepMovement.Location);
	}
}

void UUTGhostComponent::GhostMove()
{
	//TODOTIM: Don't add a move per tick. Only add if necessary
	if (UTOwner->UTCharacterMovement != nullptr)
	{
		FGhostMove NewTick;
		NewTick.Time = GetWorld()->TimeSeconds - GhostStartTime;
		NewTick.bIsCrouched = UTOwner->bIsCrouched;
		NewTick.bApplyWallSlide = UTOwner->bApplyWallSlide;

		UTOwner->GatherUTMovement();
		NewTick.RepMovement = UTOwner->UTReplicatedMovement;

		FNetworkPredictionData_Client_UTChar FakePredict(*UTOwner->UTCharacterMovement);
		FSavedMove_UTCharacter FakeMove;
		FakeMove.SetMoveFor(UTOwner, 0, FVector(0.0f), FakePredict);
		NewTick.CompressedFlags = FakeMove.GetCompressedFlags();

		GhostMoves.Add(NewTick);
	}
}

void UUTGhostComponent::GhostStartFire(uint8 FireModeNum)
{
	if (bGhostRecording)
	{
		GhostFireFlags |= (1 << FireModeNum);
		//UE_LOG(UT, Warning, TEXT("GhostFireFlags = %d"), GhostComponent->GhostFireFlags);
		GhostInputs.Add(FGhostInput(GetWorld()->TimeSeconds - GhostStartTime, GhostFireFlags));
	}
}
void UUTGhostComponent::GhostStopFire(uint8 FireModeNum)
{
	if (bGhostRecording)
	{
		GhostFireFlags &= ~(1 << FireModeNum);
		//UE_LOG(UT, Warning, TEXT("GhostFireFlags = %d"), GhostFireFlags);
		GhostInputs.Add(FGhostInput(GetWorld()->TimeSeconds - GhostStartTime, GhostFireFlags));
	}
}

void UUTGhostComponent::GhostSwitchWeapon(AUTWeapon* NewWeapon)
{
	if (bGhostRecording && NewWeapon != nullptr)
	{
		GhostWeapons.Add(FGhostWeapon(GetWorld()->TimeSeconds - GhostStartTime, NewWeapon->GetClass()));
	}
}

void UUTGhostComponent::GhostMovementEvent(const FMovementEventInfo& MovementEvent)
{
	if (bGhostRecording)
	{
		GhostEvents.Add(FGhostEvent(GetWorld()->TimeSeconds - GhostStartTime, MovementEvent));
	}
}