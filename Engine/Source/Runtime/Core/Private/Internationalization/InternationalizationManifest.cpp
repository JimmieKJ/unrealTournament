// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/InternationalizationMetadata.h"

DEFINE_LOG_CATEGORY_STATIC(LogInternationalizationManifestObject, Log, All);

FContext::FContext( const FContext& Other ) 
	: Key( Other.Key )
	, SourceLocation( Other.SourceLocation )
	, bIsOptional( Other.bIsOptional )
{
	if( Other.InfoMetadataObj.IsValid() )
	{
		InfoMetadataObj = MakeShareable( new FLocMetadataObject( *(Other.InfoMetadataObj) ) );
	}

	if( Other.KeyMetadataObj.IsValid() )
	{
		KeyMetadataObj = MakeShareable( new FLocMetadataObject( *(Other.KeyMetadataObj) ) );
	}
}

FContext& FContext::operator=( const FContext& Other )
{
	if( this != &Other )
	{
		Key = Other.Key;
		SourceLocation = Other.SourceLocation;
		bIsOptional = Other.bIsOptional;
		InfoMetadataObj.Reset();
		KeyMetadataObj.Reset();
		if( Other.InfoMetadataObj.IsValid() )
		{
			InfoMetadataObj = MakeShareable( new FLocMetadataObject( *(Other.InfoMetadataObj) ) );
		}

		if( Other.KeyMetadataObj.IsValid() )
		{
			KeyMetadataObj = MakeShareable( new FLocMetadataObject( *(Other.KeyMetadataObj) ) );
		}
	}
	return *this;
}

