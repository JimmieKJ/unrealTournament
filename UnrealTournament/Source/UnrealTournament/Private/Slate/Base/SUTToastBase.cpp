// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTToastBase.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTToastBase::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	checkSlow(PlayerOwner != NULL);

	Lifetime = InArgs._Lifetime;
	InitialLifetime = Lifetime;

	SetVisibility(EVisibility::HitTestInvisible);

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			BuildToast(InArgs)
		];
}

TSharedRef<SWidget> SUTToastBase::BuildToast(const FArguments& InArgs)
{
	return SNew(SVerticalBox)
	
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
			.HeightOverride(130)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew( SImage )		
					.Image(SUWindowsStyle::Get().GetBrush("UT.Toast.Background"))
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(16.0f, 16.0f, 16.0f, 16.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(InArgs._ToastText)
							.TextStyle(SUWindowsStyle::Get(), "UT.Toast.TextStyle")
						]
					]
				]
			]
		]
	];

}

void SUTToastBase::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Track the change
	Lifetime -= InDeltaTime;

	float FadeTime = 0.5; // InitialLifetime * 0.2;
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

bool SUTToastBase::SupportsKeyboardFocus() const
{
	return false;
}



#endif