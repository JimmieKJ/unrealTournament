// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGhostComponent.h"
#include "UTGhostData.h"
#include "UTGhostEvent.h"


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
			if (GhostDataClass != nullptr)
			{
				//write directly to the default object
				GhostData = GhostDataClass.GetDefaultObject();
			}
			else
			{
				GhostData = NewObject<UUTGhostData>(this, GhostDataClass);
			}
		}

		if (GhostData != nullptr)
		{
			GhostData->Events.Empty();
			bGhostRecording = true;
			GhostStartTime = GetWorld()->TimeSeconds;
			GhostFireFlags = 0;
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
	//load the data from a derrived BP class
	if (GhostData == nullptr && GhostDataClass != nullptr)
	{
		GhostData = NewObject<UUTGhostData>(this, GhostDataClass);
	}

	if (!bGhostRecording && !bGhostPlaying && GhostData != nullptr && GhostData->Events.Num() > 0)
	{
		bGhostPlaying = true;
		GhostStartTime = GetWorld()->TimeSeconds;
		GhostEventIndex = 0;
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
	/*if (GhostData != nullptr && GhostData->GhostMoves.Num() > 0)
	{
		UTOwner->SetActorLocation(GhostData->GhostMoves[0].RepMovement.Location);
	}*/
}


void UUTGhostComponent::GhostMove()
{
	//TODOTIM: Don't add a move per tick. Only add if necessary
	if (UTOwner->UTCharacterMovement != nullptr && GhostData != nullptr)
	{
		UUTGhostEvent_Move* MoveEvent = Cast<UUTGhostEvent_Move>(CreateAndAddEvent(UUTGhostEvent_Move::StaticClass()));
		if (MoveEvent != nullptr)
		{
			MoveEvent->bIsCrouched = UTOwner->bIsCrouched;
			MoveEvent->bApplyWallSlide = UTOwner->bApplyWallSlide;

			UTOwner->GatherUTMovement();
			MoveEvent->RepMovement = UTOwner->UTReplicatedMovement;

			FNetworkPredictionData_Client_UTChar FakePredict(*UTOwner->UTCharacterMovement);
			FSavedMove_UTCharacter FakeMove;
			FakeMove.SetMoveFor(UTOwner, 0, FVector(0.0f), FakePredict);
			MoveEvent->CompressedFlags = FakeMove.GetCompressedFlags();
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