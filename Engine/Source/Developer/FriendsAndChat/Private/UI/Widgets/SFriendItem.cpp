// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendItem.h"
#include "FriendViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendItem"

class SFriendItemImpl : public SFriendItem
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		this->ViewModel = InViewModel;
		FFriendViewModel* ViewModelPtr = ViewModel.Get();
		MenuMethod = InArgs._Method;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SAssignNew(RemoveConfirmationAnchor, SMenuAnchor)
				.Method(InArgs._Method)
				.OnGetMenuContent(this, &SFriendItemImpl::GetRemoveConfirmationContent)
				.Placement(MenuMethod == EPopupMethod::UseCurrentWindow ? MenuPlacement_MenuLeft : MenuPlacement_MenuRight)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(JoinGameConfirmationAnchor, SMenuAnchor)
				.Method(InArgs._Method)
				.OnGetMenuContent(this, &SFriendItemImpl::GetJoinGameConfirmationContent)
				.Placement(MenuMethod == EPopupMethod::UseCurrentWindow ? MenuPlacement_MenuLeft : MenuPlacement_MenuRight)
			]
			+SOverlay::Slot()
			[
				SAssignNew(Anchor, SMenuAnchor)
				.Method(InArgs._Method)
				.OnGetMenuContent(this, &SFriendItemImpl::GetMenuContent)
				.Placement(MenuMethod == EPopupMethod::UseCurrentWindow ? MenuPlacement_MenuLeft : MenuPlacement_MenuRight)
				.Content()
				[
					SNew(SButton)
					.ButtonStyle(&FriendStyle.FriendListItemButtonSimpleStyle)
					.OnClicked(this, &SFriendItemImpl::HandleItemClicked)
					.ContentPadding(5.0f)
					[
						SNew( SHorizontalBox )
						+SHorizontalBox::Slot()
						.Padding( 10, 0 )
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SImage)
								.Image(this, &SFriendItemImpl::GetPresenceBrush)

							]
							+SOverlay::Slot()
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Right)
							[
								SNew(SImage)
								.Image(this, &SFriendItemImpl::GetStatusBrush)
							]
						]
						+SHorizontalBox::Slot()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							[
								SNew(STextBlock)
								.Font(FriendStyle.FriendsFontStyleBold)
								.ColorAndOpacity(FriendStyle.DefaultFontColor)
								.Text(ViewModel->GetFriendName())
							]
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Font(FriendStyle.FriendsFontStyleSmallBold)
								.ColorAndOpacity(FriendStyle.DefaultFontColor)
								.Text(ViewModelPtr, &FFriendViewModel::GetFriendLocation)
							]
						]
					]
				]
			]
		]);
	}

