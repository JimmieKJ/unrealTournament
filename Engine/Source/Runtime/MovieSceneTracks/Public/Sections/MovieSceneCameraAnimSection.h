// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneCameraAnimSection.generated.h"

/**
 *
 */
UCLASS(MinimalAPI)
class UMovieSceneCameraAnimSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	// ctor
	UMovieSceneCameraAnimSection(const FObjectInitializer& ObjectInitializer);

	/** Sets the CameraAnim for this section */
	void SetCameraAnim(UCameraAnim* InCameraAnim) { CameraAnim = InCameraAnim; }
	
	/** Gets the CameraAnim for this section */
	class UCameraAnim* GetCameraAnim() const { return CameraAnim; }
	
	/** MovieSceneSection interface */
	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override;
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override;

	/** #todo */
//	MOVIESCENETRACKS_API FRichCurve& GetAnimWeightCurve() { return AnimWeightCurve; }

private:
// 	UPROPERTY(EditAnywhere, Category="Camera Anim")
// 	FRichCurve AnimWeightCurve;

	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	UCameraAnim* CameraAnim;

public:
	/** How fast to play back the animation. */
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	float PlayRate;
	
	/** Scalar to control intensity of the animation. */
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	float PlayScale;
	
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	float BlendInTime;
	
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	float BlendOutTime;
	
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	bool bLooping;
	
// 	UPROPERTY(EditAnywhere, Category = "Camera Anim")
 	bool bRandomStartTime;
	
// 	UPROPERTY(EditAnywhere, Category = "Camera Anim")
// 	float Duration;
	
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	TEnumAsByte<ECameraAnimPlaySpace::Type> PlaySpace;
	
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	FRotator UserDefinedPlaySpace;
};