// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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

	ReplayCustomBloomIntensity = 0.2f;
	ReplayCustomDOFScale = 1.0f;

	Avatar = FName("UT.Avatar.0");

	QuickStatsAngle = 180;
	QuickStatsDistance = 0.1f;
	QuickStatsType = EQuickStatsLayouts::Arc;
	QuickStatsBackgroundAlpha=0.15f;
	QuickStatsForegroundAlpha=1.0f;
	bQuickStatsHidden=false;
	QuickStatsScaleOverride = 1.0f;
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
	if (SettingsRevisionNum <= PAUSEKEY_FIXUP_PROFILESETTINGS_VERSION)
	{
		new(CustomBinds) FCustomKeyBinding(EKeys::Pause.GetFName(), IE_Pressed, TEXT("Pause"));
	}
	if (SettingsRevisionNum < HUDSETTINGS_FIXUP_PROFILESETTINGS_VERSION)
	{
		ResetHUD();
	}

	// The format has changed during Dev versions.  So in case some people have written out unlocks, clear them here.
	if (SettingsRevisionNum <= CHALLENGE_FIXUP_VERSION)
	{
		UnlockedDailyChallenges.Empty();
	}
}

void UUTProfileSettings::ResetHUD()
{
	QuickStatsAngle = 180.0f;
	QuickStatsDistance = 0.1f;
	QuickStatsScaleOverride = 1.0f;
	QuickStatsType = EQuickStatsLayouts::Arc;
	QuickStatsBackgroundAlpha = 0.15;
	QuickStatsForegroundAlpha = 1.0f;
	bQuickStatsHidden = false;
	bQuickStatsBob = true;
			
	HUDWidgetOpacity = 1.0f;
	HUDWidgetBorderOpacity = 1.0f;
	HUDWidgetSlateOpacity = 0.5f;
	HUDWidgetWeaponbarInactiveOpacity = 0.25f;
	HUDWidgetWeaponBarScaleOverride = 0.9f;
	HUDWidgetWeaponBarInactiveIconOpacity = 0.25f;
	HUDWidgetWeaponBarEmptyOpacity = 0.0f;
	HUDWidgetScaleOverride = 0.7f;
	HUDMessageScaleOverride = 1.0f;
	bUseWeaponColors = false;
	bDrawChatKillMsg = true;
	bDrawCenteredKillMsg = true;
	bDrawHUDKillIconMsg = true;
	bPlayKillSoundMsg = true;
	bDrawCTFMinimapHUDSetting = true;
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
			return;
		}
	}

	WeaponPriorities.Add(FStoredWeaponPriority(WeaponClassName, NewPriority));
}

float UUTProfileSettings::GetWeaponPriority(FString WeaponClassName, float DefaultPriority)
{
	for (int32 i=0;i<WeaponPriorities.Num(); i++)
	{
		//We need to do a left string compare because unfortunately during the data baking process extra characters can be appended to the name blueprint name.
		const int WeaponPrioritiesNameSize = WeaponPriorities[i].WeaponClassName.Len();
		if (WeaponPriorities[i].WeaponClassName == WeaponClassName.Left(WeaponPrioritiesNameSize))
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
		ViewBob = PC->EyeOffsetGlobalScaling;
		WeaponHand = PC->GetPreferredWeaponHand();
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
			MouseAccelerationPower = UTPlayerInput->AccelerationPower;
			MouseAcceleration = UTPlayerInput->Acceleration;
			MouseAccelerationMax = UTPlayerInput->AccelerationMax;

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

	// Unfortunately, the profile may have multiple WeaponPriorites due to older bugs, so let's clean it up here

	int32 WeaponPriIdx = 1;
	while (WeaponPriIdx < WeaponPriorities.Num())
	{
		bool bFound = false;
		for (int32 j = 0; j < WeaponPriIdx; j++)
		{
			if (WeaponPriorities[j].WeaponClassName == WeaponPriorities[WeaponPriIdx].WeaponClassName)
			{
				bFound = true;
				UE_LOG(UT, Log, TEXT("Found Duplicate %s"), *WeaponPriorities[WeaponPriIdx].WeaponClassName);
			}
		}

		if (bFound)
		{
			WeaponPriorities.RemoveAt(WeaponPriIdx);
		}
		else
		{
			WeaponPriIdx++;
		}
	}


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
		PC->EyeOffsetGlobalScaling = ViewBob;
		PC->SetWeaponHand(WeaponHand);
		PC->FFAPlayerColor = FFAPlayerColor;
		PC->ConfigDefaultFOV = PlayerFOV;

		PC->SaveConfig();
		// Get any settings from UTPlayerInput
		UUTPlayerInput* UTPlayerInput = Cast<UUTPlayerInput>(PC->PlayerInput);
		if (UTPlayerInput == NULL)
		{
			UTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();

			UTPlayerInput->AccelerationPower = MouseAccelerationPower;
			UTPlayerInput->Acceleration = MouseAcceleration;
			UTPlayerInput->AccelerationMax = MouseAccelerationMax;
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
		//Copy crosshair settings to every hud class
		for (TObjectIterator<AUTHUD> It(EObjectFlags::RF_NoFlags, true); It; ++It)
		{
			AUTHUD* Hud = *It;
			if (Hud != nullptr)
			{
				UpdateCrosshairs(Hud);
			}
		}
	}

	ProfilePlayer->SetCharacterPath(CharacterPath);
	ProfilePlayer->SetHatPath(HatPath);
	ProfilePlayer->SetLeaderHatPath(LeaderHatPath);
	ProfilePlayer->SetEyewearPath(EyewearPath);
	ProfilePlayer->SetTauntPath(TauntPath);
	ProfilePlayer->SetTaunt2Path(Taunt2Path);
	ProfilePlayer->SetHatVariant(HatVariant);
	ProfilePlayer->SetEyewearVariant(EyewearVariant);

	for (int32 i=0; i < WeaponGroups.Num(); i++)
	{
		WeaponGroupLookup.Add(WeaponGroups[i].WeaponClassName, WeaponGroups[i]);
	}

	WeaponSkins.Remove(nullptr);
}

void UUTProfileSettings::UpdateCrosshairs(AUTHUD* HUD)
{
	HUD->LoadedCrosshairs.Empty();
	HUD->CrosshairInfos = CrosshairInfos;
	HUD->bCustomWeaponCrosshairs = bCustomWeaponCrosshairs;
	HUD->SaveConfig();
}
