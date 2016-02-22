// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTSystemSettingsDialog.h"
#include "../SUWindowsStyle.h"
#include "../SUTUtils.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"

#if !UE_SERVER

SVerticalBox::FSlot& SUTSystemSettingsDialog::AddSectionHeader(const FText& SectionDesc)
{
	return SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(FMargin(0.0f, 15.0f, 0.0f, 15.0f))
		[
			SNew(STextBlock)
			.Text(SectionDesc)
			.TextStyle(SUWindowsStyle::Get(),"UT.Common.BoldText")
		];

}

SVerticalBox::FSlot& SUTSystemSettingsDialog::AddGeneralScalabilityWidget(const FString& Desc, TSharedPtr< SComboBox< TSharedPtr<FString> > >& ComboBox, TSharedPtr<STextBlock>& SelectedItemWidget, void (SUTSystemSettingsDialog::*SelectionFunc)(TSharedPtr<FString>, ESelectInfo::Type), int32 SettingValue, const TAttribute<FText>& TooltipText)
{
	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(650)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(FText::FromString(Desc))
					.ToolTip(SUTUtils::CreateTooltip(TooltipText))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ComboBox, SComboBox< TSharedPtr<FString> >)
				.InitiallySelectedItem(GeneralScalabilityList[SettingValue])
				.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
				.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
				.OptionsSource(&GeneralScalabilityList)
				.OnGenerateWidget(this, &SUTDialogBase::GenerateStringListWidget)
				.OnSelectionChanged(this, SelectionFunc)
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					[
						SAssignNew(SelectedItemWidget, STextBlock)
						.Text(FText::FromString(*GeneralScalabilityList[SettingValue].Get()))
						.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText.Black")
					]
				]
			]
		];
}

SVerticalBox::FSlot& SUTSystemSettingsDialog::AddConsoleVarSliderWidget(TSharedRef<SSlateConsoleVarDelegate> CVar, const FText& Label)
{
	CVarDelegates.Add(CVar);

	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(40.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(650)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.SmallText")
					.Text(Label)
					.ToolTip(SUTUtils::CreateTooltip(CVar->GetTooltip()))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				.Padding(FMargin(0.0f, 2.0f, 60.0f, 2.0f))
				.Content()
				[
					SNew(SSlider)
					.Style(SUWindowsStyle::Get(), "UT.Common.Slider")
					.Value(CVar, &SSlateConsoleVarDelegate::GetForSlider)
					.OnValueChanged(CVar, &SSlateConsoleVarDelegate::SetFromSlider)
				]
			]
		];
}
SVerticalBox::FSlot& SUTSystemSettingsDialog::AddTopLevelConsoleVarCheckboxWidget(TSharedRef<SSlateConsoleVarDelegate> CVar, const FText& Label)
{

	CVarDelegates.Add(CVar);

	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(650)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(Label)
					.ToolTip(SUTUtils::CreateTooltip(CVar->GetTooltip()))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				.Padding(FMargin(0.0f, 2.0f))
				.Content()
				[
					SNew(SCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.IsChecked(CVar, &SSlateConsoleVarDelegate::GetCheckbox)
					.OnCheckStateChanged(CVar, &SSlateConsoleVarDelegate::SetCheckbox)
				]
			]
		];
}

SVerticalBox::FSlot& SUTSystemSettingsDialog::AddConsoleVarCheckboxWidget(TSharedRef<SSlateConsoleVarDelegate> CVar, const FText& Label)
{
	CVarDelegates.Add(CVar);

	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(40.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(650)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.SmallText")
					.Text(Label)
					.ToolTip(SUTUtils::CreateTooltip(CVar->GetTooltip()))
				]
			]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(300.0f)
					.Padding(FMargin(0.0f, 2.0f))
					.Content()
					[
						SNew(SCheckBox)
						.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
						.IsChecked(CVar, &SSlateConsoleVarDelegate::GetCheckbox)
						.OnCheckStateChanged(CVar, &SSlateConsoleVarDelegate::SetCheckbox)
					]
				]
		];
}

SVerticalBox::FSlot& SUTSystemSettingsDialog::AddAAModeWidget(const FString& Desc, TSharedPtr< SComboBox< TSharedPtr<FString> > >& ComboBox, TSharedPtr<STextBlock>& SelectedItemWidget, void (SUTSystemSettingsDialog::*SelectionFunc)(TSharedPtr<FString>, ESelectInfo::Type), int32 SettingValue, const TAttribute<FText>& TooltipText)
{
	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(650)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(FText::FromString(Desc))
					.ToolTip(SUTUtils::CreateTooltip(TooltipText))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ComboBox, SComboBox< TSharedPtr<FString> >)
				.InitiallySelectedItem(AAModeList[SettingValue])
				.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
				.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
				.OptionsSource(&AAModeList)
				.OnGenerateWidget(this, &SUTDialogBase::GenerateStringListWidget)
				.OnSelectionChanged(this, SelectionFunc)
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					[
						SAssignNew(SelectedItemWidget, STextBlock)
						.Text(FText::FromString(*AAModeList[SettingValue].Get()))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
					]
				]
			]
		];
}

SVerticalBox::FSlot& SUTSystemSettingsDialog::AddGeneralSliderWidget(const FString& Desc, TSharedPtr<SSlider>& SliderWidget, float SettingValue, const TAttribute<FText>& TooltipText)
{
	return SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(FText::FromString(Desc))
				.ToolTip(SUTUtils::CreateTooltip(TooltipText))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300.0f)
			.Padding(FMargin(0.0f, 2.0f))
			.Content()
			[
				SAssignNew(SliderWidget, SSlider)
				.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
				.Orientation(Orient_Horizontal)
				.Value(SettingValue)
			]
		]
	];
}

