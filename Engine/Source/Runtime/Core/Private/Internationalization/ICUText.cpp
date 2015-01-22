// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU
#include "Text.h"
#include "TextHistory.h"
#include "ICUCulture.h"
#include <unicode/coll.h>
#include <unicode/sortkey.h>
#include <unicode/numfmt.h>
#include <unicode/msgfmt.h>
#include "ICUUtilities.h"
#include "ICUTextCharacterIterator.h"

bool FText::IsWhitespace( const TCHAR Char )
{
	// TCHAR should either be UTF-16 or UTF-32, so we should be fine to cast it to a UChar32 for the whitespace
	// check, since whitespace is never a pair of UTF-16 characters
	const UChar32 ICUChar = static_cast<UChar32>(Char);
	return u_isWhitespace(ICUChar) != 0;
}

FText FText::AsDate(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const FString& TimeZone, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FCulturePtr Culture = TargetCulture.IsValid() ? TargetCulture : I18N.GetCurrentCulture();

	int32 UNIXTimestamp = DateTime.ToUnixTimestamp();
	UDate ICUDate = static_cast<double>(UNIXTimestamp) * U_MILLIS_PER_SECOND;

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DateFormat> ICUDateFormat( Culture->Implementation->GetDateFormatter(DateStyle, TimeZone) );
	icu::UnicodeString FormattedString;
	ICUDateFormat->format(ICUDate, FormattedString);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ResultText = FText::CreateChronologicalText(NativeString);
	ResultText.History = MakeShareable(new FTextHistory_AsDate(DateTime, DateStyle, TimeZone, TargetCulture));

	return ResultText;
}

FText FText::AsTime(const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FCulturePtr Culture = TargetCulture.IsValid() ? TargetCulture : I18N.GetCurrentCulture();

	int32 UNIXTimestamp = DateTime.ToUnixTimestamp();
	UDate ICUDate = static_cast<double>(UNIXTimestamp) * U_MILLIS_PER_SECOND;

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DateFormat> ICUDateFormat( Culture->Implementation->GetTimeFormatter(TimeStyle, TimeZone) );
	icu::UnicodeString FormattedString;
	ICUDateFormat->format(ICUDate, FormattedString);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ResultText = FText::CreateChronologicalText(NativeString);
	ResultText.History = MakeShareable(new FTextHistory_AsTime(DateTime, TimeStyle, TimeZone, TargetCulture));

	return ResultText;
}

FText FText::AsTimespan(const FTimespan& Timespan, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FCulturePtr Culture = TargetCulture.IsValid() ? TargetCulture : I18N.GetCurrentCulture();

	FText TimespanFormatPattern = NSLOCTEXT("Timespan", "FormatPattern", "{Hours}:{Minutes}:{Seconds}");

	double TotalHours = Timespan.GetTotalHours();
	int32 Hours = static_cast<int32>(TotalHours);
	int32 Minutes = Timespan.GetMinutes();
	int32 Seconds = Timespan.GetSeconds();

	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.MinimumIntegralDigits = 2;
	NumberFormattingOptions.MaximumIntegralDigits = 2;

	FFormatNamedArguments TimeArguments;
	TimeArguments.Add(TEXT("Hours"), Hours);
	TimeArguments.Add(TEXT("Minutes"), FText::AsNumber(Minutes, &(NumberFormattingOptions), Culture));
	TimeArguments.Add(TEXT("Seconds"), FText::AsNumber(Seconds, &(NumberFormattingOptions), Culture));
	return FText::Format(TimespanFormatPattern, TimeArguments);
}

FText FText::AsDateTime(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FCulturePtr Culture = TargetCulture.IsValid() ? TargetCulture : I18N.GetCurrentCulture();

	int32 UNIXTimestamp = DateTime.ToUnixTimestamp();
	UDate ICUDate = static_cast<double>(UNIXTimestamp) * U_MILLIS_PER_SECOND;

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DateFormat> ICUDateFormat( Culture->Implementation->GetDateTimeFormatter(DateStyle, TimeStyle, TimeZone) );
	icu::UnicodeString FormattedString;
	ICUDateFormat->format(ICUDate, FormattedString);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ResultText = FText::CreateChronologicalText(NativeString);
	ResultText.History = MakeShareable(new FTextHistory_AsDateTime(DateTime, DateStyle, TimeStyle, TimeZone, TargetCulture));

	return ResultText;
}

