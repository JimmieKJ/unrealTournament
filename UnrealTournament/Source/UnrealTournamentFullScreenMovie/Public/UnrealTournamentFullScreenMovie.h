// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __UNREALTOURNAMENTFULLSCREENMOVIE_H__
#define __UNREALTOURNAMENTFULLSCREENMOVIE_H__

#include "ModuleInterface.h"
#include "SlateBasics.h"

#if !UE_SERVER
#include "MoviePlayer.h"
#endif

/** Module interface for this game's loading screens */
class IUnrealTournamentFullScreenMovieModule : public IModuleInterface
{
public:

#if !UE_SERVER

	/** Kicks off the loading screen for in game loading (not startup) */
	virtual void PlayMovie(const FString& MovieName, TSharedPtr<SWidget> SlateOverlayWidget, bool bSkippable, bool bAutoComplete, TEnumAsByte<EMoviePlaybackType> PlaybackType, bool bForce) = 0;

	// Stops a movie from playing
	virtual void StopMovie() = 0;

	/** Tells the movie player that it's ok to exit and/or wait for the movie to finished */
	virtual void WaitForMovieToFinished(TSharedPtr<SWidget> SlateOverlayWidget) = 0;

	// returns true if a movie is currently being played
	virtual bool IsMoviePlaying() = 0;

	/** Allows you to set the slate overlay after the movie has begun */
	virtual void SetSlateOverlayWidget(TSharedPtr<SWidget> NewSlateOverlayWidget) = 0;


#endif

};

#endif // __UNREALTOURNAMENTFULLSCREENMOVIE_H__


