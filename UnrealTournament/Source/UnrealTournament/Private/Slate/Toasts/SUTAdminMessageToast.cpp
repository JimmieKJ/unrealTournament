// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTAdminMessageToast.h"
#include "../SUTStyle.h"


#if !UE_SERVER

TSharedRef<SWidget> SUTAdminMessageToast::BuildToast(const FArguments& InArgs)
{
	TSharedPtr<SVerticalBox> VBox;
	SAssignNew(VBox, SVerticalBox)
		+SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(FMargin(0.0,64.0,0.0,0.0))
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox).HeightOverride(64)
								[
									SNew(SCanvas)
								]
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.Padding(FMargin(24.0,24.0,24.0,5.0))
								.AutoWidth()
								[
									SNew(SBorder)
									.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
									[
										SNew(SBox).WidthOverride(900)
										[
											SNew(STextBlock)
											.Text(InArgs._ToastText)
											.AutoWrapText(true)
											.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
										]
									]
								]
							]
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(16)
							[
								SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(FMargin(0.0,0.0,5.0,0.0))
									[
										SNew(SBox).WidthOverride(48).HeightOverride(48)
										[
											SNew(SImage)
											.Image(SUTStyle::Get().GetBrush("UT.Icon.Alert"))
										]
									]
									+SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTAdminMessageToast","Title","The Admin Says...."))
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
									]

								]
							]
						]
					]
				]
			]		
		];

	return VBox.ToSharedRef();
}


#endif