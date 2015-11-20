// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class MOVIESCENE_API MovieSceneHelpers
{
public:
	/**
	 * Gets the sections that were traversed over between the current time and the previous time
	 */
	static TArray<UMovieSceneSection*> GetTraversedSections( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime );

	/**
	 * Finds a section that exists at a given time
	 *
	 * @param Time	The time to find a section at
	 * @return The found section or null
	 */
	static UMovieSceneSection* FindSectionAtTime( const TArray<UMovieSceneSection*>& Sections, float Time );

	/**
	 * Finds the nearest section to the given time
	 *
	 * @param Time	The time to find a section at
	 * @return The found section or null
	 */
	static UMovieSceneSection* FindNearestSectionAtTime( const TArray<UMovieSceneSection*>& Sections, float Time );

	/**
	 * Get the scene component from the runtime object
	 *
	 * @param Object The object to get the scene component for
	 * @return The found scene component
	 */	
	static USceneComponent* SceneComponentFromRuntimeObject(UObject* Object);

	/*
	 * Set the runtime object movable
	 *
	 * @param Object The object to set the mobility for
	 * @param Mobility The mobility of the runtime object
	 */
	static void SetRuntimeObjectMobility(UObject* Object, EComponentMobility::Type ComponentMobility = EComponentMobility::Movable);
};

/**
 * Manages bindings to keyed properties for a track instance. 
 * Calls UFunctions to set the value on runtime objects
 */
class MOVIESCENE_API FTrackInstancePropertyBindings
{
public:
	FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath, const FName& InFunctionName = FName());

	/**
	 * Calls the setter function for a specific runtime object or if the setter function does not exist, the property is set directly
	 *
	 * @param InRuntimeObject	The runtime object whose function to call
	 * @param FunctionParams	Parameters to pass to the function
	 */
	template <typename ValueType>
	void 
	CallFunction( UObject* InRuntimeObject, void* FunctionParams )
	{
		FPropertyAndFunction PropAndFunction = RuntimeObjectToFunctionMap.FindRef(InRuntimeObject);
		if(PropAndFunction.Function)
		{
			InRuntimeObject->ProcessEvent(PropAndFunction.Function, FunctionParams);
		}
		else
		{
			ValueType* NewValue = (ValueType*)(FunctionParams);
			SetCurrentValue<ValueType>(InRuntimeObject, *NewValue);
		}
	}

	/**
	 * Rebuilds the property and function mappings for a set of runtime objects
	 *
	 * @param InRuntimeObjects	The objects to rebuild mappings for
	 */
	void UpdateBindings( const TArray<UObject*>& InRuntimeObjects );

	/**
	 * Gets the UProperty that is bound to the track instance
	 *
	 * @param Object	The Object that owns the property
	 * @return			The property on the object if it exists
	 */
	UProperty* GetProperty(const UObject* Object) const;

	/**
	 * Gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return ValueType	The current value
	 */
	template <typename ValueType>
	ValueType GetCurrentValue(const UObject* Object) const
	{
		FPropertyAndFunction PropAndFunction = RuntimeObjectToFunctionMap.FindRef(Object);

		if(PropAndFunction.PropertyAddress.Address)
		{
			const ValueType* Val = PropAndFunction.PropertyAddress.Property->ContainerPtrToValuePtr<ValueType>(PropAndFunction.PropertyAddress.Address);
			if(Val)
			{
				return *Val;
			}
		}

		return ValueType();
	}

	/**
	 * Sets the current value of a property on an object
	 *
	 * @param Object	The object to set the property on
	 * @param InValue   The value to set
	 */
	template <typename ValueType>
	void SetCurrentValue(const UObject* Object, ValueType InValue)
	{
		FPropertyAndFunction PropAndFunction = RuntimeObjectToFunctionMap.FindRef(Object);

		if(PropAndFunction.PropertyAddress.Address)
		{
			ValueType* Val = PropAndFunction.PropertyAddress.Property->ContainerPtrToValuePtr<ValueType>(PropAndFunction.PropertyAddress.Address);
			if(Val)
			{
				*Val = InValue;
			}
		}
	}

private:

	struct FPropertyAddress
	{
		UProperty* Property;
		void* Address;

		FPropertyAddress()
			: Property(nullptr)
			, Address(nullptr)
		{}
	};

	struct FPropertyAndFunction
	{
		FPropertyAddress PropertyAddress;
		UFunction* Function;

		FPropertyAndFunction()
			: PropertyAddress()
			, Function( nullptr )
		{}
	};

	FPropertyAddress FindPropertyRecursive(const UObject* Object, void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index) const;
	FPropertyAddress FindProperty(const UObject* Object, const FString& InPropertyPath) const;

private:
	/** Mapping of objects to bound functions that will be called to update data on the track */
	TMap< TWeakObjectPtr<UObject>, FPropertyAndFunction > RuntimeObjectToFunctionMap;

	/** Path to the property we are bound to */
	FString PropertyPath;

	/** Name of the function to call to set values */
	FName FunctionName;

	/** Actual name of the property we are bound to */
	FName PropertyName;

};