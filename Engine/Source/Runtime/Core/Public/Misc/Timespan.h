// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"


class FArchive;
class FOutputDevice;
class FString;
class UObject;


namespace ETimespan
{
	/** The number of timespan ticks per day. */
	const int64 TicksPerDay = 864000000000;

	/** The number of timespan ticks per hour. */
	const int64 TicksPerHour = 36000000000;

	/** The number of timespan ticks per microsecond. */
	const int64 TicksPerMicrosecond = 10;

	/** The number of timespan ticks per millisecond. */
	const int64 TicksPerMillisecond = 10000;

	/** The number of timespan ticks per minute. */
	const int64 TicksPerMinute = 600000000;

	/** The number of timespan ticks per second. */
	const int64 TicksPerSecond = 10000000;

	/** The number of timespan ticks per week. */
	const int64 TicksPerWeek = 6048000000000;
}


/**
 * Implements a time span.
 *
 * A time span is the difference between two dates and times. For example, the time span between
 * 12:00:00 January 1, 2000 and 18:00:00 January 2, 2000 is 30.0 hours. Time spans are measured in
 * positive or negative ticks depending on whether the difference is measured forward or backward.
 * Each tick has a resolution of 0.1 microseconds (= 100 nanoseconds).
 *
 * In conjunction with the companion class FDateTime, time spans can be used to perform date and time
 * based arithmetic, such as calculating the difference between two dates or adding a certain amount
 * of time to a given date.
 *
 * @see FDateTime
 */
struct FTimespan
{
public:

	/** Default constructor (no initialization). */
	FTimespan() { }

	/**
	 * Creates and initializes a new time interval with the specified number of ticks.
	 *
	 * @param Ticks The number of ticks.
	 */
	FTimespan(int64 InTicks)
		: Ticks(InTicks)
	{ }

	/**
	 * Creates and initializes a new time interval with the specified number of hours, minutes and seconds.
	 *
	 * @param Hours The hours component.
	 * @param Minutes The minutes component.
	 * @param Seconds The seconds component.
	 */
	FTimespan(int32 Hours, int32 Minutes, int32 Seconds)
	{
		Assign(0, Hours, Minutes, Seconds, 0, 0);
	}

	/**
	 * Creates and initializes a new time interval with the specified number of days, hours, minutes and seconds.
	 *
	 * @param Days The days component.
	 * @param Hours The hours component.
	 * @param Minutes The minutes component.
	 * @param Seconds The seconds component.
	 */
	FTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds)
	{
		Assign(Days, Hours, Minutes, Seconds, 0, 0);
	}

	/**
	 * Creates and initializes a new time interval with the specified number of days, hours, minutes and seconds.
	 *
	 * @param Days The days component.
	 * @param Hours The hours component.
	 * @param Minutes The minutes component.
	 * @param Seconds The seconds component.
	 * @param Milliseconds The milliseconds component.
	 * @param Microseconds The microseconds component (default = 0).
	 */
	FTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 Milliseconds, int32 Microseconds = 0)
	{
		Assign(Days, Hours, Minutes, Seconds, Milliseconds, Microseconds);
	}

