// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "UnitConversion.h"

FUnitConversion::FParseCandidate FUnitConversion::ParseCandidates[] = {
	
	{ TEXT("Micrometers"),			EUnit::Micrometers },			{ TEXT("um"),		EUnit::Micrometers }, 			{ TEXT("\u00B5m"),	EUnit::Micrometers },
	{ TEXT("Millimeters"),			EUnit::Millimeters },			{ TEXT("mm"),		EUnit::Millimeters },
	{ TEXT("Centimeters"),			EUnit::Centimeters },			{ TEXT("cm"),		EUnit::Centimeters },
	{ TEXT("Meters"),				EUnit::Meters },				{ TEXT("m"),		EUnit::Meters },
	{ TEXT("Kilometers"),			EUnit::Kilometers },			{ TEXT("km"),		EUnit::Kilometers },
	{ TEXT("Inches"),				EUnit::Inches },				{ TEXT("in"),		EUnit::Inches },
	{ TEXT("Feet"),					EUnit::Feet },					{ TEXT("ft"),		EUnit::Feet },
	{ TEXT("Yards"),				EUnit::Yards },					{ TEXT("yd"),		EUnit::Yards },
	{ TEXT("Miles"),				EUnit::Miles },					{ TEXT("mi"),		EUnit::Miles },
	{ TEXT("Lightyears"),			EUnit::Lightyears },			{ TEXT("ly"),		EUnit::Lightyears },

	{ TEXT("Degrees"),				EUnit::Degrees },				{ TEXT("deg"),		EUnit::Degrees },				{ TEXT("\u00B0"),	EUnit::Degrees },
	{ TEXT("Radians"),				EUnit::Radians },				{ TEXT("rad"),		EUnit::Radians },
		
	{ TEXT("MetersPerSecond"),		EUnit::MetersPerSecond },		{ TEXT("m/s"),		EUnit::MetersPerSecond },
	{ TEXT("KilometersPerHour"),	EUnit::KilometersPerHour },		{ TEXT("km/h"),		EUnit::KilometersPerHour },		{ TEXT("kmph"),		EUnit::KilometersPerHour },
	{ TEXT("MilesPerHour"),			EUnit::MilesPerHour },			{ TEXT("mi/h"),		EUnit::MilesPerHour },			{ TEXT("mph"),		EUnit::MilesPerHour },
		
	{ TEXT("Celsius"),				EUnit::Celsius },				{ TEXT("C"),		EUnit::Celsius },				{ TEXT("degC"),		EUnit::Celsius },			{ TEXT("\u00B0C"),		EUnit::Celsius },
	{ TEXT("Farenheit"),			EUnit::Farenheit },				{ TEXT("F"),		EUnit::Farenheit },				{ TEXT("degF"),		EUnit::Farenheit },			{ TEXT("\u00B0F"),		EUnit::Farenheit },
	{ TEXT("Kelvin"),				EUnit::Kelvin },				{ TEXT("K"),		EUnit::Kelvin },
					
	{ TEXT("Micrograms"),			EUnit::Micrograms },			{ TEXT("ug"),		EUnit::Micrograms }, 			{ TEXT("\u00B5g"),		EUnit::Micrograms },
	{ TEXT("Milligrams"),			EUnit::Milligrams },			{ TEXT("mg"),		EUnit::Milligrams },
	{ TEXT("Grams"),				EUnit::Grams },					{ TEXT("g"),		EUnit::Grams },
	{ TEXT("Kilograms"),			EUnit::Kilograms },				{ TEXT("kg"),		EUnit::Kilograms },
	{ TEXT("MetricTons"),			EUnit::MetricTons },			{ TEXT("t"),		EUnit::MetricTons },
	{ TEXT("Ounces"),				EUnit::Ounces },				{ TEXT("oz"),		EUnit::Ounces },
	{ TEXT("Pounds"),				EUnit::Pounds },				{ TEXT("lb"),		EUnit::Pounds },
	{ TEXT("Stones"),				EUnit::Stones },				{ TEXT("st"),		EUnit::Stones },

	{ TEXT("Newtons"),				EUnit::Newtons },				{ TEXT("N"),		EUnit::Newtons },
	{ TEXT("PoundsForce"),			EUnit::PoundsForce },			{ TEXT("lbf"),		EUnit::PoundsForce },
	{ TEXT("KilogramsForce"),		EUnit::KilogramsForce },		{ TEXT("kgf"),		EUnit::KilogramsForce },

	{ TEXT("Hertz"),				EUnit::Hertz },					{ TEXT("Hz"),		EUnit::Hertz },
	{ TEXT("Kilohertz"),			EUnit::Kilohertz },				{ TEXT("KHz"),		EUnit::Kilohertz },
	{ TEXT("Megahertz"),			EUnit::Megahertz },				{ TEXT("MHz"),		EUnit::Megahertz },
	{ TEXT("Gigahertz"),			EUnit::Gigahertz },				{ TEXT("GHz"),		EUnit::Gigahertz },
	{ TEXT("RevolutionsPerMinute"),	EUnit::RevolutionsPerMinute },	{ TEXT("rpm"),		EUnit::RevolutionsPerMinute },

	{ TEXT("Bytes"),				EUnit::Bytes },					{ TEXT("B"),		EUnit::Bytes },
	{ TEXT("Kilobytes"),			EUnit::Kilobytes },				{ TEXT("KB"),		EUnit::Kilobytes },
	{ TEXT("Megabytes"),			EUnit::Megabytes },				{ TEXT("MB"),		EUnit::Megabytes },
	{ TEXT("Gigabytes"),			EUnit::Gigabytes },				{ TEXT("GB"),		EUnit::Gigabytes },
	{ TEXT("Terabytes"),			EUnit::Terabytes },				{ TEXT("TB"),		EUnit::Terabytes },

	{ TEXT("Lumens"),				EUnit::Lumens },				{ TEXT("lm"),		EUnit::Lumens },

	{ TEXT("Milliseconds"),			EUnit::Milliseconds },			{ TEXT("ms"),		EUnit::Milliseconds },
	{ TEXT("Seconds"),				EUnit::Seconds },				{ TEXT("s"),		EUnit::Seconds },
	{ TEXT("Minutes"),				EUnit::Minutes },				{ TEXT("min"),		EUnit::Minutes },
	{ TEXT("Hours"),				EUnit::Hours },					{ TEXT("hrs"),		EUnit::Hours },
	{ TEXT("Days"),					EUnit::Days },					{ TEXT("dy"),		EUnit::Days },
	{ TEXT("Months"),				EUnit::Months },				{ TEXT("mth"),		EUnit::Months },
	{ TEXT("Years"),				EUnit::Years },					{ TEXT("yr"),		EUnit::Years },
};