SVerticalBox::FSlot& SUTSystemSettingsDialog::AddGeneralSliderWithLabelWidget(TSharedPtr<SSlider>& SliderWidget, TSharedPtr<STextBlock>& LabelWidget, void(SUTSystemSettingsDialog::*SelectionFunc)(float), const FString& InitialLabel, float SettingValue, const TAttribute<FText>& TooltipText)
{
	return SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SAssignNew(LabelWidget, STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(FText::FromString(InitialLabel))
				.ToolTip(SUTUtils::CreateTooltip(TooltipText))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300.0f)
			.Padding(FMargin(0.0f, 2.0f))
			.Content()
			[
				SAssignNew(SliderWidget, SSlider)
				.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
				.OnValueChanged(this, SelectionFunc)
				.Orientation(Orient_Horizontal)
				.Value(SettingValue)
			]
		]
	];
}

void SUTSystemSettingsDialog::Construct(const FArguments& InArgs)
{
	DecalLifetimeRange = FVector2D(5.0f, 105.0f);
	ScreenPercentageRange = FVector2D(25.0f, 100.0f);
	bAdvancedMode = false;

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

	if (DialogContent.IsValid())
	{
		DialogContent->ClearChildren();
		CVarDelegates.Empty();

		DialogContent->AddSlot()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
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
			+ SOverlay::Slot()
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
						.Padding(FMargin(25.0f, 0.0f, 0.0f, 0.0f))
						.AutoWidth()
						[
							SAssignNew(GeneralSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.Text(NSLOCTEXT("SUTSystemSettingsDialog", "ControlTabGeneral", "General"))
							.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							.OnClicked(this, &SUTSystemSettingsDialog::OnTabClickGeneral)
						]

						+ SHorizontalBox::Slot()
						.Padding(FMargin(25.0f, 0.0f, 0.0f, 0.0f))
						.AutoWidth()
						[
							SAssignNew(GraphicsSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							.Text(NSLOCTEXT("SUTSystemSettingsDialog", "ControlTabGraphics", "Graphics"))
							.OnClicked(this, &SUTSystemSettingsDialog::OnTabClickGraphics)
						]

						+ SHorizontalBox::Slot()
						.Padding(FMargin(25.0f, 0.0f, 0.0f, 0.0f))
						.AutoWidth()
						[
							SAssignNew(AudioSettingsTabButton, SUTTabButton)
							.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
							.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							.Text(NSLOCTEXT("SUTSystemSettingsDialog", "ControlTabAudio", "Audio"))
							.OnClicked(this, &SUTSystemSettingsDialog::OnTabClickAudio)
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.LightFill"))
						]
					]
				]

				// Content

				+SVerticalBox::Slot()
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
						// Settings Tabs
						SAssignNew(TabWidget, SWidgetSwitcher)

						// General Settings
						+ SWidgetSwitcher::Slot()
						[
							BuildGeneralTab()
						]

						// Graphics Settings
						+ SWidgetSwitcher::Slot()
							[
								BuildGraphicsTab()
							]

						// Audio Settings
						+ SWidgetSwitcher::Slot()
							[
								BuildAudioTab()
							]
					]
				]
			]
		];
	}

	UpdateAdvancedWidgets();
	OnTabClickGeneral();
}

TSharedRef<class SWidget> SUTSystemSettingsDialog::BuildCustomButtonBar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.6f)
		.HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.9f)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTSystemSettingsDialog", "Advanced", "Show Advanced Options"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.1f)
			.HAlign(HAlign_Right)
			[
				SNew(SCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.OnCheckStateChanged(this, &SUTSystemSettingsDialog::OnAdvancedCheckChanged)
				.IsChecked(bAdvancedMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			]
		];
}

void SUTSystemSettingsDialog::OnAdvancedCheckChanged(ECheckBoxState NewState)
{
	bAdvancedMode = (NewState == ECheckBoxState::Checked);
	UpdateAdvancedWidgets();
}

