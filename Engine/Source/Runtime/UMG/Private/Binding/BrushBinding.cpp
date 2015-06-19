// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "WidgetBlueprintLibrary.h"
#include "BrushBinding.h"

#define LOCTEXT_NAMESPACE "UMG"

UBrushBinding::UBrushBinding()
{
}

bool UBrushBinding::IsSupportedDestination(UProperty* Property) const
{
	return IsConcreteTypeCompatibleWithReflectedType<FSlateBrush>(Property);
}

bool UBrushBinding::IsSupportedSource(UProperty* Property) const
{
	if ( IsConcreteTypeCompatibleWithReflectedType<UObject*>(Property) )
	{
		if ( UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property) )
		{
			return ObjectProperty->PropertyClass->IsChildOf(UTexture2D::StaticClass());
		}
	}

	return IsConcreteTypeCompatibleWithReflectedType<FSlateBrush>(Property);
}

FSlateBrush UBrushBinding::GetValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		if ( bConversion.Get(EConversion::None) == EConversion::None )
		{
			FSlateBrush Value;
			if ( SourcePath.GetValue<FSlateBrush>(Source, Value) )
			{
				bConversion = EConversion::None;
				return Value;
			}
		}

		if ( bConversion.Get(EConversion::Texture) == EConversion::Texture )
		{
			UObject* Value;
			if ( SourcePath.GetValue<UObject*>(Source, Value) )
			{
				if ( UTexture2D* Texture = Cast<UTexture2D>(Value) )
				{
					bConversion = EConversion::Texture;
					return GetDefault<UWidgetBlueprintLibrary>()->MakeBrushFromTexture(Texture);
				}
			}
		}
	}

	return FSlateNoResource();
}

#undef LOCTEXT_NAMESPACE
