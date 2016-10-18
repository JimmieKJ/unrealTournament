// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Font.cpp: Unreal font code.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/Font.h"
#include "Engine/FontImportOptions.h"
#include "SlateBasics.h"
#include "EngineFontServices.h"

#include "EditorFramework/AssetImportData.h"

UFontImportOptions::UFontImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UFont::UFont(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ScalingFactor = 1.0f;
	LegacyFontSize = 9;
}

UFont::~UFont()
{
	if(FontCacheType == EFontCacheType::Runtime && FSlateApplication::IsInitialized())
	{
		TSharedPtr<FSlateRenderer> SlateRenderer = FSlateApplication::Get().GetRenderer();
		if(SlateRenderer.IsValid())
		{
			TSharedRef<FSlateFontCache> FontCache = SlateRenderer->GetFontCache();
			FontCache->FlushObject(this);
		}
	}
}

void UFont::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << CharRemap;

	// Update any composite font data to use bulk data with us as its outer
	if( Ar.UE4Ver() < VER_UE4_SLATE_BULK_FONT_DATA )
	{
		auto UpgradeFontDataToBulk = [this](FFontData& InFontData)
		{
			if( InFontData.FontData_DEPRECATED.Num() > 0 )
			{
				UFontBulkData* FontBulkData = NewObject<UFontBulkData>(this);
				FontBulkData->Initialize(InFontData.FontData_DEPRECATED.GetData(), InFontData.FontData_DEPRECATED.Num());
				InFontData.BulkDataPtr = FontBulkData;					
				InFontData.FontData_DEPRECATED.Empty();
			}
		};

		for( FTypefaceEntry& TypefaceEntry : CompositeFont.DefaultTypeface.Fonts )
		{
			UpgradeFontDataToBulk(TypefaceEntry.Font);
		}

		for( FCompositeSubFont& SubFont : CompositeFont.SubTypefaces )
		{
			for( FTypefaceEntry& TypefaceEntry : SubFont.Typeface.Fonts )
			{
				UpgradeFontDataToBulk(TypefaceEntry.Font);
			}
		}
	}
}