private:

	const FSlateBrush* GetPresenceBrush() const
	{
		if (ViewModel->IsOnline())
		{
			FString ClientId = ViewModel->GetClientId();
			//@todo samz - better way of finding known ids
			if (ClientId == TEXT("300d79839c914445948e3c1100f211db"))
			{
				return &FriendStyle.FortniteImageBrush;
			}
			else if (ClientId == TEXT("f3e80378aed4462498774a7951cd263f"))
			{
				return &FriendStyle.LauncherImageBrush;
			}
		}
		return &FriendStyle.FriendImageBrush;
	}

	const FSlateBrush* GetStatusBrush() const
	{
		switch (ViewModel->GetOnlineStatus())
		{
		case EOnlinePresenceState::Away:
		case EOnlinePresenceState::ExtendedAway:
			return &FriendStyle.AwayBrush;
		case EOnlinePresenceState::Chat:
		case EOnlinePresenceState::DoNotDisturb:
		case EOnlinePresenceState::Online:
			return &FriendStyle.OnlineBrush;
		case EOnlinePresenceState::Offline:
		default:
			return &FriendStyle.OfflineBrush;
		};
	}

	TSharedRef<SWidget> GetMenuContent()
	{
		TSharedPtr<SVerticalBox> ActionListBox;
		TSharedRef<SWidget> Contents =
			SNew(SBorder)
			.BorderImage(&FriendStyle.Background)
			.Padding(10)
			[
				SAssignNew(ActionListBox, SVerticalBox)
			];

		TArray<EFriendActionType::Type> Actions;

		ViewModel->EnumerateActions(Actions);

		for(const auto& FriendAction : Actions)
		{
			ActionListBox->AddSlot()
			.Padding(5)
			[
				SNew(SButton)
				.IsEnabled(this, &SFriendItemImpl::IsActionEnabled, FriendAction)
				.OnClicked(this, &SFriendItemImpl::HandleActionClicked, FriendAction)
				.ButtonStyle(&FriendStyle.FriendListActionButtonStyle)
				.IsEnabled(this, &SFriendItemImpl::IsActionEnabled, FriendAction)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FriendStyle.DefaultFontColor)
					.Font(FriendStyle.FriendsFontStyle)
					.Text(EFriendActionType::ToText(FriendAction))
				]
			];
		}

		MenuContent = Contents;
		return Contents;
	}

	TSharedRef<SWidget> GetRemoveConfirmationContent()
	{
		TSharedRef<SWidget> Contents =
			SNew(SBorder)
			.BorderImage(&FriendStyle.Background)
			.Padding(10)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(5)
				[
					SNew(STextBlock)
					.Text(FText::FromString("Are you sure?"))
					.Font(FriendStyle.FriendsFontStyle)
					.ColorAndOpacity(FriendStyle.DefaultFontColor)
				]
				+SVerticalBox::Slot()
				.Padding(5)
				[
					SNew(SButton)
					.OnClicked(this, &SFriendItemImpl::HandleRemoveClicked, true)
					.ButtonStyle(&FriendStyle.FriendListCriticalButtonStyle)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						
						.ColorAndOpacity(FriendStyle.DefaultFontColor)
						.Font(FriendStyle.FriendsFontStyle)
						.Text(FText::FromString("Remove"))
					]
				]
				+SVerticalBox::Slot()
				.Padding(5)
				[
					SNew(SButton)
					.OnClicked(this, &SFriendItemImpl::HandleRemoveClicked, false)
					.ButtonStyle(&FriendStyle.FriendListActionButtonStyle)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FriendStyle.DefaultFontColor)
						.Font(FriendStyle.FriendsFontStyle)
						.Text(FText::FromString("Cancel"))
					]
				]
			];

		MenuContent = Contents;
		return Contents;
	}

	TSharedRef<SWidget> GetJoinGameConfirmationContent()
	{
		TSharedRef<SWidget> Contents =
			SNew(SBorder)
			.BorderImage(&FriendStyle.Background)
			.Padding(10)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(5)
				[
					SNew(STextBlock)
					.Text(FText::FromString("Join Game?"))
					.Font(FriendStyle.FriendsFontStyle)
					.ColorAndOpacity(FriendStyle.DefaultFontColor)
				]
				+ SVerticalBox::Slot()
					.Padding(5)
					[
						SNew(SButton)
						.OnClicked(this, &SFriendItemImpl::HandleJoinConfirmClicked, true)
						.ButtonStyle(&FriendStyle.FriendListEmphasisButtonStyle)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)

							.ColorAndOpacity(FriendStyle.DefaultFontColor)
							.Font(FriendStyle.FriendsFontStyle)
							.Text(FText::FromString("Join"))
						]
					]
				+ SVerticalBox::Slot()
					.Padding(5)
					[
						SNew(SButton)
						.OnClicked(this, &SFriendItemImpl::HandleJoinConfirmClicked, false)
						.ButtonStyle(&FriendStyle.FriendListActionButtonStyle)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FriendStyle.DefaultFontColor)
							.Font(FriendStyle.FriendsFontStyle)
							.Text(FText::FromString("Cancel"))
						]
					]
			];

		MenuContent = Contents;
		return Contents;
	}

	bool IsActionEnabled(const EFriendActionType::Type FriendAction) const
	{
		return ViewModel->CanPerformAction(FriendAction);
	}

	FReply HandleActionClicked(const EFriendActionType::Type FriendAction)
	{
		if( FriendAction == EFriendActionType::RemoveFriend)
		{
			Anchor->SetIsOpen(false);
			RemoveConfirmationAnchor->SetIsOpen(true);
		}
		else if (FriendAction == EFriendActionType::JoinGame)
		{
			Anchor->SetIsOpen(false);
			JoinGameConfirmationAnchor->SetIsOpen(true);
		}
		else
		{
			Anchor->SetIsOpen(false);
			ViewModel->PerformAction(FriendAction);
		}
		return FReply::Handled();
	}

	FReply HandleRemoveClicked(bool bConfirm)
	{
		RemoveConfirmationAnchor->SetIsOpen(false);
		if(bConfirm)
		{
			ViewModel->PerformAction(EFriendActionType::RemoveFriend);
		}
		return FReply::Handled();
	}

	FReply HandleJoinConfirmClicked(bool bConfirm)
	{
		JoinGameConfirmationAnchor->SetIsOpen(false);
		if (bConfirm)
		{
			ViewModel->PerformAction(EFriendActionType::JoinGame);
		}
		return FReply::Handled();
	}

	FReply HandleItemClicked()
	{
		Anchor->SetIsOpen(true);
		OpenTime = 0.2f;
		return FReply::Handled();
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		if (Anchor.IsValid() && Anchor->IsOpen())
		{
			if (IsHovered() || Anchor->IsHovered() || (MenuContent.IsValid() && MenuContent->IsHovered()))
			{
				OpenTime = 0.2f;
			}
			else
			{
				OpenTime -= InDeltaTime;
				if (OpenTime < 0 || MenuMethod != EPopupMethod::CreateNewWindow)
				{
					Anchor->SetIsOpen(false);
				}
			}
		}
	}

private:

	TSharedPtr<FFriendViewModel> ViewModel;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	TSharedPtr<SMenuAnchor> Anchor;
	TSharedPtr<SMenuAnchor> RemoveConfirmationAnchor;
	TSharedPtr<SMenuAnchor> JoinGameConfirmationAnchor;

	TSharedPtr<SWidget> MenuContent;

	EPopupMethod MenuMethod;

	float OpenTime;
};

TSharedRef<SFriendItem> SFriendItem::New()
{
	return MakeShareable(new SFriendItemImpl());
}

#undef LOCTEXT_NAMESPACE
