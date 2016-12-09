// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneTemplateCommon.h"
#include "Components/SceneComponent.h"

/** A movie scene pre-animated token that stores a pre-animated mobility */
struct FMobilityPreAnimatedToken : IMovieScenePreAnimatedToken
{
	FMobilityPreAnimatedToken(USceneComponent& SceneComponent)
	{
		Mobility = SceneComponent.Mobility;
	}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
	{
		USceneComponent* SceneComponent = CastChecked<USceneComponent>(&InObject);
		SceneComponent->SetMobility(Mobility);
	}

private:
	EComponentMobility::Type Mobility;
};

FMovieSceneAnimTypeID FMobilityTokenProducer::GetAnimTypeID()
{
	return TMovieSceneAnimTypeID<FMobilityTokenProducer>();
}

/** Cache the existing state of an object before moving it */
IMovieScenePreAnimatedTokenPtr FMobilityTokenProducer::CacheExistingState(UObject& Object) const
{
	USceneComponent* SceneComponent = CastChecked<USceneComponent>(&Object);
	return FMobilityPreAnimatedToken(*SceneComponent);
}


void F3DTransformTrackToken::Apply(USceneComponent& SceneComponent, float DeltaTime) const
{
	/* Cache initial absolute position */
	FVector PreviousPosition = SceneComponent.GetComponentLocation();

	SceneComponent.SetRelativeLocationAndRotation(Translation, Rotation);
	SceneComponent.SetRelativeScale3D(Scale);

	// Force the location and rotation values to avoid Rot->Quat->Rot conversions
	SceneComponent.RelativeLocation = Translation;
	SceneComponent.RelativeRotation = Rotation;

	if (DeltaTime > 0.f)
	{
		/* Get current absolute position and set component velocity */
		FVector CurrentPosition = SceneComponent.GetComponentLocation();
		FVector ComponentVelocity = (CurrentPosition - PreviousPosition) / DeltaTime;
		SceneComponent.ComponentVelocity = ComponentVelocity;

		//TODO: Set Component Velocity for attached objects
	}
};


/** A movie scene pre-animated token that stores a pre-animated transform */
struct F3DTransformTrackPreAnimatedToken : F3DTransformTrackToken, IMovieScenePreAnimatedToken
{
	F3DTransformTrackPreAnimatedToken(USceneComponent& SceneComponent)
	{
		FTransform ExistingTransform = SceneComponent.GetRelativeTransform();

		Translation = ExistingTransform.GetTranslation();
		Rotation = ExistingTransform.Rotator();
		Scale = ExistingTransform.GetScale3D();
	}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
	{
		USceneComponent* SceneComponent = CastChecked<USceneComponent>(&InObject);

		Apply(*SceneComponent);
	}
};

FMovieSceneAnimTypeID F3DTransformTokenProducer::GetAnimTypeID()
{
	return TMovieSceneAnimTypeID<F3DTransformTokenProducer>();
}

/** Cache the existing state of an object before moving it */
IMovieScenePreAnimatedTokenPtr F3DTransformTokenProducer::CacheExistingState(UObject& Object) const
{
	USceneComponent* SceneComponent = CastChecked<USceneComponent>(&Object);
	return F3DTransformTrackPreAnimatedToken(*SceneComponent);
}