void SUTSystemSettingsDialog::UpdateAdvancedWidgets()
{
	for (TSharedRef<SWidget> Widget : AdvancedWidgets)
	{
		Widget->SetVisibility(bAdvancedMode ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

TSharedRef<SWidget> SUTSystemSettingsDialog::BuildGeneralTab()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	// Get Viewport size
	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	// Get pointer to the UTGameEngine
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine == NULL) // PIE
	{
		UTEngine = UUTGameEngine::StaticClass()->GetDefaultObject<UUTGameEngine>();
	}

	// find current and available screen resolutions
	int32 CurrentResIndex = INDEX_NONE;
	FScreenResolutionArray ResArray;
	if (RHIGetAvailableResolutions(ResArray, false))
	{
		TArray<FIntPoint> AddedRes; // used to more efficiently avoid duplicates
		for (int32 ModeIndex = 0; ModeIndex < ResArray.Num(); ModeIndex++)
		{
			if (ResArray[ModeIndex].Width >= 800 && ResArray[ModeIndex].Height >= 600)
			{
				FIntPoint NewRes(int32(ResArray[ModeIndex].Width), int32(ResArray[ModeIndex].Height));
				if (!AddedRes.Contains(NewRes))
				{
					ResList.Add(MakeShareable(new FString(FString::Printf(TEXT("%ix%i"), NewRes.X, NewRes.Y))));
					if (NewRes.X == int32(ViewportSize.X) && NewRes.Y == int32(ViewportSize.Y))
					{
						CurrentResIndex = ResList.Num() - 1;
					}
					AddedRes.Add(NewRes);
				}
			}
		}
	}
	if (CurrentResIndex == INDEX_NONE)
	{
		CurrentResIndex = ResList.Add(MakeShareable(new FString(FString::Printf(TEXT("%ix%i"), int32(ViewportSize.X), int32(ViewportSize.Y)))));
	}
	
	DisplayModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "DisplayModeFullscreen", "Fullscreen").ToString())));
	DisplayModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "DisplayModeWindowedFullscreen", "Windowed (Fullscreen)").ToString())));
	DisplayModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "DisplayModeWindowed", "Windowed").ToString())));
	int32 CurrentDisplayIndex = FMath::Clamp(int32(UserSettings->GetFullscreenMode()), 0, DisplayModeList.Num() - 1);

	return SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTSystemSettingsDialog", "Resolution", "Resolution"))
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTSystemSettingsDialog", "Resolution_Tooltip", "Set the resolution of the game window.")))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SComboBox< TSharedPtr<FString> >)
			.InitiallySelectedItem(ResList[CurrentResIndex])
			.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
			.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
			.OptionsSource(&ResList)
			.OnGenerateWidget(this, &SUTDialogBase::GenerateStringListWidget)
			.OnSelectionChanged(this, &SUTSystemSettingsDialog::OnResolutionSelected)
			.Content()
			[
				SAssignNew(SelectedRes, STextBlock)
				.Text(FText::FromString(*ResList[CurrentResIndex].Get()))
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.ButtonText.Black")
			]
		]
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTSystemSettingsDialog", "DisplayMode", "Display Mode"))
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTSystemSettingsDialog", "Fullscreen_Tooltip", "Toggle whether the application runs in full-screen mode or is in a window.")))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(DisplayModeComboBox, SComboBox< TSharedPtr<FString> >)
			.InitiallySelectedItem(DisplayModeList[CurrentDisplayIndex])
			.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
			.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
			.OptionsSource(&DisplayModeList)
			.OnGenerateWidget(this, &SUTDialogBase::GenerateStringListWidget)
			.OnSelectionChanged(this, &SUTSystemSettingsDialog::OnDisplayModeSelected)
			.Content()
			[
				SAssignNew(SelectedDisplayMode, STextBlock)
				.Text(FText::FromString(*DisplayModeList[CurrentDisplayIndex].Get()))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
			]
		]
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text_Raw(this, &SUTSystemSettingsDialog::GetVSyncText)
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTSystemSettingsDialog", "VSync_Tooltip", "Toggle VSync to avoid frame tearing - may lower framerate.")))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(VSync, SCheckBox)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.IsChecked(UserSettings->IsVSyncEnabled() ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
		]
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
	[

		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTSystemSettingsDialog", "Frame Rate Cap", "Frame Rate Cap"))
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTSystemSettingsDialog", "FrameRateCap_Tooltip", "Limiting the max frame rate can improve the smoothness of mouse improvement.")))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(FrameRateCap, SEditableTextBox)
			.Style(SUWindowsStyle::Get(),"UT.Common.Editbox.White")
			.ForegroundColor(FLinearColor::Black)
			.MinDesiredWidth(100.0f)
			.Text(FText::AsNumber(UTEngine->FrameRateCap))
		]
	]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(650)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUTSystemSettingsDialog", "Smooth Framerate", "Smooth Framerate"))
					.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTSystemSettingsDialog", "SmoothFramerate_Tooltip", "This setting is used to smooth framerate spikes which can affect mouse control.")))
				]
			]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SmoothFrameRate, SCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.IsChecked(GEngine->bSmoothFrameRate ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
				]
		];
}

