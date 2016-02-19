// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTMenuBase.h"
#include "SUTQuickChatWindow.h"
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTGameViewportClient.h"
#include "../Widgets/SUTChatBar.h"
#include "../Widgets/SUTChatEditBox.h"

#if !UE_SERVER

void SUTQuickChatWindow::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	PlayerOwner = InPlayerOwner;
	ChatDestination = InArgs._InitialChatDestination;

	SUTWindowBase::Construct
	(
		SUTWindowBase::FArguments()
			.Size(FVector2D(1.0f,1.0f))
			.bSizeIsRelative(true)
			.Position(FVector2D(0.5f, 0.5f))
			.AnchorPoint(FVector2D(0.5f, 0.5f))
			.bShadow(true)
			.ShadowAlpha(0.5f)
		, PlayerOwner

	);
}

void SUTQuickChatWindow::BuildWindow()
{
	Content->AddSlot().VAlign(VAlign_Fill).HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot().FillHeight(1.0)
		[
			SNew(SCanvas)
		]
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(42)
			[
				SAssignNew(ChatBar, SUTChatBar, PlayerOwner)
				.OnTextCommitted(this, &SUTQuickChatWindow::ChatTextCommited)
				.InitialChatDestination(ChatDestination)
				
			]
		]
	];
}


bool SUTQuickChatWindow::SupportsKeyboardFocus() const
{
	return true;
}


FReply SUTQuickChatWindow::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent )
{
	PlayerOwner->GetSlateOperations() = FReply::Handled().ReleaseMouseCapture().SetUserFocus(PlayerOwner->GetChatWidget().ToSharedRef(), EFocusCause::SetDirectly);
	return FReply::Handled();
}

FReply SUTQuickChatWindow::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		PlayerOwner->CloseQuickChat();
	}

	return FReply::Unhandled();
}

FReply SUTQuickChatWindow::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	PlayerOwner->CloseQuickChat();
	return FReply::Handled();
}

void SUTQuickChatWindow::ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		PlayerOwner->CloseQuickChat();
	}
}


#endif