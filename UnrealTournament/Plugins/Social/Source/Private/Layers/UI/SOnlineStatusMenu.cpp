// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SOnlineStatusMenu.h"
#include "SOnlinePresenceWidget.h"
#include "FriendContainerViewModel.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE ""

#include "FriendsStatusViewModel.h"

class SOnlineStatusMenuImpl : public SOnlineStatusMenu
{
public:

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendContainerViewModel>& InFriendsViewModel, const TSharedRef<FFriendsFontStyleService> InFontService)
	{
		FontService = InFontService;
		FriendStyle = *InArgs._FriendStyle;
		FriendsViewModel = InFriendsViewModel;
		FriendsStatusViewModel = InFriendsViewModel->GetStatusViewModel();
		Opacity = InArgs._Opacity;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.FriendsListStyle.FriendsListBackground)
			.BorderBackgroundColor(this, &SOnlineStatusMenuImpl::GetBackgroundOpacityFadeColor)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					// Header and back button
					SAssignNew(BackButton, SButton)
					.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
					.ButtonColorAndOpacity(this, &SOnlineStatusMenuImpl::GetOpacityFadeColor)
					.ContentPadding(FriendStyle.FriendsListStyle.SubMenuBackButtonMargin)
					.OnClicked(this, &SOnlineStatusMenuImpl::HandleBackButtonClicked)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(FriendStyle.FriendsListStyle.SubMenuBackIconMargin)
						[
							SNew(SImage)
							.ColorAndOpacity(this, &SOnlineStatusMenuImpl::GetBackButtonContentColor)
							.Image(&FriendStyle.FriendsListStyle.BackBrush)
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Center)
						.AutoWidth()
						[
							SNew(SSeparator)
							.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
							.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
							.Orientation(EOrientation::Orient_Vertical)
							.ColorAndOpacity(this, &SOnlineStatusMenuImpl::GetOpacityFadeColor)
						]
						+ SHorizontalBox::Slot()
						.Padding(FriendStyle.FriendsListStyle.SubMenuPageIconMargin)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(FriendStyle.FriendsListStyle.UserPresenceImageSize.X)
							.HeightOverride(FriendStyle.FriendsListStyle.UserPresenceImageSize.Y)
							[
								SNew(SOnlinePresenceWidget, FriendsViewModel.ToSharedRef())
								.ColorAndOpacity(this, &SOnlineStatusMenuImpl::GetBackButtonContentColor)
								.FriendStyle(&FriendStyle)
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Status", "STATUS"))
							.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
							.ColorAndOpacity(this, &SOnlineStatusMenuImpl::GetBackButtonContentColor)
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
					.ColorAndOpacity(this, &SOnlineStatusMenuImpl::GetOpacityFadeColor)
				]
				+ SVerticalBox::Slot()
				.Padding(FriendStyle.FriendsListStyle.SubMenuListMargin)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						GetStutusCheckBoxWidget(EOnlinePresenceState::Online).ToSharedRef()
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						GetStutusCheckBoxWidget(EOnlinePresenceState::Away).ToSharedRef()
					]
				]
			]
		]);
	}