TSharedRef<SWidget> SUTSystemSettingsDialog::BuildGraphicsTab()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	// find current and available engine scalability options
	if (!AutodetectHandle.IsValid())
	{
		AutodetectHandle = UserSettings->OnSettingsAutodetected().AddSP(this, &SUTSystemSettingsDialog::OnSettingsAutodetected);
	}
	Scalability::FQualityLevels QualitySettings = UserSettings->ScalabilityQuality;
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "SettingsLow", "Low").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "SettingsMedium", "Medium").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "SettingsHigh", "High").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "SettingsEpic", "Epic").ToString())));
	QualitySettings.TextureQuality = FMath::Clamp<int32>(QualitySettings.TextureQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.ShadowQuality = FMath::Clamp<int32>(QualitySettings.ShadowQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.PostProcessQuality = FMath::Clamp<int32>(QualitySettings.PostProcessQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.EffectsQuality = FMath::Clamp<int32>(QualitySettings.EffectsQuality, 0, GeneralScalabilityList.Num() - 1);

	// these are to restore the 'simple' settings since we apply the changes to them immediately as well
	// NOTE: IMPORTANT THAT THESE ARE FIRST! When restoring previous settings, we want to start with the combination settings and then restore individual overrides
	CVarDelegates.Add(MakeShareable(new SSlateConsoleVarDelegate(TEXT("sg.ViewDistanceQuality"))));
	CVarDelegates.Add(MakeShareable(new SSlateConsoleVarDelegate(TEXT("sg.AntiAliasingQuality"))));
	CVarDelegates.Add(MakeShareable(new SSlateConsoleVarDelegate(TEXT("sg.ShadowQuality"))));
	CVarDelegates.Add(MakeShareable(new SSlateConsoleVarDelegate(TEXT("sg.PostProcessQuality"))));
	CVarDelegates.Add(MakeShareable(new SSlateConsoleVarDelegate(TEXT("sg.TextureQuality"))));
	CVarDelegates.Add(MakeShareable(new SSlateConsoleVarDelegate(TEXT("sg.EffectsQuality"))));

	AAModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "AAModeNone", "None").ToString())));
	AAModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "AAModeFXAA", "FXAA").ToString())));
	AAModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "AAModeTemporal", "Temporal").ToString())));
	auto AAModeCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessAAQuality"));
	int32 AAModeSelection = ConvertAAModeToComboSelection(AAModeCVar->GetValueOnGameThread());

	float DecalSliderSetting = (GetDefault<AUTWorldSettings>()->MaxImpactEffectVisibleLifetime <= 0.0f) ? 1.0f : ((GetDefault<AUTWorldSettings>()->MaxImpactEffectVisibleLifetime - DecalLifetimeRange.X) / (DecalLifetimeRange.Y - DecalLifetimeRange.X));

	// Calculate our current Screen Percentage
	auto ScreenPercentageCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
	int32 ScreenPercentage = ScreenPercentageCVar->GetValueOnGameThread();
	ScreenPercentage = int32(FMath::Clamp(float(ScreenPercentage), ScreenPercentageRange.X, ScreenPercentageRange.Y));

	float ScreenPercentageSliderSetting = (float(ScreenPercentage) - ScreenPercentageRange.X) / (ScreenPercentageRange.Y - ScreenPercentageRange.X);

	TSharedRef<SBox> ShadowAdvanced = SNew(SBox)
		[
			SNew(SVerticalBox)
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.LightFunctionQuality"))), NSLOCTEXT("SUTSystemSettingsDialog", "LightFunctions", "Light Functions"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.ShadowQuality"), FVector2D(0.0f, 5.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "ShadowDetail", "Shadow Precision"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.Shadow.MaxResolution"), FVector2D(512.0f, 1024.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "ShadowRes", "Shadow Texture Resolution"))
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.DistanceFieldShadowing"))), NSLOCTEXT("SUTSystemSettingsDialog", "DistanceFieldShadow", "Distance Field Shadows"))
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.DistanceFieldAO"))), NSLOCTEXT("SUTSystemSettingsDialog", "DistanceFieldAO", "Distance Field Ambient Occlusion"))
		];
	TSharedRef<SBox> EffectsAdvanced = SNew(SBox)
		[
			SNew(SVerticalBox)
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.RefractionQuality"), FVector2D(0.0f, 2.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "RFQuality", "Refraction Quality"))
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.ReflectionEnvironment"))), NSLOCTEXT("SUTSystemSettingsDialog", "ReflectionEnv", "Reflection Environment Mapping"))
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.TranslucencyVolumeBlur"))), NSLOCTEXT("SUTSystemSettingsDialog", "TranslucencyVolume", "Translucency Volume Blur"))
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.MaterialQualityLevel"))), NSLOCTEXT("SUTSystemSettingsDialog", "MaterialQuality", "High Detail Shaders"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("Foliage.MinimumScreenSize"), FVector2D(0.0002f, 0.000002f), 1.01f)), NSLOCTEXT("SUTSystemSettingsDialog", "FoliageDrawDist", "Foliage Draw Distance"))
		];
	TSharedRef<SBox> PPAdvanced = SNew(SBox)
		[
			SNew(SVerticalBox)
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.DepthOfFieldQuality"), FVector2D(0.0f, 2.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "DOFQuality", "Depth Of Field Quality"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.LensFlareQuality"), FVector2D(0.0f, 2.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "LFQuality", "Lens Flare Quality"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.SceneColorFringeQuality"), FVector2D(0.0f, 1.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "SceneFringeQuality", "Scene Color Fringe Quality"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.BloomQuality"), FVector2D(1.0f, 5.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "BloomQuality", "Bloom Quality"))
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.BloomDirt"))), NSLOCTEXT("SUTSystemSettingsDialog", "BloomDirt", "Screen Dirt Effect"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.FastBlurThreshold"), FVector2D(0.0f, 7.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "FastBlurThreshold", "Fast Blur Threshold"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.Tonemapper.Quality"), FVector2D(0.0f, 1.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "TonemapperQuality", "Tonemapper Quality"))
			+ AddConsoleVarSliderWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.LightShaftQuality"), FVector2D(0.0f, 1.0f))), NSLOCTEXT("SUTSystemSettingsDialog", "LightShaftQuality", "Light Shaft Quality"))
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.SeparateTranslucency"))), NSLOCTEXT("SUTSystemSettingsDialog", "SeparateTranslucency", "Separate Translucency Pass"))
			+ AddConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.TonemapperFilm"))), NSLOCTEXT("SUTSystemSettingsDialog", "TonemapperFilm", "Tonemapper Film"))
		];
	AdvancedWidgets.Add(ShadowAdvanced);
	AdvancedWidgets.Add(EffectsAdvanced);
	AdvancedWidgets.Add(PPAdvanced);

	return SNew(SVerticalBox)
		+ AddGeneralSliderWithLabelWidget(ScreenPercentageSlider, ScreenPercentageLabel, &SUTSystemSettingsDialog::OnScreenPercentageChange,
		GetScreenPercentageLabelText(ScreenPercentageSliderSetting), ScreenPercentageSliderSetting,
		NSLOCTEXT("SUTSystemSettingsDialog", "ScreenPercentage_Tooltip", "Reducing screen percentage reduces the effective 3D rendering resolution, with the result upsampled to your desired resolution.\nThis improves performance while keeping your UI and HUD at full resolution and not affecting screen size on certain LCD screens."))

		+ AddGeneralScalabilityWidget(NSLOCTEXT("SUTSystemSettingsDialog", "TextureDetail", "Texture Detail").ToString(), TextureRes, SelectedTextureRes,
		&SUTSystemSettingsDialog::OnTextureResolutionSelected, QualitySettings.TextureQuality,
		NSLOCTEXT("SUTSystemSettingsDialog", "TextureDetail_Tooltip", "Controls the quality of textures, lower setting can improve performance when GPU preformance is an issue."))

		+ AddGeneralScalabilityWidget(NSLOCTEXT("SUTSystemSettingsDialog", "ShadowQuality", "Shadow Quality").ToString(), ShadowQuality, SelectedShadowQuality,
		&SUTSystemSettingsDialog::OnShadowQualitySelected, QualitySettings.ShadowQuality,
		NSLOCTEXT("SUTSystemSettingsDialog", "ShadowQuality_Tooltip", "Controls the quality of shadows, lower setting can improve performance on both CPU and GPU."))

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ShadowAdvanced
		]

		+ AddGeneralScalabilityWidget(NSLOCTEXT("SUTSystemSettingsDialog", "EffectsQuality", "Effects Quality").ToString(), EffectQuality, SelectedEffectQuality,
		&SUTSystemSettingsDialog::OnEffectQualitySelected, QualitySettings.EffectsQuality,
		NSLOCTEXT("SUTSystemSettingsDialog", "EffectQuality_Tooltip", "Controls the quality of effects, lower setting can improve performance on both CPU and GPU."))

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			EffectsAdvanced
		]

		+ AddGeneralScalabilityWidget(NSLOCTEXT("SUTSystemSettingsDialog", "PP Quality", "Post Process Quality").ToString(), PPQuality, SelectedPPQuality,
		&SUTSystemSettingsDialog::OnPPQualitySelected, QualitySettings.PostProcessQuality,
		NSLOCTEXT("SUTSystemSettingsDialog", "PPQuality_Tooltip", "Controls the quality of post processing effect, lower setting can improve performance when GPU preformance is an issue."))

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			PPAdvanced
		]

		+ AddAAModeWidget(NSLOCTEXT("SUTSystemSettingsDialog", "AAMode", "Anti Aliasing Mode").ToString(), AAMode, SelectedAAMode,
		&SUTSystemSettingsDialog::OnAAModeSelected, AAModeSelection,
		NSLOCTEXT("SUTSystemSettingsDialog", "AAMode_Tooltip", "Controls the type of antialiasing, turning it off can improve performance."))

		+ AddGeneralSliderWithLabelWidget(DecalLifetime, DecalLifetimeLabel, &SUTSystemSettingsDialog::OnDecalLifetimeChange, GetDecalLifetimeLabelText(DecalSliderSetting), DecalSliderSetting,
		NSLOCTEXT("SUTSystemSettingsDialog", "DecalLifetime_Tooltip", "Controls how long decals last (like the bullet impact marks left on walls)."))

		+ AddTopLevelConsoleVarCheckboxWidget(MakeShareable(new SSlateConsoleVarDelegate(TEXT("r.FinishCurrentFrame"))), NSLOCTEXT("SUTSystemSettingsDialog", "RenderBeforeSubmit", "Skip GPU buffering"))

		// Autodetect settings button
		+SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
			.ForegroundColor(FLinearColor::Black)
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUTSystemSettingsDialog", "AutoSettingsButtonText", "Auto Detect Settings"))
			.OnClicked(this, &SUTSystemSettingsDialog::OnAutodetectClick)
			.Visibility(this, &SUTSystemSettingsDialog::AutoDetectSettingsVisibility)
		];
}

