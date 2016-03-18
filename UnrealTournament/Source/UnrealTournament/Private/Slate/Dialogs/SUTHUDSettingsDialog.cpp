// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTHUDSettingsDialog.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"
#include "../SUTUtils.h"

#if !UE_SERVER


void SUTHUDSettingsDialog::Construct(const FArguments& InArgs)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(InArgs._PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		TargetHUD = PC->MyUTHUD;
	}
	bInGame = TargetHUD.IsValid();

	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.IsScrollable(false)
							.bShadow(!bInGame) //Don't shadow if ingame
							.OnDialogResult(InArgs._OnDialogResult)
						);

	//If main menu and no HUD, use the default
	if (!TargetHUD.IsValid())
	{
		// We need to create a hud..

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Owner = PlayerOwner->PlayerController;
		SpawnInfo.ObjectFlags |= RF_Transient;	
		TargetHUD = PlayerOwner->GetWorld()->SpawnActor<AUTHUD>(AUTHUD::StaticClass(), SpawnInfo );
	}

	//Since TargetHUD might be the CDO, Save off all the property data if we need to revert
	Old_HUDWidgetOpacity = TargetHUD->HUDWidgetOpacity;
	Old_HUDWidgetBorderOpacity = TargetHUD->HUDWidgetBorderOpacity;
	Old_HUDWidgetSlateOpacity = TargetHUD->HUDWidgetSlateOpacity;
	Old_HUDWidgetWeaponbarInactiveOpacity = TargetHUD->HUDWidgetWeaponbarInactiveOpacity;
	Old_HUDWidgetWeaponBarScaleOverride = TargetHUD->HUDWidgetWeaponBarScaleOverride;
	Old_HUDWidgetWeaponBarInactiveIconOpacity = TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity;
	Old_HUDWidgetWeaponBarEmptyOpacity = TargetHUD->HUDWidgetWeaponBarEmptyOpacity;
	Old_HUDWidgetScaleOverride = TargetHUD->HUDWidgetScaleOverride;
	Old_bDrawCTFMinimap = TargetHUD->bDrawCTFMinimapHUDSetting;
	Old_bUseWeaponColors = TargetHUD->bUseWeaponColors;
	Old_bDrawChatKillMsg = TargetHUD->bDrawChatKillMsg;
	Old_bDrawCenteredKillMsg = TargetHUD->bDrawCenteredKillMsg;
	Old_bDrawHUDKillIconMsg = TargetHUD->bDrawHUDKillIconMsg;
	Old_bPlayKillSoundMsg = TargetHUD->bPlayKillSoundMsg;

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	if (DialogContent.IsValid())
	{
		if (TargetHUD.IsValid() )
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

					// General Settings
					+ SWidgetSwitcher::Slot()
					[
						BuildGeneralTab()
					]

					// Weapon Bar Settings
					+ SWidgetSwitcher::Slot()
					[
						BuildWeaponBarTab()
					]

					// Notification Settings
					+ SWidgetSwitcher::Slot()
					[
						BuildNotificationsTab()
					]
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
					.Text(NSLOCTEXT("SUTHUDSettingsDialog","NeedsHUD","No HUD available!  Please use in game."))
				]
			];
		}
	}

	if (GeneralSettingsTabButton.IsValid()) GeneralSettingsTabButton->BePressed();
}

