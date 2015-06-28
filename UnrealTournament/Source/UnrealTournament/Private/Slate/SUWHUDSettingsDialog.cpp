// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWHUDSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"
#include "SUTUtils.h"

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
							.bShadow(false)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	AUTPlayerController* PC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		TargetHUD = PC->MyUTHUD;
	}

	KillMsgDesc.Add(NSLOCTEXT("UT", "Text", "Text"));
	KillMsgDesc.Add(NSLOCTEXT("UT", "Icon", "Icon"));
	KillMsgDesc.Add(NSLOCTEXT("UT", "Both", "Both"));
	KillMsgDesc.Add(NSLOCTEXT("UT", "None", "None"));

	KillMsgList.Add(MakeShareable(new FText(KillMsgDesc[EHudKillMsgStyle::KMS_Text])));
	KillMsgList.Add(MakeShareable(new FText(KillMsgDesc[EHudKillMsgStyle::KMS_Icon])));
	KillMsgList.Add(MakeShareable(new FText(KillMsgDesc[EHudKillMsgStyle::KMS_Both])));
	KillMsgList.Add(MakeShareable(new FText(KillMsgDesc[EHudKillMsgStyle::KMS_None])));
	TSharedPtr<FText> InitiallySelectedHand = (TargetHUD != nullptr) ? KillMsgList[TargetHUD->KillMsgStyle] : KillMsgList[EHudKillMsgStyle::KMS_Text];

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
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(this, &SUWHUDSettingsDialog::GetHUDOpacityLabel)
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog","HUDOpacityTT","Adjusts how transparent the HUD should be.")))
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
								.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
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
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(this, &SUWHUDSettingsDialog::GetHUDBorderOpacityLabel)
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog","HUDBorderOpacityTT","Adjusts how transparent the hard edge border around each element of the HUD should be.")))
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
								.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
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
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(this, &SUWHUDSettingsDialog::GetHUDSlateOpacityLabel)
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog","HUDSlateOpacityTT","Adjusts how transparent the background portion of each HUD element should be.")))
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
								.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
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
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(this, &SUWHUDSettingsDialog::GetHUDScaleLabel)
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog","HUDScaleTT","Makes the HUD elements bigger or smaller.")))
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
								.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
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
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(this, &SUWHUDSettingsDialog::GetWeaponBarOpacityLabel)
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog","HUDWeaponBarOpaictyTT","Adjusts how transparent the Weapon Bar should be.  NOTE this is applied in addition to the normal transparency setting.")))
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
								.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
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
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(this, &SUWHUDSettingsDialog::GetWeaponBarIconOpacityLabel)
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog","HUDWeaponBarIconOpacityTT","Adjusts how transparent the icons on the Weapon Bar should be.")))
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
								.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
								.Value(TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnWeaponBarIconOpacityChanged)
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
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(this, &SUWHUDSettingsDialog::GetWeaponBarEmptyOpacityLabel)
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog","HUDWeaponBarEmptyOpacityTT","Adjusts how transparent an empty Weapon Bar slot should be.")))
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
								.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
								.Orientation(Orient_Horizontal)
								.Value(TargetHUD->HUDWidgetWeaponBarEmptyOpacity)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnWeaponBarEmptyOpacityChanged)
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
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(this, &SUWHUDSettingsDialog::GetWeaponBarScaleLabel)
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog","HUDWeaponBarScaleTT","Adjusts how big or small the Weapon Bar should be.")))
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
								.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
								.Orientation(Orient_Horizontal)
								.Value(TargetHUD->HUDWidgetWeaponBarScaleOverride / 2.0f)
								.OnValueChanged(this, &SUWHUDSettingsDialog::OnWeaponBarScaleChanged)
							]
						]
					]
				+ SVerticalBox::Slot()
				.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(300.0f)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(NSLOCTEXT("SUWWeaponConfigDialog", "KillMsgStyle", "Kill Message Style"))
						]
					]
					+ SHorizontalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Right)
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(300.0f)
						[
							SAssignNew(KillMsgStyle, SComboBox< TSharedPtr<FText> >)
							.InitiallySelectedItem(InitiallySelectedHand)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&KillMsgList)
							.OnGenerateWidget(this, &SUWHUDSettingsDialog::GenerateKillMsgStyleWidget)
							.OnSelectionChanged(this, &SUWHUDSettingsDialog::OnKillMsgStyleSelected)
							.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
							.Content()
							[
								SAssignNew(SelectedKillMsgStyle, STextBlock)
								.Text(*InitiallySelectedHand.Get())
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
							]
						]
					]
				]
				+ SVerticalBox::Slot()
					.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
					.HAlign(HAlign_Right)
					[
						SAssignNew(DrawPopupKillMsg, SCheckBox)
						.ForegroundColor(FLinearColor::White)
						.IsChecked(TargetHUD->bDrawPopupKillMsg ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.OnCheckStateChanged(this, &SUWHUDSettingsDialog::OnDrawPopupKillMsgChanged)
						.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
						.Content()
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(NSLOCTEXT("SUWHUDSettingsDialog", "DrawPopupKillMsg", "Popup Kill Messages"))
						]
					]
				+ SVerticalBox::Slot()
					.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
					.HAlign(HAlign_Right)
					[
						SAssignNew(UseWeaponColor, SCheckBox)
						.ForegroundColor(FLinearColor::White)
						.IsChecked(TargetHUD->bUseWeaponColors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.OnCheckStateChanged(this, &SUWHUDSettingsDialog::OnUseWeaponColorChanged)
						.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
						.Content()
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(NSLOCTEXT("SUWHUDSettingsDialog", "UseWeaponColors", "Colorize Icons"))
							.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWHUDSettingsDialog", "HUDWeaponColorsTT", "Should the Weapon Bar colorize it's icons.")))
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
	return FText::Format(NSLOCTEXT("HUDSETTINGS","OpacityLabel","General Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetOpacity : 0.f));
}

FText SUWHUDSettingsDialog::GetHUDBorderOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "BorderOpacityLabel", "Border Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetBorderOpacity : 0.f));
}

FText SUWHUDSettingsDialog::GetHUDSlateOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "SlateOpacityLabel", "Slate Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetSlateOpacity : 0.f));
}

FText SUWHUDSettingsDialog::GetHUDScaleLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "ScaleLabel", "Scale ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetScaleOverride : 0.f));
}