const TCHAR* const FUnitConversion::DisplayStrings[] = {
	TEXT("\u00B5m"),			TEXT("mm"),					TEXT("cm"),					TEXT("m"),					TEXT("km"),
	TEXT("in"),					TEXT("ft"),					TEXT("yd"),					TEXT("mi"),
	TEXT("ly"),

	TEXT("\u00B0"), TEXT("rad"),

	TEXT("m/s"), TEXT("km/h"), TEXT("mi/h"),

	TEXT("\u00B0C"), TEXT("\u00B0F"), TEXT("K"),

	TEXT("\u00B5g"), TEXT("mg"), TEXT("g"), TEXT("kg"), TEXT("t"),
	TEXT("oz"), TEXT("lb"), TEXT("st"),

	TEXT("N"), TEXT("lbf"), TEXT("kgf"),

	TEXT("Hz"), TEXT("KHz"), TEXT("MHz"), TEXT("GHz"), TEXT("rpm"),

	TEXT("B"), TEXT("KB"), TEXT("MB"), TEXT("GB"), TEXT("TB"),

	TEXT("lm"),

	TEXT("ms"), TEXT("s"), TEXT("min"), TEXT("hr"), TEXT("dy"), TEXT("mth"), TEXT("yr"),
};

const EUnitType FUnitConversion::Types[] = {
	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,
	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,
	EUnitType::Distance,

	EUnitType::Angle,		EUnitType::Angle,

	EUnitType::Speed,		EUnitType::Speed, 		EUnitType::Speed,

	EUnitType::Temperature,	EUnitType::Temperature,	EUnitType::Temperature,

	EUnitType::Mass,		EUnitType::Mass,		EUnitType::Mass,		EUnitType::Mass,		EUnitType::Mass,
	EUnitType::Mass,		EUnitType::Mass,		EUnitType::Mass,

	EUnitType::Force,		EUnitType::Force,		EUnitType::Force,

	EUnitType::Frequency,	EUnitType::Frequency,	EUnitType::Frequency,	EUnitType::Frequency,	EUnitType::Frequency,

	EUnitType::DataSize,	EUnitType::DataSize,	EUnitType::DataSize,	EUnitType::DataSize,	EUnitType::DataSize,

	EUnitType::LuminousFlux,

	EUnitType::Time,		EUnitType::Time,		EUnitType::Time,		EUnitType::Time,		EUnitType::Time,		EUnitType::Time,		EUnitType::Time,
};

