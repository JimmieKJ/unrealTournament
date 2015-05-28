// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IFilter.h"

enum class ETextFilterComparisonOperation : uint8
{
	Equal,
	NotEqual,
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual,
};

/**
 *	A generic filter specialized for text restrictions
 */
template< typename ItemType >
class TTextFilter : public IFilter< ItemType >, public TSharedFromThis< TTextFilter< ItemType > >
{
public:

	/**	Defines a function signature for functions used to transform an Item into an array of FStrings */
	DECLARE_DELEGATE_TwoParams( FItemToStringArray, ItemType, OUT TArray< FString >& );

	/** 
	 *	TTextFilter Constructor
	 *	
	 * @param	InTransform		A required delegate used to populate an array of strings based on an Item
	 */
	TTextFilter( FItemToStringArray InTransform ) 
		: TransformArray( InTransform )
	{
		check( TransformArray.IsBound() );
	}

	DECLARE_DERIVED_EVENT(TTextFilter, IFilter<ItemType>::FChangedEvent, FChangedEvent);
	virtual FChangedEvent& OnChanged() override { return ChangedEvent; }

	/** 
	 * Returns whether the specified Item passes the Filter's text restrictions 
	 *
	 *	@param	InItem	The Item to check 
	 *	@return			Whether the specified Item passed the filter
	 */
	virtual bool PassesFilter( ItemType InItem ) const override
	{
		if( FilterSearchTerms.Num() == 0 )
		{
			return true;
		}
		
		ItemTextCollection.Empty();
		TransformArray.Execute( InItem, OUT ItemTextCollection );

		bool bPassesAnyTermSets = false;
		for( auto TermSetIndex = 0; !bPassesAnyTermSets && TermSetIndex < FilterSearchTerms.Num(); ++TermSetIndex )
		{
			bool bPassesFilter = true;
			for( auto CurSearchTermIndex = 0; bPassesFilter && CurSearchTermIndex < FilterSearchTerms[TermSetIndex].Num(); ++CurSearchTermIndex )
			{
				const FString* CurSearchTerm = &FilterSearchTerms[ TermSetIndex ][ CurSearchTermIndex ];
				FString TempSearchString;

				if( CurSearchTerm->IsEmpty() ) 
				{
					continue;
				}

				// If the search text starts with a plus, then we'll force an exact match search (whole word)
				const auto bExactMatch = CurSearchTerm->GetCharArray()[ 0 ] == TCHAR( '+' );
				if( bExactMatch )
				{
					// Strip off the leading plus
					TempSearchString = CurSearchTerm->Right( CurSearchTerm->Len() - 1 );
					CurSearchTerm = &TempSearchString;	// Storing pointer to stack variable to avoid copies (safe here)
				}

				if( CurSearchTerm->IsEmpty() )
				{
					continue;
				}

				// If the search text starts with a hyphen, then we'll explicitly exclude results using this search term
				const auto bExclude = CurSearchTerm->GetCharArray()[ 0 ] == TCHAR( '-' );
				if( bExclude )
				{
					//Strip off the leading hyphen
					TempSearchString = CurSearchTerm->Right( CurSearchTerm->Len() - 1 );
					CurSearchTerm = &TempSearchString;	// Storing pointer to stack variable to avoid copies (safe here)
				}

				if( CurSearchTerm->IsEmpty() )
				{
					continue;
				}

				bPassesFilter = false;
				for( int TextIndex = 0; !bPassesFilter && TextIndex < ItemTextCollection.Num(); ++TextIndex )
				{
					// Matches?
					const FString& ItemText = ItemTextCollection[ TextIndex ];
					bPassesFilter = bExactMatch ? ( ItemText == *CurSearchTerm ) : ( ItemText.Contains( **CurSearchTerm) );
				}

				// If excluding, then we'll reverse the meaning of our test
				if( bExclude )
				{
					bPassesFilter = !bPassesFilter;
				}
			}

			if ( bPassesFilter )
			{
				bPassesAnyTermSets = true;
			}
		}

		return bPassesAnyTermSets;
	}

	/** Returns the unsanitized and unsplit filter terms */
	FText GetRawFilterText() const
	{
		return FilterText;
	}

	/** Set the Text to be used as the Filter's restrictions */
	void SetRawFilterText( const FText& InFilterText )
	{
		// Sanitize the new string
		FText NewText = FText::TrimPrecedingAndTrailing(InFilterText);
		
		// If the text differs, re-parse and split the filter text into search terms
		if ( !NewText.EqualTo(FilterText) )
		{
			FilterText = NewText;
			FilterSearchTerms.Reset();
			FilterSearchTerms.Add( TArray< FString >() );
			int32 SearchTermIndex = 0;

			bool bIsWithinQuotedSection = false;
			FString NewSearchTerm;
			int32 StringLength = FilterText.ToString().Len();
			for( auto CurCharIndex = 0; CurCharIndex < StringLength; ++CurCharIndex )
			{
				const auto CurChar = FilterText.ToString().GetCharArray()[ CurCharIndex ];

				// Keep an eye out for double-quotes.  We want to retain whitespace within a search term if
				// it has double-quotes around it
				if( CurChar == TCHAR('\"') )
				{
					// Toggle whether we're within a quoted section, but don't bother adding the quote character
					bIsWithinQuotedSection = !bIsWithinQuotedSection;
				}
				else if ( CurChar == TCHAR('|') )
				{
					if( NewSearchTerm.Len() > 0 )
					{
						FilterSearchTerms[SearchTermIndex].Add( NewSearchTerm );
					}

					++SearchTermIndex;
					FilterSearchTerms.Add( TArray< FString >() );
					NewSearchTerm = FString();
				}
				else if( bIsWithinQuotedSection || !FChar::IsWhitespace( CurChar ) )
				{
					// Add the character!
					NewSearchTerm.AppendChar( CurChar );
				}
				else
				{
					// Encountered whitespace, so add the search term up to here
					if( NewSearchTerm.Len() > 0 )
					{
						if ( NewSearchTerm == TEXT("OR") )
						{
							++SearchTermIndex;
							FilterSearchTerms.Add( TArray< FString >() );
						}
						else
						{
							FilterSearchTerms[SearchTermIndex].Add( NewSearchTerm );
						}

						NewSearchTerm = FString();
					}
				}
			}

			// Encountered EOL, so add the search term up to here
			if( NewSearchTerm.Len() > 0 )
			{
				if ( NewSearchTerm == TEXT("OR") )
				{
					++SearchTermIndex;
					FilterSearchTerms.Add( TArray< FString >() );
				}
				else
				{
					FilterSearchTerms[SearchTermIndex].Add( NewSearchTerm );
				}

				NewSearchTerm = FString();
			}

			if( bIsWithinQuotedSection )
			{
				// User forgot to terminate their double-quoted section.  No big deal.
			}

			//Remove the last array of search terms if it is empty
			if ( FilterSearchTerms[SearchTermIndex].Num() == 0 )
			{
				FilterSearchTerms.RemoveAt( SearchTermIndex );
			}

			ChangedEvent.Broadcast();
		}
	}


private:

	/** The delegate used to transform an item into an array of strings */
	FItemToStringArray TransformArray;

	/** An array containing all the strings returned by the last Transform delegate invoked */
	mutable TArray< FString > ItemTextCollection;

	/** Split and sanitized list of filter search terms.  Cached for faster search performance. */
	TArray< TArray< FString > > FilterSearchTerms;

	/** The original unsplit and unsanitized filter terms */
	FText FilterText;

	/**	The event that fires whenever new search terms are provided */
	FChangedEvent ChangedEvent;
};
