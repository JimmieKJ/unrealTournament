// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProfileSettings.h"
#include "UTPlayerInput.h"
#include "GameFramework/InputSettings.h"

UUTProfileSettings::UUTProfileSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bNeedProfileWriteOnLevelChange = false;
}


void UUTProfileSettings::ResetProfile(EProfileResetType::Type SectionToReset)
{
	AUTPlayerController* DefaultUTPlayerController = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
	UUTPlayerInput* DefaultUTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
	UInputSettings* DefaultInputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::Character)
	{
		PlayerName = TEXT("Player");
		HatPath = TEXT("");
		LeaderHatPath = TEXT("");
		HatVariant = 0;
		EyewearPath = TEXT("");
		EyewearVariant = 0;
		TauntPath = TEXT("/Game/RestrictedAssets/Blueprints/Taunts/Taunt_Boom.Taunt_Boom_C");
		Taunt2Path = TEXT("/Game/RestrictedAssets/Blueprints/Taunts/Taunt_Bow.Taunt_Bow_C");
		CharacterPath = TEXT("");
		CountryFlag = FName(TEXT("Unreal"));
		Avatar = FName("UT.Avatar.0");

		WeaponBob = 1.0f;
		ViewBob = 1.0f;
		FFAPlayerColor = FLinearColor(0.020845f, 0.335f, 0.0f, 1.0f);
		PlayerFOV = 100.0f;
	}

	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::Challenges)
	{
		LocalXP = 0;
		ChallengeResults.Empty();
		UnlockedDailyChallenges.Empty();
	}

	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::HUD)
	{
		QuickStatsAngle = 180.0f;
		QuickStatsDistance = 0.08f;
		QuickStatsType = EQuickStatsLayouts::Arc;
		QuickStatsBackgroundAlpha = 0.15;
		QuickStatsForegroundAlpha = 1.0f;
		bQuickStatsHidden = true;
		bQuickInfoHidden = false;
		bHealthArcShown = false;
		QuickStatsScaleOverride = 0.75f;
		HealthArcRadius = 60;

		bHideDamageIndicators = false;
		bHidePaperdoll = true;
		bVerticalWeaponBar = false;

		HUDWidgetOpacity = 1.0f;
		HUDWidgetBorderOpacity = 1.0f;
		HUDWidgetSlateOpacity = 0.5f;
		HUDWidgetWeaponbarInactiveOpacity = 0.35f;
		HUDWidgetWeaponBarScaleOverride = 1.f;
		HUDWidgetWeaponBarInactiveIconOpacity = 0.35f;
		HUDWidgetWeaponBarEmptyOpacity = 0.2f;
		HUDWidgetScaleOverride = 1.f;
		HUDMessageScaleOverride = 1.0f;
		bUseWeaponColors = false;
		bDrawChatKillMsg = false;
		bDrawCenteredKillMsg = true;
		bDrawHUDKillIconMsg = true;
		bPlayKillSoundMsg = true;
	}

	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::Input)
	{
		bEnableMouseSmoothing = DefaultInputSettings ? DefaultInputSettings->bEnableMouseSmoothing : true;
		MouseAcceleration = DefaultUTPlayerInput ? DefaultUTPlayerInput->Acceleration : 0;
		MouseAccelerationPower = DefaultUTPlayerInput ? DefaultUTPlayerInput->AccelerationPower : 0;
		MouseAccelerationMax = DefaultUTPlayerInput ? DefaultUTPlayerInput->AccelerationMax : 0;
		DoubleClickTime = DefaultInputSettings ? DefaultInputSettings->DoubleClickTime : 0.3f;

		MouseSensitivity = DefaultUTPlayerInput ? DefaultUTPlayerInput->GetMouseSensitivity() : 1.0f;

		MaxDodgeClickTimeValue = 0.2;
		MaxDodgeTapTimeValue = 0.2;
		bSingleTapWallDodge = true;
		bSingleTapAfterJump = true;
		bAllowSlideFromRun = true;

		bInvertMouse = false;
	}
	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::Binds)
	{
		/**
		 *	Pull all of the input binds from DefaultInput.ini.  
		 **/

		CustomBinds.Empty();
		if (DefaultUTPlayerInput)
		{
			for (int32 i = 0; i < DefaultUTPlayerInput->CustomBinds.Num(); i++)
			{
				CustomBinds.Add(DefaultUTPlayerInput->CustomBinds[i]);
			}
		}

		ActionMappings.Empty();
		AxisMappings.Empty();
		AxisConfig.Empty();

		if (DefaultInputSettings)
		{			
			for (int32 i = 0; i < DefaultInputSettings->ActionMappings.Num(); i++)
			{
				ActionMappings.Add(DefaultInputSettings->ActionMappings[i]);
			}
			
			for (int32 i = 0; i < DefaultInputSettings->AxisMappings.Num(); i++)
			{
				AxisMappings.Add(DefaultInputSettings->AxisMappings[i]);
			}
			
			for (int32 i = 0; i < DefaultInputSettings->AxisConfig.Num(); i++)
			{
				AxisConfig.Add(DefaultInputSettings->AxisConfig[i]);
				if (AxisConfig[i].AxisKeyName == EKeys::MouseY && AxisConfig[i].AxisProperties.bInvert)
				{
					bInvertMouse = true;
				}
			}
		}

		ConsoleKey = EKeys::Tilde;
	}

	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::Social)
	{
		bSuppressToastsInGame = false;
	}

	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::System)
	{
		ReplayScreenshotResX = 1920.0f;
		ReplayScreenshotResY = 1080.0f;
		bReplayCustomPostProcess = false;
		ReplayCustomBloomIntensity = 0.2f;
		ReplayCustomDOFAmount = 0.0f;
		ReplayCustomDOFDistance = 0.0f;
		ReplayCustomDOFScale = 1.0f;
		ReplayCustomDOFNearBlur = 0.0f;
		ReplayCustomDOFFarBlur = 0.0f;
		ReplayCustomMotionBlurAmount = 0.0f;
		ReplayCustomMotionBlurMax = 0.0f;
		
		MatchmakingRegion = TEXT("NA");
		bPushToTalk = true;
		bHearsTaunts = DefaultUTPlayerController ? DefaultUTPlayerController->bHearsTaunts : true;
	}

	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::Weapons)
	{
		WeaponSkins.Empty();
		WeaponCustomizations.Empty();
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::Redeemer, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::Redeemer, 10, 10.0f, DefaultWeaponCrosshairs::Circle2, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::RocketLauncher, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::RocketLauncher, 8, 8.0f, DefaultWeaponCrosshairs::Triad2, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::Sniper, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::Sniper, 9, 9.0f, DefaultWeaponCrosshairs::Cross1, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::IGShockRifle, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::IGShockRifle, 4, 4.0f, DefaultWeaponCrosshairs::Cross1, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::ShockRifle, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::ShockRifle, 4, 4.0f, DefaultWeaponCrosshairs::Cross2, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::FlakCannon, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::FlakCannon, 7, 7.0f, DefaultWeaponCrosshairs::Triad3, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::LinkGun, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::LinkGun, 5, 5.0f, DefaultWeaponCrosshairs::Bracket1, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::Minigun, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::Minigun, 6, 6.0f, DefaultWeaponCrosshairs::Circle1, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::BioRifle, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::BioRifle, 3, 3.0f, DefaultWeaponCrosshairs::Triad1, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::Enforcer, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::Enforcer, 2, 2.0f, DefaultWeaponCrosshairs::Cross5, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::Translocator, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::Translocator, 0, 1.0f, DefaultWeaponCrosshairs::Cross3, FLinearColor::White, 1.0f));
		WeaponCustomizations.Add(EpicWeaponCustomizationTags::ImpactHammer, FWeaponCustomizationInfo(EpicWeaponCustomizationTags::ImpactHammer, 1, 1.0f, DefaultWeaponCrosshairs::Bracket1, FLinearColor::White, 1.0f));

		bAutoWeaponSwitch = true;

		WeaponWheelMapping.Empty();
		WeaponWheelMapping.Add(8);
		WeaponWheelMapping.Add(5);
		WeaponWheelMapping.Add(4);
		WeaponWheelMapping.Add(3);
		WeaponWheelMapping.Add(7);
		WeaponWheelMapping.Add(2);
		WeaponWheelMapping.Add(9);
		WeaponWheelMapping.Add(6);

	}
}