EVisibility SUTSystemSettingsDialog::AutoDetectSettingsVisibility() const
{
	if (PlayerOwner->IsMenuGame())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

TSharedRef<SWidget> SUTSystemSettingsDialog::BuildAudioTab()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	BotSpeechList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "BotSpeechNone", "None").ToString())));
	BotSpeechList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "BotSpeechStatusText", "Status Text").ToString())));
	BotSpeechList.Add(MakeShareable(new FString(NSLOCTEXT("SUTSystemSettingsDialog", "BotSpeechAll", "All").ToString())));
	const int32 SpeechSettingValue = FMath::Clamp<int32>(int32(UserSettings->GetBotSpeech()), 0, BotSpeechList.Num() - 1);

	return SNew(SVerticalBox)

	+ AddGeneralSliderWithLabelWidget(SoundVolumes[EUTSoundClass::Master], SoundVolumesLabels[EUTSoundClass::Master], &SUTSystemSettingsDialog::OnSoundVolumeChangedMaster, NSLOCTEXT("SUTSystemSettingsDialog", "MasterSoundVolume", "Master Sound Volume").ToString(), UserSettings->GetSoundClassVolume(EUTSoundClass::Master),
		NSLOCTEXT("SUTSystemSettingsDialog", "MasterSoundVolume_Tooltip", "Controls the volume of all audio, this setting in conjuction the vlolumes below will determine the volume of a particular piece of audio."))
	+ AddGeneralSliderWithLabelWidget(SoundVolumes[EUTSoundClass::Music], SoundVolumesLabels[EUTSoundClass::Music], &SUTSystemSettingsDialog::OnSoundVolumeChangedMusic, NSLOCTEXT("SUTSystemSettingsDialog", "MusicVolume", "Music Volume").ToString(), UserSettings->GetSoundClassVolume(EUTSoundClass::Music))
	+ AddGeneralSliderWithLabelWidget(SoundVolumes[EUTSoundClass::SFX], SoundVolumesLabels[EUTSoundClass::SFX], &SUTSystemSettingsDialog::OnSoundVolumeChangedSFX, NSLOCTEXT("SUTSystemSettingsDialog", "SFXVolume", "Effects Volume").ToString(), UserSettings->GetSoundClassVolume(EUTSoundClass::SFX))
	+ AddGeneralSliderWithLabelWidget(SoundVolumes[EUTSoundClass::Voice], SoundVolumesLabels[EUTSoundClass::Voice], &SUTSystemSettingsDialog::OnSoundVolumeChangedVoice, NSLOCTEXT("SUTSystemSettingsDialog", "VoiceVolume", "Announcer Volume").ToString(), UserSettings->GetSoundClassVolume(EUTSoundClass::Voice))

	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTSystemSettingsDialog", "BotSpeech", "Enable Bot Speech"))
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTSystemSettingsDialog", "BotSpeech_Tooltip", "Whether bots taunt and provide status updates.")))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(BotSpeechCombo, SComboBox<TSharedPtr<FString>>)
			.InitiallySelectedItem(BotSpeechList[SpeechSettingValue])
			.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
			.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
			.OptionsSource(&BotSpeechList)
			.OnGenerateWidget(this, &SUTDialogBase::GenerateStringListWidget)
			.OnSelectionChanged(this, &SUTSystemSettingsDialog::OnBotSpeechSelected)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				[
					SAssignNew(SelectedBotSpeech, STextBlock)
					.Text(FText::FromString(*BotSpeechList[SpeechSettingValue].Get()))
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
				]
			]
		]
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTSystemSettingsDialog", "HRTFText", "HRTF [EXPERIMENTAL] [WIN64 ONLY]"))
				.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTSystemSettingsDialog", "HRTF_Tooltip", "Toggle Head Related Transfer Function, EXPERIMENTAL, WIN64 ONLY")))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(HRTFCheckBox, SCheckBox)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.IsChecked(UserSettings->IsHRTFEnabled() ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
		]
	];
}