TSharedRef<SWidget> SUTHUDSettingsDialog::BuildGeneralTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SAssignNew(HUDOpacityLabel, STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(this, &SUTHUDSettingsDialog::GetHUDOpacityLabel)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDOpacityTT", "Adjusts how transparent the HUD should be.")))
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				.Content()
				[
					SAssignNew(HUDOpacity, SSlider)
					.Style(SUTStyle::Get(), "UT.Slider")
					.Orientation(Orient_Horizontal)
					.Value(TargetHUD->HUDWidgetOpacity)
					.OnValueChanged(this, &SUTHUDSettingsDialog::OnHUDOpacityChanged)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SAssignNew(HUDOpacityLabel, STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(this, &SUTHUDSettingsDialog::GetHUDBorderOpacityLabel)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDBorderOpacityTT", "Adjusts how transparent the hard edge border around each element of the HUD should be.")))
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				.Content()
				[
					SAssignNew(HUDOpacity, SSlider)
					.Style(SUTStyle::Get(), "UT.Slider")
					.Orientation(Orient_Horizontal)
					.Value(TargetHUD->HUDWidgetBorderOpacity)
					.OnValueChanged(this, &SUTHUDSettingsDialog::OnHUDBorderOpacityChanged)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SAssignNew(HUDOpacityLabel, STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(this, &SUTHUDSettingsDialog::GetHUDSlateOpacityLabel)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDSlateOpacityTT", "Adjusts how transparent the background portion of each HUD element should be.")))
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				.Content()
				[
					SAssignNew(HUDOpacity, SSlider)
					.Style(SUTStyle::Get(), "UT.Slider")
					.Orientation(Orient_Horizontal)
					.Value(TargetHUD->HUDWidgetSlateOpacity)
					.OnValueChanged(this, &SUTHUDSettingsDialog::OnHUDSlateOpacityChanged)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SAssignNew(HUDOpacityLabel, STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(this, &SUTHUDSettingsDialog::GetHUDScaleLabel)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDScaleTT", "Makes the HUD elements bigger or smaller.")))
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				.Content()
				[
					SAssignNew(HUDOpacity, SSlider)
					.Style(SUTStyle::Get(), "UT.Slider")
					.Orientation(Orient_Horizontal)
					.Value(TargetHUD->HUDWidgetScaleOverride / 2.0f)
					.OnValueChanged(this, &SUTHUDSettingsDialog::OnHUDScaleChanged)
				]
			]
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			 SAssignNew(UseWeaponColor, SCheckBox)
			.ForegroundColor(FLinearColor::White)
			.IsChecked(TargetHUD->bDrawCTFMinimapHUDSetting ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged(this, &SUTHUDSettingsDialog::OnDisplayCTFMinimapChanged)
			.Style(SUTStyle::Get(), "UT.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(NSLOCTEXT("SUTHUDSettingsDialog", "DisplayCTFMinimapChanged", "Display Minimap in CTF"))
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "DisplayCTFMinimapChangedTT", "Whether to display a minimap in capture the flag game modes.")))
			]
		];
}

TSharedRef<SWidget> SUTHUDSettingsDialog::BuildWeaponBarTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SAssignNew(HUDOpacityLabel, STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(this, &SUTHUDSettingsDialog::GetWeaponBarOpacityLabel)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponBarOpaictyTT", "Adjusts how transparent the Weapon Bar should be.  NOTE this is applied in addition to the normal transparency setting.")))
			]
			+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
					.WidthOverride(300.0f)
					.Content()
					[
						SAssignNew(HUDOpacity, SSlider)
						.Orientation(Orient_Horizontal)
						.Style(SUTStyle::Get(), "UT.Slider")
						.Value(TargetHUD->HUDWidgetWeaponbarInactiveOpacity)
						.OnValueChanged(this, &SUTHUDSettingsDialog::OnWeaponBarOpacityChanged)
					]
				]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SAssignNew(HUDOpacityLabel, STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(this, &SUTHUDSettingsDialog::GetWeaponBarIconOpacityLabel)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponBarIconOpacityTT", "Adjusts how transparent the icons on the Weapon Bar should be.")))
			]
			+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
					.WidthOverride(300.0f)
					.Content()
					[
						SAssignNew(HUDOpacity, SSlider)
						.Orientation(Orient_Horizontal)
						.Style(SUTStyle::Get(), "UT.Slider")
						.Value(TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity)
						.OnValueChanged(this, &SUTHUDSettingsDialog::OnWeaponBarIconOpacityChanged)
					]
				]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SAssignNew(HUDOpacityLabel, STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(this, &SUTHUDSettingsDialog::GetWeaponBarEmptyOpacityLabel)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponBarEmptyOpacityTT", "Adjusts how transparent an empty Weapon Bar slot should be.")))
			]
			+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
					.WidthOverride(300.0f)
					.Content()
					[
						SAssignNew(HUDOpacity, SSlider)
						.Style(SUTStyle::Get(), "UT.Slider")
						.Orientation(Orient_Horizontal)
						.Value(TargetHUD->HUDWidgetWeaponBarEmptyOpacity)
						.OnValueChanged(this, &SUTHUDSettingsDialog::OnWeaponBarEmptyOpacityChanged)
					]
				]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SAssignNew(HUDOpacityLabel, STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(this, &SUTHUDSettingsDialog::GetWeaponBarScaleLabel)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponBarScaleTT", "Adjusts how big or small the Weapon Bar should be.")))
			]
			+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				.Content()
				[
					SAssignNew(HUDOpacity, SSlider)
					.Style(SUTStyle::Get(), "UT.Slider")
					.Orientation(Orient_Horizontal)
					.Value(TargetHUD->HUDWidgetWeaponBarScaleOverride / 2.0f)
					.OnValueChanged(this, &SUTHUDSettingsDialog::OnWeaponBarScaleChanged)
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SAssignNew(UseWeaponColor, SCheckBox)
			.ForegroundColor(FLinearColor::White)
			.IsChecked(TargetHUD->bUseWeaponColors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged(this, &SUTHUDSettingsDialog::OnUseWeaponColorChanged)
			.Style(SUTStyle::Get(), "UT.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.Text(NSLOCTEXT("SUTHUDSettingsDialog", "UseWeaponColors", "Colorize All Icons"))
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHUDSettingsDialog", "HUDWeaponColorsTT", "Whether to colorize all weapon bar icons.")))
			]
		];
}

