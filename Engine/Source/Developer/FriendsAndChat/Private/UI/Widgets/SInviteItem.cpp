// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SInviteItem.h"
#include "SFriendsToolTip.h"
#include "SFriendsList.h"
#include "FriendViewModel.h"

#define LOCTEXT_NAMESPACE "SInviteItem"

class SInviteItemImpl : public SInviteItem
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendViewModel>& ViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		this->ViewModel = ViewModel;
		FFriendViewModel* ViewModelPtr = this->ViewModel.Get();

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SButton)
			.ButtonStyle(&FriendStyle.FriendListItemButtonSimpleStyle)
			.ContentPadding(9.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(10, 0)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(&FriendStyle.FriendImageBrush)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(0, 3, 0, 0)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					[
						SNew(STextBlock)
						.Font(FriendStyle.FriendsFontStyleBold)
						.ColorAndOpacity(FriendStyle.DefaultFontColor)
						.Text(ViewModel->GetFriendName())
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(0, 0, 5, 0)
					[
						SAssignNew(OptionContainer, SUniformGridPanel)
					]
				]
			]
		]);

		CreateOptions();
	}

private:

	void CreateOptions()
	{
		TArray<EFriendActionType::Type> Actions;
		ViewModel->EnumerateActions(Actions);

		for(const auto& FriendAction : Actions)
		{
			OptionContainer->AddSlot(OptionContainer->GetChildren()->Num(), 0)
			[
				SNew(SBox)
				.Padding(5)
				[
					SNew(SButton)
					.ToolTip(FriendAction == EFriendActionType::JoinGame ? CreateJoingGameToolTip() : NULL)
					.IsEnabled(this, &SInviteItemImpl::IsActionEnabled, FriendAction)
					.OnClicked(this, &SInviteItemImpl::PerformAction, FriendAction)
					.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(FriendAction)))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FriendStyle.DefaultFontColor)
						.Font(FriendStyle.FriendsFontStyleSmallBold)
						.Text(EFriendActionType::ToText(FriendAction))
					]
				]
			];
		}
	}

	FReply PerformAction(EFriendActionType::Type FriendAction)
	{
		ViewModel->PerformAction(FriendAction);
		return FReply::Handled();
	}

	bool IsActionEnabled(EFriendActionType::Type FriendAction) const
	{
		return ViewModel->CanPerformAction(FriendAction);
	}

	TSharedPtr<SToolTip> CreateJoingGameToolTip()
	{
		if(!ViewModel->CanPerformAction(EFriendActionType::JoinGame))
		{
			return SNew(SFriendsToolTip)
			.DisplayText(ViewModel->GetJoinGameDisallowReason())
			.FriendStyle(&FriendStyle);
		}
		return nullptr;
	}


private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<SUniformGridPanel> OptionContainer;
	TSharedPtr<FFriendViewModel> ViewModel;
};

TSharedRef<SInviteItem> SInviteItem::New()
{
	return MakeShareable(new SInviteItemImpl());
}

#undef LOCTEXT_NAMESPACE
