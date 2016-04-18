// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"

#include "UTPowerupSelectorUserWidget.h"

UUTPowerupSelectorUserWidget::UUTPowerupSelectorUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanEverTick = true;
}

void UUTPowerupSelectorUserWidget::SetPlayerPowerup()
{
	if (SelectedPowerup != NULL)
	{
		UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(GetOwningLocalPlayer());
		if (UTLP)
		{
			AUTPlayerController* UTPC = Cast<AUTPlayerController>(UTLP->PlayerController);
			if (UTPC && UTPC->UTPlayerState)
			{
				UTPC->UTPlayerState->ServerSetBoostItem(SelectedPowerup);
			}
		}
	}
}