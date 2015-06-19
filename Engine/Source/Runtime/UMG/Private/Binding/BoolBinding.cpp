// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "BoolBinding.h"

#define LOCTEXT_NAMESPACE "UMG"

UBoolBinding::UBoolBinding()
{
}

bool UBoolBinding::IsSupportedSource(UProperty* Property) const
{
	return IsConcreteTypeCompatibleWithReflectedType<bool>(Property);
}

bool UBoolBinding::IsSupportedDestination(UProperty* Property) const
{
	return IsConcreteTypeCompatibleWithReflectedType<bool>(Property);
}

bool UBoolBinding::GetValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		bool Value = false;
		if ( SourcePath.GetValue<bool>(Source, Value) )
		{
			return Value;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
