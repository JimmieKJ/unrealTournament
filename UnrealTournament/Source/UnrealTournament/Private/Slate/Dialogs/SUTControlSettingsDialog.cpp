// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTControlSettingsDialog.h"
#include "../SUWindowsStyle.h"
#include "../SKeyBind.h"

#if !UE_SERVER

FSimpleBind::FSimpleBind(const FText& InDisplayName)
{
	DisplayName = InDisplayName.ToString();
	bHeader = false;
	Key = MakeShareable(new FKey());
	AltKey = MakeShareable(new FKey());
}
FSimpleBind* FSimpleBind::AddActionMapping(const FName& Mapping)
{
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	bool bFound = false;
	for (int32 i = 0; i < InputSettings->ActionMappings.Num(); i++)
	{
		FInputActionKeyMapping Action = InputSettings->ActionMappings[i];
		if (Mapping == Action.ActionName && !Action.Key.IsGamepadKey())
		{
			ActionMappings.Add(Action);

			//Fill in the first 2 keys we find from the ini
			if (*Key == FKey())
			{
				*Key = Action.Key;
			}
			else if (*AltKey == FKey() && Action.Key != *Key)
			{
				*AltKey = Action.Key;
			}
			bFound = true;
		}
	}

	if (!bFound)
	{
		FInputActionKeyMapping Action;
		Action.ActionName = Mapping;
		ActionMappings.Add(Action);
	}
	return this;
}

FSimpleBind* FSimpleBind::AddAxisMapping(const FName& Mapping, float Scale)
{
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	bool bFound = false;
	for (int32 i = 0; i < InputSettings->AxisMappings.Num(); i++)
	{
		FInputAxisKeyMapping Axis = InputSettings->AxisMappings[i];
		if (Mapping == Axis.AxisName && Axis.Scale == Scale && !Axis.Key.IsGamepadKey())
		{
			AxisMappings.Add(Axis);

			//Fill in the first 2 keys we find from the ini
			if (*Key == FKey())
			{
				*Key = Axis.Key;
			}
			else if (*AltKey == FKey() && Axis.Key != *Key)
			{
				*AltKey = Axis.Key;
			}
			bFound = true;
		}
	}

	if (!bFound)
	{
		FInputAxisKeyMapping Action;
		Action.AxisName = Mapping;
		Action.Scale = Scale;
		AxisMappings.Add(Action);
	}
	return this;
}

FSimpleBind* FSimpleBind::AddCustomBinding(const FName& Mapping)
{
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	bool bFound = false;
	UUTPlayerInput* UTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
	for (int32 i = 0; i < UTPlayerInput->CustomBinds.Num(); i++)
	{
		FCustomKeyBinding CustomBind = UTPlayerInput->CustomBinds[i];
		if (Mapping.ToString().Compare(CustomBind.Command) == 0 && !FKey(CustomBind.KeyName).IsGamepadKey())
		{
			//Fill in the first 2 keys we find from the ini
			if (*Key == FKey())
			{
				*Key = CustomBind.KeyName;
			}
			else if (*AltKey == FKey() && CustomBind.KeyName != *Key)
			{
				*AltKey = CustomBind.KeyName;
			}

			//Check for duplicates and add if unique
			bool bDuplicate = false;
			for (auto Bind : CustomBindings)
			{
				if (Bind.Command.Compare(CustomBind.Command) == 0 && Bind.KeyName.Compare(CustomBind.KeyName) == 0)
				{
					bDuplicate = true;
					break;
				}
			}
			if (!bDuplicate)
			{
				CustomBindings.Add(CustomBind);
			}
			bFound = true;
		}
	}

	if (!bFound)
	{
		FCustomKeyBinding* Bind = new(CustomBindings)FCustomKeyBinding;
		Bind->KeyName = NAME_None;
		Bind->EventType = IE_Pressed;
		Bind->Command = Mapping.ToString();
	}
	return this;
}