void SUTSystemSettingsDialog::OnBotSpeechSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedBotSpeech->SetText(*NewSelection.Get());
}

void SUTSystemSettingsDialog::OnSoundVolumeChangedMaster(float NewValue)
{
	// Temporarily change audio level
	UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
	if (AudioSettings)
	{
		AudioSettings->SetSoundClassVolume(EUTSoundClass::Master, NewValue);
	}
}

void SUTSystemSettingsDialog::OnSoundVolumeChangedMusic(float NewValue)
{
	// Temporarily change audio level
	UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
	if (AudioSettings)
	{
		AudioSettings->SetSoundClassVolume(EUTSoundClass::Music, NewValue);
	}
}

void SUTSystemSettingsDialog::OnSoundVolumeChangedSFX(float NewValue)
{
	// Temporarily change audio level
	// This should play a sample SFX sound
	UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
	if (AudioSettings)
	{
		AudioSettings->SetSoundClassVolume(EUTSoundClass::SFX, NewValue);
	}
}

void SUTSystemSettingsDialog::OnSoundVolumeChangedVoice(float NewValue)
{
	// Temporarily change audio level
	// This should play a sample Voice sound
	UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
	if (AudioSettings)
	{
		AudioSettings->SetSoundClassVolume(EUTSoundClass::Voice, NewValue);
	}
}

FReply SUTSystemSettingsDialog::OnTabClickGeneral()
{
	TabWidget->SetActiveWidgetIndex(0);
	GeneralSettingsTabButton->BePressed();
	GraphicsSettingsTabButton->UnPressed();
	AudioSettingsTabButton->UnPressed();
	return FReply::Handled();
}

FReply SUTSystemSettingsDialog::OnTabClickGraphics()
{
	TabWidget->SetActiveWidgetIndex(1);
	GeneralSettingsTabButton->UnPressed();
	GraphicsSettingsTabButton->BePressed();
	AudioSettingsTabButton->UnPressed();
	return FReply::Handled();
}

FReply SUTSystemSettingsDialog::OnTabClickAudio()
{
	TabWidget->SetActiveWidgetIndex(2);
	GeneralSettingsTabButton->UnPressed();
	GraphicsSettingsTabButton->UnPressed();
	AudioSettingsTabButton->BePressed();
	return FReply::Handled();
}

FString SUTSystemSettingsDialog::GetScreenPercentageLabelText(float SliderValue)
{
	// Increments of 5, so divide by 5 and multiply by 5
	int32 ScreenPercentage = FMath::TruncToInt(SliderValue * (ScreenPercentageRange.Y - ScreenPercentageRange.X) + ScreenPercentageRange.X) / 5 * 5;
	return FText::Format(NSLOCTEXT("SUTPlayerSettingsDialog", "ScreenPercentage", "Screen Percentage ({Value}%)"), FText::FromString(FString::Printf(TEXT("%i"), ScreenPercentage))).ToString();
}

void SUTSystemSettingsDialog::OnScreenPercentageChange(float NewValue)
{
	ScreenPercentageLabel->SetText(GetScreenPercentageLabelText(NewValue));
}

FString SUTSystemSettingsDialog::GetDecalLifetimeLabelText(float SliderValue)
{
	if (SliderValue == 1.0f)
	{
		return NSLOCTEXT("SUTPlayerSettingsDialog", "DecalLifetimeInf", "Decal Lifetime (INF)").ToString();
	}
	
	int32 NewDecalLifetime = FMath::TruncToInt(SliderValue * (DecalLifetimeRange.Y - DecalLifetimeRange.X) + DecalLifetimeRange.X);
	return FText::Format(NSLOCTEXT("SUTPlayerSettingsDialog", "DecalLifetime", "Decal Lifetime ({Value} seconds)"), FText::FromString(FString::Printf(TEXT("%i"), NewDecalLifetime))).ToString();
}

