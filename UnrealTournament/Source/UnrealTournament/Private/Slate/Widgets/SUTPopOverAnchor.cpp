// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTPopOverAnchor.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"

#if !UE_SERVER

void SUTPopOverAnchor::Construct(const FArguments& InArgs)
{
	OnGetPopoverWidget = InArgs._OnGetPopoverWidget;
	bWaitingToPopup = false;
	TSharedPtr<SHorizontalBox> HBox;
	SMenuAnchor::Construct( SMenuAnchor::FArguments()
		.Placement( InArgs._MenuPlacement )
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.FillWidth( 1 )
			[
				InArgs._Content.Widget
			]
		]
	);
}

void SUTPopOverAnchor::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{

	if (bWaitingToPopup && GWorld->GetRealTimeSeconds() - PopupStartTime > 0.33)
	{
		Popup();
	}

	SMenuAnchor::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

}

void SUTPopOverAnchor::Popup()
{
	bWaitingToPopup = false;
	if (!IsOpen() && OnGetPopoverWidget.IsBound())
	{
		MenuContent = 
			SNew(SBorder)
			.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
			[
				OnGetPopoverWidget.Execute(SharedThis(this))
			];

		SetIsOpen( true, true);	
	}
}

void SUTPopOverAnchor::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bWaitingToPopup = true;
	PopupStartTime = GWorld->GetRealTimeSeconds();
}

void SUTPopOverAnchor::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bWaitingToPopup = false;
	if (IsOpen())
	{
		SetIsOpen( false, false);
	}

}



#endif