void UFont::PostLoad()
{
	Super::PostLoad();

	// Cache the character count and the maximum character height for this font
	CacheCharacterCountAndMaxCharHeight();

	for( int32 TextureIndex = 0 ; TextureIndex < Textures.Num() ; ++TextureIndex )
	{
		UTexture2D* Texture = Textures[TextureIndex];
		if( Texture )
		{	
			Texture->SetFlags(RF_Public);
			Texture->LODGroup = TEXTUREGROUP_UI;

			// Fix up compression type for distance field fonts.
			if (Texture->CompressionSettings == TC_Displacementmap && !Texture->SRGB)
			{
				Texture->ConditionalPostLoad();
				Texture->CompressionSettings = TC_DistanceFieldFont;
				Texture->UpdateResource();
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
void UFont::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	FAssetImportInfo ImportInfo;

	// Add all the font filenames
	for( const FTypefaceEntry& TypefaceEntry : CompositeFont.DefaultTypeface.Fonts )
	{
		ImportInfo.Insert(FAssetImportInfo::FSourceFile(TypefaceEntry.Font.FontFilename));
	}

	for( const FCompositeSubFont& SubFont : CompositeFont.SubTypefaces )
	{
		for( const FTypefaceEntry& TypefaceEntry : SubFont.Typeface.Fonts )
		{
			ImportInfo.Insert(FAssetImportInfo::FSourceFile(TypefaceEntry.Font.FontFilename));
		}
	}

	OutTags.Add( FAssetRegistryTag(SourceFileTagName(), ImportInfo.ToJson(), FAssetRegistryTag::TT_Hidden) );

	Super::GetAssetRegistryTags(OutTags);
}
#endif

void UFont::CacheCharacterCountAndMaxCharHeight()
{
	// Cache the number of characters in the font.  Obviously this is pretty simple, but note that it will be
	// computed differently for MultiFonts.  We need to cache it so that we have it available in inline functions
	NumCharacters = Characters.Num();

	// Cache maximum character height
	MaxCharHeight.Reset();
	int32 MaxCharHeightForThisFont = 1;
	for( int32 CurCharNum = 0; CurCharNum < NumCharacters; ++CurCharNum )
	{
		MaxCharHeightForThisFont = FMath::Max( MaxCharHeightForThisFont, Characters[ CurCharNum ].VSize );
	}

	// Add to the array
	MaxCharHeight.Add( MaxCharHeightForThisFont );
}

TCHAR UFont::RemapChar(TCHAR CharCode) const
{
	const uint16 UCode = CharCast<UCS2CHAR>(CharCode);
	if ( IsRemapped )
	{
		// currently, fonts are only remapped if they contain Unicode characters.
		// For remapped fonts, all characters in the CharRemap map are valid, so
		// if the characters exists in the map, it's safe to use - otherwise, return
		// the null character (an empty square on windows)
		const uint16* FontChar = CharRemap.Find(UCode);
		if ( FontChar == NULL )
			return NULLCHARACTER;

		return (TCHAR)*FontChar;
	}

	// Otherwise, our Characters array will contains 256 members, and is
	// a one-to-one mapping of character codes to array indexes, though
	// not every character is a valid character.
	if ( UCode >= NumCharacters )
	{
		return NULLCHARACTER;
	}

	// If the character's size is 0, it's non-printable or otherwise unsupported by
	// the font.  Return the default null character (an empty square on windows).
	if ( !Characters.IsValidIndex(UCode) || (Characters[UCode].VSize == 0 && UCode >= TEXT(' ')) )
	{
		return NULLCHARACTER;
	}

	return CharCode;
}

void UFont::GetCharSize(TCHAR InCh, float& Width, float& Height) const
{
	Width = Height = 0.f;

	switch(FontCacheType)
	{
	case EFontCacheType::Offline:
		{
			const int32 Ch = (int32)RemapChar(InCh);
			if( Ch < Characters.Num() )
			{
				const FFontCharacter& Char = Characters[Ch];
				if( Char.TextureIndex < Textures.Num() && Textures[Char.TextureIndex] != NULL )
				{
					Width = Char.USize;

					// The height of the character will always be the maximum height of any character in this
					// font.  This ensures consistent vertical alignment of text.  For example, we don't want
					// vertically centered text to visually shift up and down as characters are added to a string.
					// NOTE: This also gives us consistent alignment with fonts generated by the legacy importer.
					const int32 MultiFontIndex = Ch / NumCharacters;
					Height = MaxCharHeight[ MultiFontIndex ];
				}
			}
		}
		break;

	case EFontCacheType::Runtime:
		{
			TSharedPtr<FSlateFontCache> FontCache = FEngineFontServices::Get().GetFontCache();

			if (FontCache.IsValid())
			{
				const float FontScale = 1.0f;
				const FSlateFontInfo LegacyFontInfo = GetLegacySlateFontInfo();
				FCharacterList& CharacterList = FontCache->GetCharacterList(LegacyFontInfo, FontScale);
				const FCharacterEntry& Entry = CharacterList.GetCharacter(InCh, LegacyFontInfo.FontFallback);

				Width = Entry.XAdvance;

				// The height of the character will always be the maximum height of any character in this font
				Height = CharacterList.GetMaxHeight();
			}
		}
		break;

	default:
		break;
	}
}

int8 UFont::GetCharKerning(TCHAR First, TCHAR Second) const
{
	switch(FontCacheType)
	{
	case EFontCacheType::Offline:
		{
			return static_cast<int8>(Kerning);
		}
		break;

	case EFontCacheType::Runtime:
		{
			TSharedPtr<FSlateFontCache> FontCache = FEngineFontServices::Get().GetFontCache();

			if (FontCache.IsValid())
			{
				const float FontScale = 1.0f;
				const FSlateFontInfo LegacyFontInfo = GetLegacySlateFontInfo();
				FCharacterList& CharacterList = FontCache->GetCharacterList(LegacyFontInfo, FontScale);

				return CharacterList.GetKerning(First, Second, LegacyFontInfo.FontFallback);
			}
		}
		break;

	default:
		break;
	}

	return 0;
}

int16 UFont::GetCharHorizontalOffset(TCHAR InCh) const
{
	if(FontCacheType == EFontCacheType::Runtime)
	{
		TSharedPtr<FSlateFontCache> FontCache = FEngineFontServices::Get().GetFontCache();

		if (FontCache.IsValid())
		{
			const float FontScale = 1.0f;
			const FSlateFontInfo LegacyFontInfo = GetLegacySlateFontInfo();
			FCharacterList& CharacterList = FontCache->GetCharacterList(LegacyFontInfo, FontScale);
			const FCharacterEntry& Entry = CharacterList.GetCharacter(InCh, LegacyFontInfo.FontFallback);

			return Entry.HorizontalOffset;
		}
	}

	return 0;
}

int32 UFont::GetStringSize( const TCHAR *Text ) const
{
	int32 Width, Height;
	GetStringHeightAndWidth( Text, Height, Width );
	return Width;
}

int32 UFont::GetStringHeightSize( const TCHAR *Text ) const
{
	int32 Width, Height;
	GetStringHeightAndWidth( Text, Height, Width );
	return Height;
}

float UFont::GetMaxCharHeight() const
{
	float MaxHeight = 0.0f;

	switch(FontCacheType)
	{
	case EFontCacheType::Offline:
		{
			// @todo: Provide a version of this function that supports multi-fonts properly.  It should take a
			//    HeightTest parameter and report the appropriate multi-font MaxCharHeight value back.
			int32 MaxCharHeightForAllMultiFonts = 1;
			for( int32 CurMultiFontIndex = 0; CurMultiFontIndex < MaxCharHeight.Num(); ++CurMultiFontIndex )
			{
				MaxCharHeightForAllMultiFonts = FMath::Max( MaxCharHeightForAllMultiFonts, MaxCharHeight[ CurMultiFontIndex ] );
			}
			MaxHeight = MaxCharHeightForAllMultiFonts;
		}
		break;

	case EFontCacheType::Runtime:
		{
			TSharedPtr<FSlateFontCache> FontCache = FEngineFontServices::Get().GetFontCache();
			
			if (FontCache.IsValid())
			{
				const float FontScale = 1.0f;
				const FSlateFontInfo LegacyFontInfo(this, LegacyFontSize);
				FCharacterList& CharacterList = FontCache->GetCharacterList(LegacyFontInfo, FontScale);

				MaxHeight = CharacterList.GetMaxHeight();
			}
		}
		break;

	default:
		break;
	}

	return MaxHeight;
}

void UFont::GetStringHeightAndWidth( const FString& InString, int32& Height, int32& Width ) const
{
	GetStringHeightAndWidth( *InString, Height, Width );
}

void UFont::GetStringHeightAndWidth( const TCHAR *Text, int32& Height, int32& Width ) const
{
	float TotalWidth = 0.0f;
	float TotalHeight = 0.0f;
	const TCHAR* PrevChar = nullptr;
	while( *Text )
	{
		float TmpWidth, TmpHeight;
		GetCharSize( *Text, TmpWidth, TmpHeight );

		int8 CharKerning = 0;
		if (PrevChar)
		{
			CharKerning = GetCharKerning( *PrevChar, *Text );
		}

		TotalWidth += TmpWidth + CharKerning;
		TotalHeight = FMath::Max( TotalHeight, TmpHeight );

		PrevChar = Text++;
	}

	Width = FMath::CeilToInt( TotalWidth );
	Height = FMath::CeilToInt( TotalHeight );
}

SIZE_T UFont::GetResourceSize(EResourceSizeMode::Type Mode)
{
	int32 ResourceSize = 0;

	switch(FontCacheType)
	{
	case EFontCacheType::Offline:
		{
			for (UTexture2D* Texture : Textures)
			{
				if (Texture)
				{
					ResourceSize += Texture->GetResourceSize(Mode);
				}
			}
		}
		break;

	case EFontCacheType::Runtime:
		{
			auto GetTypefaceResourceSize = [](const FTypeface& Typeface) -> int32
			{
				int32 TypefaceResourceSize = 0;
				for (const FTypefaceEntry& TypefaceEntry : Typeface.Fonts)
				{
					if (TypefaceEntry.Font.BulkDataPtr)
					{
						// We use GetBulkDataSizeOnDisk since that will be the resident size once the bulk data has been decompressed
						TypefaceResourceSize += TypefaceEntry.Font.BulkDataPtr->GetBulkDataSizeOnDisk();
					}
				}
				return TypefaceResourceSize;
			};

			// Sum the contained font data sizes
			ResourceSize += GetTypefaceResourceSize(CompositeFont.DefaultTypeface);
			for (const FCompositeSubFont& SubTypeface : CompositeFont.SubTypefaces)
			{
				ResourceSize += GetTypefaceResourceSize(SubTypeface.Typeface);
			}
		}
		break;

	default:
		break;
	}

	return ResourceSize;
}

void UFont::ForceLoadFontData()
{
	if (FontCacheType == EFontCacheType::Runtime)
	{
		auto LoadFontDataForTypeface = [](const FTypeface& InTypeface)
		{
			for (const FTypefaceEntry& TypefaceEntry : InTypeface.Fonts)
			{
				// Somewhat evil, but ideally this will eventually be replaced by just having the font cache reference the bulk data directly
				// rather than have the need to unload it to save memory, and then cause potential issues with render-thread loads
				UFontBulkData* FontData = const_cast<UFontBulkData*>(TypefaceEntry.Font.BulkDataPtr);
				if (FontData)
				{
					FontData->ForceLoadBulkData();
				}
			}
		};

		LoadFontDataForTypeface(CompositeFont.DefaultTypeface);
		for (const FCompositeSubFont& SubTypeface : CompositeFont.SubTypefaces)
		{
			LoadFontDataForTypeface(SubTypeface.Typeface);
		}
	}
}
