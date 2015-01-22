// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MatineeTrackColorPropHelper.generated.h"

UCLASS()
class UMatineeTrackColorPropHelper : public UMatineeTrackVectorPropHelper
{
	GENERATED_UCLASS_BODY()

public:

	// UInterpTrackHelper interface

	virtual	bool PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const override;
	virtual void  PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const override;
};
