// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"


/* FString implementation
 *****************************************************************************/

void FString::TrimToNullTerminator()
{
	if( Data.Num() )
	{
		int32 DataLen = FCString::Strlen(Data.GetData());
		check(DataLen == 0 || DataLen < Data.Num());
		int32 Len = DataLen > 0 ? DataLen+1 : 0;

		check(Len <= Data.Num())
		Data.RemoveAt(Len, Data.Num()-Len);
	}
}


int32 FString::Find( const TCHAR* SubStr, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir, int32 StartPosition ) const
{
	if ( SubStr == NULL )
	{
		return INDEX_NONE;
	}
	if( SearchDir == ESearchDir::FromStart)
	{
		const TCHAR* Start = **this;
		if ( StartPosition != INDEX_NONE )
		{
			Start += FMath::Clamp(StartPosition, 0, Len() - 1);
		}
		const TCHAR* Tmp = SearchCase == ESearchCase::IgnoreCase
			? FCString::Stristr(Start, SubStr)
			: FCString::Strstr(Start, SubStr);

		return Tmp ? (Tmp-**this) : -1;
	}
	else
	{
		// if ignoring, do a onetime ToUpper on both strings, to avoid ToUppering multiple
		// times in the loop below
		if ( SearchCase == ESearchCase::IgnoreCase)
		{
			return ToUpper().Find(FString(SubStr).ToUpper(), ESearchCase::CaseSensitive, SearchDir, StartPosition);
		}
		else
		{
			const int32 SearchStringLength=FMath::Max(1, FCString::Strlen(SubStr));
			
			if ( StartPosition == INDEX_NONE )
			{
				StartPosition = Len();
			}
			
			for( int32 i = StartPosition - SearchStringLength; i >= 0; i-- )
			{
				int32 j;
				for( j=0; SubStr[j]; j++ )
				{
					if( (*this)[i+j]!=SubStr[j] )
					{
						break;
					}
				}
				
				if( !SubStr[j] )
				{
					return i;
				}
			}
			return -1;
		}
	}
}

FString FString::ToUpper() const
{
	FString New( **this );
	for( int32 i=0; i < New.Data.Num(); i++ )
	{
		New[i] = FChar::ToUpper(New[i]);
	}
	return New;
}

FString FString::ToLower() const
{
	FString New( **this );
	for( int32 i=0; i < New.Data.Num(); i++ )
	{
		New[i] = FChar::ToLower(New[i]);
	}
	return New;
}

bool FString::StartsWith(const FString& InPrefix, ESearchCase::Type SearchCase ) const
{
	if( SearchCase == ESearchCase::IgnoreCase )
	{
		return InPrefix.Len() > 0 && !FCString::Strnicmp(**this, *InPrefix, InPrefix.Len());
	}
	else
	{
		return InPrefix.Len() > 0 && !FCString::Strncmp(**this, *InPrefix, InPrefix.Len());
	}
}

bool FString::EndsWith(const FString& InSuffix, ESearchCase::Type SearchCase ) const
{
	if( SearchCase == ESearchCase::IgnoreCase )
	{
		return InSuffix.Len() > 0 &&
			Len() >= InSuffix.Len() &&
			!FCString::Stricmp( &(*this)[ Len() - InSuffix.Len() ], *InSuffix );
	}
	else
	{
		return InSuffix.Len() > 0 &&
			Len() >= InSuffix.Len() &&
			!FCString::Strcmp( &(*this)[ Len() - InSuffix.Len() ], *InSuffix );
	}
}

bool FString::RemoveFromStart( const FString& InPrefix, ESearchCase::Type SearchCase )
{
	if ( InPrefix.IsEmpty() )
	{
		return false;
	}

	if ( StartsWith( InPrefix, SearchCase ) )
	{
		RemoveAt( 0, InPrefix.Len() );
		return true;
	}

	return false;
}

bool FString::RemoveFromEnd( const FString& InSuffix, ESearchCase::Type SearchCase )
{
	if ( InSuffix.IsEmpty() )
	{
		return false;
	}

	if ( EndsWith( InSuffix, SearchCase ) )
	{
		RemoveAt( Len() - InSuffix.Len(), InSuffix.Len() );
		return true;
	}

	return false;
}

FString FString::Trim()
{
	int32 Pos = 0;
	while(Pos < Len())
	{
		if( FChar::IsWhitespace( (*this)[Pos] ) )
		{
			Pos++;
		}
		else
		{
			break;
		}
	}

	*this = Right( Len()-Pos );

	return *this;
}

