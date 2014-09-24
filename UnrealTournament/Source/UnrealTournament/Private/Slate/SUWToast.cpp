// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWToast.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

void SUWToast::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	checkSlow(PlayerOwner != NULL);

	Lifetime = InArgs._Lifetime;
	InitialLifetime = Lifetime;

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)

			//Top Empty Section
			+ SVerticalBox::Slot()						
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)

			// The content section
			+ SVerticalBox::Slot()													
			.Padding(0.0f,0.05, 0.05, 15.0f)
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(300)
					.HeightOverride(75)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew( SImage )		
							.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Toast.Background"))
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(10.0f, 5.0f, 10.0f, 5.05)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(InArgs._ToastText)
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Toast.TextStyle")
								.AutoWrapText(true)
							]
						]
					]
				]
			]
		];

}

void SUWToast::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Track the change
	Lifetime -= InDeltaTime;

	float FadeTime = InitialLifetime * 0.2;
	float Alpha = 1.0f;
	// Check to see if we should be fading

	if (Lifetime > InitialLifetime - FadeTime)
	{
		Alpha = (InitialLifetime - Lifetime) / FadeTime;
	}
	else if (Lifetime <= FadeTime)
	{
		Alpha = Lifetime / FadeTime;
	}

	FLinearColor DrawColor = FLinearColor::White;
	DrawColor.A = Alpha;
	SetColorAndOpacity(DrawColor);

	if (Lifetime <= 0.0f)
	{
		PlayerOwner->ToastCompleted();
	}
}

bool SUWToast::SupportsKeyboardFocus() const
{
	return false;
}



#endif