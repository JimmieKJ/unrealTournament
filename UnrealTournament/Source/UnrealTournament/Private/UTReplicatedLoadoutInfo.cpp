// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTReplicatedLoadoutInfo.h"
#include "Net/UnrealNetwork.h"
#include "Slate/SUWindowsStyle.h"

AUTReplicatedLoadoutInfo::AUTReplicatedLoadoutInfo(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;
	bNetLoadOnClient = false;
}

void AUTReplicatedLoadoutInfo::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, ItemTag, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, ItemClass, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, RoundMask, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, bDefaultInclude, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedLoadoutInfo, bPurchaseOnly, COND_InitialOnly);

	DOREPLIFETIME(AUTReplicatedLoadoutInfo, CurrentCost);
}

void AUTReplicatedLoadoutInfo::Destroyed()
{
#if !UE_SERVER
	SlateImage.Reset();
#endif
}


void AUTReplicatedLoadoutInfo::LoadItemImage()
{
#if !UE_SERVER
	if (!SlateImage.IsValid() && ItemClass != nullptr)
	{
		AUTInventory* DefaultInventory = ItemClass->GetDefaultObject<AUTInventory>();
		if (DefaultInventory != nullptr && DefaultInventory->MenuGraphic)
		{
			SlateImage = MakeShareable(new FSlateDynamicImageBrush(DefaultInventory->MenuGraphic, FVector2D(256.0, 128.0), NAME_None));
		}
	}
#endif
}


#if !UE_SERVER
const FSlateBrush* AUTReplicatedLoadoutInfo::GetItemImage() const
{
	return SlateImage.IsValid() ? SlateImage.Get() : SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");
}
#endif