FText SUWHUDSettingsDialog::GetWeaponBarOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarOpacityLabel", "Weapon Bar Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetWeaponbarInactiveOpacity : 0.f));
}

FText SUWHUDSettingsDialog::GetWeaponBarIconOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarIconOpacityLabel", "Weapon Bar Icon/Label Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity : 0.f));
}

FText SUWHUDSettingsDialog::GetWeaponBarEmptyOpacityLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarEmptyOpacityLabel", "Weapon Bar Empy Slot Opacity ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetWeaponBarEmptyOpacity : 0.f));
}


FText SUWHUDSettingsDialog::GetWeaponBarScaleLabel() const
{
	return FText::Format(NSLOCTEXT("HUDSETTINGS", "WeaponBarScaleLabel", "Weapon Bar Scale ({0})"), FText::AsNumber(TargetHUD.IsValid() ? TargetHUD->HUDWidgetWeaponBarScaleOverride : 0.f));
}


void SUWHUDSettingsDialog::OnHUDOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnHUDBorderOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetBorderOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnHUDSlateOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetSlateOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnHUDScaleChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetScaleOverride = 2.0 * float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnWeaponBarOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponbarInactiveOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnWeaponBarIconOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponBarInactiveIconOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnWeaponBarEmptyOpacityChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponBarEmptyOpacity = float(int32(NewValue * 100.0f)) / 100.0f;
	}
}



void SUWHUDSettingsDialog::OnWeaponBarScaleChanged(float NewValue)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->HUDWidgetWeaponBarScaleOverride = 2.0 * float(int32(NewValue * 100.0f)) / 100.0f;
	}
}

void SUWHUDSettingsDialog::OnUseWeaponColorChanged(ECheckBoxState NewState)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->bUseWeaponColors = NewState == ECheckBoxState::Checked;
	}
}

void SUWHUDSettingsDialog::OnDrawPopupKillMsgChanged(ECheckBoxState NewState)
{
	if (TargetHUD.IsValid())
	{
		TargetHUD->bDrawPopupKillMsg = NewState == ECheckBoxState::Checked;
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
		TargetHUD->HUDWidgetWeaponbarInactiveOpacity = DefaultHUD->HUDWidgetWeaponbarInactiveOpacity;
		TargetHUD->HUDWidgetWeaponBarEmptyOpacity = DefaultHUD->HUDWidgetWeaponBarEmptyOpacity;
		TargetHUD->HUDWidgetScaleOverride = DefaultHUD->HUDWidgetScaleOverride;
		TargetHUD->HUDWidgetWeaponbarInactiveOpacity = DefaultHUD->HUDWidgetBorderOpacity;
		TargetHUD->bUseWeaponColors = DefaultHUD->bUseWeaponColors;
		TargetHUD->KillMsgStyle = DefaultHUD->KillMsgStyle;
		TargetHUD->bDrawPopupKillMsg = DefaultHUD->bDrawPopupKillMsg;
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

TSharedRef<ITableRow> SUWHUDSettingsDialog::GenerateKillMsgStyleRow(UClass* WeaponType, const TSharedRef<STableViewBase>& OwningList)
{
	checkSlow(WeaponType->IsChildOf(AUTWeapon::StaticClass()));

	return SNew(STableRow<UClass*>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(WeaponType->GetDefaultObject<AUTWeapon>()->DisplayName)
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
		];
}

TSharedRef<SWidget> SUWHUDSettingsDialog::GenerateKillMsgStyleWidget(TSharedPtr<FText> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(*InItem.Get())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		];
}

void SUWHUDSettingsDialog::OnKillMsgStyleSelected(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (TargetHUD.IsValid())
	{
		for (int32 i = 0; i < KillMsgDesc.Num(); i++)
		{
			if (NewSelection.Get()->CompareTo(KillMsgDesc[i]) == 0)
			{
				TargetHUD.Get()->KillMsgStyle = (EHudKillMsgStyle::Type)i;
			}
		}
	}
	SelectedKillMsgStyle->SetText(*NewSelection.Get());
}


#endif