// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"

const FName FFriendsAndChatStyle::TypeName( TEXT("FFriendsAndChatStyle") );

FFriendsAndChatStyle& FFriendsAndChatStyle::SetSmallFriendsFontStyle(const FFriendsFontStyle& FontStyle)
{
	FriendsSmallFontStyle = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetNormalFriendsFontStyle(const FFriendsFontStyle& FontStyle)
{
	FriendsNormalFontStyle = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetLargeFriendsFontStyle(const FFriendsFontStyle& FontStyle)
{
	FriendsLargeFontStyle = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListStyle(const FFriendsListStyle& InFriendsListStyle)
{
	FriendsListStyle = InFriendsListStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetCheckBoxStyle(const FCheckBoxStyle& InCheckBoxStyle)
{
	CheckBoxStyle = InCheckBoxStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetRadioBoxStyle(const FCheckBoxStyle& InRadioBoxStyle)
{
	RadioBoxStyle = InRadioBoxStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsChatStyle(const FFriendsChatStyle& InFriendsChatStyle)
{
	FriendsChatStyle = InFriendsChatStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsChatChromeStyle(const FFriendsChatChromeStyle& InFriendsChatChromeStyle)
{
	FriendsChatChromeStyle = InFriendsChatChromeStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsMarkupStyle(const FFriendsMarkupStyle& InFriendsMarkupStyle)
{
	FriendsMarkupStyle = InFriendsMarkupStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetScrollbarStyle(const FScrollBarStyle& InScrollBarStyle)
{
	ScrollBarStyle = InScrollBarStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetActionButtonStyle(const FButtonStyle& ButtonStyle)
{
	ActionButtonStyle = ButtonStyle;
	return *this;
}

const FFriendsAndChatStyle& FFriendsAndChatStyle::GetDefault()
{
	static FFriendsAndChatStyle Default;
	return Default;
}

/**
	Module style set
*/
TSharedPtr< FSlateStyleSet > FFriendsAndChatModuleStyle::FriendsAndChatModuleStyleInstance = NULL;

void FFriendsAndChatModuleStyle::Initialize(FFriendsAndChatStyle FriendStyle)
{
	if ( !FriendsAndChatModuleStyleInstance.IsValid() )
	{
		FriendsAndChatModuleStyleInstance = Create(FriendStyle);
		FSlateStyleRegistry::RegisterSlateStyle( *FriendsAndChatModuleStyleInstance );
	}
}

void FFriendsAndChatModuleStyle::Shutdown()
{
	if ( FriendsAndChatModuleStyleInstance.IsValid() )
	{
		FSlateStyleRegistry::UnRegisterSlateStyle( *FriendsAndChatModuleStyleInstance );
		ensure( FriendsAndChatModuleStyleInstance.IsUnique() );
		FriendsAndChatModuleStyleInstance.Reset();
	}
}

FName FFriendsAndChatModuleStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("FriendsAndChat"));
	return StyleSetName;
}

TSharedRef< FSlateStyleSet > FFriendsAndChatModuleStyle::Create(FFriendsAndChatStyle FriendStyle)
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("FriendsAndChatStyle"));

	const FTextBlockStyle DefaultText = FTextBlockStyle(FriendStyle.FriendsChatStyle.TextStyle)
		.SetFont(FriendStyle.FriendsNormalFontStyle.FriendsFontSmall);

	// Name Style
	const FTextBlockStyle GlobalChatFont = FTextBlockStyle(DefaultText)
		.SetFont(FriendStyle.FriendsChatStyle.TextStyle.Font)
		.SetColorAndOpacity(FriendStyle.FriendsChatStyle.GlobalHyperlinkChatColor);

	const FTextBlockStyle GameChatFont = FTextBlockStyle(DefaultText)
		.SetFont(FriendStyle.FriendsChatStyle.TextStyle.Font)
		.SetColorAndOpacity(FriendStyle.FriendsChatStyle.GameHyperlinkChatColor);

	const FTextBlockStyle PartyChatFont = FTextBlockStyle(DefaultText)
		.SetFont(FriendStyle.FriendsChatStyle.TextStyle.Font)
		.SetColorAndOpacity(FriendStyle.FriendsChatStyle.PartyHyperlinkChatColor);

	const FTextBlockStyle WhisperChatFont = FTextBlockStyle(DefaultText)
		.SetFont(FriendStyle.FriendsChatStyle.TextStyle.Font)
		.SetColorAndOpacity(FriendStyle.FriendsChatStyle.WhisperHyperlinkChatColor);

	const FButtonStyle UserNameButton = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetPressed(FSlateNoResource())
		.SetHovered(FSlateNoResource());

	const FHyperlinkStyle GlobalChatHyperlink = FHyperlinkStyle()
		.SetUnderlineStyle(UserNameButton)
		.SetTextStyle(GlobalChatFont)
		.SetPadding(FMargin(0.0f));

	const FHyperlinkStyle GameChatHyperlink = FHyperlinkStyle()
		.SetUnderlineStyle(UserNameButton)
		.SetTextStyle(GameChatFont)
		.SetPadding(FMargin(0.0f));

	const FHyperlinkStyle PartyChatHyperlink = FHyperlinkStyle()
		.SetUnderlineStyle(UserNameButton)
		.SetTextStyle(PartyChatFont)
		.SetPadding(FMargin(0.0f));

	const FHyperlinkStyle WhisperChatHyperlink = FHyperlinkStyle()
		.SetUnderlineStyle(UserNameButton)
		.SetTextStyle(WhisperChatFont)
		.SetPadding(FMargin(0.0f));

	const FHyperlinkStyle DefaultChatHyperlink = FHyperlinkStyle()
		.SetUnderlineStyle(UserNameButton)
		.SetTextStyle(DefaultText)
		.SetPadding(FMargin(0.0f));

	Style->Set("UserNameTextStyle.Default", DefaultText);

	Style->Set("UserNameTextStyle.GlobalHyperlink", GlobalChatHyperlink);
	Style->Set("UserNameTextStyle.GameHyperlink", GameChatHyperlink);
	Style->Set("UserNameTextStyle.PartyHyperlink", PartyChatHyperlink);
	Style->Set("UserNameTextStyle.Whisperlink", WhisperChatHyperlink);
	Style->Set("UserNameTextStyle.DefaultHyperlink", DefaultChatHyperlink);
	Style->Set("UserNameTextStyle.GlobalTextStyle", GlobalChatFont);
	Style->Set("UserNameTextStyle.GameTextStyle", GameChatFont);
	Style->Set("UserNameTextStyle.PartyTextStyle", PartyChatFont);
	Style->Set("UserNameTextStyle.WhisperTextStyle", WhisperChatFont);

	Style->Set("MessageBreak", FTextBlockStyle(DefaultText)
		.SetFont(FSlateFontInfo(
		FriendStyle.FriendsNormalFontStyle.FriendsFontSmall.FontObject,
		6,
		FriendStyle.FriendsNormalFontStyle.FriendsFontSmall.TypefaceFontName
		)));

	return Style;
}

void FFriendsAndChatModuleStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FFriendsAndChatModuleStyle::Get()
{
	return *FriendsAndChatModuleStyleInstance;
}