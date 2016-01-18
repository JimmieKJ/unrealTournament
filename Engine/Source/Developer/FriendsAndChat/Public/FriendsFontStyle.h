// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendsFontStyle.generated.h"

/**
 * Interface for the services manager.
 */
USTRUCT()
struct FRIENDSANDCHAT_API FFriendsFontStyle
	: public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	// Default Constructor
	FFriendsFontStyle() { }

	// Default Destructor
	virtual ~FFriendsFontStyle() { }

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
	static const FFriendsFontStyle& GetDefault();

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontSmall;
	FFriendsFontStyle& SetFontSmall(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontSmallBold;
	FFriendsFontStyle& SetFontSmallBold(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontNormal;
	FFriendsFontStyle& SetFontNormal(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontNormalBold;
	FFriendsFontStyle& SetFontNormalBold(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateFontInfo FriendsFontLarge;
	FFriendsFontStyle& SetFontLarge(const FSlateFontInfo& InFontStyle);

	/** Font Style */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo FriendsFontLargeBold;
	FFriendsFontStyle& SetFontLargeBold(const FSlateFontInfo& InFontStyle);

	/** Default Font Color */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FLinearColor DefaultFontColor;
	FFriendsFontStyle& SetDefaultFontColor(const FLinearColor& InFontColor);

	UPROPERTY(EditAnywhere, Category=Appearance)
	FLinearColor InvertedFontColor;
	FFriendsFontStyle& SetInvertedFontColor(const FLinearColor& InFontColor);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor DefaultDullFontColor;
	FFriendsFontStyle& SetDefaultDullFontColor(const FLinearColor& InFontColor);
};

