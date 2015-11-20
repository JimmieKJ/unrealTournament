// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"

const FName FFriendsChatChromeStyle::TypeName( TEXT("FFriendsChatChromeStyle") );

const FFriendsChatChromeStyle& FFriendsChatChromeStyle::GetDefault()
{
	static FFriendsChatChromeStyle Default;
	return Default;
}


FFriendsChatChromeStyle& FFriendsChatChromeStyle::SetChatBackgroundBrush(const FSlateBrush& Value)
{
	ChatBackgroundBrush = Value;
	return *this;
}

FFriendsChatChromeStyle& FFriendsChatChromeStyle::SetTabBackgroundBrush(const FSlateBrush& Value)
{
	TabBackgroundBrush = Value;
	return *this;
}
