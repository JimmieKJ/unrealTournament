// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "IKeyframeSection.h"
#include "MovieScene3DTransformSection.generated.h"


namespace EKey3DTransformChannel
{
	enum Type
	{
		Translation = 0x00000001,
		Rotation = 0x00000002,
		Scale = 0x00000004,
		All = Translation | Rotation | Scale
	};

	enum ValueType
	{
		Key,
		Default
	};
}

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


struct FTransformKey
{
	FTransformKey( EKey3DTransformChannel::Type InChannel, EKey3DTransformChannel::ValueType InChannelValueType, EAxis::Type InAxis, float InValue, bool InbUnwindRotation )
	{
		Channel = InChannel;
		ChannelValueType = InChannelValueType;
		Axis = InAxis;
		Value = InValue;
		bUnwindRotation = InbUnwindRotation;
	}
	EKey3DTransformChannel::Type Channel;
	EKey3DTransformChannel::ValueType ChannelValueType;
	EAxis::Type Axis;
	float Value;
	bool bUnwindRotation;
};


/**
 * A 3D transform section
 */
UCLASS(MinimalAPI)
class UMovieScene3DTransformSection
	: public UMovieSceneSection
	, public IKeyframeSection<FTransformKey>
{
	GENERATED_UCLASS_BODY()

public:

	/** MovieSceneSection interface */
	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;

	/**
	 * Evaluates the translation component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutTranslation	The evaluated translation.  Note: will remain unchanged if there were no keys to evaluate
	 */
	void EvalTranslation( float Time, FVector& OutTranslation ) const;

	/**
	 * Evaluates the rotation component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutRotation		The evaluated rotation.  Note: will remain unchanged if there were no keys to evaluate
	 */
	void EvalRotation( float Time, FRotator& OutRotation ) const;

	/**
	 * Evaluates the scale component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutScale			The evaluated scale.  Note: will remain unchanged if there were no keys to evaluate
	 */
	void EvalScale( float Time, FVector& OutScale ) const;

	/** 
	 * Returns the translation curve for a specific axis
	 *
	 * @param Axis	The axis of the translation curve to get
	 * @return The curve on the axis
	 */
	MOVIESCENETRACKS_API FRichCurve& GetTranslationCurve( EAxis::Type Axis );

	/** 
	 * Returns the rotation curve for a specific axis
	 *
	 * @param Axis	The axis of the rotation curve to get
	 * @return The curve on the axis
	 */
	MOVIESCENETRACKS_API FRichCurve& GetRotationCurve( EAxis::Type Axis );

	/** 
	 * Returns the scale curve for a specific axis
	 *
	 * @param Axis	The axis of the scale curve to get
	 * @return The curve on the axis
	 */
	MOVIESCENETRACKS_API FRichCurve& GetScaleCurve( EAxis::Type Axis );

	// IKeyframeSection interface.

	virtual bool NewKeyIsNewData( float Time, const FTransformKey& KeyData ) const override;
	virtual bool HasKeys( const FTransformKey& KeyData ) const override;
	virtual void AddKey( float Time, const FTransformKey& KeyData, EMovieSceneKeyInterpolation KeyInterpolation ) override;
	virtual void SetDefault( const FTransformKey& KeyData ) override;

private:
	/** Translation curves */
	UPROPERTY()
	FRichCurve Translation[3];
	
	/** Rotation curves */
	UPROPERTY()
	FRichCurve Rotation[3];

	/** Scale curves */
	UPROPERTY()
	FRichCurve Scale[3];
};