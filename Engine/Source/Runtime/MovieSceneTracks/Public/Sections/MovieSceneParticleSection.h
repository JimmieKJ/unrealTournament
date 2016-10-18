// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/IntegralCurve.h"
#include "MovieSceneSection.h"
#include "MovieSceneParticleSection.generated.h"


/**
 * Defines the types of particle keys.
 */
UENUM()
namespace EParticleKey
{
	enum Type
	{
		Activate = 0,
		Deactivate = 1,
		Trigger = 2,
	};
}


/**
 * Particle section, for particle toggling and triggering.
 */
UCLASS(MinimalAPI)
class UMovieSceneParticleSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

	MOVIESCENETRACKS_API void AddKey(float Time, EParticleKey::Type KeyType);

	MOVIESCENETRACKS_API FIntegralCurve& GetParticleCurve();

public:

	//~ UMovieSceneSection interface

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	/** Curve containing the particle keys. */
	UPROPERTY()
	FIntegralCurve ParticleKeys;
};
