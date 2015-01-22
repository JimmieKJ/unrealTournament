// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePlayer.h"
#include "MovieScene.h"
#include "MovieSceneBindings.h"
#include "Runtime/Engine/Classes/MovieScene/RuntimeMovieScenePlayerInterface.h"

#include "RuntimeMovieScenePlayer.generated.h"


/**
 * RuntimeMovieScenePlayer is used to actually "play" a MovieScene asset at runtime.  It keeps track of playback
 * state and provides functions for manipulating a MovieScene while its playing.
 */
UCLASS( MinimalAPI )
class URuntimeMovieScenePlayer : public UObject, public IMovieScenePlayer, public IRuntimeMovieScenePlayerInterface
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Static:  Creates a RuntimeMovieScenePlayer.  This function is designed to be called through the expanded PlayMovieScene
	 * Kismet node, and not called directly through visual script.
	 *
	 * @param	Level	The level to play this MovieScene in.  This level will "own" the instance of the returned RuntimeMovieScenePlayer.
	 * @param	MovieSceneBindings	The MovieScene to play, along with the bindings to any actors in the level that may be possessed during playback.
	 *
	 * @return	Returns the newly created RuntimeMovieScenePlayer instance for this MovieScene
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	static URuntimeMovieScenePlayer* CreateRuntimeMovieScenePlayer( class ULevel* Level, class UMovieSceneBindings* InMovieSceneBindings );

	/**
	 * Starts playback from the current time cursor position.  Called by the expanded PlayMovieScene node.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Play();

	/**
	 * Pauses playback.  Called by the expanded PlayMovieScene node.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Pause();

	/** @return Returns true if the sequence is actively playing, or false if paused or if Play() was never called */
	bool IsPlaying() const;


protected:

	/** Sets the bindings for this RuntimeMovieScenePlayer.  Should be called right after construction. */
	void SetMovieSceneBindings( UMovieSceneBindings* NewMovieSceneBindings );

	/** IMovieScenePlayer interface */
	virtual void SpawnActorsForMovie( TSharedRef<FMovieSceneInstance> MovieSceneInstance );
	virtual void DestroyActorsForMovie( TSharedRef<FMovieSceneInstance> MovieSceneInstance );
	virtual void GetRuntimeObjects( TSharedRef<FMovieSceneInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray< UObject* >& OutObjects ) const override;
	virtual void UpdatePreviewViewports(UObject* ObjectToViewThrough) const override {}
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual void AddMovieSceneInstance( class UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneInstance> InstanceToAdd ) override;
	virtual void RemoveMovieSceneInstance( class UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneInstance> InstanceToRemove ) override;
	virtual TSharedRef<FMovieSceneInstance> GetRootMovieSceneInstance() const override { return RootMovieSceneInstance.ToSharedRef(); }
	/** IRuntimeMovieScenePlayerInterface */
	virtual void Tick( const float DeltaSeconds ) override;


private:
	struct FSpawnedActorInfo
	{
		/** Identifier that maps this actor to a movie scene spawnable */
		FGuid RuntimeGuid;
		/** The spawned actor */
		TWeakObjectPtr<AActor> SpawnedActor;
	};

	/** The world this player will spawn actors in, if needed */
	TWeakObjectPtr< UWorld > World;

	/** The MovieScene we're playing as well as bindings for this MovieScene.  May contain sub-scenes. */
	UPROPERTY()
	UMovieSceneBindings* MovieSceneBindings;

	/** The current time cursor position within the sequence (in seconds) */
	UPROPERTY()
	double TimeCursorPosition;

	/** Whether we're currently playing.  If false, then sequence playback is paused or was never started. */
	UPROPERTY()
	bool bIsPlaying;

	/** The root movie scene instance to update when playing. */
	TSharedPtr<class FMovieSceneInstance> RootMovieSceneInstance;

	/** Maps spawnable GUIDs to their spawned actor in the world */
	TMap< TWeakPtr<FMovieSceneInstance>, TArray< FSpawnedActorInfo > > InstanceToSpawnedActorMap;
};