FSimpleBind* FSimpleBind::AddSpecialBinding(const FName& Mapping)
{
	if (Mapping == FName(TEXT("Console")))
	{
		UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
		if (InputSettings->ConsoleKeys.IsValidIndex(0))
		{
			*Key = InputSettings->ConsoleKeys[0];
		}
		if (InputSettings->ConsoleKeys.IsValidIndex(1))
		{
			*AltKey = InputSettings->ConsoleKeys[1];
		}
	}

	SpecialBindings.Add(Mapping);

	return this;
}

void FSimpleBind::WriteBind()
{
	if (bHeader)
	{
		return;
	}
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	// Collapse the keys if the main key is missing.
	if (*Key == FKey() && *AltKey != FKey())
	{
		Key = AltKey;
		AltKey = MakeShareable(new FKey());
	}

	//Remove the original bindings
	for (auto& Bind : ActionMappings)
	{
		InputSettings->RemoveActionMapping(Bind);
	}
	for (auto& Bind : AxisMappings)
	{
		InputSettings->RemoveAxisMapping(Bind);
	}
	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		UUTPlayerInput* UTPlayerInput = *It;
		for (auto& Bind : CustomBindings)
		{
			for (int32 i = 0; i < UTPlayerInput->CustomBinds.Num(); i++)
			{
				const FCustomKeyBinding& KeyMapping = UTPlayerInput->CustomBinds[i];
				if (KeyMapping.Command == Bind.Command)
				{
					UTPlayerInput->CustomBinds.RemoveAt(i, 1);
					i--;
				}
			}
		}
	}
	//Set our new keys and readd them
	for (auto Bind : ActionMappings)
	{
		Bind.Key = *Key;
		InputSettings->AddActionMapping(Bind);
		if (*AltKey != FKey())
		{
			Bind.Key = *AltKey;
			InputSettings->AddActionMapping(Bind);
		}
	}
	for (auto Bind : AxisMappings)
	{
		Bind.Key = *Key;
		InputSettings->AddAxisMapping(Bind);
		if (*AltKey != FKey())
		{
			Bind.Key = *AltKey;
			InputSettings->AddAxisMapping(Bind);
		}
	}

	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		UUTPlayerInput* UTPlayerInput = *It;
		for (auto Bind : CustomBindings)
		{
			Bind.KeyName = FName(*Key->ToString());
			UTPlayerInput->CustomBinds.Add(Bind);
			if (*AltKey != FKey())
			{
				Bind.KeyName = FName(*AltKey->ToString());
				UTPlayerInput->CustomBinds.Add(Bind);
			}
		}
	}

	for (FName Bind : SpecialBindings)
	{
		if (Bind == FName(TEXT("Console")))
		{
			InputSettings->ConsoleKeys.Empty();
			InputSettings->ConsoleKeys.Add(*Key);
			InputSettings->ConsoleKeys.Add(*AltKey);
		}
	}
}

