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
		MouseAcceleration = 0.00005f;
		MouseAccelerationPower = 0.0f;
		MouseAccelerationMax = 1.0f;
		DoubleClickTime = 0.3f;

		MouseSensitivity = 0.07f;

		MaxDodgeClickTimeValue = 0.25;
		MaxDodgeTapTimeValue = 0.3;
		bSingleTapWallDodge = true;
		bSingleTapAfterJump = true;
		bAllowSlideFromRun = true;

		bInvertMouse = false;
	}
	if (SectionToReset == EProfileResetType::All || SectionToReset == EProfileResetType::Binds)
	{
		GetDefaultGameActions(GameActions);
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
		bCustomWeaponCrosshairs = false;
		bSingleCustomWeaponCrosshair = false;

		WeaponWheelMapping.Empty();
		WeaponWheelMapping.Add(8);
		WeaponWheelMapping.Add(5);
		WeaponWheelMapping.Add(4);
		WeaponWheelMapping.Add(3);
		WeaponWheelMapping.Add(7);
		WeaponWheelMapping.Add(2);
		WeaponWheelMapping.Add(9);
		WeaponWheelMapping.Add(6);

		SingleCustomWeaponCrosshair.CrosshairTag = DefaultWeaponCrosshairs::Dot;
		SingleCustomWeaponCrosshair.CrosshairColorOverride = FLinearColor::White;
		SingleCustomWeaponCrosshair.CrosshairScaleOverride = 1.0f;
	}
}

