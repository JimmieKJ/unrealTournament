// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "MovieSceneCapture.h"
#include "ActiveMovieSceneCaptures.h"

TUniquePtr<FActiveMovieSceneCaptures> FActiveMovieSceneCaptures::Singleton;

FActiveMovieSceneCaptures& FActiveMovieSceneCaptures::Get()
{
	if (!Singleton)
	{
		Singleton.Reset(new FActiveMovieSceneCaptures);
	}

	return *Singleton;
}

void FActiveMovieSceneCaptures::Add(UMovieSceneCapture* Capture)
{
	ActiveCaptures.AddUnique(Capture);
}

void FActiveMovieSceneCaptures::Remove(UMovieSceneCapture* Capture)
{
	ActiveCaptures.Remove(Capture);
}

void FActiveMovieSceneCaptures::Shutdown()
{
	TArray<UMovieSceneCapture*> ActiveCapturesCopy;
	Swap(ActiveCaptures, ActiveCapturesCopy);

	for (auto* Obj : ActiveCapturesCopy)
	{
		Obj->Close();
	}

	Singleton.Reset();
}

void FActiveMovieSceneCaptures::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto* Obj : ActiveCaptures)
	{
		Collector.AddReferencedObject(Obj);
	}
}