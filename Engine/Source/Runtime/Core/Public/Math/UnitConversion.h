// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Optional.h"
#include "Function.h"

/** Enum *must* be zero-indexed and sequential. Must be grouped by relevance and ordered by magnitude. */
/** Enum *must* match the mirrored enum that exists in CoreUObject/Classes/Object.h for the purposes of UObject reflection */
enum class EUnit
{
	/** Scalar distance/length units */
	Micrometers, Millimeters, Centimeters, Meters, Kilometers,
	Inches, Feet, Yards, Miles,
	Lightyears,

	/** Angular units */
	Degrees, Radians,

	/** Speed units */
	MetersPerSecond, KilometersPerHour, MilesPerHour,

	/** Temperature units */
	Celsius, Farenheit, Kelvin,

	/** Mass units */
	Micrograms, Milligrams, Grams, Kilograms, MetricTons,
	Ounces, Pounds, Stones,

	/** Force units */
	Newtons, PoundsForce, KilogramsForce,

	/** Frequency units */
	Hertz, Kilohertz, Megahertz, Gigahertz, RevolutionsPerMinute,

	/** Data Size units */
	Bytes, Kilobytes, Megabytes, Gigabytes, Terabytes,

	/** Luminous flux units */
	Lumens,

	/** Time units */
	Milliseconds, Seconds, Minutes, Hours, Days, Months, Years,

	/** Symbolic entry, not specifyable on meta data */
	Unspecified
};

/** Enumeration that specifies particular classes of unit */
enum class EUnitType
{
	Distance, Angle, Speed, Temperature, Mass, Force, Frequency, DataSize, LuminousFlux, Time,

	// Symbolic entry - do not use directly
	NumberOf,
};

template<typename NumericType> struct FNumericUnit;

/** Unit settings accessed globally through FUnitConversion::Settings() */
class CORE_API FUnitSettings
{
public:

	FUnitSettings();

	/** Check whether unit display is globally enabled or disabled */
	bool ShouldDisplayUnits() { return bGlobalUnitDisplay; }
	void SetShouldDisplayUnits(bool bInGlobalUnitDisplay) { bGlobalUnitDisplay = bInGlobalUnitDisplay; SettingChangedEvent.Broadcast(); }
	
	/** Get the specific valid units to display the specified type of unit in */
	const TArray<EUnit>& GetDisplayUnits(EUnitType InType);
	void SetDisplayUnits(EUnitType InType, const TArray<EUnit>& Units);
	void SetDisplayUnits(EUnitType InType, EUnit Units);

	/** Returns an event delegate that is executed when a display setting has changed. (GlobalUnitDisplay or DefaultInputUnits) */
	DECLARE_EVENT(FUnitSettings, FDisplaySettingChanged);
	FDisplaySettingChanged& OnDisplaySettingsChanged() { return SettingChangedEvent; }

private:

	/** Global toggle controlling whether we should display units or not */
	bool bGlobalUnitDisplay;

	/** Arrays of units that are valid to display on interfaces */
	TArray<EUnit> DisplayUnits[(uint8)EUnitType::NumberOf + 1];
	
	/** Holds an event delegate that is executed when a display setting has changed. */
	FDisplaySettingChanged SettingChangedEvent;
};

struct CORE_API FUnitConversion
{
	/** Get the global settings for unit conversion/display */
	static FUnitSettings& Settings();

	/** Check whether it is possible to convert a number between the two specified units */
	static bool AreUnitsCompatible(EUnit From, EUnit To)
	{
		return From == EUnit::Unspecified || To == EUnit::Unspecified || Types[(uint8)From] == Types[(uint8)To];
	}

	/** Check whether a unit is of the specified type */
	static bool IsUnitOfType(EUnit Unit, EUnitType Type)
	{
		return Unit != EUnit::Unspecified && Types[(uint8)Unit] == Type;
	}

