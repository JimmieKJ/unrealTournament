// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


struct FSlateBrush;


/**
 * Struct used to represent an icon in Slate
 */
struct SLATE_API FSlateIcon
{
	/**
	 * Default constructor (empty icon).
	 */
	FSlateIcon( );

	/**
	 * Creates and initializes a new icon from a style set and style name
	 *
	 * @param InStyleSetName The name of the style set the icon can be found in.
	 * @param StyleName The name of the style for the icon (assumes there may also be a ".Small" variant)
	 */
	FSlateIcon( const FName& InStyleSetName, const FName& InStyleName );

	/**
	 * Creates and initializes a new icon from a style set and style name
	 *
	 * @param InStyleSetName The name of the style set the icon can be found in.
	 * @param StyleName The name of the style for the icon
	 * @param InSmallStyleName The name of the style for the small icon
	 */
	FSlateIcon( const FName& InStyleSetName, const FName& InStyleName, const FName& InSmallStyleName );

public:

	/**
	 * Gets the resolved icon.
	 *
	 * @return Icon brush, or FStyleDefaults::GetNoBrush() if the icon wasn't found.
	 * @see GetSmallIcon
	 */
	const FSlateBrush* GetIcon( ) const;

	/**
	 * Gets the resolved small icon.
	 *
	 * @return Icon brush, or FStyleDefaults::GetNoBrush() if the icon wasn't found.
	 * @see GetIcon
	 */
	const FSlateBrush* GetSmallIcon( ) const;

	/**
	 * Gets the name of the style for the icon.
	 *
	 * @return Style name.
	 * @see GetStyleName, GetStyleSet, GetStyleSetName
	 */
	const FName& GetSmallStyleName( ) const
	{
		return SmallStyleName;
	}

	/**
	 * Gets the name of the style for the icon.
	 *
	 * @return Style name.
	 * @see GetSmallStyleName, GetStyleSet, GetStyleSetName
	 */
	const FName& GetStyleName( ) const
	{
		return StyleName;
	}

	/**
	 * Gets the resolved style set.
	 *
	 * @return Style set, or nullptr if the style set wasn't found.
	 * @see GetSmallStyleName, GetStyleName, GetStyleSetName
	 */
	const class ISlateStyle* GetStyleSet( ) const;

	/**
	 * Gets the name of the style set the icon can be found in.
	 *
	 * @return Style name.
	 * @see GetSmallStyleName, GetStyleName, GetStyleSet
	 */
	const FName& GetStyleSetName( ) const
	{
		return StyleSetName;
	}

	/**
	 * Checks whether the icon is set to something.
	 *
	 * @return true if the icon is set, false otherwise.
	 */
	const bool IsSet( ) const
	{
		return bIsSet;
	}

private:

	// The name of the style set the icon can be found in.
	FName StyleSetName;

	// The name of the style for the icon.
	FName StyleName;

	// The name of the style for the small icon.
	FName SmallStyleName;

	// Whether this icon has been explicitly set-up.
	bool bIsSet;
};
