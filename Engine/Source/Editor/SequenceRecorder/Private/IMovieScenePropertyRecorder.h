// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IMovieScenePropertyRecorder
{
public:
	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* MovieScene, const FGuid& Guid, float Time, bool bRecord) = 0;
	virtual void FinalizeSection() = 0;
	virtual void Record(float CurrentTime) = 0;
	virtual void InvalidateObjectToRecord() = 0;
};