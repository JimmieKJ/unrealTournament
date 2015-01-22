// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SerializationPrivatePCH.h"
#include "JsonStructSerializerBackend.h"


/* Internal helpers
 *****************************************************************************/

namespace JsonStructSerializerBackend
{
	// Writes a property value to the serialization output.
	template<typename ValueType>
	void WritePropertyValue( const TSharedRef<TJsonWriter<UCS2CHAR>> JsonWriter, UProperty* Property, const ValueType& Value )
	{
		if ((Property == nullptr) || (Property->ArrayDim > 1) || (Property->GetOuter()->GetClass() == UArrayProperty::StaticClass()))
		{
			JsonWriter->WriteValue(Value);
		}
		else
		{
			JsonWriter->WriteValue(Property->GetName(), Value);
		}
	}

	// Writes a null value to the serialization output.
	void WriteNull( const TSharedRef<TJsonWriter<UCS2CHAR>> JsonWriter, UProperty* Property )
	{
		if ((Property == nullptr) || (Property->ArrayDim > 1) || (Property->GetOuter()->GetClass() == UArrayProperty::StaticClass()))
		{
			JsonWriter->WriteNull();
		}
		else
		{
			JsonWriter->WriteNull(Property->GetName());
		}
	}
}


/* IStructSerializerBackend interface
 *****************************************************************************/

void FJsonStructSerializerBackend::BeginArray( UProperty* Property )
{
	UObject* Outer = Property->GetOuter();

	if ((Outer != nullptr) && (Outer->GetClass() == UArrayProperty::StaticClass()))
	{
		JsonWriter->WriteArrayStart();
	}
	else
	{
		JsonWriter->WriteArrayStart(Property->GetName());
	}
}


void FJsonStructSerializerBackend::BeginStructure( UProperty* Property )
{
	UObject* Outer = Property->GetOuter();

	if ((Outer != nullptr) && (Outer->GetClass() == UArrayProperty::StaticClass()))
	{
		JsonWriter->WriteObjectStart();
	}
	else
	{
		JsonWriter->WriteObjectStart(Property->GetName());
	}
}


void FJsonStructSerializerBackend::BeginStructure( UStruct* TypeInfo )
{
	JsonWriter->WriteObjectStart();
}


void FJsonStructSerializerBackend::EndArray( UProperty* Property )
{
	JsonWriter->WriteArrayEnd();
}


void FJsonStructSerializerBackend::EndStructure()
{
	JsonWriter->WriteObjectEnd();
}


void FJsonStructSerializerBackend::WriteComment( const FString& Comment )
{
	// Json does not support comments
}


void FJsonStructSerializerBackend::WriteProperty( UProperty* Property, const void* Data, UStruct* TypeInfo, int32 ArrayIndex )
{
	using namespace JsonStructSerializerBackend;

	// booleans
	if (TypeInfo == UBoolProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, Cast<UBoolProperty>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}

	// unsigned bytes & enumerations
	else if (TypeInfo == UByteProperty::StaticClass())
	{
		UByteProperty* ByteProperty = Cast<UByteProperty>(Property);

		if (ByteProperty->IsEnum())
		{
			WritePropertyValue(JsonWriter, Property, ByteProperty->Enum->GetEnumName(ByteProperty->GetPropertyValue_InContainer(Data, ArrayIndex)));
		}
		else
		{
			WritePropertyValue(JsonWriter, Property, (double)Cast<UByteProperty>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
		}			
	}

	// floating point numbers
	else if (TypeInfo == UDoubleProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, Cast<UDoubleProperty>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}
	else if (TypeInfo == UFloatProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, Cast<UFloatProperty>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}

	// signed integers
	else if (TypeInfo == UIntProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, (double)Cast<UIntProperty>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}
	else if (TypeInfo == UInt8Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, (double)Cast<UInt8Property>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}
	else if (TypeInfo == UInt16Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, (double)Cast<UInt16Property>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}
	else if (TypeInfo == UInt64Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, (double)Cast<UInt64Property>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}

	// unsigned integers
	else if (TypeInfo == UUInt16Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, (double)Cast<UUInt16Property>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}
	else if (TypeInfo == UUInt32Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, (double)Cast<UUInt32Property>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}
	else if (TypeInfo == UUInt64Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, (double)Cast<UUInt64Property>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}

	// names & strings
	else if (TypeInfo == UNameProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, Cast<UNameProperty>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex).ToString());
	}
	else if (TypeInfo == UStrProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, Cast<UStrProperty>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex));
	}

	// classes & objects
	else if (TypeInfo == UClassProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, Property, Cast<UClassProperty>(Property)->GetPropertyValue_InContainer(Data, ArrayIndex)->GetPathName());
	}
	else if (TypeInfo == UObjectProperty::StaticClass())
	{
		WriteNull(JsonWriter, Property);
	}
	
	else
	{
		UE_LOG(LogSerialization, Verbose, TEXT("FJsonStructSerializerBackend: Property %s cannot be serialized, because its type (%s) is not supported"), *Property->GetFName().ToString(), *TypeInfo->GetFName().ToString());
	}
}
