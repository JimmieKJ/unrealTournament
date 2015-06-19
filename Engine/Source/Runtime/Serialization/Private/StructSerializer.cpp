// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SerializationPrivatePCH.h"
#include "IStructSerializerBackend.h"
#include "StructSerializer.h"


/* Internal helpers
 *****************************************************************************/

namespace StructSerializer
{
	/**
	 * Structure for the write state stack.
	 */
	struct FWriteState
	{
		/** Holds a pointer to the property's data. */
		const void* Data;

		/** Holds a flag indicating whether the property has been processed. */
		bool HasBeenProcessed;

		/** Holds the property's meta data. */
		UProperty* Property;

		/** Holds a pointer to the UStruct describing the data. */
		UStruct* TypeInfo;
	};


	/**
	 * Gets the value from the given property.
	 *
	 * @param PropertyType The type name of the property.
	 * @param State The stack state that holds the property's data.
	 * @param Property A pointer to the property.
	 * @return A pointer to the property's value, or nullptr if it couldn't be found.
	 */
	template<typename UPropertyType, typename PropertyType>
	PropertyType* GetPropertyValue( const FWriteState& State, UProperty* Property )
	{
		PropertyType* ValuePtr = nullptr;
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(State.Property);

		if (ArrayProperty)
		{
			check(ArrayProperty->Inner == Property);

			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->template ContainerPtrToValuePtr<void>(State.Data));
			int32 Index = ArrayHelper.AddValue();
		
			ValuePtr = (PropertyType*)ArrayHelper.GetRawPtr( Index );
		}
		else
		{
			UPropertyType* TypedProperty = Cast<UPropertyType>(Property);
			check(TypedProperty != nullptr);

			ValuePtr = TypedProperty->template ContainerPtrToValuePtr<PropertyType>(State.Data);
		}

		return ValuePtr;
	}
}


/* FStructSerializer static interface
 *****************************************************************************/

void FStructSerializer::Serialize( const void* Struct, UStruct& TypeInfo, IStructSerializerBackend& Backend, const FStructSerializerPolicies& Policies )
{
	using namespace StructSerializer;

	check(Struct != nullptr);

	// initialize serialization
	TArray<FWriteState> StateStack;
	{
		FWriteState NewState;

		NewState.Data = Struct;
		NewState.Property = nullptr;
		NewState.TypeInfo = &TypeInfo;
		NewState.HasBeenProcessed = false;

		StateStack.Push(NewState);
	}

	// process state stack
	while (StateStack.Num() > 0)
	{
		FWriteState CurrentState = StateStack.Pop(/*bAllowShrinking=*/ false);

		// structures
		if ((CurrentState.Property == nullptr) || (CurrentState.TypeInfo == UStructProperty::StaticClass()))
		{
			if (!CurrentState.HasBeenProcessed)
			{
				const void* NewData = CurrentState.Data;

				// write object start
				if (CurrentState.Property == nullptr)
				{
					Backend.BeginStructure(CurrentState.TypeInfo);
				}
				else
				{
					UObject* Outer = CurrentState.Property->GetOuter();

					if ((Outer == nullptr) || (Outer->GetClass() != UArrayProperty::StaticClass()))
					{
						NewData = CurrentState.Property->ContainerPtrToValuePtr<void>(CurrentState.Data);
					}

					Backend.BeginStructure(CurrentState.Property);
				}

				CurrentState.HasBeenProcessed = true;
				StateStack.Push(CurrentState);

				// serialize fields
				if (CurrentState.Property != nullptr)
				{
					UStructProperty* StructProperty = Cast<UStructProperty>(CurrentState.Property);

					if (StructProperty != nullptr)
					{
						CurrentState.TypeInfo = StructProperty->Struct;
					}
					else
					{
						UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(CurrentState.Property);

						if (ObjectProperty != nullptr)
						{
							CurrentState.TypeInfo = ObjectProperty->PropertyClass;
						}
					}
				}

				TArray<FWriteState> NewStates;

				for (TFieldIterator<UProperty> It(CurrentState.TypeInfo, EFieldIteratorFlags::IncludeSuper); It; ++It)
				{
					FWriteState NewState;

					NewState.Data = NewData;
					NewState.Property = *It;
					NewState.TypeInfo = It->GetClass();
					NewState.HasBeenProcessed = false;

					NewStates.Add(NewState);
				}

				// push child properties on stack (in reverse order)
				for (int Index = NewStates.Num() - 1; Index >= 0; --Index)
				{
					StateStack.Push(NewStates[Index]);
				}
			}
			else
			{
				Backend.EndStructure();
			}
		}

		// dynamic arrays
		else if (CurrentState.TypeInfo == UArrayProperty::StaticClass())
		{
			if (!CurrentState.HasBeenProcessed)
			{
				Backend.BeginArray(CurrentState.Property);

				CurrentState.HasBeenProcessed = true;
				StateStack.Push(CurrentState);

				UArrayProperty* ArrayProperty = Cast<UArrayProperty>(CurrentState.Property);
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(CurrentState.Data));
				UProperty* Inner = ArrayProperty->Inner;

				// push elements on stack (in reverse order)
				for (int Index = ArrayHelper.Num() - 1; Index >= 0; --Index)
				{
					FWriteState NewState;

					NewState.Data = ArrayHelper.GetRawPtr(Index);
					NewState.Property = Inner;
					NewState.TypeInfo = Inner->GetClass();
					NewState.HasBeenProcessed = false;

					StateStack.Push(NewState);
				}
			}
			else
			{
				Backend.EndArray(CurrentState.Property);
			}
		}

		// static arrays
		else if (CurrentState.Property->ArrayDim > 1)
		{
			Backend.BeginArray(CurrentState.Property);

			for (int32 ArrayIndex = 0; ArrayIndex < CurrentState.Property->ArrayDim; ++ArrayIndex)
			{
				Backend.WriteProperty(CurrentState.Property, CurrentState.Data, CurrentState.TypeInfo, ArrayIndex);
			}

			Backend.EndArray(CurrentState.Property);
		}

		// all other properties
		else
		{
			Backend.WriteProperty(CurrentState.Property, CurrentState.Data, CurrentState.TypeInfo);
		}
	}
}
