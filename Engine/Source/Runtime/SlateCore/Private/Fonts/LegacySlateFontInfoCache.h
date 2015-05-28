// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Cache used to efficiently upgrade legacy FSlateFontInfo structs to use a composite font by reducing the amount of duplicate instances that are created
 */
class FLegacySlateFontInfoCache : public FGCObject
{
public:

	/**
	 * Get (or create) the singleton instance of this cache
	 */
	static FLegacySlateFontInfoCache& Get();

	/**
	 * Get (or create) an appropriate composite font from the legacy font name
	 */
	TSharedPtr<const FCompositeFont> GetCompositeFont(const FName& InLegacyFontName, const EFontHinting InLegacyFontHinting);

	/**
	 * Get (or create) the default system font
	 */
	TSharedPtr<const FCompositeFont> GetSystemFont();

	/**
	 * Get (or create) the culture specific fallback font
	 */
	TSharedPtr<const FCompositeFont> GetFallbackFont();

	/**
	 * Get (or create) the culture specific fallback font
	 */
	const FFontData& GetFallbackFontData();

	/**
	 * Get (or create) the last resort fallback font
	 */
	const FFontData& GetLastResortFontData();

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	struct FLegacyFontKey
	{
		FLegacyFontKey()
			: Name()
			, Hinting(EFontHinting::Default)
		{
		}

		FLegacyFontKey(const FName& InName, const EFontHinting InHinting)
			: Name(InName)
			, Hinting(InHinting)
		{
		}

		bool operator==(const FLegacyFontKey& Other) const 
		{
			return Name == Other.Name && Hinting == Other.Hinting;
		}

		bool operator!=(const FLegacyFontKey& Other) const 
		{
			return !(*this == Other);
		}

		friend inline uint32 GetTypeHash(const FLegacyFontKey& Key)
		{
			uint32 Hash = 0;
			Hash = HashCombine(Hash, GetTypeHash(Key.Name));
			Hash = HashCombine(Hash, uint32(Key.Hinting));
			return Hash;
		}

		FName Name;
		EFontHinting Hinting;
	};

	TMap<FLegacyFontKey, TSharedPtr<const FCompositeFont>> LegacyFontNameToCompositeFont;
	TSharedPtr<const FCompositeFont> SystemFont;
	TSharedPtr<const FCompositeFont> FallbackFont;
	
	TSharedPtr<const FFontData> FallbackFontData;
	TSharedPtr<const FFontData> LastResortFontData;

	int32 FallbackFontHistoryVersion;
	int32 FallbackFontDataHistoryVersion;

	FCriticalSection FallbackFontCS;
	FCriticalSection FallbackFontDataCS;
	FCriticalSection LastResortFontDataCS;

	static TSharedPtr<FLegacySlateFontInfoCache> Singleton;
};