TSharedRef<SWidget> SUTHUDSettingsDialog::BuildNotificationsTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(650.0f)
				[
					SNew(STextBlock)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
					.Text(NSLOCTEXT("SUTHUDSettingsDialog", "ChatKillMessages", "Show Kill Messages In Chat"))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SAssignNew(DrawChatKillMsg, SCheckBox)
				.ForegroundColor(FLinearColor::White)
				.IsChecked(TargetHUD->bDrawChatKillMsg ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SUTHUDSettingsDialog::OnDrawChatKillMsgMsgChanged)
				.Style(SUTStyle::Get(), "UT.CheckBox")
			]
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(650.0f)
				[
					SNew(STextBlock)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
					.Text(NSLOCTEXT("SUTHUDSettingsDialog", "DrawCenteredKillMsg", "Centered Kill Messages"))
				]
			]
			+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SAssignNew(DrawPopupKillMsg, SCheckBox)
					.ForegroundColor(FLinearColor::White)
					.IsChecked(TargetHUD->bDrawCenteredKillMsg ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SUTHUDSettingsDialog::OnDrawCenteredKillMsgChanged)
					.Style(SUTStyle::Get(), "UT.CheckBox")
				]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(650.0f)
				[
					SNew(STextBlock)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
					.Text(NSLOCTEXT("SUTHUDSettingsDialog", "DrawHUDKillIconMsg", "Display Skull On Kills"))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SAssignNew(DrawHUDKillIconMsg, SCheckBox)
				.ForegroundColor(FLinearColor::White)
				.IsChecked(TargetHUD->bDrawHUDKillIconMsg ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SUTHUDSettingsDialog::OnDrawHUDKillIconMsgChanged)
				.Style(SUTStyle::Get(), "UT.CheckBox")
			]
		]
		
		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(650.0f)
				[
					SNew(STextBlock)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
					.Text(NSLOCTEXT("SUTHUDSettingsDialog", "PlayKillSoundMsg", "Play Kill Sound Effect"))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SAssignNew(DrawPlayKillSoundMsg, SCheckBox)
				.ForegroundColor(FLinearColor::White)
				.IsChecked(TargetHUD->bPlayKillSoundMsg ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SUTHUDSettingsDialog::OnPlayKillSoundMsgChanged)
				.Style(SUTStyle::Get(), "UT.CheckBox")
			]
		];
}

FReply SUTHUDSettingsDialog::OnTabClickGeneral()
{
	TabWidget->SetActiveWidgetIndex(0);

	GeneralSettingsTabButton->BePressed();
	WeaponBarSettingsTabButton->UnPressed();
	NotificationsSettingsTabButton->UnPressed();

	return FReply::Handled();
}

