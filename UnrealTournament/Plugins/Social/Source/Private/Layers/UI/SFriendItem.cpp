// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendItem.h"
#include "FriendViewModel.h"
#include "SFriendsToolTip.h"
#include "SFriendsList.h"
#include "SAutoTextScroller.h"
#include "SActionMenu.h"
#include "SFriendItemToolTip.h"
#include "FriendContainerViewModel.h"

#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE "SFriendItem"

class SFriendItemImpl : public SFriendItem
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService>& InFontService, const TSharedRef<FFriendViewModel>& InViewModel, const TSharedRef<FFriendContainerViewModel>& ContainerViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		FontService = InFontService;
		FriendViewModel = InViewModel;
		FFriendViewModel* ViewModelPtr = FriendViewModel.Get();

		// Setup popup Action Menu
		SAssignNew(ActionMenu, SActionMenu, InFontService, ContainerViewModel).FriendStyle(&FriendStyle);
		ActionMenu->SetFriendViewModel(FriendViewModel.ToSharedRef());

		// Setup the AutoScroller
		SAssignNew(PresenceAutoScroller, SAutoTextScroller, FontService.ToSharedRef())
			.Style(&FriendStyle)
			.Text(ViewModelPtr, &FFriendViewModel::GetFriendLocation);

		if (ViewModelPtr->DisplayPCIcon())
		{
			// Setup the PC Icon
			SAssignNew(PlatformIdentifierBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FriendStyle.FriendsListStyle.FriendItemPlatformMargin)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(&FriendStyle.FriendsListStyle.PCIconBrush)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HeightOverride(FriendStyle.FriendsListStyle.PCIconBrush.ImageSize.Y)
				[
					SNew(SSeparator)
					.Orientation(Orient_Vertical)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(FriendStyle.FriendsListStyle.FriendItemTextScrollerMargin)
			.VAlign(VAlign_Center)
			[
				PresenceAutoScroller.ToSharedRef()
			];
		}

		// Setup the tooltip
		SAssignNew(ToolTip, SFriendItemToolTip, FontService.ToSharedRef(), FriendViewModel.ToSharedRef())
			.FriendStyle(&FriendStyle);

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(ToolTipAnchor, SMenuAnchor)
				.Placement(MenuPlacement_MenuLeft)
				.MenuContent(ToolTip.ToSharedRef())
			]
			+ SOverlay::Slot()
			[
				SAssignNew(MenuAnchor, SMenuAnchor)
				.Placement(MenuPlacement_MenuLeft)
				.MenuContent(ActionMenu.ToSharedRef())
			]
			+SOverlay::Slot()
			[
				SAssignNew(FriendButton, SButton)
				.OnHovered(FSimpleDelegate::CreateSP(this, &SFriendItemImpl::OnHovor ))
				.OnUnhovered(FSimpleDelegate::CreateSP(this, &SFriendItemImpl::OnUnhover))
				.ButtonStyle(&FriendStyle.FriendsListStyle.FriendItemButtonStyle)
				.ContentPadding(0)
				[
					SNew(SBorder)
					.OnMouseDoubleClick(this, &SFriendItemImpl::OnDoubleClick)
					.OnMouseButtonDown(this, &SFriendItemImpl::OnMouseDown)
					.Padding(FMargin(0))
					.BorderBackgroundColor(FLinearColor::Transparent)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Visibility(this, &SFriendItemImpl::ActionVisibility)
							.Image(this, &SFriendItemImpl::GetButtonPressedColor)
						]
						+ SOverlay::Slot()
						.Padding(FriendStyle.FriendsListStyle.FriendItemMargin)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(FriendStyle.FriendsListStyle.FriendItemStatusMargin)
							.AutoWidth()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(SImage)
								.Image(this, &SFriendItemImpl::GetStatusBrush)
							]
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.FillWidth(1)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
									.ColorAndOpacity(FriendStyle.FriendsNormalFontStyle.DefaultFontColor)
									.Text(FText::FromString(FriendViewModel->GetName()))
								]
								+ SVerticalBox::Slot()
								.Expose(AutoScrollerSlot)
								.AutoHeight()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Left)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FriendStyle.FriendsListStyle.FriendItemPresenceMargin)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(FriendStyle.FriendsListStyle.FriendImageBrush.ImageSize.X)
								.HeightOverride(FriendStyle.FriendsListStyle.FriendImageBrush.ImageSize.Y)
								[
									SNew(SImage)
									.Visibility(this, &SFriendItemImpl::GetPresenceVisibility)
									.Image(this, &SFriendItemImpl::GetPresenceBrush)
								]
							]
						]
					]
				]
			]
		]);

		if(FriendViewModel->DisplayPresenceScroller())
		{
			if (PlatformIdentifierBox.IsValid())
			{
				AutoScrollerSlot->AttachWidget(PlatformIdentifierBox.ToSharedRef());
			}
			else
			{
				AutoScrollerSlot->AttachWidget(PresenceAutoScroller.ToSharedRef());
			}
		}
	}

	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		ActionMenuAnchorPosition = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation();
	}

