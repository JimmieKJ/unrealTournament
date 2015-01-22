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

	TSharedRef<const FCompositeFont> NewCompositeFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, LegacyFontPath, new UFontBulkData(LegacyFontPath), InLegacyFontHinting));
	LegacyFontNameToCompositeFont.Add(LegacyFontKey, NewCompositeFont);
	return NewCompositeFont;
}

TSharedPtr<const FCompositeFont> FLegacySlateFontInfoCache::GetSystemFont()
{
	if (!SystemFont.IsValid())
	{
		TArray<uint8> FontBytes = FPlatformMisc::GetSystemFontBytes();
		SystemFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, TEXT("DefaultSystemFont"), new UFontBulkData(FontBytes.GetData(), FontBytes.Num()), EFontHinting::Default));
	}
	return SystemFont;
}

const FFontData& FLegacySlateFontInfoCache::GetFallbackFont()
{
	const FName FallbackFontName = *(FPaths::EngineContentDir() / TEXT("Slate/Fonts/") / (NSLOCTEXT("Slate", "FallbackFont", "DroidSansFallback").ToString() + TEXT(".ttf")));

	// GetFallbackFont is called directly from the font cache, so may be called from multiple threads at once
	FScopeLock Lock(&FallbackFontCS);

	{
		TSharedPtr<const FFontData>* const ExistingFallbackFont = FallbackFonts.Find(FallbackFontName);
		if(ExistingFallbackFont)
		{
			return **ExistingFallbackFont;
		}
	}

	const FString FallbackFontPath = FallbackFontName.ToString();
	TSharedRef<const FFontData> NewFallbackFont = MakeShareable(new FFontData(FallbackFontPath, new UFontBulkData(FallbackFontPath), EFontHinting::Default));
	FallbackFonts.Add(FallbackFontName, NewFallbackFont);
	return *NewFallbackFont;
}

const FFontData& FLegacySlateFontInfoCache::GetLastResortFont()
{
	// GetLastResortFont is called directly from the font cache, so may be called from multiple threads at once
	FScopeLock Lock(&LastResortFontCS);

	if (!LastResortFont.IsValid())
	{
		const FString LastResortFontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/LastResort.ttf");
		LastResortFont = MakeShareable(new FFontData(LastResortFontPath, new UFontBulkData(LastResortFontPath), EFontHinting::Default));
	}

	return *LastResortFont;
}

void FLegacySlateFontInfoCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	for(auto& FallbackFontEntry : FallbackFonts)
	{
		const UFontBulkData* TmpPtr = FallbackFontEntry.Value->BulkDataPtr;
		Collector.AddReferencedObject(TmpPtr);
	}

	if(LastResortFont.IsValid())
	{
		const UFontBulkData* TmpPtr = LastResortFont->BulkDataPtr;
		Collector.AddReferencedObject(TmpPtr);
	}
}
