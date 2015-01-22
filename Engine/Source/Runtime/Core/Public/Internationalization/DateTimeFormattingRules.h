// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealString.h"

class FDateTimeFormattingRules
{
	friend class FText;
public:
	FDateTimeFormattingRules()
		: AMDesignator(FString(TEXT("")))
		, DateSeparator(FString(TEXT("")))
		, TimeSeparator(FString(TEXT("")))
		, FirstDayOfWeek(0)
		, FullDateTimePattern(FString(TEXT("")))
		, LongDatePattern(FString(TEXT("")))
		, LongTimePattern(FString(TEXT("")))
		, MonthDayPattern(FString(TEXT("")))
		, PMDesignator(FString(TEXT("")))
		, RFC1123Pattern(FString(TEXT("")))
		, ShortDatePattern(FString(TEXT("")))
		, ShortTimePattern(FString(TEXT("")))
		, SortableDateTimePattern(FString(TEXT("")))
		, UniversalSortableDateTimePattern(FString(TEXT("")))
		, YearMonthPattern(FString(TEXT("")))
	{
	}

	FDateTimeFormattingRules(const TArray<FString>& InAbbreviatedDayNames, const TArray<FString>& InAbbreviatedMonthNames, const TArray<FString>& InDayNames, const TArray<FString>& InMonthNames, const TArray<FString>& InShortestDayNames, const FString InAMDesignator, const FString InPMDesignator, const FString InDateSeparator, const FString InTimeSeparator, const int InFirstDayOfWeek, const FString InFullDateTimePattern, const FString InLongDatePattern, const FString InLongTimePattern, const FString InMonthDayPattern, const FString InRFC1123Pattern, const FString InShortDatePattern, const FString InShortTimePattern, const FString InSortableDateTimePattern, const FString InUniversalSortableDateTimePattern, const FString InYearMonthPattern)
		: AbbreviatedDayNames(InAbbreviatedDayNames)
		, AbbreviatedMonthNames(InAbbreviatedMonthNames)
		, DayNames(InDayNames)
		, MonthNames(InMonthNames)
		, ShortestDayNames(InShortestDayNames)
		, AMDesignator(InAMDesignator)
		, DateSeparator(InDateSeparator)
		, TimeSeparator(InTimeSeparator)
		, FirstDayOfWeek(InFirstDayOfWeek)
		, FullDateTimePattern(InFullDateTimePattern)
		, LongDatePattern(InLongDatePattern)
		, LongTimePattern(InLongTimePattern)
		, MonthDayPattern(InMonthDayPattern)
		, PMDesignator(InPMDesignator)
		, RFC1123Pattern(InRFC1123Pattern)
		, ShortDatePattern(InShortDatePattern)
		, ShortTimePattern(InShortTimePattern)
		, SortableDateTimePattern(InSortableDateTimePattern)
		, UniversalSortableDateTimePattern(InUniversalSortableDateTimePattern)
		, YearMonthPattern(InYearMonthPattern)
	{ }

protected:
	// array of abbreviated names for the days of the week
	const TArray<FString> AbbreviatedDayNames;

	// array of abbreviated names for the months
	const TArray<FString> AbbreviatedMonthNames;

	// array of names for the days of the week
	const TArray<FString> DayNames;

	// Array of the names of the months
	const TArray<FString> MonthNames;

	// Array of the shortest abbreviations of the name of the days
	const TArray<FString> ShortestDayNames;

	// string designator for hours between 0:00:00 and 11:59:59
	const FString AMDesignator;

	// the character used to separate the date components of day, month, and year
	const FString DateSeparator;

	// the character used to separate the time components of seconds, minutes and hours
	const FString TimeSeparator;

	// The first day of the week @todo is this an index into DayNames, should the values of the arrays of DayNames and AbbreviatedDayNames be common ordered between cultures?
	const int FirstDayOfWeek;

	// Pattern to use for a long date and time
	const FString FullDateTimePattern;

	// Pattern to use for a long dates
	const FString LongDatePattern;

	// Pattern to use for a long times
	const FString LongTimePattern;

	// Pattern to use for month and day value
	const FString MonthDayPattern;

	// string designator for hours between 12:00:00 and 23:59:59 in non-24 hour display
	const FString PMDesignator;

	// Pattern to use for the Request for Comments (RFC) 1123 specification
	const FString RFC1123Pattern;

	// Pattern to use for a short dates
	const FString ShortDatePattern;

	// Pattern to use for a short times
	const FString ShortTimePattern;

	// Pattern to use for a date time which can be sorted
	const FString SortableDateTimePattern;

	// Pattern to use for a universal date time which can be sorted
	const FString UniversalSortableDateTimePattern;

	// Pattern to use for a year month.
	const FString YearMonthPattern;


	// @todo Pretty sure we'll need the genitive names of the months.

	// @todo Need static InvariantInfo? CurrentInfo?



};
	// @todo do we need a calendar class
	//Calendar
	//CalendarWeekRule
	//NativeCalendarName