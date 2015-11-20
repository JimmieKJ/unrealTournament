// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyPropertyParams.h"

class IPropertyHandle;

DECLARE_MULTICAST_DELEGATE_OneParam( FOnAnimatablePropertyChanged, const FPropertyChangedParams& );

DECLARE_MULTICAST_DELEGATE_OneParam ( FOnPropagateObjectChanges, UObject* );

/**
 * Listens for changes objects and calls delegates when those objects change
 */
class ISequencerObjectChangeListener
{
public:
	/**
	 * A delegate for when a property of a specific UProperty class is changed.  
	 */
	virtual FOnAnimatablePropertyChanged& GetOnAnimatablePropertyChanged( FName PropertyTypeName ) = 0;

	/**
	 * A delegate for when object changes should be propagated to/from puppet actors
	 */
	virtual FOnPropagateObjectChanges& GetOnPropagateObjectChanges() = 0;
	
	/**
	 * Triggers all properties as changed for the passed in object.
	 */
	virtual void TriggerAllPropertiesChanged(UObject* Object) = 0;

	virtual bool CanKeyProperty(FCanKeyPropertyParams KeyPropertyParams) const = 0;

	virtual void KeyProperty(FKeyPropertyParams KeyPropertyParams) const = 0;
};