public:

	/**
	 * Returns result of adding the given time span to this time span.
	 *
	 * @return A time span whose value is the sum of this time span and the given time span.
	 */
	FTimespan operator+(const FTimespan& Other) const
	{
		return FTimespan(Ticks + Other.Ticks);
	}

	/**
	 * Adds the given time span to this time span.
	 *
	 * @return This time span.
	 */
	FTimespan& operator+=(const FTimespan& Other)
	{
		Ticks += Other.Ticks;

		return *this;
	}

	/**
	 * Returns the inverse of this time span.
	 *
	 * The value of this time span must be greater than FTimespan::MinValue(), or else an overflow will occur.
	 *
	 * @return Inverse of this time span.
	 */
	FTimespan operator-() const
	{
		return FTimespan(-Ticks);
	}

	/**
	 * Returns the result of subtracting the given time span from this time span.
	 *
	 * @param Other The time span to compare with.
	 * @return A time span whose value is the difference of this time span and the given time span.
	 */
	FTimespan operator-(const FTimespan& Other) const
	{
		return FTimespan(Ticks - Other.Ticks);
	}

	/**
	 * Subtracts the given time span from this time span.
	 *
	 * @param Other The time span to subtract.
	 * @return This time span.
	 */
	FTimespan& operator-=(const FTimespan& Other)
	{
		Ticks -= Other.Ticks;

		return *this;
	}

	/**
	 * Returns the result of multiplying the this time span with the given scalar.
	 *
	 * @param Scalar The scalar to multiply with.
	 * @return A time span whose value is the product of this time span and the given scalar.
	 */
	FTimespan operator*(float Scalar) const
	{
		return FTimespan((int64)(Ticks * Scalar));
	}

	/**
	 * Multiplies this time span with the given scalar.
	 *
	 * @param Scalar The scalar to multiply with.
	 * @return This time span.
	 */
	FTimespan& operator*=(float Scalar)
	{
		Ticks = (int64)(Ticks * Scalar);

		return *this;
	}

	/**
	 * Compares this time span with the given time span for equality.
	 *
	 * @param Other The time span to compare with.
	 * @return true if the time spans are equal, false otherwise.
	 */
	bool operator==(const FTimespan& Other) const
	{
		return (Ticks == Other.Ticks);
	}

	/**
	 * Compares this time span with the given time span for inequality.
	 *
	 * @param Other The time span to compare with.
	 * @return true if the time spans are not equal, false otherwise.
	 */
	bool operator!=(const FTimespan& Other) const
	{
		return (Ticks != Other.Ticks);
	}

	/**
	 * Checks whether this time span is greater than the given time span.
	 *
	 * @param Other The time span to compare with.
	 * @return true if this time span is greater, false otherwise.
	 */
	bool operator>(const FTimespan& Other) const
	{
		return (Ticks > Other.Ticks);
	}

	/**
	 * Checks whether this time span is greater than or equal to the given time span.
	 *
	 * @param Other The time span to compare with.
	 * @return true if this time span is greater or equal, false otherwise.
	 */
	bool operator>=(const FTimespan& Other) const
	{
		return (Ticks >= Other.Ticks);
	}

	/**
	 * Checks whether this time span is less than the given time span.
	 *
	 * @param Other The time span to compare with.
	 * @return true if this time span is less, false otherwise.
	 */
	bool operator<(const FTimespan& Other) const
	{
		return (Ticks < Other.Ticks);
	}

	/**
	 * Checks whether this time span is less than or equal to the given time span.
	 *
	 * @param Other The time span to compare with.
	 * @return true if this time span is less or equal, false otherwise.
	 */
	bool operator<=(const FTimespan& Other) const
	{
		return (Ticks <= Other.Ticks);
	}

