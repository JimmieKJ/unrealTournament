// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendsAndChatStyle.generated.h"

UENUM()
namespace EChatChannelStyle
{
	enum Type
	{
		List,
		Conversation
	};
}

/**
 * Interface for the services manager.
 */
USTRUCT()
struct FRIENDSANDCHAT_API FFriendsAndChatStyle
	: public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	// Default Constructor
	FFriendsAndChatStyle() { }

	// Default Destructor
	virtual ~FFriendsAndChatStyle() { }

	/**
	 * Override widget style function.
	 */
	virtual void GetResources( TArray< const FSlateBrush* >& OutBrushes ) const override { }

	// Holds the widget type name
	static const FName TypeName;

	/**
	 * Get the type name.
	 * @return the type name
	 */
	virtual const FName GetTypeName() const override { return TypeName; };

	/**
	 * Get the default style.
	 * @return the default style
	 */
	static const FFriendsAndChatStyle& GetDefault();

	/** Friends List Open Button style */
	UPROPERTY()
	FButtonStyle FriendListOpenButtonStyle;
	FFriendsAndChatStyle& SetFriendsListOpenButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends General Purpose Button style */
	UPROPERTY()
	FButtonStyle FriendGeneralButtonStyle;
	FFriendsAndChatStyle& SetFriendGeneralButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY()
	FSlateColor ButtonInvertedForegroundColor;
	FFriendsAndChatStyle& SetButtonInvertedForegroundColor(const FSlateColor& Value);

	UPROPERTY()
	FSlateColor ButtonForegroundColor;
	FFriendsAndChatStyle& SetButtonForegroundColor(const FSlateColor& Value);

	/** Friends List Action Button style */
	UPROPERTY()
	FButtonStyle FriendListActionButtonStyle;
	FFriendsAndChatStyle& SetFriendListActionButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Critical Button style */
	UPROPERTY()
	FButtonStyle FriendListCriticalButtonStyle;
	FFriendsAndChatStyle& SetFriendsListCriticalButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Emphasis Button style */
	UPROPERTY()
	FButtonStyle FriendListEmphasisButtonStyle;
	FFriendsAndChatStyle& SetFriendsListEmphasisButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Combo Button style */
	UPROPERTY()
	FComboButtonStyle FriendListComboButtonStyle;
	FFriendsAndChatStyle& SetFriendsListComboButtonStyle(const FComboButtonStyle& ButtonStyle);

	UPROPERTY()
	FButtonStyle ComboItemButtonStyle;
	FFriendsAndChatStyle& SetComboItemButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY()
	FButtonStyle FriendListItemButtonStyle;
	FFriendsAndChatStyle& SetFriendsListItemButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY()
	FButtonStyle FriendListItemButtonSimpleStyle;
	FFriendsAndChatStyle& SetFriendsListItemButtonSimpleStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY()
	FSlateBrush GlobalChatHeaderBrush;
	FFriendsAndChatStyle& SetGlobalChatHeaderBrush(const FSlateBrush& Value);

	UPROPERTY()
	FButtonStyle GlobalChatButtonStyle;
	FFriendsAndChatStyle& SetGlobalChatButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Close button style */
	UPROPERTY()
	FButtonStyle FriendListCloseButtonStyle;
	FFriendsAndChatStyle& SetFriendsListClosedButtonStyle(const FButtonStyle& ButtonStyle);

	/** Add Friend Close button style */
	UPROPERTY()
	FButtonStyle AddFriendCloseButtonStyle;
	FFriendsAndChatStyle& SetAddFriendCloseButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Combo Button menu background image (left) */
	UPROPERTY()
	FSlateBrush FriendComboBackgroundLeftBrush;
	FFriendsAndChatStyle& SetFriendComboBackgroundLeftBrush(const FSlateBrush& BrushStyle);

	/** Friends List Combo Button menu background image (right) */
	UPROPERTY()
	FSlateBrush FriendComboBackgroundRightBrush;
	FFriendsAndChatStyle& SetFriendComboBackgroundRightBrush(const FSlateBrush& BrushStyle);

	/** Friends List Combo Button menu background image (left-flipped) - for MenuPlacement_ComboBoxRight menus */
	UPROPERTY()
	FSlateBrush FriendComboBackgroundLeftFlippedBrush;
	FFriendsAndChatStyle& SetFriendComboBackgroundLeftFlippedBrush(const FSlateBrush& BrushStyle);

	/** Friends List Combo Button menu background image (right-flipped) - for MenuPlacement_ComboBoxRight menus */
	UPROPERTY()
	FSlateBrush FriendComboBackgroundRightFlippedBrush;
	FFriendsAndChatStyle& SetFriendComboBackgroundRightFlippedBrush(const FSlateBrush& BrushStyle);

	/** Optional content for the Add Friend button */
	UPROPERTY()
	FSlateBrush AddFriendButtonContentBrush;
	FFriendsAndChatStyle& SetAddFriendButtonContentBrush(const FSlateBrush& BrushStyle);

	/** Optional content for the Add Friend button (hovered) */
	UPROPERTY()
	FSlateBrush AddFriendButtonContentHoveredBrush;
	FFriendsAndChatStyle& SetAddFriendButtonContentHoveredBrush(const FSlateBrush& BrushStyle);

	/** Title Bar brush style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush TitleBarBrush;
	FFriendsAndChatStyle& SetTitleBarBrush(const FSlateBrush& BrushStyle);

	/** Friend Image brush style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendImageBrush;
	FFriendsAndChatStyle& SetFriendImageBrush(const FSlateBrush& BrushStyle);

	/** Fortnite Image brush style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FortniteImageBrush;
	FFriendsAndChatStyle& SetFortniteImageBrush(const FSlateBrush& BrushStyle);

	/** Launcher Image brush style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush LauncherImageBrush;
	FFriendsAndChatStyle& SetLauncherImageBrush(const FSlateBrush& BrushStyle);

	/** UnrealTournament Image brush style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush UTImageBrush;
	FFriendsAndChatStyle& SetUTImageBrush(const FSlateBrush& BrushStyle);

	/** Friend combo dropdown Image */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendsComboDropdownImageBrush;
	FFriendsAndChatStyle& SetFriendsComboDropdownImageBrush(const FSlateBrush& BrushStyle);

	/** Friends Add Icon */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendsAddImageBrush;
	FFriendsAndChatStyle& SetFriendsAddImageBrush(const FSlateBrush& BrushStyle);

	/** Friend combo callout Image */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendsCalloutBrush;
	FFriendsAndChatStyle& SetFriendsCalloutBrush(const FSlateBrush& BrushStyle);

	/** Offline brush style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush OfflineBrush;
	FFriendsAndChatStyle& SetOfflineBrush(const FSlateBrush& InOffLine);

	/** Online brush style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush OnlineBrush;
	FFriendsAndChatStyle& SetOnlineBrush(const FSlateBrush& InOnLine);

	/** Away brush style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush AwayBrush;
	FFriendsAndChatStyle& SetAwayBrush(const FSlateBrush& AwayBrush);

	/** Window background style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush Background;
	FFriendsAndChatStyle& SetBackgroundBrush(const FSlateBrush& InBackground);

	/** Window edging style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush WindowEdgingBrush;
	FFriendsAndChatStyle& SetWindowEdgingBrush(const FSlateBrush& Value);

	/** Friend container header */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendContainerHeader;
	FFriendsAndChatStyle& SetFriendContainerHeader(const FSlateBrush& InFriendContainerHeader);

	/** Friend list header */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendListHeader;
	FFriendsAndChatStyle& SetFriendListHeader(const FSlateBrush& InFriendListHeader);

	/** Friend user header background */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendUserHeaderBackground;
	FFriendsAndChatStyle& SetFriendUserHeaderBackground(const FSlateBrush& InFriendUserHeaderBackground);

	/** Friend item selected */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendItemSelected;
	FFriendsAndChatStyle& SetFriendItemSelected(const FSlateBrush& InFriendItemSelected);

	/** Chat window background */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatContainerBackground;
	FFriendsAndChatStyle& SetChatContainerBackground(const FSlateBrush& InChatContainerBackground);

	/** Friends window background */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendsContainerBackground;
	FFriendsAndChatStyle& SetFriendContainerBackground(const FSlateBrush& InFriendContainerBackground);

	/** Add Friend editable text border */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush AddFriendEditBorder;
	FFriendsAndChatStyle& SetAddFriendEditBorder(const FSlateBrush& InAddFriendEditBorder);

	/** Text Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FTextBlockStyle TextStyle;
	FFriendsAndChatStyle& SetTextStyle(const FTextBlockStyle& InTextStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontStyle;
	FFriendsAndChatStyle& SetFontStyle(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontStyleBold;
	FFriendsAndChatStyle& SetFontStyleBold(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateFontInfo FriendsFontStyleUserLarge;
	FFriendsAndChatStyle& SetFontStyleUserLarge(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontStyleSmall;
	FFriendsAndChatStyle& SetFontStyleSmall(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontStyleSmallBold;
	FFriendsAndChatStyle& SetFontStyleSmallBold(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateFontInfo ChatFontStyleEntry;
	FFriendsAndChatStyle& SetChatFontStyleEntry(const FSlateFontInfo& InFontStyle);

	/** Default Font Color */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FLinearColor DefaultFontColor;
	FFriendsAndChatStyle& SetDefaultFontColor(const FLinearColor& InFontColor);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor DefaultDullFontColor;
	FFriendsAndChatStyle& SetDefaultDullFontColor(const FLinearColor& InFontColor);

	UPROPERTY(EditAnywhere, Category=Appearance)
	FLinearColor DefaultChatColor;
	FFriendsAndChatStyle& SetDefaultChatColor(const FLinearColor& InFontColor);

	UPROPERTY(EditAnywhere, Category=Appearance)
	FLinearColor WhisplerChatColor;
	FFriendsAndChatStyle& SetWhisplerChatColor(const FLinearColor& InFontColor);

	UPROPERTY(EditAnywhere, Category=Appearance)
	FLinearColor PartyChatColor;
	FFriendsAndChatStyle& SetPartyChatColor(const FLinearColor& InFontColor);

	UPROPERTY(EditAnywhere, Category=Appearance)
	FLinearColor NetworkChatColor;
	FFriendsAndChatStyle& SetNetworkChatColor(const FLinearColor& InFontColor);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor ComboItemTextColorNormal;
	FFriendsAndChatStyle& SetComboItemTextColorNormal(const FLinearColor& InColor);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor ComboItemTextColorHovered;
	FFriendsAndChatStyle& SetComboItemTextColorHovered(const FLinearColor& InColor);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor FriendListActionFontColor;
	FFriendsAndChatStyle& SetFriendListActionFontColor(const FLinearColor& InColor);

	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush ChatGlobalBrush;
	FFriendsAndChatStyle& SetChatGlobalBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush ChatPartyBrush;
	FFriendsAndChatStyle& SetChatPartyBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush ChatWhisperBrush;
	FFriendsAndChatStyle& SetChatWhisperBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatInvalidBrush;
	FFriendsAndChatStyle& SetChatInvalidBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FEditableTextBoxStyle AddFriendEditableTextStyle;
	FFriendsAndChatStyle& SetAddFriendEditableTextStyle(const FEditableTextBoxStyle& InEditableTextStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FTextBlockStyle ComboItemTextStyle;
	FFriendsAndChatStyle& SetComboItemTextStyle(const FTextBlockStyle& InTextStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FTextBlockStyle FriendsComboTextStyle;
	FFriendsAndChatStyle& SetFriendsComboTextStyle(const FTextBlockStyle& InTextStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FEditableTextBoxStyle ChatEditableTextStyle;
	FFriendsAndChatStyle& SetChatEditableTextStyle(const FEditableTextBoxStyle& InEditableTextStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FCheckBoxStyle FriendCheckboxStyle;
	FFriendsAndChatStyle& SetFriendCheckboxStyle(const FCheckBoxStyle& InFriendCheckboxStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FVector2D StatusButtonSize;
	FFriendsAndChatStyle& SetStatusButtonSize(const FVector2D& InStatusButtonSize);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FVector2D ActionComboButtonSize;
	FFriendsAndChatStyle& SetActionComboButtonSize(const FVector2D& InActionComboButtonSize);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FComboButtonStyle ActionComboButtonStyle;
	FFriendsAndChatStyle& SetActionComboButtonStyle(const FComboButtonStyle& ButtonStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FTextBlockStyle ActionComboButtonTextStyle;
	FFriendsAndChatStyle& SetActionComboButtonTextStyle(const FTextBlockStyle& TextStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FVector2D UserPresenceImageSize;
	FFriendsAndChatStyle& SetUserPresenceImageSize(const FVector2D& InUserPresenceImageSize);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin BorderPadding;
	FFriendsAndChatStyle& SetBorderPadding(const FMargin& InBorderPadding);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin UserHeaderPadding;
	FFriendsAndChatStyle& SetUserHeaderPadding(const FMargin& InUserHeaderPadding);

	UPROPERTY(EditAnywhere, Category = Appearance)
	float FriendsListWidth;
	FFriendsAndChatStyle& SetFriendsListWidth(const float FriendsListLength);

	UPROPERTY(EditAnywhere, Category = Appearance)
	float ChatListPadding;
	FFriendsAndChatStyle& SetChatListPadding(const float ChatListPadding);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatBackgroundBrush;
	FFriendsAndChatStyle& SetChatBackgroundBrush(const FSlateBrush& InChatBackgroundBrush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatBubbleBackgroundBrush;
	FFriendsAndChatStyle& SetChatBubbleBackgroundBrush(const FSlateBrush& Value);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatBubbleLeftCallout;
	FFriendsAndChatStyle& SetChatBubbleLeftCalloutBrush(const FSlateBrush& Value);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatBubbleRightCallout;
	FFriendsAndChatStyle& SetChatBubbleRightCalloutBrush(const FSlateBrush& Value);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatFooterBrush;
	FFriendsAndChatStyle& SetChatFooterBrush(const FSlateBrush& Value);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatChannelsBackgroundBrush;
	FFriendsAndChatStyle& SetChatChannelsBackgroundBrush(const FSlateBrush& InChatChannelsBackgroundBrush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatOptionsBackgroundBrush;
	FFriendsAndChatStyle& SetChatOptionsBackgroundBrush(const FSlateBrush& InChatOptionsBackgroundBrush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FScrollBarStyle ScrollBarStyle;
	FFriendsAndChatStyle& SetScrollbarStyle(const FScrollBarStyle& InScrollBarStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FWindowStyle WindowStyle;
	FFriendsAndChatStyle& SetWindowStyle(const FWindowStyle& InStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin ComboMenuPadding;
	FFriendsAndChatStyle& SetComboMenuPadding(const FMargin& InPadding);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin ComboItemPadding;
	FFriendsAndChatStyle& SetComboItemPadding(const FMargin& InPadding);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin ComboItemContentPadding;
	FFriendsAndChatStyle& SetComboItemContentPadding(const FMargin& InPadding);

	UPROPERTY(EditAnywhere, Category = Appearance)
	bool HasUserHeader;
	FFriendsAndChatStyle& SetHasUserHeader(bool InHasUserHeader);

	UPROPERTY(EditAnywhere, Category = Appearance)
	TEnumAsByte<EChatChannelStyle::Type> ChatChannelStyle;
	FFriendsAndChatStyle&  SetChatWindowStyle(const EChatChannelStyle::Type Style);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ClanDetailsBrush;
	FFriendsAndChatStyle& SetClanDetailsBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ClanMembersBrush;
	FFriendsAndChatStyle& SetClanMembersBrush(const FSlateBrush& Brush);
};

/** Manages the style which provides resources for the rich text widget. */
class FRIENDSANDCHAT_API FFriendsAndChatModuleStyle
{
public:

	static void Initialize(FFriendsAndChatStyle FriendStyle);

	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set for the Friends and chat module */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef< class FSlateStyleSet > Create(FFriendsAndChatStyle FriendStyle);

private:

	static TSharedPtr< class FSlateStyleSet > FriendsAndChatModuleStyleInstance;
};
