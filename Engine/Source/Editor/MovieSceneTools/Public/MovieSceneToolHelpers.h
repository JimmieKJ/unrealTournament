// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class UMovieSceneSection;


class MOVIESCENETOOLS_API MovieSceneToolHelpers
{
public:
	/**
	* Trim section at the given time
	*
	* @param Sections The sections to trim
	* @param Time	The time at which to trim
	* @param bTrimLeft Trim left or trim right
	*/
	static void TrimSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time, bool bTrimLeft);

	/**
	* Splits sections at the given time
	*
	* @param Sections The sections to split
	* @param Time	The time at which to split
	*/
	static void SplitSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time);
};