FString FString::TrimTrailing( void )
{
	int32 Pos = Len() - 1;
	while( Pos >= 0 )
	{
		if( !FChar::IsWhitespace( ( *this )[Pos] ) )
		{
			break;
		}

		Pos--;
	}

	*this = Left( Pos + 1 );

	return( *this );
}

FString FString::TrimQuotes( bool* bQuotesRemoved ) const
{
	bool bQuotesWereRemoved=false;
	int32 Start = 0, Count = Len();
	if ( Count > 0 )
	{
		if ( (*this)[0] == TCHAR('"') )
		{
			Start++;
			Count--;
			bQuotesWereRemoved=true;
		}

		if ( Len() > 1 && (*this)[Len() - 1] == TCHAR('"') )
		{
			Count--;
			bQuotesWereRemoved=true;
		}
	}

	if ( bQuotesRemoved != NULL )
	{
		*bQuotesRemoved = bQuotesWereRemoved;
	}
	return Mid(Start, Count);
}

int32 FString::CullArray( TArray<FString>* InArray )
{
	check(InArray);
	FString Empty;
	InArray->Remove(Empty);
	return InArray->Num();
}

FString FString::Reverse() const
{
	FString New(*this);
	New.ReverseString();
	return New;
}

void FString::ReverseString()
{
	if ( Len() > 0 )
	{
		TCHAR* StartChar = &(*this)[0];
		TCHAR* EndChar = &(*this)[Len()-1];
		TCHAR TempChar;
		do 
		{
			TempChar = *StartChar;	// store the current value of StartChar
			*StartChar = *EndChar;	// change the value of StartChar to the value of EndChar
			*EndChar = TempChar;	// change the value of EndChar to the character that was previously at StartChar

			StartChar++;
			EndChar--;

		} while( StartChar < EndChar );	// repeat until we've reached the midpoint of the string
	}
}

FString FString::FormatAsNumber( int32 InNumber )
{
	FString Number = FString::FromInt( InNumber ), Result;

	int32 dec = 0;
	for( int32 x = Number.Len()-1 ; x > -1 ; --x )
	{
		Result += Number.Mid(x,1);

		dec++;
		if( dec == 3 && x > 0 )
		{
			Result += TEXT(",");
			dec = 0;
		}
	}

	return Result.Reverse();
}

/**
 * Serializes a string as ANSI char array.
 *
 * @param	String			String to serialize
 * @param	Ar				Archive to serialize with
 * @param	MinCharacters	Minimum number of characters to serialize.
 */
void FString::SerializeAsANSICharArray( FArchive& Ar, int32 MinCharacters ) const
{
	int32	Length = FMath::Max( Len(), MinCharacters );
	Ar << Length;
	
	for( int32 CharIndex=0; CharIndex<Len(); CharIndex++ )
	{
		ANSICHAR AnsiChar = CharCast<ANSICHAR>( (*this)[CharIndex] );
		Ar << AnsiChar;
	}

	// Zero pad till minimum number of characters are written.
	for( int32 i=Len(); i<Length; i++ )
	{
		ANSICHAR NullChar = 0;
		Ar << NullChar;
	}
}

