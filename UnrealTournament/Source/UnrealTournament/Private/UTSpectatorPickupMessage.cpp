// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTSpectatorPickupMessage.h"

UUTSpectatorPickupMessage::UUTSpectatorPickupMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("GameMessages"));
	bIsStatusAnnouncement = true;
	bIsPartiallyUnique = true;
	FontSizeIndex = 1;

	PickupFormat = NSLOCTEXT("UUTSpectatorPickupMessage", "PickupFormat", "{PlayerName} picked up the {DisplayName}");
}


FText UUTSpectatorPickupMessage::ResolveMessage_Implementation(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	if (OptionalObject != nullptr && RelatedPlayerState_1 != nullptr)
	{
		FText DisplayName;
		const AUTInventory* Inventory = Cast<UClass>(OptionalObject) ? Cast<AUTInventory>(Cast<UClass>(OptionalObject)->GetDefaultObject()) : NULL;
		const AUTPickup* Pickup = Cast<UClass>(OptionalObject) ? Cast<AUTPickup>(Cast<UClass>(OptionalObject)->GetDefaultObject()) : NULL;

		if (Inventory != nullptr)
		{
			DisplayName = Inventory->DisplayName;
		}
		else if (Pickup != nullptr)
		{
			DisplayName = Pickup->GetDisplayName();
		}

		if (!DisplayName.IsEmpty())
		{
			int32 PlayerPickups = Switch & 0xFFFF;
			int32 TotalPickups = Switch >> 16;

			FText Format = PickupFormat;

			if (Switch > 0)
			{
				Format = FText::Format(FText::FromString(TEXT("{0}  ({Num} / {TotalNum})")), Format);
			}

			FFormatNamedArguments Args;
			Args.Add(TEXT("PlayerName"), FText::FromString(RelatedPlayerState_1->PlayerName));
			Args.Add(TEXT("DisplayName"), DisplayName);
			Args.Add(TEXT("Num"), PlayerPickups);
			Args.Add(TEXT("TotalNum"), TotalPickups);

			return FText::Format(Format, Args);
		}
	}
	return NSLOCTEXT("PickupMessage", "NoPickupMessage", "No Pickup Message");
}