void SUTControlSettingsDialog::CreateBinds()
{
	//Movement
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Movement", "Movement")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Move Forward", "Move Forward")))
		->AddAxisMapping("MoveForward", 1.0f)
		->AddActionMapping("TapForward")
		->AddActionMapping("TapForwardRelease")
		->AddDefaults(EKeys::W, EKeys::Up)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Move Backward", "Move Backward")))
		->AddAxisMapping("MoveBackward", 1.0f)
		->AddActionMapping("TapBack")
		->AddActionMapping("TapBackRelease")
		->AddDefaults(EKeys::S, EKeys::Down)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Move Left", "Move Left")))
		->AddAxisMapping("MoveLeft", 1.0f)
		->AddActionMapping("TapLeft")
		->AddActionMapping("TapLeftRelease")
		->AddDefaults(EKeys::A)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Move Right", "Move Right")))
		->AddAxisMapping("MoveRight", 1.0f)
		->AddActionMapping("TapRight")
		->AddActionMapping("TapRightRelease")
		->AddDefaults(EKeys::D)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Turn Left", "Turn Left")))
		->AddAxisMapping("TurnRate", -1.0f)
		->AddDefaults(EKeys::Left)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Turn Right", "Turn Right")))
		->AddAxisMapping("TurnRate", 1.0f)
		->AddDefaults(EKeys::Right)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Jump", "Jump")))
		->AddAxisMapping("MoveUp", 1.0f)
		->AddActionMapping("Jump")
		->AddDefaults(EKeys::SpaceBar)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Crouch", "Crouch")))
		->AddAxisMapping("MoveUp", -1.0f)
		->AddActionMapping("Crouch")
		->AddDefaults(EKeys::LeftControl, EKeys::C)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "One Tap Dodge", "One Tap Dodge")))
		->AddActionMapping("SingleTapDodge")
		->AddDefaults(EKeys::V)));
	//Weapons
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Weapons", "Weapons")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Fire", "Fire")))
		->AddActionMapping("StartFire")
		->AddActionMapping("StopFire")
		->AddDefaults(EKeys::LeftMouseButton, EKeys::RightControl)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Alt Fire", "Alt Fire")))
		->AddActionMapping("StartAltFire")
		->AddActionMapping("StopAltFire")
		->AddDefaults(EKeys::RightMouseButton)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Next Weapon", "Next Weapon")))
		->AddActionMapping("NextWeapon")
		->AddDefaults(EKeys::MouseScrollUp)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Previous Weapon", "Previous Weapon")))
		->AddActionMapping("PrevWeapon")
		->AddDefaults(EKeys::MouseScrollDown)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "BestWeapon", "Best Weapon")))
		->AddCustomBinding("SwitchToBestWeapon")
		->AddDefaults(EKeys::E)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Throw Weapon", "Throw Weapon")))
		->AddActionMapping("ThrowWeapon")
		->AddDefaults(EKeys::M)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Translocator", "Toggle Translocator")))
		->AddCustomBinding("ToggleTranslocator")
		->AddDefaults(EKeys::Q)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup1", "Select Group 1")))
		->AddCustomBinding("SwitchWeapon 1")
		->AddDefaults(EKeys::One)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup2", "Select Group 2")))
		->AddCustomBinding("SwitchWeapon 2")
		->AddDefaults(EKeys::Two)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup3", "Select Group 3")))
		->AddCustomBinding("SwitchWeapon 3")
		->AddDefaults(EKeys::Three)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup4", "Select Group 4")))
		->AddCustomBinding("SwitchWeapon 4")
		->AddDefaults(EKeys::Four)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup5", "Select Group 5")))
		->AddCustomBinding("SwitchWeapon 5")
		->AddDefaults(EKeys::Five)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup6", "Select Group 6")))
		->AddCustomBinding("SwitchWeapon 6")
		->AddDefaults(EKeys::Six)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup7", "Select Group 7")))
		->AddCustomBinding("SwitchWeapon 7")
		->AddDefaults(EKeys::Seven)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup8", "Select Group 8")))
		->AddCustomBinding("SwitchWeapon 8")
		->AddDefaults(EKeys::Eight)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup9", "Select Group 9")))
		->AddCustomBinding("SwitchWeapon 9")
		->AddDefaults(EKeys::Nine)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select WeaponGroup10", "Select Group 10")))
		->AddCustomBinding("SwitchWeapon 10")
		->AddDefaults(EKeys::Zero)));
	//Taunts
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Taunts", "Taunts")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Taunt", "Taunt 1")))
		->AddActionMapping("PlayTaunt")
		->AddDefaults(EKeys::J)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Taunt2", "Taunt 2")))
		->AddActionMapping("PlayTaunt2")
		->AddDefaults(EKeys::K)));
	//Misc
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Misc", "Misc")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Feign Death", "Feign Death")))
		->AddCustomBinding("FeignDeath")
		->AddDefaults(EKeys::H)));
	//Hud
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Hud", "Hud")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Show Scores", "Show Scores")))
		->AddActionMapping("ShowScores")
		->AddDefaults(EKeys::Tab)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Show Menu", "Show Menu")))
		->AddActionMapping("ShowMenu")
		->AddDefaults(EKeys::Escape)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Console", "Console")))
		->AddSpecialBinding("Console")
		->AddDefaults(EKeys::Tilde)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Talk", "Talk")))
		->AddActionMapping("Talk")
		->AddDefaults(EKeys::T)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Team Talk", "Team Talk")))
		->AddActionMapping("TeamTalk")
		->AddDefaults(EKeys::Y)));

	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Buy Menu", "Buy Menu")))
		->AddActionMapping("ShowBuyMenu")
		->AddDefaults(EKeys::B)));

	// TODO: mod binding registration
}

