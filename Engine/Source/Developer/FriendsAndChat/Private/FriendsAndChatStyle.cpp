// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"


const FName FFriendsAndChatStyle::TypeName( TEXT("FFriendsAndChatStyle") );


FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListOpenButtonStyle(const FButtonStyle& ButtonStyle)
{
	FriendListOpenButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListClosedButtonStyle(const FButtonStyle& ButtonStyle)
{
	FriendListCloseButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetAddFriendCloseButtonStyle(const FButtonStyle& InAddFriendCloseButtonStyle)
{
	AddFriendCloseButtonStyle = InAddFriendCloseButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendComboBackgroundLeftBrush(const FSlateBrush& BrushStyle)
{
	FriendComboBackgroundLeftBrush = BrushStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendComboBackgroundRightBrush(const FSlateBrush& BrushStyle)
{
	FriendComboBackgroundRightBrush = BrushStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendComboBackgroundLeftFlippedBrush(const FSlateBrush& BrushStyle)
{
	FriendComboBackgroundLeftFlippedBrush = BrushStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendComboBackgroundRightFlippedBrush(const FSlateBrush& BrushStyle)
{
	FriendComboBackgroundRightFlippedBrush = BrushStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetAddFriendButtonContentBrush(const FSlateBrush& BrushStyle)
{
	AddFriendButtonContentBrush = BrushStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetAddFriendButtonContentHoveredBrush(const FSlateBrush& BrushStyle)
{
	AddFriendButtonContentHoveredBrush = BrushStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendGeneralButtonStyle(const FButtonStyle& ButtonStyle)
{
	FriendGeneralButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetButtonInvertedForegroundColor(const FSlateColor& Value)
{
	ButtonInvertedForegroundColor = Value;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetButtonForegroundColor(const FSlateColor& Value)
{
	ButtonForegroundColor = Value;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendListActionButtonStyle(const FButtonStyle& ButtonStyle)
{
	FriendListActionButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListCriticalButtonStyle(const FButtonStyle& ButtonStyle)
{
	FriendListCriticalButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListEmphasisButtonStyle(const FButtonStyle& ButtonStyle)
{
	FriendListEmphasisButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListComboButtonStyle(const FComboButtonStyle& ButtonStyle)
{
	FriendListComboButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetComboItemButtonStyle(const FButtonStyle& ButtonStyle)
{  
	ComboItemButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListItemButtonStyle(const FButtonStyle& ButtonStyle)
{
	FriendListItemButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListItemButtonSimpleStyle(const FButtonStyle& ButtonStyle)
{
	FriendListItemButtonSimpleStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetGlobalChatHeaderBrush(const FSlateBrush& Value)
{
	GlobalChatHeaderBrush = Value;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetGlobalChatButtonStyle(const FButtonStyle& ButtonStyle)
{
	GlobalChatButtonStyle = ButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetTitleBarBrush(const FSlateBrush& Brush)
{
	TitleBarBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendImageBrush(const FSlateBrush& Brush)
{
	FriendImageBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFortniteImageBrush(const FSlateBrush& Brush)
{
	FortniteImageBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetLauncherImageBrush(const FSlateBrush& Brush)
{
	LauncherImageBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetUTImageBrush(const FSlateBrush& Brush)
{
	UTImageBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsComboDropdownImageBrush(const FSlateBrush& Brush)
{
	FriendsComboDropdownImageBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsAddImageBrush(const FSlateBrush& Brush)
{
	FriendsAddImageBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsCalloutBrush(const FSlateBrush& Brush)
{
	FriendsCalloutBrush = Brush;
	return *this;
}


FFriendsAndChatStyle& FFriendsAndChatStyle::SetOfflineBrush(const FSlateBrush& InOffLine)
{
	OfflineBrush = InOffLine; 
	return *this; 
}


FFriendsAndChatStyle& FFriendsAndChatStyle::SetOnlineBrush(const FSlateBrush& InOnLine)
{
	OnlineBrush = InOnLine; 
	return *this; 
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetAwayBrush(const FSlateBrush& InAwayBrush)
{
	AwayBrush = InAwayBrush; 
	return *this; 
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetBackgroundBrush(const FSlateBrush& InBackground)
{
	Background = InBackground;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetWindowEdgingBrush(const FSlateBrush& Value)
{
	WindowEdgingBrush = Value;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendContainerHeader(const FSlateBrush& InFriendContainerHeader)
{
	FriendContainerHeader = InFriendContainerHeader;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendListHeader(const FSlateBrush& InFriendListHeader)
{
	FriendListHeader = InFriendListHeader;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendUserHeaderBackground(const FSlateBrush& InFriendUserHeaderBackground)
{
	FriendUserHeaderBackground = InFriendUserHeaderBackground;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendItemSelected(const FSlateBrush& InFriendItemSelected)
{
	FriendItemSelected = InFriendItemSelected;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatContainerBackground(const FSlateBrush& InChatContainerBackground)
{
	ChatContainerBackground = InChatContainerBackground;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendContainerBackground(const FSlateBrush& InFriendContainerBackground)
{
	FriendsContainerBackground = InFriendContainerBackground;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetAddFriendEditBorder(const FSlateBrush& InAddFriendEditBorder)
{
	AddFriendEditBorder = InAddFriendEditBorder;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetTextStyle(const FTextBlockStyle& InTextStle)
{
	TextStyle = InTextStle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFontStyle(const FSlateFontInfo& InFontStyle)
{
	FriendsFontStyle = InFontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFontStyleBold(const FSlateFontInfo& InFontStyle)
{
	FriendsFontStyleBold = InFontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFontStyleUserLarge(const FSlateFontInfo& InFontStyle)
{
	FriendsFontStyleUserLarge = InFontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFontStyleSmall(const FSlateFontInfo& FontStyle)
{
	FriendsFontStyleSmall = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFontStyleSmallBold(const FSlateFontInfo& FontStyle)
{
	FriendsFontStyleSmallBold = FontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatFontStyleEntry(const FSlateFontInfo& InFontStyle)
{
	ChatFontStyleEntry = InFontStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetDefaultFontColor(const FLinearColor& InFontColor)
{
	DefaultFontColor = InFontColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetDefaultDullFontColor(const FLinearColor& InFontColor)
{
	DefaultDullFontColor = InFontColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetDefaultChatColor(const FLinearColor& InFontColor)
{
	DefaultChatColor = InFontColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetWhisplerChatColor(const FLinearColor& InFontColor)
{
	WhisplerChatColor = InFontColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetPartyChatColor(const FLinearColor& InFontColor)
{
	PartyChatColor = InFontColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetNetworkChatColor(const FLinearColor& InFontColor)
{
	NetworkChatColor = InFontColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetComboItemTextColorNormal(const FLinearColor& InColor)
{
	ComboItemTextColorNormal = InColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetComboItemTextColorHovered(const FLinearColor& InColor)
{
	ComboItemTextColorHovered = InColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendListActionFontColor(const FLinearColor& InColor)
{
	FriendListActionFontColor = InColor;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatGlobalBrush(const FSlateBrush& Brush)
{
	ChatGlobalBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatPartyBrush(const FSlateBrush& Brush)
{
	ChatPartyBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatWhisperBrush(const FSlateBrush& Brush)
{
	ChatWhisperBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatInvalidBrush(const FSlateBrush& Brush)
{
	ChatInvalidBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetAddFriendEditableTextStyle(const FEditableTextBoxStyle& InEditableTextStyle)
{
	AddFriendEditableTextStyle = InEditableTextStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetComboItemTextStyle(const FTextBlockStyle& InTextStyle)
{
	ComboItemTextStyle = InTextStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsComboTextStyle(const FTextBlockStyle& InTextStyle)
{
	FriendsComboTextStyle = InTextStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatEditableTextStyle(const FEditableTextBoxStyle& InEditableTextStyle)
{
	ChatEditableTextStyle = InEditableTextStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendCheckboxStyle(const FCheckBoxStyle& InFriendCheckboxStyle)
{
	FriendCheckboxStyle = InFriendCheckboxStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetStatusButtonSize(const FVector2D& InStatusButtonSize)
{
	StatusButtonSize = InStatusButtonSize;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetActionComboButtonSize(const FVector2D& InActionComboButtonSize)
{
	ActionComboButtonSize = InActionComboButtonSize;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetActionComboButtonStyle(const FComboButtonStyle& InActionComboButtonStyle)
{
	ActionComboButtonStyle = InActionComboButtonStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetActionComboButtonTextStyle(const FTextBlockStyle& InActionComboButtonTextStyle)
{
	ActionComboButtonTextStyle = InActionComboButtonTextStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetUserPresenceImageSize(const FVector2D& InUserPresenceImageSize)
{
	UserPresenceImageSize = InUserPresenceImageSize;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetBorderPadding(const FMargin& Padding)
{
	BorderPadding = Padding;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetUserHeaderPadding(const FMargin& InUserHeaderPadding)
{
	UserHeaderPadding = InUserHeaderPadding;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetFriendsListWidth(const float InFriendsListWidth)
{
	FriendsListWidth = InFriendsListWidth;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatListPadding(const float InChatListPadding)
{
	ChatListPadding = InChatListPadding;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatBackgroundBrush(const FSlateBrush& InChatBackgroundBrush)
{
	ChatBackgroundBrush = InChatBackgroundBrush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatBubbleBackgroundBrush(const FSlateBrush& Value)
{
	ChatBubbleBackgroundBrush = Value;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatBubbleLeftCalloutBrush(const FSlateBrush& Value)
{
	ChatBubbleLeftCallout = Value;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatBubbleRightCalloutBrush(const FSlateBrush& Value)
{
	ChatBubbleRightCallout = Value;
	return *this;
}


FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatFooterBrush(const FSlateBrush& Value)
{
	ChatFooterBrush = Value;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatChannelsBackgroundBrush(const FSlateBrush& InChatChannelsBackgroundBrush)
{
	ChatChannelsBackgroundBrush = InChatChannelsBackgroundBrush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatOptionsBackgroundBrush(const FSlateBrush& InChatOptionsBackgroundBrush)
{
	ChatOptionsBackgroundBrush = InChatOptionsBackgroundBrush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetScrollbarStyle(const FScrollBarStyle& InScrollBarStyle)
{
	ScrollBarStyle = InScrollBarStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetWindowStyle(const FWindowStyle& InStyle)
{
	WindowStyle = InStyle;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetComboMenuPadding(const FMargin& InPadding)
{
	ComboMenuPadding = InPadding;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetComboItemPadding(const FMargin& InPadding)
{
	ComboItemPadding = InPadding;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetComboItemContentPadding(const FMargin& InPadding)
{
	ComboItemContentPadding = InPadding;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetHasUserHeader(bool InHasUserHeader)
{
	HasUserHeader = InHasUserHeader;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetChatWindowStyle(const EChatChannelStyle::Type Value)
{
	ChatChannelStyle = Value;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetClanDetailsBrush(const FSlateBrush& Brush)
{
	ClanDetailsBrush = Brush;
	return *this;
}

FFriendsAndChatStyle& FFriendsAndChatStyle::SetClanMembersBrush(const FSlateBrush& Brush)
{
	ClanMembersBrush = Brush;
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
	FSlateStyleRegistry::UnRegisterSlateStyle( *FriendsAndChatModuleStyleInstance );
	ensure( FriendsAndChatModuleStyleInstance.IsUnique() );
	FriendsAndChatModuleStyleInstance.Reset();
}

FName FFriendsAndChatModuleStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("FriendsAndChat"));
	return StyleSetName;
}

TSharedRef< FSlateStyleSet > FFriendsAndChatModuleStyle::Create(FFriendsAndChatStyle FriendStyle)
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("FriendsAndChatStyle"));

	const FTextBlockStyle DefaultText = FTextBlockStyle(FriendStyle.TextStyle)
		.SetFont(FriendStyle.FriendsFontStyleSmall);

	// Name Style
	const FTextBlockStyle GlobalChatFont = FTextBlockStyle(DefaultText)
		.SetFont(FriendStyle.FriendsFontStyleSmallBold)
		.SetColorAndOpacity(FriendStyle.DefaultChatColor);

	const FTextBlockStyle PartyChatFont = FTextBlockStyle(DefaultText)
		.SetFont(FriendStyle.FriendsFontStyleSmallBold)
		.SetColorAndOpacity(FriendStyle.PartyChatColor);

	const FTextBlockStyle WhisperChatFont = FTextBlockStyle(DefaultText)
		.SetFont(FriendStyle.FriendsFontStyleSmallBold)
		.SetColorAndOpacity(FriendStyle.WhisplerChatColor);

	const FButtonStyle UserNameButton = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetPressed(FSlateNoResource())
		.SetHovered(FSlateNoResource());

	const FHyperlinkStyle GlobalChatHyperlink = FHyperlinkStyle()
		.SetUnderlineStyle(UserNameButton)
		.SetTextStyle(GlobalChatFont)
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
	Style->Set("UserNameTextStyle.PartyHyperlink", PartyChatHyperlink);
	Style->Set("UserNameTextStyle.Whisperlink", WhisperChatHyperlink);
	Style->Set("UserNameTextStyle.DefaultHyperlink", DefaultChatHyperlink);
	Style->Set("UserNameTextStyle.GlobalTextStyle", GlobalChatFont);
	Style->Set("UserNameTextStyle.PartyTextStyle", PartyChatFont);
	Style->Set("UserNameTextStyle.WhisperTextStyle", WhisperChatFont);

	Style->Set("MessageBreak", FTextBlockStyle(DefaultText)
		.SetFont(FSlateFontInfo(
		FriendStyle.FriendsFontStyleSmall.FontObject,
		6,
		FriendStyle.FriendsFontStyleSmall.TypefaceFontName
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