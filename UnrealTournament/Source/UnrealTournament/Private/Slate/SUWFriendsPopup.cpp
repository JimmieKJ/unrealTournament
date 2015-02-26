// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWFriendsPopup.h"
#include "Widgets/SUTChatWidget.h"
#include "Widgets/SUTFriendsWidget.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

void SUWFriendsPopup::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	checkSlow(PlayerOwner != NULL);
	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(CloseButton, SButton)
				.ButtonColorAndOpacity(FLinearColor::Transparent)
			]
			+SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
				[
					SNew(SUTChatWidget, PlayerOwner->PlayerController)
				]
			]
			+SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(15.0f, 30.0f))
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
				[
					SNew(SUTFriendsWidget, PlayerOwner->PlayerController)
				]
			]

		];
}

void SUWFriendsPopup::SetOnCloseClicked(FOnClicked InOnClicked)
{
	CloseButton->SetOnClicked(InOnClicked);
}

/******************** ALL OF THE HACKS NEEDED TO MAINTAIN WINDOW FOCUS *********************************/
/*
bool SUWFriendsPopup::SupportsKeyboardFocus() const
{
	return true;
}

FReply SUWFriendsPopup::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent)
{
	return FReply::Handled()
		.ReleaseMouseCapture()
		.LockMouseToWidget(SharedThis(this));

}

FReply SUWFriendsPopup::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return FReply::Unhandled();
}

FReply SUWFriendsPopup::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}


FReply SUWFriendsPopup::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SUWFriendsPopup::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}


FReply SUWFriendsPopup::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return FReply::Handled();
}
*/

#endif