public:

	/**
	 * Exports this time span value to a string.
	 *
	 * @param ValueStr Will hold the string value.
	 * @param DefaultValue The default value.
	 * @param Parent Not used.
	 * @param PortFlags Not used.
	 * @param ExportRootScope Not used.
	 * @return true on success, false otherwise.
	 * @see ImportTextItem
	 */
	CORE_API bool ExportTextItem(FString& ValueStr, FTimespan const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;

	/**
	 * Gets the days component of this time span.
	 *
	 * @return Days component.
	 */
	int32 GetDays() const
	{
		return (int32)(Ticks / ETimespan::TicksPerDay);
	}

	/**
	 * Returns a time span with the absolute value of this time span.
	 *
	 * This method may overflow the timespan if its value is equal to MinValue.
	 *
	 * @return Duration of this time span.
	 * @see MinValue
	 */
	FTimespan GetDuration()
	{
		return FTimespan(Ticks >= 0 ? Ticks : -Ticks);
	}

	/**
	 * Gets the hours component of this time span.
	 *
	 * @return Hours component.
	 * @see GetTotalHours
	 */
	int32 GetHours() const
	{
		return (int32)((Ticks / ETimespan::TicksPerHour) % 24);
	}

	/**
	 * Gets the microseconds component of this time span.
	 *
	 * @return Microseconds component.
	 * @see GetTotalMicroseconds
	 */
	int32 GetMicroseconds() const
	{
		return (int32)((Ticks / ETimespan::TicksPerMicrosecond) % 1000);
	}

	/**
	 * Gets the milliseconds component of this time span.
	 *
	 * @return Milliseconds component.
	 * @see GetTotalMilliseconds
	 */
	int32 GetMilliseconds() const
	{
		return (int32)((Ticks / ETimespan::TicksPerMillisecond) % 1000);
	}

	/**
	 * Gets the minutes component of this time span.
	 *
	 * @return Minutes component.
	 * @see GetTotalMinutes
	 */
	int32 GetMinutes() const
	{
		return (int32)((Ticks / ETimespan::TicksPerMinute) % 60);
	}

	/**
	 * Gets the seconds component of this time span.
	 *
	 * @return Seconds component.
	 * @see GetTotalSeconds
	 */
	int32 GetSeconds() const
	{
		return (int32)((Ticks / ETimespan::TicksPerSecond) % 60);
	}

	/**
	 * Gets the number of ticks represented by this time span.
	 *
	 * @return Number of ticks.
	 */
	int64 GetTicks() const
	{
		return Ticks;
	}

	/**
	 * Gets the total number of days represented by this time span.
	 *
	 * @return Number of days.
	 * @see GetDays
	 */
	double GetTotalDays() const
	{
		return ((double)Ticks / ETimespan::TicksPerDay);
	}

	/**
	 * Gets the total number of hours represented by this time span.
	 *
	 * @return Number of hours.
	 * @see GetHours
	 */
	double GetTotalHours() const
	{
		return ((double)Ticks / ETimespan::TicksPerHour);
	}

	/**
	 * Gets the total number of microseconds represented by this time span.
	 *
	 * @return Number of microseconds.
	 * @see GetMicroseconds
	 */
	double GetTotalMicroseconds() const
	{
		return ((double)Ticks / ETimespan::TicksPerMicrosecond);
	}

	/**
	 * Gets the total number of milliseconds represented by this time span.
	 *
	 * @return Number of milliseconds.
	 * @see GetMilliseconds
	 */
	double GetTotalMilliseconds() const
	{
		return ((double)Ticks / ETimespan::TicksPerMillisecond);
	}

	/**
	 * Gets the total number of minutes represented by this time span.
	 *
	 * @return Number of minutes.
	 * @see GetMinutes
	 */
	double GetTotalMinutes() const
	{
		return ((double)Ticks / ETimespan::TicksPerMinute);
	}

	/**
	 * Gets the total number of seconds represented by this time span.
	 *
	 * @return Number of seconds.
	 * @see GetSeconds
	 */
	double GetTotalSeconds() const
	{
		return ((double)Ticks / ETimespan::TicksPerSecond);
	}

	/**
	 * Imports a time span value from a text buffer.
	 *
	 * @param Buffer The text buffer to import from.
	 * @param PortFlags Not used.
	 * @param Parent Not used.
	 * @param ErrorText The output device for error logging.
	 * @return true on success, false otherwise.
	 * @see ExportTextItem
	 */
	CORE_API bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);


	/**
	 * Serializes this time span from or into the specified archive.
	 *
	 * @param Ar The archive to serialize from or into.
	 * @return true on success, false otherwise.
	 */
	CORE_API bool Serialize(FArchive& Ar);

	/**
	 * Returns the string representation of this time span using a default format.
	 *
	 * The returned string has the following format:
	 *		[-][d.]hh:mm:ss.fff
	 *
	 * @return String representation.
	 * @see Parse
	 */
	CORE_API FString ToString() const;

	/**
	 * Converts this time span to its string representation.
	 *
	 * The following formatting codes are available:
	 *		%n - prints the minus sign (for negative time spans only)
	 *		%N - prints the minus or plus sign (always)
	 *		%d - prints the time span's days part
	 *		%h - prints the time span's hours part (0..23)
	 *		%m - prints the time span's minutes part (0..59)
	 *		%s - prints the time span's seconds part (0..59)
	 *		%f - prints the time span's milliseconds part (0..999)
	 *		%D - prints the total number of days (without minus sign)
	 *		%H - prints the total number of hours (without minus sign)
	 *		%M - prints the total number of minutes (without minus sign)
	 *		%S - prints the total number of seconds (without minus sign)
	 *		%F - prints the total number of milliseconds (without minus sign)
	 *
	 * @param Format - The format of the returned string.
	 * @return String representation.
	 * @see Parse
	 */
	CORE_API FString ToString(const TCHAR* Format) const;

