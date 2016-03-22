// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "UTProfileSettings.h"
#include "SUTHUDSettingsDialog.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"
#include "../SUTUtils.h"

#if !UE_SERVER

const FName NAME_QuickStatsAngle			= FName(TEXT("QuickStatsAngle"));
const FName NAME_QuickStatsDistance			= FName(TEXT("QuickStatsDistance"));
const FName NAME_QuickStatsType				= FName(TEXT("QuickStatsType"));
const FName NAME_QuickStatsBackgroundAlpha	= FName(TEXT("QuickStatsBackgroundAlpha"));
const FName NAME_QuickStatsForegroundAlpha	= FName(TEXT("QuickStatsForegroundAlpha"));
const FName NAME_bQuickStatsHidden			= FName(TEXT("bQuickStatsHidden"));
const FName NAME_bQuickStatsBob				= FName(TEXT("bQuickStatsBob"));
			
const FName NAME_HUDWidgetOpacity						= FName(TEXT("HUDWidgetOpacity"));
const FName NAME_HUDWidgetBorderOpacity					= FName(TEXT("HUDWidgetBorderOpacity"));
const FName NAME_HUDWidgetSlateOpacity					= FName(TEXT("HUDWidgetSlateOpacity"));
const FName NAME_HUDWidgetWeaponbarInactiveOpacity		= FName(TEXT("HUDWidgetWeaponbarInactiveOpacity"));
const FName NAME_HUDWidgetWeaponBarScaleOverride		= FName(TEXT("HUDWidgetWeaponBarScaleOverride"));
const FName NAME_HUDWidgetWeaponBarInactiveIconOpacity	= FName(TEXT("HUDWidgetWeaponBarInactiveIconOpacity"));
const FName NAME_HUDWidgetWeaponBarEmptyOpacity			= FName(TEXT("HUDWidgetWeaponBarEmptyOpacity"));
const FName NAME_HUDWidgetScaleOverride					= FName(TEXT("HUDWidgetScaleOverride"));
const FName NAME_HUDMessageScaleOverride				= FName(TEXT("HUDMessageScaleOverride"));
const FName NAME_bUseWeaponColors						= FName(TEXT("bUseWeaponColors"));
const FName NAME_bDrawChatKillMsg						= FName(TEXT("bDrawChatKillMsg"));
const FName NAME_bDrawCenteredKillMsg					= FName(TEXT("bDrawCenteredKillMsg"));
const FName NAME_bDrawHUDKillIconMsg					= FName(TEXT("bDrawHUDKillIconMsg"));
const FName NAME_bPlayKillSoundMsg						= FName(TEXT("bPlayKillSoundMsg"));
const FName NAME_bDrawCTFMinimapHUDSetting				= FName(TEXT("bDrawCTFMinimapHUDSetting"));