	/** Convert the specified number from one unit to another. Does nothing if the units are incompatible. */
	template<typename T>
	static T Convert(T InValue, EUnit From, EUnit To)
	{
		if (!AreUnitsCompatible(From, To))
		{
			return InValue;
		}
		else if (From == EUnit::Unspecified || To == EUnit::Unspecified)
		{
			return InValue;
		}

		switch(Types[(uint8)From])
		{
			case EUnitType::Distance:			return InValue * DistanceUnificationFactor(From)		* (1.0 / DistanceUnificationFactor(To));
			case EUnitType::Angle:				return InValue * AngleUnificationFactor(From) 			* (1.0 / AngleUnificationFactor(To));
			case EUnitType::Speed:				return InValue * SpeedUnificationFactor(From) 			* (1.0 / SpeedUnificationFactor(To));
			case EUnitType::Mass:				return InValue * MassUnificationFactor(From) 			* (1.0 / MassUnificationFactor(To));
			case EUnitType::Force:				return InValue * ForceUnificationFactor(From) 			* (1.0 / ForceUnificationFactor(To));
			case EUnitType::Frequency:			return InValue * FrequencyUnificationFactor(From) 		* (1.0 / FrequencyUnificationFactor(To));
			case EUnitType::DataSize:			return InValue * DataSizeUnificationFactor(From) 		* (1.0 / DataSizeUnificationFactor(To));
			case EUnitType::LuminousFlux:		return InValue;
			case EUnitType::Time:				return InValue * TimeUnificationFactor(From) 			* (1.0 / TimeUnificationFactor(To));			
			// Temperature conversion is not just a simple multiplication, so needs special treatment
			case EUnitType::Temperature:
			{
				double NewValue = InValue;
				// Put it into kelvin
				switch (From)
				{
					case EUnit::Celsius:			NewValue = NewValue + 273.15;					break;
					case EUnit::Farenheit:			NewValue = (NewValue + 459.67) * 5.f/9.f;		break;
					default: 																		break;
				}
				// And out again
				switch (To)
				{
					case EUnit::Celsius:			return NewValue - 273.15;
					case EUnit::Farenheit:			return NewValue * 9.f/5.f - 459.67;
					default: 						return NewValue;
				}
			}

			default:							return InValue;
		}
	}

	/** Get the display string for the the specified unit type */
	static const TCHAR* GetUnitDisplayString(EUnit Unit);

	/** Helper function to find a unit from a string (name or display string) */
	static TOptional<EUnit> UnitFromString(const TCHAR* UnitString);

private:

	/** Find the common quantization factor for the specified distance unit. Quantizes to Meters. */
	static double DistanceUnificationFactor(EUnit From);
	/** Find the common quantization factor for the specified angular unit. Quantizes to Degrees. */
	static double AngleUnificationFactor(EUnit From);
	/** Find the common quantization factor for the specified speed unit. Quantizes to km/h. */
	static double SpeedUnificationFactor(EUnit From);
	/** Find the common quantization factor for the specified temperature unit. Quantizes to Kelvin. */
	static double TemperatureUnificationFactor(EUnit From);
	/** Find the common quantization factor for the specified mass unit. Quantizes to Grams. */
	static double MassUnificationFactor(EUnit From);
	/** Find the common quantization factor for the specified force unit. Quantizes to Newtons. */
	static double ForceUnificationFactor(EUnit From);
	/** Find the common quantization factor for the specified frequency unit. Quantizes to KHz. */
	static double FrequencyUnificationFactor(EUnit From);
	/** Find the common quantization factor for the specified data size unit. Quantizes to MB. */
	static double DataSizeUnificationFactor(EUnit From);
	/** Find the common quantization factor for the specified time unit. Quantizes to hours. */
	static double TimeUnificationFactor(EUnit From);

public:

	/** Structure used to define the factor required to get from one unit type to the next. */
	struct FQuantizationInfo
	{
		/** The unit to which this factor applies */
		EUnit Units;
		/** The factor by which to multiply to get to the next unit in this range */
		float Factor;

		/** Constructor */
		FQuantizationInfo(EUnit InUnit, float InFactor) : Units(InUnit), Factor(InFactor) {}
	};

	/** Find the quantization bounds for the specified unit, if any */
	static TOptional<const TArray<FQuantizationInfo>*> GetQuantizationBounds(EUnit Unit);

	/** Quantizes this number to the most appropriate unit for user friendly presentation (e.g. 1000m returns 1km). */
	template<typename T>
	static FNumericUnit<T> QuantizeUnitsToBestFit(T Value, EUnit Units)
	{
		auto OptionalBounds = FUnitConversion::GetQuantizationBounds(Units);
		if (!OptionalBounds.IsSet())
		{
			return FNumericUnit<T>(Value, Units);
		}

		const auto& Bounds = *OptionalBounds.GetValue();

		const int32 CurrentUnitIndex = (uint8)Units - (uint8)Bounds[0].Units;

		EUnit NewUnits = Units;
		double NewValue = Value;

		if (FMath::Abs(NewValue) > 1)
		{
			// Large number? Try larger units
			for (int32 Index = CurrentUnitIndex; Index < Bounds.Num(); ++Index)
			{
				if (Bounds[Index].Factor == 0)
				{
					break;
				}

				const auto Tmp = NewValue / Bounds[Index].Factor;

				if (FMath::Abs(Tmp) < 1)
				{
					break;
				}

				NewValue = Tmp;
				NewUnits = Bounds[Index + 1].Units;
			}
		}
		else if (NewValue != 0)
		{
			// Small number? Try smaller units
			for (int32 Index = CurrentUnitIndex - 1; Index >= 0; --Index)
			{
				NewValue *= Bounds[Index].Factor;
				NewUnits = Bounds[Index].Units;

				if (FMath::Abs(NewValue) > 1)
				{
					break;
				}
			}
		}

		return FNumericUnit<T>(NewValue, NewUnits);
	}

