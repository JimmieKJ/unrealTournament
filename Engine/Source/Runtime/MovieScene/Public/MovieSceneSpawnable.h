// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSpawnable.generated.h"


/**
 * MovieSceneSpawnable describes an object that can be spawned for this MovieScene
 */
USTRUCT()
struct FMovieSceneSpawnable 
{
	GENERATED_USTRUCT_BODY()

public:

	/** FMovieSceneSpawnable default constructor */
	FMovieSceneSpawnable() { }

/** FMovieSceneSpawnable initialization constructor */
	FMovieSceneSpawnable(const FString& InitName, UClass* InitClass)
		: Guid(FGuid::NewGuid())
		, Name(InitName)
		, GeneratedClass(InitClass)
	{ }

public:

	/**
	 * Get the Blueprint associated with the spawnable object.
	 *
	 * @return Blueprint class
	 * @see GetGuid, GetName
	 */
	UClass* GetClass()
	{
		return GeneratedClass;
	}

	/**
	 * Get the unique identifier of the spawnable object.
	 *
	 * @return Object GUID.
	 * @see GetClass, GetName
	 */
	const FGuid& GetGuid() const
	{
		return Guid;
	}

	/**
	 * Get the name of the spawnable object.
	 *
	 * @return Object name.
	 * @see GetClass, GetGuid
	 */
	const FString& GetName() const
	{
		return Name;
	}

	/**
	 * Report the specified GUID as being an inner possessable dependency for this spawnable
	 *
	 * @param PossessableGuid The guid pertaining to the inner possessable
	 */
	void AddChildPossessable(const FGuid& PossessableGuid)
	{
		ChildPossessables.AddUnique(PossessableGuid);
	}

	/**
	 * Remove the specified GUID from this spawnables list of dependent possessables
	 *
	 * @param PossessableGuid The guid pertaining to the inner possessable
	 */
	void RemoveChildPossessable(const FGuid& PossessableGuid)
	{
		ChildPossessables.Remove(PossessableGuid);
	}

	/**
	 * @return const access to the child possessables set
	 */
	const TArray<FGuid>& GetChildPossessables() const
	{
		return ChildPossessables;
	}

private:

	/** Unique identifier of the spawnable object. */
	// @todo sequencer: Guids need to be handled carefully when the asset is duplicated (or loaded after being copied on disk).
	//					Sometimes we'll need to generate fresh Guids.
	UPROPERTY()
	FGuid Guid;

	/** Name label */
	// @todo sequencer: Should be editor-only probably
	UPROPERTY()
	FString Name;

	/** Data-only blueprint-generated-class for this object */
	// @todo sequencer: Could be weak object ptr here, IF blueprints that are inners are housekept properly without references
	UPROPERTY()
	UClass* GeneratedClass;

	/** Set of GUIDs to possessable object bindings that are bound to an object inside this spawnable */
	// @todo sequencer: This should be a TSet, but they don't duplicate correctly atm
	UPROPERTY()
	TArray<FGuid> ChildPossessables;
};
