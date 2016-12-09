// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"

struct HMovieSceneKeyProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( MOVIESCENETRACKS_API );

	/** The track that contains the section */
	class UMovieSceneTrack* MovieSceneTrack;
	/** The section that contains the keyframe */
	class UMovieSceneSection* MovieSceneSection;
	/** The time of the key that is selected */
	float Time;

	HMovieSceneKeyProxy(class UMovieSceneTrack* InTrack, class UMovieSceneSection* InSection, float InTime) :
		HHitProxy(HPP_UI),
		MovieSceneTrack(InTrack),
		MovieSceneSection(InSection),
		Time(InTime)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

