// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "LinkerPlaceholderExportObject.h"
#include "LinkerPlaceholderClass.h"

/*-----------------------------------------------------------------------------
	UObjectProperty.
-----------------------------------------------------------------------------*/

FString UObjectProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	return FString::Printf( TEXT("%s%s*"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName() );
}

FString UObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FString UObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("%s%s"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("OBJECT");
}

void UObjectProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	UObject* ObjectValue = GetObjectPropertyValue(Value);
	Ar << ObjectValue;

	UObject* CurrentValue = GetObjectPropertyValue(Value);
	if (ObjectValue != CurrentValue)
	{
		SetObjectPropertyValue(Value, ObjectValue);

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
		if (ULinkerPlaceholderExportObject* PlaceholderVal = Cast<ULinkerPlaceholderExportObject>(ObjectValue))
		{
			PlaceholderVal->AddReferencingPropertyValue(this, Value);
		}
		else if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(ObjectValue))
		{
			PlaceholderClass->AddReferencingPropertyValue(this, Value);
		}
		// NOTE: we don't remove this from CurrentValue if it is a 
		//       ULinkerPlaceholderExportObject; this is because this property 
		//       could be an array inner, and another member of that array (also 
		//       referenced through this property)... if this becomes a problem,
		//       then we could inc/decrement a ref count per referencing property 
		//
		// @TODO: if this becomes problematic (because ObjectValue doesn't match 
		//        this property's PropertyClass), then we could spawn another
		//        placeholder object (of PropertyClass's type), or use null; but
		//        we'd have to modify ULinkerPlaceholderExportObject::ReplaceReferencingObjectValues()
		//        to accommodate this (as it depends on finding itself as the set value)
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

		CheckValidObject(Value);
	}
}

void UObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	SetPropertyValue(PropertyValueAddress, Value);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectProperty, UObjectPropertyBase,
	{
	}
);

