// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyParams.h"

class MOVIESCENE_API MovieSceneHelpers
{
public:

	/**
	 * Gets the sections that were traversed over between the current time and the previous time, including overlapping sections
	 */
	static TArray<UMovieSceneSection*> GetAllTraversedSections( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime );

	/**
	 * Gets the sections that were traversed over between the current time and the previous time, excluding overlapping sections (highest wins)
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

	/*
	 * Fix up consecutive sections so that there are no gaps
	 * 
	 * @param Sections All the sections
	 * @param Section The section that was modified 
	 * @param bDelete Was this a deletion?
	 */
	static void FixupConsecutiveSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete);

	/*
 	 * Sort consecutive sections so that they are in order based on start time
 	 */
	static void SortConsecutiveSections(TArray<UMovieSceneSection*>& Sections);

	/**
	 * Get the scene component from the runtime object
	 *
	 * @param Object The object to get the scene component for
	 * @return The found scene component
	 */	
	static USceneComponent* SceneComponentFromRuntimeObject(UObject* Object);

	/**
	 * Get the active camera component from the actor 
	 *
	 * @param InActor The actor to look for the camera component on
	 * @return The active camera component
	 */
	static UCameraComponent* CameraComponentFromActor(const AActor* InActor);

	/**
	 * Find and return camera component from the runtime object
	 *
	 * @param Object The object to get the camera component for
	 * @return The found camera component
	 */	
	static UCameraComponent* CameraComponentFromRuntimeObject(UObject* RuntimeObject);

	/**
	 * Set the runtime object movable
	 *
	 * @param Object The object to set the mobility for
	 * @param Mobility The mobility of the runtime object
	 */
	static void SetRuntimeObjectMobility(UObject* Object, EComponentMobility::Type ComponentMobility = EComponentMobility::Movable);

	/*
	 * Set the key interpolation
	 *
	 * @param InCurve The curve that contains the key handle to set
	 * @param InKeyHandle The key handle to set
	 * @param InInterpolation The interpolation to set
	 */
	static void SetKeyInterpolation(FRichCurve& InCurve, FKeyHandle InKeyHandle, EMovieSceneKeyInterpolation InKeyInterpolation);
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
	 * @param InRuntimeObject The runtime object whose function to call
	 * @param PropertyValue The new value to assign to the property (if bound to property)
	 * @param FunctionParams Parameters to pass to the function (if bound to function)
	 */
	template <typename ValueType>
	void 
	CallFunction( UObject* InRuntimeObject, void* PropertyValue, void* FunctionParams )
	{
		FPropertyAndFunction PropAndFunction = RuntimeObjectToFunctionMap.FindRef(InRuntimeObject);
		if(PropAndFunction.Function)
		{
			InRuntimeObject->ProcessEvent(PropAndFunction.Function, FunctionParams);
		}
		else
		{
			ValueType* NewValue = (ValueType*)(PropertyValue);
			SetCurrentValue<ValueType>(InRuntimeObject, *NewValue);
		}
	}

	/** For backwards compatibility. */
	template <typename ValueType>
	void 
	CallFunction( UObject* InRuntimeObject, void* PropertyValue )
	{
		CallFunction<ValueType>(InRuntimeObject, PropertyValue, PropertyValue);
	}

	/**
	 * Rebuilds the property and function mappings for a set of runtime objects
	 *
	 * @param InRuntimeObjects	The objects to rebuild mappings for
	 */
	void UpdateBindings( const TArray<TWeakObjectPtr<UObject>>& InRuntimeObjects );

	/**
	 * Rebuilds the property and function mappings for a single runtime object
	 *
	 * @param InRuntimeObject	The object to rebuild mappings for
	 */
	void UpdateBinding( const TWeakObjectPtr<UObject>& InRuntimeObject );

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
	void SetCurrentValue(const UObject* Object, const ValueType& InValue)
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

	/** @return the property path that this binding was initialized from */
	const FString& GetPropertyPath() const
	{
		return PropertyPath;
	}

	/** @return the property name that this binding was initialized from */
	const FName& GetPropertyName() const
	{
		return PropertyName;
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