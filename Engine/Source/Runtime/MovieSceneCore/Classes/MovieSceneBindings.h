// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneBindings.generated.h"


/**
 * MovieSceneBoundObject connects an object to a "possessable" slot in the MovieScene asset
 */
USTRUCT()
struct FMovieSceneBoundObject
{
	GENERATED_USTRUCT_BODY( FMovieSceneBoundObject )

public:

	/** FMovieSceneBoundObject default constructor */
	FMovieSceneBoundObject()
	{
	}

	/**
	 * FMovieSceneBoundObject initialization constructor.  Binds the specified possessable from a MovieScene asset, to the specified object
	 *
	 * @param	InitPossessableGuid		The Guid of the possessable from the MovieScene asset to bind to
	 * @param	InitObjects				The objects to bind
	 */
	FMovieSceneBoundObject( const FGuid& InitPosessableGuid, const TArray< UObject* >& InitObjects );

	/** @return Returns the guid of the possessable that we're bound to */
	const FGuid& GetPossessableGuid() const
	{
		return PossessableGuid;
	}

	/** @return Returns the objects we're bound to */
	TArray< UObject* >& GetObjects()
	{
		return Objects;
	}

	/** @return Returns the objects we're bound to */
	const TArray< UObject* >& GetObjects() const
	{
		return Objects;
	}

	/**
	 * Returns whether or not an object is bound
	 *
	 * @param Object	The object to search for
	 * @return true if the passed in object is bound, false otherwise 
	 */
	bool ContainsObject( UObject* Object )
	{
		return Objects.Contains( Object );	
	}

	/**
	 * Sets the actual objects bound to this slot.  Remember to call Modify() on the owner object if you change this!
	 * 
	 * @param	NewObjects	The list of new bound object (can be NULL)
	 */
	void SetObjects( TArray< UObject* > NewObjects )
	{
		Objects = NewObjects;
	}

private:

	/** Guid of the possessable in the MovieScene that we're binding to.  Note that we always want this GUID to be preserved
	    when the object is duplicated (copy should have same relationship with possessed object.) */
	UPROPERTY()
	FGuid PossessableGuid;

	/** The actual bound objects (we support binding multiple objects to the same possessable) */
	UPROPERTY()
	TArray<	UObject* > Objects;

};


/**
 * MovieSceneBindings.  Contains the relationship between MovieScene possessables and actual objects being possessed.
 */
UCLASS( MinimalAPI )
class UMovieSceneBindings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Associates a MovieScene asset with these bindings
	 *
	 * @param	NewMovieScene	The MovieScene asset to use (can be NULL to clear it)
	 */
	virtual void SetRootMovieScene( class UMovieScene* NewMovieScene );

	/** @return	Returns the root MovieScene associated with these bindings (or NULL, if no MovieScene is associated yet) */
	virtual class UMovieScene* GetRootMovieScene();

	/**
	 * Adds a binding between the specified possessable from a MovieScene asset, to the specified objects
	 *
	 * @param	PossessableGuid		The Guid of the possessable from the MovieScene asset to bind to
	 * @param	Objects				The objects to bind
	 *
	 * @return	Reference to the wrapper structure for this bound object
	 */
	virtual FMovieSceneBoundObject& AddBinding( const FGuid& PosessableGuid, const TArray< UObject* >& Objects );

	/**
	 * Given a guid for a possessable, attempts to find the object bound to that guid
	 *
	 * @param	Guid	The possessable guid to search for
	 *
	 * @return	The bound object, if found, otherwise NULL
	 */
	virtual TArray< UObject* > FindBoundObjects( const FGuid& Guid ) const;

	/**
	 * Gets all bound objects
	 *
	 * @return	List of all bound objects.
	 */
	virtual TArray< FMovieSceneBoundObject >& GetBoundObjects();


private:

	/** The Root MovieScene asset to play */
	UPROPERTY()
	class UMovieScene* RootMovieScene;

	/** List of all of the objects that we're bound to */
	UPROPERTY()
	TArray< FMovieSceneBoundObject > BoundObjects;
};