void UUTProfileSettings::GetDefaultGameActions(TArray<FKeyConfigurationInfo>& outGameActions)
{
	outGameActions.Empty();
	FKeyConfigurationInfo Key;

	// Move

	Key = FKeyConfigurationInfo("MoveForward", EControlCategory::Movement, EKeys::W, EKeys::Up, NSLOCTEXT("Keybinds","MoveForward","Move Forward"));
	Key.AddAxisMapping("MoveForward", 1.0f);
	Key.AddActionMapping("TapForward");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("MoveBackward", EControlCategory::Movement, EKeys::S, EKeys::Down, NSLOCTEXT("Keybinds", "MoveBackward", "Move Backwards"));
	Key.AddAxisMapping("MoveBackward", 1.0f);
	Key.AddActionMapping("TapBack");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("MoveLeft", EControlCategory::Movement, EKeys::A, EKeys::PageUp, NSLOCTEXT("Keybinds", "MoveLeft", "Strafe Left"));
	Key.AddAxisMapping("MoveLeft", 1.0f);
	Key.AddActionMapping("TapLeft");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("MoveRight", EControlCategory::Movement, EKeys::D, EKeys::PageDown, NSLOCTEXT("Keybinds", "MoveRight", "Strafe Right"));
	Key.AddAxisMapping("MoveRight", 1.0f);
	Key.AddActionMapping("TapRight");
	outGameActions.Add(Key);

	// Turn

	Key = FKeyConfigurationInfo("TurnLeft", EControlCategory::Movement, EKeys::Left, EKeys::Invalid, NSLOCTEXT("Keybinds", "TurnLeft", "Turn Left"));
	Key.AddAxisMapping("TurnRate", -1.0f);
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("TurnRight", EControlCategory::Movement, EKeys::Right, EKeys::Invalid, NSLOCTEXT("Keybinds", "TurnRight", "Turn Right"));
	Key.AddAxisMapping("TurnRate", 1.0f);
	outGameActions.Add(Key);

	// Actions

	Key = FKeyConfigurationInfo("Jump", EControlCategory::Movement, EKeys::SpaceBar, EKeys::Invalid, NSLOCTEXT("Keybinds", "Jump", "Jump"));
	Key.AddAxisMapping("MoveUp", 1.0f);
	Key.AddActionMapping("Jump");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("Crouch", EControlCategory::Movement, EKeys::LeftControl, EKeys::C, NSLOCTEXT("Keybinds", "Crouch", "Crouch"));
	Key.AddAxisMapping("MoveUp", -1.0f);
	Key.AddActionMapping("Crouch");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("Slide", EControlCategory::Movement, EKeys::LeftShift, EKeys::Invalid, NSLOCTEXT("Keybinds", "Slide", "Slide"));
	Key.AddActionMapping("Slide");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SingleTapDodge", EControlCategory::Movement, EKeys::V, EKeys::Invalid, NSLOCTEXT("Keybinds", "SingleTapDodge", "One Tap Dodge"));
	Key.AddActionMapping("SingleTapDodge");
	outGameActions.Add(Key);

	// Combat

	Key = FKeyConfigurationInfo("Fire", EControlCategory::Combat, EKeys::LeftMouseButton, EKeys::Invalid, NSLOCTEXT("Keybinds", "Fire", "Fire"));
	Key.AddActionMapping("StartFire");
	Key.AddActionMapping("StopFire");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("AltFire", EControlCategory::Combat, EKeys::RightMouseButton, EKeys::Invalid, NSLOCTEXT("Keybinds", "AltFire", "Alt Fire"));
	Key.AddActionMapping("StartAltFire");
	Key.AddActionMapping("StopAltFire");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("NextWeapon", EControlCategory::Combat, EKeys::MouseScrollUp, EKeys::Invalid, NSLOCTEXT("Keybinds", "NextWeapon", "Next Weapon"));
	Key.AddActionMapping("NextWeapon");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("PrevWeapon", EControlCategory::Combat, EKeys::MouseScrollDown, EKeys::Invalid, NSLOCTEXT("Keybinds", "PrevWeapon", "Previous Weapon"));
	Key.AddActionMapping("PrevWeapon");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("BestWeapon", EControlCategory::Combat, EKeys::Enter, EKeys::Invalid, NSLOCTEXT("Keybinds", "BestWeapon", "Best Weapon"));
	Key.AddActionMapping("BestWeapon");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("ThrowWeapon", EControlCategory::Combat, EKeys::M, EKeys::Invalid, NSLOCTEXT("Keybinds", "ThrowWeapon", "Throw Weapon"));
	Key.AddActionMapping("ThrowWeapon");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("ToggleTrans", EControlCategory::Combat, EKeys::Q, EKeys::Invalid, NSLOCTEXT("Keybinds", "ToggleTrans", "Toggle Translocator"));
	Key.AddCustomBinding("ToggleTranslocator");
	Key.Restrictions = EControlGameModeRestriction::RestrictInCTF;
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SelectTrans", EControlCategory::Combat, EKeys::Invalid, EKeys::Invalid, NSLOCTEXT("Keybinds", "SelectTrans", "Select Translocator"));
	Key.AddCustomBinding("SelectTranslocator");
	Key.Restrictions = EControlGameModeRestriction::RestrictInCTF;
	outGameActions.Add(Key);
	
	Key = FKeyConfigurationInfo("StartActivatePowerup", EControlCategory::Combat, EKeys::Q, EKeys::Invalid, NSLOCTEXT("Keybinds", "ActivatePowerup", "Activate Powerup"));
	Key.AddActionMapping("StartActivatePowerup");
	Key.Restrictions = EControlGameModeRestriction::RestrictInFlagRun;
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("ToggleWepaonWheel", EControlCategory::Combat, EKeys::Invalid, EKeys::Invalid, NSLOCTEXT("Keybinds", "ToggleWeaponWheel", "Show Weapon Wheel"));
	Key.AddActionMapping("ToggleWeaponWheel");
	outGameActions.Add(Key);

	// Weapon

	Key = FKeyConfigurationInfo("SwitchWeapon1", EControlCategory::Weapon, EKeys::One, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon1", "Select Weapon Group 1"));
	Key.AddCustomBinding("SwitchWeapon 1");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon2", EControlCategory::Weapon, EKeys::Two, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon2", "Select Weapon Group 2"));
	Key.AddCustomBinding("SwitchWeapon 2");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon3", EControlCategory::Weapon, EKeys::Three, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon3", "Select Weapon Group 3"));
	Key.AddCustomBinding("SwitchWeapon 3");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon4", EControlCategory::Weapon, EKeys::Four, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon4", "Select Weapon Group 4"));
	Key.AddCustomBinding("SwitchWeapon 4");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon5", EControlCategory::Weapon, EKeys::Five, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon5", "Select Weapon Group 5"));
	Key.AddCustomBinding("SwitchWeapon 5");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon6", EControlCategory::Weapon, EKeys::Six, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon6", "Select Weapon Group 6"));
	Key.AddCustomBinding("SwitchWeapon 6");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon7", EControlCategory::Weapon, EKeys::Seven, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon7", "Select Weapon Group 7"));
	Key.AddCustomBinding("SwitchWeapon 7");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon8", EControlCategory::Weapon, EKeys::Eight, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon8", "Select Weapon Group 8"));
	Key.AddCustomBinding("SwitchWeapon 8");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon9", EControlCategory::Weapon, EKeys::Nine, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon9", "Select Weapon Group 9"));
	Key.AddCustomBinding("SwitchWeapon 9");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("SwitchWeapon0", EControlCategory::Weapon, EKeys::Zero, EKeys::Invalid, NSLOCTEXT("Keybinds", "SwitchWeapon0", "Select Weapon Group 0"));
	Key.AddCustomBinding("SwitchWeapon 0");
	outGameActions.Add(Key);

	// Taunts

	Key = FKeyConfigurationInfo("Taunt1", EControlCategory::Taunts, EKeys::J, EKeys::Invalid, NSLOCTEXT("Keybinds", "Taunt1", "Taunt #1"));
	Key.AddActionMapping("PlayTaunt");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("Taunt2", EControlCategory::Taunts, EKeys::K, EKeys::Invalid, NSLOCTEXT("Keybinds", "Taunt2", "Taunt #2"));
	Key.AddActionMapping("PlayTaunt2");
	outGameActions.Add(Key);

	// UI

	Key = FKeyConfigurationInfo("ShowMenu", EControlCategory::UI, EKeys::Escape, EKeys::Invalid, NSLOCTEXT("Keybinds", "ShowMenu", "Show Menu"));
	Key.AddActionMapping("ShowMenu");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("ShowScores", EControlCategory::UI, EKeys::Tab, EKeys::Invalid, NSLOCTEXT("Keybinds", "ShowScores", "Show Scores"));
	Key.AddActionMapping("ShowScores");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("ShowConsole", EControlCategory::UI, EKeys::Tilde, EKeys::Invalid, NSLOCTEXT("Keybinds", "ShowConsole", "Show Console"));
	Key.AddActionMapping("Console");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("Talk", EControlCategory::UI, EKeys::T, EKeys::Invalid, NSLOCTEXT("Keybinds", "Talk", "Talk"));
	Key.AddActionMapping("Talk");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("TeamTalk", EControlCategory::UI, EKeys::Y, EKeys::Invalid, NSLOCTEXT("Keybinds", "TeamTalk", "Team Talk"));
	Key.AddActionMapping("TeamTalk");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("BuyMenu", EControlCategory::UI, EKeys::B, EKeys::Invalid, NSLOCTEXT("Keybinds", "BuyMenu", "Show Buy Menu"));
	Key.AddActionMapping("ShowBuyMenu");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("ComsMenu", EControlCategory::UI, EKeys::F, EKeys::Invalid, NSLOCTEXT("Keybinds", "ShowComsMenu", "Show Coms Menu"));
	Key.AddCustomBinding("ToggleComMenu");
	outGameActions.Add(Key);

	// Misc

	Key = FKeyConfigurationInfo("FeignDeath", EControlCategory::Misc, EKeys::H, EKeys::Invalid, NSLOCTEXT("Keybinds", "FeignDeath", "Feign Death"));
	Key.AddCustomBinding("FeignDeath");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("DropCarriedObject", EControlCategory::Misc, EKeys::G, EKeys::Invalid, NSLOCTEXT("Keybinds", "DropCarriedObject", "Drop Carried Object"));
	Key.AddActionMapping("DropCarriedObject");
	outGameActions.Add(Key);

	Key = FKeyConfigurationInfo("RequestRally", EControlCategory::Misc, EKeys::E, EKeys::Invalid, NSLOCTEXT("Keybinds", "RequestRally", "Request Rally"));
	Key.AddActionMapping("RequestRally");
	Key.Restrictions = EControlGameModeRestriction::RestrictInFlagRun;
	outGameActions.Add(Key);