	/** Quantizes this number to the most appropriate unit for user friendly presentation (e.g. 1000m returns 1km), adhereing to global display settings. */
	template<typename T>
	static EUnit CalculateDisplayUnit(T Value, EUnit InUnits)
	{
		if (InUnits == EUnit::Unspecified)
		{
			return EUnit::Unspecified;
		}

		const TArray<EUnit>& DisplayUnits = Settings().GetDisplayUnits(Types[(uint8)InUnits]);
		if (DisplayUnits.Num() == 0)
		{
			return QuantizeUnitsToBestFit(Value, InUnits).Units;
		}
		else if (DisplayUnits.Num() == 1)
		{
			return DisplayUnits[0];
		}

		// If the value we were given was 0, change it to something we can actually work with
		if (Value == 0)
		{
			Value = 1;
		}

		int32 BestIndex = 0;
		for (int32 Index = 0; Index < DisplayUnits.Num() - 1; ++Index)
		{
			double This = Convert(Value, InUnits, DisplayUnits[Index]);
			double Next = Convert(Value, InUnits, DisplayUnits[Index + 1]);

			if (FMath::Abs(FMath::LogX(10.0f, This)) < FMath::Abs(FMath::LogX(10.0f, Next)))
			{
				BestIndex = Index;
			}
			else
			{
				BestIndex = Index + 1;
			}
		}

		return DisplayUnits[BestIndex];
	}

private:

	/** Structure used to match units when parsing */
	struct FParseCandidate
	{
		const TCHAR* String;
		EUnit Unit;
	};
	/** Arbitrary length array used for parsing input strings into EUnit types */
	static FParseCandidate	 	ParseCandidates[];

	/** Static array of display strings that directly map to EUnit enumerations */
	static const TCHAR* const 	DisplayStrings[];

	/** Static array of unit types that directly map to EUnit enumerations */
	static const EUnitType 		Types[];
};


/**
 * FNumericUnit is a numeric type that wraps the templated type, whilst a specified unit.
 * It handles conversion to/from related units automatically. The units are considered not to contribute to the type's state, and as such should be considered immutable once set.
 */
template<typename NumericType>
struct FNumericUnit
{
	/** The numeric (scalar) value */
	NumericType Value;
	/** The associated units for the value. Can never change once set to anything other than EUnit::Unspecified. */
	const EUnit Units;

	/** Constructors */
	FNumericUnit() : Units(EUnit::Unspecified) {}
	FNumericUnit(const NumericType& InValue, EUnit InUnits = EUnit::Unspecified) : Value(InValue), Units(InUnits) {}

	/** Copy construction/assignment from the same type */
	FNumericUnit(const FNumericUnit& Other) : Units(EUnit::Unspecified)							{	(*this) = Other;								}
	FNumericUnit& operator=(const FNumericUnit& Other)											{ 	CopyValueWithConversion(Other);	return *this; 	}

	/** Templated Copy construction/assignment from differing numeric types. Relies on implicit conversion of the two numeric types. */
	template<typename OtherType> FNumericUnit(const FNumericUnit<OtherType>& Other)				{	(*this) = Other;								}
	template<typename OtherType> FNumericUnit& operator=(const FNumericUnit<OtherType>& Other)	{	CopyValueWithConversion(Other);	return *this;	}

	/** Convert this quantity to a different unit */
	TOptional<FNumericUnit<NumericType>> ConvertTo(EUnit ToUnits) const
	{
		if (Units == EUnit::Unspecified)
		{
			return FNumericUnit(Value, ToUnits);
		}
		else if (FUnitConversion::AreUnitsCompatible(Units, ToUnits))
		{
			return FNumericUnit<NumericType>(FUnitConversion::Convert(Value, Units, ToUnits), ToUnits);
		}

		return TOptional<FNumericUnit<NumericType>>();
	}

public:

	/** Quantizes this number to the most appropriate unit for user friendly presentation (e.g. 1000m returns 1km). */
	FNumericUnit<NumericType> QuantizeUnitsToBestFit() const
	{
		return FUnitConversion::QuantizeUnitsToBestFit(Value, Units);
	}

