// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PropertyTag.h"

/*-----------------------------------------------------------------------------
	UByteProperty.
-----------------------------------------------------------------------------*/
void UByteProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	if(Enum && Ar.UseToResolveEnumerators())
	{
		 const int32 ResolvedIndex = Enum->ResolveEnumerator(Ar, *(uint8*)Value);
		 *(uint8*)Value = static_cast<uint8>(ResolvedIndex);
		 return;
	}

	// Serialize enum values by name unless we're not saving or loading OR for backwards compatibility
	const bool bUseBinarySerialization = (Enum == NULL) || (!Ar.IsLoading() && !Ar.IsSaving());
	if( bUseBinarySerialization )
	{
		Super::SerializeItem(Ar, Value, Defaults);
	}
	// Loading
	else if (Ar.IsLoading())
	{
		FName EnumValueName;
		Ar << EnumValueName;
		// Make sure enum is properly populated
		if( Enum->HasAnyFlags(RF_NeedLoad) )
		{
			Ar.Preload(Enum);
		}

		// There's no guarantee EnumValueName is still present in Enum, in which case Value will be set to the enum's max value.
		// On save, it will then be serialized as NAME_None.
		int32 EnumIndex = Enum->FindEnumIndex(EnumValueName);
		if (EnumIndex == INDEX_NONE)
		{
			*(uint8*)Value = Enum->GetMaxEnumValue();
		}
		else
		{
			*(uint8*)Value = Enum->GetValueByIndex(EnumIndex);
		}
	}
	// Saving
	else
	{
		FName EnumValueName;
		uint8 ByteValue = *(uint8*)Value;

		// subtract 1 because the last entry in the enum's Names array
		// is the _MAX entry
		if ( Enum->IsValidEnumValue(ByteValue) )
		{
			EnumValueName = Enum->GetNameByValue(ByteValue);
		}
		else
		{
			EnumValueName = NAME_None;
		}
		Ar << EnumValueName;
	}
}
bool UByteProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	Ar.SerializeBits( Data, Enum ? FMath::CeilLogTwo(Enum->GetMaxEnumValue()) : 8 );
	return 1;
}
void UByteProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Enum;
	if (Enum != nullptr)
	{
		Ar.Preload(Enum);
	}
}
void UByteProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UByteProperty* This = CastChecked<UByteProperty>(InThis);
	Collector.AddReferencedObject( This->Enum, This );
	Super::AddReferencedObjects( This, Collector );
}
FString UByteProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	if (Enum)
	{
		const bool bEnumClassForm = Enum->GetCppForm() == UEnum::ECppForm::EnumClass;
		const bool bNonNativeEnum = Enum->GetClass() != UEnum::StaticClass(); // cannot use RF_Native flag, because in UHT the flag is not set
		const bool bRawParam = (CPPExportFlags & CPPF_ArgumentOrReturnValue)
			&& (((PropertyFlags & CPF_ReturnParm) || !(PropertyFlags & CPF_OutParm))
				|| bEnumClassForm || bNonNativeEnum);
		const bool bConvertedCode = (CPPExportFlags & CPPF_BlueprintCppBackend) && (bEnumClassForm || bNonNativeEnum);

		FString FullyQualifiedEnumName;
		if (!Enum->CppType.IsEmpty())
		{
			FullyQualifiedEnumName = Enum->CppType;
		}
		else
		{
			// This would give the wrong result if it's a namespaced type and the CppType hasn't
			// been set, but we do this here in case existing code relies on it... somehow.
			if ((CPPExportFlags & CPPF_BlueprintCppBackend) && bNonNativeEnum)
			{
				ensure(Enum->CppType.IsEmpty());
				FullyQualifiedEnumName = ::UnicodeToCPPIdentifier(Enum->GetName(), false, TEXT("E__"));
			}
			else
			{
				FullyQualifiedEnumName = Enum->GetName();
			}
		}
		 
		if (bRawParam || bConvertedCode)
		{
			return FullyQualifiedEnumName;
		}
		else
		{
			return FString::Printf(TEXT("TEnumAsByte<%s>"), *FullyQualifiedEnumName);
		}
	}
	return Super::GetCPPType(ExtendedTypeText, CPPExportFlags);
}

