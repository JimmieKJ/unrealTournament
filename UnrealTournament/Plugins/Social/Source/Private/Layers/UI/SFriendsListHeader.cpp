// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendsListHeader.h"

#include "FriendListViewModel.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE ""

/**
 * Declares the Friends List display widget
*/
class SFriendsListHeaderImpl : public SFriendsListHeader
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService>& InFontService, const TSharedRef<FFriendListViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;
		FontService = InFontService;
		FFriendListViewModel* ViewModelPtr = ViewModel.Get();

		bHighlighted = false;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SAssignNew(HeaderButton, SButton)
			.Visibility(ViewModelPtr, &FFriendListViewModel::GetListVisibility)
			.OnClicked(this, &SFriendsListHeaderImpl::HandleShowFriendsClicked)
			.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
			.ContentPadding(FMargin(0))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Visibility(this, &SFriendsListHeaderImpl::GetHiglightVisibility)
					.Image(this, &SFriendsListHeaderImpl::GetButtonPressedColor)
				]
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(FriendStyle.FriendsListStyle.FriendsListHeaderMargin)
					[
						SNew(STextBlock)
						.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendsListHeaderImpl::GetButtonContentColor)))
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
						.Text(ViewModel->GetListName())
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.Padding(FriendStyle.FriendsListStyle.FriendsListHeaderCountMargin)
					[
						SNew(STextBlock)
						.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendsListHeaderImpl::GetButtonContentColor)))
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
						.Text(ViewModelPtr, &FFriendListViewModel::GetListCountText)
					]
				]
			]
		]);
	}

	DECLARE_DERIVED_EVENT(SFriendsListContainer, SFriendsListHeader::FOnFriendsListVisibilityChanged, FOnFriendsListVisibilityChanged)
	virtual FOnFriendsListVisibilityChanged& OnVisibilityChanged() override
	{
		return OnVisibilityChangedDelegate;
	}

	virtual void SetHighlighted(bool Highlighted) override
	{
		bHighlighted = Highlighted;
	}

private:


	FSlateColor GetButtonContentColor()
	{
		if (HeaderButton->IsHovered() || bHighlighted)
		{
			return FriendStyle.FriendsListStyle.ButtonHoverContentColor;
		}
		return FriendStyle.FriendsListStyle.ButtonContentColor;
	}

	const FSlateBrush* GetButtonPressedColor() const
	{
		if (HeaderButton->IsHovered())
		{
			return &FriendStyle.FriendsListStyle.HeaderButtonStyle.Hovered;
		}
		return &FriendStyle.FriendsListStyle.HeaderButtonStyle.Pressed;
	}

	EVisibility GetHiglightVisibility() const
	{
		return bHighlighted ? EVisibility::Visible : EVisibility::Hidden;
	}

	FReply HandleShowFriendsClicked()
	{
		OnVisibilityChanged().Broadcast(ViewModel->GetListType());
		return FReply::Handled();
	}

private:

	bool bHighlighted;

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	// Holds the style to use when making the widget.
	FFriendsAndChatStyle FriendStyle;

	// Holds the view model of the list
	TSharedPtr<FFriendListViewModel> ViewModel;

	// Visibility Delegate
	FOnFriendsListVisibilityChanged OnVisibilityChangedDelegate;

	TSharedPtr<SButton> HeaderButton;
};

TSharedRef<SFriendsListHeader> SFriendsListHeader::New()
{
	return MakeShareable(new SFriendsListHeaderImpl());
}

#undef LOCTEXT_NAMESPACE
