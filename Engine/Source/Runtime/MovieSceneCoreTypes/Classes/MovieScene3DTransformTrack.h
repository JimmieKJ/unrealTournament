// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieScene3DTransformTrack.generated.h"

/**
 * Stores information about a transform for the purpose of adding keys to a transform section
 */
struct FTransformData
{
	/** Translation component */
	FVector Translation;
	/** Rotation component */
	FRotator Rotation;
	/** Scale component */
	FVector Scale;
	/** Whether or not the data is valid (any values set) */
	bool bValid;

	bool IsValid() const { return bValid; }

	/**
	 * Constructor.  Builds the data from a scene component
	 * Uses relative transform only
	 *
	 * @param InComponent	The component to build from
	 */
	FTransformData( const USceneComponent* InComponent )
		: Translation( InComponent->RelativeLocation )
		, Rotation( InComponent->RelativeRotation )
		, Scale( InComponent->RelativeScale3D )
		, bValid( true )
	{}

	FTransformData()
		: bValid( false )
	{}
};

/**
 * Represents a transform that should be keyed at a specific  time
 * 
 * Only keys components of the transform that have changed
 */
class FTransformKey
{
public:
	/**
	 * Constructor
	 *
	 * @param InKeyTime			The time where keys should be added
	 * @param InNewTransform	The transform values that should be keyed
	 * @param PreviousTransform	The previous transform values (for comparing against new values).  If this is set only changed values will be keyed
	 * 
	 */
	FTransformKey( float InKeyTime, const FTransformData& InNewTransform, const FTransformData& InPreviousTransform = FTransformData() )
		: NewTransform( InNewTransform )
		, PreviousTransform( InPreviousTransform )
		, KeyTime( InKeyTime )
	{
	}	

	/** @return The time where transform keys should be added */
	float GetKeyTime() const { return KeyTime; }

	/** @return the translation value of the key */
	const FVector& GetTranslationValue() const { return NewTransform.Translation; }

	/** @return the rotation value of the key */
	const FRotator& GetRotationValue() const { return NewTransform.Rotation; }

	/** @return the scale value of the key */
	const FVector& GetScaleValue() const { return NewTransform.Scale; }

	/**
	 * Determines if a key should be added on the given axis of translation
	 *
	 * @param Axis	The axis to check
	 * @return Whether or not we should key translation on the given axis
	 */
	bool ShouldKeyTranslation( EAxis::Type Axis ) const;

	/**
	 * Determines if a key should be added on the given axis of rotation
	 *
	 * @param Axis	The axis to check
	 * @return Whether or not we should key rotation on the given axis
	 */
	bool ShouldKeyRotation( EAxis::Type Axis ) const;

	/**
	 * Determines if a key should be added on the given axis of scale
	 *
	 * @param Axis	The axis to check
	 * @return Whether or not we should key scale on the given axis
	 */
	bool ShouldKeyScale( EAxis::Type Axis ) const;
	
	/**
	 * Determines if a key should be added to any axis for any type of transform
	 *
	 * @return Whether or not we should key on the given axis
	 */
	bool ShouldKeyAny() const;
private:
	/**
	 * Determines whether or not a vector component is different on the given axis
	 *
	 * @param Axis	The axis to check
	 * @param Current	The current vector value
	 * @param Previous	The previous vector value
	 * @return true if a vector component is different on the given axis
	 */
	bool GetVectorComponentIfDifferent( EAxis::Type Axis, const FVector& Current, const FVector& Previous ) const;
private:
	/** New transform data */
	FTransformData NewTransform;
	FTransformData PreviousTransform;
	float KeyTime;
};

/**
 * Handles manipulation of component transforms in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieScene3DTransformTrack : public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()
public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	
	/**
	 * Adds a key to a section.  Will create the section if it doesn't exist
	 *
	 * @param ObjectHandle		Handle to the object(s) being changed
	 * @param InKey				The transform key to add
	 * @param bUnwindRotation	When true, the value will be treated like a rotation value in degrees, and will automatically be unwound to prevent flipping 360 degrees from the previous key 	 
	 * @return True if the key was successfully added.
	 */
	virtual bool AddKeyToSection( const FGuid& ObjectHandle, const FTransformKey& InKey, const bool bUnwindRotation );

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position		The current playback position
	 * @param LastPosition		The last plackback position
	 * @param OutTranslation	The evalulated translation component of the transform
	 * @param OutRotation		The evalulated rotation component of the transform
	 * @param OutScale		The evalulated scale component of the transform
	 * @param OutHasTranslationKeys true if a translation key was evaluated 
	 * @param OutHasRotationKeys 	true if a rotation key was evaluated
	 * @param OutHasScaleKeys 	true if a scale key was evaluated
	 * @return true if anything was evaluated. Note: if false is returned OutBool remains unchanged
	 */
	virtual bool Eval( float Position, float LastPosition, FVector& OutTranslation, FRotator& OutRotation, FVector& OutScale, bool& OutHasTranslationKeys, bool& OutHasRotationKeys, bool& OutHasScaleKeys ) const;
};
