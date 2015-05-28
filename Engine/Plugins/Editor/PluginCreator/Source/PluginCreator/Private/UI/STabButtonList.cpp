// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginCreatorPrivatePCH.h"
#include "STabButtonList.h"
#include "STabButton.h"

#define LOCTEXT_NAMESPACE "STabButtonList"

void STabButtonList::Construct(const FArguments& InArgs)
{
	ButtonLabels = InArgs._ButtonLabels;
	OnTabButtonSelectionChanged = InArgs._OnTabButtonSelectionChanged;

	if (ButtonLabels.Num() > 0)
	{
		CurrentTabButton = ButtonLabels[0];
	}

	TabsContainer = SNew(SWrapBox)
		.UseAllottedWidth(true);

	for (auto Label : ButtonLabels)
	{
		AddTabButton(Label);
	}

	ChildSlot
		[
			TabsContainer.ToSharedRef()
		];
}

void STabButtonList::AddTabButton(FText ButtonLabel)
{
	if (TabsContainer.IsValid())
	{
		TabsContainer->AddSlot()
			[
				SNew(STabButton)
				.ButtonLabel(ButtonLabel)
				.OnButtonClicked(this, &STabButtonList::OnButtonClicked, ButtonLabel)
				.OnIsActive(this, &STabButtonList::IsButtonActive, ButtonLabel)
			];
	}
}

FReply STabButtonList::OnButtonClicked(FText InLabel)
{
	if (!CurrentTabButton.EqualTo(InLabel))
	{
		CurrentTabButton = InLabel;

		OnTabButtonSelectionChanged.ExecuteIfBound(CurrentTabButton);
	}

	return FReply::Handled();
}

bool STabButtonList::IsButtonActive(FText InLabel)
{
	return CurrentTabButton.EqualTo(InLabel);
}

#undef LOCTEXT_NAMESPACE