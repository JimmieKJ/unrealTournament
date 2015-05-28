// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendRequest.h"
#include "FriendsViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendRequest"

/**
 * Declares the Friends List display widget
*/
class SFriendRequestImpl : public SFriendRequest
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;
		// Set up titles
		const FText SearchStringLabel = LOCTEXT( "FriendList_SearchLabel", "[Enter Display Name]" );

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Font(FriendStyle.FriendsFontStyle)
				.Text(FText::FromString("Send friend request to:"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SAssignNew(FriendNameTextBox, SEditableTextBox)
				.Font(FriendStyle.FriendsFontStyle)
				.OnTextCommitted(this, &SFriendRequestImpl::HandleFriendEntered)
				.BackgroundColor(FLinearColor::Black)
				.ForegroundColor(FLinearColor::White)
				.HintText(SearchStringLabel)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(5,0))
				.MinDesiredSlotWidth(80)
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(&FriendStyle.FriendGeneralButtonStyle)
					.OnClicked(this, &SFriendRequestImpl::HandleActionButtonClicked, true)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::White)
						.Font(FriendStyle.FriendsFontStyle)
						.Text(FText::FromString("Send"))
					]
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(&FriendStyle.FriendGeneralButtonStyle)
					.OnClicked(this, &SFriendRequestImpl::HandleActionButtonClicked, false)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::White)
						.Font(FriendStyle.FriendsFontStyle)
						.Text(FText::FromString("Cancel"))
					]
				]
			]
		]);
	}

private:

	FReply HandleActionButtonClicked(bool bPeformAction)
	{
		if ( FriendNameTextBox.IsValid() )
		{
			if (bPeformAction)
			{
				ViewModel->RequestFriend(FriendNameTextBox->GetText());
			}
			ViewModel->PerformAction();
			FriendNameTextBox->SetText(FText::GetEmpty());
		}
		return FReply::Handled();
	}

	void HandleFriendEntered(const FText& CommentText, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter)
		{
			HandleActionButtonClicked(true);
		}
	}

private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr< SEditableTextBox > FriendNameTextBox;
	TSharedPtr<FFriendsViewModel> ViewModel;
};

TSharedRef<SFriendRequest> SFriendRequest::New()
{
	return MakeShareable(new SFriendRequestImpl());
}

#undef LOCTEXT_NAMESPACE
