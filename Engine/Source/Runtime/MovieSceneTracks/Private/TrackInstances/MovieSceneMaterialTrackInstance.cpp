// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneMaterialTrack.h"
#include "MovieSceneMaterialTrackInstance.h"

FMovieSceneMaterialTrackInstance::FMovieSceneMaterialTrackInstance( UMovieSceneMaterialTrack& InMaterialTrack )
{
	MaterialTrack = &InMaterialTrack;
}

void FMovieSceneMaterialTrackInstance::SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for( UObject* Object : RuntimeObjects )
	{
		if (RuntimeObjectToOriginalMaterialMap.Find(FObjectKey(Object)) == nullptr)
		{
			UMaterialInterface* CurrentMaterial = GetMaterialForObject(Object);
			if ( CurrentMaterial->IsA( UMaterialInstanceDynamic::StaticClass() ) )
			{
				TWeakObjectPtr<UMaterialInterface>* OriginalMaterial = DynamicMaterialToOriginalMaterialMap.Find( FObjectKey( CurrentMaterial ) );
				if ( OriginalMaterial != nullptr )
				{
					CurrentMaterial = OriginalMaterial->Get();
				}
				else
				{
					CurrentMaterial = nullptr;
				}
			}
			if (CurrentMaterial != nullptr)
			{
				RuntimeObjectToOriginalMaterialMap.Add( FObjectKey(Object), CurrentMaterial );
			}
		}
	}
}

void FMovieSceneMaterialTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for( UObject* Object : RuntimeObjects )
	{
		if (!IsValid(Object))
		{
			continue;
		}

		TWeakObjectPtr<UMaterialInterface>* OriginalMaterial = RuntimeObjectToOriginalMaterialMap.Find( FObjectKey( Object ) );
		if ( OriginalMaterial != nullptr && OriginalMaterial->IsValid() )
		{
			SetMaterialForObject(Object, OriginalMaterial->Get());
		}
	}
}

void FMovieSceneMaterialTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) 
{
	TArray<FScalarParameterNameAndValue> ScalarValues;
	TArray<FVectorParameterNameAndValue> VectorValues;
	MaterialTrack->Eval( Position, ScalarValues, VectorValues );

	// Iterate from back to front to allow for fast remove of invalid weak pointers.
	for ( int32 i = DynamicMaterialInstances.Num() - 1; i >= 0; i--)
	{
		if ( DynamicMaterialInstances[i].IsValid() )
		{
			UMaterialInstanceDynamic* DynamicMaterialInstance = DynamicMaterialInstances[i].Get();
			for ( const FScalarParameterNameAndValue& ScalarValue : ScalarValues )
			{
				DynamicMaterialInstance->SetScalarParameterValue( ScalarValue.ParameterName, ScalarValue.Value );
			}
			for ( const FVectorParameterNameAndValue& VectorValue : VectorValues )
			{
				DynamicMaterialInstance->SetVectorParameterValue( VectorValue.ParameterName, VectorValue.Value );
			}
		}
		else
		{
			DynamicMaterialInstances.RemoveAtSwap(i);
		}
	}
}

void FMovieSceneMaterialTrackInstance::RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	DynamicMaterialInstances.Empty();
	for ( UObject* Object : RuntimeObjects )
	{
		UMaterialInterface* Material = GetMaterialForObject( Object );
		UMaterialInstanceDynamic* DynamicMaterialInstance = Cast<UMaterialInstanceDynamic>(Material);
		if ( DynamicMaterialInstance == nullptr )
		{
			DynamicMaterialInstance = UMaterialInstanceDynamic::Create(Material, Object);
			SetMaterialForObject( Object, DynamicMaterialInstance );
			DynamicMaterialToOriginalMaterialMap.Add( FObjectKey( DynamicMaterialInstance ), Material );
		}
		DynamicMaterialInstances.Add( DynamicMaterialInstance );
	}
}

void FMovieSceneMaterialTrackInstance::ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	DynamicMaterialInstances.Empty();
	DynamicMaterialToOriginalMaterialMap.Empty();
	RuntimeObjectToOriginalMaterialMap.Empty();
}

FMovieSceneComponentMaterialTrackInstance::FMovieSceneComponentMaterialTrackInstance( UMovieSceneComponentMaterialTrack& InMaterialTrack )
	: FMovieSceneMaterialTrackInstance( InMaterialTrack )
	, MaterialIndex( InMaterialTrack.GetMaterialIndex() )
{
}

UMaterialInterface* FMovieSceneComponentMaterialTrackInstance::GetMaterialForObject( UObject* Object ) const 
{
	UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);
	if ( Component != nullptr )
	{
		return Component->GetMaterial( MaterialIndex );
	}
	return nullptr;
}

void FMovieSceneComponentMaterialTrackInstance::SetMaterialForObject( UObject* Object, UMaterialInterface* Material )
{
	checkf(Object->IsA(UPrimitiveComponent::StaticClass()), TEXT("Unsupported object type."));
	UPrimitiveComponent* Component = Cast<UPrimitiveComponent>( Object );
	Component->SetMaterial(MaterialIndex, Material);
}
