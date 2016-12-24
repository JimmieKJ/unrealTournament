// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTControlSettingsDialog.h"
#include "../SUWindowsStyle.h"
#include "../SKeyBind.h"
#include "SScrollBox.h"

#if !UE_SERVER


void SUTControlSettingsDialog::Construct(const FArguments& InArgs)
{
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.IsScrollable(false)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.OnDialogResult(InArgs._OnDialogResult)
						);

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
					.HeightOverride(55)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
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
					.HeightOverride(55)
					[
						SNew(SHorizontalBox)
		
						+ SHorizontalBox::Slot()
						.Padding(FMargin(25.0f,0.0f,0.0f,0.0f))
						.AutoWidth()
						[
							SAssignNew(KeyboardSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
							.IsToggleButton(true)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
							.Text(NSLOCTEXT("SUTControlSettingsDialog", "ControlTabKeyboard", "Keyboard"))
							.OnClicked(this, &SUTControlSettingsDialog::OnTabClickKeyboard)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(MouseSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
							.IsToggleButton(true)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
							.Text(NSLOCTEXT("SUTControlSettingsDialog", "ControlTabMouse", "Mouse"))
							.OnClicked(this, &SUTControlSettingsDialog::OnTabClickMouse)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(MovementSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
							.IsToggleButton(true)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
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
		.FillWidth(0.4f)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(), "UT.Option.ColumnHeaders")
			.Text(NSLOCTEXT("SUTControlSettingsDialog", "Action", "Action"))
		]
		+ SHorizontalBox::Slot()
		.Padding(10.0f, 0.0f, 10.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.FillWidth(0.3f)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(), "UT.Option.ColumnHeaders")
			.Text(NSLOCTEXT("SUTControlSettingsDialog", "KeyBinds", "Key"))
		]
		+ SHorizontalBox::Slot()
		.Padding(10.0f, 0.0f, 10.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.FillWidth(0.3f)
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
		SNew(SBox).HeightOverride(700)
		[
			SAssignNew(ScrollBox, SScrollBox)
			.Orientation(Orient_Vertical)
			+SScrollBox::Slot().Padding(0.0f,5.0f,0.0f,0.0f)
			[
				SAssignNew(ControlList, SVerticalBox)
			]
		]
	];

	UUTProfileSettings* ProfileSettings = PlayerOwner->GetProfileSettings();

	if (ProfileSettings && KeyboardBox.IsValid())
	{
		EControlCategory::Type CurrentSection = EControlCategory::MAX;
		for (int32 i = 0 ; i < ProfileSettings->GameActions.Num(); i++)		
		{
			if (ProfileSettings->GameActions[i].GameActionTag != FName(TEXT("ShowMenu")) )
			{

				// If the section changes, create a new section
				if (ProfileSettings->GameActions[i].Category != CurrentSection)
				{
					CurrentSection = ProfileSettings->GameActions[i].Category;
					FText SectionTitle = FText::GetEmpty();
					switch (ProfileSettings->GameActions[i].Category)
					{
						case EControlCategory::Movement :	SectionTitle = NSLOCTEXT("SUTControlSettingsDialog", "MovementHeader", "Movement"); break;
						case EControlCategory::Weapon :		SectionTitle = NSLOCTEXT("SUTControlSettingsDialog", "WeaponHeader", "Weapon"); break;
						case EControlCategory::Combat :		SectionTitle = NSLOCTEXT("SUTControlSettingsDialog", "CombatHeader", "Combat"); break;
						case EControlCategory::UI:			SectionTitle = NSLOCTEXT("SUTControlSettingsDialog", "UIHeader", "UI"); break;
						case EControlCategory::Taunts :		SectionTitle = NSLOCTEXT("SUTControlSettingsDialog", "TauntsHeader", "Taunts"); break;
						case EControlCategory::Misc :		SectionTitle = NSLOCTEXT("SUTControlSettingsDialog", "MiscHeader", "Misc"); break;
					}

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
							.Text(SectionTitle)
						]
					];
				}
			
				// Add the bind

				TSharedPtr<FKeyBindTracker> Bind = FKeyBindTracker::Make(&ProfileSettings->GameActions[i]);
				if (Bind.IsValid())
				{
					BindList.Add(Bind);
					ControlList->AddSlot()
					.AutoHeight()
					.Padding(FMargin(10.0f, 4.0f, 10.0f, 4.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(10.0f, 0.0f, 10.0f, 0.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.FillWidth(0.4f)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(Bind->KeyConfig->MenuText)
							.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SUTControlSettingsDialog::GetLabelColorAndOpacity, Bind)))
						]
						+ SHorizontalBox::Slot()
						.Padding(10.0f, 4.0f, 10.0f, 4.0f)
						.FillWidth(0.30f)
						[
							SAssignNew(Bind->PrimaryKeyBindWidget, SKeyBind)
							.Key(MakeShareable(&Bind->KeyConfig->PrimaryKey))
							.DefaultKey(Bind->KeyConfig->PrimaryKey)
							.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.Keybind")
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
							.OnKeyBindingChanged(this, &SUTControlSettingsDialog::OnKeyBindingChanged, Bind, true)
						]
						+ SHorizontalBox::Slot()
						.Padding(10.0f, 4.0f, 10.0f, 4.0f)
						.FillWidth(0.30f)
						[
							SAssignNew(Bind->SecondaryKeyBindWidget, SKeyBind)
							.ContentPadding(FMargin(4.0f, 4.0f))
							.Key(MakeShareable(&Bind->KeyConfig->SecondaryKey))
							.DefaultKey(Bind->KeyConfig->SecondaryKey)
							.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.Keybind")
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
							.OnKeyBindingChanged(this, &SUTControlSettingsDialog::OnKeyBindingChanged, Bind, false)
						]
					];
				}
			}
		}

		// Add a little extra space at the end because the scrollbox is dumb
		ControlList->AddSlot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 10.0f))
		[
			SNew(SCanvas)
		];

	}
	return KeyboardBox.ToSharedRef();
}