void SUTControlSettingsDialog::Construct(const FArguments& InArgs)
{
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	CreateBinds();

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(46)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.MidFill"))
						]
					]
				]

			]
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(46)
					[
						SNew(SHorizontalBox)
		
						+ SHorizontalBox::Slot()
						.Padding(FMargin(25.0f,0.0f,0.0f,0.0f))
						.AutoWidth()
						[
							SAssignNew(KeyboardSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.Text(NSLOCTEXT("SUTControlSettingsDialog", "ControlTabKeyboard", "Keyboard"))
							.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							.OnClicked(this, &SUTControlSettingsDialog::OnTabClickKeyboard)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(MouseSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							.Text(NSLOCTEXT("SUTControlSettingsDialog", "ControlTabMouse", "Mouse"))
							.OnClicked(this, &SUTControlSettingsDialog::OnTabClickMouse)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(MovementSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							.Text(NSLOCTEXT("SUTControlSettingsDialog", "ControlTabMovement", "Movement"))
							.OnClicked(this, &SUTControlSettingsDialog::OnTabClickMovement)
						]

					]
				]

				// Content

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.Padding(5.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(0.0f, 5.0f, 0.0f, 5.0f)
					.AutoHeight()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						//Keyboard Settings
						SAssignNew(TabWidget, SWidgetSwitcher)

						// Key binds

						+ SWidgetSwitcher::Slot()
						[
							BuildKeyboardTab()
						]

						//Mouse Settings

						+ SWidgetSwitcher::Slot()
						[
							BuildMouseTab()
						]
						//Movement Settings
						+ SWidgetSwitcher::Slot()
						[
							BuildMovementTab()
						]			
					]
				]
			]
		];
	}

	OnTabClickKeyboard();
}
	
TSharedRef<class SWidget> SUTControlSettingsDialog::BuildCustomButtonBar()
{
	return SAssignNew(ResetToDefaultsButton, SButton)
		.HAlign(HAlign_Center)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
		.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
		.Text(NSLOCTEXT("SUTControlSettingsDialog", "BindDefault", "RESET TO DEFAULTS"))
		.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
		.OnClicked(this, &SUTControlSettingsDialog::OnBindDefaultClick);
}

TSharedRef<SWidget> SUTControlSettingsDialog::BuildKeyboardTab()
{
	TSharedPtr<SVerticalBox> KeyboardBox;
	SAssignNew(KeyboardBox, SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(5.0f, 5.0f, 10.0f, 5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(10.0f, 0.0f, 10.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(), "UT.Option.ColumnHeaders")
			.Text(NSLOCTEXT("SUTControlSettingsDialog", "Action", "Action"))
		]
		+ SHorizontalBox::Slot()
		.Padding(10.0f, 0.0f, 10.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.FillWidth(1.2f)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(), "UT.Option.ColumnHeaders")
			.Text(NSLOCTEXT("SUTControlSettingsDialog", "KeyBinds", "Key"))
		]
		+ SHorizontalBox::Slot()
		.Padding(10.0f, 0.0f, 10.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.FillWidth(1.2f)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(), "UT.Option.ColumnHeaders")
			.Text(NSLOCTEXT("SUTControlSettingsDialog", "KeyBinds", "Alternate Key"))
		]
	]

	//Key bind list

	+SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 10.0f, 10.0f, 5.0f))
	[
		SAssignNew(ControlList, SVerticalBox)
	];

	if (KeyboardBox.IsValid())
	{
		//Create the bind list
		for (const auto& Bind : Binds)
		{
			if (Bind->bHeader)
			{
				ControlList->AddSlot()
				.AutoHeight()
				.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.BoldText")
						.Text(FText::FromString(Bind->DisplayName))
					]
				];
			}
			else
			{
				ControlList->AddSlot()
				.AutoHeight()
				.Padding(FMargin(10.0f, 4.0f, 10.0f, 4.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(FText::FromString(Bind->DisplayName))
					]
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 4.0f, 10.0f, 4.0f)
					[
						SAssignNew(Bind->KeyWidget, SKeyBind)
						.Key(Bind->Key)
						.DefaultKey(Bind->DefaultKey)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
						.OnKeyBindingChanged( this, &SUTControlSettingsDialog::OnKeyBindingChanged, Bind, true)
					]
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 4.0f, 10.0f, 4.0f)
					[
						SAssignNew(Bind->AltKeyWidget, SKeyBind)
						.ContentPadding(FMargin(4.0f, 4.0f))
						.Key(Bind->AltKey)
						.DefaultKey(Bind->DefaultAltKey)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
						.OnKeyBindingChanged( this, &SUTControlSettingsDialog::OnKeyBindingChanged, Bind, false)
					]
				];
			}
		}
	}

	return KeyboardBox.ToSharedRef();
}

