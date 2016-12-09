// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGhostComponent.h"
#include "UTGhostData.h"
#include "UTGhostEvent.h"
#include "UTGhostController.h"


UUTGhostComponent::UUTGhostComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	GhostData = nullptr;
	MaxMoveDelta = 0.064f;
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

			while (GhostData->Events.IsValidIndex(GhostEventIndex) && GhostData->Events[GhostEventIndex]->Time < CurrentTime)
			{
				GhostData->Events[GhostEventIndex]->ApplyEvent(UTOwner);
				GhostEventIndex++;
			}

			if (!GhostData->Events.IsValidIndex(GhostEventIndex))
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
		if (GhostData == nullptr)
		{
			GhostData = NewObject<UUTGhostData>(this);
		}

		if (GhostData != nullptr)
		{
			GhostData->Events.Empty();
			GhostData->StartTransform = UTOwner->GetTransform();
			bGhostRecording = true;
			GhostStartTime = GetWorld()->TimeSeconds;
			GhostFireFlags = 0;
			OldCompressedFlags = 0;
			NextMoveTime = 0.0f;
			PrimaryComponentTick.SetTickFunctionEnable(true);

			//add the current weapon if there is one being held
			if (UTOwner->Weapon != nullptr)
			{
				UUTGhostEvent_Weapon* WeaponEvent = Cast<UUTGhostEvent_Weapon>(CreateAndAddEvent(UUTGhostEvent_Weapon::StaticClass()));
				if (WeaponEvent != nullptr)
				{
					WeaponEvent->WeaponClass = UTOwner->Weapon->GetClass();
				}
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
	}
}

void UUTGhostComponent::GhostStartPlaying()
{
	if (!bGhostRecording && !bGhostPlaying && GhostData != nullptr && GhostData->Events.Num() > 0)
	{
		bGhostPlaying = true;
		GhostStartTime = GetWorld()->TimeSeconds;
		GhostEventIndex = 0;
		GhostFireFlags = 0;
		OldCompressedFlags = 0;
		NextMoveTime = 0.0f;
		
		//Spawn the GhostController
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Owner = nullptr;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save AI controllers into a map
		AController* NewController = GetWorld()->SpawnActor<AController>(AUTGhostController::StaticClass(), UTOwner->GetActorLocation(), UTOwner->GetActorRotation(), SpawnInfo);
		if (NewController != NULL)
		{
			NewController->Possess(UTOwner);
		}

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
	if (GhostData != nullptr)
	{
		UTOwner->SetActorTransform(GhostData->StartTransform);
	}
}


void UUTGhostComponent::GhostMove()
{
	if (UTOwner->UTCharacterMovement != nullptr && UTOwner->Controller != nullptr && GhostData != nullptr)
	{
		FNetworkPredictionData_Client_UTChar FakePredict(*UTOwner->UTCharacterMovement);
		FSavedMove_UTCharacter FakeMove;
		FakeMove.SetMoveFor(UTOwner, 0, FVector(0.0f), FakePredict);

		//If the movement compressed flags change. save the movment regardless of MaxMoveDelta
		if (NextMoveTime <= GetWorld()->TimeSeconds || FakeMove.GetCompressedFlags() != OldCompressedFlags)
		{
			UUTGhostEvent_Move* MoveEvent = Cast<UUTGhostEvent_Move>(CreateAndAddEvent(UUTGhostEvent_Move::StaticClass()));
			if (MoveEvent != nullptr)
			{
				MoveEvent->bIsCrouched = UTOwner->bIsCrouched;
				MoveEvent->bApplyWallSlide = UTOwner->bApplyWallSlide;

				UTOwner->GatherUTMovement();
				MoveEvent->RepMovement = UTOwner->UTReplicatedMovement;
				MoveEvent->RepMovement.Rotation.Pitch = UTOwner->Controller->GetControlRotation().Pitch;

				MoveEvent->CompressedFlags = FakeMove.GetCompressedFlags();
				OldCompressedFlags = FakeMove.GetCompressedFlags();
			}
			NextMoveTime = GetWorld()->TimeSeconds + MaxMoveDelta;
		}
	}
}

void UUTGhostComponent::GhostStartFire(uint8 FireModeNum)
{
	if (bGhostRecording && GhostData != nullptr)
	{
		UUTGhostEvent_Input* InputEvent = Cast<UUTGhostEvent_Input>(CreateAndAddEvent(UUTGhostEvent_Input::StaticClass()));
		if (InputEvent != nullptr)
		{
			GhostFireFlags |= (1 << FireModeNum);
			InputEvent->FireFlags = GhostFireFlags;
		}
	}
}
void UUTGhostComponent::GhostStopFire(uint8 FireModeNum)
{
	if (bGhostRecording && GhostData != nullptr)
	{
		UUTGhostEvent_Input* InputEvent = Cast<UUTGhostEvent_Input>(CreateAndAddEvent(UUTGhostEvent_Input::StaticClass()));
		if (InputEvent != nullptr)
		{
			GhostFireFlags &= ~(1 << FireModeNum);
			InputEvent->FireFlags = GhostFireFlags;
			CreateAndAddEvent(UUTGhostEvent::StaticClass());
		}
	}
}

void UUTGhostComponent::GhostSwitchWeapon(AUTWeapon* NewWeapon)
{
	if (bGhostRecording && NewWeapon != nullptr && GhostData != nullptr)
	{
		UUTGhostEvent_Weapon* WeaponEvent = Cast<UUTGhostEvent_Weapon>(CreateAndAddEvent(UUTGhostEvent_Weapon::StaticClass()));
		if (WeaponEvent != nullptr)
		{
			WeaponEvent->WeaponClass = NewWeapon->GetClass();
		}
	}
}

void UUTGhostComponent::GhostMovementEvent(const FMovementEventInfo& MovementEvent)
{
	if (bGhostRecording && GhostData != nullptr)
	{
		UUTGhostEvent_MovementEvent* NewEvent = Cast<UUTGhostEvent_MovementEvent>(CreateAndAddEvent(UUTGhostEvent_MovementEvent::StaticClass()));
		if (NewEvent != nullptr)
		{
			NewEvent->MovementEvent = MovementEvent;
		}
	}
}

void UUTGhostComponent::GhostJumpBoots(TSubclassOf<class AUTReplicatedEmitter> SuperJumpEffect, USoundBase* SuperJumpSound)
{
	if (bGhostRecording && GhostData != nullptr)
	{
		UUTGhostEvent_JumpBoots* NewEvent = Cast<UUTGhostEvent_JumpBoots>(CreateAndAddEvent(UUTGhostEvent_JumpBoots::StaticClass()));
		if (NewEvent != nullptr)
		{
			NewEvent->SuperJumpEffect = SuperJumpEffect;
			NewEvent->SuperJumpSound = SuperJumpSound;
		}
	}
}

UUTGhostEvent* UUTGhostComponent::CreateAndAddEvent(TSubclassOf<UUTGhostEvent> EventClass)
{
	UUTGhostEvent* NewEvent = nullptr;
	if (bGhostRecording && GhostData != nullptr)
	{
		NewEvent = NewObject<UUTGhostEvent>(GhostData, EventClass);
		if (NewEvent != nullptr)
		{
			NewEvent->Time = GetWorld()->TimeSeconds - GhostStartTime;
			GhostData->Events.Add(NewEvent);
		}
	}
	return NewEvent;
}