bool FContext::operator==( const FContext& Other ) const
{
	if( Key.Equals( Other.Key, ESearchCase::CaseSensitive ) )
	{
		if( !KeyMetadataObj.IsValid() && !Other.KeyMetadataObj.IsValid() )
		{
			return true;
		}
		else if( KeyMetadataObj.IsValid() != Other.KeyMetadataObj.IsValid() )
		{
			// If we are in here, we know that one of the metadata entries is null, if the other
			//  contains zero entries we will still consider them equivalent.
			if( (KeyMetadataObj.IsValid() && KeyMetadataObj->Values.Num() == 0) ||
				(Other.KeyMetadataObj.IsValid() && Other.KeyMetadataObj->Values.Num() == 0) )
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		return *(KeyMetadataObj) == *(Other.KeyMetadataObj);
	}
	return false;
}

bool FContext::operator<( const FContext& Other ) const
{
	int32 Result = Key.Compare( Other.Key, ESearchCase::CaseSensitive );

	bool bResult = false;
	if( Result == -1 ) //Is Less than
	{
		bResult = true;
	}
	else if ( Result == 0 ) //Is Equal
	{
		if( KeyMetadataObj.IsValid() != Other.KeyMetadataObj.IsValid() )
		{
			// If we are in here, we know that one of the metadata entries is null, if the other
			//  contains zero entries we will still consider them equivalent.
			if( (KeyMetadataObj.IsValid() && KeyMetadataObj->Values.Num() == 0) ||
				(Other.KeyMetadataObj.IsValid() && Other.KeyMetadataObj->Values.Num() == 0) )
			{
				return false;
			}
			else
			{
				bResult = Other.KeyMetadataObj.IsValid();
			}
		}
		else if( KeyMetadataObj.IsValid() && Other.KeyMetadataObj.IsValid() )
		{
			bResult = (*(KeyMetadataObj) < *(Other.KeyMetadataObj));
		}
	}
	return bResult;
}

FLocItem::FLocItem( const FLocItem& Other ) 
	: Text( Other.Text )
{
	if( Other.MetadataObj.IsValid() )
	{
		MetadataObj = MakeShareable( new FLocMetadataObject( *(Other.MetadataObj) ) );
	}
}

FLocItem& FLocItem::operator=( const FLocItem& Other )
{
	if( this != &Other )
	{
		Text = Other.Text;
		MetadataObj.Reset();

		if( Other.MetadataObj.IsValid() )
		{
			MetadataObj = MakeShareable( new FLocMetadataObject( *(Other.MetadataObj) ) );
		}
	}
	return *this;
}

bool FLocItem::operator==( const FLocItem& Other ) const
{
	if( Text.Equals( Other.Text, ESearchCase::CaseSensitive ) )
	{
		if( !MetadataObj.IsValid() && !Other.MetadataObj.IsValid() )
		{
			return true;
		}
		else if( MetadataObj.IsValid() != Other.MetadataObj.IsValid() )
		{
			// If we are in here, we know that one of the metadata entries is null, if the other
			//  contains zero entries we will still consider them equivalent.
			if( (MetadataObj.IsValid() && MetadataObj->Values.Num() == 0) ||
				(Other.MetadataObj.IsValid() && Other.MetadataObj->Values.Num() == 0) )
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		return *(MetadataObj) == *(Other.MetadataObj);
	}
	return false;
}

bool FLocItem::operator<( const FLocItem& Other ) const
{
	int32 Result = Text.Compare( Other.Text, ESearchCase::CaseSensitive );

	bool bResult = false;
	if( Result == -1 ) //Is less than
	{
		bResult = true;
	}
	else if ( Result == 0 ) //Is equal
	{
		if( MetadataObj.IsValid() != Other.MetadataObj.IsValid() )
		{
			// If we are in here, we know that one of the metadata entries is null, if the other
			//  contains zero entries we will still consider them equivalent.
			if( (MetadataObj.IsValid() && MetadataObj->Values.Num() == 0) ||
				(Other.MetadataObj.IsValid() && Other.MetadataObj->Values.Num() == 0) )
			{
				return false;
			}
			else
			{
				bResult = Other.MetadataObj.IsValid();
			}
		}
		else if( MetadataObj.IsValid() && Other.MetadataObj.IsValid() )
		{
			bResult = (*(MetadataObj) < *(Other.MetadataObj));
		}
	}
	return bResult;
}

bool FLocItem::IsExactMatch( const FLocItem& Other ) const
{
	if( Text.Equals( Other.Text, ESearchCase::CaseSensitive ) )
	{
		return FLocMetadataObject::IsMetadataExactMatch(MetadataObj.Get(), Other.MetadataObj.Get() );
	}
	return false;
}

bool FInternationalizationManifest::AddSource( const FString& Namespace, const FLocItem& Source, const FContext& Context )
{
	TSharedPtr< FManifestEntry > ExistingEntry = FindEntryByContext( Namespace, Context );

	if ( ExistingEntry.IsValid() )
	{
		return ( Source.IsExactMatch( ExistingEntry->Source ) );
	}
			
	ExistingEntry = FindEntryBySource( Namespace, Source );

	if ( ExistingEntry.IsValid() && !Source.IsExactMatch( ExistingEntry->Source ) )
	{
		return false;
	}

	if ( !ExistingEntry.IsValid() )
	{
		TSharedRef< FManifestEntry > NewEntry = MakeShareable( new FManifestEntry( Namespace, Source ) );
		EntriesBySourceText.Add( NewEntry->Source.Text, NewEntry );

		ExistingEntry = NewEntry;
	}

	ExistingEntry->Contexts.Add( Context );
	EntriesByContextId.Add( Context.Key, ExistingEntry.ToSharedRef() );
	return true;	
}

TSharedPtr< FManifestEntry > FInternationalizationManifest::FindEntryBySource( const FString& Namespace, const FLocItem& Source )
{	
	TArray< TSharedRef< FManifestEntry > > MatchingEntries;
	EntriesBySourceText.MultiFind( Source.Text, /*OUT*/MatchingEntries );

	for (int Index = 0; Index < MatchingEntries.Num(); Index++)
	{
		if ( MatchingEntries[Index]->Source == Source && 
			MatchingEntries[Index]->Namespace.Equals( Namespace, ESearchCase::CaseSensitive ) )
		{
			return MatchingEntries[Index];
		}
	}
	return NULL;
}

TSharedPtr< FManifestEntry > FInternationalizationManifest::FindEntryByContext( const FString& Namespace, const FContext& Context )
{
	TArray< TSharedRef< FManifestEntry > > MatchingEntries;
	EntriesByContextId.MultiFind( Context.Key, MatchingEntries );

	for (int Index = 0; Index < MatchingEntries.Num(); Index++)
	{
		if ( MatchingEntries[Index]->Namespace.Equals( Namespace, ESearchCase::CaseSensitive ) )
		{
			for( auto ContextIter = MatchingEntries[Index]->Contexts.CreateConstIterator(); ContextIter; ++ContextIter )
			{
				if( *ContextIter == Context )
				{
					return MatchingEntries[Index];
				}
			}
		}
	}
	return NULL;
}

void FInternationalizationManifest::UpdateEntry(const TSharedRef<FManifestEntry>& OldEntry, TSharedRef<FManifestEntry>& NewEntry)
{
	for (const FContext& Context : OldEntry->Contexts)
	{
		EntriesByContextId.RemoveSingle(Context.Key, OldEntry);
	}
	for (const FContext& Context : NewEntry->Contexts)
	{
		EntriesByContextId.Add(Context.Key, NewEntry);
	}

	EntriesBySourceText.RemoveSingle(OldEntry->Source.Text, OldEntry);
	EntriesBySourceText.Add(NewEntry->Source.Text, NewEntry);
}

FContext* FManifestEntry::FindContext( const FString& ContextKey, const TSharedPtr<FLocMetadataObject>& KeyMetadata /*= NULL */ )
{
	for( auto ContextIter = Contexts.CreateIterator(); ContextIter; ++ContextIter )
	{
		FContext& Context = *ContextIter;

		if( Context.Key.Equals( ContextKey, ESearchCase::CaseSensitive ) )
		{
			if( Context.KeyMetadataObj.IsValid() != KeyMetadata.IsValid() )
			{
				continue;
			}
			else if( (!Context.KeyMetadataObj.IsValid() && !KeyMetadata.IsValid()) ||
				(*(Context.KeyMetadataObj) == *(KeyMetadata)) )
			{
				return &Context;
			}
		}
	}
	return NULL;
}