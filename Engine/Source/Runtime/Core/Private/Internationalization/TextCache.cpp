// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "TextCache.h"

FTextCache& FTextCache::Get()
{
	static FTextCache Instance;
	return Instance;
}

FText FTextCache::FindOrCache(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey)
{
	FCacheKey CacheKey = FCacheKey::MakeReference(InTextLiteral, InNamespace, InKey);

	// First try and find a cached instance
	{
		FScopeLock Lock(&CachedTextCS);

		const FText* FoundText = CachedText.Find(CacheKey);
		if (FoundText)
		{
			return *FoundText;
		}
	}

	// Not current cached, make a new instance...
	FText NewText = FText(InTextLiteral, InNamespace, InKey, ETextFlag::Immutable);

	// ... and add it to the cache
	{
		CacheKey.Persist(); // Persist the key as we'll be adding it to the map

		FScopeLock Lock(&CachedTextCS);

		CachedText.Add(CacheKey, NewText);
	}

	return NewText;
}

void FTextCache::Flush()
{
	FScopeLock Lock(&CachedTextCS);

	CachedText.Empty();
}
