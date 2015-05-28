// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnitConversion.h"

template<typename NumericType>
TNumericUnitTypeInterface<NumericType>::TNumericUnitTypeInterface(EUnit InUnits)
	: UnderlyingUnits(InUnits)
{}

template<typename NumericType>
FString TNumericUnitTypeInterface<NumericType>::ToString(const NumericType& Value) const
{
	using namespace LexicalConversion;

	FNumericUnit<NumericType> FinalValue(Value, UnderlyingUnits);

	if (FixedDisplayUnits.IsSet())
	{
		auto Converted = FinalValue.ConvertTo(FixedDisplayUnits.GetValue());
		if (Converted.IsSet())
		{
			return ToSanitizedString(Converted.GetValue());
		}
	}
	
	return ToSanitizedString(FinalValue);
}

template<typename NumericType>
TOptional<NumericType> TNumericUnitTypeInterface<NumericType>::FromString(const FString& InString)
{
	using namespace LexicalConversion;

	// Always parse in as a double, to allow for input of higher-order units with decimal numerals into integral types (eg, inputting 0.5km as 500m)
	FNumericUnit<double> NewValue;
	bool bEvalResult = TryParseString( NewValue, *InString ) && FUnitConversion::AreUnitsCompatible( NewValue.Units, UnderlyingUnits );
	if (bEvalResult)
	{
		// Convert the number into the correct units
		EUnit SourceUnits = NewValue.Units;
		if (SourceUnits == EUnit::Unspecified && FixedDisplayUnits.IsSet())
		{
			// Use the default supplied input units
			SourceUnits = FixedDisplayUnits.GetValue();
		}
		return FUnitConversion::Convert(NewValue.Value, SourceUnits, UnderlyingUnits);
	}
	else
	{
		float FloatValue = 0.f;
		if (FMath::Eval( *InString, FloatValue  ))
		{
			return FloatValue;
		}
	}

	return TOptional<NumericType>();
}

template<typename NumericType>
bool TNumericUnitTypeInterface<NumericType>::IsCharacterValid(TCHAR InChar) const
{
	return true;
}

template<typename NumericType>
void TNumericUnitTypeInterface<NumericType>::SetupFixedDisplay(const NumericType& InValue)
{
	EUnit DisplayUnit = FUnitConversion::CalculateDisplayUnit(InValue, UnderlyingUnits);
	if (DisplayUnit != EUnit::Unspecified)
	{
		FixedDisplayUnits = DisplayUnit;
	}
}