FSlateColor SUTControlSettingsDialog::GetLabelColorAndOpacity(TSharedPtr<FKeyBindTracker> Tracker) const
{
	if (Tracker.IsValid())
	{
		if (Tracker->PrimaryKeyBindWidget.IsValid() && Tracker->PrimaryKeyBindWidget->GetKey() == EKeys::Invalid &&
			Tracker->SecondaryKeyBindWidget.IsValid() && Tracker->SecondaryKeyBindWidget->GetKey() == EKeys::Invalid)
		{
			return FSlateColor(FLinearColor::Yellow);
		}
	}
	return FSlateColor(FLinearColor::White);
}


TSharedRef<SWidget> SUTControlSettingsDialog::BuildMouseTab()
{
	UUTProfileSettings* ProfileSettings = PlayerOwner->GetProfileSettings();

	MouseSensitivityRange = FVector2D(0.001f, 0.15f);
	MouseAccelerationRange = FVector2D(0.00001f, 0.0001f);
	MouseAccelerationMaxRange = FVector2D(0.5f, 1.5f);
		
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
				.IsChecked(ProfileSettings->bEnableMouseSmoothing ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
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
				.IsChecked(ProfileSettings->bInvertMouse ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
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
					.Text(FText::AsNumber(ProfileSettings->MouseSensitivity))
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
					.IndentHandle(false)
					.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
					.Orientation(Orient_Horizontal)
					.Value((ProfileSettings->MouseSensitivity - MouseSensitivityRange.X) / (MouseSensitivityRange.Y - MouseSensitivityRange.X))
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
				.IsChecked(ProfileSettings->MouseAccelerationPower > 0 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
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
					.Text(FText::AsNumber(ProfileSettings->MouseAcceleration))
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
					.IndentHandle(false)
					.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
					.Orientation(Orient_Horizontal)
					.Value((ProfileSettings->MouseAcceleration - MouseAccelerationRange.X) / (MouseAccelerationRange.Y - MouseAccelerationRange.X))
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
					.Text(FText::AsNumber(ProfileSettings->MouseAccelerationMax))
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
					.IndentHandle(false)
					.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
					.Orientation(Orient_Horizontal)
					.Value((ProfileSettings->MouseAccelerationMax - MouseAccelerationMaxRange.X) / (MouseAccelerationMaxRange.Y - MouseAccelerationMaxRange.X))
				]
			]
		]	
	];
}