void SUTHUDSettingsDialog::Construct(const FArguments& InArgs)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(InArgs._PlayerOwner->PlayerController);

	ProfileSettings = InArgs._PlayerOwner->GetProfileSettings();

	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(UTDIALOG_BUTTON_APPLY + UTDIALOG_BUTTON_OK + UTDIALOG_BUTTON_CANCEL)
							.IsScrollable(false)
							.bShadow(!bInGame) //Don't shadow if ingame
							.OnDialogResult(InArgs._OnDialogResult)
						);

	SettingsInfos.Empty();

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	if (DialogContent.IsValid())
	{
		if (ProfileSettings.IsValid() )
		{
			DialogContent->AddSlot()
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()  // Tab Bar
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(55)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
						]
						+SOverlay::Slot()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(FMargin(25.0f, 0.0f, 0.0f, 0.0f))
							.AutoWidth()
							[
								SAssignNew(GeneralSettingsTabButton, SUTButton)
								.IsToggleButton(true)
								.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
								.Text(NSLOCTEXT("SUTHUDSettingsDialog", "ControlTabGeneral", "General"))
								.OnClicked(this, &SUTHUDSettingsDialog::OnTabClickGeneral)
							]


							+ SHorizontalBox::Slot()
							.Padding(FMargin(25.0f, 0.0f, 0.0f, 0.0f))
							.AutoWidth()
							[
								SAssignNew(WeaponBarSettingsTabButton, SUTButton)
								.IsToggleButton(true)
								.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
								.Text(NSLOCTEXT("SUTHUDSettingsDialog", "ControlTabWeaponBar", "Weapon Bar"))
								.OnClicked(this, &SUTHUDSettingsDialog::OnTabClickWeaponBar)
							]

							+ SHorizontalBox::Slot()
							.Padding(FMargin(25.0f, 0.0f, 0.0f, 0.0f))
							.AutoWidth()
							[
								SAssignNew(NotificationsSettingsTabButton, SUTButton)
								.IsToggleButton(true)
								.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
								.Text(NSLOCTEXT("SUTHUDSettingsDialog", "ControlNotifications", "Notifications"))
								.OnClicked(this, &SUTHUDSettingsDialog::OnTabClickNotifications)
							]

							+ SHorizontalBox::Slot()
							.Padding(FMargin(25.0f, 0.0f, 0.0f, 0.0f))
							.AutoWidth()
							[
								SAssignNew(QuickStatsSettingsTabButton, SUTButton)
								.IsToggleButton(true)
								.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
								.Text(NSLOCTEXT("SUTHUDSettingsDialog", "ControlQuickStats", "Quick Stats"))
								.OnClicked(this, &SUTHUDSettingsDialog::OnTabClickQuickStats)
							]

						]
					]
				]

				// Content

				+SVerticalBox::Slot()
				.FillHeight(1.0)
				.Padding(5.0f, 5.0f, 5.0f, 5.0f)
				[
					// Settings Tabs
					SAssignNew(TabWidget, SWidgetSwitcher)
				]

				+ SVerticalBox::Slot()
				.Padding(20.0f, 50.0f, 20.0f, 0.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
					.Text(bInGame ? FText::GetEmpty() : NSLOCTEXT("SUTHUDSettingsDialog", "MainMenuInstruction", "If you modify these options while in game, you will instantly see the effects of your changes."))
				]
			];
		}
		else
		{
			DialogContent->AddSlot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
					.Text(NSLOCTEXT("SUTHUDSettingsDialog","NoProfile","Currently, you must be logged in with an active profile to adjust your HUD.  It will change but #PreAlpha!"))
				]
			];
		}
	}

	MakeTabs();

	if (GeneralSettingsTabButton.IsValid()) GeneralSettingsTabButton->BePressed();
}

void SUTHUDSettingsDialog::MakeTabs()
{
	if (TabWidget.IsValid())
	{
		// General Settings
		TabWidget->AddSlot()
		[
			BuildGeneralTab()
		];

		// Weapon Bar Settings
		TabWidget->AddSlot()
		[
			BuildWeaponBarTab()
		];

		// Notification Settings
		TabWidget->AddSlot()
		[
			BuildNotificationsTab()
		];

		TabWidget->AddSlot()
		[
			BuildQuickStatsTab()
		];
	}
}


TSharedRef<class SWidget> SUTHUDSettingsDialog::BuildCustomButtonBar()
{
	return SNew(SButton)
		.HAlign(HAlign_Center)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
		.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
		.Text(NSLOCTEXT("SUTControlSettingsDialog", "BindDefault", "RESET TO DEFAULTS"))
		.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
		.OnClicked(this, &SUTHUDSettingsDialog::ResetClick);
}


