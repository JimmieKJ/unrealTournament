// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Internationalization/InternationalizationArchive.h"
#include "Internationalization/InternationalizationMetadata.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherArchiveObject, Log, All);

FArchiveEntry::FArchiveEntry( const FString& InNamespace, const FLocItem& InSource, const FLocItem& InTranslation, TSharedPtr<FLocMetadataObject> InKeyMetadataObj /*= NULL*/, bool IsOptional /*= false */ ) : Namespace( InNamespace )
	, Source( InSource )
	, Translation( InTranslation )
	, bIsOptional( IsOptional )
{
	if( InKeyMetadataObj.IsValid() )
	{
		KeyMetadataObj = MakeShareable( new FLocMetadataObject( *(InKeyMetadataObj) ) );
	}
}

bool FInternationalizationArchive::AddEntry( const FString& Namespace, const FLocItem& Source, const FLocItem& Translation, const TSharedPtr<FLocMetadataObject> KeyMetadataObj, const bool bOptional )
{

	TSharedPtr<FArchiveEntry> ExistingEntry = FindEntryBySource( Namespace, Source, KeyMetadataObj );

	if( ExistingEntry.IsValid() )
	{
		return ( ExistingEntry->Translation == Translation );
	}

	TSharedRef< FArchiveEntry > NewEntry = MakeShareable( new FArchiveEntry( Namespace, Source, Translation, KeyMetadataObj, bOptional ) );
	EntriesBySourceText.Add( Source.Text, NewEntry );
	return true;
}

bool FInternationalizationArchive::AddEntry( const TSharedRef<FArchiveEntry>& InEntry )
{
	return AddEntry( InEntry->Namespace, InEntry->Source, InEntry->Translation, InEntry->KeyMetadataObj, InEntry->bIsOptional );
}

bool FInternationalizationArchive::SetTranslation( const FString& Namespace, const FLocItem& Source, const FLocItem& Translation, const TSharedPtr<FLocMetadataObject> KeyMetadataObj )
{
	TSharedPtr<FArchiveEntry> Entry = FindEntryBySource( Namespace, Source, KeyMetadataObj );
	if( Entry.IsValid() )
	{
		Entry->Translation = Translation;
		return true;
	}
	return false;
}

TSharedPtr< FArchiveEntry > FInternationalizationArchive::FindEntryBySource( const FString& Namespace, const FLocItem& Source, const TSharedPtr<FLocMetadataObject> KeyMetadataObj ) const
{
	TArray< TSharedRef< FArchiveEntry > > MatchingEntries;
	EntriesBySourceText.MultiFind( Source.Text, MatchingEntries );

	for (int Index = 0; Index < MatchingEntries.Num(); Index++)
	{
		if ( MatchingEntries[Index]->Namespace.Equals( Namespace, ESearchCase::CaseSensitive ) )
		{
			if( Source == MatchingEntries[Index]->Source )
			{
				if( !MatchingEntries[Index]->KeyMetadataObj.IsValid() && !KeyMetadataObj.IsValid() )
				{
					return MatchingEntries[Index];
				}
				else if( (KeyMetadataObj.IsValid() != MatchingEntries[Index]->KeyMetadataObj.IsValid()) )
				{
					// If we are in here, we know that one of the metadata entries is null, if the other
					//  contains zero entries we will still consider them equivalent.
					if( (KeyMetadataObj.IsValid() && KeyMetadataObj->Values.Num() == 0) ||
						(MatchingEntries[Index]->KeyMetadataObj.IsValid() && MatchingEntries[Index]->KeyMetadataObj->Values.Num() == 0) )
					{
						return MatchingEntries[Index];
					}
				}
				else if( *(MatchingEntries[Index]->KeyMetadataObj) == *(KeyMetadataObj) )
				{
					return MatchingEntries[Index];
				}
			}
		}
	}

	return NULL;
}

void FInternationalizationArchive::UpdateEntry(const TSharedRef<FArchiveEntry>& OldEntry, const TSharedRef<FArchiveEntry>& NewEntry)
{
	EntriesBySourceText.RemoveSingle(OldEntry->Source.Text, OldEntry);
	EntriesBySourceText.Add(NewEntry->Source.Text, NewEntry);
}

TArchiveEntryContainer::TConstIterator FInternationalizationArchive::GetEntryIterator() const
{
	return EntriesBySourceText.CreateConstIterator();
}



