// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SerializationPrivatePCH.h"
#include "IStructDeserializerBackend.h"
#include "StructDeserializer.h"


/* Internal helpers
 *****************************************************************************/

namespace StructDeserializer
{
	/**
	 * Structure for the read state stack.
	 */
	struct FReadState
	{
		/** Holds the property's current array index. */
		int32 ArrayIndex;

		/** Holds a pointer to the property's data. */
		void* Data;

		/** Holds the property's meta data. */
		UProperty* Property;

		/** Holds a pointer to the UStruct describing the data. */
		UStruct* TypeInfo;
	};


	/**
	 * Finds the class for the given stack state.
	 *
	 * @param State The stack state to find the class for.
	 * @return The class, if found.
	 */
	UStruct* FindClass( const FReadState& State )
	{
		UStruct* Class = nullptr;

		if (State.Property != nullptr)
		{
			UProperty* ParentProperty = State.Property;
			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(ParentProperty);

			if (ArrayProperty != nullptr)
			{
				ParentProperty = ArrayProperty->Inner;
			}

			UStructProperty* StructProperty = Cast<UStructProperty>(ParentProperty);
			UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(ParentProperty);

			if (StructProperty != nullptr)
			{
				Class = StructProperty->Struct;
			}
			else if (ObjectProperty != nullptr)
			{
				Class = ObjectProperty->PropertyClass;
			}
		}
		else
		{
			UObject* RootObject = static_cast<UObject*>(State.Data);
			Class = RootObject->GetClass();
		}

		return Class;
	}
}


/* FStructDeserializer static interface
 *****************************************************************************/

