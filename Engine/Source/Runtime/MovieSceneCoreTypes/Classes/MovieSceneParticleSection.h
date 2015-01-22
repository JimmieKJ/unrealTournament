// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneParticleSection.generated.h"

/**
 * The kind of key this particle section is
 */
UENUM()
namespace EParticleKey
{
	enum Type
	{
		Toggle,
		Trigger
	};
}

/**
 * Particle section, for particle toggling and triggering
 */
UCLASS( MinimalAPI )
class UMovieSceneParticleSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	/** Sets this section's key type*/
	void SetKeyType(EParticleKey::Type InKeyType) {KeyType = InKeyType;}

	/** Gets the key type for this section */
	EParticleKey::Type GetKeyType() {return KeyType;}
	
private:
	/** The way this particle key will operate */
	UPROPERTY()
	TEnumAsByte<EParticleKey::Type> KeyType;
};
