// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"

const FName FFriendsMarkupStyle::TypeName( TEXT("FFriendsMarkupStyle") );

const FFriendsMarkupStyle& FFriendsMarkupStyle::GetDefault()
{
	static FFriendsMarkupStyle Default;
	return Default;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetMarkupButtonStyle(const FButtonStyle& ButtonStyle)
{
	MarkupButtonStyle = ButtonStyle;
	return *this;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetMarkupTextStyle(const FTextBlockStyle& InTextStyle)
{
	MarkupTextStyle = InTextStyle;
	return *this;
}

FFriendsMarkupStyle& FFriendsMarkupStyle::SetTipBackgroundBrush(const FSlateBrush& Value)
{
	MarkupBackground = Value;
	return *this;
}