SVerticalBox::FSlot& SUTHUDSettingsDialog::AddFloatOption(FName OptionTag, FText CaptionText, FText ToolTip, FText Prefix, float InitialValue, float Min, float Max)
{
	// Create the entry..

	TSharedPtr<SHUDSettingInfo> Info = MakeShareable(new SHUDSettingInfo(InitialValue, Prefix, Min, Max));
	SettingsInfos.Add(OptionTag, Info);
	TSharedPtr<STextBlock> Caption;
	SAssignNew(Caption, STextBlock)
		.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
		.Text(CaptionText)
		.ToolTip(SUTUtils::CreateTooltip(ToolTip));

	TSharedPtr<SEditableTextBox> EditBox;
	SAssignNew(EditBox, SEditableTextBox)
		.Style(SUTStyle::Get(),"UT.EditBox.Boxed")
		.ForegroundColor(FLinearColor::Black)
		.MinDesiredWidth(100.0f)
		.Text(Info.Get(), &SHUDSettingInfo::GetText_float)
		.OnTextCommitted(Info.Get(), &SHUDSettingInfo::TextCommited_float);


	TSharedPtr<SSlider> Slider;
	SAssignNew(Slider, SSlider)
		.Style(SUTStyle::Get(), "UT.Slider")
		.Value(Info.Get(), &SHUDSettingInfo::GetSliderValue_float)
		.OnValueChanged(Info.Get(), &SHUDSettingInfo::FloatValueChanged);

	// Build the slot..

	Info->EditBox = EditBox;
	Info->Slider = Slider;
	Info->CheckBox.Reset();

	Info->Set_float(InitialValue);

	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(40.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				Caption.ToSharedRef()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(140)
				[
					EditBox.ToSharedRef()
				]
			]
			+SHorizontalBox::Slot()
			[
				Slider.ToSharedRef()
			]
		];
}
SVerticalBox::FSlot& SUTHUDSettingsDialog::AddIntOption(FName OptionTag, FText CaptionText, FText ToolTip, FText Prefix, int InitialValue, int32 Min, int32 Max)
{
	// Create the entry..

	TSharedPtr<SHUDSettingInfo> Info = MakeShareable(new SHUDSettingInfo(InitialValue, Prefix, Min, Max));
	SettingsInfos.Add(OptionTag, Info);

	TSharedPtr<STextBlock> Caption;
	SAssignNew(Caption, STextBlock)
		.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
		.Text(CaptionText)
		.ToolTip(SUTUtils::CreateTooltip(ToolTip));

	TSharedPtr<SEditableTextBox> EditBox;
	SAssignNew(EditBox, SEditableTextBox)
		.Style(SUTStyle::Get(),"UT.EditBox.Boxed")
		.ForegroundColor(FLinearColor::Black)
		.MinDesiredWidth(100.0f)
		.Text(Info.Get(), &SHUDSettingInfo::GetText_int32)
		.OnTextCommitted(Info.Get(), &SHUDSettingInfo::TextCommited_int32);


	TSharedPtr<SSlider> Slider;
	SAssignNew(Slider, SSlider)
		.Style(SUTStyle::Get(), "UT.Slider")
		.Value(Info.Get(), &SHUDSettingInfo::GetSliderValue_int32)
		.OnValueChanged(Info.Get(), &SHUDSettingInfo::Int32ValueChanged);

	// Build the slot..

	Info->EditBox = EditBox;
	Info->Slider = Slider;
	Info->CheckBox.Reset();

	Info->Set_int32(InitialValue);

	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(40.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				Caption.ToSharedRef()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(140)
				[
					EditBox.ToSharedRef()
				]
			]
			+SHorizontalBox::Slot()
			[
				Slider.ToSharedRef()
			]
		];

}

SVerticalBox::FSlot& SUTHUDSettingsDialog::AddBoolOption(FName OptionTag, FText CaptionText, FText ToolTip, bool bInitialValue)
{
	// Create the entry..

	TSharedPtr<SHUDSettingInfo> Info = MakeShareable(new SHUDSettingInfo(bInitialValue));
	SettingsInfos.Add(OptionTag, Info);

	TSharedPtr<STextBlock> Caption;
	SAssignNew(Caption, STextBlock)
		.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
		.Text(CaptionText)
		.ToolTip(SUTUtils::CreateTooltip(ToolTip));

	TSharedPtr<SCheckBox> CheckBox;
	SAssignNew(CheckBox, SCheckBox)
		.Style(SUTStyle::Get(), "UT.CheckBox")
		.IsChecked(Info.Get(), &SHUDSettingInfo::GetCheckBoxState)
		.OnCheckStateChanged(Info.Get(), &SHUDSettingInfo::CheckboxChanged);

	// Build the slot..

	Info->EditBox.Reset();
	Info->Slider.Reset();
	Info->CheckBox = CheckBox;

	Info->CheckBox->SetIsChecked(bInitialValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(40.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				Caption.ToSharedRef()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(140)
				[
					CheckBox.ToSharedRef()
				]
			]
			+SHorizontalBox::Slot()
			[
				SNew(SCanvas)
			]
		];

}


