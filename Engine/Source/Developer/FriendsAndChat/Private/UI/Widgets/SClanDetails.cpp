// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsListContainer.h"
#include "SClanDetails.h"
#include "ClanInfoViewModel.h"

#define LOCTEXT_NAMESPACE ""

/**
 * Declares the Clan details widget
*/
class SClanDetailsImpl : public SClanDetails
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FClanInfoViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;
		FClanInfoViewModel* ViewModelPtr = ViewModel.Get();
		OnHomeClicked = InArgs._OnHomeClicked;

		ExternalScrollbar = SNew(SScrollBar)
		.Thickness(FVector2D(4, 4))
		.Style(&FriendStyle.ScrollBarStyle)
		.AlwaysShowScrollbar(true);

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(SButton)
					.Text(FText::FromString("Back"))
					.OnClicked(this, &SClanDetailsImpl::HandleHomeClicked)
				]
				+SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(ViewModel->GetClanTitle())
				]
				+SHorizontalBox::Slot()
				.Padding(15, 0)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(ViewModelPtr, &FClanInfoViewModel::GetListCountText)
				]
			]
			+SVerticalBox::Slot()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SScrollBox)
					.ExternalScrollbar(ExternalScrollbar.ToSharedRef())
					+ SScrollBox::Slot()
					.Padding(FMargin(10, 10, 30, 0))
					[
						SNew(SFriendsListContainer, ViewModel->GetFriendListViewModel())
						.FriendStyle(&FriendStyle)
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.Padding(FMargin(10, 20, 10, 20))
				[
					ExternalScrollbar.ToSharedRef()
				]
			]
		]);
	}

private:
	FReply HandleHomeClicked()
	{
		FReply Reply = FReply::Unhandled();
		Reply = OnHomeClicked.IsBound() ? OnHomeClicked.Execute() : FReply::Handled();
		return Reply;
	}

private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<SScrollBar> ExternalScrollbar;
	TSharedPtr<FClanInfoViewModel> ViewModel;
	FOnClicked OnHomeClicked;
};

TSharedRef<SClanDetails> SClanDetails::New()
{
	return MakeShareable(new SClanDetailsImpl());
}

#undef LOCTEXT_NAMESPACE
