// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUTFriendsWidget.h"
#include "../SUWindowsStyle.h"
#include "FriendsAndChat.h"

#if !UE_SERVER

void SUTFriendsWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx)
{
	Ctx = InCtx;

	//grab the OnClose del
	OnCloseDelegate = InArgs._OnClose;

	ChildSlot
		.HAlign(HAlign_Right)
		[
			SAssignNew(ContentWidget, SWeakWidget)
		];

	if (FModuleManager::Get().IsModuleLoaded(TEXT("FriendsAndChat")))
	{
		ContentWidget->SetContent(
			IFriendsAndChatModule::Get().GetFriendsAndChatManager()->GenerateFriendsListWidget(&SUWindowsStyle::Get().GetWidgetStyle< FFriendsAndChatStyle >("FriendsStyle")).ToSharedRef()
			);
	}
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