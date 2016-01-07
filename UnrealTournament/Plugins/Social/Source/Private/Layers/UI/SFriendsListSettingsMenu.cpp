// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendsListSettingsMenu.h"
#include "SChatChromeSettings.h"

#include "ChatSettingsViewModel.h"
#include "ChatSettingsService.h"

#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE ""

class SFriendsListSettingsMenuImpl : public SFriendsListSettingsMenu
{
public:


	void Construct(const FArguments& InArgs, const TSharedRef<FChatSettingsService>& InSettingsService, const TSharedRef<FFriendsFontStyleService> InFontService)
	{
		FontService = InFontService;
		SettingsService = InSettingsService;
		FriendStyle = *InArgs._FriendStyle;
		Opacity = InArgs._Opacity;

		TArray<EChatSettingsType::Type> FriendsListSettings;
		FriendsListSettings.Add(EChatSettingsType::HideOffline);
		FriendsListSettings.Add(EChatSettingsType::HideOutgoing);
		FriendsListSettings.Add(EChatSettingsType::HideSuggestions);
		FriendsListSettings.Add(EChatSettingsType::HideRecent);
		FriendsListSettings.Add(EChatSettingsType::HideBlocked);
		FriendsListSettings.Add(EChatSettingsType::FontSize);

		TSharedPtr<FChatSettingsViewModel> ChatSettingsViewModel = FChatSettingsViewModelFactory::Create(SettingsService.ToSharedRef());

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.FriendsListStyle.FriendsListBackground)
			.BorderBackgroundColor(this, &SFriendsListSettingsMenuImpl::GetBackgroundOpacityFadeColor)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					// Header and back button
					SAssignNew(BackButton, SButton)
					.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
					.ButtonColorAndOpacity(this, &SFriendsListSettingsMenuImpl::GetOpacityFadeColor)
					.ContentPadding(FriendStyle.FriendsListStyle.SubMenuBackButtonMargin)
					.OnClicked(this, &SFriendsListSettingsMenuImpl::HandleBackButtonClicked)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(FriendStyle.FriendsListStyle.SubMenuBackIconMargin)
						[
							SNew(SImage)
							.ColorAndOpacity(this, &SFriendsListSettingsMenuImpl::GetBackButtonContentColor)
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
							.ColorAndOpacity(this, &SFriendsListSettingsMenuImpl::GetOpacityFadeColor)
						]
						+ SHorizontalBox::Slot()
						.Padding(FriendStyle.FriendsListStyle.SubMenuPageIconMargin)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(&FriendStyle.FriendsListStyle.SettingsBrush)
							.ColorAndOpacity(this, &SFriendsListSettingsMenuImpl::GetBackButtonContentColor)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Settings", "SETTINGS"))
							.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
							.ColorAndOpacity(this, &SFriendsListSettingsMenuImpl::GetBackButtonContentColor)
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
					.ColorAndOpacity(this, &SFriendsListSettingsMenuImpl::GetOpacityFadeColor)
				]
				+SVerticalBox::Slot()
				.Padding(FriendStyle.FriendsListStyle.SubMenuListMargin)
				[
					SNew(SChatChromeSettings, ChatSettingsViewModel, FontService.ToSharedRef())
					.FriendStyle(&FriendStyle)
					.Settings(FriendsListSettings)
					.Opacity(Opacity)
				]
			]
		]);
	}

private:

	DECLARE_DERIVED_EVENT(SFriendsListSettingsMenu, SFriendsListSettingsMenu::FOnCloseEvent, FOnCloseEvent);
	virtual FOnCloseEvent& OnCloseEvent() override
	{
		return OnCloseDelegate;
	}

	FSlateColor GetOpacityFadeColor() const
	{
		return FLinearColor(1, 1, 1, Opacity.Get());
	}

	FSlateColor GetBackgroundOpacityFadeColor() const
	{
		return FLinearColor(1, 1, 1, Opacity.Get() * 0.9f);
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

	FSlateColor GetBackButtonContentColor() const
	{
		return GetButtonContentColor(BackButton);
	}

	FReply HandleBackButtonClicked()
	{
		OnCloseDelegate.Broadcast();
		return FReply::Handled();
	}

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	FOnCloseEvent OnCloseDelegate;
	TSharedPtr<FChatSettingsService> SettingsService;
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<SButton> BackButton;
	TAttribute<float> Opacity;
};

TSharedRef<SFriendsListSettingsMenu> SFriendsListSettingsMenu::New()
{
	return MakeShareable(new SFriendsListSettingsMenuImpl());
}

#undef LOCTEXT_NAMESPACE
