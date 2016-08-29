// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneMaterialTrack.h"
#include "MovieSceneMaterialTrackInstance.h"

FMovieSceneMaterialTrackInstance::FMovieSceneMaterialTrackInstance( UMovieSceneMaterialTrack& InMaterialTrack )
{
	MaterialTrack = &InMaterialTrack;
}

void FMovieSceneMaterialTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (RuntimeObjectToOriginalMaterialMap.Find(FObjectKey(Object)) == nullptr)
		{
			UMaterialInterface* CurrentMaterial = GetMaterialForObject(Object);
			if ( CurrentMaterial != nullptr )
			{
				UMaterialInterface* OriginalMaterial = nullptr;
				if ( CurrentMaterial->IsA( UMaterialInstanceDynamic::StaticClass() ) )
				{
					// If the current material is a dynamic material see if we can find the original material.
					TWeakObjectPtr<UMaterialInterface>* OriginalMaterialPtr = DynamicMaterialToOriginalMaterialMap.Find( FObjectKey( CurrentMaterial ) );
					if ( OriginalMaterialPtr != nullptr )
					{
						OriginalMaterial = OriginalMaterialPtr->Get();
					}
				}
				else
				{
					// If the material isn't dynamic than the original material is the current material.
					OriginalMaterial = CurrentMaterial;
				}

				if ( OriginalMaterial != nullptr )
				{
					RuntimeObjectToOriginalMaterialMap.Add( FObjectKey( Object ), OriginalMaterial );
				}
			}
		}
	}
}

void FMovieSceneMaterialTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

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

void FMovieSceneMaterialTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) 
{
	TArray<FScalarParameterNameAndValue> ScalarValues;
	TArray<FColorParameterNameAndValue> VectorValues;
	MaterialTrack->Eval( UpdateData.Position, ScalarValues, VectorValues );

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
			for ( const FColorParameterNameAndValue& VectorValue : VectorValues )
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

void FMovieSceneMaterialTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	DynamicMaterialInstances.Empty();

	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();
		UMaterialInterface* Material = GetMaterialForObject( Object );

		if ( Material != nullptr)
		{
			UMaterialInstanceDynamic* DynamicMaterialInstance = Cast<UMaterialInstanceDynamic>( Material );

			if ( DynamicMaterialInstance == nullptr )
			{
				DynamicMaterialInstance = UMaterialInstanceDynamic::Create( Material, Object, FName( *( Material->GetName() + "_Animated" ) ) );
				SetMaterialForObject( Object, DynamicMaterialInstance );
				DynamicMaterialToOriginalMaterialMap.Add( FObjectKey( DynamicMaterialInstance ), Material );
			}

			DynamicMaterialInstances.Add( DynamicMaterialInstance );
		}
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
