// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "ISequencerObjectChangeListener.h"

class IPropertyHandle;

/**
 * Listens for changes objects and calls delegates when those objects change
 */
class FSequencerObjectChangeListener : public ISequencerObjectChangeListener
{
public:
	FSequencerObjectChangeListener( TSharedRef<ISequencer> InSequencer, bool bInListenForActorsOnly );
	virtual ~FSequencerObjectChangeListener();

	/** ISequencerObjectChangeListener interface */
	virtual FOnAnimatablePropertyChanged& GetOnAnimatablePropertyChanged( FName PropertyTypeName ) override;
	virtual FOnPropagateObjectChanges& GetOnPropagateObjectChanges() override;
	virtual bool CanKeyProperty(FCanKeyPropertyParams KeyPropertyParams) const override;
	virtual void KeyProperty(FKeyPropertyParams KeyPropertyParams) const override;
	virtual void TriggerAllPropertiesChanged(UObject* Object) override;

private:
	/**
	 * Called when PreEditChange is called on an object
	 *
	 * @param Object	The object that PreEditChange was called on
	 * @param Property	The property that is about to change
	 */
	void OnObjectPreEditChange( UObject* Object, const FEditPropertyChain& PropertyChain );

	/**
	 * Called when PostEditChange is called on an object
	 *
	 * @param Object	The object that PostEditChange was called on
	 */
	void OnObjectPostEditChange( UObject* Object, FPropertyChangedEvent& PropertyChangedEvent );

	/**
	 * Called after an actor is moved (PostEditMove)
	 *
	 * @param Actor The actor that PostEditMove was called on
	 */
	void OnActorPostEditMove( AActor* Actor );

	/**
	 * Called when a property changes on an object that is being observed
	 *
	 * @param Object	The object that PostEditChange was called on
	 */
	void OnPropertyChanged( const TArray<UObject*>& ChangedObjects, const IPropertyHandle& PropertyHandle ) const;

	/** Broadcasts the property change callback to the appropriate listeners. */
	void BroadcastPropertyChanged(FKeyPropertyParams KeyPropertyParams) const;

	/**
	 * @return True if an object is valid for listening to property changes 
	 */
	bool IsObjectValidForListening( UObject* Object ) const;

	/** @return Whether or not a property setter could be found for a property on a class */
	bool FindPropertySetter( const UClass& ObjectClass, const FName PropertyTypeName, const FString& PropertyVarName, const UStructProperty* StructProperty = 0 ) const;
private:
	/** Mapping of object to a listener used to check for property changes */
	TMap< TWeakObjectPtr<UObject>, TSharedPtr<class IPropertyChangeListener> > ActivePropertyChangeListeners;

	/** A mapping of property classes to multi-cast delegate that is broadcast when properties of that type change */
	TMap< FName, FOnAnimatablePropertyChanged > ClassToPropertyChangedMap;

	/** Delegate to call when object changes should be propagated */
	FOnPropagateObjectChanges OnPropagateObjectChanges;

	/** The owning sequencer */
	TWeakPtr< ISequencer > Sequencer;

	/** True to listen for actors only */
	bool bListenForActorsOnly;
};