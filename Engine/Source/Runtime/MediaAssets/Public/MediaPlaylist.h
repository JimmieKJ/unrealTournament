// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaPlaylist.generated.h"


class UMediaSource;


/**
 * Implements a media play list.
 */
UCLASS(BlueprintType, hidecategories=(Object))
class MEDIAASSETS_API UMediaPlaylist
	: public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Add a media source to the play list.
	 *
	 * @param MediaSource The media source to append.
	 * @see Insert, RemoveAll, Remove
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	void Add(UMediaSource* MediaSource)
	{
		Items.Add(MediaSource);
	}

	/**
	 * Get the media source at the specified index.
	 *
	 * @param Index The index of the media source to get.
	 * @return The media source, or nullptr if the index doesn't exist.
	 * @see GetNext, GetRandom
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	UMediaSource* Get(int32 Index);

	/**
	 * Get the next media source in the play list.
	 *
	 * @param InOutIndex Index of the current media source (will contain the new index).
	 * @return The media source after the current one, or nullptr if the list is empty.
	 * @see , GetPrevious, GetRandom
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	UMediaSource* GetNext(int32& InOutIndex);

	/**
	 * Get the previous media source in the play list.
	 *
	 * @param InOutIndex Index of the current media source (will contain the new index).
	 * @return The media source before the current one, or nullptr if the list is empty.
	 * @see , GetNext, GetRandom
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	UMediaSource* GetPrevious(int32& InOutIndex);

	/**
	 * Get a random media source in the play list.
	 *
	 * @param InOutIndex Index of the current media source (will contain the new index).
	 * @return The random media source, or nullptr if the list is empty.
	 * @see Get, GetNext, GetPrevious
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	UMediaSource* GetRandom(int32& InOutIndex);

	/**
	 * Insert a media source into the play list at the given position.
	 *
	 * @param MediaSource The media source to insert.
	 * @param Index The index to insert into.
	 * @see Add, Remove, RemoveAll
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	void Insert(UMediaSource* MediaSource, int32 Index)
	{
		Items.Insert(MediaSource, Index);
	}

	/**
	 * Get the number of media sources in the play list.
	 *
	 * @return Number of media sources.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	int32 Num()
	{
		return Items.Num();
	}

	/**
	 * Remove all occurrences of the given media source in the play list.
	 *
	 * @param MediaSource The media source to remove.
	 * @see Add, Insert, Remove
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	void Remove(UMediaSource* MediaSource)
	{
		Items.Remove(MediaSource);
	}

	/**
	 * Remove the media source at the specified position.
	 *
	 * @param Index The index of the media source to remove.
	 * @see Add, Insert, RemoveAll
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlaylist")
	void RemoveAt(int32 Index)
	{
		Items.RemoveAt(Index);
	}

protected:

	/** List of media sources to play. */
	UPROPERTY(EditAnywhere, Category=Playlist)
	TArray<UMediaSource*> Items;
};
