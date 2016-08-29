// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieScenePrivatePCH.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneCommonHelpers.h"


TArray<UMovieSceneSection*> MovieSceneHelpers::GetAllTraversedSections( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime )
{
	TArray<UMovieSceneSection*> TraversedSections;

	bool bPlayingBackwards = CurrentTime - PreviousTime < 0.0f;
	float MaxTime = bPlayingBackwards ? PreviousTime : CurrentTime;
	float MinTime = bPlayingBackwards ? CurrentTime : PreviousTime;

	TRange<float> TraversedRange(MinTime, TRangeBound<float>::Inclusive(MaxTime));

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* Section = Sections[SectionIndex];
		if ((Section->GetStartTime() == CurrentTime) || TraversedRange.Overlaps(TRange<float>(Section->GetRange())))
		{
			TraversedSections.Add(Section);
		}
	}

	return TraversedSections;
}

TArray<UMovieSceneSection*> MovieSceneHelpers::GetTraversedSections( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime )
{
	TArray<UMovieSceneSection*> TraversedSections = GetAllTraversedSections(Sections, CurrentTime, PreviousTime);

	// Remove any overlaps that are underneath another
	for (int32 RemoveAt = 0; RemoveAt < TraversedSections.Num(); )
	{
		UMovieSceneSection* Section = TraversedSections[RemoveAt];
		
		const bool bShouldRemove = TraversedSections.ContainsByPredicate([=](UMovieSceneSection* OtherSection){
			if (Section->GetRowIndex() == OtherSection->GetRowIndex() &&
				Section->GetRange().Overlaps(OtherSection->GetRange()) &&
				Section->GetOverlapPriority() < OtherSection->GetOverlapPriority())
			{
				return true;
			}
			return false;
		});
		
		if (bShouldRemove)
		{
			TraversedSections.RemoveAt(RemoveAt, 1, false);
		}
		else
		{
			++RemoveAt;
		}
	}

	return TraversedSections;
}

UMovieSceneSection* MovieSceneHelpers::FindSectionAtTime( const TArray<UMovieSceneSection*>& Sections, float Time )
{
	for( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		//@todo sequencer: There can be multiple sections overlapping in time. Returning instantly does not account for that.
		if( Section->IsTimeWithinSection( Time ) && Section->IsActive() )
		{
			return Section;
		}
	}

	return nullptr;
}


UMovieSceneSection* MovieSceneHelpers::FindNearestSectionAtTime( const TArray<UMovieSceneSection*>& Sections, float Time )
{
	// Only update the section if the position is within the time span of the section
	// Or if there are no sections at the time, the left closest section to the time
	// Or in the case that Time is before all sections, take the section with the earliest start time
	UMovieSceneSection* ClosestSection = nullptr;
	float ClosestSectionTime = 0.f;
	UMovieSceneSection* EarliestSection = nullptr;
	float EarliestSectionTime = 0.f;
	for( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* Section = Sections[SectionIndex];
		checkSlow(Section);

		if (Section->IsActive())
		{
			//@todo sequencer: There can be multiple sections overlapping in time. Returning instantly does not account for that.
			if( Section->IsTimeWithinSection( Time ) )
			{
				return Section;
			}

			float EndTime = Section->GetEndTime();
			if (EndTime < Time)
			{
				float ClosestTime = Time - EndTime;
				if (!ClosestSection || ClosestTime < ClosestSectionTime)
				{
					ClosestSection = Section;
					ClosestSectionTime = ClosestTime;
				}
			}

			float StartTime = Section->GetStartTime();
			if (!EarliestSection || StartTime < EarliestSectionTime)
			{
				EarliestSection = Section;
				EarliestSectionTime = StartTime;
			}
		}
	}

	// if we get here, we are off of any section
	// if ClosestSection, then we take the closest to left of this time
	// else, we take the EarliestSection
	// if that's nullptr, then there are no sections
	return ClosestSection ? ClosestSection : EarliestSection;
}


void MovieSceneHelpers::SortConsecutiveSections(TArray<UMovieSceneSection*>& Sections)
{
	Sections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B)
		{
			return A.GetStartTime() < B.GetStartTime();
		}
	);
}