public:

	/**
	 * Creates a time span that represents the specified number of days.
	 *
	 * @param Days The number of days.
	 * @return Time span.
	 * @see FromHours, FromMicroseconds, FromMilliseconds, FromMinutes, FromSeconds
	 */
	static CORE_API FTimespan FromDays(double Days);

	/**
	 * Creates a time span that represents the specified number of hours.
	 *
	 * @param Hours The number of hours.
	 * @return Time span.
	 * @see FromDays, FromMicroseconds, FromMilliseconds, FromMinutes, FromSeconds
	 */
	static CORE_API FTimespan FromHours(double Hours);

	/**
	 * Creates a time span that represents the specified number of microseconds.
	 *
	 * @param Microseconds The number of microseconds.
	 * @return Time span.
	 * @see FromDays, FromHours, FromMinutes, FromSeconds, FromMilliseconds
	 */
	static CORE_API FTimespan FromMicroseconds(double Microseconds);

	/**
	 * Creates a time span that represents the specified number of milliseconds.
	 *
	 * @param Milliseconds The number of milliseconds.
	 * @return Time span.
	 * @see FromDays, FromHours, FromMicroseconds, FromMinutes, FromSeconds
	 */
	static CORE_API FTimespan FromMilliseconds(double Milliseconds);

	/**
	 * Creates a time span that represents the specified number of minutes.
	 *
	 * @param Minutes The number of minutes.
	 * @return Time span.
	 * @see FromDays, FromHours, FromMicroseconds, FromMilliseconds, FromSeconds
	 */
	static CORE_API FTimespan FromMinutes(double Minutes);

	/**
	 * Creates a time span that represents the specified number of seconds.
	 *
	 * @param Seconds The number of seconds.
	 * @return Time span.
	 * @see FromDays, FromHours, FromMicroseconds, FromMilliseconds, FromMinutes
	 */
	static CORE_API FTimespan FromSeconds(double Seconds);

	/**
	 * Returns the maximum time span value.
	 *
	 * The maximum time span value is slightly more than 10,675,199 days.
	 *
	 * @return Maximum time span.
	 * @see MinValue,Zero
	 */
	static FTimespan MaxValue()
	{
		return FTimespan(9223372036854775807);
	}

	/**
	 * Returns the minimum time span value.
	 *
	 * The minimum time span value is slightly less than -10,675,199 days.
	 *
	 * @return Minimum time span.
	 * @see MaxValue, ZeroValue
	 */
	static FTimespan MinValue()
	{
		return FTimespan(-9223372036854775807 - 1);
	}

	/**
	 * Converts a string to a time span.
	 *
	 * Currently, the string must be in the format written by FTimespan.ToString().
	 * Other formats are not supported at this time.
	 *
	 * @param TimespanString The string to convert.
	 * @param OutTimespan Will contain the parsed time span.
	 * @return true if the string was converted successfully, false otherwise.
	 * @see ToString
	 */
	static CORE_API bool Parse(const FString& TimespanString, FTimespan& OutTimespan);

	/**
	 * Returns the zero time span value.
	 *
	 * The zero time span value can be used in comparison operations with other time spans.
	 *
	 * @return Zero time span.
	 * @see MaxValue, MinValue
	 */
	static FTimespan Zero()
	{
		return FTimespan(0);
	}

public:

	friend class UObject;

	/**
	 * Serializes the given time span from or into the specified archive.
	 *
	 * @param Ar The archive to serialize from or into.
	 * @param Timespan The time span value to serialize.
	 * @return The archive.
	 *
	 * @todo gmp: Figure out better include order in Core.h so this can be inlined.
	 */
	friend CORE_API FArchive& operator<<(FArchive& Ar, FTimespan& Timespan);

	/**
	 * Gets the hash for the specified time span.
	 *
	 * @param Timespan The timespan to get the hash for.
	 * @return Hash value.
	 */
	friend uint32 GetTypeHash(const FTimespan& Timespan);

protected:

	/**
	 * Assigns the specified components to this time span.
	 *
	 * @param Days The days component.
	 * @param Hours The hours component.
	 * @param Minutes The minutes component.
	 * @param Seconds The seconds component.
	 * @param Milliseconds The milliseconds component.
	 * @param Microseconds The microseconds component.
	 */
	void CORE_API Assign(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 Milliseconds, int32 Microseconds);

private:

	/** Holds the time span in 100 nanoseconds resolution. */
	int64 Ticks;
};


/**
 * Pre-multiplies a time span with the given scalar.
 *
 * @param Scalar The scalar to pre-multiply with.
 * @param Timespan The time span to multiply.
 */
inline FTimespan operator*(float Scalar, const FTimespan& Timespan)
{
	return Timespan.operator*(Scalar);
}