/*
	Key = FKeyConfigurationInfo("PushToTalk", EControlCategory::Misc, EKeys::CapsLock, EKeys::Invalid, NSLOCTEXT("Keybinds", "PushToTalk", "Push to Talk"));
	Key.AddCustomBinding("PushToTalk");
	outGameActions.Add(Key);
*/
}


void UUTProfileSettings::ValidateGameActions()
{
	TArray<FKeyConfigurationInfo> DefaultGameActions;
	GetDefaultGameActions(DefaultGameActions);
	
	// Remove any GameActions that no longer exist in the default list.
	int32 Action = GameActions.Num()-1;
	while (Action >= 0)
	{
		bool bFound = false;
		for (int32 i=0; i < DefaultGameActions.Num(); i++)
		{
			if (GameActions[Action].GameActionTag == DefaultGameActions[i].GameActionTag)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound) GameActions.RemoveAt(Action,1);
		Action--;
	}

	// Now reverse it, add any are missing

	for (int32 i = 0; i < DefaultGameActions.Num(); i++)
	{
		bool bFound = false;
		for (int32 j = 0; j < GameActions.Num(); j++)
		{
			if (DefaultGameActions[i].GameActionTag == GameActions[j].GameActionTag)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			GameActions.Add(DefaultGameActions[i]);
		}
	}

}