TSharedRef<SWidget> SUTControlSettingsDialog::BuildMouseTab()
{
	MouseSensitivityRange = FVector2D(0.0075f, 0.15f);
	MouseAccelerationRange = FVector2D(0.00001f, 0.0001f);
	MouseAccelerationMaxRange = FVector2D(0.5f, 1.5f);

	//Is Mouse Inverted
	bool bMouseInverted = false;
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::MouseY)
		{
			bMouseInverted = Entry.AxisProperties.bInvert;
			break;
		}
	}
	
	return SNew(SBox)
	.VAlign(VAlign_Fill)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.AutoHeight()
		.HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUTControlSettingsDialog", "MouseSmoothing", "Mouse Smoothing"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(MouseSmoothing, SCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.IsChecked(GetDefault<UInputSettings>()->bEnableMouseSmoothing ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUTControlSettingsDialog", "MouseInvert", "Invert Mouse"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(MouseInvert, SCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.IsChecked(bMouseInverted ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			]
		]		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUTControlSettingsDialog", "MouseSensitivity", "Mouse Sensitivity (0-20)"))
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(200)
				[
					SAssignNew(MouseSensitivityEdit, SEditableTextBox)
					.MinDesiredWidth(200)
					.Style(SUWindowsStyle::Get(),"UT.Common.Editbox.White")
					.Text(FText::AsNumber(UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->GetMouseSensitivity()))
					.OnTextCommitted(this, &SUTControlSettingsDialog::EditSensitivity)
				]		
			]
			+ SHorizontalBox::Slot()
			.Padding(20.0f, 0.0f, 0.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(400)
				.Content()
				[
					SAssignNew(MouseSensitivity, SSlider)
					.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
					.Orientation(Orient_Horizontal)
					.Value((UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->GetMouseSensitivity() - MouseSensitivityRange.X) / (MouseSensitivityRange.Y - MouseSensitivityRange.X))
				]
			]
		]		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUTControlSettingsDialog", "MouseAcceleration", "Mouse Acceleration"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(MouseAccelerationCheckBox, SCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.IsChecked(UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->AccelerationPower > 0 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			]
		]				
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUTControlSettingsDialog", "MouseAccelerationAmount", "Acceleration Amount (0-20)"))
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(200)
				[
					SAssignNew(MouseAccelerationEdit, SEditableTextBox)
					.MinDesiredWidth(200)
					.Style(SUWindowsStyle::Get(),"UT.Common.Editbox.White")
					.Text(FText::AsNumber(UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->Acceleration))
					.OnTextCommitted(this, &SUTControlSettingsDialog::EditAcceleration)
				]		
			]
			+ SHorizontalBox::Slot()
			.Padding(20.0f, 0.0f, 0.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(400)
				.Content()
				[
					SAssignNew(MouseAcceleration, SSlider)
					.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
					.Orientation(Orient_Horizontal)
					.Value((UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->Acceleration - MouseAccelerationRange.X) / (MouseAccelerationRange.Y - MouseAccelerationRange.X))
				]
			]
		]	
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUTControlSettingsDialog", "MouseAccelerationMax", "Acceleration Max (0-20)"))
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(200)
				[
					SAssignNew(MouseAccelerationMaxEdit, SEditableTextBox)
					.MinDesiredWidth(200)
					.Style(SUWindowsStyle::Get(),"UT.Common.Editbox.White")
					.Text(FText::AsNumber(UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->AccelerationMax))
					.OnTextCommitted(this, &SUTControlSettingsDialog::EditAccelerationMax)
				]		
			]
			+ SHorizontalBox::Slot()
			.Padding(20.0f, 0.0f, 0.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(400)
				.Content()
				[
					SAssignNew(MouseAccelerationMax, SSlider)
					.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
					.Orientation(Orient_Horizontal)
					.Value((UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->AccelerationMax - MouseAccelerationMaxRange.X) / (MouseAccelerationMaxRange.Y - MouseAccelerationMaxRange.X))
				]
			]
		]	
	];
}

