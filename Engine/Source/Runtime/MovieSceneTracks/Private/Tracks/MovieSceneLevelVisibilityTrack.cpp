// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneLevelVisibilityTrack.h"
#include "Sections/MovieSceneLevelVisibilitySection.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneLevelVisibilityTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"


#define LOCTEXT_NAMESPACE "MovieSceneLevelVisibilityTrack"

UMovieSceneLevelVisibilityTrack::UMovieSceneLevelVisibilityTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}


void UMovieSceneLevelVisibilityTrack::PostCompile(FMovieSceneEvaluationTrack& OutTrack, const FMovieSceneTrackCompilerArgs& Args) const
{
	// Set priority to highest possible
	OutTrack.SetEvaluationPriority(GetEvaluationPriority());

	FMovieSceneSharedDataId UniqueId = FMovieSceneLevelVisibilitySharedTrack::GetSharedDataKey().UniqueId;

	FMovieSceneEvaluationTrack ActuatorTemplate(Args.ObjectBindingId);
	ActuatorTemplate.DefineAsSingleTemplate(FMovieSceneLevelVisibilitySharedTrack());
	ActuatorTemplate.SetEvaluationPriority(GetEvaluationPriority() - 1);
	
	Args.Generator.AddSharedTrack(MoveTemp(ActuatorTemplate), UniqueId, *this);
}


bool UMovieSceneLevelVisibilityTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}


void UMovieSceneLevelVisibilityTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


void UMovieSceneLevelVisibilityTrack::RemoveSection( UMovieSceneSection& Section )
{
	Sections.Remove(&Section);
}


UMovieSceneSection* UMovieSceneLevelVisibilityTrack::CreateNewSection()
{
	return NewObject<UMovieSceneLevelVisibilitySection>(this, UMovieSceneLevelVisibilitySection::StaticClass(), NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneLevelVisibilityTrack::GetAllSections() const
{
	return Sections;
}


TRange<float> UMovieSceneLevelVisibilityTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		Bounds.Add(Sections[SectionIndex]->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}


bool UMovieSceneLevelVisibilityTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneLevelVisibilityTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("DisplayName", "Level Visibility");
}
#endif

#undef LOCTEXT_NAMESPACE