void MovieSceneHelpers::FixupConsecutiveSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete)
{
	// Find the previous section and extend it to take the place of the section being deleted
	int32 SectionIndex = INDEX_NONE;

	if (Sections.Find(&Section, SectionIndex))
	{
		int32 PrevSectionIndex = SectionIndex - 1;
		if( Sections.IsValidIndex( PrevSectionIndex ) )
		{
			// Extend the previous section
			Sections[PrevSectionIndex]->SetEndTime( bDelete ? Section.GetEndTime() : Section.GetStartTime() );
		}

		if( !bDelete )
		{
			int32 NextSectionIndex = SectionIndex + 1;
			if(Sections.IsValidIndex(NextSectionIndex))
			{
				// Shift the next CameraCut's start time so that it starts when the new CameraCut ends
				Sections[NextSectionIndex]->SetStartTime(Section.GetEndTime());
			}
		}
	}

	SortConsecutiveSections(Sections);
}



USceneComponent* MovieSceneHelpers::SceneComponentFromRuntimeObject(UObject* Object)
{
	AActor* Actor = Cast<AActor>(Object);

	USceneComponent* SceneComponent = nullptr;
	if (Actor && Actor->GetRootComponent())
	{
		// If there is an actor, modify its root component
		SceneComponent = Actor->GetRootComponent();
	}
	else
	{
		// No actor was found.  Attempt to get the object as a component in the case that we are editing them directly.
		SceneComponent = Cast<USceneComponent>(Object);
	}
	return SceneComponent;
}

UCameraComponent* MovieSceneHelpers::CameraComponentFromActor(const AActor* InActor)
{
	TArray<UCameraComponent*> CameraComponents;
	InActor->GetComponents<UCameraComponent>(CameraComponents);

	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		if (CameraComponent->bIsActive)
		{
			return CameraComponent;
		}
	}

	// now see if any actors are attached to us, directly or indirectly, that have an active camera component we might want to use
	// we will just return the first one.
	// #note: assumption here that attachment cannot be circular
	TArray<AActor*> AttachedActors;
	InActor->GetAttachedActors(AttachedActors);
	for (AActor* AttachedActor : AttachedActors)
	{
		UCameraComponent* const Comp = CameraComponentFromActor(AttachedActor);
		if (Comp)
		{
			return Comp;
		}
	}

	return nullptr;
}

UCameraComponent* MovieSceneHelpers::CameraComponentFromRuntimeObject(UObject* RuntimeObject)
{
	if (RuntimeObject)
	{
		// find camera we want to control
		UCameraComponent* const CameraComponent = dynamic_cast<UCameraComponent*>(RuntimeObject);
		if (CameraComponent)
		{
			return CameraComponent;
		}

		// see if it's an actor that has a camera component
		AActor* const Actor = dynamic_cast<AActor*>(RuntimeObject);
		if (Actor)
		{
			return CameraComponentFromActor(Actor);
		}
	}

	return nullptr;
}


void MovieSceneHelpers::SetKeyInterpolation(FRichCurve& InCurve, FKeyHandle InKeyHandle, EMovieSceneKeyInterpolation InKeyInterpolation)
{
	switch (InKeyInterpolation)
	{
		case EMovieSceneKeyInterpolation::Auto:
			InCurve.SetKeyInterpMode(InKeyHandle, RCIM_Cubic);
			InCurve.SetKeyTangentMode(InKeyHandle, RCTM_Auto);
			break;

		case EMovieSceneKeyInterpolation::User:
			InCurve.SetKeyInterpMode(InKeyHandle, RCIM_Cubic);
			InCurve.SetKeyTangentMode(InKeyHandle, RCTM_User);
			break;

		case EMovieSceneKeyInterpolation::Break:
			InCurve.SetKeyInterpMode(InKeyHandle, RCIM_Cubic);
			InCurve.SetKeyTangentMode(InKeyHandle, RCTM_Break);
			break;

		case EMovieSceneKeyInterpolation::Linear:
			InCurve.SetKeyInterpMode(InKeyHandle, RCIM_Linear);
			InCurve.SetKeyTangentMode(InKeyHandle, RCTM_Auto);
			break;

		case EMovieSceneKeyInterpolation::Constant:
			InCurve.SetKeyInterpMode(InKeyHandle, RCIM_Constant);
			InCurve.SetKeyTangentMode(InKeyHandle, RCTM_Auto);
			break;

		default:
			InCurve.SetKeyInterpMode(InKeyHandle, RCIM_Cubic);
			InCurve.SetKeyTangentMode(InKeyHandle, RCTM_Auto);
			break;
	}
}