TSharedRef<SWidget> SUTControlSettingsDialog::BuildMovementTab()
{
	//Get the dodge settings
	AUTPlayerController* PC = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
	MaxDodgeClickTimeValue = PC->MaxDodgeClickTime;
	MaxDodgeTapTimeValue = PC->MaxDodgeTapTime;

	return SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.Padding(FMargin(10.0f, 25.0f, 10.0f, 5.0f))
	.AutoHeight()
	.HAlign(HAlign_Left)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(750)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTControlSettingsDialog", "SingleTapWallDodge", "Enable one tap wall dodge"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(SingleTapWallDodge, SCheckBox)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.ForegroundColor(FLinearColor::White)
			.IsChecked(PC->bSingleTapWallDodge ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		]
	]
	+ SVerticalBox::Slot()
	.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
	.AutoHeight()
	.HAlign(HAlign_Left)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(750)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTControlSettingsDialog", "SingleTapAfterJump", "One tap wall dodge only after jump"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(SingleTapAfterJump, SCheckBox)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.ForegroundColor(FLinearColor::White)
			.IsChecked(PC->bSingleTapAfterJump ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		]
	]
	+ SVerticalBox::Slot()
	.HAlign(HAlign_Left)
	.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(750)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTControlSettingsDialog", "MaxDodgeTapTime", "One Tap Wall Dodge Hold Time"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(150)
			[
				SAssignNew(MaxDodgeTapTime, SNumericEntryBox<float>)
				.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.Editbox.White")
				.LabelPadding(FMargin(50.0f,50.0f))
				.Value(this, &SUTControlSettingsDialog::GetMaxDodgeTapTimeValue)
				.OnValueCommitted(this, &SUTControlSettingsDialog::SetMaxDodgeTapTimeValue)
			]
		]
	]
	+ SVerticalBox::Slot()
	.HAlign(HAlign_Left)
	.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(750)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTControlSettingsDialog", "MaxDodgeClickTime", "Dodge Double-Click Time"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(150)
			[
				SAssignNew(MaxDodgeClickTime, SNumericEntryBox<float>)
				.EditableTextBoxStyle(SUWindowsStyle::Get(),"UT.Common.Editbox.White")
				.Value(this, &SUTControlSettingsDialog::GetMaxDodgeClickTimeValue)
				.OnValueCommitted(this, &SUTControlSettingsDialog::SetMaxDodgeClickTimeValue)
			]
		]
	]
	+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 25.0f, 10.0f, 5.0f))
		.AutoHeight()
		.HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(750)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUTControlSettingsDialog", "Slide From Run", "Enable slide from run"))
				]
			]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SlideFromRun, SCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.ForegroundColor(FLinearColor::White)
					.IsChecked(PC->bAllowSlideFromRun ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				]
		];
}

FReply SUTControlSettingsDialog::OnBindDefaultClick()
{
	//Reset all the binds to their default value
	for (auto Bind : Binds)
	{
		if (!Bind->bHeader)
		{
			Bind->KeyWidget->SetKey(Bind->DefaultKey, false,false);
			Bind->AltKeyWidget->SetKey(Bind->DefaultAltKey, false,false);
		}
	}
	return FReply::Handled();
}

