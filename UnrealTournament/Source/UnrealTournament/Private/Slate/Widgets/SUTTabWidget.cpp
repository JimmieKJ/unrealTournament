// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTTabWidget.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER
void SUTTabWidget::Construct(const FArguments& InArgs)
{
	OnTabButtonSelectionChanged = InArgs._OnTabButtonSelectionChanged;
	OnTabButtonNumberChanged = InArgs._OnTabButtonNumberChanged;

	TabsContainer = SNew(SWrapBox)
		.UseAllottedWidth(true);

	TabSwitcher = SNew(SWidgetSwitcher);

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(0.0f)
				.BorderImage(SUWindowsStyle::Get().GetBrush("UT.TopMenu.MidFill"))
				.Content()
				[
					TabsContainer.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			[
				TabSwitcher.ToSharedRef()
			]
		];
}

void SUTTabWidget::AddTab(FText ButtonLabel, TSharedPtr<SWidget> Widget)
{
	if (!ButtonLabel.IsEmpty() && Widget.IsValid())
	{
		TSharedPtr<SUTTabButton> Button = SNew(SUTTabButton)
			.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
			.ClickMethod(EButtonClickMethod::MouseDown)
			.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
			.Text(ButtonLabel)
			.OnClicked(this, &SUTTabWidget::OnButtonClicked, ButtonLabel);

		ButtonLabels.Add(ButtonLabel);
		TabButtons.Add(Button);

		//Auto select the first tab
		if (ButtonLabels.Num() == 1)
		{
			OnButtonClicked(ButtonLabel);
		}

		//Add the tab button and the widget to the switcher
		if (TabsContainer.IsValid())
		{
			TabsContainer->AddSlot()
			[
				Button.ToSharedRef()
			];
		}

		if (TabSwitcher.IsValid())
		{
			TabSwitcher->AddSlot()
			[
				Widget.ToSharedRef()
			];
		}
	}
}

FReply SUTTabWidget::OnButtonClicked(FText InLabel)
{
	if (!CurrentTabButton.EqualTo(InLabel))
	{
		int32 NewIndex = 0;
		CurrentTabButton = InLabel;

		for (int32 i = 0; i < TabButtons.Num(); i++)
		{
			if (ButtonLabels.IsValidIndex(i))
			{
				if (ButtonLabels[i].EqualTo(InLabel))
				{
					TabButtons[i]->BePressed();
					TabSwitcher->SetActiveWidgetIndex(i);
					NewIndex = i;
				}
				else
				{
					TabButtons[i]->UnPressed();
				}
			}
		}

		OnTabButtonSelectionChanged.ExecuteIfBound(CurrentTabButton);
		OnTabButtonNumberChanged.ExecuteIfBound(NewIndex);
	}
	return FReply::Handled();
}


#endif