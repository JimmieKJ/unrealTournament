// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneActorReferenceSection.h"


UMovieSceneActorReferenceSection::UMovieSceneActorReferenceSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


FGuid UMovieSceneActorReferenceSection::Eval( float Position ) const
{
	int32 ActorGuidIndex = ActorGuidIndexCurve.Evaluate( Position );
	return ActorGuidIndex >= 0 && ActorGuidIndex < ActorGuids.Num() ? ActorGuids[ActorGuidIndex] : FGuid();
}


void UMovieSceneActorReferenceSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	// Move the curve
	ActorGuidIndexCurve.ShiftCurve( DeltaPosition, KeyHandles );
}


void UMovieSceneActorReferenceSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{	
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	ActorGuidIndexCurve.ScaleCurve( Origin, DilationFactor, KeyHandles );
}


void UMovieSceneActorReferenceSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const
{
	for ( auto It( ActorGuidIndexCurve.GetKeyHandleIterator() ); It; ++It )
	{
		float Time = ActorGuidIndexCurve.GetKeyTime( It.Key() );
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}
}


void UMovieSceneActorReferenceSection::AddKey( float Time, const FGuid& Value, EMovieSceneKeyInterpolation KeyInterpolation )
{
	int32 NewActorGuidIndex = ActorGuids.Add(Value);
	ActorGuidIndexCurve.AddKey(Time, NewActorGuidIndex);
}


bool UMovieSceneActorReferenceSection::NewKeyIsNewData(float Time, const FGuid& Value) const
{
	return Eval(Time) != Value;
}


bool UMovieSceneActorReferenceSection::HasKeys( const FGuid& Value ) const
{
	return ActorGuidIndexCurve.GetNumKeys() > 0;
}


void UMovieSceneActorReferenceSection::SetDefault( const FGuid& Value )
{
	int32 DefaultIndex = ActorGuids.Add( Value );
	ActorGuidIndexCurve.SetDefaultValue( DefaultIndex );
}


void UMovieSceneActorReferenceSection::PreSave()
{
	ActorGuidStrings.Empty();
	for ( const FGuid& ActorGuid : ActorGuids )
	{
		ActorGuidStrings.Add( ActorGuid.ToString() );
	}
	Super::PreSave();
}


void UMovieSceneActorReferenceSection::PostLoad()
{
	Super::PostLoad();
	ActorGuids.Empty();
	for ( const FString& ActorGuidString : ActorGuidStrings )
	{
		FGuid ActorGuid;
		FGuid::Parse( ActorGuidString, ActorGuid );
		ActorGuids.Add( ActorGuid );
	}
}
