// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTFriendsWidget.h"
#include "../SUWindowsStyle.h"
#include "Social.h"

#if !UE_SERVER

void SUTFriendsWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx)
{
	Ctx = InCtx;

	//grab the OnClose del
	OnCloseDelegate = InArgs._OnClose;

	ChildSlot
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.WidthOverride(380)
			.HeightOverride(1000)
			[
				SAssignNew(ContentWidget, SWeakWidget)
			]
		];
	ContentWidget->SetContent(ISocialModule::Get().GetFriendsAndChatManager()->GenerateFriendsListWidget(InArgs._FriendStyle).ToSharedRef());
}

SUTFriendsWidget::~SUTFriendsWidget()
{
}

FReply SUTFriendsWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	//if (InKeyEvent.GetKey() == EKeys::Escape)
	//{
	//	OnClose();
	//}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SUTFriendsWidget::OnClose()
{
	if (OnCloseDelegate.IsBound())
	{
		OnCloseDelegate.Execute();
	}

	return FReply::Handled();
}

#endif