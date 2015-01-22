// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SerializationPrivatePCH.h"
#include "JsonStructDeserializerBackend.h"


/* Internal helpers
 *****************************************************************************/

namespace JsonStructDeserializerBackend
{
	/**
	 * Clears the value of the given property.
	 *
	 * @param Property The property to clear.
	 * @param Outer The property that contains the property to be cleared, if any.
	 * @param Data A pointer to the memory holding the property's data.
	 * @param ArrayIndex The index of the element to clear (if the property is an array).
	 * @return true on success, false otherwise.
	 * @see SetPropertyValue
	 */
	bool ClearPropertyValue( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex )
	{
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Outer);

		if (ArrayProperty != nullptr)
		{
			if (ArrayProperty->Inner != Property)
			{
				return false;
			}

			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->template ContainerPtrToValuePtr<void>(Data));
			ArrayIndex = ArrayHelper.AddValue();
		}

		Property->ClearValue_InContainer(Data, ArrayIndex);

		return true;
	}


	/**
	 * Sets the value of the given property.
	 *
	 * @param Property The property to set.
	 * @param Outer The property that contains the property to be set, if any.
	 * @param Data A pointer to the memory holding the property's data.
	 * @param ArrayIndex The index of the element to set (if the property is an array).
	 * @return true on success, false otherwise.
	 * @see ClearPropertyValue
	 */
	template<typename UPropertyType, typename PropertyType>
	bool SetPropertyValue( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex, const PropertyType& Value )
	{
		PropertyType* ValuePtr = nullptr;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Outer);

		if (ArrayProperty != nullptr)
		{
			if (ArrayProperty->Inner != Property)
			{
				return false;
			}

			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->template ContainerPtrToValuePtr<void>(Data));
			int32 Index = ArrayHelper.AddValue();
		
			ValuePtr = (PropertyType*)ArrayHelper.GetRawPtr(Index);
		}
		else
		{
			UPropertyType* TypedProperty = Cast<UPropertyType>(Property);

			if (TypedProperty == nullptr)
			{
				return false;
			}

			ValuePtr = TypedProperty->template ContainerPtrToValuePtr<PropertyType>(Data, ArrayIndex);
		}

		if (ValuePtr == nullptr)
		{
			return false;
		}

		*ValuePtr = Value;

		return true;
	}
}


/* IStructDeserializerBackend interface
 *****************************************************************************/

const FString& FJsonStructDeserializerBackend::GetCurrentPropertyName() const
{
	return JsonReader->GetIdentifier();
}


FString FJsonStructDeserializerBackend::GetDebugString() const
{
	return FString::Printf(TEXT("Line: %u, Ch: %u"), JsonReader->GetLineNumber(), JsonReader->GetCharacterNumber());
}


const FString& FJsonStructDeserializerBackend::GetLastErrorMessage() const
{
	return JsonReader->GetErrorMessage();
}


bool FJsonStructDeserializerBackend::GetNextToken( EStructDeserializerBackendTokens& OutToken )
{
	if (!JsonReader->ReadNext(LastNotation))
	{
		return false;
	}

	switch (LastNotation)
	{
	case EJsonNotation::ArrayEnd:
		OutToken = EStructDeserializerBackendTokens::ArrayEnd;
		break;

	case EJsonNotation::ArrayStart:
		OutToken = EStructDeserializerBackendTokens::ArrayStart;
		break;

	case EJsonNotation::Boolean:
	case EJsonNotation::Null:
	case EJsonNotation::Number:
	case EJsonNotation::String:
		{
			OutToken = EStructDeserializerBackendTokens::Property;
		}
		break;

	case EJsonNotation::Error:
		OutToken = EStructDeserializerBackendTokens::Error;
		break;

	case EJsonNotation::ObjectEnd:
		OutToken = EStructDeserializerBackendTokens::StructureEnd;
		break;

	case EJsonNotation::ObjectStart:
		OutToken = EStructDeserializerBackendTokens::StructureStart;
		break;

	default:
		OutToken = EStructDeserializerBackendTokens::None;
	}

	return true;
}


