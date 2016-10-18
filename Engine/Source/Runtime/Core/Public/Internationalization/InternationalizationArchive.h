// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/InternationalizationManifest.h"

class CORE_API FArchiveEntry
{
public:
	FArchiveEntry(const FString& InNamespace, const FString& InKey, const FLocItem& InSource, const FLocItem& InTranslation, TSharedPtr<FLocMetadataObject> InKeyMetadataObj = nullptr, bool IsOptional = false);

	const FString Namespace;
	const FString Key;
	const FLocItem Source;
	FLocItem Translation;
	bool bIsOptional;
	TSharedPtr<FLocMetadataObject> KeyMetadataObj;
};

struct FInternationalizationArchiveStringKeyFuncs : BaseKeyFuncs<TSharedRef< FArchiveEntry >, FString, true>
{
	static FORCEINLINE const FString& GetSetKey(const TPair<FString, TSharedRef< FArchiveEntry >>& Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(const FString& A, const FString& B)
	{
		return A.Equals(B, ESearchCase::CaseSensitive);
	}
	static FORCEINLINE uint32 GetKeyHash(const FString& Key)
	{
		return FCrc::StrCrc32<TCHAR>(*Key);
	}
};

typedef TMultiMap< FString, TSharedRef< FArchiveEntry >, FDefaultSetAllocator, FInternationalizationArchiveStringKeyFuncs > FArchiveEntryByStringContainer;

class CORE_API FInternationalizationArchive 
{
public:
	enum class EFormatVersion : uint8
	{
		Initial = 0,
		EscapeFixes,
		AddedKeys,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};

	FInternationalizationArchive()
		: FormatVersion(EFormatVersion::Latest)
	{
	}

	bool AddEntry(const FString& Namespace, const FString& Key, const FLocItem& Source, const FLocItem& Translation, const TSharedPtr<FLocMetadataObject> KeyMetadataObj, const bool bOptional);
	bool AddEntry(const TSharedRef<FArchiveEntry>& InEntry);

	void UpdateEntry(const TSharedRef<FArchiveEntry>& OldEntry, const TSharedRef<FArchiveEntry>& NewEntry);

	bool SetTranslation(const FString& Namespace, const FString& Key, const FLocItem& Source, const FLocItem& Translation, const TSharedPtr<FLocMetadataObject> KeyMetadataObj);

	TSharedPtr<FArchiveEntry> FindEntryByKey(const FString& Namespace, const FString& Key, const TSharedPtr<FLocMetadataObject> KeyMetadataObj) const;

	FArchiveEntryByStringContainer::TConstIterator GetEntriesByKeyIterator() const
	{
		return EntriesByKey.CreateConstIterator();
	}

	int32 GetNumEntriesByKey() const
	{
		return EntriesByKey.Num();
	}

	FArchiveEntryByStringContainer::TConstIterator GetEntriesBySourceTextIterator() const
	{
		return EntriesBySourceText.CreateConstIterator();
	}

	int32 GetNumEntriesBySourceText() const
	{
		return EntriesBySourceText.Num();
	}

	void SetFormatVersion(const EFormatVersion Version)
	{
		FormatVersion = Version;
	}

	EFormatVersion GetFormatVersion() const
	{
		return FormatVersion;
	}

private:
	EFormatVersion FormatVersion;
	FArchiveEntryByStringContainer EntriesBySourceText;
	FArchiveEntryByStringContainer EntriesByKey;
};
