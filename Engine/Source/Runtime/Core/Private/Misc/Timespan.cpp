// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "PropertyPortFlags.h"

/* FTimespan interface
 *****************************************************************************/

bool FTimespan::ExportTextItem(FString& ValueStr, FTimespan const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FTimespan(0x%016X)"), Ticks);
		return true;
	}

	ValueStr += ToString(TEXT("%N%d.%h:%m:%s.%f"));

	return true;
}


bool FTimespan::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	// @todo gmp: implement FTimespan::ImportTextItem
	return false;
}


bool FTimespan::Serialize(FArchive& Ar)
{
	Ar << *this;

	return true;
}


FString FTimespan::ToString() const
{
	if (GetDays() == 0)
	{
		return ToString(TEXT("%n%h:%m:%s.%f"));
	}

	return ToString(TEXT("%n%d.%h:%m:%s.%f"));
}


FString FTimespan::ToString(const TCHAR* Format) const
{
	FString Result;

	while (*Format != TCHAR('\0'))
	{
		if ((*Format == TCHAR('%')) && (*++Format != TCHAR('\0')))
		{
			switch (*Format)
			{
			case TCHAR('n'): if (Ticks < 0) Result += TCHAR('-'); break;
			case TCHAR('N'): Result += (Ticks < 0) ? TCHAR('-') : TCHAR('+'); break;
			case TCHAR('d'): Result += FString::Printf(TEXT("%i"), FMath::Abs(GetDays())); break;
			case TCHAR('h'): Result += FString::Printf(TEXT("%02i"), FMath::Abs(GetHours())); break;
			case TCHAR('m'): Result += FString::Printf(TEXT("%02i"), FMath::Abs(GetMinutes())); break;
			case TCHAR('s'): Result += FString::Printf(TEXT("%02i"), FMath::Abs(GetSeconds())); break;
			case TCHAR('f'): Result += FString::Printf(TEXT("%03i"), FMath::Abs(GetMilliseconds())); break;
			case TCHAR('D'): Result += FString::Printf(TEXT("%f"), FMath::Abs(GetTotalDays())); break;
			case TCHAR('H'): Result += FString::Printf(TEXT("%f"), FMath::Abs(GetTotalHours())); break;
			case TCHAR('M'): Result += FString::Printf(TEXT("%f"), FMath::Abs(GetTotalMinutes())); break;
			case TCHAR('S'): Result += FString::Printf(TEXT("%f"), FMath::Abs(GetTotalSeconds())); break;
			case TCHAR('F'): Result += FString::Printf(TEXT("%f"), FMath::Abs(GetTotalMilliseconds())); break;

			default:

				Result += *Format;
			}
		}
		else
		{
			Result += *Format;
		}

		++Format;
	}

	return Result;
}


/* FTimespan static interface
 *****************************************************************************/

FTimespan FTimespan::FromDays(double Days)
{
	check((Days >= MinValue().GetTotalDays()) && (Days <= MaxValue().GetTotalDays()));

	return FTimespan(Days * ETimespan::TicksPerDay);
}


FTimespan FTimespan::FromHours(double Hours)
{
	check((Hours >= MinValue().GetTotalHours()) && (Hours <= MaxValue().GetTotalHours()));

	return FTimespan(Hours * ETimespan::TicksPerHour);
}


FTimespan FTimespan::FromMicroseconds(double Microseconds)
{
	check((Microseconds >= MinValue().GetTotalMicroseconds()) && (Microseconds <= MaxValue().GetTotalMicroseconds()));

	return FTimespan(Microseconds * ETimespan::TicksPerMicrosecond);
}


FTimespan FTimespan::FromMilliseconds(double Milliseconds)
{
	check((Milliseconds >= MinValue().GetTotalMilliseconds()) && (Milliseconds <= MaxValue().GetTotalMilliseconds()));

	return FTimespan(Milliseconds * ETimespan::TicksPerMillisecond);
}


FTimespan FTimespan::FromMinutes(double Minutes)
{
	check((Minutes >= MinValue().GetTotalMinutes()) && (Minutes <= MaxValue().GetTotalMinutes()));

	return FTimespan(Minutes * ETimespan::TicksPerMinute);
}


FTimespan FTimespan::FromSeconds(double Seconds)
{
	check((Seconds >= MinValue().GetTotalSeconds()) && (Seconds <= MaxValue().GetTotalSeconds()));

	return FTimespan(Seconds * ETimespan::TicksPerSecond);
}


bool FTimespan::Parse(const FString& TimespanString, FTimespan& OutTimespan)
{
	// @todo gmp: implement stricter FTimespan parsing; this implementation is too forgiving.
	FString TokenString = TimespanString.Replace(TEXT("."), TEXT(":"));

	bool Negative = TokenString.StartsWith(TEXT("-"));
	TokenString.ReplaceInline(TEXT("-"), TEXT(":"), ESearchCase::CaseSensitive);

	TArray<FString> Tokens;
	TokenString.ParseIntoArray(Tokens, TEXT(":"), true);

	if (Tokens.Num() == 4)
	{
		Tokens.Insert(TEXT("0"), 0);
	}

	if (Tokens.Num() != 5)
	{
		return false;
	}

	OutTimespan.Assign(FCString::Atoi(*Tokens[0]), FCString::Atoi(*Tokens[1]), FCString::Atoi(*Tokens[2]), FCString::Atoi(*Tokens[3]), FCString::Atoi(*Tokens[4]), 0);

	if (Negative)
	{
		OutTimespan.Ticks *= -1;
	}

	return true;
}


/* FTimespan friend functions
 *****************************************************************************/

FArchive& operator<<(FArchive& Ar, FTimespan& Timespan)
{
	return Ar << Timespan.Ticks;
}

uint32 GetTypeHash(const FTimespan& Timespan)
{
	return GetTypeHash(Timespan.Ticks);
}

/* FTimespan implementation
 *****************************************************************************/

void FTimespan::Assign(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 Milliseconds, int32 Microseconds)
{
	int64 TotalTicks = ETimespan::TicksPerMicrosecond * (1000 * (1000 * (60 * 60 * 24 * (int64)Days + 60 * 60 * (int64)Hours + 60 * (int64)Minutes + (int64)Seconds) + (int64)Milliseconds) + (int64)Microseconds);
	check((TotalTicks >= MinValue().GetTicks()) && (TotalTicks <= MaxValue().GetTicks()));

	Ticks = TotalTicks;
}