TSharedRef<SWidget> SUTHUDSettingsDialog::BuildGeneralTab()
{
	return SNew(SVerticalBox)
		+AddIntOption(NAME_HUDWidgetOpacity, NSLOCTEXT("HUDSETTINGS","OpacityLabel","General Opacity:"),NSLOCTEXT("SUTHUDSettingsDialog", "HUDOpacityTT", "Adjusts how transparent the HUD should be."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->HUDWidgetOpacity * 100.0f), 0, 100)
		+AddIntOption(NAME_HUDWidgetBorderOpacity, NSLOCTEXT("HUDSETTINGS", "BorderOpacityLabel", "Border Opacity:"), NSLOCTEXT("SUTHUDSettingsDialog", "HUDBorderOpacityTT", "Adjusts how transparent the hard edge border around each element of the HUD should be."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->HUDWidgetOpacity * 100.0f), 0, 100)
		+AddIntOption(NAME_HUDWidgetSlateOpacity, NSLOCTEXT("HUDSETTINGS", "SlateOpacityLabel", "Slate Opacity:"), NSLOCTEXT("SUTHUDSettingsDialog", "HUDSlateOpacityTT", "Adjusts how transparent the background portion of each HUD element should be."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->HUDWidgetSlateOpacity * 100), 0, 100)
		+AddIntOption(NAME_HUDWidgetScaleOverride, NSLOCTEXT("HUDSETTINGS", "ScaleLabel", "Scale:"), NSLOCTEXT("SUTHUDSettingsDialog", "HUDScaleTT", "Makes the HUD elements bigger or smaller."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->HUDWidgetScaleOverride * 100.0f), 25, 300)

		// Spacer....
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(48)
		]

		+AddBoolOption(NAME_bDrawCTFMinimapHUDSetting, NSLOCTEXT("HUDSETTINGS", "CTFMinimap", "Show CTF Mini-map:"), NSLOCTEXT("SUTHUDSettingsDialog", "CTFMinimapTT", "Displays the mini-map in CTF."), ProfileSettings->bDrawCTFMinimapHUDSetting);
}

