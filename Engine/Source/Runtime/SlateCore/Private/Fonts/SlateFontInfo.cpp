// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "FontProviderInterface.h"
#include "LegacySlateFontInfoCache.h"


/* FSlateFontInfo structors
 *****************************************************************************/

FSlateFontInfo::FSlateFontInfo( )
	: FontObject(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(0)
	, FontName_DEPRECATED()
	, Hinting_DEPRECATED(EFontHinting::Default)
{
}


FSlateFontInfo::FSlateFontInfo( TSharedPtr<const FCompositeFont> InCompositeFont, const int32 InSize, const FName& InTypefaceFontName )
	: FontObject(nullptr)
	, CompositeFont(InCompositeFont)
	, TypefaceFontName(InTypefaceFontName)
	, Size(InSize)
	, FontName_DEPRECATED()
	, Hinting_DEPRECATED(EFontHinting::Default)
{
}


FSlateFontInfo::FSlateFontInfo( const UObject* InFontObject, const int32 InSize, const FName& InTypefaceFontName )
	: FontObject(InFontObject)
	, CompositeFont()
	, TypefaceFontName(InTypefaceFontName)
	, Size(InSize)
	, FontName_DEPRECATED()
	, Hinting_DEPRECATED(EFontHinting::Default)
{
}


FSlateFontInfo::FSlateFontInfo( const FString& InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontName_DEPRECATED(*InFontName)
	, Hinting_DEPRECATED(InHinting)
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo();
}


FSlateFontInfo::FSlateFontInfo( const FName& InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo();
}


FSlateFontInfo::FSlateFontInfo( const ANSICHAR* InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo();
}


FSlateFontInfo::FSlateFontInfo( const WIDECHAR* InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo();
}


bool FSlateFontInfo::HasValidFont() const
{
	return CompositeFont.IsValid() || FontObject != nullptr;
}


const FCompositeFont* FSlateFontInfo::GetCompositeFont() const
{
	const IFontProviderInterface* FontProvider = Cast<const IFontProviderInterface>(FontObject);
	if (FontProvider)
	{
		return FontProvider->GetCompositeFont();
	}

	if (CompositeFont.IsValid())
	{
		return CompositeFont.Get();
	}

	return nullptr;
}


void FSlateFontInfo::PostSerialize(const FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_SLATE_COMPOSITE_FONTS && !FontObject)
	{
		UpgradeLegacyFontInfo();
	}
}


void FSlateFontInfo::UpgradeLegacyFontInfo()
{
	static const FName SpecialName_DefaultSystemFont("DefaultSystemFont");

	// Special case for using the default system font
	CompositeFont = (FontName_DEPRECATED == SpecialName_DefaultSystemFont)
		? FLegacySlateFontInfoCache::Get().GetSystemFont()
		: FLegacySlateFontInfoCache::Get().GetCompositeFont(FontName_DEPRECATED, Hinting_DEPRECATED);
}
