// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "VisibilityBinding.h"

#define LOCTEXT_NAMESPACE "UMG"

UVisibilityBinding::UVisibilityBinding()
{
}

bool UVisibilityBinding::IsSupportedSource(UProperty* Property) const
{
	return IsSupportedDestination(Property);
}

bool UVisibilityBinding::IsSupportedDestination(UProperty* Property) const
{
	static const FName VisibilityEnum(TEXT("ESlateVisibility"));

	if ( UByteProperty* ByteProperty = Cast<UByteProperty>(Property) )
	{
		if ( ByteProperty->IsEnum() )
		{
			return ByteProperty->Enum->GetFName() == VisibilityEnum;
		}
	}

	return false;
}

ESlateVisibility UVisibilityBinding::GetValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		uint8 Value = 0;
		if ( SourcePath.GetValue<uint8>(Source, Value) )
		{
			return static_cast<ESlateVisibility>( Value );
		}
	}

	return ESlateVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE
