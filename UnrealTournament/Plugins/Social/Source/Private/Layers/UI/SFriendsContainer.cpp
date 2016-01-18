// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendsContainer.h"
#include "FriendContainerViewModel.h"

#include "FriendsUserViewModel.h"
#include "FriendListViewModel.h"
#include "ChatSettingsService.h"

#include "SFriendsList.h"
#include "SFriendsListHeader.h"
#include "SFilterFriendsListMenu.h"
#include "SOnlineStatusMenu.h"
#include "SAddFriendMenu.h"
#include "SFriendsListSettingsMenu.h"
#include "SOnlinePresenceWidget.h"
#include "SConfirmation.h"

#include "SHeaderButtonToolTip.h"
#include "SFriendsToolTip.h"

#define LOCTEXT_NAMESPACE "SFriendsContainer"

namespace EFriendListButtons
{
	enum Type
	{
		Status_Button,
		Add_Button,
		Settings_Button
	};
};


/**
 * Declares the Friends List display widget
*/
class SFriendsContainerImpl : public SFriendsContainer
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService> InFontService, const TSharedRef<FChatSettingsService>& InSettingsService, const TSharedRef<FFriendContainerViewModel>& InViewModel)
	{
		FontService = InFontService;
		SettingsService = InSettingsService;
		ViewModel = InViewModel;
		FriendStyle = *InArgs._FriendStyle;
		ActiveList = EFriendsDisplayLists::DefaultDisplay;
		AnimTime = 0.2f;

		SettingsService->OnChatSettingStateUpdated().AddSP(this, &SFriendsContainerImpl::SettingUpdated);

		CurveHandle = CurveSequence.AddCurve(0, AnimTime);
			
		for (int32 ListIndex = 0; ListIndex < EFriendsDisplayLists::MAX_None; ListIndex++)
		{
			ListViewModels.Add(ViewModel->GetFriendListViewModel((EFriendsDisplayLists::Type)ListIndex));
			ListViewModels[ListIndex]->OnFriendsListUpdated().AddSP(this, &SFriendsContainerImpl::ListRefreshed);
		}

		// Quick hack to disable clan system via commandline
		EVisibility ClanVisibility = FParse::Param(FCommandLine::Get(), TEXT("ShowClans")) ? EVisibility::Visible : EVisibility::Collapsed;

		TSharedPtr<SOverlay> MenuOverlay;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SAssignNew(MenuOverlay, SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(FriendStyle.FriendsListStyle.HeaderButtonMargin)
						[
							GetStatusWidget()
						]
						+ SHorizontalBox::Slot()
						.Padding(FriendStyle.FriendsListStyle.HeaderButtonMargin)
						[
							GetAddFriendWidget()
						]
						+ SHorizontalBox::Slot()
						[
							GetSettingsWidget()
						]
					]
					+ SVerticalBox::Slot()
					[
						GetFriendsListWidget()
					]
				]
			]
			+ SOverlay::Slot()
			.Expose(MenuSlot)
			+ SOverlay::Slot()
			[
				SAssignNew(ConfirmationMenu, SConfirmation, ViewModel.ToSharedRef(), FontService.ToSharedRef())
				.FriendStyle(&FriendStyle)
				.Visibility(ViewModel.ToSharedRef(), &FFriendContainerViewModel::GetConfirmVisibility)
			]
		]);
	}

private:

	FSlateColor GetButtonContentColor(TSharedPtr<SButton> Button)
	{
		if (Button->IsHovered())
		{
			return FriendStyle.FriendsListStyle.ButtonHoverContentColor;
		}
		return FriendStyle.FriendsListStyle.ButtonContentColor;
	}

	void OnHovor(TSharedPtr<SHeaderButtonToolTip> ToolTip, TSharedPtr<SMenuAnchor> ToolTipAnchor)
	{
		if (ToolTipAnchor->IsOpen() == false)
		{
			ToolTip->PlayIntroAnim();
			ToolTipAnchor->SetIsOpen(true);
		}
	}

	void OnUnhover(TSharedPtr<SMenuAnchor> ToolTipAnchor)
	{
		ToolTipAnchor->SetIsOpen(false);
	}

	// Status Widgets
	TSharedRef<SWidget> GetStatusWidget()
	{
		TSharedPtr<SOnlinePresenceWidget> ButtonImage = SNew(SOnlinePresenceWidget, ViewModel.ToSharedRef())
			.ColorAndOpacity(FLinearColor::White)
			.FriendStyle(&FriendStyle);

		TSharedPtr<SHeaderButtonToolTip> StatusToolTip = SNew(SHeaderButtonToolTip, FontService.ToSharedRef(), ViewModel.ToSharedRef())
			.ShowStatus(true)
			.FriendStyle(&FriendStyle)
			.Tip(LOCTEXT("ChangeStatus", "Online Status"));

		
		TSharedPtr<SMenuAnchor> MenuAnchor = SNew(SMenuAnchor)
			.Placement(MenuPlacement_CenteredBelowAnchor)
			.MenuContent(StatusToolTip.ToSharedRef());

		TSharedPtr<SButton> Button = SNew(SButton)
			.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
			.ContentPadding(FriendStyle.FriendsListStyle.HeaderButtonContentMargin)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SFriendsContainerImpl::HandleStatusButtonClicked)
			.OnHovered(FSimpleDelegate::CreateSP(this, &SFriendsContainerImpl::OnHovor, StatusToolTip, MenuAnchor))
			.OnUnhovered(FSimpleDelegate::CreateSP(this, &SFriendsContainerImpl::OnUnhover, MenuAnchor))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					MenuAnchor.ToSharedRef()
				]
				+ SOverlay::Slot()
				[
					ButtonImage.ToSharedRef()
				]
			];

		ButtonImage->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendsContainerImpl::GetButtonContentColor, Button)));

		return Button.ToSharedRef();
	}

	// Settings Widget
	TSharedRef<SWidget> GetSettingsWidget()
	{
		TSharedPtr<SImage> ButtonImage = SNew(SImage)
			.Image(&FriendStyle.FriendsListStyle.SettingsBrush);

		TSharedPtr<SHeaderButtonToolTip> SettingsToolTip = SNew(SHeaderButtonToolTip, FontService.ToSharedRef(), ViewModel.ToSharedRef())
			.ShowStatus(false)
			.FriendStyle(&FriendStyle)
			.Tip(LOCTEXT("SocialSettingsTip", "Social Settings"));

		TSharedPtr<SMenuAnchor> MenuAnchor = SNew(SMenuAnchor)
			.Placement(MenuPlacement_CenteredBelowAnchor)
			.MenuContent(SettingsToolTip.ToSharedRef());

		TSharedPtr<SButton> Button = SNew(SButton)
			.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
			.ContentPadding(FriendStyle.FriendsListStyle.HeaderButtonContentMargin)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SFriendsContainerImpl::HandleSettingsButtonClicked)
			.OnHovered(FSimpleDelegate::CreateSP(this, &SFriendsContainerImpl::OnHovor, SettingsToolTip, MenuAnchor))
			.OnUnhovered(FSimpleDelegate::CreateSP(this, &SFriendsContainerImpl::OnUnhover, MenuAnchor))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					MenuAnchor.ToSharedRef()
				]
				+ SOverlay::Slot()
				[
					ButtonImage.ToSharedRef()
				]
			];

		ButtonImage->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendsContainerImpl::GetButtonContentColor, Button)));

		return Button.ToSharedRef();
	}

	// Add Friend And Filter Widgets
	TSharedRef<SWidget> GetAddFriendWidget()
	{
		TSharedPtr<SImage> ButtonImage = SNew(SImage)
			.Image(&FriendStyle.FriendsListStyle.AddFriendButtonContentBrush);

		TSharedPtr<SHeaderButtonToolTip> AddFriendToolTip = SNew(SHeaderButtonToolTip, FontService.ToSharedRef(), ViewModel.ToSharedRef())
			.ShowStatus(false)
			.FriendStyle(&FriendStyle)
			.Tip(LOCTEXT("AddFriendTip", "Add Friend"));

		TSharedPtr<SMenuAnchor> MenuAnchor = SNew(SMenuAnchor)
			.Placement(MenuPlacement_CenteredBelowAnchor)
			.MenuContent(AddFriendToolTip.ToSharedRef());

		TSharedPtr<SButton> Button = SNew(SButton)
			.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
			.ContentPadding(FriendStyle.FriendsListStyle.HeaderButtonContentMargin)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SFriendsContainerImpl::HandleAddFriendButtonClicked)
			.OnHovered(FSimpleDelegate::CreateSP(this, &SFriendsContainerImpl::OnHovor, AddFriendToolTip, MenuAnchor))
			.OnUnhovered(FSimpleDelegate::CreateSP(this, &SFriendsContainerImpl::OnUnhover, MenuAnchor))
			[
				SNew(SBox)
				.WidthOverride(FriendStyle.FriendsListStyle.UserPresenceImageSize.X)
				.HeightOverride(FriendStyle.FriendsListStyle.UserPresenceImageSize.Y)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						MenuAnchor.ToSharedRef()
					]
					+ SOverlay::Slot()
					[
						ButtonImage.ToSharedRef()
					]
				]
			];

		ButtonImage->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendsContainerImpl::GetButtonContentColor, Button)));

		return Button.ToSharedRef();
	}

	TSharedRef<SWidget> GetFriendsListWidget()
	{
		SAssignNew(Lists, SVerticalBox);
		CreateLists();
		return Lists.ToSharedRef();
	}

	void ListRefreshed()
	{
		if (ListViewModels[ActiveList]->GetListCount() == 0)
		{
			ActiveList = EFriendsDisplayLists::DefaultDisplay;
		}
	}

	void CreateLists()
	{
		Lists->ClearChildren();
		Lists->AddSlot()
			.AutoHeight()
			.Padding(FriendStyle.FriendsListStyle.FriendsListNoFriendsMargin)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoFriendsNotice", "Press the Plus Button to add friends."))
				.Font(FriendStyle.FriendsNormalFontStyle.FriendsFontNormal)
				.ColorAndOpacity(FLinearColor::White)
				.Visibility(this, &SFriendsContainerImpl::NoFriendsNoticeVisibility)
			];

		for (int32 ListIndex = 0; ListIndex < EFriendsDisplayLists::MAX_None; ++ListIndex)
		{
			TSharedPtr<SFriendsListHeader> NewList;

			Lists->AddSlot()
			.AutoHeight()
			.Padding(FriendStyle.FriendsListStyle.FriendsListMargin)
			[
				SAssignNew(NewList, SFriendsListHeader, FontService.ToSharedRef(), ListViewModels[ListIndex].ToSharedRef())
				.FriendStyle(&FriendStyle)
				.Visibility(this, &SFriendsContainerImpl::GetListHeaderVisibility, EFriendsDisplayLists::Type(ListIndex))
			];

			Lists->AddSlot()
			.Padding(FriendStyle.FriendsListStyle.FriendsListMargin)
			[
				SNew(SFriendsList, FontService.ToSharedRef(), ListViewModels[ListIndex].ToSharedRef(), ViewModel.ToSharedRef())
				.FriendStyle(&FriendStyle)
				.Visibility(this, &SFriendsContainerImpl::GetListVisibility, EFriendsDisplayLists::Type(ListIndex))
			];

			NewList->OnVisibilityChanged().AddSP(this, &SFriendsContainerImpl::ToggleVisibleList);
			ListHeaders.Add(NewList);
		}
		// TODO - provide a method of setting the default list to display 
		ToggleVisibleList(EFriendsDisplayLists::DefaultDisplay);
		ToggleVisibleList(EFriendsDisplayLists::DefaultDisplay);
	}

	TSharedPtr<SToolTip> CreateAddFriendToolTip()
	{
		const FText AddFriendToolTipText = LOCTEXT( "FriendContain_AddFriendToolTip", "Add friend by account name or email" );
		return SNew(SFriendsToolTip)
		.DisplayText(AddFriendToolTipText)
		.BackgroundImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground);
	}

	TSharedPtr<SToolTip> CreateGlobalChatToolTip()
	{
		const FText GlobalChatToolTipText = LOCTEXT("FriendContain_GlobalChatToolTip", "Fortnite Global Chat");
		return SNew(SFriendsToolTip)
			.DisplayText(GlobalChatToolTipText)
			.BackgroundImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground);
	}

	FText GetDisplayName() const
	{
		return FText::FromString(ViewModel->GetName());
	}

	TOptional<FSlateRenderTransform> GetSubMenuRenderTransform() const
	{
		TOptional<FSlateRenderTransform> RenderTransform;
		float Lerp = CurveHandle.GetLerp();
		const float AnimOffset = 250;
		if (Lerp < 1.0f)
		{
			float XOffset = AnimOffset - (AnimOffset * Lerp);
			RenderTransform = TransformCast<FSlateRenderTransform>(FTransform2D(FVector2D(XOffset, 0.f)));
		}
		return RenderTransform;
	}


	float GetContentOpacity() const
	{
		return CurveHandle.GetLerp();
	}

	void OnCloseSubMenu()
	{
		CurveSequence.PlayReverse(AsShared());
		if (TickerHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		}
		TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SFriendsContainerImpl::CloseComplete), AnimTime);
	}

	bool CloseComplete(float Tick)
	{
		MenuSlot->DetachWidget();
		TickerHandle.Reset();
		return false;
	}

	FReply HandleStatusButtonClicked()
	{
		if (!StatusMenu.IsValid())
		{
			SAssignNew(StatusMenu, SOnlineStatusMenu, ViewModel.ToSharedRef(), FontService.ToSharedRef())
				.RenderTransform(this, &SFriendsContainerImpl::GetSubMenuRenderTransform)
				.FriendStyle(&FriendStyle)
				.Opacity(this, &SFriendsContainerImpl::GetContentOpacity);
			
			StatusMenu->OnCloseEvent().AddSP(this, &SFriendsContainerImpl::OnCloseSubMenu);
		}

		MenuSlot->DetachWidget();
		MenuSlot->AttachWidget(StatusMenu.ToSharedRef());
		CurveSequence.Play(AsShared());
		return FReply::Handled();
	}

	FReply HandleSettingsButtonClicked()
	{
		if (!SettingsMenu.IsValid())
		{
			SAssignNew(SettingsMenu, SFriendsListSettingsMenu, SettingsService.ToSharedRef(), FontService.ToSharedRef())
				.RenderTransform(this, &SFriendsContainerImpl::GetSubMenuRenderTransform)
				.FriendStyle(&FriendStyle)
				.Opacity(this, &SFriendsContainerImpl::GetContentOpacity);

			SettingsMenu->OnCloseEvent().AddSP(this, &SFriendsContainerImpl::OnCloseSubMenu);
		}

		MenuSlot->DetachWidget();
		MenuSlot->AttachWidget(SettingsMenu.ToSharedRef());
		CurveSequence.Play(AsShared());
		return FReply::Handled();
	}

	FReply HandleAddFriendButtonClicked()
	{
		if (!AddFriendMenu.IsValid())
		{
			SAssignNew(AddFriendMenu, SAddFriendMenu, ViewModel.ToSharedRef(), FontService.ToSharedRef())
				.RenderTransform(this, &SFriendsContainerImpl::GetSubMenuRenderTransform)
				.FriendStyle(&FriendStyle)
				.Opacity(this, &SFriendsContainerImpl::GetContentOpacity);

			AddFriendMenu->OnCloseEvent().AddSP(this, &SFriendsContainerImpl::OnCloseSubMenu);
		}
		MenuSlot->DetachWidget();
		MenuSlot->AttachWidget(AddFriendMenu.ToSharedRef());
		CurveSequence.Play(AsShared());
		AddFriendMenu->Initialize();
		return FReply::Handled();
	}

	FReply HandleBackButtonClicked()
	{
		FSlateApplication::Get().DismissMenuByWidget(AsShared());
		return FReply::Handled();
	}

	void OnFilterList(const FText& FilterText)
	{
		ListViewModels[ActiveList]->SetListFilter(FilterText);
	}

	EVisibility GetGlobalVisibility() const
	{
		return ViewModel->GetGlobalChatButtonVisibility();
	}

	EVisibility NoFriendsNoticeVisibility() const
	{
		for (auto& ListViewModel : ListViewModels)
		{
			if (ListViewModel->GetListVisibility() == EVisibility::Visible)
			{
				return EVisibility::Collapsed;
			}
		}
		return EVisibility::Visible;
	}

	FReply OnGlobalChatButtonClicked()
	{
		ViewModel->DisplayGlobalChatWindow();
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled();
	}