FReply SUTControlSettingsDialog::OKClick()
{
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	//Write the binds to the ini
	for (const auto& Bind : Binds)
	{
		Bind->WriteBind();
	}
	InputSettings->SaveConfig();

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (UTPC != nullptr)
	{
		UTPC->UpdateWeaponGroupKeys();
	}

	//Update the playing players custom binds
	APlayerController* PC = GetPlayerOwner()->PlayerController;
	if (PC != NULL && Cast<UUTPlayerInput>(PC->PlayerInput) != NULL)
	{
		Cast<UUTPlayerInput>(PC->PlayerInput)->CustomBinds = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->CustomBinds;
	}

	//Mouse Settings
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	// mouse sensitivity
	float NewSensitivity = MouseSensitivity->GetValue() * (MouseSensitivityRange.Y - MouseSensitivityRange.X) + MouseSensitivityRange.X;
	float NewAcceleration = MouseAcceleration->GetValue() * (MouseAccelerationRange.Y - MouseAccelerationRange.X) + MouseAccelerationRange.X;
	float NewAccelerationMax = MouseAccelerationMax->GetValue() * (MouseAccelerationMaxRange.Y - MouseAccelerationMaxRange.X) + MouseAccelerationMaxRange.X;
	float NewAccelerationPower = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->AccelerationPower;
	if (MouseAccelerationCheckBox->IsChecked())
	{
		// Leave untouched if non-zero
		if (NewAccelerationPower <= 0)
		{
			NewAccelerationPower = 1.0f;
		}
	}
	else
	{
		NewAccelerationPower = 0;
	}
	UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->AccelerationPower = NewAccelerationPower;
	UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->Acceleration = NewAcceleration;
	UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->AccelerationMax = NewAccelerationMax;

	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		It->SetMouseSensitivity(NewSensitivity);
		It->AccelerationPower = NewAccelerationPower;
		It->Acceleration = NewAcceleration;
		It->AccelerationMax = NewAccelerationMax;

		//Invert mouse
		for (FInputAxisConfigEntry& Entry : It->AxisConfig)
		{
			if (Entry.AxisKeyName == EKeys::MouseY)
			{
				Entry.AxisProperties.bInvert = MouseInvert->IsChecked();
			}
		}
	}

	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::MouseX || Entry.AxisKeyName == EKeys::MouseY)
		{
			Entry.AxisProperties.Sensitivity = NewSensitivity;
		}
	}
	//Invert Mouse
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::MouseY)
		{
			Entry.AxisProperties.bInvert = MouseInvert->IsChecked();
		}
	}
	//Mouse Smooth
	InputSettings->bEnableMouseSmoothing = MouseSmoothing->IsChecked();
	InputSettings->SaveConfig();

	//Movement settings
	for (TObjectIterator<AUTPlayerController> It(RF_NoFlags); It; ++It)
	{
		It->bSingleTapWallDodge = SingleTapWallDodge->IsChecked();
		It->bSingleTapAfterJump = SingleTapAfterJump->IsChecked();
		It->MaxDodgeClickTime = MaxDodgeClickTimeValue;
		It->MaxDodgeTapTime = MaxDodgeTapTimeValue;
		It->bAllowSlideFromRun = SlideFromRun->IsChecked();
	}
	AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>()->SaveConfig();
	UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->SaveConfig();

	GetPlayerOwner()->CloseDialog(SharedThis(this));
	GetPlayerOwner()->SaveProfileSettings();

	return FReply::Handled();
}

FReply SUTControlSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUTControlSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();

}

void SUTControlSettingsDialog::EditSensitivity(const FText& Input, ETextCommit::Type)
{
	float ChangedSensitivity = FCString::Atof(*Input.ToString());

	if (ChangedSensitivity >= 0.0f && ChangedSensitivity <= 20.0f)
	{
		ChangedSensitivity /= 20.0f;
		MouseSensitivity->SetValue(ChangedSensitivity);
	}
}

void SUTControlSettingsDialog::EditAcceleration(const FText& Input, ETextCommit::Type)
{
	float ChangedAcceleration = FCString::Atof(*Input.ToString());

	if (ChangedAcceleration >= 0.0f && ChangedAcceleration <= 20.0f)
	{
		ChangedAcceleration /= 20.0f;
		MouseAcceleration->SetValue(ChangedAcceleration);
	}
}

