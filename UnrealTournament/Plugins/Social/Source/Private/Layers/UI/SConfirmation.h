// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendContainerViewModel;

class SConfirmation : public SUserWidget
{
public:

	SLATE_USER_ARGS(SConfirmation)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendContainerViewModel>& ContainerViewModel, const TSharedRef<FFriendsFontStyleService>& FontService){}

};