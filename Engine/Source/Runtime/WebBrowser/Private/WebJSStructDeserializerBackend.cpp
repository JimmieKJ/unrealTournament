// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "WebJSStructDeserializerBackend.h"

#if WITH_CEF3
/* Internal helpers
 *****************************************************************************/
namespace {

	template<typename ValueType, typename ContainerType, typename KeyType>
	ValueType GetNumeric(CefRefPtr<ContainerType> Container, KeyType Key)
	{
		switch(Container->GetType(Key))
		{
			case VTYPE_BOOL:
				return static_cast<ValueType>(Container->GetBool(Key));
			case VTYPE_INT:
				return static_cast<ValueType>(Container->GetInt(Key));
			case VTYPE_DOUBLE:
				return static_cast<ValueType>(Container->GetDouble(Key));
			case VTYPE_STRING:
			case VTYPE_DICTIONARY:
			case VTYPE_LIST:
			case VTYPE_NULL:
			case VTYPE_BINARY:
			default:
				return static_cast<ValueType>(0);
		}
	}

	template<typename ContainerType, typename KeyType>
	void AssignTokenFromContainer(ContainerType Container, KeyType Key,  EStructDeserializerBackendTokens& OutToken, FString& PropertyName, TSharedPtr<ICefContainerWalker>& Retval)
	{
		switch (Container->GetType(Key))
		{
			case VTYPE_NULL:
			case VTYPE_BOOL:
			case VTYPE_INT:
			case VTYPE_DOUBLE:
			case VTYPE_STRING:
				OutToken = EStructDeserializerBackendTokens::Property;
				break;
			case VTYPE_DICTIONARY:
			{
				CefRefPtr<CefDictionaryValue> Dictionary = Container->GetDictionary(Key);
				if (Dictionary->GetType("$type") == VTYPE_STRING )
				{
					OutToken = EStructDeserializerBackendTokens::Property;
				}
				else
				{
					TSharedPtr<ICefContainerWalker> NewWalker(new FCefDictionaryValueWalker(Retval, Dictionary));
					Retval = NewWalker->GetNextToken(OutToken, PropertyName);
				}
				break;
			}
			case VTYPE_LIST:
			{
				TSharedPtr<ICefContainerWalker> NewWalker(new FCefListValueWalker(Retval, Container->GetList(Key)));
				Retval = NewWalker->GetNextToken(OutToken, PropertyName);
				break;
			}
			case VTYPE_BINARY:
			case VTYPE_INVALID:
			default:
				OutToken = EStructDeserializerBackendTokens::Error;
				break;
		}
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

			if (TypedProperty == nullptr || ArrayIndex >= TypedProperty->ArrayDim)
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


	template<typename PropertyType, typename ContainerType, typename KeyType>
	bool ReadNumericProperty(UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex, CefRefPtr<ContainerType> Container, KeyType Key )
	{
		typedef typename PropertyType::TCppType TCppType;
		if (Property->IsA<PropertyType>())
		{
			return SetPropertyValue<PropertyType, TCppType>(Property, Outer, Data, ArrayIndex, GetNumeric<TCppType>(Container, Key));
		}
		else
		{
			return false;
		}
	}

	template<typename ContainerType, typename KeyType>
	bool ReadBoolProperty(UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex, CefRefPtr<ContainerType> Container, KeyType Key )
	{
		if (Property->IsA<UBoolProperty>())
		{
			return SetPropertyValue<UBoolProperty, bool>(Property, Outer, Data, ArrayIndex, GetNumeric<int>(Container, Key)!=0);
		}
		return false;
		
	}

	template<typename ContainerType, typename KeyType>
	bool ReadJSFunctionProperty(TSharedPtr<FWebJSScripting> Scripting, UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex, CefRefPtr<ContainerType> Container, KeyType Key )
	{
		if (Container->GetType(Key) != VTYPE_DICTIONARY || !Property->IsA<UStructProperty>())
		{
			return false;
		}
		CefRefPtr<CefDictionaryValue> Dictionary = Container->GetDictionary(Key);
		UStructProperty* StructProperty = Cast<UStructProperty>(Property);

		if ( StructProperty->Struct != FWebJSFunction::StaticStruct())
		{
			return false;
		}

		FGuid CallbackID;
		if (!FGuid::Parse(FString(Dictionary->GetString("$id").ToWString().c_str()), CallbackID))
		{
			// Invalid GUID
			return false;
		}
		FWebJSFunction CallbackObject(Scripting, CallbackID);
		return SetPropertyValue<UStructProperty, FWebJSFunction>(Property, Outer, Data, ArrayIndex, CallbackObject);
	}

	template<typename ContainerType, typename KeyType>
	bool ReadStringProperty(UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex, CefRefPtr<ContainerType> Container, KeyType Key )
	{
		if (Container->GetType(Key) == VTYPE_STRING)
		{
			FString StringValue = Container->GetString(Key).ToWString().c_str();

			if (Property->IsA<UStrProperty>())
			{
				return SetPropertyValue<UStrProperty, FString>(Property, Outer, Data, ArrayIndex, StringValue);
			}
			else if (Property->IsA<UNameProperty>())
			{
				return SetPropertyValue<UNameProperty, FName>(Property, Outer, Data, ArrayIndex, *StringValue);
			}
			else if (Property->IsA<UTextProperty>())
			{
				return SetPropertyValue<UTextProperty, FText>(Property, Outer, Data, ArrayIndex, FText::FromString(StringValue));
			}
			else if (Property->IsA<UByteProperty>())
			{
				UByteProperty* ByteProperty = Cast<UByteProperty>(Property);
				int32 Index = ByteProperty->Enum->FindEnumIndex(*StringValue);

				if (Index == INDEX_NONE)
				{
					return false;
				}

				return SetPropertyValue<UByteProperty, uint8>(Property, Outer, Data, ArrayIndex, (uint8)Index);
			}
		}

		return false;
	}

	template<typename ContainerType, typename KeyType>
	bool ReadProperty(TSharedPtr<FWebJSScripting> Scripting, UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex, CefRefPtr<ContainerType> Container, KeyType Key )
	{
		return ReadBoolProperty(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadStringProperty(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UByteProperty>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UInt8Property>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UInt16Property>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UIntProperty>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UInt64Property>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UUInt16Property>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UUInt32Property>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UUInt64Property>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UFloatProperty>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadNumericProperty<UDoubleProperty>(Property, Outer, Data, ArrayIndex, Container, Key)
			|| ReadJSFunctionProperty(Scripting, Property, Outer, Data, ArrayIndex, Container, Key);
	}
}

TSharedPtr<ICefContainerWalker> FCefListValueWalker::GetNextToken(EStructDeserializerBackendTokens& OutToken, FString& PropertyName)
{
	TSharedPtr<ICefContainerWalker> Retval = SharedThis(this);
	Index++;
	if (Index == -1)
	{
		OutToken = EStructDeserializerBackendTokens::ArrayStart;
	}
	else if ( Index < List->GetSize() )
	{
		AssignTokenFromContainer(List, Index, OutToken, PropertyName, Retval);
		PropertyName = FString();
	}
	else
	{
		OutToken = EStructDeserializerBackendTokens::ArrayEnd;
		Retval = Parent;
	}
	return Retval;
}

bool FCefListValueWalker::ReadProperty(TSharedPtr<FWebJSScripting> Scripting, UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex)
{
	return ::ReadProperty(Scripting, Property, Outer, Data, ArrayIndex, List, Index);
}

TSharedPtr<ICefContainerWalker> FCefDictionaryValueWalker::GetNextToken(EStructDeserializerBackendTokens& OutToken, FString& PropertyName)
{
	TSharedPtr<ICefContainerWalker> Retval = SharedThis(this);
	Index++;
	if (Index == -1)
	{
		OutToken = EStructDeserializerBackendTokens::StructureStart;
	}
	else if ( Index < Keys.size() )
	{
		AssignTokenFromContainer(Dictionary, Keys[Index], OutToken, PropertyName, Retval);
		PropertyName = Keys[Index].ToWString().c_str();
	}
	else
	{
		OutToken = EStructDeserializerBackendTokens::StructureEnd;
		Retval = Parent;
	}
	return Retval;
}

bool FCefDictionaryValueWalker::ReadProperty(TSharedPtr<FWebJSScripting> Scripting, UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex)
{
	return ::ReadProperty(Scripting, Property, Outer, Data, ArrayIndex, Dictionary, Keys[Index]);
}


/* IStructDeserializerBackend interface
 *****************************************************************************/



const FString& FWebJSStructDeserializerBackend::GetCurrentPropertyName() const
{
	return CurrentPropertyName;
}


FString FWebJSStructDeserializerBackend::GetDebugString() const
{
	return CurrentPropertyName;
}


const FString& FWebJSStructDeserializerBackend::GetLastErrorMessage() const
{
	return CurrentPropertyName;
}


bool FWebJSStructDeserializerBackend::GetNextToken( EStructDeserializerBackendTokens& OutToken )
{
	if (Walker.IsValid())
	{
		Walker = Walker->GetNextToken(OutToken, CurrentPropertyName);
		return true;
	}
	else
	{
		return false;
	}
}


bool FWebJSStructDeserializerBackend::ReadProperty( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex )
{
	return Walker->ReadProperty(Scripting, Property, Outer, Data, ArrayIndex);
}


void FWebJSStructDeserializerBackend::SkipArray() 
{
	EStructDeserializerBackendTokens Token;
	int32 depth = 1;
	while (GetNextToken(Token) && depth > 0)
	{
		switch (Token)
		{
		case EStructDeserializerBackendTokens::ArrayEnd:
			depth --;
			break;
		case EStructDeserializerBackendTokens::ArrayStart:
			depth ++;
			break;
		default:
			break;
		}
	}
}

void FWebJSStructDeserializerBackend::SkipStructure()
{
	EStructDeserializerBackendTokens Token;
	int32 depth = 1;
	while (GetNextToken(Token) && depth > 0)
	{
		switch (Token)
		{
		case EStructDeserializerBackendTokens::StructureEnd:
			depth --;
			break;
		case EStructDeserializerBackendTokens::StructureStart:
			depth ++;
			break;
		default:
			break;
		}
	}
}

#endif