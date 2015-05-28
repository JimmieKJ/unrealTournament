// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTGameObjective.h"
#include "Net/UnrealNetwork.h"

AUTGameObjective::AUTGameObjective(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false);
	CarriedObjectClass = NULL;
	InitialSpawnDelay = 0.0f;
	SetReplicates(true);
	bReplicateMovement = true;
	bAlwaysRelevant = true;
	NetPriority=1.0;
	BestViewYaw = 0.f;
	LastSecondSaveDistance = 2000.f;
	TeamNum = 255;
}

void AUTGameObjective::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGameObjective, CarriedObject);
	DOREPLIFETIME(AUTGameObjective, CarriedObjectState);
	DOREPLIFETIME(AUTGameObjective, CarriedObjectHolder);
}

void AUTGameObjective::InitializeObjective()
{
	if (InitialSpawnDelay > 0.0f)
	{
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameObjective::CreateCarriedObject, InitialSpawnDelay, false);
	}
	else
	{
		CreateCarriedObject();
	}
}

void AUTGameObjective::CreateCarriedObject()
{
	if (CarriedObjectClass == NULL) return;	// Sanity

	FActorSpawnParameters Params;
	Params.Owner = this;

	CarriedObject = GetWorld()->SpawnActor<AUTCarriedObject>(CarriedObjectClass, GetActorLocation() + FVector(0,0,96), GetActorRotation(), Params);
	if (CarriedObject != NULL)
	{
		CarriedObject->Init(this);
	}
	else
	{
		UE_LOG(UT,Warning,TEXT("%s Could not create an object of type %s"), *GetNameSafe(this), *GetNameSafe(CarriedObjectClass));
	}
}

AUTCarriedObject* AUTGameObjective::GetCarriedObject() const
{
	return CarriedObject;
}

FName AUTGameObjective::GetCarriedObjectState() const
{
	return CarriedObjectState;
}

AUTPlayerState* AUTGameObjective::GetCarriedObjectHolder()
{
	return CarriedObjectHolder;
}

void AUTGameObjective::ObjectWasPickedUp(AUTCharacter* NewHolder, bool bWasHome)
{
	CarriedObjectHolder = NewHolder != NULL ? Cast<AUTPlayerState>(NewHolder->PlayerState) : NULL;
}

void AUTGameObjective::ObjectWasDropped(AUTCharacter* LastHolder)
{
	CarriedObjectHolder = NULL;
}

void AUTGameObjective::ObjectReturnedHome(AUTCharacter* Returner)
{
	CarriedObjectHolder = NULL;
}

void AUTGameObjective::ObjectStateWasChanged(FName NewObjectState)
{
	if (Role==ROLE_Authority)
	{
		CarriedObjectState = NewObjectState;
		OnObjectStateChanged();
	}
}

void AUTGameObjective::OnObjectStateChanged()
{
	// Subclass me
}

bool AUTGameObjective::ActorIsNearMe(AActor *Other) const
{
	return ((Other->GetActorLocation() - GetActorLocation()).Size() < LastSecondSaveDistance);
}