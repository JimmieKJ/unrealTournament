// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendsAndChatStyle.generated.h"


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

	/** Friends List Online Status Button style */
	UPROPERTY()
	FButtonStyle FriendListStatusButtonStyle;
	FFriendsAndChatStyle& SetFriendsListStatusButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Action Button style */
	UPROPERTY()
	FButtonStyle FriendListActionButtonStyle;
	FFriendsAndChatStyle& SetFriendsListActionButtonStyle(const FButtonStyle& ButtonStyle);

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
	FButtonStyle FriendListComboButtonStyle;
	FFriendsAndChatStyle& SetFriendsListComboButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY()
	FButtonStyle FriendListItemButtonStyle;
	FFriendsAndChatStyle& SetFriendsListItemButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY()
	FButtonStyle FriendListItemButtonSimpleStyle;
	FFriendsAndChatStyle& SetFriendsListItemButtonSimpleStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Close button style */
	UPROPERTY()
	FButtonStyle FriendListCloseButtonStyle;
	FFriendsAndChatStyle& SetFriendsListClosedButtonStyle(const FButtonStyle& ButtonStyle);

	/** Add Friend Close button style */
	UPROPERTY()
	FButtonStyle AddFriendCloseButtonStyle;
	FFriendsAndChatStyle& SetAddFriendCloseButtonStyle(const FButtonStyle& ButtonStyle);

	/** Add Friend button style */
	UPROPERTY()
	FButtonStyle AddFriendButtonStyle;
	FFriendsAndChatStyle& SetAddFriendButtonStyle(const FButtonStyle& ButtonStyle);

	/** Optional content for the Add Friend button */
	UPROPERTY()
	FSlateBrush AddFriendButtonContentBrush;
	FFriendsAndChatStyle& SetAddFriendButtonContentBrush(const FSlateBrush& BrushStyle);

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

	/** Friend container header */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendContainerHeader;
	FFriendsAndChatStyle& SetFriendContainerHeader(const FSlateBrush& InFriendContainerHeader);

	/** Friend list header */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendListHeader;
	FFriendsAndChatStyle& SetFriendListHeader(const FSlateBrush& InFriendListHeader);

	/** Friend item selected */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendItemSelected;
	FFriendsAndChatStyle& SetFriendItemSelected(const FSlateBrush& InFriendItemSelected);

	/** Friend container background */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendContainerBackground;
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
	FEditableTextBoxStyle AddFriendEditableTextStyle;
	FFriendsAndChatStyle& SetAddFriendEditableTextStyle(const FEditableTextBoxStyle& InEditableTextStyle);

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
	FMargin BorderPadding;
	FFriendsAndChatStyle& SetBorderPadding(const FMargin& BorderPadding);

	UPROPERTY(EditAnywhere, Category = Appearance)
	float FriendsListWidth;
	FFriendsAndChatStyle& SetFriendsListWidth(const float FriendsListLength);

	UPROPERTY(EditAnywhere, Category = Appearance)
	float ChatListWidth;
	FFriendsAndChatStyle& SetChatListWidth(const float ChatListWidth);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatBackgroundBrush;
	FFriendsAndChatStyle& SetChatBackgroundBrush(const FSlateBrush& InChatBackgroundBrush);

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
};

/** Manages the style which provides resources for the rich text widget. */
class FRIENDSANDCHAT_API FFriendsAndChatModuleStyle
{
public:

	static void Initialize(FFriendsAndChatStyle FriendStyle);

	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set for the Shooter game */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef< class FSlateStyleSet > Create(FFriendsAndChatStyle FriendStyle);

private:

	static TSharedPtr< class FSlateStyleSet > FriendsAndChatModuleStyleInstance;
};