FUnitSettings::FUnitSettings()
	: bGlobalUnitDisplay(true)
{
	DisplayUnits[(uint8)EUnitType::Distance].Add(EUnit::Centimeters);
	DisplayUnits[(uint8)EUnitType::Angle].Add(EUnit::Degrees);
	DisplayUnits[(uint8)EUnitType::Speed].Add(EUnit::MetersPerSecond);
	DisplayUnits[(uint8)EUnitType::Temperature].Add(EUnit::Celsius);
	DisplayUnits[(uint8)EUnitType::Mass].Add(EUnit::Kilograms);
	DisplayUnits[(uint8)EUnitType::Force].Add(EUnit::Newtons);
	DisplayUnits[(uint8)EUnitType::Frequency].Add(EUnit::Hertz);
	DisplayUnits[(uint8)EUnitType::DataSize].Add(EUnit::Megabytes);
	DisplayUnits[(uint8)EUnitType::LuminousFlux].Add(EUnit::Lumens);
	DisplayUnits[(uint8)EUnitType::Time].Add(EUnit::Seconds);
}

const TArray<EUnit>& FUnitSettings::GetDisplayUnits(EUnitType InType)
{
	return DisplayUnits[(uint8)InType];
}

void FUnitSettings::SetDisplayUnits(EUnitType InType, EUnit Unit)
{
	if (InType != EUnitType::NumberOf)
	{
		DisplayUnits[(uint8)InType].Empty();
		if (FUnitConversion::IsUnitOfType(Unit, InType))
		{
			DisplayUnits[(uint8)InType].Add(Unit);
		}

		SettingChangedEvent.Broadcast();
	}	
}

void FUnitSettings::SetDisplayUnits(EUnitType InType, const TArray<EUnit>& Units)
{
	if (InType != EUnitType::NumberOf)
	{
		DisplayUnits[(uint8)InType].Reset();
		for (EUnit Unit : Units)
		{
			if (FUnitConversion::IsUnitOfType(Unit, InType))
			{
				DisplayUnits[(uint8)InType].Add(Unit);
			}
		}

		SettingChangedEvent.Broadcast();
	}
}

/** Get the global settings for unit conversion/display */
FUnitSettings& FUnitConversion::Settings()
{
	static TUniquePtr<FUnitSettings> Settings(new FUnitSettings);
	return *Settings;
}

/** Get the unit abbreviation the specified unit type */
const TCHAR* FUnitConversion::GetUnitDisplayString(EUnit Unit)
{
	static_assert(ARRAY_COUNT(FUnitConversion::Types) == (uint8)EUnit::Unspecified, "Type array does not match size of unit enum");
	static_assert(ARRAY_COUNT(FUnitConversion::DisplayStrings) == (uint8)EUnit::Unspecified, "Display String array does not match size of unit enum");
	
	if (Unit != EUnit::Unspecified)
	{
		return DisplayStrings[(uint8)Unit];
	}
	return nullptr;
}