private:

	void SettingUpdated(EChatSettingsType::Type Setting, bool NewState)
	{
		if(ActiveList != EFriendsDisplayLists::MAX_None)
		{
			if(ListViewModels[ActiveList]->IsEnabled() == false)
			{
				ActiveList = EFriendsDisplayLists::DefaultDisplay;
			}
		}
	}

	EVisibility GetListHeaderVisibility(EFriendsDisplayLists::Type ListType) const
	{
		EVisibility HeaderVis = ListViewModels[ListType]->GetListVisibility();
		if (HeaderVis != EVisibility::Visible)
		{
			return EVisibility::Collapsed;
		}
		return ListViewModels[ListType]->IsEnabled() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetListVisibility(EFriendsDisplayLists::Type ListType) const
	{
		if (ListType == ActiveList)
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}

	void ToggleVisibleList(EFriendsDisplayLists::Type ListType)
	{
		if (ListType == ActiveList)
		{
			// If the list is already open move to the next list down, or up if were the bottom list
			for (int ListIndex = 0; ListIndex < EFriendsDisplayLists::MAX_None; ++ListIndex)
			{
				if (ListViewModels[ListIndex]->IsEnabled())
				{
					if (ListIndex != ActiveList)
					{
						ListType = EFriendsDisplayLists::Type(ListIndex);
						if (ListIndex > ActiveList)
						{
							break;
						}
					}
				}
			}
		}
		if (ListType != ActiveList)
		{
			ListHeaders[ActiveList]->SetHighlighted(false);
			ActiveList = ListType;
			ListHeaders[ActiveList]->SetHighlighted(true);
		}
	}

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	// Holds the Settings Service
	TSharedPtr<FChatSettingsService> SettingsService;

	// Holds the Friends List view model
	TSharedPtr<FFriendContainerViewModel> ViewModel;

	// Holds the Friends Sub-List view models
	TArray<TSharedPtr<FFriendListViewModel>> ListViewModels;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	// Holds the FriendsListHeaders and FriendsLists
	TSharedPtr<SVerticalBox> Lists;

	// Holds the FriendsListHeaders and FriendsLists
	TArray<TSharedPtr<SFriendsListHeader>> ListHeaders;

	// Add Friend Button
	TArray<TSharedPtr<SButton>> Buttons;

	// The list we are currently viewing
	EFriendsDisplayLists::Type ActiveList;

	// Overlay Menus
	SOverlay::FOverlaySlot* MenuSlot;
	TSharedPtr<SOnlineStatusMenu> StatusMenu;
	TSharedPtr<SAddFriendMenu> AddFriendMenu;
	TSharedPtr<SFriendsListSettingsMenu> SettingsMenu;
	TSharedPtr<SFilterFriendsListMenu> FilterMenu;
	TSharedPtr<SConfirmation> ConfirmationMenu;
	float AnimTime;
	FCurveSequence CurveSequence;
	FCurveHandle CurveHandle;
	FDelegateHandle TickerHandle;
};

TSharedRef<SFriendsContainer> SFriendsContainer::New()
{
	return MakeShareable(new SFriendsContainerImpl());
}

#undef LOCTEXT_NAMESPACE
