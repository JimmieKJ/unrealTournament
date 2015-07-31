// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProfileSettings.h"
#include "UTPlayerInput.h"
#include "GameFramework/InputSettings.h"

UUTProfileSettings::UUTProfileSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PlayerName = TEXT("Player");
	bSuppressToastsInGame = false;

	bNeedProfileWriteOnLevelChange = false;
}

void UUTProfileSettings::ClearWeaponPriorities()
{
	WeaponPriorities.Empty();
}

void UUTProfileSettings::VersionFixup()
{
	if (SettingsRevisionNum <= EMOTE_TO_TAUNT_PROFILESETTINGS_VERSION)
	{
		for (auto Iter = ActionMappings.CreateIterator(); Iter; ++Iter)
		{
			if (Iter->ActionName == FName(TEXT("PlayEmote1")))
			{
				Iter->ActionName = FName(TEXT("PlayTaunt"));
			}
			else if (Iter->ActionName == FName(TEXT("PlayEmote2")))
			{
				Iter->ActionName = FName(TEXT("PlayTaunt2"));
			}
			else if (Iter->ActionName == FName(TEXT("PlayEmote3")))
			{
				ActionMappings.RemoveAt(Iter.GetIndex());
			}
		}
	}
	if (SettingsRevisionNum <= TAUNTFIXUP_PROFILESETTINGS_VERSION)
	{
		FInputActionKeyMapping Taunt2;
		Taunt2.ActionName = FName(TEXT("PlayTaunt2"));
		Taunt2.Key = EKeys::K;
		ActionMappings.AddUnique(Taunt2);
	}
	if (SettingsRevisionNum <= SLIDEFROMRUN_FIXUP_PROFILESETTINGS_VERSION)
	{
		bAllowSlideFromRun = true;
	}
	if (SettingsRevisionNum <= HEARTAUNTS_FIXUP_PROFILESETTINGS_VERSION)
	{
		bHearsTaunts = true;
	}
	
}

void UUTProfileSettings::SetWeaponPriority(FString WeaponClassName, float NewPriority)
{
	for (int32 i=0;i<WeaponPriorities.Num(); i++)
	{
		if (WeaponPriorities[i].WeaponClassName == WeaponClassName)
		{
			if (WeaponPriorities[i].WeaponPriority != NewPriority)
			{
				WeaponPriorities[i].WeaponPriority = NewPriority;
			}
			break;
		}
	}

	WeaponPriorities.Add(FStoredWeaponPriority(WeaponClassName, NewPriority));
}

float UUTProfileSettings::GetWeaponPriority(FString WeaponClassName, float DefaultPriority)
{
	for (int32 i=0;i<WeaponPriorities.Num(); i++)
	{
		if (WeaponPriorities[i].WeaponClassName == WeaponClassName)
		{
			return WeaponPriorities[i].WeaponPriority;
		}
	}

	return DefaultPriority;
}


