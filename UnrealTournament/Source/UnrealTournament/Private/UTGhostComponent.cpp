// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGhostComponent.h"
#include "UTGhostData.h"


UUTGhostComponent::UUTGhostComponent()
{
	bWantsBeginPlay = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	GhostData = nullptr;
	GhostDataClass = nullptr;
}

void UUTGhostComponent::BeginPlay()
{
	Super::BeginPlay();

	UTOwner = Cast<AUTCharacter>(GetOwner());
}

void UUTGhostComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	if (GhostData != nullptr)
	{
		if (bGhostPlaying)
		{
			float CurrentTime = GetWorld()->TimeSeconds - GhostStartTime;

			//TODOTIM: All these gross loops should be unrolled into one based class loop

			while (GhostData->GhostMoves.IsValidIndex(GhostMoveIndex) && GhostData->GhostMoves[GhostMoveIndex].Time < CurrentTime)
			{
				//Copy over the RepMovement so SimulatedMove can play it
				UTOwner->UTReplicatedMovement = GhostData->GhostMoves[GhostMoveIndex].RepMovement;
				UTOwner->OnRep_UTReplicatedMovement();

				//Set the movement flags
				if (UTOwner->UTCharacterMovement != nullptr)
				{
					UTOwner->UTCharacterMovement->UpdateFromCompressedFlags(GhostData->GhostMoves[GhostMoveIndex].CompressedFlags);
					UTOwner->OnRepFloorSliding();

					UTOwner->bIsCrouched = GhostData->GhostMoves[GhostMoveIndex].bIsCrouched;
					UTOwner->OnRep_IsCrouched();
					UTOwner->bApplyWallSlide = GhostData->GhostMoves[GhostMoveIndex].bApplyWallSlide;

				}

				//Rotate the controller so weapon fire is in the right direction
				if (UTOwner->GetController() != nullptr)
				{
					UTOwner->GetController()->SetControlRotation(UTOwner->UTReplicatedMovement.Rotation);
				}

				GhostMoveIndex++;
			}

			while (GhostData->GhostEvents.IsValidIndex(GhostEventIndex) && GhostData->GhostEvents[GhostEventIndex].Time < CurrentTime)
			{
				UTOwner->MovementEvent = GhostData->GhostEvents[GhostEventIndex].MovementEvent;
				UTOwner->MovementEventReplicated();

				GhostEventIndex++;
			}

			while (GhostData->GhostInputs.IsValidIndex(GhostInputIndex) && GhostData->GhostInputs[GhostInputIndex].Time < CurrentTime)
			{
				if (UTOwner->Weapon != nullptr)
				{
					for (int32 FireBit = 0; FireBit < sizeof(uint8) * 8; FireBit++)
					{
						uint8 OldBit = (GhostFireFlags >> FireBit) & 1;
						uint8 NewBit = (GhostData->GhostInputs[GhostInputIndex].FireFlags >> FireBit) & 1;
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
					GhostFireFlags = GhostData->GhostInputs[GhostInputIndex].FireFlags;
				}
				GhostInputIndex++;
			}

			while (GhostData->GhostWeapons.IsValidIndex(GhostWeaponIndex) && GhostData->GhostWeapons[GhostWeaponIndex].Time < CurrentTime)
			{
				TSubclassOf<AUTWeapon> NewWeaponClass = GhostData->GhostWeapons[GhostWeaponIndex].WeaponClass;
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

			if (!GhostData->GhostMoves.IsValidIndex(GhostMoveIndex) &&
				!GhostData->GhostEvents.IsValidIndex(GhostEventIndex) &&
				!GhostData->GhostInputs.IsValidIndex(GhostInputIndex) &&
				!GhostData->GhostWeapons.IsValidIndex(GhostWeaponIndex))
			{
				GhostStopPlaying();
			}
		}
		else if (bGhostRecording)
		{
			GhostMove();
		}
	}
}



void UUTGhostComponent::GhostStartRecording()
{
	if (UTOwner->Role == ROLE_Authority && !bGhostRecording)
	{
		//load the data from a derrived BP class
		if (GhostData == nullptr && GhostDataClass != nullptr)
		{
			GhostData = NewObject<UUTGhostData>(this, GhostDataClass);
		}

		if (GhostData != nullptr)
		{
			GhostData->GhostMoves.Empty();
			GhostData->GhostEvents.Empty();
			GhostData->GhostInputs.Empty();
			GhostData->GhostWeapons.Empty();
			bGhostRecording = true;
			GhostStartTime = GetWorld()->TimeSeconds;
			GhostFireFlags = 0;
			PrimaryComponentTick.SetTickFunctionEnable(true);

			if (UTOwner->Weapon != nullptr)
			{
				GhostData->GhostWeapons.Add(FGhostWeapon(GetWorld()->TimeSeconds - GhostStartTime, UTOwner->Weapon->GetClass()));
			}
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
		//Save directly to the default object. The asset will still need to be saved
		if (GhostDataClass != nullptr)
		{
			UUTGhostData* DefaultGhostData = GhostDataClass.GetDefaultObject();
			DefaultGhostData->GhostMoves = GhostData->GhostMoves;
			DefaultGhostData->GhostEvents = GhostData->GhostEvents;
			DefaultGhostData->GhostInputs = GhostData->GhostInputs;
			DefaultGhostData->GhostWeapons = GhostData->GhostWeapons;
		}
#endif
	}
}

void UUTGhostComponent::GhostStartPlaying()
{
	//load the data from a derrived BP class
	if (GhostData == nullptr && GhostDataClass != nullptr)
	{
		GhostData = NewObject<UUTGhostData>(this, GhostDataClass);
	}

	if (!bGhostRecording && !bGhostPlaying && GhostData != nullptr && GhostData->GhostMoves.Num() > 0)
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

		OnGhostPlayFinished.Broadcast();
		PrimaryComponentTick.SetTickFunctionEnable(false);
	}
}

void UUTGhostComponent::GhostMoveToStart()
{
	if (GhostData != nullptr && GhostData->GhostMoves.Num() > 0)
	{
		UTOwner->SetActorLocation(GhostData->GhostMoves[0].RepMovement.Location);
	}
}

void UUTGhostComponent::GhostMove()
{
	//TODOTIM: Don't add a move per tick. Only add if necessary
	if (UTOwner->UTCharacterMovement != nullptr && GhostData != nullptr)
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

		GhostData->GhostMoves.Add(NewTick);
	}
}

