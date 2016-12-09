// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Fonts/SlateFontInfo.h"
#include "Templates/Casts.h"
#include "SlateGlobals.h"
#include "Fonts/FontProviderInterface.h"
#include "Fonts/LegacySlateFontInfoCache.h"


/* FSlateFontInfo structors
 *****************************************************************************/

FFontOutlineSettings FFontOutlineSettings::NoOutline;

FSlateFontInfo::FSlateFontInfo( )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(0)
	, FontName_DEPRECATED()
	, Hinting_DEPRECATED(EFontHinting::Default)
	, FontFallback(EFontFallback::FF_Max)
{
}


FSlateFontInfo::FSlateFontInfo( TSharedPtr<const FCompositeFont> InCompositeFont, const int32 InSize, const FName& InTypefaceFontName )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont(InCompositeFont)
	, TypefaceFontName(InTypefaceFontName)
	, Size(InSize)
	, FontName_DEPRECATED()
	, Hinting_DEPRECATED(EFontHinting::Default)
	, FontFallback(EFontFallback::FF_Max)
{
	if (!InCompositeFont.IsValid())
	{
		UE_LOG(LogSlate, Warning, TEXT("FSlateFontInfo was constructed with a null FCompositeFont. Slate will be forced to use the fallback font path which may be slower."));
	}
}


FSlateFontInfo::FSlateFontInfo( const UObject* InFontObject, const int32 InSize, const FName& InTypefaceFontName )
	: FontObject(InFontObject)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName(InTypefaceFontName)
	, Size(InSize)
	, FontName_DEPRECATED()
	, Hinting_DEPRECATED(EFontHinting::Default)
	, FontFallback(EFontFallback::FF_Max)
{
	if (InFontObject)
	{
		const IFontProviderInterface* FontProvider = Cast<const IFontProviderInterface>(InFontObject);
		if (!FontProvider || !FontProvider->GetCompositeFont())
		{
			UE_LOG(LogSlate, Verbose, TEXT("'%s' does not provide a composite font that can be used with Slate. Slate will be forced to use the fallback font path which may be slower."), *InFontObject->GetName());
		}
	}
	else
	{
		UE_LOG(LogSlate, Warning, TEXT("FSlateFontInfo was constructed with a null UFont. Slate will be forced to use the fallback font path which may be slower."));
	}
}


FSlateFontInfo::FSlateFontInfo( const FString& InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontName_DEPRECATED(*InFontName)
	, Hinting_DEPRECATED(InHinting)
	, FontFallback(EFontFallback::FF_Max)
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo();
}


FSlateFontInfo::FSlateFontInfo( const FName& InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
	, FontFallback(EFontFallback::FF_Max)
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo();
}


FSlateFontInfo::FSlateFontInfo( const ANSICHAR* InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
	, FontFallback(EFontFallback::FF_Max)
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo();
}


FSlateFontInfo::FSlateFontInfo( const WIDECHAR* InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
	, FontFallback(EFontFallback::FF_Max)
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
		const FCompositeFont* const ProvidedCompositeFont = FontProvider->GetCompositeFont();
		return (ProvidedCompositeFont) ? ProvidedCompositeFont : FLegacySlateFontInfoCache::Get().GetLastResortFont().Get();
	}

	if (CompositeFont.IsValid())
	{
		return CompositeFont.Get();
	}

	return FLegacySlateFontInfoCache::Get().GetLastResortFont().Get();
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