FText FText::AsMemory(SIZE_T NumBytes, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FFormatNamedArguments Args;

	if (NumBytes < 1024)
	{
		Args.Add( TEXT("Number"), FText::AsNumber( (uint64)NumBytes, Options, TargetCulture) );
		Args.Add( TEXT("Unit"), FText::FromString( FString( TEXT("B") ) ) );
		return FText::Format( NSLOCTEXT("Internationalization", "ComputerMemoryFormatting", "{Number} {Unit}"), Args );
	}

	static const TCHAR* Prefixes = TEXT("kMGTPEZY");
	int32 Prefix = 0;

	for (; NumBytes > 1024 * 1024; NumBytes >>= 10)
	{
		++Prefix;
	}

	const double MemorySizeAsDouble = (double)NumBytes / 1024.0;
	Args.Add( TEXT("Number"), FText::AsNumber( MemorySizeAsDouble, Options, TargetCulture) );
	Args.Add( TEXT("Unit"), FText::FromString( FString( 1, &Prefixes[Prefix] ) + TEXT("B") ) );
	return FText::Format( NSLOCTEXT("Internationalization", "ComputerMemoryFormatting", "{Number} {Unit}"), Args);
}

int32 FText::CompareTo( const FText& Other, const ETextComparisonLevel::Type ComparisonLevel ) const
{
	const TSharedRef<const icu::Collator, ESPMode::ThreadSafe> Collator( FInternationalization::Get().GetCurrentCulture()->Implementation->GetCollator(ComparisonLevel) );

	// Create an iterator for 'this' so that we can interface with ICU
	UCharIterator DisplayStringICUIterator;
	FICUTextCharacterIterator DisplayStringIterator(&DisplayString.Get());
	uiter_setCharacterIterator(&DisplayStringICUIterator, &DisplayStringIterator);

	// Create an iterator for 'Other' so that we can interface with ICU
	UCharIterator OtherDisplayStringICUIterator;
	FICUTextCharacterIterator OtherDisplayStringIterator(&Other.DisplayString.Get());
	uiter_setCharacterIterator(&OtherDisplayStringICUIterator, &OtherDisplayStringIterator);

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const UCollationResult Result = Collator->compare(DisplayStringICUIterator, OtherDisplayStringICUIterator, ICUStatus);

	return Result;
}

int32 FText::CompareToCaseIgnored( const FText& Other ) const
{
	return CompareTo(Other, ETextComparisonLevel::Secondary);
}

bool FText::EqualTo( const FText& Other, const ETextComparisonLevel::Type ComparisonLevel ) const
{
	return CompareTo(Other, ComparisonLevel) == 0;
}

bool FText::EqualToCaseIgnored( const FText& Other ) const
{
	return EqualTo(Other, ETextComparisonLevel::Secondary);
}

class FText::FSortPredicate::FSortPredicateImplementation
{
public:
	FSortPredicateImplementation(const ETextComparisonLevel::Type InComparisonLevel)
		: ComparisonLevel(InComparisonLevel)
		, ICUCollator(FInternationalization::Get().GetCurrentCulture()->Implementation->GetCollator(InComparisonLevel))
	{
	}

	bool Compare(const FText& A, const FText& B)
	{
		// Create an iterator for 'A' so that we can interface with ICU
		UCharIterator ADisplayStringICUIterator;
		FICUTextCharacterIterator ADisplayStringIterator(&A.DisplayString.Get());
		uiter_setCharacterIterator(&ADisplayStringICUIterator, &ADisplayStringIterator);

		// Create an iterator for 'B' so that we can interface with ICU
		UCharIterator BDisplayStringICUIterator;
		FICUTextCharacterIterator BDisplayStringIterator(&B.DisplayString.Get());
		uiter_setCharacterIterator(&BDisplayStringICUIterator, &BDisplayStringIterator);

		UErrorCode ICUStatus = U_ZERO_ERROR;
		const UCollationResult Result = ICUCollator->compare(ADisplayStringICUIterator, BDisplayStringICUIterator, ICUStatus);

		return Result != UCOL_GREATER;
	}

private:
	const ETextComparisonLevel::Type ComparisonLevel;
	const TSharedRef<const icu::Collator, ESPMode::ThreadSafe> ICUCollator;
};

