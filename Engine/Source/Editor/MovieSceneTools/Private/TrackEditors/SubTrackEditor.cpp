// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneSubSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSubTrack.h"
#include "SubTrackEditor.h"


namespace SubTrackEditorConstants
{
	const float TrackHeight = 50.0f;
}


#define LOCTEXT_NAMESPACE "FSubTrackEditor"


/**
 * A generic implementation for displaying simple property sections.
 */
class FSubSection
	: public ISequencerSection
{
public:

	FSubSection(TSharedPtr<ISequencer> InSequencer, UMovieSceneSection& InSection, const FText& InDisplayName)
		: DisplayName(InDisplayName)
		, SectionObject(*CastChecked<UMovieSceneSubSection>(&InSection))
		, Sequencer(InSequencer)
	{
		SequenceInstance = InSequencer->GetSequenceInstanceForSection(InSection);
	}

public:

	// ISequencerSection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override
	{
		// do nothing
	}

	virtual bool ShouldDrawKeyAreaBackground() const override
	{
		return false;
	}

	virtual FText GetDisplayName() const override
	{
		return GetSectionTitle();
	}

	virtual float GetSectionHeight() const override
	{
		return SubTrackEditorConstants::TrackHeight;
	}

	virtual UMovieSceneSection* GetSectionObject() override
	{
		return &SectionObject;
	}

	virtual FText GetSectionTitle() const override
	{
		return FText::FromString(SectionObject.GetSequence()->GetName());
	}

	virtual int32 OnPaintSection(const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled) const override 
	{
		const float SectionSize = SectionObject.GetTimeSize();

		if (SectionSize <= 0.0f)
		{
			return LayerId;
		}

		const float DrawScale = AllottedGeometry.Size.X / SectionSize;
		const ESlateDrawEffect::Type DrawEffects = bParentEnabled
			? ESlateDrawEffect::None
			: ESlateDrawEffect::DisabledEffect;

		UMovieScene* MovieScene = SectionObject.GetSequence()->GetMovieScene();
		const FFloatRange& PlaybackRange = MovieScene->GetPlaybackRange();

		// add box for the working size
		const float StartOffset = SectionObject.TimeScale * SectionObject.StartOffset;
		const float WorkingStart = -SectionObject.TimeScale * PlaybackRange.GetLowerBoundValue() - StartOffset;
		const float WorkingSize = SectionObject.TimeScale * MovieScene->GetEditorData().WorkingRange.Size<float>();

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(WorkingStart * DrawScale, 0.f),
				FVector2D(WorkingSize * DrawScale, AllottedGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect,
			DrawEffects,
			FColor(220, 120, 120)
		);

		// add dark tint for left out-of-bounds & working range
		if (StartOffset < 0.0f)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(0.0f, 0.f),
					FVector2D(-StartOffset * DrawScale, AllottedGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				SectionClippingRect,
				ESlateDrawEffect::None,
				FLinearColor::Black.CopyWithNewOpacity(0.2f)
			);
		}

		// add green line for playback start
		if (StartOffset <= 0)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(-StartOffset * DrawScale, 0.f),
					FVector2D(1.0f, AllottedGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				SectionClippingRect,
				ESlateDrawEffect::None,
				FColor(32, 128, 32)	// 120, 75, 50 (HSV)
			);
		}

		// add dark tint for left out-of-bounds & working range
		const float PlaybackEnd = SectionObject.TimeScale * PlaybackRange.Size<float>() - StartOffset;

		if (PlaybackEnd < SectionSize)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(PlaybackEnd * DrawScale, 0.f),
					FVector2D((SectionSize - PlaybackEnd) * DrawScale, AllottedGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				SectionClippingRect,
				ESlateDrawEffect::None,
				FLinearColor::Black.CopyWithNewOpacity(0.2f)
			);
		}

		// add red line for playback end
		if (PlaybackEnd <= SectionSize)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(PlaybackEnd * DrawScale, 0.f),
					FVector2D(1.0f, AllottedGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				SectionClippingRect,
				ESlateDrawEffect::None,
				FColor(128, 32, 32)	// 0, 75, 50 (HSV)
			);
		}

		// add number of tracks within the section's sequence
		int32 NumTracks = MovieScene->GetPossessableCount() + MovieScene->GetSpawnableCount() + MovieScene->GetMasterTracks().Num();

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 3,
			AllottedGeometry.ToOffsetPaintGeometry(FVector2D(5.0f, 32.0f)),
			FText::Format(LOCTEXT("NumTracksFormat", "{0} track(s)"), FText::AsNumber(NumTracks)),
			FEditorStyle::GetFontStyle("NormalFont"),
			SectionClippingRect,
			DrawEffects,
			FLinearColor::White
		);

		return LayerId;
	}

	virtual FReply OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent) override
	{
		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			Sequencer.Pin()->FocusSequenceInstance(SequenceInstance.Pin().ToSharedRef());
		}

		return FReply::Handled();
	}

