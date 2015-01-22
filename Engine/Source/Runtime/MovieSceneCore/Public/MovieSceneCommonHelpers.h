// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class MOVIESCENECORE_API MovieSceneHelpers
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

};

/**
 * Manages bindings to keyed properties for a track instance. 
 * Calls UFunctions to set the value on runtime objects
 */
class MOVIESCENECORE_API FTrackInstancePropertyBindings
{
public:
	FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath );

	/**
	 * Calls the setter function for a specific runtime object
	 *
	 * @param InRuntimeObject	The runtime object whose function to call
	 * @param FunctionParams	Parameters to pass to the function
	 */
	void CallFunction( UObject* InRuntimeObject, void* FunctionParams );

	/**
	 * Rebuilds the property and function mappings for a set of runtime objects
	 *
	 * @param InRuntimeObjects	The objects to rebuild mappings for
	 */
	void UpdateBindings( const TArray<UObject*>& InRuntimeObjects );

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

	FPropertyAddress FindPropertyRecursive(UObject* Object, void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index) const;
	FPropertyAddress FindProperty(UObject* Object, const FString& InPropertyPath) const;

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