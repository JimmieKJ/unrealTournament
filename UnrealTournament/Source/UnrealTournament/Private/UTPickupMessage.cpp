// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/Console.h"
#include "UTLocalMessage.h"
#include "UTHUD.h"
#include "UTAnnouncer.h"
#include "UTPickupMessage.h"
#include "UTPickup.h"
#include "UTPickupWeapon.h"
#include "UTWeapon.h"

UUTPickupMessage::UUTPickupMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("PickupMessage"));
	StyleTag = FName(TEXT("Default"));

	bCountInstances = true;
	bIsUnique = true;
	//bIsConsoleMessage = false;
}

FText UUTPickupMessage::ResolveMessage_Implementation(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	const AUTPickup* Pickup = Cast<UClass>(OptionalObject) ? Cast<AUTPickup>(Cast<UClass>(OptionalObject)->GetDefaultObject()) : NULL;
	if (Pickup)
	{
		return Pickup->PickupMessageString;
	}
	const AUTInventory* Weapon = Cast<UClass>(OptionalObject) ? Cast<AUTInventory>(Cast<UClass>(OptionalObject)->GetDefaultObject()) : NULL;
	if (Weapon)
	{
		return Weapon->DisplayName;
	}
	return NSLOCTEXT("PickupMessage", "NoPickupMessage", "No Pickup Message");
}

void UUTPickupMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	Super::ClientReceive(ClientData);

	if (ClientData.LocalPC)
	{
		AUTHUD* HUD = Cast<AUTHUD>(ClientData.LocalPC->MyHUD);
		if (HUD)
		{
			HUD->LastPickupTime = HUD->GetWorld()->GetTimeSeconds();
		}
	}
}
