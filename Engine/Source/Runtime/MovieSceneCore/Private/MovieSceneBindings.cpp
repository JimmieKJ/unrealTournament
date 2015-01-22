// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCorePrivatePCH.h"
#include "MovieSceneBindings.h"

FMovieSceneBoundObject::FMovieSceneBoundObject( const FGuid& InitPosessableGuid, const TArray< UObject* >& InitObjects )
	: PossessableGuid( InitPosessableGuid ),
	  Objects( InitObjects )
{
}



UMovieSceneBindings::UMovieSceneBindings( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

	RootMovieScene = NULL;
}



UMovieScene* UMovieSceneBindings::GetRootMovieScene()
{
	return RootMovieScene;
}


void UMovieSceneBindings::SetRootMovieScene( class UMovieScene* NewMovieScene )
{
	if( NewMovieScene != RootMovieScene )
	{
		Modify();
		RootMovieScene = NewMovieScene;
	}
}


FMovieSceneBoundObject& UMovieSceneBindings::AddBinding( const FGuid& PosessableGuid, const TArray< UObject* >& Objects )
{
	Modify();
	BoundObjects.Add( FMovieSceneBoundObject( PosessableGuid, Objects ) );
	return BoundObjects.Last();
}


TArray< UObject* > UMovieSceneBindings::FindBoundObjects( const FGuid& Guid ) const
{
	for( TArray< FMovieSceneBoundObject >::TConstIterator BoundObjectIter( BoundObjects.CreateConstIterator() ); BoundObjectIter; ++BoundObjectIter )
	{
		const FMovieSceneBoundObject& BoundObject = *BoundObjectIter;

		if( BoundObject.GetPossessableGuid() == Guid )
		{
			return BoundObject.GetObjects();
		}
	}

	return TArray< UObject* >();
}


TArray< FMovieSceneBoundObject >& UMovieSceneBindings::GetBoundObjects()
{
	return BoundObjects;
}