bool FJsonStructDeserializerBackend::ReadProperty( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex )
{
	using namespace JsonStructDeserializerBackend;

	switch (LastNotation)
	{
	// boolean values
	case EJsonNotation::Boolean:
		{
			bool BoolValue = JsonReader->GetValueAsBoolean();

			if (Property->GetClass() == UBoolProperty::StaticClass())
			{
				return SetPropertyValue<UBoolProperty, bool>(Property, Outer, Data, ArrayIndex, BoolValue);
			}

			UE_LOG(LogSerialization, Verbose, TEXT("Boolean field %s with value '%s' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), BoolValue ? *(GTrue.ToString()) : *(GFalse.ToString()), *Property->GetClass()->GetName(), *GetDebugString());

			return false;
		}
		break;

	// numeric values
	case EJsonNotation::Number:
		{
			double NumericValue = JsonReader->GetValueAsNumber();

			if (Property->GetClass() == UByteProperty::StaticClass())
			{
				return SetPropertyValue<UByteProperty, int8>(Property, Outer, Data, ArrayIndex, (int8)NumericValue);
			}

			if (Property->GetClass() == UDoubleProperty::StaticClass())
			{
				return SetPropertyValue<UDoubleProperty, double>(Property, Outer, Data, ArrayIndex, NumericValue);
			}
			
			if (Property->GetClass() == UFloatProperty::StaticClass())
			{
				return SetPropertyValue<UFloatProperty, float>(Property, Outer, Data, ArrayIndex, NumericValue);
			}
			
			if (Property->GetClass() == UIntProperty::StaticClass())
			{
				return SetPropertyValue<UIntProperty, int32>(Property, Outer, Data, ArrayIndex, (int32)NumericValue);
			}
			
			if (Property->GetClass() == UUInt32Property::StaticClass())
			{
				return SetPropertyValue<UUInt32Property, uint32>(Property, Outer, Data, ArrayIndex, (uint32)NumericValue);
			}
			
			if (Property->GetClass() == UInt16Property::StaticClass())
			{
				return SetPropertyValue<UInt16Property, int16>(Property, Outer, Data, ArrayIndex, (int16)NumericValue);
			}
			
			if (Property->GetClass() == UUInt16Property::StaticClass())
			{
				return SetPropertyValue<UUInt16Property, uint16>(Property, Outer, Data, ArrayIndex, (uint16)NumericValue);
			}
			
			if (Property->GetClass() == UInt64Property::StaticClass())
			{
				return SetPropertyValue<UInt64Property, int64>(Property, Outer, Data, ArrayIndex, (int64)NumericValue);
			}
			
			if (Property->GetClass() == UUInt64Property::StaticClass())
			{
				return SetPropertyValue<UUInt64Property, uint64>(Property, Outer, Data, ArrayIndex, (uint64)NumericValue);
			}
			
			if (Property->GetClass() == UInt8Property::StaticClass())
			{
				return SetPropertyValue<UInt8Property, int8>(Property, Outer, Data, ArrayIndex, (int8)NumericValue);
			}

			UE_LOG(LogSerialization, Verbose, TEXT("Numeric field %s with value '%f' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), NumericValue, *Property->GetClass()->GetName(), *GetDebugString());

			return false;
		}
		break;

	// null values
	case EJsonNotation::Null:
		return ClearPropertyValue(Property, Outer, Data, ArrayIndex);

	// strings, names & enumerations
	case EJsonNotation::String:
		{
			const FString& StringValue = JsonReader->GetValueAsString();

			if (Property->GetClass() == UStrProperty::StaticClass())
			{
				return SetPropertyValue<UStrProperty, FString>(Property, Outer, Data, ArrayIndex, StringValue);
			}
			
			if (Property->GetClass() == UNameProperty::StaticClass())
			{
				return SetPropertyValue<UNameProperty, FName>(Property, Outer, Data, ArrayIndex, *StringValue);
			}
			
			if (Property->GetClass() == UByteProperty::StaticClass())
			{
				UByteProperty* ByteProperty = Cast<UByteProperty>(Property);
				int32 Index = ByteProperty->Enum->FindEnumIndex(*StringValue);

				if (Index == INDEX_NONE)
				{
					return false;
				}

				return SetPropertyValue<UByteProperty, uint8>(Property, Outer, Data, ArrayIndex, (uint8)Index);
			}
			
			if (Property->GetClass() == UClassProperty::StaticClass())
			{
				return SetPropertyValue<UClassProperty, UClass*>(Property, Outer, Data, ArrayIndex, LoadObject<UClass>(NULL, *StringValue, NULL, LOAD_NoWarn));
			}

			UE_LOG(LogSerialization, Verbose, TEXT("String field %s with value '%s' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), *StringValue, *Property->GetClass()->GetName(), *GetDebugString());

			return false;
		}
		break;
	}

	return true;
}


void FJsonStructDeserializerBackend::SkipArray() 
{
	JsonReader->SkipArray();
}


void FJsonStructDeserializerBackend::SkipStructure()
{
	JsonReader->SkipObject();
}
