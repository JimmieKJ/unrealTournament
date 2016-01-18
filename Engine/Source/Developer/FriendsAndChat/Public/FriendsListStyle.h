// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendsFontStyle.h"
#include "FriendsListStyle.generated.h"

/**
 * Interface for the services manager.
 */
USTRUCT()
struct FRIENDSANDCHAT_API FFriendsListStyle
	: public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	// Default Constructor
	FFriendsListStyle() { }

	// Default Destructor
	virtual ~FFriendsListStyle() { }

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
	static const FFriendsListStyle& GetDefault();

	// Friends List Style
	UPROPERTY( EditAnywhere, Category = Appearance )
	FButtonStyle GlobalChatButtonStyle;
	FFriendsListStyle& SetGlobalChatButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Open Button style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FButtonStyle FriendItemButtonStyle;
	FFriendsListStyle& SetFriendItemButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FButtonStyle ConfirmButtonStyle;
	FFriendsListStyle& SetConfirmButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FButtonStyle CancelButtonStyle;
	FFriendsListStyle& SetCancelButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateColor ButtonContentColor;
	FFriendsListStyle& SetButtonContentColor(const FSlateColor& InColor);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateColor ButtonHoverContentColor;
	FFriendsListStyle& SetButtonHoverContentColor(const FSlateColor& InColor);

	/** Optional content for the Add Friend button */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ActionMenuArrowBrush;
	FFriendsListStyle& SetActionMenuArrowBrush(const FSlateBrush& BrushStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ToolTipArrowBrush;
	FFriendsListStyle& SetToolTipArrowBrush(const FSlateBrush& BrushStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FButtonStyle BackButtonStyle;
	FFriendsListStyle& SetBackButtonStyle(const FButtonStyle& ButtonStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FButtonStyle HeaderButtonStyle;
	FFriendsListStyle& SetHeaderButtonStyle(const FButtonStyle& ButtonStyle);

	/** Friends List Action Button style */
	UPROPERTY( EditAnywhere, Category = Appearance )
	FButtonStyle FriendListActionButtonStyle;
	FFriendsListStyle& SetFriendListActionButtonStyle(const FButtonStyle& ButtonStyle);

	/** Optional content for the Add Friend button */
	UPROPERTY( EditAnywhere, Category = Appearance )
	FSlateBrush AddFriendButtonContentBrush;
	FFriendsListStyle& SetAddFriendButtonContentBrush(const FSlateBrush& BrushStyle);

	/** Friend Image brush style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush StatusIconBrush;
	FFriendsListStyle& SetStatusIconBrush(const FSlateBrush& BrushStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush PCIconBrush;
	FFriendsListStyle& SetPCIconBrush(const FSlateBrush& BrushStyle);

	/** Friend Image brush style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendImageBrush;
	FFriendsListStyle& SetFriendImageBrush(const FSlateBrush& BrushStyle);

	/** Offline brush style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush OfflineBrush;
	FFriendsListStyle& SetOfflineBrush(const FSlateBrush& InOffLine);

	/** Online brush style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush OnlineBrush;
	FFriendsListStyle& SetOnlineBrush(const FSlateBrush& InOnLine);

	/** Away brush style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateBrush AwayBrush;
	FFriendsListStyle& SetAwayBrush(const FSlateBrush& AwayBrush);

	/** Friends window background */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendsContainerBackground;
	FFriendsListStyle& SetFriendContainerBackground(const FSlateBrush& InFriendContainerBackground);

	/** Friends window background */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FriendsListBackground;
	FFriendsListStyle& SetFriendsListBackground(const FSlateBrush& InBrush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FEditableTextBoxStyle AddFriendEditableTextStyle;
	FFriendsListStyle& SetAddFriendEditableTextStyle(const FEditableTextBoxStyle& InEditableTextStyle);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FVector2D UserPresenceImageSize;
	FFriendsListStyle& SetUserPresenceImageSize(const FVector2D& InUserPresenceImageSize);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush BackBrush;
	FFriendsListStyle& SetBackBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush SelectedOptionBrush;
	FFriendsListStyle& SetSelectedOptionBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush SettingsBrush;
	FFriendsListStyle& SetSettingsBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush SeperatorBrush;
	FFriendsListStyle& SetSeperatorBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush FontSizeBrush;
	FFriendsListStyle& SetFontSizeBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush SearchBrush;
	FFriendsListStyle& SetSearchBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin BackButtonMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin HeaderButtonMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendsListMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin BackButtonContentMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendsListNoFriendsMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendsListHeaderMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendsListHeaderCountMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin HeaderButtonContentMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendItemMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendItemStatusMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendItemPresenceMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendItemPlatformMargin;
	
	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin FriendItemTextScrollerMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin ConfirmationBorderMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin ConfirmationButtonMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin ConfirmationButtonContentMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin SubMenuBackIconMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin SubMenuPageIconMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin RadioSettingTitleMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin SubMenuSearchIconMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin SubMenuSearchTextMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin SubMenuBackButtonMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin SubMenuSettingButtonMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin SubMenuListMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	float SubMenuSeperatorThickness;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin ToolTipMargin;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin TipStatusMargin;


// Clan Settings

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ClanDetailsBrush;
	FFriendsListStyle& SetClanDetailsBrush(const FSlateBrush& Brush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ClanMembersBrush;
	FFriendsListStyle& SetClanMembersBrush(const FSlateBrush& Brush);
};

