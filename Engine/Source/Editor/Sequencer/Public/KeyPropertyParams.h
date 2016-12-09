// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

class IPropertyHandle;
enum class ESequencerKeyMode;

/**
 * Parameters for determining if a property can be keyed.
 */
struct SEQUENCER_API FCanKeyPropertyParams
{
	/**
	 * Creates new can key property parameters.
	 * @param InObjectClass the class of the object which has the property to be keyed.
	 * @param InPropertyPath an array of UProperty objects which represents a path of properties to get from
	 *        the root object to the property to be keyed.
	 */
	FCanKeyPropertyParams(UClass* InObjectClass, TArray<UProperty*> InPropertyPath);

	/**
	* Creates new can key property parameters.
	* @param InObjectClass the class of the object which has the property to be keyed.
	* @param InPropertyHandle a handle to the property to be keyed.
	*/
	FCanKeyPropertyParams(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle);

	/** The class of the object which has the property to be keyed. */
	const UClass* ObjectClass;

	/** An array of UProperty objects which represents a path of properties to get from the root object to the
	property to be keyed. */
	const TArray<UProperty*> PropertyPath;
};

/**
 * Parameters for keying a property.
 */
struct SEQUENCER_API FKeyPropertyParams
{
	/**
	* Creates new key property parameters for a manually triggered property change.
	* @param InObjectsToKey an array of the objects who's property will be keyed.
	* @param InPropertyPath an array of UProperty objects which represents a path of properties to get from
	*        the root object to the property to be keyed.
	*/
	FKeyPropertyParams(TArray<UObject*> InObjectsToKey, TArray<UProperty*> InPropertyPath, ESequencerKeyMode InKeyMode);

	/**
	* Creates new key property parameters from an actual property change notification with a property handle.
	* @param InObjectsToKey an array of the objects who's property will be keyed.
	* @param InPropertyHandle a handle to the property to be keyed.
	*/
	FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const IPropertyHandle& InPropertyHandle, ESequencerKeyMode InKeyMode);

	/** An array of the objects who's property will be keyed. */
	const TArray<UObject*> ObjectsToKey;

	/** An array of UProperty objects which represents a path of properties to get from the root object to the
	property to be keyed. */
	const TArray<UProperty*> PropertyPath;

	/** Keyframing params */
	const ESequencerKeyMode KeyMode;
};

/**
 * Parameters for the property changed callback.
 */
class SEQUENCER_API FPropertyChangedParams
{
public:
	FPropertyChangedParams(TArray<UObject*> InObjectsThatChanged, TArray<UProperty*> InPropertyPath, FName InStructPropertyNameToKey, ESequencerKeyMode InKeyMode);

	/**
	 * Gets the value of the property that changed.
	 */
	template<typename ValueType>
	ValueType GetPropertyValue() const
	{
		void* CurrentObject = ObjectsThatChanged[0];
		void* PropertyValue = nullptr;
		for (int32 i = 0; i < PropertyPath.Num(); i++)
		{
			CurrentObject = PropertyPath[i]->ContainerPtrToValuePtr<ValueType>(CurrentObject, 0);
		}

		// Bool property values are stored in a bit field so using a straight cast of the pointer to get their value does not
		// work.  Instead use the actual property to get the correct value.
		const UBoolProperty* BoolProperty = Cast<const UBoolProperty>( PropertyPath.Last() );
		bool BoolPropertyValue;
		if ( BoolProperty )
		{
			BoolPropertyValue = BoolProperty->GetPropertyValue(CurrentObject);
			PropertyValue =  &BoolPropertyValue;
		}
		else
		{
			PropertyValue = CurrentObject;
		}

		return *((ValueType*)PropertyValue);
	}

	/** Gets the property path as a period seperated string of property names. */
	FString GetPropertyPathString() const;

	/** An array of the objects that changed. */
	const TArray<UObject*> ObjectsThatChanged;

	/** An array of UProperty objects which represents a path of properties to get from the root object to the 
	property that changed. */
	const TArray<UProperty*> PropertyPath;

	/** Represents the FName of an inner property which should be keyed for a struct property.  If all inner 
	properties should be keyed, this will be FName::None. */
	const FName StructPropertyNameToKey;

	/** Keyframing params */
	const ESequencerKeyMode KeyMode;
};
