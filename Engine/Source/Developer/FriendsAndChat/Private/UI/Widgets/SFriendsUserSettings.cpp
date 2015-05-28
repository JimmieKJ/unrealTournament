// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsUserSettings.h"
#include "FriendsUserSettingsViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsUserSettings"

/**
 * Declares the Friends Status display widget
*/
class SFriendsUserSettingsImpl : public SFriendsUserSettings
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsUserSettingsViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SAssignNew(ActionMenu, SMenuAnchor)
			.Placement(EMenuPlacement::MenuPlacement_BelowAnchor)
			.Method(InArgs._Method)
			.MenuContent(GetMenuContent())
			[
				SNew(SButton)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.OnClicked(this, &SFriendsUserSettingsImpl::HandleUserSettingsDropDownClicked)
				.ButtonStyle(&FriendStyle.FriendGeneralButtonStyle)
				.Cursor(EMouseCursor::Hand)
				[
					SNew(SSpacer)
					.Size(FVector2D(15.0f, 15.0f))
				]
			 ]
		]);
	}

private:

	FReply HandleUserSettingsDropDownClicked() const
	{
		ActionMenu->SetIsOpen(true);
		return FReply::Handled();
	}

	/**
	 * Generate the UserSettings menu.
	 * @return the UserSettings menu widget
	 */
	TSharedRef<SWidget> GetMenuContent()
	{
		TSharedPtr<SVerticalBox> UserSettingsListBox;
		TSharedRef<SWidget> Contents =
			SNew(SBorder)
			.BorderImage(&FriendStyle.Background)
			.BorderBackgroundColor(FLinearColor::White)
			.Padding(10)
			[
				SAssignNew(UserSettingsListBox, SVerticalBox)
			];

		TArray<EUserSettngsType::Type> UserSettings;

		ViewModel->EnumerateUserSettings(UserSettings);

		for(const auto& Option : UserSettings)
		{
			UserSettingsListBox->AddSlot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(ViewModel.Get(), &FFriendsUserSettingsViewModel::HandleCheckboxStateChanged, Option)
					.IsChecked(ViewModel.Get(), &FFriendsUserSettingsViewModel::GetOptionCheckState, Option)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(EUserSettngsType::ToText(Option))
				]
			];
		}
		return Contents;
	}

private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<SMenuAnchor> ActionMenu;
	TSharedPtr<FFriendsUserSettingsViewModel> ViewModel;
};

TSharedRef<SFriendsUserSettings> SFriendsUserSettings::New()
{
	return MakeShareable(new SFriendsUserSettingsImpl());
}

#undef LOCTEXT_NAMESPACE