void UUTProfileSettings::VersionFixup()
{
	// HACK - We aren't supporting weapon skins right now since they are disabled in netplay in editor builds.  So force clear them here just in
	// case there was stale data.
	WeaponSkins.Empty();
	ValidateGameActions();

}

void UUTProfileSettings::ApplyAllSettings(UUTLocalPlayer* ProfilePlayer)
{
	ProfilePlayer->bSuppressToastsInGame = bSuppressToastsInGame;
	ProfilePlayer->SetNickname(PlayerName);
	ProfilePlayer->SetCharacterPath(CharacterPath);
	ProfilePlayer->SetHatPath(HatPath);
	ProfilePlayer->SetLeaderHatPath(LeaderHatPath);
	ProfilePlayer->SetEyewearPath(EyewearPath);
	ProfilePlayer->SetTauntPath(TauntPath);
	ProfilePlayer->SetTaunt2Path(Taunt2Path);
	ProfilePlayer->SetHatVariant(HatVariant);
	ProfilePlayer->SetEyewearVariant(EyewearVariant);

	ApplyInputSettings(ProfilePlayer);

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
		if (bCustomWeaponCrosshairs)
		{
			if (bSingleCustomWeaponCrosshair)
			{
				outWeaponCustomizationInfo.CrosshairTag = SingleCustomWeaponCrosshair.CrosshairTag;
				outWeaponCustomizationInfo.CrosshairColorOverride = SingleCustomWeaponCrosshair.CrosshairColorOverride;
				outWeaponCustomizationInfo.CrosshairScaleOverride = SingleCustomWeaponCrosshair.CrosshairScaleOverride;
			}
		}
		else
		{
			outWeaponCustomizationInfo.CrosshairTag = WeaponCustomizations[WeaponCustomizationTag].DefaultCrosshairTag;
			outWeaponCustomizationInfo.CrosshairColorOverride = FLinearColor::White;
			outWeaponCustomizationInfo.CrosshairScaleOverride = 1.0f;
		}
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

void UUTProfileSettings::ApplyInputSettings(UUTLocalPlayer* ProfilePlayer)
{
	AUTBasePlayerController* BasePlayerController = Cast<AUTBasePlayerController>(ProfilePlayer->PlayerController);
	if (BasePlayerController == nullptr)
	{
		BasePlayerController = AUTBasePlayerController::StaticClass()->GetDefaultObject<AUTBasePlayerController>();
	}

	UUTPlayerInput* PlayerInput = Cast<UUTPlayerInput>(BasePlayerController->PlayerInput);
	if (PlayerInput == nullptr)
	{
		PlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
	}

	// First step, remove all of the mappings associated with our GameActions.

	for (int32 i = 0; i < GameActions.Num(); i++)
	{
		for (int32 B=0; B < GameActions[i].ActionMappings.Num(); B++)
		{
			int32 P = PlayerInput->ActionMappings.Num() - 1;
			while ( P >= 0 )
			{
				if (PlayerInput->ActionMappings[P].ActionName == GameActions[i].ActionMappings[B].ActionName
					&& PlayerInput->ActionMappings[P].bShift == GameActions[i].ActionMappings[B].bShift
					&& PlayerInput->ActionMappings[P].bCtrl == GameActions[i].ActionMappings[B].bCtrl
					&& PlayerInput->ActionMappings[P].bAlt == GameActions[i].ActionMappings[B].bAlt
					&& PlayerInput->ActionMappings[P].bCmd == GameActions[i].ActionMappings[B].bCmd)
				{
					PlayerInput->ActionMappings.RemoveAt(P,1);
				}
				P--;
			}
		}

		for (int32 B=0; B < GameActions[i].AxisMappings.Num(); B++)
		{
			int32 P = PlayerInput->AxisMappings.Num() - 1;
			while ( P >= 0 )
			{
				if (PlayerInput->AxisMappings[P].AxisName == GameActions[i].AxisMappings[B].AxisName)
				{
					PlayerInput->AxisMappings.RemoveAt(P,1);
				}
				P--;
			}
		}

		for (int32 B=0; B < GameActions[i].CustomBindings.Num(); B++)
		{
			int32 P = PlayerInput->CustomBinds.Num() - 1;
			while ( P >= 0 )
			{
				if (PlayerInput->CustomBinds[P].Command == GameActions[i].CustomBindings[B].Command)
				{
					PlayerInput->CustomBinds.RemoveAt(P,1);
				}
				P--;
			}
		}

		// Not yet...
		for (int32 B=0; B < GameActions[i].SpectatorBindings.Num(); B++)
		{
		}
	}

	// Next Add our actions back in.
	for (int32 i = 0; i < GameActions.Num(); i++)
	{
		// Special handling for the console key
		if ( GameActions[i].GameActionTag == FName(TEXT("ShowConsole")) )
		{
			// Save all settings to the UInputSettings object
			UInputSettings* DefaultInputSettingsObject = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
			if (DefaultInputSettingsObject)
			{
				DefaultInputSettingsObject->ConsoleKeys.Empty();
				if (GameActions[i].PrimaryKey != FKey()) DefaultInputSettingsObject->ConsoleKeys.Add(GameActions[i].PrimaryKey);
				if (GameActions[i].SecondaryKey != FKey()) DefaultInputSettingsObject->ConsoleKeys.Add(GameActions[i].SecondaryKey);
			}
		}
		else
		{
			for (int32 K = 0; K < 2; K++)
			{
				FKey Key = K == 0 ? GameActions[i].PrimaryKey : GameActions[i].SecondaryKey;
				if (Key != FKey())
				{
					for (int32 B=0; B < GameActions[i].ActionMappings.Num(); B++)
					{
						PlayerInput->ActionMappings.Add( FInputActionKeyMapping(
							GameActions[i].ActionMappings[B].ActionName,
							Key,
							GameActions[i].ActionMappings[B].bShift,
							GameActions[i].ActionMappings[B].bCtrl,
							GameActions[i].ActionMappings[B].bAlt,
							GameActions[i].ActionMappings[B].bCmd));
					}

					for (int32 B=0; B < GameActions[i].AxisMappings.Num(); B++)
					{
						PlayerInput->AddAxisMapping( FInputAxisKeyMapping(
								GameActions[i].AxisMappings[B].AxisName,
								Key,
								GameActions[i].AxisMappings[B].Scale));
					}

					for (int32 B=0; B < GameActions[i].CustomBindings.Num(); B++)
					{
						PlayerInput->CustomBinds.Add(FCustomKeyBinding(Key.GetFName(), GameActions[i].CustomBindings[B].EventType, GameActions[i].CustomBindings[B].Command));
					}

					// Not yet...
					for (int32 B=0; B < GameActions[i].SpectatorBindings.Num(); B++)
					{
					}
				}
			}
		}
	}

	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		PlayerInput->SetMouseSensitivity(MouseSensitivity);
		PlayerInput->AccelerationPower = MouseAccelerationPower;
		PlayerInput->Acceleration = MouseAcceleration;
		PlayerInput->AccelerationMax = MouseAccelerationMax;

		//Invert mouse
		for (FInputAxisConfigEntry& Entry : PlayerInput->AxisConfig)
		{
			if (Entry.AxisKeyName == EKeys::MouseX || Entry.AxisKeyName == EKeys::MouseY)
			{
				Entry.AxisProperties.Sensitivity = MouseSensitivity;
			}

			if (Entry.AxisKeyName == EKeys::MouseY)
			{
				Entry.AxisProperties.bInvert = bInvertMouse;
			}
		}
	}

	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	if (InputSettings)
	{
		//Mouse Smooth
		InputSettings->bEnableMouseSmoothing = bEnableMouseSmoothing;
		InputSettings->SaveConfig();
	}

	PlayerInput->ForceRebuildingKeyMaps(false);

	AUTPlayerController* PlayerController = Cast<AUTPlayerController>(BasePlayerController);
	if (PlayerController != nullptr)
	{
		PlayerController->UpdateWeaponGroupKeys();
		PlayerController->UpdateInventoryKeys();
		PlayerController->MaxDodgeClickTime = MaxDodgeClickTimeValue;
		PlayerController->MaxDodgeTapTime = MaxDodgeTapTimeValue;
		PlayerController->bSingleTapWallDodge = bSingleTapWallDodge;
		PlayerController->bSingleTapAfterJump = bSingleTapAfterJump;
		PlayerController->bCrouchTriggersSlide = bAllowSlideFromRun;
		PlayerController->bHearsTaunts = bHearsTaunts;
		PlayerController->WeaponBobGlobalScaling = WeaponBob;
		PlayerController->EyeOffsetGlobalScaling = ViewBob;
		PlayerController->SetWeaponHand(WeaponHand);
		PlayerController->FFAPlayerColor = FFAPlayerColor;
		PlayerController->ConfigDefaultFOV = PlayerFOV;

		AUTCharacter* Character = Cast<AUTCharacter>(PlayerController->GetPawn());
		if (Character != NULL)
		{
			Character->NotifyTeamChanged();
		}
	}

	// Save all settings to the UInputSettings object
	UInputSettings* DefaultInputSettingsObject = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	if (DefaultInputSettingsObject)
	{
		DefaultInputSettingsObject->bEnableMouseSmoothing = bEnableMouseSmoothing;
		DefaultInputSettingsObject->DoubleClickTime = DoubleClickTime;
	}


}
