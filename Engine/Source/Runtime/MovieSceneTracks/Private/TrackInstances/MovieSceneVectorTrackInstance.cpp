// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneVectorTrackInstance.h"
#include "MovieSceneVectorTrack.h"


FMovieSceneVectorTrackInstance::FMovieSceneVectorTrackInstance( UMovieSceneVectorTrack& InVectorTrack )
{
	VectorTrack = &InVectorTrack;
	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( VectorTrack->GetPropertyName(), VectorTrack->GetPropertyPath() ) );
}


void FMovieSceneVectorTrackInstance::SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	int32 NumChannelsUsed = VectorTrack->GetNumChannelsUsed();
	for( UObject* Object : RuntimeObjects )
	{
		switch( NumChannelsUsed )
		{
			case 2:
			{
				if (InitVector2DMap.Find(Object) == nullptr)
				{
					FVector2D VectorValue = PropertyBindings->GetCurrentValue<FVector2D>(Object);
					InitVector2DMap.Add(Object, VectorValue);
				}
				break;
			}
			case 3:
			{
				if (InitVector3DMap.Find(Object) == nullptr)
				{
					FVector VectorValue = PropertyBindings->GetCurrentValue<FVector>(Object);
					InitVector3DMap.Add(Object, VectorValue);
				}
				break;
			}
			case 4:
			{
				if (InitVector4DMap.Find(Object) == nullptr)
				{
					FVector4 VectorValue = PropertyBindings->GetCurrentValue<FVector4>(Object);
					InitVector4DMap.Add(Object, VectorValue);
				}
				break;
			}
		}
	}
}


void FMovieSceneVectorTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	int32 NumChannelsUsed = VectorTrack->GetNumChannelsUsed();
	for( UObject* Object : RuntimeObjects )
	{
		if (!IsValid(Object))
		{
			continue;
		}

		switch( NumChannelsUsed )
		{
			case 2:
			{
				FVector2D *VectorValue = InitVector2DMap.Find(Object);
				if (VectorValue != nullptr)
				{
					PropertyBindings->CallFunction<FVector2D>(Object, VectorValue);
				}
				break;
			}
			case 3:
			{
				FVector *VectorValue = InitVector3DMap.Find(Object);
				if (VectorValue != nullptr)
				{
					PropertyBindings->CallFunction<FVector2D>(Object, VectorValue);
				}
				break;
			}
			case 4:
			{
				FVector4 *VectorValue = InitVector4DMap.Find(Object);
				if (VectorValue != nullptr)
				{
					PropertyBindings->CallFunction<FVector2D>(Object, VectorValue);
				}
				break;
			}
		}
	}

	PropertyBindings->UpdateBindings( RuntimeObjects );
}


void FMovieSceneVectorTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) 
{
	FVector4 Vector;
	if( VectorTrack->Eval( Position, LastPosition, Vector ) )
	{
		int32 NumChannelsUsed = VectorTrack->GetNumChannelsUsed();
		switch( NumChannelsUsed )
		{
			case 2:
			{
				FVector2D Value(Vector.X, Vector.Y);
				for(UObject* Object : RuntimeObjects)
				{
					PropertyBindings->CallFunction<FVector2D>(Object, &Value);
				}
				break;
			}
			case 3:
			{
				FVector Value(Vector.X, Vector.Y, Vector.Z);
				for(UObject* Object : RuntimeObjects)
				{
					PropertyBindings->CallFunction<FVector>(Object, &Value);
				}
				break;
			}
			case 4:
			{
				for(UObject* Object : RuntimeObjects)
				{
					PropertyBindings->CallFunction<FVector4>(Object, &Vector);
				}
				break;
			}
			default:
				UE_LOG(LogMovieScene, Warning, TEXT("Invalid number of channels(%d) for vector track"), NumChannelsUsed );
				break;
		}
		
	}
}


void FMovieSceneVectorTrackInstance::RefreshInstance(const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	PropertyBindings->UpdateBindings(RuntimeObjects);
}
