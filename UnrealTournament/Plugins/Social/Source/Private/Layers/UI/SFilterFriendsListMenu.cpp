// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFilterFriendsListMenu.h"
#include "FriendContainerViewModel.h"

#define LOCTEXT_NAMESPACE ""

class SFilterFriendsListMenuImpl : public SFilterFriendsListMenu
{
public:

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendContainerViewModel>& InFriendsViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		FriendsViewModel = InFriendsViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground)
			.BorderBackgroundColor(InArgs._BorderColorAndOpacity)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					// Header and back button
					SNew(SButton)
					.ButtonStyle(&FriendStyle.FriendsListStyle.FriendListActionButtonStyle)
					.ButtonColorAndOpacity(InArgs._ColorAndOpacity)
					.ContentPadding(20)
					.OnClicked(this, &SFilterFriendsListMenuImpl::HandleBackButtonClicked)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(5, 0)
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("FilterList", "Filter List"))
							.Font(FriendStyle.FriendsLargeFontStyle.FriendsFontNormalBold)
							.ColorAndOpacity(InArgs._FontColorAndOpacity)
						]
						+ SHorizontalBox::Slot()
						.Padding(5, 0)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(&FriendStyle.FriendsListStyle.BackBrush)
							.ColorAndOpacity(InArgs._ColorAndOpacity)
						]
					]
				]
				+ SVerticalBox::Slot()
				.Padding(0, 10)
				.AutoHeight()
				[
					SAssignNew(FilterTextBox, SEditableTextBox)
					.HintText(LOCTEXT("Filter Friends Hint", "Filter"))
					.Style(&FriendStyle.FriendsListStyle.AddFriendEditableTextStyle)
					.Font(FriendStyle.FriendsSmallFontStyle.FriendsFontNormal)
					.OnTextCommitted(this, &SFilterFriendsListMenuImpl::HandleFilterTextEntered)
					.OnTextChanged(this, &SFilterFriendsListMenuImpl::HandleFilterTextChanged)
					.BackgroundColor(InArgs._ColorAndOpacity)
					.ForegroundColor(InArgs._FontColorAndOpacity)
				]
			]
		]);
	}

	virtual void Initialize() override
	{
		FilterTextBox->SetText(FText::GetEmpty());
		FSlateApplication::Get().SetKeyboardFocus(FilterTextBox, EFocusCause::SetDirectly);
	}

private:

	DECLARE_DERIVED_EVENT(SAddFriendMenu, SFilterFriendsListMenu::FOnCloseEvent, FOnCloseEvent);
	virtual FOnCloseEvent& OnCloseEvent() override
	{
		return OnCloseDelegate;
	}

	DECLARE_DERIVED_EVENT(SAddFriendMenu, SFilterFriendsListMenu::FOnFilterEvent, FOnFilterEvent);
	virtual FOnFilterEvent& OnFilterEvent() override
	{
		return OnFilterDelegate;
	}

	void HandleFilterTextEntered(const FText& CommitText, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter)
		{
			OnFilterDelegate.Broadcast(CommitText);
			HandleBackButtonClicked();
		}
	}

	void HandleFilterTextChanged(const FText& CurrentText)
	{
		FString CurrentTextString = CurrentText.ToString();
		if (CurrentTextString.Len() > 254)
		{
			FilterTextBox->SetText(FText::FromString(CurrentTextString.Left(254)));
		}
	}

	FReply HandleBackButtonClicked()
	{
		OnCloseDelegate.Broadcast();
		return FReply::Handled();
	}

	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FFriendContainerViewModel> FriendsViewModel;
	FOnCloseEvent OnCloseDelegate;
	FOnFilterEvent OnFilterDelegate;

	// Holds the Friends add text box
	TSharedPtr< SEditableTextBox > FilterTextBox;
};

TSharedRef<SFilterFriendsListMenu> SFilterFriendsListMenu::New()
{
	return MakeShareable(new SFilterFriendsListMenuImpl());
}

#undef LOCTEXT_NAMESPACE
