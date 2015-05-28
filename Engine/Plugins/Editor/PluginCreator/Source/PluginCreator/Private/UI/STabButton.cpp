// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginCreatorPrivatePCH.h"
#include "STabButton.h"

#include "PluginCreatorStyle.h"

void STabButton::Construct(const FArguments& InArgs)
{
	ActiveTopbarColor = FLinearColor(0.9f, 0.45f, 0.06f);
	InactiveTopbarColor = FLinearColor(1.f, 1.f, 1.f, 0.0f);

	IsActive = InArgs._OnIsActive;
	OnButtonClicked = InArgs._OnButtonClicked;

	TSharedRef<SWidget> ButtonWidget = SNew(SButton)
		.ForegroundColor(FCoreStyle::Get().GetSlateColor("Foreground"))
		.Text(InArgs._ButtonLabel)
		.TextStyle(FPluginCreatorStyle::Get(), TEXT("TabButton.Text"))
		.ButtonStyle(FPluginCreatorStyle::Get(), TEXT("NoBorderButton"))
		.ContentPadding(FMargin(15, 5))
		.OnClicked(OnButtonClicked);


	this->ChildSlot
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderImage(FPluginCreatorStyle::Get().GetBrush(TEXT("NoBorder")))
			[

				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.Padding(0)
						.BorderImage(this, &STabButton::GetButtonBorderImage)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()

							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SBox)
									.HeightOverride(3)
									[
										SNew(SColorBlock)
										.Color(this, &STabButton::GetTopbarColor)
									]
								]
								+ SVerticalBox::Slot()
									.AutoHeight()
									[
										ButtonWidget
									]
							]
						]
					]
				]
			]
		];
}


FLinearColor STabButton::GetTopbarColor() const
{
	if (IsActive.IsBound() && IsActive.Execute())
	{
		return ActiveTopbarColor;
	}

	return InactiveTopbarColor;
}

const FSlateBrush* STabButton::GetButtonBorderImage() const
{
	if (IsActive.IsBound() && IsActive.Execute())
	{
		return FPluginCreatorStyle::Get().GetBrush(TEXT("DarkGrayBackground"));
	}

	return FPluginCreatorStyle::Get().GetBrush(TEXT("NoBorder"));
}