FTrackInstancePropertyBindings::FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath, const FName& InFunctionName )
    : PropertyPath( InPropertyPath )
	, PropertyName( InPropertyName )
{
	if (InFunctionName != FName())
	{
		FunctionName = InFunctionName;
	}
	else
	{
		static const FString Set(TEXT("Set"));

		const FString FunctionString = Set + PropertyName.ToString();

		FunctionName = FName(*FunctionString);
	}
}

FTrackInstancePropertyBindings::FPropertyAddress FTrackInstancePropertyBindings::FindPropertyRecursive( const UObject* Object, void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index ) const
{
	UProperty* Property = FindField<UProperty>(InStruct, *InPropertyNames[Index]);
	
	FTrackInstancePropertyBindings::FPropertyAddress NewAddress;

	UStructProperty* StructProp = Cast<UStructProperty>( Property );
	if( StructProp )
	{
		NewAddress.Property = StructProp;
		NewAddress.Address = BasePointer;

		if( InPropertyNames.IsValidIndex(Index+1) )
		{
			void* StructContainer = StructProp->ContainerPtrToValuePtr<void>(BasePointer);
			return FindPropertyRecursive( Object, StructContainer, StructProp->Struct, InPropertyNames, Index+1 );
		}
		else
		{
			check( StructProp->GetName() == InPropertyNames[Index] );
		}
	}
	else if( Property )
	{
		NewAddress.Property = Property;
		NewAddress.Address = BasePointer;
	}

	return NewAddress;

}


FTrackInstancePropertyBindings::FPropertyAddress FTrackInstancePropertyBindings::FindProperty( const UObject* InObject, const FString& InPropertyPath ) const
{
	TArray<FString> PropertyNames;

	InPropertyPath.ParseIntoArray(PropertyNames, TEXT("."), true);

	if( PropertyNames.Num() > 0 )
	{
		return FindPropertyRecursive( InObject, (void*)InObject, InObject->GetClass(), PropertyNames, 0 );
	}
	else
	{
		return FTrackInstancePropertyBindings::FPropertyAddress();
	}

}


void FTrackInstancePropertyBindings::UpdateBindings( const TArray<TWeakObjectPtr<UObject>>& InRuntimeObjects )
{
	for (auto ObjectPtr : InRuntimeObjects)
	{
		UpdateBinding(ObjectPtr);
	}
}

void FTrackInstancePropertyBindings::UpdateBinding(const TWeakObjectPtr<UObject>& InRuntimeObject)
{
	UObject* Object = InRuntimeObject.Get();

	if (Object != nullptr)
	{
		FPropertyAndFunction PropAndFunction;
		{
			PropAndFunction.Function = Object->FindFunction(FunctionName);
			PropAndFunction.PropertyAddress = FindProperty(Object, PropertyPath);
		}

		RuntimeObjectToFunctionMap.Add(Object, PropAndFunction);
	}
}

UProperty* FTrackInstancePropertyBindings::GetProperty(const UObject* Object) const
{
	UProperty* Prop = nullptr;

	FPropertyAndFunction PropAndFunction = RuntimeObjectToFunctionMap.FindRef(Object);
	if( PropAndFunction.PropertyAddress.Property ) 
	{
		Prop = PropAndFunction.PropertyAddress.Property;
	}
	else
	{
		FPropertyAddress PropertyAddress = FindProperty( Object, PropertyPath );
		Prop = PropertyAddress.Property;
	}

	return Prop;
}