void UUTProfileSettings::GatherAllSettings(UUTLocalPlayer* ProfilePlayer)
{
	bSuppressToastsInGame = ProfilePlayer->bSuppressToastsInGame;
	PlayerName = ProfilePlayer->GetNickname();
	
	// Get all settings from the Player Controller
	AUTPlayerController* PC = Cast<AUTPlayerController>(ProfilePlayer->PlayerController);
	if (PC == NULL)
	{
		PC = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
	}

	if (PC != NULL)
	{
		MaxDodgeClickTimeValue = PC->MaxDodgeClickTime;
		MaxDodgeTapTimeValue = PC->MaxDodgeTapTime;
		bSingleTapWallDodge = PC->bSingleTapWallDodge;
		bSingleTapAfterJump = PC->bSingleTapAfterJump;
		bAutoWeaponSwitch = PC->bAutoWeaponSwitch;
		bAllowSlideFromRun = PC->bAllowSlideFromRun;
		bHearsTaunts = PC->bHearsTaunts;
		WeaponBob = PC->WeaponBobGlobalScaling;
		WeaponHand = PC->GetWeaponHand();
		FFAPlayerColor = PC->FFAPlayerColor;

		PlayerFOV = PC->ConfigDefaultFOV;

		// Get any settings from UTPlayerInput
		UUTPlayerInput* UTPlayerInput = Cast<UUTPlayerInput>(PC->PlayerInput);
		if (UTPlayerInput == NULL)
		{
			UTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
		}
		if (UTPlayerInput)
		{
			CustomBinds.Empty();
			for (int32 i = 0; i < UTPlayerInput->CustomBinds.Num(); i++)
			{
				CustomBinds.Add(UTPlayerInput->CustomBinds[i]);
			}
			//MouseSensitivity = UTPlayerInput->GetMouseSensitivity();
		}
	}

	// Grab the various settings from the InputSettings object.

	UInputSettings* DefaultInputSettingsObject = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	if (DefaultInputSettingsObject)
	{
		ActionMappings.Empty();
		for (int32 i = 0; i < DefaultInputSettingsObject->ActionMappings.Num(); i++)
		{
			ActionMappings.Add(DefaultInputSettingsObject->ActionMappings[i]);
		}

		AxisMappings.Empty();
		for (int32 i = 0; i < DefaultInputSettingsObject->AxisMappings.Num(); i++)
		{
			AxisMappings.Add(DefaultInputSettingsObject->AxisMappings[i]);
		}

		AxisConfig.Empty();
		for (int32 i = 0; i < DefaultInputSettingsObject->AxisConfig.Num(); i++)
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

		if (DefaultInputSettingsObject->ConsoleKeys.Num() > 0)
		{
			ConsoleKey = DefaultInputSettingsObject->ConsoleKeys[0];
		}
	}
}
void UUTProfileSettings::ApplyAllSettings(UUTLocalPlayer* ProfilePlayer)
{
	ProfilePlayer->bSuppressToastsInGame = bSuppressToastsInGame;
	ProfilePlayer->SetNickname(PlayerName);
	ProfilePlayer->SaveConfig();

	// Get all settings from the Player Controller
	AUTPlayerController* PC = Cast<AUTPlayerController>(ProfilePlayer->PlayerController);
	if (PC == NULL)
	{
		PC = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
	}

	if (PC != NULL)
	{
		PC->MaxDodgeClickTime = MaxDodgeClickTimeValue;
		PC->MaxDodgeTapTime = MaxDodgeTapTimeValue;
		PC->bSingleTapWallDodge = bSingleTapWallDodge;
		PC->bSingleTapAfterJump = bSingleTapAfterJump;
		PC->bAutoWeaponSwitch = bAutoWeaponSwitch;
		PC->bAllowSlideFromRun = bAllowSlideFromRun;
		PC->bHearsTaunts = bHearsTaunts;
		PC->WeaponBobGlobalScaling = WeaponBob;
		PC->SetWeaponHand(WeaponHand);
		PC->FFAPlayerColor = FFAPlayerColor;
		PC->ConfigDefaultFOV = PlayerFOV;

		PC->SaveConfig();
		// Get any settings from UTPlayerInput
		UUTPlayerInput* UTPlayerInput = Cast<UUTPlayerInput>(PC->PlayerInput);
		if (UTPlayerInput == NULL)
		{
			UTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
		}
		if (UTPlayerInput && CustomBinds.Num() > 0)
		{
			UTPlayerInput->CustomBinds.Empty();
			for (int32 i = 0; i < CustomBinds.Num(); i++)
			{
				UTPlayerInput->CustomBinds.Add(CustomBinds[i]);
			}
			//UTPlayerInput->SetMouseSensitivity(MouseSensitivity);
			UTPlayerInput->SaveConfig();
		}

		AUTCharacter* C = Cast<AUTCharacter>(PC->GetPawn());
		if (C != NULL)
		{
			C->NotifyTeamChanged();
		}
	}

	// Save all settings to the UInputSettings object
	UInputSettings* DefaultInputSettingsObject = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	if (DefaultInputSettingsObject)
	{
		if (ActionMappings.Num() > 0)
		{
			DefaultInputSettingsObject->ActionMappings.Empty();
			for (int32 i = 0; i < ActionMappings.Num(); i++)
			{
				DefaultInputSettingsObject->ActionMappings.Add(ActionMappings[i]);
			}
		}

		if (AxisConfig.Num() > 0)
		{
			DefaultInputSettingsObject->AxisConfig.Empty();
			for (int32 i = 0; i < AxisConfig.Num(); i++)
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
			for (int32 i = 0; i < AxisMappings.Num(); i++)
			{
				DefaultInputSettingsObject->AxisMappings.Add(AxisMappings[i]);
			}
		}

		DefaultInputSettingsObject->bEnableMouseSmoothing = bEnableMouseSmoothing;
		DefaultInputSettingsObject->bEnableFOVScaling = bEnableFOVScaling;
		DefaultInputSettingsObject->FOVScale = FOVScale;
		DefaultInputSettingsObject->DoubleClickTime = DoubleClickTime;
		DefaultInputSettingsObject->ConsoleKeys.Empty();
		DefaultInputSettingsObject->ConsoleKeys.Add(ConsoleKey);
		DefaultInputSettingsObject->SaveConfig();
	}

	if (ProfilePlayer->PlayerController != NULL && ProfilePlayer->PlayerController->PlayerInput != NULL)
	{
		// make sure default object mirrors live object
		ProfilePlayer->PlayerController->PlayerInput->GetClass()->GetDefaultObject()->ReloadConfig();

		UUTPlayerInput* UTPlayerInput = Cast<UUTPlayerInput>(ProfilePlayer->PlayerController->PlayerInput);
		if (UTPlayerInput != NULL)
		{
			UTPlayerInput->UTForceRebuildingKeyMaps(true);
		}
		else
		{
			ProfilePlayer->PlayerController->PlayerInput->ForceRebuildingKeyMaps(true);
		}
	}
	UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->ForceRebuildingKeyMaps(true);

	//Don't overwrite default crosshair info if nothing has been saved to the profile yet
	if (CrosshairInfos.Num() > 0)
	{
		//Apply crosshair settings to the hud(s)
		TArray<AUTHUD*> Huds;
		if (PC != nullptr && PC->MyUTHUD != nullptr)
		{
			Huds.Add(PC->MyUTHUD);
		}
		Huds.Add(AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>());

		for (AUTHUD* Hud : Huds)
		{
			Hud->LoadedCrosshairs.Empty(); //Force the hud to rebuild crosshairs after loading
			Hud->CrosshairInfos = CrosshairInfos;
			Hud->bCustomWeaponCrosshairs = bCustomWeaponCrosshairs;
			Hud->SaveConfig();
		}
	}
}

