// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieScenePrivatePCH.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneCommonHelpers.h"


TArray<UMovieSceneSection*> MovieSceneHelpers::GetTraversedSections( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime )
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

void MovieSceneHelpers::SetRuntimeObjectMobility(UObject* Object, EComponentMobility::Type ComponentMobility)
{
	USceneComponent* SceneComponent = SceneComponentFromRuntimeObject(Object);
	if (SceneComponent)
	{
		if (SceneComponent->Mobility != ComponentMobility)
		{
			SceneComponent->Modify();

			SceneComponent->SetMobility(ComponentMobility);
		}
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


void FTrackInstancePropertyBindings::UpdateBindings( const TArray<UObject*>& InRuntimeObjects )
{
	for(UObject* Object : InRuntimeObjects)
	{
		FPropertyAndFunction PropAndFunction;

		PropAndFunction.Function = Object->FindFunction(FunctionName);
		PropAndFunction.PropertyAddress = FindProperty( Object, PropertyPath );
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
