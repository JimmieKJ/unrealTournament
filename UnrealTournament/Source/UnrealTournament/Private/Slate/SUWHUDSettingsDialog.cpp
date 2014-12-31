// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWHUDSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"

#if !UE_SERVER


void SUWHUDSettingsDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
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

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	AUTPlayerController* PC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		TargetHUD = PC->MyUTHUD;
	}

	if (DialogContent.IsValid())
	{
		if (TargetHUD.IsValid() )
		{
			DialogContent->AddSlot()
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
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
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
							.Text(this, &SUWHUDSettingsDialog::GetHUDOpacityLabel)
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
								.Value(TargetHUD->HUDWidgetOpacity)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnHUDOpacityChanged)
							]
						]
					]

				+SVerticalBox::Slot()
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
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
							.Text(this, &SUWHUDSettingsDialog::GetHUDBorderOpacityLabel)
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
								.Value(TargetHUD->HUDWidgetBorderOpacity)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnHUDBorderOpacityChanged)
							]
						]
					]

				+SVerticalBox::Slot()
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
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
							.Text(this, &SUWHUDSettingsDialog::GetHUDSlateOpacityLabel)
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
								.Value(TargetHUD->HUDWidgetSlateOpacity)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnHUDSlateOpacityChanged)
							]
						]
					]

				+SVerticalBox::Slot()
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
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
							.Text(this, &SUWHUDSettingsDialog::GetHUDScaleLabel)
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
								.Value(TargetHUD->HUDWidgetScaleOverride / 2.0f)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnHUDScaleChanged)
							]
						]
					]

				+SVerticalBox::Slot()
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
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
							.Text(this, &SUWHUDSettingsDialog::GetWeaponBarOpacityLabel)
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
								.Value(TargetHUD->HUDWidgetWeaponbarInactiveOpacity)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnWeaponBarOpacityChanged)
							]
						]
					]
				+SVerticalBox::Slot()
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
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
							.Text(this, &SUWHUDSettingsDialog::GetWeaponBarScaleLabel)
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
								.Value(TargetHUD->HUDWidgetWeaponBarScaleOverride / 2.0f)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnWeaponBarScaleChanged)
							]
						]
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
					.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
					.Text(NSLOCTEXT("SUWHUDSettingsDialog","NeedsHUD","No HUD available!  Please use in game."))
				]
			];
		}
	}
}

FText SUWHUDSettingsDialog::GetHUDOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS","OpacityLabel","General Opacity ({0})"), FText::AsNumber(TargetHUD->HUDWidgetOpacity));
}

FText SUWHUDSettingsDialog::GetHUDBorderOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "BorderOpacityLabel", "Border Opacity ({0})"), FText::AsNumber(TargetHUD->HUDWidgetBorderOpacity));
}

FText SUWHUDSettingsDialog::GetHUDSlateOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "SlateOpacityLabel", "Slate Opacity ({0})"), FText::AsNumber(TargetHUD->HUDWidgetSlateOpacity));
}

FText SUWHUDSettingsDialog::GetHUDScaleLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "ScaleLabel", "Scale ({0})"), FText::AsNumber(TargetHUD->HUDWidgetScaleOverride));
}

FText SUWHUDSettingsDialog::GetWeaponBarOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarOpacityLabel", "Weapon Bar Opacity ({0})"), FText::AsNumber(TargetHUD->HUDWidgetWeaponbarInactiveOpacity));
}

FText SUWHUDSettingsDialog::GetWeaponBarScaleLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarScaleLabel", "Weapon Bar Scale ({0})"), FText::AsNumber(TargetHUD->HUDWidgetWeaponBarScaleOverride));
}


void SUWHUDSettingsDialog::OnHUDOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetOpacity = float(int(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnHUDBorderOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetBorderOpacity = float(int(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnHUDSlateOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetSlateOpacity = float(int(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnHUDScaleChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetScaleOverride = 2.0 * float(int(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnWeaponBarOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponbarInactiveOpacity = float(int(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnWeaponBarScaleChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponBarScaleOverride = 2.0 * float(int(NewValue * 100.0f)) / 100.0f;
	}
}



FReply SUWHUDSettingsDialog::OKClick()
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->SaveConfig();
	}
	TargetHUD.Reset();
	GetPlayerOwner()->HideHUDSettings();
	return FReply::Handled();
}

FReply SUWHUDSettingsDialog::CancelClick()
{

	AUTHUD* DefaultHUD = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>();
	if (TargetHUD.IsValid() && DefaultHUD)
	{
		TargetHUD->HUDWidgetOpacity = DefaultHUD->HUDWidgetOpacity;
		TargetHUD->HUDWidgetBorderOpacity = DefaultHUD->HUDWidgetBorderOpacity;
		TargetHUD->HUDWidgetSlateOpacity = DefaultHUD->HUDWidgetSlateOpacity;
		TargetHUD->HUDWidgetScaleOverride = DefaultHUD->HUDWidgetScaleOverride;
		TargetHUD->HUDWidgetWeaponbarInactiveOpacity = DefaultHUD->HUDWidgetBorderOpacity;
	}

	TargetHUD.Reset();
	GetPlayerOwner()->HideHUDSettings();
	return FReply::Handled();
}


FReply SUWHUDSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();
}

#endif