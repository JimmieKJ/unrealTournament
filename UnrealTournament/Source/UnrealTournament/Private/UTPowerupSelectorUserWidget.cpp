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
	UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(GetOwningLocalPlayer());
	if (UTLP)
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(UTLP->PlayerController);
		if (UTPC && UTPC->UTPlayerState)
		{
			UTPC->UTPlayerState->ServerSetBoostItem(SelectedPowerupIndex);
		}
	}
}

FString UUTPowerupSelectorUserWidget::GetBuyMenuKeyName()
{
	FText returnValue = FText::GetEmpty();

	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	if (InputSettings)
	{
		for (int32 inputIndex = 0; inputIndex < InputSettings->ActionMappings.Num(); ++inputIndex)
		{
			FInputActionKeyMapping& Action = InputSettings->ActionMappings[inputIndex];
			if (Action.ActionName == "ShowBuyMenu")
			{
				returnValue = Action.Key.GetDisplayName();
				break;
			}
		}
	}

	return returnValue.ToString();
}