	/** Parse a numeric unit from a string */
	static TOptional<FNumericUnit<NumericType>> TryParseString(const TCHAR* InSource)
	{
		TOptional<FNumericUnit<NumericType>> Result;
		if (!InSource || !*InSource)
		{
			return Result;
		}

		const TCHAR* NumberEnd = nullptr;
		if (!ExtractNumberBoundary(InSource, NumberEnd))
		{
			return Result;
		}

		NumericType NewValue;
		LexicalConversion::FromString(NewValue, InSource);

		// Now parse the units
		while(FChar::IsWhitespace(*NumberEnd)) ++NumberEnd;

		if (*NumberEnd == '\0')
		{
			// No units
			Result.Emplace(NewValue);
		}
		else
		{
			// If the string specifies units, they must map to something that exists for this function to succeed
			auto NewUnits = FUnitConversion::UnitFromString(NumberEnd);
			if (NewUnits)
			{
				Result.Emplace(NewValue, NewUnits.GetValue());
			}
		}

		return Result;
	}

public:
	/** Global arithmetic operators for number types. Deals with conversion from related units correctly. */
	template<typename OtherType>
	friend bool operator==(const FNumericUnit& LHS, const FNumericUnit<OtherType>& RHS)
	{
		if (LHS.Units != EUnit::Unspecified && RHS.Units != EUnit::Unspecified)
		{
			if (LHS.Units == RHS.Units)
			{
				return LHS.Value == RHS.Value;
			}
			else if (FUnitConversion::AreUnitsCompatible(LHS.Units, RHS.Units))
			{
				return LHS.Value == FUnitConversion::Convert(RHS.Value, RHS.Units, LHS.Units);
			}
			else
			{
				// Invalid conversion
				return false;
			}
		}
		else
		{
			return LHS.Value == RHS.Value;
		}
	}

	template<typename OtherType>
	friend bool operator!=(const FNumericUnit& LHS, const OtherType& RHS)
	{
		return !(LHS == RHS);
	}

private:
	/** Conversion to the numeric type disabled as coupled with implicit construction from NumericType can easily lead to loss of associated units. */
	operator const NumericType&() const;

	/** Copy another unit into this one, taking account of its units, and applying necessary conversion */
	template<typename OtherType>
	void CopyValueWithConversion(const FNumericUnit<OtherType>& Other)
	{
		if (Units != EUnit::Unspecified && Other.Units != EUnit::Unspecified)
		{
			if (Units == Other.Units)
			{
				Value = Other.Value;
			}
			else if (FUnitConversion::AreUnitsCompatible(Units, Other.Units))
			{
				Value = FUnitConversion::Convert(Other.Value, Other.Units, Units);
			}
			else
			{
				// Invalid conversion - assignment invalid
			}
		}
		else
		{
			// If our units haven't been specified, we take on the units of the rhs
			if (Units == EUnit::Unspecified)
			{
				// This is the only time we ever change units. Const_cast is 'acceptible' here since the units haven't been specified yet.
				const_cast<EUnit&>(Units) = Other.Units;
			}

			Value = Other.Value;
		}
	}

	/** Given a string, skip past whitespace, then any numeric characters. Set End pointer to the end of the last numeric character. */
	static bool ExtractNumberBoundary(const TCHAR* Start, const TCHAR*& End)
	{
		while(FChar::IsWhitespace(*Start)) ++Start;

		End = Start;
		if (*End == '-' || *End == '+')
		{
			End++;
		}

		bool bHasDot = false;
		while (FChar::IsDigit(*End) || *End == '.')
		{
			if (*End == '.')
			{
				if (bHasDot)
				{
					return false;
				}
				bHasDot = true;
			}
			++End;
		}

		return true;
	}
};


template <typename NumericType>
struct TNumericLimits<FNumericUnit<NumericType>> : public TNumericLimits<NumericType>
{ };

namespace LexicalConversion
{
	template<typename T>
	FString ToString(const FNumericUnit<T>& NumericUnit)
	{
		FString String = ToString(NumericUnit.Value);
		String += TEXT(" ");
		String += FUnitConversion::GetUnitDisplayString(NumericUnit.Units);

		return String;
	}

	template<typename T>
	FString ToSanitizedString(const FNumericUnit<T>& NumericUnit)
	{
		FString String = ToSanitizedString(NumericUnit.Value);
		String += TEXT(" ");
		String += FUnitConversion::GetUnitDisplayString(NumericUnit.Units);

		return String;
	}

	template<typename T>
	void FromString(FNumericUnit<T>& OutValue, const TCHAR* String)
	{
		auto Parsed = FNumericUnit<T>::TryParseString(String);
		if (Parsed)
		{
			OutValue = Parsed.GetValue();
		}
	}
	
	template<typename T>
	bool TryParseString(FNumericUnit<T>& OutValue, const TCHAR* String)
	{
		auto Parsed = FNumericUnit<T>::TryParseString(String);
		if (Parsed)
		{
			OutValue = Parsed.GetValue();
			return true;
		}

		return false;
	}
}