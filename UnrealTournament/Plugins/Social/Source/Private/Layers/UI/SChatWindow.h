// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SChatWindow : public SUserWidget
{
public:
	SLATE_USER_ARGS(SChatWindow)
	{ }
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
		SLATE_ARGUMENT(EPopupMethod, Method)

		// The hint that shows what key activates chat
		SLATE_ATTRIBUTE(FText, ActivationHintText)

	SLATE_END_ARGS()

	virtual void HandleWindowActivated() = 0;
	virtual void HandleWindowDeactivated() = 0;

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FChatViewModel>& InDisplayViewModel, const TSharedRef<class FFriendsFontStyleService>& InFontService) = 0;

};
