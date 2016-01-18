// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTServerBeaconLobbyHostListener.h"
#include "UTServerBeaconClient.h"
#include "UTServerBeaconLobbyClient.h"

AUTServerBeaconLobbyHostObject::AUTServerBeaconLobbyHostObject(const FObjectInitializer& PCIP) :
Super(PCIP)
{
	ClientBeaconActorClass = AUTServerBeaconLobbyClient::StaticClass();
	BeaconTypeName = AUTServerBeaconLobbyClient::StaticClass()->GetDefaultObject<AUTServerBeaconLobbyClient>()->GetBeaconType();
}

void AUTServerBeaconLobbyHostObject::ClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
	UE_LOG(UT, Verbose, TEXT("Instance Connected %s from (%s)"),
		NewClientActor ? *NewClientActor->GetName() : TEXT("NULL"),
		NewClientActor ? *NewClientActor->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));
}

AOnlineBeaconClient* AUTServerBeaconLobbyHostObject::SpawnBeaconActor(UNetConnection* ClientConnection)
{
	FActorSpawnParameters SpawnInfo;
	AUTServerBeaconLobbyClient* BeaconActor = GetWorld()->SpawnActor<AUTServerBeaconLobbyClient>(AUTServerBeaconLobbyClient::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (BeaconActor)
	{
		BeaconActor->SetBeaconOwner(this);
	}

	return BeaconActor;
}
