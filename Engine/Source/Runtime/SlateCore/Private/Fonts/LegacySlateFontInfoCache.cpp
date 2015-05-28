// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "LegacySlateFontInfoCache.h"

TSharedPtr<FLegacySlateFontInfoCache> FLegacySlateFontInfoCache::Singleton;

FLegacySlateFontInfoCache& FLegacySlateFontInfoCache::Get()
{
	if (!Singleton.IsValid())
	{
		Singleton = MakeShareable(new FLegacySlateFontInfoCache());
	}
	return *Singleton;
}

TSharedPtr<const FCompositeFont> FLegacySlateFontInfoCache::GetCompositeFont(const FName& InLegacyFontName, const EFontHinting InLegacyFontHinting)
{
	if (InLegacyFontName.IsNone())
	{
		return nullptr;
	}

	FString LegacyFontPath = InLegacyFontName.ToString();

	// Work out what the given path is supposed to be relative to
	if (!FPaths::FileExists(LegacyFontPath))
	{
		// UMG assets specify the path either relative to the game or engine content directories - test both
		LegacyFontPath = FPaths::GameContentDir() / InLegacyFontName.ToString();
		if (!FPaths::FileExists(LegacyFontPath))
		{
			LegacyFontPath = FPaths::EngineContentDir() / InLegacyFontName.ToString();
			if (!FPaths::FileExists(LegacyFontPath))
			{
				// Missing font file - just use what we were given
				LegacyFontPath = InLegacyFontName.ToString();
			}
		}
	}

	const FLegacyFontKey LegacyFontKey(*LegacyFontPath, InLegacyFontHinting);

	{
		TSharedPtr<const FCompositeFont>* const ExistingCompositeFont = LegacyFontNameToCompositeFont.Find(LegacyFontKey);
		if(ExistingCompositeFont)
		{
			return *ExistingCompositeFont;
		}
	}

	UFontBulkData* FontBulkData = NewObject<UFontBulkData>();
	FontBulkData->Initialize(LegacyFontPath);
	TSharedRef<const FCompositeFont> NewCompositeFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, LegacyFontPath, FontBulkData, InLegacyFontHinting));
	LegacyFontNameToCompositeFont.Add(LegacyFontKey, NewCompositeFont);
	return NewCompositeFont;
}

TSharedPtr<const FCompositeFont> FLegacySlateFontInfoCache::GetSystemFont()
{
	if (!SystemFont.IsValid())
	{
		TArray<uint8> FontBytes = FPlatformMisc::GetSystemFontBytes();
		UFontBulkData* FontBulkData = NewObject<UFontBulkData>();
		FontBulkData->Initialize(FontBytes.GetData(), FontBytes.Num());
		SystemFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, TEXT("DefaultSystemFont"), FontBulkData, EFontHinting::Default));
	}
	return SystemFont;
}

TSharedPtr<const FCompositeFont> FLegacySlateFontInfoCache::GetFallbackFont()
{
	// GetFallbackFont may be called from multiple threads at once
	FScopeLock Lock(&FallbackFontCS);

	// The fallback font can change if the active culture is changed
	const int32 CurrentHistoryVersion = FTextLocalizationManager::Get().GetTextRevision();

	if (!FallbackFont.IsValid() || FallbackFontHistoryVersion != CurrentHistoryVersion)
	{
		FallbackFontHistoryVersion = CurrentHistoryVersion;

		const FString FallbackFontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/") / (NSLOCTEXT("Slate", "FallbackFont", "DroidSansFallback").ToString() + TEXT(".ttf"));
		UFontBulkData* FontBulkData = NewObject<UFontBulkData>();
		FontBulkData->Initialize(FallbackFontPath);
		FallbackFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, FallbackFontPath, FontBulkData, EFontHinting::Default));
	}

	return FallbackFont;
}

const FFontData& FLegacySlateFontInfoCache::GetFallbackFontData()
{
	// GetFallbackFontData is called directly from the font cache, so may be called from multiple threads at once
	FScopeLock Lock(&FallbackFontDataCS);

	// The fallback font can change if the active culture is changed
	const int32 CurrentHistoryVersion = FTextLocalizationManager::Get().GetTextRevision();

	if (!FallbackFontData.IsValid() || FallbackFontDataHistoryVersion != CurrentHistoryVersion)
	{
		FallbackFontDataHistoryVersion = CurrentHistoryVersion;

		const FString FallbackFontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/") / (NSLOCTEXT("Slate", "FallbackFont", "DroidSansFallback").ToString() + TEXT(".ttf"));
		UFontBulkData* FontBulkData = NewObject<UFontBulkData>();
		FontBulkData->Initialize(FallbackFontPath);
		FallbackFontData = MakeShareable(new FFontData(FallbackFontPath, FontBulkData, EFontHinting::Default));
	}

	return *FallbackFontData;
}

const FFontData& FLegacySlateFontInfoCache::GetLastResortFontData()
{
	// GetLastResortFontData is called directly from the font cache, so may be called from multiple threads at once
	FScopeLock Lock(&LastResortFontDataCS);

	if (!LastResortFontData.IsValid())
	{
		const FString LastResortFontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/LastResort.ttf");
		UFontBulkData* FontBulkData = NewObject<UFontBulkData>();
		FontBulkData->Initialize(LastResortFontPath);
		LastResortFontData = MakeShareable(new FFontData(LastResortFontPath, FontBulkData, EFontHinting::Default));
	}

	return *LastResortFontData;
}

void FLegacySlateFontInfoCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	if(FallbackFontData.IsValid())
	{
		const UFontBulkData* TmpPtr = FallbackFontData->BulkDataPtr;
		Collector.AddReferencedObject(TmpPtr);
	}

	if(LastResortFontData.IsValid())
	{
		const UFontBulkData* TmpPtr = LastResortFontData->BulkDataPtr;
		Collector.AddReferencedObject(TmpPtr);
	}
}