void SUTSystemSettingsDialog::OnDecalLifetimeChange(float NewValue)
{
	DecalLifetimeLabel->SetText(GetDecalLifetimeLabelText(NewValue));
}

void SUTSystemSettingsDialog::OnSettingsAutodetected(const Scalability::FQualityLevels& DetectedQuality)
{
	TextureRes->SetSelectedItem(GeneralScalabilityList[DetectedQuality.TextureQuality]);
	ShadowQuality->SetSelectedItem(GeneralScalabilityList[DetectedQuality.ShadowQuality]);
	EffectQuality->SetSelectedItem(GeneralScalabilityList[DetectedQuality.EffectsQuality]);
	PPQuality->SetSelectedItem(GeneralScalabilityList[DetectedQuality.PostProcessQuality]);

	int32 AAModeInt = UUTGameUserSettings::ConvertAAScalabilityQualityToAAMode(DetectedQuality.AntiAliasingQuality);
	int32 AAModeSelection = ConvertAAModeToComboSelection(AAModeInt);
	AAMode->SetSelectedItem(AAModeList[AAModeSelection]);

	int32 ScreenPercentage = DetectedQuality.ResolutionQuality;
	ScreenPercentage = int32(FMath::Clamp(float(ScreenPercentage), ScreenPercentageRange.X, ScreenPercentageRange.Y));
	float ScreenPercentageSliderSetting = (float(ScreenPercentage) - ScreenPercentageRange.X) / (ScreenPercentageRange.Y - ScreenPercentageRange.X);
	ScreenPercentageSlider->SetValue(ScreenPercentageSliderSetting);
	ScreenPercentageLabel->SetText(GetScreenPercentageLabelText(ScreenPercentageSliderSetting));
}

FReply SUTSystemSettingsDialog::OnAutodetectClick()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	UUTLocalPlayer* LocalPlayer = GetPlayerOwner().Get();
	if (ensure(LocalPlayer) && !UserSettings->bBenchmarkInProgress)
	{
		UserSettings->BenchmarkDetailSettings(LocalPlayer, false);

		UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
		if (UTEngine != NULL)
		{
			int32 RefreshRate;
			if (UTEngine->GetMonitorRefreshRate(RefreshRate))
			{
				int32 AutoDetectedFramerateCap = 120;
				if (RefreshRate < 120)
				{
					AutoDetectedFramerateCap = 60;
				}
				FrameRateCap->SetText(FText::AsNumber(AutoDetectedFramerateCap));
			}
		}
	}

	return FReply::Handled();
}

FReply SUTSystemSettingsDialog::OKClick()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	// sound settings
	for (int32 i = 0; i < ARRAY_COUNT(SoundVolumes); i++)
	{
		UserSettings->SetSoundClassVolume(EUTSoundClass::Type(i), SoundVolumes[i]->GetValue());
	}
	// engine scalability
	UserSettings->ScalabilityQuality = Scalability::GetQualityLevels(); // sets in UserSettings what was already applied
	Scalability::SaveState(GGameUserSettingsIni); // note: settings were applied previously on change of individual items
	for (TSharedRef<SSlateConsoleVarDelegate> CVar : CVarDelegates)
	{
		GConfig->SetString(TEXT("ConsoleVariables"), CVar->GetVarName(), *CVar->GetString(), GEngineIni);
	}
	GConfig->Flush(false, GEngineIni);
	// resolution
	int32 NewDisplayMode = DisplayModeList.Find(DisplayModeComboBox->GetSelectedItem());
	TArray<FString> Suffixes;
	Suffixes.Add(FString("f"));
	Suffixes.Add(FString("wf"));
	Suffixes.Add(FString("w"));
	GetPlayerOwner()->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("setres %s%s"), *SelectedRes->GetText().ToString(), *Suffixes[NewDisplayMode]));

	UserSettings->SetBotSpeech(EBotSpeechOption(FMath::Max<int32>(0, BotSpeechList.Find(BotSpeechCombo->GetSelectedItem()))));
	UserSettings->SetHRTFEnabled(HRTFCheckBox->IsChecked());
	UserSettings->SetAAMode(ConvertComboSelectionToAAMode(*AAMode->GetSelectedItem().Get()));

	// Increments of 5, so divide by 5 and multiply by 5
	int32 NewScreenPercentage = FMath::TruncToInt(ScreenPercentageSlider->GetValue() * (ScreenPercentageRange.Y - ScreenPercentageRange.X) + ScreenPercentageRange.X) / 5 * 5;
	UserSettings->SetScreenPercentage(NewScreenPercentage);

	const TCHAR* Cmd = *SelectedRes->GetText().ToString();
	int32 X=FCString::Atoi(Cmd);
	const TCHAR* CmdTemp = FCString::Strchr(Cmd,'x') ? FCString::Strchr(Cmd,'x')+1 : FCString::Strchr(Cmd,'X') ? FCString::Strchr(Cmd,'X')+1 : TEXT("");
	int32 Y=FCString::Atoi(CmdTemp);
	UserSettings->SetScreenResolution(FIntPoint(X, Y));
	UserSettings->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(NewDisplayMode));
	UserSettings->SetVSyncEnabled(VSync->IsChecked());
	UserSettings->SaveConfig();

	// Immediately change the vsync, UserSettings would do it, but it's in a function that we don't typically call
	static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	CVar->Set(VSync->IsChecked(), ECVF_SetByGameSetting);

	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine == NULL) // PIE
	{
		UTEngine = UUTGameEngine::StaticClass()->GetDefaultObject<UUTGameEngine>();
	}
	if (FrameRateCap->GetText().ToString().IsNumeric())
	{
		UTEngine->FrameRateCap = FCString::Atoi(*FrameRateCap->GetText().ToString());
	}
	GEngine->bSmoothFrameRate = SmoothFrameRate->IsChecked();
	GEngine->SaveConfig();
	UTEngine->SaveConfig();

	// impact effect lifetime - note that 1.0 on the slider is infinite lifetime
	float NewDecalLifetime = (DecalLifetime->GetValue() * (DecalLifetimeRange.Y - DecalLifetimeRange.X) + DecalLifetimeRange.X);
	AUTWorldSettings* DefaultWS = AUTWorldSettings::StaticClass()->GetDefaultObject<AUTWorldSettings>();
	DefaultWS->MaxImpactEffectVisibleLifetime = NewDecalLifetime;
	DefaultWS->MaxImpactEffectInvisibleLifetime = NewDecalLifetime * 0.5f;
	DefaultWS->SaveConfig();
	if (GetPlayerOwner()->PlayerController != NULL)
	{
		AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetPlayerOwner()->PlayerController->GetWorld()->GetWorldSettings());
		if (WS != NULL)
		{
			WS->MaxImpactEffectVisibleLifetime = DefaultWS->MaxImpactEffectVisibleLifetime;
			WS->MaxImpactEffectInvisibleLifetime = DefaultWS->MaxImpactEffectInvisibleLifetime;
		}
	}
	
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUTSystemSettingsDialog::CancelClick()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	// revert sound settings to default
	for (int32 i = 0; i < ARRAY_COUNT(SoundVolumes); i++)
	{
		UserSettings->SetSoundClassVolume(EUTSoundClass::Type(i), UserSettings->GetSoundClassVolume(EUTSoundClass::Type(i)));
	}
	// revert cvars
	for (TSharedRef<SSlateConsoleVarDelegate> CVar : CVarDelegates)
	{
		CVar->RestoreValue();
	}

	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

