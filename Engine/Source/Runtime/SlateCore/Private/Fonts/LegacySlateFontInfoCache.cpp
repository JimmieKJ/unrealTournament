// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "LegacySlateFontInfoCache.h"

TSharedPtr<FLegacySlateFontInfoCache> FLegacySlateFontInfoCache::Singleton;

FLegacySlateFontInfoCache& FLegacySlateFontInfoCache::Get()
{
	if (!Singleton.IsValid())
	{
		Singleton = MakeShareable(new FLegacySlateFontInfoCache());

		FInternationalization::Get().OnCultureChanged().AddSP(Singleton.Get(), &FLegacySlateFontInfoCache::HandleCultureChanged);
	}
	return *Singleton;
}

FLegacySlateFontInfoCache::FLegacySlateFontInfoCache()
	: LocalizedFallbackFontRevision(0)
	, LocalizedFallbackFontDataHistoryVersion(0)
	, LocalizedFallbackFontFrameCounter(0)
{
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

	// Don't allow GC while we perform this allocation
	FGCScopeGuard GCGuard;

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
		// Don't allow GC while we perform this allocation
		FGCScopeGuard GCGuard;

		TArray<uint8> FontBytes = FPlatformMisc::GetSystemFontBytes();
		UFontBulkData* FontBulkData = NewObject<UFontBulkData>();
		FontBulkData->Initialize(FontBytes.GetData(), FontBytes.Num());
		SystemFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, TEXT("DefaultSystemFont"), FontBulkData, EFontHinting::Default));
	}

	return SystemFont;
}

const FFontData& FLegacySlateFontInfoCache::GetLocalizedFallbackFontData()
{
	// GetLocalizedFallbackFontData is called directly from the font cache, so may be called from multiple threads at once
	FScopeLock Lock(&LocalizedFallbackFontDataCS);

	// The fallback font can change if the active culture is changed
	const uint16 CurrentHistoryVersion = FTextLocalizationManager::Get().GetTextRevision();
	const uint64 CurrentFrameCounter = GFrameCounter;

	// Only allow the fallback font to be updated once per-frame, as a culture change mid-frame could cause it to change unexpectedly and invalidate some assumptions that the font cache makes
	// By only allowing it to update once per-frame, we ensure that the font cache has been flushed (which happens at the end of the frame) before we return a new font
	if (!LocalizedFallbackFontData.IsValid() || (LocalizedFallbackFontDataHistoryVersion != CurrentHistoryVersion && LocalizedFallbackFontFrameCounter != CurrentFrameCounter))
	{
		// Don't allow GC while we perform this allocation
		FGCScopeGuard GCGuard;

		LocalizedFallbackFontDataHistoryVersion = CurrentHistoryVersion;
		LocalizedFallbackFontFrameCounter = CurrentFrameCounter;

		TSharedPtr<const FFontData> PreviousLocalizedFallbackFontData = LocalizedFallbackFontData;

		const FString FallbackFontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/") / (NSLOCTEXT("Slate", "FallbackFont", "DroidSansFallback").ToString() + TEXT(".ttf"));
		LocalizedFallbackFontData = AllLocalizedFallbackFontData.FindRef(FallbackFontPath);

		if (!LocalizedFallbackFontData.IsValid())
		{
			UFontBulkData* FontBulkData = NewObject<UFontBulkData>();
			FontBulkData->Initialize(FallbackFontPath);
			LocalizedFallbackFontData = MakeShareable(new FFontData(FallbackFontPath, FontBulkData, EFontHinting::Default));

			AllLocalizedFallbackFontData.Add(FallbackFontPath, LocalizedFallbackFontData);
		}

		if (LocalizedFallbackFontData != PreviousLocalizedFallbackFontData)
		{
			// Only bump the revision if the font has actually changed
			while (++LocalizedFallbackFontRevision == 0) {} // Zero is special, don't allow an overflow to stay at zero
		}
	}

	return *LocalizedFallbackFontData;
}

uint16 FLegacySlateFontInfoCache::GetLocalizedFallbackFontRevision() const
{
	return LocalizedFallbackFontRevision;
}

TSharedPtr<const FCompositeFont> FLegacySlateFontInfoCache::GetLastResortFont()
{
	// GetLastResortFont may be called from multiple threads at once
	FScopeLock Lock(&LastResortFontCS);

	if (!LastResortFont.IsValid())
	{
		const FFontData& FontData = GetLastResortFontData();
		LastResortFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, FontData.FontFilename, FontData.BulkDataPtr, EFontHinting::Default));
	}

	return LastResortFont;
}

const FFontData& FLegacySlateFontInfoCache::GetLastResortFontData()
{
	// GetLastResortFontData is called directly from the font cache, so may be called from multiple threads at once
	FScopeLock Lock(&LastResortFontDataCS);

	if (!LastResortFontData.IsValid())
	{
		// Don't allow GC while we perform this allocation
		FGCScopeGuard GCGuard;

		const FString LastResortFontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/LastResort.ttf");
		UFontBulkData* FontBulkData = NewObject<UFontBulkData>();
		FontBulkData->Initialize(LastResortFontPath);
		LastResortFontData = MakeShareable(new FFontData(LastResortFontPath, FontBulkData, EFontHinting::Default));
	}

	return *LastResortFontData;
}

void FLegacySlateFontInfoCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (const auto& Pair : AllLocalizedFallbackFontData)
	{
		const UFontBulkData* TmpPtr = Pair.Value->BulkDataPtr;
		Collector.AddReferencedObject(TmpPtr);
	}

	if (LastResortFontData.IsValid())
	{
		const UFontBulkData* TmpPtr = LastResortFontData->BulkDataPtr;
		Collector.AddReferencedObject(TmpPtr);
	}
}

void FLegacySlateFontInfoCache::HandleCultureChanged()
{
	// We set this to the current frame count, as this will prevent the fallback font being updated for the remainder of this frame (as the culture change may have affected the fallback font used)
	LocalizedFallbackFontFrameCounter = GFrameCounter;
}
