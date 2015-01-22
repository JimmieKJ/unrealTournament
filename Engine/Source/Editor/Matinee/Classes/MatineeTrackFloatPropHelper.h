// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once
#include "MatineeTrackFloatPropHelper.generated.h"

UCLASS()
class UMatineeTrackFloatPropHelper : public UInterpTrackHelper
{
	GENERATED_UCLASS_BODY()

	void OnCreateTrackTextEntry(const FString& ChosenText, TSharedRef<SWindow> Window, FString* OutputString);

public:

	// UInterpTrackHelper interface

	virtual	bool PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const override;
	virtual void  PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const override;
};