void SUTSystemSettingsDialog::OnResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedRes->SetText(*NewSelection.Get());
}
void SUTSystemSettingsDialog::OnDisplayModeSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedDisplayMode->SetText(*NewSelection.Get());
}
void SUTSystemSettingsDialog::OnTextureResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	Scalability::FQualityLevels ScalabilityQuality = Scalability::GetQualityLevels();
	ScalabilityQuality.TextureQuality = GeneralScalabilityList.Find(TextureRes->GetSelectedItem());
	Scalability::SetQualityLevels(ScalabilityQuality);
	SelectedTextureRes->SetText(*NewSelection.Get());
}
void SUTSystemSettingsDialog::OnShadowQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	Scalability::FQualityLevels ScalabilityQuality = Scalability::GetQualityLevels();
	ScalabilityQuality.ShadowQuality = GeneralScalabilityList.Find(ShadowQuality->GetSelectedItem());
	Scalability::SetQualityLevels(ScalabilityQuality);
	SelectedShadowQuality->SetText(*NewSelection.Get());
}
void SUTSystemSettingsDialog::OnPPQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	Scalability::FQualityLevels ScalabilityQuality = Scalability::GetQualityLevels();
	ScalabilityQuality.PostProcessQuality = GeneralScalabilityList.Find(PPQuality->GetSelectedItem());
	Scalability::SetQualityLevels(ScalabilityQuality);
	SelectedPPQuality->SetText(*NewSelection.Get());
}
void SUTSystemSettingsDialog::OnEffectQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	Scalability::FQualityLevels ScalabilityQuality = Scalability::GetQualityLevels();
	ScalabilityQuality.EffectsQuality = GeneralScalabilityList.Find(EffectQuality->GetSelectedItem());
	Scalability::SetQualityLevels(ScalabilityQuality);
	SelectedEffectQuality->SetText(*NewSelection.Get());
}
void SUTSystemSettingsDialog::OnAAModeSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedAAMode->SetText(*NewSelection.Get());
}

int32 SUTSystemSettingsDialog::ConvertAAModeToComboSelection(int32 NewAAMode)
{
	// 0:off, 1:very low (faster FXAA), 2:low (FXAA), 3:medium (faster TemporalAA), 4:high (default TemporalAA)
	if (NewAAMode == 0)
	{
		return 0;
	}
	else if (NewAAMode == 1 || NewAAMode == 2)
	{
		return 1;
	}
	else
	{
		return 2;
	}
}

int32 SUTSystemSettingsDialog::ConvertComboSelectionToAAMode(const FString& Selection)
{
	// 0:off, 1:very low (faster FXAA), 2:low (FXAA), 3:medium (faster TemporalAA), 4:high (default TemporalAA)
	if (Selection == *AAModeList[0].Get())
	{
		return 0;
	}
	else if (Selection == *AAModeList[1].Get())
	{
		return 2;
	}
	else
	{
		return 4;
	}
}

FReply SUTSystemSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();
}

FText SUTSystemSettingsDialog::GetVSyncText() const
{
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	int32 RefreshRate = 0;
	if (UTEngine && UTEngine->GetMonitorRefreshRate(RefreshRate))
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("RefreshRate"), RefreshRate);
		return FText::Format(NSLOCTEXT("SUTSystemSettingsDialog", "VSync", "VSync (Monitor {RefreshRate}hz)"), Arguments);
	}

	return NSLOCTEXT("SUTSystemSettingsDialog", "VSync", "VSync");
}

#endif