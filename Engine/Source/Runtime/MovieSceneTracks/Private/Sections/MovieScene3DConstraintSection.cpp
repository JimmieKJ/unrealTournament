// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieScene3DConstraintSection.h"


UMovieScene3DConstraintSection::UMovieScene3DConstraintSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


void UMovieScene3DConstraintSection::SetConstraintId(const FGuid& InConstraintId)
{
	Modify();

	ConstraintId = InConstraintId;
}


FGuid UMovieScene3DConstraintSection::GetConstraintId() const
{
	return ConstraintId;
}
