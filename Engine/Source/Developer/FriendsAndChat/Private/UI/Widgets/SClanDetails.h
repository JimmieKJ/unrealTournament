// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SClanDetails : public SUserWidget
{
public:

	SLATE_USER_ARGS(SClanDetails) { }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	/** Called when the home button is pressed */
	SLATE_EVENT(FOnClicked, OnHomeClicked)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FClanInfoViewModel>& ViewModel) = 0;
};