bool UUTProfileSettings::HasTokenBeenPickedUpBefore(FName TokenUniqueID)
{
	return FoundTokenUniqueIDs.Contains(TokenUniqueID);
}

void UUTProfileSettings::TokenPickedUp(FName TokenUniqueID)
{
	TempFoundTokenUniqueIDs.AddUnique(TokenUniqueID);
}

void UUTProfileSettings::TokenRevoke(FName TokenUniqueID)
{
	TempFoundTokenUniqueIDs.Remove(TokenUniqueID);
}

void UUTProfileSettings::TokensCommit()
{
	for (auto ID : TempFoundTokenUniqueIDs)
	{
		FoundTokenUniqueIDs.AddUnique(ID);
		bNeedProfileWriteOnLevelChange = true;
	}

	TempFoundTokenUniqueIDs.Empty();
}

void UUTProfileSettings::TokensReset()
{
	TempFoundTokenUniqueIDs.Empty();
}

void UUTProfileSettings::TokensClear()
{
	TempFoundTokenUniqueIDs.Empty();
	FoundTokenUniqueIDs.Empty();
}

bool UUTProfileSettings::GetBestTime(FName TimingName, float& OutBestTime)
{
	OutBestTime = 0;

	float* BestTime = BestTimes.Find(TimingName);
	if (BestTime)
	{
		OutBestTime = *BestTime;
		return true;
	}

	return false;
}

void UUTProfileSettings::SetBestTime(FName TimingName, float InBestTime)
{
	BestTimes.Add(TimingName, InBestTime);
	bNeedProfileWriteOnLevelChange = true;
}