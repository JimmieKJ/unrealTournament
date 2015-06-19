// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/TypeHash.h"
#include "CompositeFont.h"
#include "SlateFontInfo.generated.h"

/**
 * A representation of a font in Slate.
 */
USTRUCT(BlueprintType)
struct SLATECORE_API FSlateFontInfo
{
	GENERATED_USTRUCT_BODY()

	/** The font object (valid when used from UMG or a Slate widget style asset) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules, meta=(AllowedClasses="Font", DisplayName="Font Family"))
	const UObject* FontObject;

	/** The composite font data to use (valid when used with a Slate style set in C++) */
	TSharedPtr<const FCompositeFont> CompositeFont;

	/** The name of the font to use from the default typeface (None will use the first entry) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules, meta=(DisplayName="Font"))
	FName TypefaceFontName;

	/** The size of the font */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules)
	int32 Size;

private:

	/** The name of the font */
	UPROPERTY()
	FName FontName_DEPRECATED;

	/** The hinting algorithm to use with the font */
	UPROPERTY()
	EFontHinting Hinting_DEPRECATED;

public:

	/** Default constructor. */
	FSlateFontInfo();

	/**
	 * Creates and initializes a new instance with the specified font, size, and emphasis.
	 *
	 * @param InCompositeFont The font instance to use.
	 * @param InSize The size of the font.
	 * @param InTypefaceFontName The name of the font to use from the default typeface (None will use the first entry)
	 */
	FSlateFontInfo( TSharedPtr<const FCompositeFont> InCompositeFont, const int32 InSize, const FName& InTypefaceFontName = NAME_None );

	/**
	 * Creates and initializes a new instance with the specified font, size, and emphasis.
	 *
	 * @param InFontObject The font instance to use.
	 * @param InSize The size of the font.
	 * @param InFamilyFontName The name of the font to use from the default typeface (None will use the first entry)
	 */
	FSlateFontInfo( const UObject* InFontObject, const int32 InSize, const FName& InTypefaceFontName = NAME_None );

	/**
	 * DEPRECATED - Creates and initializes a new instance with the specified font name and size.
	 *
	 * @param InFontName The name of the font.
	 * @param InSize The size of the font.
	 * @param InHinting The type of hinting to use for the font.
	 */
	FSlateFontInfo( const FString& InFontName, uint16 InSize, EFontHinting InHinting = EFontHinting::Default );

	/**
	 * DEPRECATED - Creates and initializes a new instance with the specified font name and size.
	 *
	 * @param InFontName The name of the font.
	 * @param InSize The size of the font.
	 * @param InHinting The type of hinting to use for the font.
	 */
	FSlateFontInfo( const FName& InFontName, uint16 InSize, EFontHinting InHinting = EFontHinting::Default );

	/**
	 * DEPRECATED - Creates and initializes a new instance with the specified font name and size.
	 *
	 * @param InFontName The name of the font.
	 * @param InSize The size of the font.
	 * @param InHinting The type of hinting to use for the font.
	 */
	FSlateFontInfo( const ANSICHAR* InFontName, uint16 InSize, EFontHinting InHinting = EFontHinting::Default );

	/**
	 * DEPRECATED - Creates and initializes a new instance with the specified font name and size.
	 *
	 * @param InFontName The name of the font.
	 * @param InSize The size of the font.
	 * @param InHinting The type of hinting to use for the font.
	 */
	FSlateFontInfo( const WIDECHAR* InFontName, uint16 InSize, EFontHinting InHinting = EFontHinting::Default );

public:

	/**
	 * Compares this font info with another for equality.
	 *
	 * @param Other The other font info.
	 * @return true if the two font infos are equal, false otherwise.
	 */
	bool operator==( const FSlateFontInfo& Other ) const 
	{
		return FontObject == Other.FontObject
			&& CompositeFont == Other.CompositeFont 
			&& TypefaceFontName == Other.TypefaceFontName
			&& Size == Other.Size;
	}

	/**
	 * Compares this font info with another for inequality.
	 *
	 * @param Other The other font info.
	 *
	 * @return false if the two font infos are equal, true otherwise.
	 */
	bool operator!=( const FSlateFontInfo& Other ) const 
	{
		return !(*this == Other);
	}

	/**
	 * Check to see whether this font info has a valid composite font pointer set (either directly or via a UFont)
	 */
	bool HasValidFont() const;

	/**
	 * Get the composite font pointer associated with this font info (either directly or via a UFont)
	 * @note This function will return the fallback font if this font info itself does not contain a valid font. If you want to test whether this font info is empty, use HasValidFont
	 */
	const FCompositeFont* GetCompositeFont() const;

	/**
	 * Calculates a type hash value for a font info.
	 *
	 * Type hashes are used in certain collection types, such as TMap.
	 *
	 * @param FontInfo The font info to calculate the hash for.
	 * @return The hash value.
	 */
	friend inline uint32 GetTypeHash( const FSlateFontInfo& FontInfo )
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.FontObject));
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.CompositeFont));
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.TypefaceFontName));
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.Size));
		return Hash;
	}

	/**
	 * Used to upgrade legacy font into so that it uses composite fonts
	 */
	void PostSerialize(const FArchive& Ar);

private:

	/**
	 * Used to upgrade legacy font into so that it uses composite fonts
	 */
	void UpgradeLegacyFontInfo();
};

template<>
struct TStructOpsTypeTraits<FSlateFontInfo>
	: public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithPostSerialize = true,
	};
};
