// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyframeTrackEditor.h"
#include "MovieScene3DTransformTrack.h"
#include "MovieScene3DTransformSection.h"


/**
 * Tools for animatable transforms
 */
class F3DTransformTrackEditor
	: public FKeyframeTrackEditor<UMovieScene3DTransformTrack, UMovieScene3DTransformSection, FTransformKey>
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer	The sequencer instance to be used by this tool
	 */
	F3DTransformTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~F3DTransformTrackEditor();

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	// ISequencerTrackEditor interface

	virtual void AddKey( const FGuid& ObjectGuid, UObject* AdditionalAsset = NULL ) override;
	virtual void BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings) override;
	virtual void BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track ) override;
	virtual void OnRelease() override;
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;

	// FKeyframeTrackEditor interface
	virtual bool ShouldAddKey(UMovieScene3DTransformTrack* InTrack, FTransformKey InKey, FKeyParams InKeyParams) const override;

private:

	/** Returns whether or not a transform track can be added for an actor with a specific handle. */
	bool CanAddTransformTrackForActorHandle(FGuid ActorHandle) const;

	/**
	 * Called before an actor or component transform changes
	 *
	 * @param Object The object whose transform is about to change
	 */
	void OnPreTransformChanged( UObject& InObject );

	/**
	 * Called when an actor or component transform changes
	 *
	 * @param Object The object whose transform has changed
	 */
	void OnTransformChanged( UObject& InObject );

	/** Delegate for camera button visible state */
	EVisibility IsCameraVisible(FGuid ObjectGuid) const;

	/** Delegate for camera button lock state */
	ECheckBoxState IsCameraLocked(FGuid ObjectGuid) const; 

	/** Delegate for locked camera button */
	void OnLockCameraClicked(ECheckBoxState CheckBoxState, FGuid ObjectGuid);

	/** Delegate for camera button lock tooltip */
	FText GetLockCameraToolTip(FGuid ObjectGuid) const; 

	/** Tries to get an actor and root scene component from an object which can be either an actor or a component. */
	void GetActorAndSceneComponentFromObject(UObject* Object, AActor*& OutActor, USceneComponent*& OutSceneComponent);

	/** Generates transform keys based on the last transform, the current transform, and other options. 
		One transform key is generated for each individual key to be added to the section. */
	void GetChangedTransformKeys(const FTransformData& LastTransform, const FTransformData& CurrentTransform, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, TArray<FTransformKey>& OutKeys );

	/**
	* Adds transform tracks and keys to the selected objects in the level.
	*
	* @param Channel The transform channel to add keys for.
	*/
	void OnAddTransformKeysForSelectedObjects( EKey3DTransformChannel::Type Channel );

	/** 
	 * Adds transform keys to an object represented by a handle.

	 * @param ObjectHandle The handle to the object to add keys to.
	 * @param ChannelToKey The channels to add keys to.
	 * @param KeyParams Parameters which control how the keys are added. 
	 */
	void AddTransformKeysForHandle( FGuid ObjectHandle, EKey3DTransformChannel::Type ChannelToKey, FKeyParams KeyParams );

	/**
	* Adds transform keys to a specific object.

	* @param Object The object to add keys to.
	* @param ChannelToKey The channels to add keys to.
	* @param KeyParams Parameters which control how the keys are added.
	*/
	void AddTransformKeysForObject( UObject* Object, EKey3DTransformChannel::Type ChannelToKey, FKeyParams KeyParams );

	/**
	* Adds keys to a specific actor.

	* @param LastTransform The last known transform of the actor if any.
	* @param CurrentTransform The current transform of the actor.
	* @param ChannelToKey The channels to add keys to.
	* @param bUnwindRotation Whether or not rotation key values should be unwound.
	* @param KeyParams Parameters which control how the keys are added.
	*/
	void AddTransformKeys( AActor* ActorToKey, const FTransformData& LastTransform, const FTransformData& CurrentTransform, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, FKeyParams KeyParams );

	/**
	* Delegate target of AnimatablePropertyChanged which actually adds the keys.

	* @param Time The time to add keys.
	* @param ActorToKey The actor to add keys to.
	* @param Keys The keys to add.
	* @param KeyParams Parameters which control how the keys are added.
	*/
	bool OnAddTransformKeys( float Time, AActor* ActorToKey, TArray<FTransformKey>* Keys, FTransformData CurrentTransform, FKeyParams KeyParams );

private:

	DECLARE_MULTICAST_DELEGATE_TwoParams( FOnSetIntermediateValueFromTransformChange, UMovieSceneTrack*, FTransformData )

	void SetIntermediateValueFromTransformChange( UMovieSceneTrack* Track, FTransformData TransformData );

private:

	static FName TransformPropertyName;

	/** Mapping of objects to their existing transform data (for comparing against new transform data) */
	TMap< TWeakObjectPtr<UObject>, FTransformData > ObjectToExistingTransform;

	FOnSetIntermediateValueFromTransformChange OnSetIntermediateValueFromTransformChange;
};
