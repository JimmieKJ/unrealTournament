// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendActions.h"
#include "FriendViewModel.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE "SFriendItem"

class SFriendActionsImpl : public SFriendActions
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendViewModel>& InFriendViewModel, const TSharedRef<class FFriendsFontStyleService>& InFontService)
	{
		FriendStyle = *InArgs._FriendStyle;
		FriendViewModel = InFriendViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SAssignNew(ActionContainer, SVerticalBox)
		]);


		TArray<EFriendActionType::Type> Actions;
		FriendViewModel->EnumerateActions(Actions, InArgs._FromChat, InArgs._DisplayChatOption);
		for (EFriendActionType::Type Action : Actions)
		{
			ActionContainer->AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.OnClicked(this, &SFriendActionsImpl::HandleActionClicked, Action)
				.ButtonStyle(&FriendStyle.ActionButtonStyle)
				.ContentPadding(FriendStyle.FriendsChatStyle.FriendActionPadding)
				[
					SNew(STextBlock)
					.Font(InFontService, &FFriendsFontStyleService::GetNormalFont)
					.ColorAndOpacity(FLinearColor::White)
					.Text(EFriendActionType::ToText(Action))
				]
			];
		}
	}

private:

	FReply HandleActionClicked(EFriendActionType::Type ActionType)
	{
		FriendViewModel->PerformAction(ActionType);
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

private:

	TSharedPtr<FFriendViewModel> FriendViewModel;
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<SVerticalBox> ActionContainer;
};

TSharedRef<SFriendActions> SFriendActions::New()
{
	return MakeShareable(new SFriendActionsImpl());
}

#undef LOCTEXT_NAMESPACE
