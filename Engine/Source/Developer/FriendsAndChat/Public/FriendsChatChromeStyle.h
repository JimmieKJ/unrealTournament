// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendsChatChromeStyle.generated.h"

/**
 * Interface for the services manager.
 */
USTRUCT()
struct FRIENDSANDCHAT_API FFriendsChatChromeStyle
	: public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	// Default Constructor
	FFriendsChatChromeStyle() { }

	// Default Destructor
	virtual ~FFriendsChatChromeStyle() { }

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
	static const FFriendsChatChromeStyle& GetDefault();

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush ChatBackgroundBrush;
	FFriendsChatChromeStyle& SetChatBackgroundBrush(const FSlateBrush& InChatBackgroundBrush);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush TabBackgroundBrush;
	FFriendsChatChromeStyle& SetTabBackgroundBrush(const FSlateBrush& InTabBackgroundBrush);

};

