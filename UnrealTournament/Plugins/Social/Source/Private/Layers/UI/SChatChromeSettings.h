// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SChatChromeSettings : public SUserWidget
{
public:
	SLATE_USER_ARGS(SChatChromeSettings)
	{ }
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
		SLATE_ARGUMENT(TArray<EChatSettingsType::Type>, Settings)
		SLATE_ATTRIBUTE(float, Opacity)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	 virtual void Construct(const FArguments& InArgs, TSharedPtr<class FChatSettingsViewModel> SettingsViewModel, const TSharedRef<class FFriendsFontStyleService>& InFontService) = 0;

};
