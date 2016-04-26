// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MovieSceneFolder.generated.h"

/** Reprents a folder used for organizing objects in tracks in a movie scene. */
UCLASS()
class MOVIESCENE_API UMovieSceneFolder : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Gets the name of this folder. */
	FName GetFolderName() const;

	/** Sets the name of this folder. */
	void SetFolderName( FName InFolderName );

	/** Gets the folders contained by this folder. */
	const TArray<UMovieSceneFolder*>& GetChildFolders() const;

	/** Adds a child folder to this folder. */
	void AddChildFolder( UMovieSceneFolder* InChildFolder );

	/** Removes a child folder from this folder. */
	void RemoveChildFolder( UMovieSceneFolder* InChildFolder );

	/** Gets the master tracks contained by this folder. */
	const TArray<UMovieSceneTrack*>& GetChildMasterTracks() const;

	/** Adds a master track to this folder. */
	void AddChildMasterTrack( UMovieSceneTrack* InMasterTrack );

	/** Removes a master track from this folder. */
	void RemoveChildMasterTrack( UMovieSceneTrack* InMasterTrack );

	/** Gets the guids for the object bindings contained by this folder. */
	const TArray<FGuid>& GetChildObjectBindings() const;

	/** Adds a guid for an object binding to this folder. */
	void AddChildObjectBinding(const FGuid& InObjectBinding );

	/** Removes a guid for an object binding from this folder. */
	void RemoveChildObjectBinding( const FGuid& InObjectBinding );


	virtual void PreSave() override;
	virtual void PostLoad() override;


private:
	/** The name of this folder. */
	UPROPERTY()
	FName FolderName;

	/** The folders contained by this folder. */
	UPROPERTY()
	TArray<UMovieSceneFolder*> ChildFolders;

	/** The master tracks contained by this folder. */
	UPROPERTY()
	TArray<UMovieSceneTrack*> ChildMasterTracks;

	/** The guid strings used to serialize the guids for the object bindings contained by this folder. */
	UPROPERTY()
	TArray<FString> ChildObjectBindingStrings;

	/** The guids for the object bindings contained by this folder. */
	TArray<FGuid> ChildObjectBindings;
};