private:

	void OnHovor()
	{
		PresenceAutoScroller->Start();
		//if (MenuAnchor->IsOpen() == false)
		//{
			//ToolTip->PlayIntroAnim();
			//ToolTipAnchor->SetIsOpen(true);
		//}
	}

	void OnUnhover()
	{
		PresenceAutoScroller->Stop();
		//ToolTipAnchor->SetIsOpen(false);
	}

	EVisibility ActionVisibility() const
	{
		return MenuAnchor->IsOpen() ? EVisibility::Visible : EVisibility::Hidden;
	}

	FReply OnDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
	{
		FriendViewModel->PerformAction(EFriendActionType::Chat);
		return FReply::Handled();
	}

	FReply OnMouseDown(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
	{
		MenuAnchor->SetIsOpen(true);
		ToolTipAnchor->SetIsOpen(false);

		ActionMenu->SetFriendViewModel(FriendViewModel.ToSharedRef()); // set the viewmodel again to make sure to re enumerate the friend actions
		ActionMenu->PlayIntroAnim();
 		return FReply::Handled();
	}

	EVisibility GetPresenceVisibility() const
	{
		if (FriendViewModel.IsValid() && FriendViewModel->IsOnline())
		{
			return EVisibility::Visible;
		}
		return EVisibility::Hidden;
	}

	const FSlateBrush* GetPresenceBrush() const
	{
		return FriendViewModel->GetPresenceBrush();
	}

	const FSlateBrush* GetStatusBrush() const
	{
		switch (FriendViewModel->GetOnlineStatus())
		{
		case EOnlinePresenceState::Away:
		case EOnlinePresenceState::ExtendedAway:
			return &FriendStyle.FriendsListStyle.AwayBrush;
		case EOnlinePresenceState::Chat:
		case EOnlinePresenceState::DoNotDisturb:
		case EOnlinePresenceState::Online:
			return &FriendStyle.FriendsListStyle.OnlineBrush;
		case EOnlinePresenceState::Offline:
		default:
			return &FriendStyle.FriendsListStyle.OfflineBrush;
		};
	}

	const FSlateBrush* GetButtonPressedColor() const
	{
		if (FriendButton->IsHovered())
		{
			return &FriendStyle.FriendsListStyle.HeaderButtonStyle.Hovered;
		}
		return &FriendStyle.FriendsListStyle.HeaderButtonStyle.Pressed;
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

private:

	TSharedPtr<SMenuAnchor> MenuAnchor;

	TSharedPtr<SMenuAnchor> ToolTipAnchor;

	TSharedPtr<SFriendItemToolTip> ToolTip;

	TSharedPtr<SAutoTextScroller> PresenceAutoScroller;

	TSharedPtr<SHorizontalBox> PlatformIdentifierBox;

	SVerticalBox::FSlot* AutoScrollerSlot;

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	TSharedPtr<FFriendViewModel> FriendViewModel;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	TSharedPtr<SActionMenu> ActionMenu;
	FVector2D ActionMenuAnchorPosition;

	TSharedPtr<SButton> FriendButton;
};

TSharedRef<SFriendItem> SFriendItem::New()
{
	return MakeShareable(new SFriendItemImpl());
}

#undef LOCTEXT_NAMESPACE