FText::FSortPredicate::FSortPredicate(const ETextComparisonLevel::Type ComparisonLevel)
	: Implementation( new FSortPredicateImplementation(ComparisonLevel) )
{

}

bool FText::FSortPredicate::operator()(const FText& A, const FText& B) const
{
	return Implementation->Compare(A, B);
}

void FText::GetFormatPatternParameters(const FText& Pattern, TArray<FString>& ParameterNames)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;

	const FString& NativePatternString = Pattern.ToString();

	icu::UnicodeString ICUPatternString;
	ICUUtilities::ConvertString(NativePatternString, ICUPatternString);

	UParseError ICUParseError;
	icu::MessagePattern ICUMessagePattern(ICUPatternString, &(ICUParseError), ICUStatus);

	const int32_t PartCount = ICUMessagePattern.countParts();
	for(int32_t i = 0; i < PartCount; ++i)
	{
		UMessagePatternPartType PartType = ICUMessagePattern.getPartType(i);
		if(PartType == UMSGPAT_PART_TYPE_ARG_NAME || PartType == UMSGPAT_PART_TYPE_ARG_NUMBER)
		{
			icu::MessagePattern::Part Part = ICUMessagePattern.getPart(i);

			/** The following does a case-sensitive "TArray::AddUnique" by first checking to see if
			the parameter is in the ParameterNames list (using a case-sensitive comparison) followed
			by adding it to the ParameterNames */

			bool bIsCaseSensitiveUnique = true;
			FString CurrentArgument;
			ICUUtilities::ConvertString(ICUMessagePattern.getSubstring(Part), CurrentArgument);

			for(auto It = ParameterNames.CreateConstIterator(); It; ++It)
			{
				if(It->Equals(CurrentArgument))
				{
					bIsCaseSensitiveUnique = false;
				}
			}

			if(bIsCaseSensitiveUnique)
			{
				ParameterNames.Add( CurrentArgument );
			}
		}
	}
}

FText FText::FormatInternal(const FText& Pattern, const FFormatNamedArguments& Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	CA_SUPPRESS(6316)
	const bool bEnableErrorResults = ENABLE_TEXT_ERROR_CHECKING_RESULTS && bEnableErrorCheckingResults;

	TArray<icu::UnicodeString> ArgumentNames;
	ArgumentNames.Reserve(Arguments.Num());

	TArray<icu::Formattable> ArgumentValues;
	ArgumentValues.Reserve(Arguments.Num());

	for(auto It = Arguments.CreateConstIterator(); It; ++It)
	{
		const FString& NativeArgumentName = It.Key();
		icu::UnicodeString ICUArgumentName;
		ICUUtilities::ConvertString(NativeArgumentName, ICUArgumentName, false);
		ArgumentNames.Add( ICUArgumentName );

		const FFormatArgumentValue& ArgumentValue = It.Value();
		switch(ArgumentValue.Type)
		{
		case EFormatArgumentType::Int:
			{
				ArgumentValues.Add( icu::Formattable( static_cast<int64_t>(ArgumentValue.IntValue) ));
			}
			break;
		case EFormatArgumentType::UInt:
			{
				ArgumentValues.Add( icu::Formattable( static_cast<int64_t>(ArgumentValue.UIntValue) ));
			}
			break;
		case EFormatArgumentType::Float:
			{
				ArgumentValues.Add( icu::Formattable( ArgumentValue.FloatValue ));
			}
			break;
		case EFormatArgumentType::Double:
			{
				ArgumentValues.Add( icu::Formattable( ArgumentValue.DoubleValue ));
			}
			break;
		case EFormatArgumentType::Text:
			{
				// When doing a rebuild, all FText arguments need to be rebuilt during the Format
				if(bInRebuildText)
				{
					ArgumentValue.TextValue->Rebuild();
				}

				FString NativeStringValue = bInRebuildAsSource? ArgumentValue.TextValue->BuildSourceString() : ArgumentValue.TextValue->ToString();
				icu::UnicodeString ICUStringValue;
				ICUUtilities::ConvertString(NativeStringValue, ICUStringValue, false);
				ArgumentValues.Add( icu::Formattable( ICUStringValue ) );
			}
			break;
		}
	}

	const FString& NativePatternString = bInRebuildAsSource? Pattern.BuildSourceString() : Pattern.ToString();
	icu::UnicodeString ICUPatternString;
	ICUUtilities::ConvertString(NativePatternString, ICUPatternString);

	UErrorCode ICUStatus = U_ZERO_ERROR;
	UParseError ICUParseError;
	icu::MessageFormat ICUMessageFormatter(ICUPatternString, icu::Locale::getDefault(), ICUParseError, ICUStatus);
	if(U_FAILURE(ICUStatus))
	{
		return bEnableErrorResults ? FText::FromString(u_errorName(ICUStatus)) : FText();
	}

	icu::UnicodeString ICUResultString;
	ICUMessageFormatter.format(ArgumentNames.GetData(), ArgumentValues.GetData(), Arguments.Num(), ICUResultString, ICUStatus);
	if(U_FAILURE(ICUStatus))
	{
		return bEnableErrorResults ? FText::FromString(u_errorName(ICUStatus)) : FText();
	}

	FString NativeResultString;
	ICUUtilities::ConvertString(ICUResultString, NativeResultString);

	FText ResultText(NativeResultString);
	ResultText.History = MakeShareable(new FTextHistory_NamedFormat(Pattern, Arguments));

	return ResultText;
}