void SUTControlSettingsDialog::EditAccelerationMax(const FText& Input, ETextCommit::Type)
{
	float ChangedAccelerationMax = FCString::Atof(*Input.ToString());

	if (ChangedAccelerationMax >= 0.0f && ChangedAccelerationMax <= 20.0f)
	{
		ChangedAccelerationMax /= 20.0f;
		MouseAccelerationMax->SetValue(ChangedAccelerationMax);
	}
}

void SUTControlSettingsDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SUTDialogBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (MouseSensitivityEdit.IsValid() && !MouseSensitivityEdit->HasKeyboardFocus())
	{		
		MouseSensitivityEdit->SetText(FText::FromString(FString::Printf(TEXT("%f"), MouseSensitivity->GetValue() * 20.0f)));
	}

	if (MouseAccelerationEdit.IsValid() && !MouseAccelerationEdit->HasKeyboardFocus())
	{
		MouseAccelerationEdit->SetText(FText::FromString(FString::Printf(TEXT("%f"), MouseAcceleration->GetValue() * 20.0f)));
	}

	if (MouseAccelerationMaxEdit.IsValid() && !MouseAccelerationMaxEdit->HasKeyboardFocus())
	{
		MouseAccelerationMaxEdit->SetText(FText::FromString(FString::Printf(TEXT("%f"), MouseAccelerationMax->GetValue() * 20.0f)));
	}
}

void SUTControlSettingsDialog::OnKeyBindingChanged( FKey PreviousKey, FKey NewKey, TSharedPtr<FSimpleBind> BindingThatChanged, bool bPrimaryKey ) 
{
	// Primary or Alt key changed to a valid state.  Search through all the other bindings to find a duplicate
	if( NewKey.IsValid() )
	{
		FString DisplayNameOfDuplicate;

		for (const auto& Bind : Binds)
		{
			if( !Bind->bHeader && Bind != BindingThatChanged )
			{
				if( NewKey == (*Bind->Key) || NewKey == (*Bind->AltKey ) )
				{
					DisplayNameOfDuplicate = Bind->DisplayName;
					break;
				}
			}
		}

		TSharedPtr<SUTControlSettingsDialog> CSD = SharedThis(this);

		auto OnDuplicateDialogResult = [CSD, BindingThatChanged, bPrimaryKey, NewKey, PreviousKey](TSharedPtr<SCompoundWidget> Widget, uint16 Button)
		{
			
			if( Button == UTDIALOG_BUTTON_NO )
			{
				if( bPrimaryKey )
				{
					BindingThatChanged->KeyWidget->SetKey( PreviousKey, true, false );
				}
				else
				{
					BindingThatChanged->AltKeyWidget->SetKey( PreviousKey, true, false );
				}
			}
			else if (Button == UTDIALOG_BUTTON_YESCLEAR)
			{
				for (const auto& Bind : CSD->Binds)
				{
					if (!Bind->bHeader && Bind != BindingThatChanged)
					{
						if (NewKey == (*Bind->Key))
						{
							Bind->KeyWidget->SetKey(FKey());
						}
						
						if (NewKey == (*Bind->AltKey))
						{
							Bind->AltKeyWidget->SetKey(FKey());
						}
					}
				}
			}
		};

		if( !DisplayNameOfDuplicate.IsEmpty() )
		{
			GetPlayerOwner()->ShowMessage
			( 
				NSLOCTEXT("SUTControlSettingsDialog", "DuplicateBindingTitle", "Duplicate Binding"),
				FText::Format( NSLOCTEXT("SUTControlSettingsDialog", "DuplicateBindingMessage", "{0} is already bound to {1}.\n\nBind to {2} anyway?"), NewKey.GetDisplayName(), FText::FromString( DisplayNameOfDuplicate ), FText::FromString(BindingThatChanged->DisplayName) ),
				UTDIALOG_BUTTON_YES | UTDIALOG_BUTTON_YESCLEAR | UTDIALOG_BUTTON_NO,
				FDialogResultDelegate::CreateLambda( OnDuplicateDialogResult )
			);
		}
	}

}

#endif