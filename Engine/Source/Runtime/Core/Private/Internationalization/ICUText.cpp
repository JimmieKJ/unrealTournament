// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU
#include "Text.h"
#include "TextHistory.h"
#include "ICUCulture.h"

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include <unicode/coll.h>
#include <unicode/sortkey.h>
#include <unicode/numfmt.h>
#include <unicode/msgfmt.h>
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

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

	int64 UNIXTimestamp = DateTime.ToUnixTimestamp();
	UDate ICUDate = static_cast<double>(UNIXTimestamp) * U_MILLIS_PER_SECOND;

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DateFormat> ICUDateFormat( Culture->Implementation->GetDateFormatter(DateStyle, TimeZone) );
	icu::UnicodeString FormattedString;
	ICUDateFormat->format(ICUDate, FormattedString);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ResultText = FText::CreateChronologicalText(MoveTemp(NativeString));
	ResultText.History = MakeShareable(new FTextHistory_AsDate(DateTime, DateStyle, TimeZone, TargetCulture));

	return ResultText;
}

FText FText::AsTime(const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FCulturePtr Culture = TargetCulture.IsValid() ? TargetCulture : I18N.GetCurrentCulture();

	int64 UNIXTimestamp = DateTime.ToUnixTimestamp();
	UDate ICUDate = static_cast<double>(UNIXTimestamp) * U_MILLIS_PER_SECOND;

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DateFormat> ICUDateFormat( Culture->Implementation->GetTimeFormatter(TimeStyle, TimeZone) );
	icu::UnicodeString FormattedString;
	ICUDateFormat->format(ICUDate, FormattedString);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ResultText = FText::CreateChronologicalText(MoveTemp(NativeString));
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

	int64 UNIXTimestamp = DateTime.ToUnixTimestamp();
	UDate ICUDate = static_cast<double>(UNIXTimestamp) * U_MILLIS_PER_SECOND;

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DateFormat> ICUDateFormat( Culture->Implementation->GetDateTimeFormatter(DateStyle, TimeStyle, TimeZone) );
	icu::UnicodeString FormattedString;
	ICUDateFormat->format(ICUDate, FormattedString);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ResultText = FText::CreateChronologicalText(MoveTemp(NativeString));
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

#endif