FText FText::FormatInternal(const FText& Pattern, const FFormatOrderedArguments& Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	CA_SUPPRESS(6316)
	const bool bEnableErrorResults = ENABLE_TEXT_ERROR_CHECKING_RESULTS && bEnableErrorCheckingResults;

	TArray<icu::Formattable> ArgumentValues;
	ArgumentValues.Reserve(Arguments.Num());

	for(auto It = Arguments.CreateConstIterator(); It; ++It)
	{
		const FFormatArgumentValue& ArgumentValue = *It;
		switch(ArgumentValue.Type)
		{
		case EFormatArgumentType::Int:
			{
				ArgumentValues.Add( icu::Formattable( static_cast<int64_t>(ArgumentValue.IntValue) ));
			}
			break;
		case EFormatArgumentType::UInt:
			{
				ArgumentValues.Add( icu::Formattable( static_cast<int64_t>(ArgumentValue.UIntValue) ));
			}
			break;
		case EFormatArgumentType::Float:
			{
				ArgumentValues.Add( icu::Formattable( ArgumentValue.FloatValue ));
			}
			break;
		case EFormatArgumentType::Double:
			{
				ArgumentValues.Add( icu::Formattable( ArgumentValue.DoubleValue ));
			}
			break;
		case EFormatArgumentType::Text:
			{
				// When doing a rebuild, all FText arguments need to be rebuilt during the Format
				if(bInRebuildText)
				{
					ArgumentValue.TextValue->Rebuild();
				}

				FString NativeStringValue = bInRebuildAsSource? ArgumentValue.TextValue->BuildSourceString() : ArgumentValue.TextValue->ToString();
				icu::UnicodeString ICUStringValue;
				ICUUtilities::ConvertString(NativeStringValue, ICUStringValue, false);
				ArgumentValues.Add( icu::Formattable( ICUStringValue ) );
			}
			break;
		}
	}

	const FString& NativePatternString = bInRebuildAsSource? Pattern.BuildSourceString() : Pattern.ToString();
	icu::UnicodeString ICUPatternString;
	ICUUtilities::ConvertString(NativePatternString, ICUPatternString);

	UErrorCode ICUStatus = U_ZERO_ERROR;
	UParseError ICUParseError;
	icu::MessageFormat ICUMessageFormatter(ICUPatternString, icu::Locale::getDefault(), ICUParseError, ICUStatus);
	if(U_FAILURE(ICUStatus))
	{
		return bEnableErrorResults ? FText::FromString(u_errorName(ICUStatus)) : FText();
	}

	icu::UnicodeString ICUResultString;
	ICUMessageFormatter.format(NULL, ArgumentValues.GetData(), Arguments.Num(), ICUResultString, ICUStatus);
	if(U_FAILURE(ICUStatus))
	{
		return bEnableErrorResults ? FText::FromString(u_errorName(ICUStatus)) : FText();
	}

	FString NativeResultString;
	ICUUtilities::ConvertString(ICUResultString, NativeResultString);

	FText ResultText = FText(NativeResultString);

	if (!GIsEditor)
	{
		ResultText.Flags = ResultText.Flags | ETextFlag::Transient;
	}

	ResultText.History = MakeShareable(new FTextHistory_OrderedFormat(Pattern, Arguments));
	return ResultText;
}