void UUTGhostComponent::GhostStartFire(uint8 FireModeNum)
{
	if (bGhostRecording && GhostData != nullptr)
	{
		GhostFireFlags |= (1 << FireModeNum);
		//UE_LOG(UT, Warning, TEXT("GhostFireFlags = %d"), GhostComponent->GhostFireFlags);
		GhostData->GhostInputs.Add(FGhostInput(GetWorld()->TimeSeconds - GhostStartTime, GhostFireFlags));
	}
}
void UUTGhostComponent::GhostStopFire(uint8 FireModeNum)
{
	if (bGhostRecording && GhostData != nullptr)
	{
		GhostFireFlags &= ~(1 << FireModeNum);
		//UE_LOG(UT, Warning, TEXT("GhostFireFlags = %d"), GhostFireFlags);
		GhostData->GhostInputs.Add(FGhostInput(GetWorld()->TimeSeconds - GhostStartTime, GhostFireFlags));
	}
}

void UUTGhostComponent::GhostSwitchWeapon(AUTWeapon* NewWeapon)
{
	if (bGhostRecording && NewWeapon != nullptr && GhostData != nullptr)
	{
		GhostData->GhostWeapons.Add(FGhostWeapon(GetWorld()->TimeSeconds - GhostStartTime, NewWeapon->GetClass()));
	}
}

void UUTGhostComponent::GhostMovementEvent(const FMovementEventInfo& MovementEvent)
{
	if (bGhostRecording && GhostData != nullptr)
	{
		GhostData->GhostEvents.Add(FGhostEvent(GetWorld()->TimeSeconds - GhostStartTime, MovementEvent));
	}
}