bool FStructDeserializer::Deserialize( void* OutStruct, UStruct& TypeInfo, IStructDeserializerBackend& Backend, const FStructDeserializerPolicies& Policies )
{
	using namespace StructDeserializer;

	check(OutStruct != nullptr);

	// initialize deserialization
	FReadState CurrentState;
	{
		CurrentState.ArrayIndex = 0;
		CurrentState.Data = OutStruct;
		CurrentState.Property = nullptr;
		CurrentState.TypeInfo = &TypeInfo;
	}

	TArray<FReadState> StateStack;
	EStructDeserializerBackendTokens Token;

	// process state stack
	while (Backend.GetNextToken(Token))
	{
		FString PropertyName = Backend.GetCurrentPropertyName();

		switch (Token)
		{
		case EStructDeserializerBackendTokens::ArrayEnd:
			{
				if (StateStack.Num() == 0)
				{
					UE_LOG(LogSerialization, Verbose, TEXT("Malformed input: Found ArrayEnd without matching ArrayStart"));

					return false;
				}

				CurrentState = StateStack.Pop(/*bAllowShrinking=*/ false);
			}
			break;

		case EStructDeserializerBackendTokens::ArrayStart:
			{
				FReadState NewState;

				NewState.Property = FindField<UProperty>(CurrentState.TypeInfo, *PropertyName);

				if (NewState.Property == nullptr)
				{
					if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
					{
						UE_LOG(LogSerialization, Verbose, TEXT("The array property '%s' does not exist"), *PropertyName);
					}

					if (Policies.MissingFields == EStructDeserializerErrorPolicies::Error)
					{
						return false;
					}

					Backend.SkipArray();
				}
				else
				{
					NewState.ArrayIndex = 0;
					NewState.Data = CurrentState.Data;
					NewState.TypeInfo = FindClass(NewState);

					StateStack.Push(CurrentState);
					CurrentState = NewState;
				}
			}
			break;

		case EStructDeserializerBackendTokens::Error:
			{
				return false;
			}

		case EStructDeserializerBackendTokens::Property:
			{
				UProperty* Property = nullptr;

				if (PropertyName.IsEmpty())
				{
					// handle array property
					UArrayProperty* ArrayProperty = Cast<UArrayProperty>(CurrentState.Property);

					if (ArrayProperty != nullptr)
					{
						// dynamic array property
						Property = ArrayProperty->Inner;
					}
					else
					{
						// static array property
						Property = CurrentState.Property;
					}

					if (Property == nullptr)
					{
						// error: no property for array element
						if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
						{
							UE_LOG(LogSerialization, Verbose, TEXT("Failed to serialize array element %i"), CurrentState.ArrayIndex);
						}

						return false;
					}
					else if (!Backend.ReadProperty(Property, CurrentState.Property, CurrentState.Data, CurrentState.ArrayIndex))
					{
						UE_LOG(LogSerialization, Verbose, TEXT("The array element '%s[%i]' could not be read (%s)"), *PropertyName, CurrentState.ArrayIndex, *Backend.GetDebugString());
					}

					++CurrentState.ArrayIndex;
				}
				else
				{
					// handle scalar property
					Property = FindField<UProperty>(CurrentState.TypeInfo, *PropertyName);

					if (Property == nullptr)
					{
						// error: property not found
						if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
						{
							UE_LOG(LogSerialization, Verbose, TEXT("The property '%s' does not exist"), *PropertyName);
						}

						if (Policies.MissingFields == EStructDeserializerErrorPolicies::Error)
						{
							return false;
						}
					}
					else if (!Backend.ReadProperty(Property, CurrentState.Property, CurrentState.Data, CurrentState.ArrayIndex))
					{
						UE_LOG(LogSerialization, Verbose, TEXT("The property '%s' could not be read (%s)"), *PropertyName, *Backend.GetDebugString());
					}
				}
			}
			break;

		case EStructDeserializerBackendTokens::StructureEnd:
			{
				if (StateStack.Num() == 0)
				{
					return true;
				}

				CurrentState = StateStack.Pop(/*bAllowShrinking=*/ false);
			}
			break;

		case EStructDeserializerBackendTokens::StructureStart:
			{
				FReadState NewState;

				if (PropertyName.IsEmpty())
				{
					// skip root structure
					if (CurrentState.Property == nullptr)
					{
						continue;
					}

					// handle structured array element
					UArrayProperty* ArrayProperty = Cast<UArrayProperty>(CurrentState.Property);

					if (ArrayProperty == nullptr)
					{
						UE_LOG(LogSerialization, Verbose, TEXT("Property %s is not an array"));

						return false;
					}

					FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(CurrentState.Data));
					const int32 ArrayIndex = ArrayHelper.AddValue();

					NewState.Property = ArrayProperty->Inner;
					NewState.Data = ArrayHelper.GetRawPtr(ArrayIndex);
				}
				else
				{
					// handle structured property
					NewState.Property = FindField<UProperty>(CurrentState.TypeInfo, *PropertyName);

					if (NewState.Property == nullptr)
					{
						// error: structured property not found
						if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
						{
							UE_LOG(LogSerialization, Verbose, TEXT("Structured property '%s' not found"), *PropertyName);
						}

						if (Policies.MissingFields == EStructDeserializerErrorPolicies::Error)
						{
							return false;
						}
					}
					else
					{
						NewState.Data = NewState.Property->ContainerPtrToValuePtr<void>(CurrentState.Data);
					}
				}

				if (NewState.Property != nullptr)
				{
					NewState.ArrayIndex = 0;
					NewState.TypeInfo = FindClass(NewState);

					StateStack.Push(CurrentState);
					CurrentState = NewState;
				}
				else
				{
					// error: structured property not found
					Backend.SkipStructure();

					if (Policies.MissingFields != EStructDeserializerErrorPolicies::Ignore)
					{
						UE_LOG(LogSerialization, Verbose, TEXT("Structured property '%s' not found"), *PropertyName);
					}

					if (Policies.MissingFields == EStructDeserializerErrorPolicies::Error)
					{
						return false;
					}
				}
			}

		default:

			continue;
		}
	}

	// root structure not completed
	return false;
}
