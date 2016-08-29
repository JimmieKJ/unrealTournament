// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneCameraShakeSection.generated.h"

class UCameraShake;

/**
 *
 */
UCLASS(MinimalAPI)
class UMovieSceneCameraShakeSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:

	// ctor
	UMovieSceneCameraShakeSection(const FObjectInitializer& ObjectInitializer);

	/** Sets the CameraShake for this section */
	void SetCameraShakeClass(TSubclassOf<UCameraShake> InShakeClass) { ShakeClass = InShakeClass; }

	/** Gets the CameraAnim for this section */
	UClass* GetCameraShakeClass() const { return ShakeClass; }

	/** MovieSceneSection interface */
	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override;
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override;

	/** 
	 * @return Returns the weighting curve
	 */
//	MOVIESCENETRACKS_API FRichCurve& GetShakeWeightCurve() { return ShakeWeightCurve; }
	
private:
	
	/** */
// 	UPROPERTY(EditAnywhere, Category = "Camera Shake")
// 	FRichCurve ShakeWeightCurve;

public:
	/** Class of the camera shake to play */
	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	TSubclassOf<UCameraShake> ShakeClass;
	
	/** Scalar that affects shake intensity */
	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	float PlayScale;
	
	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	TEnumAsByte<ECameraAnimPlaySpace::Type> PlaySpace;

	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	FRotator UserDefinedPlaySpace;
};