void FString::AppendInt( int32 InNum )
{
	int64 Num						= InNum; // This avoids having to deal with negating -MAX_int32-1
	const TCHAR* NumberChar[11]		= { TEXT("0"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("-") };
	bool bIsNumberNegative			= false;
	TCHAR TempNum[16];				// 16 is big enough
	int32 TempAt					= 16; // fill the temp string from the top down.

	// Correctly handle negative numbers and convert to positive integer.
	if( Num < 0 )
	{
		bIsNumberNegative = true;
		Num = -Num;
	}

	TempNum[--TempAt] = 0; // NULL terminator

	// Convert to string assuming base ten and a positive integer.
	do 
	{
		TempNum[--TempAt] = *NumberChar[Num % 10];
		Num /= 10;
	} while( Num );

	// Append sign as we're going to reverse string afterwards.
	if( bIsNumberNegative )
	{
		TempNum[--TempAt] = *NumberChar[10];
	}

	*this += TempNum + TempAt;
}

/**
 * Converts a string into a boolean value
 *   1, "True", "Yes", GTrue, GYes, and non-zero integers become true
 *   0, "False", "No", GFalse, GNo, and unparsable values become false
 *
 * @param Source the string to parse
 *
 * @return The boolean value
 */
bool FString::ToBool() const
{
	return FCString::ToBool(**this);
}

/**
 * Converts a buffer to a string by hex-ifying the elements
 *
 * @param SrcBuffer the buffer to stringify
 * @param SrcSize the number of bytes to convert
 *
 * @return the blob in string form
 */
FString FString::FromBlob(const uint8* SrcBuffer,const uint32 SrcSize)
{
	FString Result;
	// Convert and append each byte in the buffer
	for (uint32 Count = 0; Count < SrcSize; Count++)
	{
		Result += FString::Printf(TEXT("%03d"),(uint32)SrcBuffer[Count]);
	}
	return Result;
}

/**
 * Converts a string into a buffer
 *
 * @param DestBuffer the buffer to fill with the string data
 * @param DestSize the size of the buffer in bytes (must be at least string len / 2)
 *
 * @return true if the conversion happened, false otherwise
 */
bool FString::ToBlob(const FString& Source,uint8* DestBuffer,const uint32 DestSize)
{
	// Make sure the buffer is at least half the size and that the string is an
	// even number of characters long
	if (DestSize >= (uint32)(Source.Len() / 3) &&
		(Source.Len() % 3) == 0)
	{
		TCHAR ConvBuffer[4];
		ConvBuffer[3] = TEXT('\0');
		int32 WriteIndex = 0;
		// Walk the string 2 chars at a time
		for (int32 Index = 0; Index < Source.Len(); Index += 3, WriteIndex++)
		{
			ConvBuffer[0] = Source[Index];
			ConvBuffer[1] = Source[Index + 1];
			ConvBuffer[2] = Source[Index + 2];
			DestBuffer[WriteIndex] = FCString::Atoi(ConvBuffer);
		}
		return true;
	}
	return false;
}

FString FString::SanitizeFloat( double InFloat )
{
	// Avoids negative zero
	if( InFloat == 0 )
	{
		InFloat = 0;
	}

	FString TempString;
	// First create the string
	TempString = FString::Printf(TEXT("%f"), InFloat );
	const TArray< TCHAR >& Chars = TempString.GetCharArray();	
	const TCHAR Zero = '0';
	const TCHAR Period = '.';
	int32 TrimIndex = 0;
	// Find the first non-zero char in the array
	for (int32 Index = Chars.Num()-2; Index >= 2; --Index )
	{
		const TCHAR EachChar = Chars[Index];
		const TCHAR NextChar = Chars[Index-1];
		if( ( EachChar != Zero ) || (NextChar == Period ) )
		{			
			TrimIndex = Index;
			break;
		}
	}	
	// If we changed something trim the string
	if( TrimIndex != 0 )
	{
		TempString = TempString.Left( TrimIndex + 1 );
	}
	return TempString;
}

FString FString::Chr( TCHAR Ch )
{
	TCHAR Temp[2]={Ch,0};
	return FString(Temp);
}


FString FString::ChrN( int32 NumCharacters, TCHAR Char )
{
	check( NumCharacters >= 0 );

	FString Temp;
	Temp.Data.AddUninitialized(NumCharacters+1);
	for( int32 Cx = 0; Cx < NumCharacters; ++Cx )
	{
		Temp[Cx] = Char;
	}
	Temp[NumCharacters]=0;
	return Temp;
}

FString FString::LeftPad( int32 ChCount ) const
{
	int32 Pad = ChCount - Len();

	if (Pad > 0)
	{
		return ChrN(Pad, ' ') + *this;
	}
	else
	{
		return *this;
	}
}
FString FString::RightPad( int32 ChCount ) const
{
	int32 Pad = ChCount - Len();

	if (Pad > 0)
	{
		return *this + ChrN(Pad, ' ');
	}
	else
	{
		return *this;
	}
}

bool FString::IsNumeric() const
{
	if (IsEmpty())
	{
		return 0;
	}

	TCHAR C = (*this)[0];
	
	if( C == '-' || C == '+' || C =='.' || FChar::IsDigit( C ) )
	{
		bool HasDot = (C == '.');

		for( int32 i=1; i<Len(); i++ )
		{
			C = (*this)[i];

			if( C == '.' )
			{
				if( HasDot )
				{
					return 0;
				}
				else
				{
					HasDot = 1;
				}
			}
			else if( !FChar::IsDigit(C) )
			{
				return 0;
			}
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Breaks up a delimited string into elements of a string array.
 *
 * @param	InArray		The array to fill with the string pieces
 * @param	pchDelim	The string to delimit on
 * @param	InCullEmpty	If 1, empty strings are not added to the array
 *
 * @return	The number of elements in InArray
 */
int32 FString::ParseIntoArray( TArray<FString>* InArray, const TCHAR* pchDelim, bool InCullEmpty ) const
{
	// Make sure the delimit string is not null or empty
	check(pchDelim);
	check(InArray);
	InArray->Empty();
	const TCHAR *Start = Data.GetData();
	int32 DelimLength = FCString::Strlen(pchDelim);
	if (Start && DelimLength)
	{
		while( const TCHAR *At = FCString::Strstr(Start,pchDelim) )
		{
			if (!InCullEmpty || At-Start)
			{
				new (*InArray) FString(At-Start,Start);
			}
			Start += DelimLength + (At-Start);
		}
		if (!InCullEmpty || *Start)
		{
			new(*InArray) FString(Start);
		}

	}
	return InArray->Num();
}

/**
 * Takes a string, and skips over all instances of white space and returns the new string
 *
 * @param	WhiteSpace		An array of white space strings
 * @param	NumWhiteSpaces	The length of the WhiteSpace array
 * @param	S				The input and output string
 */
static void SkipOver(const TCHAR** WhiteSpace, int32 NumWhiteSpaces, FString& S)
{
	bool bStop = false;

	// keep going until we hit non-white space
	while (!bStop)
	{
		// we stop it we don't find any white space
		bStop = true;
		// loop over all possible white spaces to search for
		for (int32 iWS = 0; iWS < NumWhiteSpaces; iWS++)
		{
			// get the length (tiny optimization)
			int32 WSLen = FCString::Strlen(WhiteSpace[iWS]);

			// if we start with this bit of whitespace, chop it off, and keep looking for more whitespace
			if (FCString::Strnicmp(*S, WhiteSpace[iWS], WSLen) == 0)
			{
				// chop it off
				S = S.Mid(WSLen);
				// keep looking!
				bStop = false;
				break;
			}
		}
	}
}

/**
 * Splits the input string on the first bit of white space, and returns the initial token
 * (to the left of the white space), and the rest (white space and to the right)
 *
 * @param	WhiteSpace		An array of white space strings
 * @param	NumWhiteSpaces	The length of the WhiteSpace array
 * @param	Token			The first token before any white space
 * @param	S				The input and outputted remainder string
 *
 * @return	Was there a valid token before the end of the string?
 */
static bool SplitOn( const TCHAR** WhiteSpace, int32 NumWhiteSpaces, FString& Token, FString& S, TCHAR& InCh )
{
	// this is the index of the first instance of whitespace
	int32 SmallestToken = MAX_int32;
	InCh = TEXT(' ');

	// loop through all possible white spaces
	for (int32 iWS = 0; iWS < NumWhiteSpaces; iWS++)
	{
		// look for the first instance of it
		int32 NextWS = S.Find(WhiteSpace[iWS]);

		// if shouldn't be at the start of the string, because SkipOver should have been called
		check(NextWS != 0);

		// if we found this white space, and it is before any other white spaces, remember it
		if (NextWS > 0 && NextWS < SmallestToken)
		{
			SmallestToken = NextWS;
			InCh = *WhiteSpace[iWS];
		}
	}

	// if we found some white space, SmallestToken is pointing to the the first one
	if (SmallestToken != MAX_int32)
	{
		// get the token before the white space
		Token = S.Left(SmallestToken);
		// update out string with the remainder
		S = S.Mid(SmallestToken);
		// we found a token
		return true;
	}

	// we failed to find a token
	return false;
}

bool FString::MatchesWildcard(const FString& InWildcard, ESearchCase::Type SearchCase) const
{
	FString Wildcard(InWildcard);
	FString Target(*this);
	int32 IndexOfStar = Wildcard.Find(TEXT("*"), ESearchCase::CaseSensitive, ESearchDir::FromEnd); // last occurance
	int32 IndexOfQuestion = Wildcard.Find( TEXT("?"), ESearchCase::CaseSensitive, ESearchDir::FromEnd); // last occurance
	int32 Suffix = FMath::Max<int32>(IndexOfStar, IndexOfQuestion);
	if (Suffix == INDEX_NONE)
	{
		// no wildcards
		if (SearchCase == ESearchCase::IgnoreCase)
		{
			return FCString::Stricmp( *Target, *Wildcard )==0;
		}
		else
		{
			return FCString::Strcmp( *Target, *Wildcard )==0;
		}
	}
	else
	{
		if (Suffix + 1 < Wildcard.Len())
		{
			FString SuffixString = Wildcard.Mid(Suffix + 1);
			if (!Target.EndsWith(SuffixString, SearchCase))
			{
				return false;
			}
			Wildcard = Wildcard.Left(Suffix + 1);
			Target = Target.Left(Target.Len() - SuffixString.Len());
		}
		int32 PrefixIndexOfStar = Wildcard.Find(TEXT("*")); 
		int32 PrefixIndexOfQuestion = Wildcard.Find( TEXT("?"));
		int32 Prefix = FMath::Min<int32>(PrefixIndexOfStar < 0 ? MAX_int32 : PrefixIndexOfStar, PrefixIndexOfQuestion < 0 ? MAX_int32 : PrefixIndexOfQuestion);
		check(Prefix >= 0 && Prefix < Wildcard.Len());
		if (Prefix > 0)
		{
			FString PrefixString = Wildcard.Left(Prefix);
			if (!Target.StartsWith(PrefixString, SearchCase))
			{
				return false;
			}
			Wildcard = Wildcard.Mid(Prefix);
			Target = Target.Mid(Prefix);
		}
	}
	// This routine is very slow, though it does ok with one wildcard
	check(Wildcard.Len());
	TCHAR FirstWild = Wildcard[0];
	Wildcard = Wildcard.Right(Wildcard.Len() - 1);
	if (FirstWild == TEXT('*') || FirstWild == TEXT('?'))
	{
		if (!Wildcard.Len())
		{
			if (FirstWild == TEXT('*') || Target.Len() < 2)
			{
				return true;
			}
		}
		int32 MaxNum = FMath::Min<int32>(Target.Len(), FirstWild == TEXT('?') ? 1 : MAX_int32);
		for (int32 Index = 0; Index <= MaxNum; Index++)
		{
			if (Target.Right(Target.Len() - Index).MatchesWildcard(Wildcard, SearchCase))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		check(0); // we should have dealt with prefix comparison above
		return false;
	}
}


/** Caution!! this routine is O(N^2) allocations...use it for parsing very short text or not at all */
int32 FString::ParseIntoArrayWS( TArray<FString>* InArray, const TCHAR* pchExtraDelim ) const
{
	// default array of White Spaces, the last entry can be replaced with the optional pchExtraDelim string
	// (if you want to split on white space and another character)
	static const TCHAR* WhiteSpace[] = 
	{
		TEXT(" "),
		TEXT("\t"),
		TEXT("\r"),
		TEXT("\n"),
		TEXT(""),
	};

	// start with just the standard whitespaces
	int32 NumWhiteSpaces = ARRAY_COUNT(WhiteSpace) - 1;
	// if we got one passed in, use that in addition
	if (pchExtraDelim && *pchExtraDelim)
	{
		WhiteSpace[NumWhiteSpaces++] = pchExtraDelim;
	}

	return ParseIntoArray(InArray, WhiteSpace, NumWhiteSpaces);
}

int32 FString::ParseIntoArrayLines(TArray<FString>* InArray) const
{
	// default array of LineEndings
	static const TCHAR* LineEndings[] =
	{				
		TEXT("\r"),
		TEXT("\n"),	
	};

	// start with just the standard line endings
	int32 NumLineEndings = ARRAY_COUNT(LineEndings);	
	return ParseIntoArray(InArray, LineEndings, NumLineEndings);
}

int32 FString::ParseIntoArray(TArray<FString>* InArray, const TCHAR** DelimArray, int32 NumDelims) const
{
	check(DelimArray);
	check(InArray);
	InArray->Empty();

	// this is our temporary workhorse string
	FString S = *this;

	bool bStop = false;
	// keep going until we run out of tokens
	while (!bStop)
	{
		// skip over any white space at the beginning of the string
		SkipOver(DelimArray, NumDelims, S);

		// find the first token in the string, and if we get one, add it to the output array of tokens
		FString Token;
		TCHAR ch;
		if (SplitOn(DelimArray, NumDelims, Token, S, ch))
		{
			if (Token[0] == TEXT('"'))
			{
				int32 SaveSz = Token.Len();

				FString Wk = FString::Printf(TEXT("%s%c"), *Token, ch);
				for (int32 x = 1; x < S.Len(); ++x)
				{
					if (S[x] == TEXT('"'))
					{
						Wk += TEXT("\"");
						break;
					}
					else
					{
						Wk = Wk + S.Mid(x, 1);
					}
				}

				Token = Wk;

				int32 DiffSz = Token.Len() - SaveSz;
				S = S.Mid(DiffSz);
			}

			// stick it on the end
			new(*InArray)FString(Token);
		}
		else
		{
			// if the remaining string is not empty, then we need to add the last token
			if (S.Len())
			{
				new(*InArray)FString(S);
			}

			// and, we're done this crazy ride
			bStop = true;
		}
	}

	// simply return the number of elements in the output array
	return InArray->Num();
}

FString FString::Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase) const
{
	// Previous code used to accidentally accept a NULL replacement string - this is no longer accepted.
	check(To);

	if (IsEmpty() || !From || !*From)
	{
		return *this;
	}

	// get a pointer into the character data
	const TCHAR* Travel = Data.GetData();

	// precalc the lengths of the replacement strings
	int32 FromLength = FCString::Strlen(From);
	int32 ToLength   = FCString::Strlen(To);

	FString Result;
	while (true)
	{
		// look for From in the remaining string
		const TCHAR* FromLocation = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(Travel, From) : FCString::Strstr(Travel, From);
		if (!FromLocation)
			break;

		// copy everything up to FromLocation
		Result.AppendChars(Travel, FromLocation - Travel);

		// copy over the To
		Result.AppendChars(To, ToLength);

		Travel = FromLocation + FromLength;
	}

	// copy anything left over
	Result += Travel;

	return Result;
}

int32 FString::ReplaceInline( const TCHAR* SearchText, const TCHAR* ReplacementText, ESearchCase::Type SearchCase )
{
	int32 ReplacementCount = 0;

	if (Len() > 0
	&&	SearchText != NULL && *SearchText != 0
	&&	ReplacementText != NULL && (SearchCase == ESearchCase::IgnoreCase || FCString::Strcmp(SearchText, ReplacementText) != 0) )
	{
		const int32 NumCharsToReplace=FCString::Strlen(SearchText);
		const int32 NumCharsToInsert=FCString::Strlen(ReplacementText);

		if ( NumCharsToInsert == NumCharsToReplace )
		{
			TCHAR* Pos = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(&(*this)[0], SearchText) : FCString::Strstr(&(*this)[0], SearchText);
			while ( Pos != NULL )
			{
				ReplacementCount++;

				// FCString::Strcpy now inserts a terminating zero so can't use that
				for ( int32 i = 0; i < NumCharsToInsert; i++ )
				{
					Pos[i] = ReplacementText[i];
				}

				if ( Pos + NumCharsToReplace - **this < Len() )
				{
					Pos = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(Pos + NumCharsToReplace, SearchText) : FCString::Strstr(Pos + NumCharsToReplace, SearchText);
				}
				else
				{
					break;
				}
			}
		}
		else if ( Contains(SearchText, SearchCase) )
		{
			FString Copy(*this);
			Empty(Len());

			// get a pointer into the character data
			TCHAR* WritePosition = (TCHAR*)Copy.Data.GetData();
			// look for From in the remaining string
			TCHAR* SearchPosition = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(WritePosition, SearchText) : FCString::Strstr(WritePosition, SearchText);
			while ( SearchPosition != NULL )
			{
				ReplacementCount++;

				// replace the first letter of the From with 0 so we can do a strcpy (FString +=)
				*SearchPosition = 0;

				// copy everything up to the SearchPosition
				(*this) += WritePosition;

				// copy over the ReplacementText
				(*this) += ReplacementText;

				// restore the letter, just so we don't have 0's in the string
				*SearchPosition = *SearchText;

				WritePosition = SearchPosition + NumCharsToReplace;
				SearchPosition = SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(WritePosition, SearchText) : FCString::Strstr(WritePosition, SearchText);
			}

			// copy anything left over
			(*this) += WritePosition;
		}
	}

	return ReplacementCount;
}


/**
 * Returns a copy of this string with all quote marks escaped (unless the quote is already escaped)
 */
FString FString::ReplaceQuotesWithEscapedQuotes() const
{
	if ( Contains(TEXT("\"")) )
	{
		FString Result;

		const TCHAR* pChar = **this;

		bool bEscaped = false;
		while ( *pChar != 0 )
		{
			if ( bEscaped )
			{
				bEscaped = false;
			}
			else if ( *pChar == TCHAR('\\') )
			{
				bEscaped = true;
			}
			else if ( *pChar == TCHAR('"') )
			{
				Result += TCHAR('\\');
			}

			Result += *pChar++;
		}
		
		return Result;
	}

	return *this;
}

static const TCHAR* CharToEscapeSeqMap[][2] =
{
	// Always replace \\ first to avoid double-escaping characters
	{ TEXT("\\"), TEXT("\\\\") },
	{ TEXT("\n"), TEXT("\\n")  },
	{ TEXT("\r"), TEXT("\\r")  },
	{ TEXT("\t"), TEXT("\\t")  },
	{ TEXT("\'"), TEXT("\\'")  },
	{ TEXT("\""), TEXT("\\\"") }
};

static const uint32 MaxSupportedEscapeChars = ARRAY_COUNT(CharToEscapeSeqMap);

/**
 * Replaces certain characters with the "escaped" version of that character (i.e. replaces "\n" with "\\n").
 * The characters supported are: { \n, \r, \t, \', \", \\ }.
 *
 * @param	Chars	by default, replaces all supported characters; this parameter allows you to limit the replacement to a subset.
 *
 * @return	a string with all control characters replaced by the escaped version.
 */
FString FString::ReplaceCharWithEscapedChar( const TArray<TCHAR>* Chars/*=NULL*/ ) const
{
	if ( Len() > 0 && (Chars == NULL || Chars->Num() > 0) )
	{
		FString Result(*this);
		for ( int32 ChIdx = 0; ChIdx < MaxSupportedEscapeChars; ChIdx++ )
		{
			if ( Chars == NULL || Chars->Contains(*(CharToEscapeSeqMap[ChIdx][0])) )
			{
				// use ReplaceInline as that won't create a copy of the string if the character isn't found
				Result.ReplaceInline(CharToEscapeSeqMap[ChIdx][0], CharToEscapeSeqMap[ChIdx][1]);
			}
		}
		return Result;
	}

	return *this;
}
/**
 * Removes the escape backslash for all supported characters, replacing the escape and character with the non-escaped version.  (i.e.
 * replaces "\\n" with "\n".  Counterpart to ReplaceCharWithEscapedChar().
 */
FString FString::ReplaceEscapedCharWithChar( const TArray<TCHAR>* Chars/*=NULL*/ ) const
{
	if ( Len() > 0 && (Chars == NULL || Chars->Num() > 0) )
	{
		FString Result(*this);
		// Spin CharToEscapeSeqMap backwards to ensure we're doing the inverse of ReplaceCharWithEscapedChar
		for ( int32 ChIdx = MaxSupportedEscapeChars - 1; ChIdx >= 0; ChIdx-- )
		{
			if ( Chars == NULL || Chars->Contains(*(CharToEscapeSeqMap[ChIdx][0])) )
			{
				// use ReplaceInline as that won't create a copy of the string if the character isn't found
				Result.ReplaceInline( CharToEscapeSeqMap[ChIdx][1], CharToEscapeSeqMap[ChIdx][0] );
			}
		}
		return Result;
	}

	return *this;
}

/** 
 * Replaces all instances of '\t' with TabWidth number of spaces
 * @param InSpacesPerTab - Number of spaces that a tab represents
 */
FString FString::ConvertTabsToSpaces (const int32 InSpacesPerTab)
{
	//must call this with at least 1 space so the modulus operation works
	check(InSpacesPerTab > 0);

	FString FinalString = *this;
	int32 TabIndex;
	while ((TabIndex = FinalString.Find(TEXT("\t"))) != INDEX_NONE )
	{
		FString LeftSide = FinalString.Left(TabIndex);
		FString RightSide = FinalString.Mid(TabIndex+1);

		FinalString = LeftSide;
		//for a tab size of 4, 
		int32 LineBegin = LeftSide.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, TabIndex);
		if (LineBegin == INDEX_NONE)
		{
			LineBegin = 0;
		}
		int32 CharactersOnLine = (LeftSide.Len()-LineBegin);

		int32 NumSpacesForTab = InSpacesPerTab - (CharactersOnLine % InSpacesPerTab);
		for (int32 i = 0; i < NumSpacesForTab; ++i)
		{
			FinalString.AppendChar(' ');
		}
		FinalString += RightSide;
	}

	return FinalString;
}

// This starting size catches 99.97% of printf calls - there are about 700k printf calls per level
#define STARTING_BUFFER_SIZE		512

VARARG_BODY( FString, FString::Printf, const TCHAR*, VARARG_NONE )
{
	int32		BufferSize	= STARTING_BUFFER_SIZE;
	TCHAR	StartingBuffer[STARTING_BUFFER_SIZE];
	TCHAR*	Buffer		= StartingBuffer;
	int32		Result		= -1;

	// First try to print to a stack allocated location 
	GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );

	// If that fails, start allocating regular memory
	if( Result == -1 )
	{
		Buffer = NULL;
		while(Result == -1)
		{
			BufferSize *= 2;
			Buffer = (TCHAR*) FMemory::Realloc( Buffer, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		};
	}

	Buffer[Result] = 0;

	FString ResultString(Buffer);

	if( BufferSize != STARTING_BUFFER_SIZE )
	{
		FMemory::Free( Buffer );
	}

	return ResultString;
}

FArchive& operator<<( FArchive& Ar, FString& A )
{
	// > 0 for ANSICHAR, < 0 for UCS2CHAR serialization

	if (Ar.IsLoading())
	{
		int32 SaveNum;
		Ar << SaveNum;

		bool LoadUCS2Char = SaveNum < 0;
		if (LoadUCS2Char)
		{
			SaveNum = -SaveNum;
		}

		// Protect against network packets allocating too much memory
		auto MaxSerializeSize = Ar.GetMaxSerializeSize();
		if (MaxSerializeSize > 0 && ( SaveNum > MaxSerializeSize || SaveNum < 0 ))		// If SaveNum is still less than 0, they must have passed in MIN_INT
		{
			Ar.ArIsError         = 1;
			Ar.ArIsCriticalError = 1;
			UE_LOG( LogNetSerialization, Error, TEXT( "String is too large" ) );
			return Ar;
		}

		// Resize the array only if it passes the above tests to prevent rogue packets from crashing
		A.Data.Empty           (SaveNum);
		A.Data.AddUninitialized(SaveNum);

		if (SaveNum)
		{
			if (LoadUCS2Char)
			{
				// read in the unicode string and byteswap it, etc
				auto Passthru = StringMemoryPassthru<UCS2CHAR>(A.Data.GetData(), SaveNum, SaveNum);
				Ar.Serialize(Passthru.Get(), SaveNum * sizeof(UCS2CHAR));
				// Ensure the string has a null terminator
				Passthru.Get()[SaveNum-1] = '\0';
				Passthru.Apply();

				INTEL_ORDER_TCHARARRAY(A.Data.GetData())

				// Since Microsoft's vsnwprintf implementation raises an invalid parameter warning
				// with a character of 0xffff, scan for it and terminate the string there.
				// 0xffff isn't an actual Unicode character anyway.
				int Index = 0;
				if(A.FindChar(0xffff, Index))
				{
					A[Index] = '\0';
					A.TrimToNullTerminator();		
				}
			}
			else
			{
				auto Passthru = StringMemoryPassthru<ANSICHAR>(A.Data.GetData(), SaveNum, SaveNum);
				Ar.Serialize(Passthru.Get(), SaveNum * sizeof(ANSICHAR));
				// Ensure the string has a null terminator
				Passthru.Get()[SaveNum-1] = '\0';
				Passthru.Apply();
			}

			// Throw away empty string.
			if (SaveNum == 1)
			{
				A.Data.Empty();
			}
		}
	}
	else
	{
		bool  SaveUCS2Char = Ar.IsForcingUnicode() || !FCString::IsPureAnsi(*A);
		int32 Num        = A.Data.Num();
		int32 SaveNum    = SaveUCS2Char ? -Num : Num;

		Ar << SaveNum;

		A.Data.CountBytes( Ar );

		if (SaveNum)
		{
			if (SaveUCS2Char)
			{
				// TODO - This is creating a temporary in order to byte-swap.  Need to think about how to make this not necessary.
				#if !PLATFORM_LITTLE_ENDIAN
					FString  ATemp  = A;
					FString& A      = ATemp;
					INTEL_ORDER_TCHARARRAY(A.Data.GetData());
				#endif

				Ar.Serialize((void*)StringCast<UCS2CHAR>(A.Data.GetData(), Num).Get(), sizeof(UCS2CHAR)* Num);
			}
			else
			{
				Ar.Serialize((void*)StringCast<ANSICHAR>(A.Data.GetData(), Num).Get(), sizeof(ANSICHAR)* Num);
			}
		}
	}

	return Ar;
}