TSharedRef<SWidget> SUTHUDSettingsDialog::BuildWeaponBarTab()
{

	return SNew(SVerticalBox)
		+AddIntOption(NAME_HUDWidgetWeaponbarInactiveOpacity, NSLOCTEXT("HUDSETTINGS", "WeaponBarOpacityLabel", "General Opacity:"),NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponBarOpaictyTT", "Adjusts how transparent the Weapon Bar should be.  NOTE this is applied in addition to the normal transparency setting."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->HUDWidgetWeaponbarInactiveOpacity * 100.0f), 0, 100)
		+AddIntOption(NAME_HUDWidgetWeaponBarScaleOverride, NSLOCTEXT("HUDSETTINGS", "WeaponBarScaleLabel", "Scale:"), NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponBarScaleTT", "Adjusts how big or small the Weapon Bar should be."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), (ProfileSettings->HUDWidgetWeaponBarScaleOverride * 100.0f), 25, 300)
		+AddIntOption(NAME_HUDWidgetWeaponBarInactiveIconOpacity, NSLOCTEXT("HUDSETTINGS", "WeaponBarIconOpacityLabel", "Icon/Label Opacity:"), NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponBarIconOpacityTT", "Adjusts how transparent the icons on the Weapon Bar should be."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->HUDWidgetWeaponBarInactiveIconOpacity * 100.0f), 0, 100)
		+AddIntOption(NAME_HUDWidgetWeaponBarEmptyOpacity, NSLOCTEXT("HUDSETTINGS", "WeaponBarEmptyOpacityLabel", "Empty Slot Opacity:"), NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponBarEmptyOpacityTT", "Adjusts how transparent an empty Weapon Bar slot should be."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->HUDWidgetWeaponBarEmptyOpacity * 100.0f), 0, 100)

		// Spacer....
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(48)
		]

		+AddBoolOption(NAME_bUseWeaponColors, NSLOCTEXT("SUTHUDSettingsDialog", "UseWeaponColors", "Colorize All Icons"), NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponColorsTT", "Whether to colorize all weapon bar icons."), ProfileSettings->bUseWeaponColors);
}

TSharedRef<SWidget> SUTHUDSettingsDialog::BuildNotificationsTab()
{
	return SNew(SVerticalBox)
		+AddIntOption(NAME_HUDMessageScaleOverride, NSLOCTEXT("HUDSETTINGS", "HUDMessageScaleOverride", "Scale:"), NSLOCTEXT("SUTHUDSettingsDialog", "HUDMessageScaleOverrideTT", "Adjusts how big or small the messages appear."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), (ProfileSettings->HUDMessageScaleOverride * 100.0f), 25, 300)

		// Spacer....
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(48)
		]

		+AddBoolOption(NAME_bDrawChatKillMsg, NSLOCTEXT("SUTHUDSettingsDialog", "ChatKillMessages", "Show Kill Messages In Chat"), NSLOCTEXT("SUTHUDSettingsDialog", "bDrawChatKillMsgTT", "Show kill messages in the chat window during the game."), ProfileSettings->bDrawChatKillMsg)
		+AddBoolOption(NAME_bDrawCenteredKillMsg, NSLOCTEXT("SUTHUDSettingsDialog", "DrawCenteredKillMsg", "Centered Kill Messages"), NSLOCTEXT("SUTHUDSettingsDialog", "bDrawCenteredKillMsgTT", "Show kill message in the center of the screen."), ProfileSettings->bDrawCenteredKillMsg)
		+AddBoolOption(NAME_bDrawHUDKillIconMsg, NSLOCTEXT("SUTHUDSettingsDialog", "DrawHUDKillIconMsg", "Display Skull On Kills"), NSLOCTEXT("SUTHUDSettingsDialog", "bDrawHUDKillIconMsgTT", "Use icons in the kill messages."), ProfileSettings->bDrawHUDKillIconMsg)
		+AddBoolOption(NAME_bPlayKillSoundMsg, NSLOCTEXT("SUTHUDSettingsDialog", "PlayKillSoundMsg", "Play Kill Sound Effect"), NSLOCTEXT("SUTHUDSettingsDialog", "bPlayKillSoundMsgTT", "Play a sound effect when a player dies."), ProfileSettings->bPlayKillSoundMsg);
}

TSharedRef<SWidget> SUTHUDSettingsDialog::BuildQuickStatsTab()
{

	QuickStatTypesList.Empty();
	QuickStatTypesList.Add( MakeShareable(new FString(TEXT("Arc"))));
	QuickStatTypesList.Add( MakeShareable(new FString(TEXT("Bunched"))));

	int32 CurrentQuickStatType = ProfileSettings.IsValid() ? (ProfileSettings->QuickStatsType == FName(TEXT("Bunch")) ? 1 : 0) : 0;

	return SNew(SVerticalBox)

		+AddBoolOption(NAME_bQuickStatsHidden, NSLOCTEXT("SUTHUDSettingsDialog", "bQuickStatsHidden", "Hide Quick Stats"), NSLOCTEXT("SUTHUDSettingsDialog", "bQuickStatsHiddenTT", "Check this box if you wish to hide the quick stats indicator all together."), ProfileSettings->bQuickStatsHidden)
		
		// The Layout
		+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			.Padding(FMargin(40.0f, 15.0f, 10.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(STextBlock)
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
						.Text(NSLOCTEXT("SUTHUDSettingsDialog","LayoutCaption","Layout:"))
						.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog","LayoutTT","Change the layout style of the quick stats indicator.")))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(140)
					[
						SNew(SComboBox< TSharedPtr<FString> >)
						.InitiallySelectedItem(QuickStatTypesList[CurrentQuickStatType])
						.ComboBoxStyle(SUTStyle::Get(), "UT.ComboBox")
						.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.Bright")
						.OptionsSource(&QuickStatTypesList)
						.OnGenerateWidget(this, &SUTDialogBase::GenerateStringListWidget)
						.OnSelectionChanged(this, &SUTHUDSettingsDialog::OnLayoutChanged)
						.Content()
						[
							SAssignNew(SelectedLayout, STextBlock)
							.Text(FText::FromString(*QuickStatTypesList[CurrentQuickStatType].Get()))
							.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
							.ColorAndOpacity(FLinearColor::Black)
						]
					]
				]
				+SHorizontalBox::Slot()
				[
					SNew(SCanvas)
				]
			]

		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(48)
		]

		+AddFloatOption(NAME_QuickStatsAngle, NSLOCTEXT("HUDSETTINGS", "QuickStatsAngle", "Angle:"),NSLOCTEXT("SUTHUDSettingsDialog", "QuickStatsAngleTT", "Changes where around the crosshair will the quick stats be displayed."), NSLOCTEXT("SUTHUDSettingsDialog", "Angle", "deg"), ProfileSettings->QuickStatsAngle, 0.0f, 360.0f)
		+AddFloatOption(NAME_QuickStatsDistance, NSLOCTEXT("HUDSETTINGS", "QuickStatsDistance", "Distance:"), NSLOCTEXT("SUTHUDSettingsDialog", "QuickStatsDistanceTT", "Changes how far from the crosshair your quick stats menu should be."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), ProfileSettings->QuickStatsDistance, 0.0f, 0.75f)
		+AddIntOption(NAME_QuickStatsForegroundAlpha, NSLOCTEXT("HUDSETTINGS", "QuickStatsForegroundAlpha", "Icon & Text Opacity:"), NSLOCTEXT("SUTHUDSettingsDialog", "QuickStatsForegroundAlphaTT", "Adjusts the opacity of the icon and text for each stat."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->QuickStatsForegroundAlpha * 100.0f), 0, 100)
		+AddIntOption(NAME_QuickStatsBackgroundAlpha, NSLOCTEXT("HUDSETTINGS", "QuickStatsBackgroundAlpha", "Background Opacity:"), NSLOCTEXT("SUTHUDSettingsDialog", "QuickStatsBackgroundAlphaTT", "Adjusts the opacity of the background."), NSLOCTEXT("SUTHUDSettingsDialog", "Percent", "%"), int32(ProfileSettings->QuickStatsBackgroundAlpha * 100.0f), 0, 100)
		+AddBoolOption(NAME_bQuickStatsBob, NSLOCTEXT("SUTHUDSettingsDialog", "bQuickStatsBob", "Bob With Weapon"), NSLOCTEXT("SUTHUDSettingsDialog", "bQuickStatsBobTT", "If selected, the on-screen inditators will animate and follow the weapon's bob."), ProfileSettings->bQuickStatsBob);
}


FReply SUTHUDSettingsDialog::OnTabClickGeneral()
{
	TabWidget->SetActiveWidgetIndex(0);

	GeneralSettingsTabButton->BePressed();
	WeaponBarSettingsTabButton->UnPressed();
	NotificationsSettingsTabButton->UnPressed();
	QuickStatsSettingsTabButton->UnPressed();

	return FReply::Handled();
}

FReply SUTHUDSettingsDialog::OnTabClickWeaponBar()
{
	TabWidget->SetActiveWidgetIndex(1);

	GeneralSettingsTabButton->UnPressed();
	WeaponBarSettingsTabButton->BePressed();
	NotificationsSettingsTabButton->UnPressed();
	QuickStatsSettingsTabButton->UnPressed();

	return FReply::Handled();
}

FReply SUTHUDSettingsDialog::OnTabClickNotifications()
{
	TabWidget->SetActiveWidgetIndex(2);

	GeneralSettingsTabButton->UnPressed();
	WeaponBarSettingsTabButton->UnPressed();
	NotificationsSettingsTabButton->BePressed();
	QuickStatsSettingsTabButton->UnPressed();

	return FReply::Handled();
}

FReply SUTHUDSettingsDialog::OnTabClickQuickStats()
{
	TabWidget->SetActiveWidgetIndex(3);

	GeneralSettingsTabButton->UnPressed();
	WeaponBarSettingsTabButton->UnPressed();
	NotificationsSettingsTabButton->UnPressed();
	QuickStatsSettingsTabButton->BePressed();

	return FReply::Handled();
}


void SUTHUDSettingsDialog::ApplySettings()
{
	if (ProfileSettings.IsValid())
	{
		ProfileSettings->HUDWidgetOpacity = float(SettingsInfos[NAME_HUDWidgetOpacity]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->HUDWidgetBorderOpacity = float(SettingsInfos[NAME_HUDWidgetBorderOpacity]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->HUDWidgetSlateOpacity = float(SettingsInfos[NAME_HUDWidgetSlateOpacity]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->HUDWidgetWeaponbarInactiveOpacity = float(SettingsInfos[NAME_HUDWidgetWeaponbarInactiveOpacity]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->HUDWidgetWeaponBarScaleOverride = float(SettingsInfos[NAME_HUDWidgetWeaponBarScaleOverride]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->HUDWidgetWeaponBarInactiveIconOpacity = float(SettingsInfos[NAME_HUDWidgetWeaponBarInactiveIconOpacity]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->HUDWidgetWeaponBarEmptyOpacity = float(SettingsInfos[NAME_HUDWidgetWeaponBarEmptyOpacity]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->HUDWidgetScaleOverride = float(SettingsInfos[NAME_HUDWidgetScaleOverride]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->HUDMessageScaleOverride = float(SettingsInfos[NAME_HUDMessageScaleOverride]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->bUseWeaponColors = SettingsInfos[NAME_bUseWeaponColors]->GetActualValue_bool();
		ProfileSettings->bDrawChatKillMsg = SettingsInfos[NAME_bDrawChatKillMsg]->GetActualValue_bool();
		ProfileSettings->bDrawCenteredKillMsg = SettingsInfos[NAME_bDrawCenteredKillMsg]->GetActualValue_bool();
		ProfileSettings->bDrawHUDKillIconMsg = SettingsInfos[NAME_bDrawHUDKillIconMsg]->GetActualValue_bool();
		ProfileSettings->bPlayKillSoundMsg = SettingsInfos[NAME_bPlayKillSoundMsg]->GetActualValue_bool();
		ProfileSettings->bDrawCTFMinimapHUDSetting = SettingsInfos[NAME_bDrawCTFMinimapHUDSetting]->GetActualValue_bool();

		ProfileSettings->QuickStatsAngle = SettingsInfos[NAME_QuickStatsAngle]->GetActualValue_float();
		ProfileSettings->QuickStatsDistance = SettingsInfos[NAME_QuickStatsDistance]->GetActualValue_float();
		ProfileSettings->QuickStatsBackgroundAlpha = float(SettingsInfos[NAME_QuickStatsBackgroundAlpha]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->QuickStatsForegroundAlpha = float(SettingsInfos[NAME_QuickStatsForegroundAlpha]->GetActualValue_int32()) / 100.0f;
		ProfileSettings->bQuickStatsHidden = SettingsInfos[NAME_bQuickStatsHidden]->GetActualValue_bool();
		ProfileSettings->bQuickStatsBob = SettingsInfos[NAME_bQuickStatsBob]->GetActualValue_bool();

		ProfileSettings->QuickStatsType = SelectedLayout->GetText().ToString().Equals(TEXT("Arc"),ESearchCase::IgnoreCase) ? EQuickStatsLayouts::Arc : EQuickStatsLayouts::Bunch;
		PlayerOwner->SaveProfileSettings();
	}
}

FReply SUTHUDSettingsDialog::ResetClick()
{
	if (ProfileSettings.IsValid())
	{
		ProfileSettings->ResetHUD();
		PlayerOwner->SaveProfileSettings();
		GetPlayerOwner()->HideHUDSettings();
	}
	return FReply::Handled();

}

FReply SUTHUDSettingsDialog::OKClick()
{
	ApplySettings();
	GetPlayerOwner()->HideHUDSettings();
	return FReply::Handled();
}

FReply SUTHUDSettingsDialog::CancelClick()
{
	GetPlayerOwner()->HideHUDSettings();
	return FReply::Handled();
}


FReply SUTHUDSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	else if (ButtonID == UTDIALOG_BUTTON_APPLY) ApplySettings();
	return FReply::Handled();
}


void SUTHUDSettingsDialog::OnLayoutChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedLayout->SetText(*NewSelection.Get());
}

#endif