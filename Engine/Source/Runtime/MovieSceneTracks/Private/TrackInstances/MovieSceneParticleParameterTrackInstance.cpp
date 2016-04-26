// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneParticleParameterTrack.h"
#include "MovieSceneParticleParameterTrackInstance.h"

FMovieSceneParticleParameterTrackInstance::FMovieSceneParticleParameterTrackInstance( UMovieSceneParticleParameterTrack& InParticleParameterTrack )
{
	ParticleParameterTrack = &InParticleParameterTrack;
}

void FMovieSceneParticleParameterTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();
		AEmitter* EmitterActor = Cast<AEmitter>(Object);

		if ( EmitterActor != nullptr )
		{
			UParticleSystemComponent* ParticleSystemComponent = EmitterActor->GetParticleSystemComponent();
			if ( ParticleSystemComponent != nullptr )
			{
				TSharedPtr<FInitialParameterValues> InitialParameterValues;
				TSharedPtr<FInitialParameterValues>* InitialParameterValuesPtr = ObjectToInitialParameterValuesMap.Find( FObjectKey( Object ) );
				if ( InitialParameterValuesPtr == nullptr )
				{
					InitialParameterValues = MakeShareable( new FInitialParameterValues() );
					ObjectToInitialParameterValuesMap.Add( FObjectKey( Object ), InitialParameterValues );
				}
				else
				{
					InitialParameterValues = *InitialParameterValuesPtr;
				}

				const TArray<FParticleSysParam> InstanceParameters = ParticleSystemComponent->GetAsyncInstanceParameters();
				for ( const FParticleSysParam& ParticleSystemParameter : InstanceParameters )
				{
					switch ( ParticleSystemParameter.ParamType )
					{
					case PSPT_Scalar:
						InitialParameterValues->ScalarNameToValueMap.Add( ParticleSystemParameter.Name, ParticleSystemParameter.Scalar );
						break;
					case PSPT_Vector:
						InitialParameterValues->VectorNameToValueMap.Add( ParticleSystemParameter.Name, ParticleSystemParameter.Vector );
						break;
					case PSPT_Color:
						InitialParameterValues->ColorNameToValueMap.Add( ParticleSystemParameter.Name, ParticleSystemParameter.Color );
						break;
					}
				}
			}
		}
	}
}

void FMovieSceneParticleParameterTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();
		TSharedPtr<FInitialParameterValues>* InitialParameterValuesPtr = ObjectToInitialParameterValuesMap.Find(FObjectKey(Object));

		if ( InitialParameterValuesPtr != nullptr )
		{
			TSharedPtr<FInitialParameterValues> InitialParameterValues = *InitialParameterValuesPtr;

			AEmitter* EmitterActor = Cast<AEmitter>( Object );
			if ( EmitterActor != nullptr )
			{
				UParticleSystemComponent* ParticleSystemComponent = EmitterActor->GetParticleSystemComponent();
				if ( ParticleSystemComponent != nullptr )
				{
					const TArray<FParticleSysParam> InstanceParameters = ParticleSystemComponent->GetAsyncInstanceParameters();

					// Set saved parameters back to their initial values, and collect new parameters to be cleared.
					for ( const FParticleSysParam& ParticleSystemParameter : InstanceParameters )
					{
						switch ( ParticleSystemParameter.ParamType )
						{
						case PSPT_Scalar:
						{
							float* InitialFloatValue = InitialParameterValues->ScalarNameToValueMap.Find(ParticleSystemParameter.Name);
							if ( InitialFloatValue != nullptr )
							{
								ParticleSystemComponent->SetFloatParameter(ParticleSystemParameter.Name, *InitialFloatValue);
							}
							break;
						}
						case PSPT_Vector:
						{
							FVector* InitialVectorValue = InitialParameterValues->VectorNameToValueMap.Find( ParticleSystemParameter.Name );
							if ( InitialVectorValue != nullptr )
							{
								ParticleSystemComponent->SetVectorParameter( ParticleSystemParameter.Name, *InitialVectorValue );
							}
							break;
						}
						case PSPT_Color:
						{
							FLinearColor* InitialColorValue = InitialParameterValues->ColorNameToValueMap.Find( ParticleSystemParameter.Name );
							if ( InitialColorValue != nullptr )
							{
								ParticleSystemComponent->SetColorParameter( ParticleSystemParameter.Name, *InitialColorValue );
							}
							break;
						}
						}
					}
				}
			}
		}
	}
	ObjectToInitialParameterValuesMap.Empty();
}

void FMovieSceneParticleParameterTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	TArray<FScalarParameterNameAndValue> ScalarNamesAndValues;
	TArray<FVectorParameterNameAndValue> VectorNamesAndValues;
	TArray<FColorParameterNameAndValue> ColorNamesAndValues;
	ParticleParameterTrack->Eval( UpdateData.Position, ScalarNamesAndValues, VectorNamesAndValues, ColorNamesAndValues);
	for ( UParticleSystemComponent* ParticleSystemComponent : ParticleSystemComponents )
	{
		for ( const FScalarParameterNameAndValue& ScalarNameAndValue : ScalarNamesAndValues )
		{
			ParticleSystemComponent->SetFloatParameter( ScalarNameAndValue.ParameterName, ScalarNameAndValue.Value );
		}
		for ( const FVectorParameterNameAndValue& VectorNameAndValue : VectorNamesAndValues )
		{
			ParticleSystemComponent->SetVectorParameter( VectorNameAndValue.ParameterName, VectorNameAndValue.Value );
		}
		for ( const FColorParameterNameAndValue& ColorNameAndValue : ColorNamesAndValues )
		{
			ParticleSystemComponent->SetColorParameter( ColorNameAndValue.ParameterName, ColorNameAndValue.Value );
		}
	}
}

void FMovieSceneParticleParameterTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	ParticleSystemComponents.Empty();

	// Collection the animated parameter names.
	TSet<FName> UsedParameterNames;
	for ( UMovieSceneSection* Section : ParticleParameterTrack->GetAllSections() )
	{
		UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>(Section);
		ParameterSection->GetParameterNames(UsedParameterNames);
	}

	// Cache the particle components which have matching parameter names.
	for (auto ObjectPtr : RuntimeObjects)
	{
		AEmitter* EmitterActor = Cast<AEmitter>(ObjectPtr.Get());
		if ( EmitterActor != nullptr )
		{
			UParticleSystemComponent* ParticleSystemComponent = EmitterActor->GetParticleSystemComponent();
			if ( ParticleSystemComponent != nullptr )
			{
				const TArray<FParticleSysParam> InstanceParameters = ParticleSystemComponent->GetAsyncInstanceParameters();
				for ( const FParticleSysParam& ParticleSystemParameter : InstanceParameters )
				{
					if ( UsedParameterNames.Contains( ParticleSystemParameter.Name ) )
					{
						ParticleSystemComponents.Add(ParticleSystemComponent);
						break;
					}
				}
			}
		}
	}
}

void FMovieSceneParticleParameterTrackInstance::ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	ParticleSystemComponents.Empty();
	ObjectToInitialParameterValuesMap.Empty();
}
