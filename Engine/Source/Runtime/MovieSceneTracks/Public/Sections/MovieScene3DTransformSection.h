// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IKeyframeSection.h"
#include "MovieSceneKeyStruct.h"
#include "MovieSceneSection.h"
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
}

#if WITH_EDITORONLY_DATA
/** Visibility options for 3d trajectory. */
UENUM()
enum class EShow3DTrajectory
{
	EST_OnlyWhenSelected UMETA(DisplayName="Only When Selected"),
	EST_Always UMETA(DisplayName="Always"),
	EST_Never UMETA(DisplayName="Never"),
};
#endif

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
	FTransformKey( EKey3DTransformChannel::Type InChannel, EAxis::Type InAxis, float InValue, bool InbUnwindRotation )
	{
		Channel = InChannel;
		Axis = InAxis;
		Value = InValue;
		bUnwindRotation = InbUnwindRotation;
	}
	EKey3DTransformChannel::Type Channel;
	EAxis::Type Axis;
	float Value;
	bool bUnwindRotation;
};


/**
 * Proxy structure for translation keys in 3D transform sections.
 */
USTRUCT()
struct FMovieScene3DLocationKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** They key's translation value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector Location;

	FRichCurveKey* LocationKeys[3];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};


/**
 * Proxy structure for translation keys in 3D transform sections.
 */
USTRUCT()
struct FMovieScene3DRotationKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** They key's rotation value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FRotator Rotation;

	FRichCurveKey* RotationKeys[3];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};


/**
 * Proxy structure for translation keys in 3D transform sections.
 */
USTRUCT()
struct FMovieScene3DScaleKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** They key's scale value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector Scale;

	FRichCurveKey* ScaleKeys[3];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};


/**
 * Proxy structure for 3D transform section key data.
 */
USTRUCT()
struct FMovieScene3DTransformKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** They key's translation value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector Location;

	/** They key's rotation value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FRotator Rotation;

	/** They key's scale value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector Scale;

	FRichCurveKey* LocationKeys[3];
	FRichCurveKey* RotationKeys[3];
	FRichCurveKey* ScaleKeys[3];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
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

	/**
	 * Evaluates the translation component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutTranslation	The evaluated translation.  Note: will remain unchanged if there were no keys to evaluate
	 */
	MOVIESCENETRACKS_API void EvalTranslation( float Time, FVector& OutTranslation ) const;

	/**
	 * Evaluates the rotation component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutRotation		The evaluated rotation.  Note: will remain unchanged if there were no keys to evaluate
	 */
	MOVIESCENETRACKS_API void EvalRotation( float Time, FRotator& OutRotation ) const;

	/**
	 * Evaluates the scale component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutScale			The evaluated scale.  Note: will remain unchanged if there were no keys to evaluate
	 */
	MOVIESCENETRACKS_API void EvalScale( float Time, FVector& OutScale ) const;

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

	/**
	 * Return the trajectory visibility
	 */
#if WITH_EDITORONLY_DATA
	MOVIESCENETRACKS_API EShow3DTrajectory GetShow3DTrajectory() { return Show3DTrajectory; }
#endif

public:

	// UMovieSceneSection interface

	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(const TArray<FKeyHandle>& KeyHandles) override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override;
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override;

public:

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

#if WITH_EDITORONLY_DATA
	/** Whether to show the 3d trajectory */
	UPROPERTY(EditAnywhere, DisplayName = "Show 3D Trajectory", Category = "Transform")
	TEnumAsByte<EShow3DTrajectory> Show3DTrajectory;
#endif
};