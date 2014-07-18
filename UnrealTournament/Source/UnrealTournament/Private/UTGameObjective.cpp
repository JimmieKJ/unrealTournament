// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTGameObjective.h"
#include "Net/UnrealNetwork.h"

AUTGameObjective::AUTGameObjective(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	RootComponent = PCIP.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false, false, false);;
	CarriedObjectClass = NULL;
	InitialSpawnDelay = 0.0f;

	SetReplicates(true);
	bReplicateMovement = true;
	bAlwaysRelevant = true;
	NetPriority=1.0;
}

void AUTGameObjective::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGameObjective, MyCarriedObject);
}


void AUTGameObjective::InitializeObjective()
{
	Super::BeginPlay();
	if (InitialSpawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(this, &AUTGameObjective::CreateCarriedObject, InitialSpawnDelay, false);
	}
	else
	{
		CreateCarriedObject();
	}
}

void AUTGameObjective::CreateCarriedObject()
{
	UE_LOG(UT,Log,TEXT("Location: %s"), *GetActorLocation().ToString());
	if (CarriedObjectClass == NULL) return;	// Sanity

	FActorSpawnParameters Params;
	Params.Owner = this;

	MyCarriedObject = GetWorld()->SpawnActor<AUTCarriedObject>(CarriedObjectClass, GetActorLocation() + FVector(0,0,96), GetActorRotation(), Params);
	if (MyCarriedObject != NULL)
	{
		MyCarriedObject->Init(this);
	}
	else
	{
		UE_LOG(UT,Warning,TEXT("%s Could not create an object of type %s"), *GetNameSafe(this), *GetNameSafe(CarriedObjectClass));
	}

	UE_LOG(UT,Log,TEXT("Base: %s   Flag: %s"), *GetActorLocation().ToString(), *MyCarriedObject->GetActorLocation().ToString());	

}


FName AUTGameObjective::GetObjectState()
{
	if (MyCarriedObject != NULL) return MyCarriedObject->ObjectState;
	return NAME_None;
}


void AUTGameObjective::ObjectWasPickedUp(AUTCharacter* NewHolder)
{
	// Subclass me and place any effects code here.
}


void AUTGameObjective::ObjectReturnedHome(AUTCharacter* Returner)
{
	// Subclass me and place any effects code here
}