/** Helper function to find a unit from a string (name or abbreviation) */
TOptional<EUnit> FUnitConversion::UnitFromString(const TCHAR* UnitString)
{
	if (!UnitString || *UnitString == '\0')
	{
		return TOptional<EUnit>();
	}

	for (int32 Index = 0; Index < ARRAY_COUNT(ParseCandidates); ++Index)
	{
		if (FCString::Stricmp(UnitString, ParseCandidates[Index].String) == 0)
		{
			return ParseCandidates[Index].Unit;
		}
	}

	return TOptional<EUnit>();
}

double FUnitConversion::DistanceUnificationFactor(EUnit From)
{
	// Convert to meters
	double Factor = 1;
	switch (From)
	{
		case EUnit::Micrometers:		return 0.000001;
		case EUnit::Millimeters:		return 0.001;
		case EUnit::Centimeters:		return 0.01;
		case EUnit::Kilometers:			return 1000;

		case EUnit::Lightyears:			return 9.4605284e15;

		case EUnit::Miles:				Factor *= 1760;				// fallthrough
		case EUnit::Yards:				Factor *= 3;				// fallthrough
		case EUnit::Feet:				Factor *= 12;				// fallthrough
		case EUnit::Inches:				Factor /= 39.3700787;		// fallthrough
		default: 						return Factor;				// return
	}
}

double FUnitConversion::AngleUnificationFactor(EUnit From)
{
	// Convert to degrees
	switch (From)
	{
		case EUnit::Radians:			return (180 / PI);
		default: 						return 1;
	}
}

double FUnitConversion::SpeedUnificationFactor(EUnit From)
{
	// Convert to km/h
	switch (From)
	{
		case EUnit::MetersPerSecond:	return 3.6;
		case EUnit::MilesPerHour:		return DistanceUnificationFactor(EUnit::Miles) / 1000;
		default: 						return 1;
	}
}

double FUnitConversion::MassUnificationFactor(EUnit From)
{
	double Factor = 1;
	// Convert to grams
	switch (From)
	{
		case EUnit::Micrograms:			return 0.000001;
		case EUnit::Milligrams:			return 0.001;
		case EUnit::Kilograms:			return 1000;
		case EUnit::MetricTons:			return 1000000;

		case EUnit::Stones:				Factor *= 14;		// fallthrough
		case EUnit::Pounds:				Factor *= 16;		// fallthrough
		case EUnit::Ounces:				Factor *= 28.3495;	// fallthrough
		default: 						return Factor;		// return
	}
}

double FUnitConversion::ForceUnificationFactor(EUnit From)
{
	// Convert to Newtons
	switch (From)
	{
		case EUnit::PoundsForce:		return 4.44822162;
		case EUnit::KilogramsForce:		return 9.80665;
		default: 						return 1;
	}
}

double FUnitConversion::FrequencyUnificationFactor(EUnit From)
{
	// Convert to KHz
	switch (From)
	{
		case EUnit::Hertz:				return 0.001;
		case EUnit::Megahertz:			return 1000;
		case EUnit::Gigahertz:			return 1000000;

		case EUnit::RevolutionsPerMinute:	return 0.001/60;

		default: 						return 1;
	}
}

double FUnitConversion::DataSizeUnificationFactor(EUnit From)
{
	// Convert to MB
	switch (From)
	{
		case EUnit::Bytes:				return 1.0/(1024*1024);
		case EUnit::Kilobytes:			return 1.0/1024;
		case EUnit::Gigabytes:			return 1024;
		case EUnit::Terabytes:			return 1024*1024;

		default: 						return 1;
	}
}

double FUnitConversion::TimeUnificationFactor(EUnit From)
{
	// Convert to hours
	double Factor = 1;
	switch (From)
	{
		case EUnit::Months:				return (365.242 * 24) / 12;

		case EUnit::Years:				Factor *= 365.242;	// fallthrough
		case EUnit::Days:				Factor *= 24;		// fallthrough
										return Factor;

		case EUnit::Milliseconds:		Factor /= 1000;		// fallthrough
		case EUnit::Seconds:			Factor /= 60;		// fallthrough
		case EUnit::Minutes:			Factor /= 60;		// fallthrough
										return Factor;

		default: 						return 1;
	}
}