FReply SUTHUDSettingsDialog::OnTabClickWeaponBar()
{
	TabWidget->SetActiveWidgetIndex(1);

	GeneralSettingsTabButton->UnPressed();
	WeaponBarSettingsTabButton->BePressed();
	NotificationsSettingsTabButton->UnPressed();

	return FReply::Handled();
}

FReply SUTHUDSettingsDialog::OnTabClickNotifications()
{
	TabWidget->SetActiveWidgetIndex(2);

	GeneralSettingsTabButton->UnPressed();
	WeaponBarSettingsTabButton->UnPressed();
	NotificationsSettingsTabButton->BePressed();

	return FReply::Handled();
}

FText SUTHUDSettingsDialog::GetHUDOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS","OpacityLabel","General Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetOpacity : 0.f));
}

FText SUTHUDSettingsDialog::GetHUDBorderOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "BorderOpacityLabel", "Border Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetBorderOpacity : 0.f));
}

FText SUTHUDSettingsDialog::GetHUDSlateOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "SlateOpacityLabel", "Slate Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetSlateOpacity : 0.f));
}

FText SUTHUDSettingsDialog::GetHUDScaleLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "ScaleLabel", "Scale ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetScaleOverride : 0.f));
}

FText SUTHUDSettingsDialog::GetWeaponBarOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarOpacityLabel", "Weapon Bar Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetWeaponbarInactiveOpacity : 0.f));
}

FText SUTHUDSettingsDialog::GetWeaponBarIconOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarIconOpacityLabel", "Weapon Bar Icon/Label Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity : 0.f));
}

FText SUTHUDSettingsDialog::GetWeaponBarEmptyOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarEmptyOpacityLabel", "Weapon Bar Empty Slot Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetWeaponBarEmptyOpacity : 0.f));
}


FText SUTHUDSettingsDialog::GetWeaponBarScaleLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarScaleLabel", "Weapon Bar Scale ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetWeaponBarScaleOverride : 0.f));
}


void SUTHUDSettingsDialog::OnHUDOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUTHUDSettingsDialog::OnHUDBorderOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetBorderOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUTHUDSettingsDialog::OnHUDSlateOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetSlateOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUTHUDSettingsDialog::OnHUDScaleChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetScaleOverride = 2.0 * float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUTHUDSettingsDialog::OnWeaponBarOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponbarInactiveOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
		TargetHUD->bHUDWeaponBarSettingChanged = true;
	}
}

void SUTHUDSettingsDialog::OnWeaponBarIconOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
		TargetHUD->bHUDWeaponBarSettingChanged = true;
	}
}

void SUTHUDSettingsDialog::OnWeaponBarEmptyOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponBarEmptyOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
		TargetHUD->bHUDWeaponBarSettingChanged = true;
	}
}

void SUTHUDSettingsDialog::OnWeaponBarScaleChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponBarScaleOverride = 2.0 * float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUTHUDSettingsDialog::OnDisplayCTFMinimapChanged(ECheckBoxState NewState)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->bDrawCTFMinimapHUDSetting = NewState == ECheckBoxState::Checked;
	}
}

void SUTHUDSettingsDialog::OnUseWeaponColorChanged(ECheckBoxState NewState)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->bUseWeaponColors = NewState == ECheckBoxState::Checked;
	}
}

void SUTHUDSettingsDialog::OnDrawCenteredKillMsgChanged(ECheckBoxState NewState)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->bDrawCenteredKillMsg = NewState == ECheckBoxState::Checked;
	}
}

void SUTHUDSettingsDialog::OnDrawChatKillMsgMsgChanged(ECheckBoxState NewState)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->bDrawChatKillMsg = NewState == ECheckBoxState::Checked;
	}
}

void SUTHUDSettingsDialog::OnDrawHUDKillIconMsgChanged(ECheckBoxState NewState)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->bDrawHUDKillIconMsg = (NewState == ECheckBoxState::Checked);
	}
}