private:

	DECLARE_DERIVED_EVENT(SOnlineStatusMenu, SOnlineStatusMenu::FOnCloseEvent, FOnCloseEvent);
	virtual FOnCloseEvent& OnCloseEvent() override
	{
		return OnCloseDelegate;
	}

	TSharedPtr<SWidget> GetStutusCheckBoxWidget(EOnlinePresenceState::Type StateType)
	{
		FText StateName;
		switch (StateType)
		{
		case EOnlinePresenceState::Away:
			StateName = LOCTEXT("Away", "Away");
			break;
		case EOnlinePresenceState::Online:
			StateName = LOCTEXT("Online", "Online");
			break;
		case EOnlinePresenceState::Offline:
		default:
			StateName = LOCTEXT("Offline", "Offline");
			break;
		}

		TSharedPtr<STextBlock> TextBlock = SNew(STextBlock)
			.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
			.Text(StateName);

		TSharedPtr<SImage> RadioImage = SNew(SImage)
			.Image(this, &SOnlineStatusMenuImpl::GetRadioOptionBrush, StateType);

		TSharedPtr<SButton> Button = SNew(SButton)
			.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
			.ButtonColorAndOpacity(this, &SOnlineStatusMenuImpl::GetOpacityFadeColor)
			.ContentPadding(FriendStyle.FriendsListStyle.SubMenuSettingButtonMargin)
			.OnClicked(this, &SOnlineStatusMenuImpl::HandleRadioButtonClicked, StateType)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					TextBlock.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					RadioImage.ToSharedRef()
				]
			];

		
		RadioImage->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SOnlineStatusMenuImpl::GetButtonContentColor, Button)));
		TextBlock->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SOnlineStatusMenuImpl::GetButtonContentColor, Button)));

		return Button;
	}

	const FSlateBrush* GetRadioOptionBrush(EOnlinePresenceState::Type StateType) const
	{
		if (FriendsStatusViewModel.IsValid())
		{
			if (FriendsStatusViewModel->GetOnlineStatus() == StateType)
			{
				return &FriendStyle.RadioBoxStyle.CheckedImage;
			}
		}
		return &FriendStyle.RadioBoxStyle.UncheckedImage;
	}

	ECheckBoxState GetRadioOptionCheckState(EOnlinePresenceState::Type StateType) const
	{
		if (FriendsStatusViewModel.IsValid())
		{
			if (FriendsStatusViewModel->GetOnlineStatus() == StateType)
			{
				return ECheckBoxState::Checked;
			}
		}
		return ECheckBoxState::Unchecked;
	}

	FReply HandleRadioButtonClicked(EOnlinePresenceState::Type StateType)
	{
		ECheckBoxState NewState = GetRadioOptionCheckState(StateType);
		NewState = NewState == ECheckBoxState::Checked ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
		HandleRadioboxStateChanged(NewState, StateType);
		return FReply::Handled();
	}

	void HandleRadioboxStateChanged(ECheckBoxState NewState, EOnlinePresenceState::Type StateType)
	{
		if (FriendsStatusViewModel.IsValid() && NewState == ECheckBoxState::Checked)
		{
			FriendsStatusViewModel->SetOnlineStatus(StateType);
		}
	}

	FSlateColor GetOpacityFadeColor() const
	{
		return FLinearColor(1, 1, 1, Opacity.Get());
	}

	FSlateColor GetBackgroundOpacityFadeColor() const
	{
		return FLinearColor(1, 1, 1, Opacity.Get() * 0.9f);
	}

	FSlateColor GetBackButtonContentColor() const
	{
		return GetButtonContentColor(BackButton);
	}

	FSlateColor GetButtonContentColor(TSharedPtr<SButton> Button) const
	{
		FLinearColor ButtonColor = FriendStyle.FriendsListStyle.ButtonContentColor.GetSpecifiedColor();
		if (Button.IsValid() && Button->IsHovered())
		{
			ButtonColor = FriendStyle.FriendsListStyle.ButtonHoverContentColor.GetSpecifiedColor();
		}
		ButtonColor.A = Opacity.Get();
		return ButtonColor;
	}

	FReply HandleBackButtonClicked()
	{
		OnCloseDelegate.Broadcast();
		return FReply::Handled();
	}

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FFriendContainerViewModel> FriendsViewModel;
	TSharedPtr<FFriendsStatusViewModel> FriendsStatusViewModel;
	FOnCloseEvent OnCloseDelegate;
	TSharedPtr<SButton> BackButton;
	TAttribute<float> Opacity;
};

TSharedRef<SOnlineStatusMenu> SOnlineStatusMenu::New()
{
	return MakeShareable(new SOnlineStatusMenuImpl());
}

#undef LOCTEXT_NAMESPACE