TOptional<const TArray<FUnitConversion::FQuantizationInfo>*> FUnitConversion::GetQuantizationBounds(EUnit Unit)
{
	struct FStaticBounds
	{
		TArray<FQuantizationInfo> MetricDistance;
		TArray<FQuantizationInfo> ImperialDistance;

		TArray<FQuantizationInfo> MetricMass;
		TArray<FQuantizationInfo> ImperialMass;

		TArray<FQuantizationInfo> Frequency;
		TArray<FQuantizationInfo> DataSize;

		TArray<FQuantizationInfo> Time;

		FStaticBounds()
		{
			MetricDistance.Emplace(EUnit::Micrometers,	1000);
			MetricDistance.Emplace(EUnit::Millimeters,	10);
			MetricDistance.Emplace(EUnit::Centimeters,	100);
			MetricDistance.Emplace(EUnit::Meters,			1000);
			MetricDistance.Emplace(EUnit::Kilometers,		0);

			ImperialDistance.Emplace(EUnit::Inches,	12);
			ImperialDistance.Emplace(EUnit::Feet,		3);
			ImperialDistance.Emplace(EUnit::Yards,	1760);
			ImperialDistance.Emplace(EUnit::Miles,	0);

			MetricMass.Emplace(EUnit::Micrograms,	1000);
			MetricMass.Emplace(EUnit::Milligrams,	1000);
			MetricMass.Emplace(EUnit::Grams,		1000);
			MetricMass.Emplace(EUnit::Kilograms,	1000);
			MetricMass.Emplace(EUnit::MetricTons,	0);

			ImperialMass.Emplace(EUnit::Ounces,	16);
			ImperialMass.Emplace(EUnit::Pounds,	14);
			ImperialMass.Emplace(EUnit::Stones,	0);

			Frequency.Emplace(EUnit::Hertz,		1000);
			Frequency.Emplace(EUnit::Kilohertz,	1000);
			Frequency.Emplace(EUnit::Megahertz,	1000);
			Frequency.Emplace(EUnit::Gigahertz,	0);

			DataSize.Emplace(EUnit::Bytes,		1000);
			DataSize.Emplace(EUnit::Kilobytes,	1000);
			DataSize.Emplace(EUnit::Megabytes,	1000);
			DataSize.Emplace(EUnit::Gigabytes,	1000);
			DataSize.Emplace(EUnit::Terabytes,	0);

			Time.Emplace(EUnit::Milliseconds,		1000);
			Time.Emplace(EUnit::Seconds,			60);
			Time.Emplace(EUnit::Minutes,			60);
			Time.Emplace(EUnit::Hours,			24);
			Time.Emplace(EUnit::Days,				365.242f / 12);
			Time.Emplace(EUnit::Months,			12);
			Time.Emplace(EUnit::Years,			0);
		}
	};

	static FStaticBounds Bounds;

	switch (Unit)
	{
	case EUnit::Micrometers: case EUnit::Millimeters: case EUnit::Centimeters: case EUnit::Meters: case EUnit::Kilometers:
		return &Bounds.MetricDistance;

	case EUnit::Inches: case EUnit::Feet: case EUnit::Yards: case EUnit::Miles:
		return &Bounds.ImperialDistance;

	case EUnit::Micrograms: case EUnit::Milligrams: case EUnit::Grams: case EUnit::Kilograms: case EUnit::MetricTons:
		return &Bounds.MetricMass;

	case EUnit::Ounces: case EUnit::Pounds: case EUnit::Stones:
		return &Bounds.ImperialMass;

	case EUnit::Hertz: case EUnit::Kilohertz: case EUnit::Megahertz: case EUnit::Gigahertz: case EUnit::RevolutionsPerMinute:
		return &Bounds.Frequency;

	case EUnit::Bytes: case EUnit::Kilobytes: case EUnit::Megabytes: case EUnit::Gigabytes: case EUnit::Terabytes:
		return &Bounds.DataSize;

	case EUnit::Milliseconds: case EUnit::Seconds: case EUnit::Minutes: case EUnit::Hours: case EUnit::Days: case EUnit::Months: case EUnit::Years:
		return &Bounds.Time;

	default:
		return TOptional<const TArray<FQuantizationInfo>*>();
	}
}