TSharedRef<SWidget> SUTControlSettingsDialog::BuildMovementTab()
{
	UUTProfileSettings* ProfileSettings = PlayerOwner->GetProfileSettings();

	MaxDodgeClickTimeValue = ProfileSettings->MaxDodgeClickTimeValue;
	MaxDodgeTapTimeValue = ProfileSettings->MaxDodgeTapTimeValue;

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
				.Text(NSLOCTEXT("SUTControlSettingsDialog", "SingleTapWallDodge", "Enable single movement key tap wall dodge"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(SingleTapWallDodge, SCheckBox)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.ForegroundColor(FLinearColor::White)
			.IsChecked(ProfileSettings->bSingleTapWallDodge ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
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
				.Text(NSLOCTEXT("SUTControlSettingsDialog", "SingleTapAfterJump", "Single tap wall dodge only after jump"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(SingleTapAfterJump, SCheckBox)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.ForegroundColor(FLinearColor::White)
			.IsChecked(ProfileSettings->bSingleTapAfterJump ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
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
				.Text(NSLOCTEXT("SUTControlSettingsDialog", "MaxDodgeTapTime", "Single Tap Wall Dodge Hold Time"))
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
				.Text(NSLOCTEXT("SUTControlSettingsDialog", "EnableDoubleTapDodge", "Enable Double Tap Dodge"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(150)
			[
				SAssignNew(EnableDoubleTapDodge, SCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.IsChecked(ProfileSettings->bEnableDoubleTapDodge ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
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
					.Text(NSLOCTEXT("SUTControlSettingsDialog", "Slide From Crouch", "Crouch button will trigger slide if moving"))
				]
			]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SlideFromRun, SCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.ForegroundColor(FLinearColor::White)
					.IsChecked(ProfileSettings->bAllowSlideFromRun ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				]
		];
}

FReply SUTControlSettingsDialog::OnBindDefaultClick()
{
	UUTProfileSettings* ProfileSettings = PlayerOwner->GetProfileSettings();
	if (ProfileSettings != nullptr)
	{
		ProfileSettings->ResetProfile(EProfileResetType::Input);
		ProfileSettings->ResetProfile(EProfileResetType::Binds);
		PlayerOwner->SaveProfileSettings();

		// Close then reopen
		GetPlayerOwner()->CloseDialog(SharedThis(this));
		PlayerOwner->OpenDialog(SNew(SUTControlSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUTMenuBase","Controls","Control Settings")));
	}
	return FReply::Handled();
}

FReply SUTControlSettingsDialog::OKClick()
{

	// Look at all of the control settings and make sure they are all set.

	// Copy the binds back in to the profile
	for (int32 i=0 ; i < BindList.Num(); i++)
	{
		if (BindList[i]->PrimaryKeyBindWidget->GetKey() == EKeys::Invalid &&
			BindList[i]->SecondaryKeyBindWidget->GetKey() == EKeys::Invalid)
		{

			if (ScrollBox.IsValid())
			{
				ScrollBox->ScrollDescendantIntoView(BindList[i]->PrimaryKeyBindWidget, false, SScrollBox::EDescendantScrollDestination::IntoView);
			}

			GetPlayerOwner()->ShowMessage
					(
						NSLOCTEXT("SUTControlSettingsDialog", "EmptyBindingTitle", "Missing Key"),
						FText::Format(NSLOCTEXT("SUTControlSettingsDialog", "EmptyBindingMessage", "The action '{0}' doesn't have a key bound to it.  Please set a key for it before saving."), BindList[i]->KeyConfig->MenuText),
						UTDIALOG_BUTTON_OK, nullptr
					);

			return FReply::Handled();

		}
	}


	// Copy the action settings back in to the profile;

	UUTProfileSettings* ProfileSettings = PlayerOwner->GetProfileSettings();
	if (ProfileSettings != nullptr)
	{
		// Copy the binds back in to the profile
		for (int32 i=0 ; i < BindList.Num(); i++)
		{
			FKeyConfigurationInfo* GameAction = BindList[i]->KeyConfig;
			GameAction->PrimaryKey = BindList[i]->PrimaryKeyBindWidget->GetKey();
			GameAction->SecondaryKey = BindList[i]->SecondaryKeyBindWidget->GetKey();
		}

		ProfileSettings->MouseSensitivity = MouseSensitivity->GetValue() * (MouseSensitivityRange.Y - MouseSensitivityRange.X) + MouseSensitivityRange.X;
		ProfileSettings->MouseAcceleration = MouseAcceleration->GetValue() * (MouseAccelerationRange.Y - MouseAccelerationRange.X) + MouseAccelerationRange.X;
		ProfileSettings->MouseAccelerationMax = MouseAccelerationMax->GetValue() * (MouseAccelerationMaxRange.Y - MouseAccelerationMaxRange.X) + MouseAccelerationMaxRange.X;
		if (MouseAccelerationCheckBox->IsChecked())
		{
			float MAP = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->AccelerationPower;
			ProfileSettings->MouseAccelerationPower = MAP <= 0 ? 1.0f : MAP;
		}
		else
		{
			ProfileSettings->MouseAccelerationPower = 0.0f;
		}

		ProfileSettings->bInvertMouse = MouseInvert->IsChecked();
		ProfileSettings->bEnableMouseSmoothing = MouseSmoothing->IsChecked();

		ProfileSettings->bAllowSlideFromRun = SlideFromRun->IsChecked();
		ProfileSettings->bEnableDoubleTapDodge = EnableDoubleTapDodge->IsChecked();
		ProfileSettings->MaxDodgeClickTimeValue = MaxDodgeClickTimeValue;
		ProfileSettings->MaxDodgeTapTimeValue = MaxDodgeTapTimeValue;

		ProfileSettings->bSingleTapWallDodge = SingleTapWallDodge->IsChecked();
		ProfileSettings->bSingleTapAfterJump = SingleTapAfterJump->IsChecked();

		ProfileSettings->ApplyInputSettings(PlayerOwner.Get());
	}


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

void SUTControlSettingsDialog::OnKeyBindingChanged( FKey PreviousKey, FKey NewKey, TSharedPtr<FKeyBindTracker> BindingThatChanged, bool bPrimaryKey ) 
{
	// Validate the binding

	for (int32 i = 0; i < BindList.Num(); i++)
	{
		if (BindList[i]->KeyConfig != BindingThatChanged->KeyConfig)
		{
			if (BindList[i]->PrimaryKeyBindWidget->GetKey() == NewKey || BindList[i]->SecondaryKeyBindWidget->GetKey() == NewKey)
			{
				TSharedPtr<FKeyBindTracker> NewBindInContention = BindingThatChanged;
				TSharedPtr<FKeyBindTracker> OldBindInContention = BindList[i];

				bool bOldPrimary = BindList[i]->PrimaryKeyBindWidget->GetKey() == NewKey;
				auto OnDuplicateDialogResult = [bPrimaryKey, NewKey, PreviousKey, NewBindInContention, OldBindInContention, bOldPrimary](TSharedPtr<SCompoundWidget> Widget, uint16 Button)
				{
					if (Button == UTDIALOG_BUTTON_NO)
					{
						if (bPrimaryKey)
						{
							NewBindInContention->PrimaryKeyBindWidget->SetKey(PreviousKey, true, false);
						}
						else
						{
							NewBindInContention->SecondaryKeyBindWidget->SetKey(PreviousKey, true, false);
						}

					}
					else if (Button == UTDIALOG_BUTTON_YES)
					{
						if (bOldPrimary)
						{
							OldBindInContention->PrimaryKeyBindWidget->SetKey(FKey(), true, false);
						}
						else
						{
							OldBindInContention->SecondaryKeyBindWidget->SetKey(FKey(), true, false);
						}
					}
				};

				GetPlayerOwner()->ShowMessage
						(
							NSLOCTEXT("SUTControlSettingsDialog", "DuplicateBindingTitle", "Duplicate Binding"),
							FText::Format(NSLOCTEXT("SUTControlSettingsDialog", "DuplicateBindingMessage", "The key '{0}' is already bound to \"{1}\".\n\nDo you want to Bind '{0}' to \"{2}\" and remove the key from \"{1}\"?"), NewKey.GetDisplayName(), BindList[i]->KeyConfig->MenuText, BindingThatChanged->KeyConfig->MenuText),
							UTDIALOG_BUTTON_YES | UTDIALOG_BUTTON_NO,
							FDialogResultDelegate::CreateLambda(OnDuplicateDialogResult)
						);
			}
		}
	}

	if (BindingThatChanged->PrimaryKeyBindWidget->GetKey() == BindingThatChanged->SecondaryKeyBindWidget->GetKey())
	{
		BindingThatChanged->SecondaryKeyBindWidget->SetKey(FKey(), true, false);
	}

}

#endif