FText FText::FormatInternal(const FText& Pattern, const TArray< FFormatArgumentData > InArguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	CA_SUPPRESS(6316)
	const bool bEnableErrorResults = ENABLE_TEXT_ERROR_CHECKING_RESULTS && bEnableErrorCheckingResults;

	TArray<icu::UnicodeString> ArgumentNames;
	ArgumentNames.Reserve(InArguments.Num());

	TArray<icu::Formattable> ArgumentValues;
	ArgumentValues.Reserve(InArguments.Num());

	for(int32 x = 0; x < InArguments.Num(); ++x)
	{
		const FString& ArgumentName = InArguments[x].ArgumentName.ToString();
		icu::UnicodeString ICUArgumentName;
		ICUUtilities::ConvertString(ArgumentName, ICUArgumentName, false);
		ArgumentNames.Add( ICUArgumentName );

		const FFormatArgumentValue& ArgumentValue = InArguments[x].ArgumentValue;
		switch(ArgumentValue.Type)
		{
		case EFormatArgumentType::Int:
			{
				ArgumentValues.Add( icu::Formattable( static_cast<int64_t>(ArgumentValue.IntValue) ));
			}
			break;
		case EFormatArgumentType::UInt:
			{
				ArgumentValues.Add( icu::Formattable( static_cast<int64_t>(ArgumentValue.UIntValue) ));
			}
			break;
		case EFormatArgumentType::Float:
			{
				ArgumentValues.Add( icu::Formattable( ArgumentValue.FloatValue ));
			}
			break;
		case EFormatArgumentType::Double:
			{
				ArgumentValues.Add( icu::Formattable( ArgumentValue.DoubleValue ));
			}
			break;
		case EFormatArgumentType::Text:
			{
				// When doing a rebuild, all FText arguments need to be rebuilt during the Format
				if(bInRebuildText)
				{
					ArgumentValue.TextValue->Rebuild();
				}

				FString StringValue = bInRebuildAsSource? ArgumentValue.TextValue->BuildSourceString() : ArgumentValue.TextValue->ToString();
				icu::UnicodeString ICUStringValue;
				ICUUtilities::ConvertString(StringValue, ICUStringValue, false);
				ArgumentValues.Add( ICUStringValue );
			}
			break;
		}
	}

	const FString& PatternString = bInRebuildAsSource? Pattern.BuildSourceString() : Pattern.ToString();
	icu::UnicodeString ICUPatternString;
	ICUUtilities::ConvertString(PatternString, ICUPatternString, false);

	UErrorCode ICUStatus = U_ZERO_ERROR;
	UParseError ICUParseError;
	icu::MessageFormat ICUMessageFormatter(ICUPatternString, icu::Locale::getDefault(), ICUParseError, ICUStatus);
	if(U_FAILURE(ICUStatus))
	{
		return bEnableErrorResults ? FText::FromString(u_errorName(ICUStatus)) : FText();
	}

	icu::UnicodeString Result;
	ICUMessageFormatter.format(ArgumentNames.GetData(), ArgumentValues.GetData(), InArguments.Num(), Result, ICUStatus);
	if(U_FAILURE(ICUStatus))
	{
		return bEnableErrorResults ? FText::FromString(u_errorName(ICUStatus)) : FText();
	}

	FString NativeResultString;
	ICUUtilities::ConvertString(Result, NativeResultString);

	FText ResultText = FText(NativeResultString);

	if (!GIsEditor)
	{
		ResultText.Flags = ResultText.Flags | ETextFlag::Transient;
	}

	ResultText.History = MakeShareable(new FTextHistory_ArgumentDataFormat(Pattern, InArguments));
	return ResultText;
}

FText FText::Format(const FText& Pattern, const FFormatNamedArguments& Arguments)
{
	return FormatInternal(Pattern, Arguments, false, false);
}

FText FText::Format(const FText& Pattern, const FFormatOrderedArguments& Arguments)
{
	return FormatInternal(Pattern, Arguments, false, false);
}

FText FText::Format(const FText& Pattern, const TArray< struct FFormatArgumentData > InArguments)
{
	return FormatInternal(Pattern, InArguments, false, false);
}

#endif