void SUTHUDSettingsDialog::OnPlayKillSoundMsgChanged(ECheckBoxState NewState)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->bPlayKillSoundMsg = (NewState == ECheckBoxState::Checked);
	}
}

FReply SUTHUDSettingsDialog::OKClick()
{
	//Save the new values to all AUTHUD CDOs
	if (TargetHUD.IsValid())
	{
		for (TObjectIterator<AUTHUD> It(EObjectFlags::RF_NoFlags,true); It; ++It)
		{
			if (It->HasAnyFlags(RF_ClassDefaultObject))
			{
				AUTHUD* HudCDO = (*It);
				HudCDO->HUDWidgetOpacity = TargetHUD->HUDWidgetOpacity;
				HudCDO->HUDWidgetBorderOpacity = TargetHUD->HUDWidgetBorderOpacity;
				HudCDO->HUDWidgetSlateOpacity = TargetHUD->HUDWidgetSlateOpacity;
				HudCDO->HUDWidgetWeaponbarInactiveOpacity = TargetHUD->HUDWidgetWeaponbarInactiveOpacity;
				HudCDO->HUDWidgetWeaponBarScaleOverride = TargetHUD->HUDWidgetWeaponBarScaleOverride;
				HudCDO->HUDWidgetWeaponBarInactiveIconOpacity = TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity;
				HudCDO->HUDWidgetWeaponBarEmptyOpacity = TargetHUD->HUDWidgetWeaponBarEmptyOpacity;
				HudCDO->HUDWidgetScaleOverride = TargetHUD->HUDWidgetScaleOverride;
				HudCDO->bDrawCTFMinimapHUDSetting = TargetHUD->bDrawCTFMinimapHUDSetting;
				HudCDO->bUseWeaponColors = TargetHUD->bUseWeaponColors;
				HudCDO->bDrawChatKillMsg = TargetHUD->bDrawChatKillMsg;
				HudCDO->bDrawCenteredKillMsg = TargetHUD->bDrawCenteredKillMsg;
				HudCDO->bDrawHUDKillIconMsg = TargetHUD->bDrawHUDKillIconMsg;
				HudCDO->bPlayKillSoundMsg = TargetHUD->bPlayKillSoundMsg;

				if (HudCDO == AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>())
				{
					HudCDO->SaveConfig();
				}
			}
		}
	}

	TargetHUD.Reset();
	GetPlayerOwner()->HideHUDSettings();
	return FReply::Handled();
}

FReply SUTHUDSettingsDialog::CancelClick()
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetOpacity = Old_HUDWidgetOpacity;
		TargetHUD->HUDWidgetBorderOpacity = Old_HUDWidgetBorderOpacity;
		TargetHUD->HUDWidgetSlateOpacity = Old_HUDWidgetSlateOpacity;
		TargetHUD->HUDWidgetWeaponbarInactiveOpacity = Old_HUDWidgetWeaponbarInactiveOpacity;
		TargetHUD->HUDWidgetWeaponBarScaleOverride = Old_HUDWidgetWeaponBarScaleOverride;
		TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity = Old_HUDWidgetWeaponBarInactiveIconOpacity;
		TargetHUD->HUDWidgetWeaponBarEmptyOpacity = Old_HUDWidgetWeaponBarEmptyOpacity;
		TargetHUD->HUDWidgetScaleOverride = Old_HUDWidgetScaleOverride;
		TargetHUD->bDrawCTFMinimapHUDSetting = Old_bDrawCTFMinimap;
		TargetHUD->bUseWeaponColors = Old_bUseWeaponColors;
		TargetHUD->bDrawChatKillMsg = Old_bDrawChatKillMsg;
		TargetHUD->bDrawCenteredKillMsg = Old_bDrawCenteredKillMsg;
		TargetHUD->bDrawHUDKillIconMsg = Old_bDrawHUDKillIconMsg;
		TargetHUD->bPlayKillSoundMsg = Old_bPlayKillSoundMsg;
	}

	TargetHUD.Reset();
	GetPlayerOwner()->HideHUDSettings();
	return FReply::Handled();
}


FReply SUTHUDSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();
}

#endif