// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SConfirmation.h"
#include "FriendContainerViewModel.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE ""

class SConfirmationImpl : public SConfirmation
{
public:

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendContainerViewModel>& InContainerViewModel, const TSharedRef<FFriendsFontStyleService>& InFontService)
	{
		FontService = InFontService;
		FriendStyle = *InArgs._FriendStyle;
		ContainerViewModel = InContainerViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground)
			.BorderBackgroundColor(FLinearColor(0, 0, 0, 0.8f))
			.Padding(FriendStyle.FriendsListStyle.ConfirmationBorderMargin)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(this, &SConfirmationImpl::GetDescription)
					.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SVerticalBox::Slot()
				.Padding(0, 60)
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(FriendStyle.FriendsListStyle.ConfirmationButtonMargin)
					[
						SNew(SButton)
						.ButtonStyle(&FriendStyle.FriendsListStyle.ConfirmButtonStyle)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &SConfirmationImpl::HandleConfirmButtonClicked)
						.ContentPadding(FriendStyle.FriendsListStyle.ConfirmationButtonContentMargin)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Yes", "Yes"))
							.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
							.ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SHorizontalBox::Slot()
					.Padding(FriendStyle.FriendsListStyle.ConfirmationButtonMargin)
					[
						SNew(SButton)
						.ButtonStyle(&FriendStyle.FriendsListStyle.CancelButtonStyle)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &SConfirmationImpl::HandleCancelButtonClicked)
						.ContentPadding(FriendStyle.FriendsListStyle.ConfirmationButtonContentMargin)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Cancel", "Cancel"))
							.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
							.ColorAndOpacity(FLinearColor::White)
						]
					]
				]
			]
		]);
	}

private:

	FText GetDescription() const
	{
		return ContainerViewModel->GetConfirmDescription();
	}

	FReply HandleCancelButtonClicked()
	{
		ContainerViewModel->OnCancel();
		return FReply::Handled();
	}

	FReply HandleConfirmButtonClicked()
	{
		ContainerViewModel->OnConfirm();
		return FReply::Handled();
	}

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FFriendContainerViewModel> ContainerViewModel;
};

TSharedRef<SConfirmation> SConfirmation::New()
{
	return MakeShareable(new SConfirmationImpl());
}

#undef LOCTEXT_NAMESPACE
