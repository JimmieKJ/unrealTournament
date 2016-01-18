// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneCapture.h"

class FActiveMovieSceneCaptures : public FGCObject
{
public:
	/** Singleton access to an instance of this class */
	static FActiveMovieSceneCaptures& Get();

	/** Add a capture to this class to ensure that it will get updated */
	void Add(UMovieSceneCapture* Capture);

	/** Remove a capture from this class */
	void Remove(UMovieSceneCapture* Capture);

	/** Shutdown this class, waiting for any currently active captures to finish capturing */
	void Shutdown();

	/** We use the pending array here, as it's the most up to date */
	const TArray<UMovieSceneCapture*>& GetActiveCaptures() const { return ActiveCaptures; }

private:

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	/** Private construction to enforce singleton use */
	FActiveMovieSceneCaptures() {}

	/** Array of active captures */
	TArray<UMovieSceneCapture*> ActiveCaptures;

	/** Class singleton */
	static TUniquePtr<FActiveMovieSceneCaptures> Singleton;
};