private:

	/** Display name of the section */
	FText DisplayName;

	/** The section we are visualizing */
	UMovieSceneSubSection& SectionObject;

	/** The instance that this section is part of */
	TWeakPtr<FMovieSceneSequenceInstance> SequenceInstance;

	/** Sequencer interface */
	TWeakPtr<ISequencer> Sequencer;
};


/* FSubTrackEditor structors
 *****************************************************************************/

FSubTrackEditor::FSubTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer) 
{ }


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FSubTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	UMovieSceneSequence* RootMovieSceneSequence = GetSequencer()->GetRootMovieSceneSequence();

	if ((RootMovieSceneSequence == nullptr) || (RootMovieSceneSequence->GetClass()->GetName() != TEXT("LevelSequence")))
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddSubTrack", "Sub Track"),
		LOCTEXT("AddSubTooltip", "Adds a new track that can contain other sequences."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Sub"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FSubTrackEditor::HandleAddSubTrackMenuEntryExecute)
		)
	);
}


TSharedRef<ISequencerTrackEditor> FSubTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FSubTrackEditor(InSequencer));
}


TSharedRef<ISequencerSection> FSubTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track)
{
	return MakeShareable(new FSubSection(GetSequencer(), SectionObject, Track.GetDisplayName()));
}


bool FSubTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(Asset);

	if (Sequence == nullptr)
	{
		return false;
	}

	if (CanAddSubSequence(*Sequence))
	{
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSubTrackEditor::HandleSequenceAdded, Sequence));
	}

	return true;
}


bool FSubTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	// We support sub movie scenes
	return Type == UMovieSceneSubTrack::StaticClass();
}


/* FSubTrackEditor callbacks
 *****************************************************************************/

bool FSubTrackEditor::CanAddSubSequence(const UMovieSceneSequence& Sequence) const
{
	// prevent adding ourselves and ensure we have a valid movie scene
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();

	if ((FocusedSequence == nullptr) || (FocusedSequence == &Sequence) || (FocusedSequence->GetMovieScene() == nullptr))
	{
		return false;
	}

	// ensure that the other sequence has a valid movie scene
	UMovieScene* SequenceMovieScene = Sequence.GetMovieScene();

	if (SequenceMovieScene == nullptr)
	{
		return false;
	}

	// make sure we are not contained in the other sequence (circular dependency)
	// @todo sequencer: this check is not sufficient (does not prevent circular dependencies of 2+ levels)
	UMovieSceneSubTrack* SequenceSubTrack = SequenceMovieScene->FindMasterTrack<UMovieSceneSubTrack>();

	if (SequenceSubTrack == nullptr)
	{
		return true;
	}

	return !SequenceSubTrack->ContainsSequence(*FocusedSequence, true);
}


/* FSubTrackEditor callbacks
 *****************************************************************************/

void FSubTrackEditor::HandleAddSubTrackMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddSubTrack_Transaction", "Add Sub Track"));
	FocusedMovieScene->Modify();

	auto NewTrack = FocusedMovieScene->AddMasterTrack<UMovieSceneSubTrack>();
	ensure(NewTrack);

	GetSequencer()->NotifyMovieSceneDataChanged();
}


bool FSubTrackEditor::HandleSequenceAdded(float KeyTime, UMovieSceneSequence* Sequence)
{
	auto SubTrack = FindOrCreateMasterTrack<UMovieSceneSubTrack>().Track;
	SubTrack->AddSequence(*Sequence, KeyTime);
	SubTrack->SetDisplayName(LOCTEXT("SubTrackName", "Sequences"));

	return true;
}


#undef LOCTEXT_NAMESPACE
