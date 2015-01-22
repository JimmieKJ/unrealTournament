// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Tickable.h"

/**
 * This class provides common registration for gamethread editor only tickable objects. It is an
 * abstract base class requiring you to implement the Tick() method.
 */
class FTickableEditorObject : public FTickableObjectBase
{
public:
	static void TickObjects( float DeltaSeconds )
	{
		for( int32 ObjectIndex=0; ObjectIndex < TickableObjects.Num(); ++ObjectIndex)
		{
			FTickableEditorObject* TickableObject = TickableObjects[ObjectIndex];
			if ( TickableObject->IsTickable() )
			{
				TickableObject->Tick(DeltaSeconds);
			}
		}
	}

	/**
	 * Registers this instance with the static array of tickable objects.	
	 */
	FTickableEditorObject()
	{
		TickableObjects.Add( this );
	}

	/**
	 * Removes this instance from the static array of tickable objects.
	 */
	virtual ~FTickableEditorObject()
	{
		const int32 Pos = TickableObjects.Find(this);
		check(Pos!=INDEX_NONE);
		TickableObjects.RemoveAt(Pos);
	}

private:
	/** Static array of tickable objects */
	UNREALED_API static TArray<FTickableEditorObject*> TickableObjects;
};