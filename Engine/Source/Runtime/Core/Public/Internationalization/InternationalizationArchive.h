// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/InternationalizationManifest.h"

class CORE_API FArchiveEntry
{
public:
	FArchiveEntry( const FString& InNamespace, const FLocItem& InSource, const FLocItem& InTranslation, TSharedPtr<FLocMetadataObject> InKeyMetadataObj = NULL, bool IsOptional = false );

	const FString Namespace;
	FLocItem Source;
	FLocItem Translation;
	bool bIsOptional;
	TSharedPtr<FLocMetadataObject> KeyMetadataObj;
};

struct FInternationalizationArchiveSourceTextKeyFuncs : BaseKeyFuncs<TSharedRef< FArchiveEntry >, FString, true>
{
	static FORCEINLINE const FString& GetSetKey( const TPair<FString,TSharedRef< FArchiveEntry >>& Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(const FString& A, const FString& B)
	{
		return A.Equals( B, ESearchCase::CaseSensitive );
	}
	static FORCEINLINE uint32 GetKeyHash(const FString& Key)
	{
		return FCrc::StrCrc32<TCHAR>(*Key);
	}
};

typedef TMultiMap< FString, TSharedRef< FArchiveEntry >, FDefaultSetAllocator, FInternationalizationArchiveSourceTextKeyFuncs > TArchiveEntryContainer;

class CORE_API FInternationalizationArchive 
{
	friend class IInternationalizationArchiveSerializer;

public:
	enum EFormatVersion
	{
		Initial = 0,
		EscapeFixes,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};

	FInternationalizationArchive()
		: FormatVersion(EFormatVersion::Latest)
	{ }

	TArchiveEntryContainer::TConstIterator GetEntryIterator() const;

	bool AddEntry( const FString& Namespace, const FLocItem& Source, const FLocItem& Translation, const TSharedPtr<FLocMetadataObject> KeyMetadataObj, const bool bOptional );
	bool AddEntry( const TSharedRef<FArchiveEntry>& InEntry );

	bool SetTranslation( const FString& Namespace, const FLocItem& Source, const FLocItem& Translation, const TSharedPtr<FLocMetadataObject> KeyMetadataObj );

	TSharedPtr< FArchiveEntry > FindEntryBySource( const FString& Namespace, const FLocItem& Source, const TSharedPtr<FLocMetadataObject> KeyMetadataObj ) const;

	int32 GetNumEntriesBySourceText() const
	{
		return EntriesBySourceText.Num();
	}

	void UpdateEntry(const TSharedRef<FArchiveEntry>& OldEntry, const TSharedRef<FArchiveEntry>& NewEntry);

	void SetFormatVersion(const EFormatVersion Version)
	{
		FormatVersion = Version;
	}

	EFormatVersion GetFormatVersion() const
	{
		return EFormatVersion(FormatVersion);
	}

private:

	EFormatVersion FormatVersion;
	TArchiveEntryContainer EntriesBySourceText;
};
