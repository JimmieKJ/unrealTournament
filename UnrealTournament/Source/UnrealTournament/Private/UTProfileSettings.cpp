// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProfileSettings.h"
#include "UTPlayerInput.h"
#include "GameFramework/InputSettings.h"

UUTProfileSettings::UUTProfileSettings(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	PlayerName = TEXT("Malcolm");
}


void UUTProfileSettings::SetPlayerName(FString NewName)
{
	PlayerName = NewName;
}

FString UUTProfileSettings::GetPlayerName()
{
	return PlayerName;
}

void UUTProfileSettings::GatherInputSettings()
{
	UInputSettings* DefaultInputSettingsObject = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	if (DefaultInputSettingsObject)
	{
		for (int i=0;i<DefaultInputSettingsObject->ActionMappings.Num();i++)
		{
			ActionMappings.Add(DefaultInputSettingsObject->ActionMappings[i]);
		}

		for (int i=0;i<DefaultInputSettingsObject->AxisMappings.Num();i++)
		{
			AxisMappings.Add(DefaultInputSettingsObject->AxisMappings[i]);
		}

		for (int i=0;i < DefaultInputSettingsObject->AxisConfig.Num() ;i++)
		{
			AxisConfig.Add(DefaultInputSettingsObject->AxisConfig[i]);
			if (AxisConfig[i].AxisKeyName == EKeys::MouseY && AxisConfig[i].AxisProperties.bInvert)
			{
				bInvertMouse = true;
			}
		}

		bEnableMouseSmoothing = DefaultInputSettingsObject->bEnableMouseSmoothing;
		bEnableFOVScaling = DefaultInputSettingsObject->bEnableFOVScaling;
		FOVScale = DefaultInputSettingsObject->FOVScale;
		DoubleClickTime = DefaultInputSettingsObject->DoubleClickTime;
		ConsoleKey = DefaultInputSettingsObject->ConsoleKey;
	}


	UUTPlayerInput* UTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
	if (UTPlayerInput)
	{
		for (int i=0;i < UTPlayerInput->CustomBinds.Num() ;i++)
		{
			CustomBinds.Add(UTPlayerInput->CustomBinds[i]);
		}
		MouseSensitivity = UTPlayerInput->GetMouseSensitivity();
		
	}

	AUTPlayerController* PC = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
	if (PC != NULL)
	{
		MaxDodgeClickTimeValue = PC->MaxDodgeClickTime;
		MaxDodgeTapTimeValue = PC->MaxDodgeTapTime;
		bSingleTapWallDodge = PC->bSingleTapWallDodge;
	}

}

void UUTProfileSettings::ApplyInputSettings()
{
	UInputSettings* DefaultInputSettingsObject = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	if (DefaultInputSettingsObject)
	{
		if (ActionMappings.Num() > 0)
		{
			DefaultInputSettingsObject->ActionMappings.Empty();
			for (int i=0;i<ActionMappings.Num();i++)
			{
				DefaultInputSettingsObject->ActionMappings.Add(ActionMappings[i]);
			}
		}

		if (AxisConfig.Num() > 0)
		{
			DefaultInputSettingsObject->AxisConfig.Empty();
			for (int i=0;i<AxisConfig.Num();i++)
			{
				DefaultInputSettingsObject->AxisConfig.Add(AxisConfig[i]);
				if (bInvertMouse && AxisConfig[i].AxisKeyName == EKeys::MouseY)
				{
					AxisConfig[i].AxisProperties.bInvert = true;
				}
			}
		}

		if (AxisMappings.Num() > 0)
		{
			DefaultInputSettingsObject->AxisMappings.Empty();
			for (int i=0;i<AxisMappings.Num();i++)
			{
				DefaultInputSettingsObject->AxisMappings.Add(AxisMappings[i]);
			}
		}

		DefaultInputSettingsObject->bEnableMouseSmoothing = bEnableMouseSmoothing;
		DefaultInputSettingsObject->bEnableFOVScaling = bEnableFOVScaling;
		DefaultInputSettingsObject->FOVScale = FOVScale;
		DefaultInputSettingsObject->DoubleClickTime = DoubleClickTime;
		DefaultInputSettingsObject->ConsoleKey = ConsoleKey;
	}

	UUTPlayerInput* DefaultUTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
	if (DefaultUTPlayerInput && CustomBinds.Num() > 0)
	{
		for (int i=0;i < CustomBinds.Num() ;i++)
		{
			DefaultUTPlayerInput->CustomBinds.Add(CustomBinds[i]);
		}
		DefaultUTPlayerInput->SetMouseSensitivity(MouseSensitivity);
	}

	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		UUTPlayerInput* UTPlayerInput = *It;
		UTPlayerInput->SetMouseSensitivity(MouseSensitivity);
		UTPlayerInput->UTForceRebuildingKeyMaps(true);
	}

}
