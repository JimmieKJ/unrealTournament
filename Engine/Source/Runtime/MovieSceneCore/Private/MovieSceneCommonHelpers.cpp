// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCorePrivatePCH.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneCommonHelpers.h"


TArray<UMovieSceneSection*> MovieSceneHelpers::GetTraversedSections( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime )
{
	TArray<UMovieSceneSection*> TraversedSections;

	bool bPlayingBackwards = CurrentTime - PreviousTime < 0.0f;
	
	float MaxTime = bPlayingBackwards ? PreviousTime : CurrentTime;
	float MinTime = bPlayingBackwards ? CurrentTime : PreviousTime;

	TRange<float> SliderRange(MinTime, MaxTime);

	for( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* Section = Sections[SectionIndex];
		if( SliderRange.Overlaps( TRange<float>( Section->GetRange() ) ) )
		{
			TraversedSections.Add( Section );
		}
	}

	return TraversedSections;
}

UMovieSceneSection* MovieSceneHelpers::FindSectionAtTime( const TArray<UMovieSceneSection*>& Sections, float Time )
{
	// Only update the section if the position is within the time span of the section
	// Or if there are no sections at the time, the left closest section to the time
	// Or in the case that Time is before all sections, take the section with the earliest start time
	UMovieSceneSection* ClosestSection = NULL;
	float ClosestSectionTime = 0.f;
	UMovieSceneSection* EarliestSection = NULL;
	float EarliestSectionTime = 0.f;
	for( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		//@todo Sequencer - There can be multiple sections overlapping in time. Returning instantly does not account for that.
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

	// if we get here, we are off of any section
	// if ClosestSection, then we take the closest to left of this time
	// else, we take the EarliestSection
	// if that's NULL, then there are no sections
	return ClosestSection ? ClosestSection : EarliestSection;
}


FTrackInstancePropertyBindings::FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath )
    : PropertyPath( InPropertyPath )
	, PropertyName( InPropertyName )
{
	
	static const FString Set(TEXT("Set"));

	const FString FunctionString = Set + PropertyName.ToString();

	FunctionName = FName(*FunctionString);
}

void FTrackInstancePropertyBindings::CallFunction( UObject* InRuntimeObject, void* FunctionParams )
{
	FPropertyAndFunction PropAndFunction = RuntimeObjectToFunctionMap.FindRef(InRuntimeObject);
	if(PropAndFunction.Function)
	{
		InRuntimeObject->ProcessEvent(PropAndFunction.Function, FunctionParams);
	}
}


FTrackInstancePropertyBindings::FPropertyAddress FTrackInstancePropertyBindings::FindPropertyRecursive( UObject* Object, void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index ) const
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

FTrackInstancePropertyBindings::FPropertyAddress FTrackInstancePropertyBindings::FindProperty( UObject* InObject, const FString& InPropertyPath ) const
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
		if(PropAndFunction.Function)
		{
			PropAndFunction.PropertyAddress = FindProperty( Object, PropertyPath );
			RuntimeObjectToFunctionMap.Add(Object, PropAndFunction);
		}
		else
		{
			// Dont call potentially invalid functions
			RuntimeObjectToFunctionMap.Remove(Object);
		}
	}
}
