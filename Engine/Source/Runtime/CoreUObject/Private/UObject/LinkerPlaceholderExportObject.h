// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Object.h"
#include "Set.h"

// Forward declarations
class FObjectInitializer;
class UObjectProperty;

/**  
 * A utility class for the deferred dependency loader, used to stub in temporary
 * export objects so we don't spawn any Blueprint class instances before the 
 * class is fully regenerated.
 */ 
class ULinkerPlaceholderExportObject : public UObject
{
public:
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(ULinkerPlaceholderExportObject, UObject, /*TStaticFlags =*/0, CoreUObject, /*TStaticCastFlags =*/0, NO_API)

	ULinkerPlaceholderExportObject(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	virtual void BeginDestroy() override;
	// End of UObject interface

	/**
	 * Logs the supplied property as referencing this class (meaning an instance  
	 * is referencing this object through that property's value).
	 * 
	 * @param  ReferencingProperty    The property whose object-value is pointing to this class.
	 */
	void AddReferencingProperty(const UObjectProperty* ReferencingProperty);

	/**
	 * Checks to see if the specified property is being tracked by this as a
	 * referencer.
	 * 
	 * @param  Property    The property you want to check for.
	 * @return True if the property was added as a referencer through AddReferencingProperty(), otherwise false.
	 */
	bool IsReferencedBy(const UObjectProperty* Property);

	/**
	 * Resolves all referencing object values (assuming they were all made as 
	 * part of the specified Container object).
	 *
	 * NOTE: there is a very strict assumption here, that any property values 
	 *       that were set to this (and recorded through AddReferencingProperty)
	 *       were properties belonging to the specified Container object (if not
	 *       there could be dangling, unresolved references).
	 * 
	 * @param  Container		The object whose properties may be referencing this temporary object.
	 * @param  NewObjectValue   The object which should replace all references to this. 
	 */
	void ReplaceReferencingObjectValues(UClass* Container, UObject* NewObjectValue);

private:
	/**
	 * A recursive method that replaces all leaf references to this object with 
	 * the supplied ReplacementValue.
	 *
	 * This function recurses the property chain (from class owner down) because 
	 * at the time of AddReferencingProperty() we cannot know/record the 
	 * address/index of array properties (as they may change during array 
	 * re-allocation or compaction). So we must follow the property chain and 
	 * check every UArrayProperty member for references to this (hence, the need
	 * for this recursive function).
	 * 
	 * @param  PropertyChain    An ascending outer chain, where the property at index zero is the leaf (referencer) property.
	 * @param  ChainIndex		An index into the PropertyChain that this call should start at and iterate DOWN to zero.
	 * @param  ValueAddress     The memory address of the value corresponding to the property at ChainIndex.
	 * @param  ReplacementValue	The new object to replace all references to this with.
	 * @return The number of references that were replaced.
	 */
	int32 ResolvePlaceholderValues(const TArray<const UProperty*>& PropertyChain, int32 ChainIndex, uint8* ValueAddress, UObject* ReplacementValue);

	/**
	 * A list of KNOWN referencing properties (meaning an object instance 
	 * references this object through these properties).
	 *
	 * @TODO: consider making this a map with a ref-counter, so we can remove 
	 *        property references (as they may be replaced prior to resolve)
	 */
	TSet< TWeakObjectPtr<UObjectProperty> > ReferencingProperties;

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
public:
	/** Used to catch any references that were added after ULinkerLoad has already done a resolve pass. */
	bool bWasResolved;
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
}; 
