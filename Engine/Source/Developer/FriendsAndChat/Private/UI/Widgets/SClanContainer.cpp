// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SClanContainer.h"
#include "SClanDetails.h"
#include "SClanHome.h"
#include "SWidgetSwitcher.h"
#include "ClanViewModel.h"

#define LOCTEXT_NAMESPACE ""

/**
 * Declares the Clan display widget
*/
class SClanContainerImpl : public SClanContainer
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FClanViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;
		MenuMethod = InArgs._Method;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(Switcher, SWidgetSwitcher)
				+SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SButton)
						.Text(FText::FromString("Details"))
						.OnClicked(this, &SClanContainerImpl::HandleWidgetSwitcherClicked, 1)
					]
					+SVerticalBox::Slot()
					[
						SNew(SClanHome, ViewModel.ToSharedRef())
						.FriendStyle(&FriendStyle)
					]
				]
				+SWidgetSwitcher::Slot()
				[
					SNew(SClanDetails, ViewModel->GetClanInfoViewModel().ToSharedRef())
					.FriendStyle(&FriendStyle)
					.OnHomeClicked(this, &SClanContainerImpl::HandleWidgetSwitcherClicked, 0)
				]
			]
		]);
	}

private:

	FReply HandleWidgetSwitcherClicked(int32 Index)
	{
		Switcher->SetActiveWidgetIndex(Index);
		return FReply::Handled();
	}
private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	TSharedPtr<FClanViewModel> ViewModel;

	// Holds the list container switcher
	TSharedPtr<SWidgetSwitcher> Switcher;

	EPopupMethod MenuMethod;
};

TSharedRef<SClanContainer> SClanContainer::New()
{
	return MakeShareable(new SClanContainerImpl());
}

#undef LOCTEXT_NAMESPACE
