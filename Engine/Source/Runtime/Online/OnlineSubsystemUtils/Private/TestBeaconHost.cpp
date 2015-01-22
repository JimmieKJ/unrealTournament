// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"

ATestBeaconHost::ATestBeaconHost(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	BeaconTypeName = TEXT("TestBeacon");
}

bool ATestBeaconHost::Init()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBeacon, Verbose, TEXT("Init"));
#endif
	return true;
}

void ATestBeaconHost::ClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBeacon, Verbose, TEXT("ClientConnected %s from (%s)"), 
		NewClientActor ? *NewClientActor->GetName() : TEXT("NULL"), 
		NewClientActor ? *NewClientActor->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));

	ATestBeaconClient* BeaconClient = Cast<ATestBeaconClient>(NewClientActor);
	if (BeaconClient != NULL)
	{
		BeaconClient->ClientPing();
	}
#endif
}

AOnlineBeaconClient* ATestBeaconHost::SpawnBeaconActor(UNetConnection* ClientConnection)
{	
#if !UE_BUILD_SHIPPING
	FActorSpawnParameters SpawnInfo;
	ATestBeaconClient* BeaconActor = GetWorld()->SpawnActor<ATestBeaconClient>(ATestBeaconClient::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (BeaconActor)
	{
		BeaconActor->SetBeaconOwner(this);
	}

	return BeaconActor;
#else
	return NULL;
#endif
}