template <typename OldIntType>
struct TConvertIntToEnumProperty
{
	static void Convert(FArchive& Ar, UByteProperty* Property, UEnum* Enum, void* Obj, const FPropertyTag& Tag)
	{
		OldIntType OldValue;
		Ar << OldValue;

		uint8 NewValue = OldValue;
		if (OldValue > (OldIntType)TNumericLimits<uint8>::Max() || !Enum->IsValidEnumValue(NewValue))
		{
			UE_LOG(
				LogClass,
				Warning,
				TEXT("Failed to find valid enum value '%d' for enum type '%s' when converting property '%s' during property loading - setting to '%s'"),
				OldValue,
				*Enum->GetName(),
				*Property->GetName(),
				*Enum->GetNameByValue(Enum->GetMaxEnumValue()).ToString()
				);

			NewValue = Enum->GetMaxEnumValue();
		}

		Property->SetPropertyValue_InContainer(Obj, NewValue, Tag.ArrayIndex);
	}
};

bool UByteProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	bOutAdvanceProperty = true;

	if (Tag.Type == NAME_ByteProperty  && ((Tag.EnumName == NAME_None) != (Enum == nullptr)))
	{
		// a byte property gained or lost an enum
		// attempt to convert it
		uint8 PreviousValue;
		if (Tag.EnumName == NAME_None)
		{
			// simply pretend the property still doesn't have an enum and serialize the single byte
			Ar << PreviousValue;
		}
		else
		{
			// attempt to find the old enum and get the byte value from the serialized enum name
			PreviousValue = ReadEnumAsUint8(Ar, DefaultsStruct, Tag);
		}

		// now copy the value into the object's address space
		SetPropertyValue_InContainer(Data, PreviousValue, Tag.ArrayIndex);
	}
	else if (Tag.Type == NAME_Int8Property)
	{
		if (Enum)
		{
			TConvertIntToEnumProperty<int8>::Convert(Ar, this, Enum, Data, Tag);
		}
		else
		{
			ConvertFromInt<int8>(Ar, Data, Tag);
		}
	}
	else if (Tag.Type == NAME_Int16Property)
	{
		if (Enum)
		{
			TConvertIntToEnumProperty<int16>::Convert(Ar, this, Enum, Data, Tag);
		}
		else
		{
			ConvertFromInt<int16>(Ar, Data, Tag);
		}
	}
	else if (Tag.Type == NAME_IntProperty)
	{
		if (Enum)
		{
			TConvertIntToEnumProperty<int32>::Convert(Ar, this, Enum, Data, Tag);
		}
		else
		{
			ConvertFromInt<int32>(Ar, Data, Tag);
		}
	}
	else if (Tag.Type == NAME_Int64Property)
	{
		if (Enum)
		{
			TConvertIntToEnumProperty<int64>::Convert(Ar, this, Enum, Data, Tag);
		}
		else
		{
			ConvertFromInt<int64>(Ar, Data, Tag);
		}
	}
	else if (Tag.Type == NAME_UInt16Property)
	{
		if (Enum)
		{
			TConvertIntToEnumProperty<uint16>::Convert(Ar, this, Enum, Data, Tag);
		}
		else
		{
			ConvertFromInt<uint16>(Ar, Data, Tag);
		}
	}
	else if (Tag.Type == NAME_UInt32Property)
	{
		if (Enum)
		{
			TConvertIntToEnumProperty<uint32>::Convert(Ar, this, Enum, Data, Tag);
		}
		else
		{
			ConvertFromInt<uint32>(Ar, Data, Tag);
		}
	}
	else if (Tag.Type == NAME_UInt64Property)
	{
		if (Enum)
		{
			TConvertIntToEnumProperty<uint64>::Convert(Ar, this, Enum, Data, Tag);
		}
		else
		{
			ConvertFromInt<uint64>(Ar, Data, Tag);
		}
	}
	else
	{
		bOutAdvanceProperty = false;
	}

	return bOutAdvanceProperty;
}

void UByteProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		if (Enum)
		{
			const int32 ActualValue = *(const uint8*)PropertyValue;
			const int32 MaxValue = Enum->GetMaxEnumValue();
			const int32 GoodValue = Enum->IsValidEnumValue(ActualValue) ? ActualValue : MaxValue;
			const bool bNonNativeEnum = Enum->GetClass() != UEnum::StaticClass();
			ensure(!bNonNativeEnum || Enum->CppType.IsEmpty());
			const FString FullyQualifiedEnumName = bNonNativeEnum ? ::UnicodeToCPPIdentifier(Enum->GetName(), false, TEXT("E__"))
				: (Enum->CppType.IsEmpty() ? Enum->GetName() : Enum->CppType);
			if (GoodValue == MaxValue)
			{
				// not all native enums have Max value declared
				ValueStr += FString::Printf(TEXT("(%s)(%d)"), *FullyQualifiedEnumName, ActualValue);
			}
			else
			{
				ValueStr += FString::Printf(TEXT("%s::%s"), *FullyQualifiedEnumName,
					*Enum->GetEnumName(Enum->GetIndexByValue(GoodValue)));
			}
		}
		else
		{
			Super::ExportTextItem(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope);
		}
		return;
	}

	if( Enum && (PortFlags & PPF_ConsoleVariable) == 0 )
	{
		// if the value is the max value (the autogenerated *_MAX value), export as "INVALID", unless we're exporting text for copy/paste (for copy/paste,
		// the property text value must actually match an entry in the enum's names array)
		bool bIsValid = Enum->IsValidEnumValue(*(const uint8*)PropertyValue);
		bool bIsMax = *(const uint8*)PropertyValue == Enum->GetMaxEnumValue();
		if (bIsValid && (!bIsMax || (PortFlags & PPF_Copy)))
		{
			// We do not want to export the enum text for non-display uses, localization text is very dynamic and would cause issues on import
			if (PortFlags & PPF_PropertyWindow)
			{
				ValueStr += Enum->GetEnumTextByValue(*(const uint8*)PropertyValue).ToString();
			}
			else
			{
				ValueStr += Enum->GetEnumNameStringByValue(*(const uint8*)PropertyValue);
			}
		}
		else
		{
			ValueStr += TEXT("(INVALID)");
		}
	}
	else
	{
		Super::ExportTextItem(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope);
	}
}
const TCHAR* UByteProperty::ImportText_Internal( const TCHAR* InBuffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	if( Enum && (PortFlags & PPF_ConsoleVariable) == 0 )
	{
		FString Temp;
		const TCHAR* Buffer = UPropertyHelpers::ReadToken( InBuffer, Temp, true );
		if( Buffer != NULL )
		{
			int32 EnumIndex = Enum->FindEnumIndex(*Temp);
			if (EnumIndex != INDEX_NONE)
			{
				*(uint8*)Data = Enum->GetValueByIndex(EnumIndex);
				return Buffer;
			}
		}
	}
	
	// Interpret "True" and "False" as 1 and 0. This is mostly for importing a property that was exported as a bool and is imported as a non-enum byte.
	if (!Enum)
	{
		FString Temp;
		const TCHAR* Buffer = UPropertyHelpers::ReadToken(InBuffer, Temp);
		if (Buffer)
		{
			if (Temp == TEXT("True") || Temp == *(GTrue.ToString()))
			{
				SetIntPropertyValue(Data, 1ull);
				return Buffer;
			}
			else if (Temp == TEXT("False") || Temp == *(GFalse.ToString()))
			{
				SetIntPropertyValue(Data, 0ull);
				return Buffer;
			}
		}
	}

	return Super::ImportText_Internal( InBuffer, Data, PortFlags, Parent, ErrorText );
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UByteProperty, UNumericProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(UByteProperty, Enum), TEXT("Enum"));
	}
);