void UUTProfileSettings::VersionFixup()
{
	// Place any hacks needed to fix or update the profile here.  NOTE: you should no longer need to hack in changes for
	// input.  VerifyInputRules() will attempt to make sure any new input binds are brought in to the profile as well as
	// check for double bindings.

	// HACK - We aren't supporting weapon skins right now since they are disabled in netplay in editor builds.  So force clear them here just in
	// case there was stale data.
	WeaponSkins.Empty();
}



bool UUTProfileSettings::VerifyInputRules()
{
	bool bCustomBindsChanged = false;

	UUTPlayerInput* DefaultUTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
	if (DefaultUTPlayerInput)
	{
		// Verify that the custom binds is correct.  First we remove any custom binds in the profile that are no longer in the DefaultObject.

		int32 BindIndex = 0;
		while (BindIndex < CustomBinds.Num())
		{
			bool bFound = false;
			for (int32 i = 0; i < DefaultUTPlayerInput->CustomBinds.Num(); i++)
			{
				if (DefaultUTPlayerInput->CustomBinds[i].Command.Equals(CustomBinds[BindIndex].Command,ESearchCase::IgnoreCase))
				{
					bFound = true;
					break;
				}
			}
		
			if (!bFound)
			{
				CustomBinds.RemoveAt(BindIndex);
				bCustomBindsChanged = true;
			}
			else
			{
				BindIndex++;
			}
		}

		// Now add in any ini binds that are missing from the custom binds array
		
		for (int32 i = 0; i < DefaultUTPlayerInput->CustomBinds.Num(); i++)
		{
			bool bFound = false;
			for (int32 j = 0; j < CustomBinds.Num(); j++)
			{
				if (DefaultUTPlayerInput->CustomBinds[i].Command.Equals(CustomBinds[j].Command, ESearchCase::IgnoreCase))
				{
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				CustomBinds.Add(DefaultUTPlayerInput->CustomBinds[i]);
				bCustomBindsChanged = true;
			}
		}

		// Search for double key binds.
	}
	return bCustomBindsChanged;
}

void UUTProfileSettings::ApplyAllSettings(UUTLocalPlayer* ProfilePlayer)
{
	ProfilePlayer->bSuppressToastsInGame = bSuppressToastsInGame;
	ProfilePlayer->SetNickname(PlayerName);

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
		PC->bCrouchTriggersSlide = bAllowSlideFromRun;
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

	ProfilePlayer->SetCharacterPath(CharacterPath);
	ProfilePlayer->SetHatPath(HatPath);
	ProfilePlayer->SetLeaderHatPath(LeaderHatPath);
	ProfilePlayer->SetEyewearPath(EyewearPath);
	ProfilePlayer->SetTauntPath(TauntPath);
	ProfilePlayer->SetTaunt2Path(Taunt2Path);
	ProfilePlayer->SetHatVariant(HatVariant);
	ProfilePlayer->SetEyewearVariant(EyewearVariant);
}

void UUTProfileSettings::GetWeaponGroup(AUTWeapon* Weapon, int32& WeaponGroup, int32& GroupPriority)
{
	if (Weapon != nullptr)
	{
		WeaponGroup = Weapon->DefaultGroup;
		GroupPriority = Weapon->GroupSlot;

		if (WeaponCustomizations.Contains(Weapon->WeaponCustomizationTag))
		{
			WeaponGroup = WeaponCustomizations[Weapon->WeaponCustomizationTag].WeaponGroup;
		}
	}
}
void UUTProfileSettings::GetWeaponCustomization(FName WeaponCustomizationTag, FWeaponCustomizationInfo& outWeaponCustomizationInfo)
{
	if (WeaponCustomizations.Contains(WeaponCustomizationTag))
	{
		outWeaponCustomizationInfo= WeaponCustomizations[WeaponCustomizationTag];
	}
	else
	{
		outWeaponCustomizationInfo = FWeaponCustomizationInfo();
	}
}

void UUTProfileSettings::GetWeaponCustomizationForWeapon(AUTWeapon* Weapon, FWeaponCustomizationInfo& outWeaponCustomizationInfo)
{
	GetWeaponCustomization(Weapon->WeaponCustomizationTag, outWeaponCustomizationInfo);
}

FString UUTProfileSettings::GetWeaponSkinClassname(AUTWeapon* Weapon)
{
	if (WeaponSkins.Contains(Weapon->WeaponSkinCustomizationTag))
	{
		return WeaponSkins[Weapon->WeaponSkinCustomizationTag];
	}
	return TEXT("");
}
