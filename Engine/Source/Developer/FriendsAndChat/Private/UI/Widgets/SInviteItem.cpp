// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SInviteItem.h"
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
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(&FriendStyle.FriendImageBrush)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(STextBlock)
					.Font(FriendStyle.FriendsFontStyleSmallBold)
					.Text(ViewModel->GetFriendName())
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				[
					SAssignNew(OptionContainer, SUniformGridPanel)
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
					.IsEnabled(this, &SInviteItemImpl::IsActionEnabled, FriendAction)
					.OnClicked(this, &SInviteItemImpl::PerformAction, FriendAction)
					.ButtonStyle(&FriendStyle.FriendListActionButtonStyle)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FriendStyle.DefaultFontColor)
						.Font(FriendStyle.FriendsFontStyle)
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
