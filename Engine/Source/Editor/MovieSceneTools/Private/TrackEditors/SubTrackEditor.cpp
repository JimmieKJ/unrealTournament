// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneSubSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSubTrack.h"
#include "MovieSceneCinematicShotTrack.h"
#include "SubTrackEditor.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"
#include "SequencerSectionPainter.h"
#include "ISequenceRecorder.h"
#include "SequenceRecorderSettings.h"

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
		if (InSequencer->HasSequenceInstanceForSection(InSection))
		{
			SequenceInstance = InSequencer->GetSequenceInstanceForSection(InSection);
		}
	}

public:

	// ISequencerSection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override
	{
		// do nothing
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
		if(SectionObject.GetSequence())
		{
			return FText::FromString(SectionObject.GetSequence()->GetName());
		}
		else if(UMovieSceneSubSection::GetRecordingSection() == &SectionObject)
		{
			AActor* ActorToRecord = UMovieSceneSubSection::GetActorToRecord();

			ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
			if(SequenceRecorder.IsRecording())
			{
				if(ActorToRecord != nullptr)
				{
					return FText::Format(LOCTEXT("RecordingIndicatorWithActor", "Sequence Recording for \"{0}\""), FText::FromString(ActorToRecord->GetActorLabel()));
				}
				else
				{
					return LOCTEXT("RecordingIndicator", "Sequence Recording");
				}
			}
			else
			{
				if(ActorToRecord != nullptr)
				{
					return FText::Format(LOCTEXT("RecordingPendingIndicatorWithActor", "Sequence Recording Pending for \"{0}\""), FText::FromString(ActorToRecord->GetActorLabel()));
				}
				else
				{
					return LOCTEXT("RecordingPendingIndicator", "Sequence Recording Pending");
				}
			}
		}
		else
		{
			return LOCTEXT("InvalidSequence", "No Sequence Selected");
		}
	}
	
	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override
	{
		int32 LayerId = InPainter.PaintSectionBackground();

		const float SectionSize = SectionObject.GetTimeSize();

		if (SectionSize <= 0.0f)
		{
			return LayerId;
		}

		const float DrawScale = InPainter.SectionGeometry.Size.X / SectionSize;
		const ESlateDrawEffect::Type DrawEffects = InPainter.bParentEnabled
			? ESlateDrawEffect::None
			: ESlateDrawEffect::DisabledEffect;

		UMovieScene* MovieScene = nullptr;
		FFloatRange PlaybackRange;
		if(SectionObject.GetSequence() != nullptr)
		{
			MovieScene = SectionObject.GetSequence()->GetMovieScene();
			PlaybackRange = MovieScene->GetPlaybackRange();
		}
		else
		{
			UMovieSceneTrack* MovieSceneTrack = CastChecked<UMovieSceneTrack>(SectionObject.GetOuter());
			MovieScene = CastChecked<UMovieScene>(MovieSceneTrack->GetOuter());
			PlaybackRange = MovieScene->GetPlaybackRange();
		}

		// add box for the working size
		const float StartOffset = SectionObject.TimeScale * SectionObject.StartOffset;
		const float WorkingStart = -SectionObject.TimeScale * PlaybackRange.GetLowerBoundValue() - StartOffset;
		const float WorkingSize = SectionObject.TimeScale * (MovieScene != nullptr ? MovieScene->GetEditorData().WorkingRange.Size<float>() : 1.0f);
		
		if(UMovieSceneSubSection::GetRecordingSection() == &SectionObject)
		{
			FColor SubSectionColor = FColor(180, 75, 75, 190);
	
			ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
			if(SequenceRecorder.IsRecording())
			{
				SubSectionColor = FColor(200, 10, 10, 190);
			}

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(WorkingStart * DrawScale, 0.f),
					FVector2D(WorkingSize * DrawScale, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("Sequencer.Section.BackgroundTint"),
				InPainter.SectionClippingRect,
				DrawEffects,
				SubSectionColor
			);
		}

		// add dark tint for left out-of-bounds & working range
		if (StartOffset < 0.0f)
		{
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(0.0f, 0.f),
					FVector2D(-StartOffset * DrawScale, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				InPainter.SectionClippingRect,
				ESlateDrawEffect::None,
				FLinearColor::Black.CopyWithNewOpacity(0.2f)
			);
		}

		// add green line for playback start
		if (StartOffset <= 0)
		{
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(-StartOffset * DrawScale, 0.f),
					FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				InPainter.SectionClippingRect,
				ESlateDrawEffect::None,
				FColor(32, 128, 32)	// 120, 75, 50 (HSV)
			);
		}

		// add dark tint for left out-of-bounds & working range
		const float PlaybackEnd = SectionObject.TimeScale * PlaybackRange.Size<float>() - StartOffset;

		if (PlaybackEnd < SectionSize)
		{
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(PlaybackEnd * DrawScale, 0.f),
					FVector2D((SectionSize - PlaybackEnd) * DrawScale, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				InPainter.SectionClippingRect,
				ESlateDrawEffect::None,
				FLinearColor::Black.CopyWithNewOpacity(0.2f)
			);
		}

		// add red line for playback end
		if (PlaybackEnd <= SectionSize)
		{
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(PlaybackEnd * DrawScale, 0.f),
					FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				InPainter.SectionClippingRect,
				ESlateDrawEffect::None,
				FColor(128, 32, 32)	// 0, 75, 50 (HSV)
			);
		}

		// add number of tracks within the section's sequence
		if(SectionObject.GetSequence() != nullptr && MovieScene != nullptr)
		{
			int32 NumTracks = MovieScene->GetPossessableCount() + MovieScene->GetSpawnableCount() + MovieScene->GetMasterTracks().Num();

			FSlateDrawElement::MakeText(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToOffsetPaintGeometry(FVector2D(11.0f, 32.0f)),
				FText::Format(LOCTEXT("NumTracksFormat", "{0} track(s)"), FText::AsNumber(NumTracks)),
				FEditorStyle::GetFontStyle("NormalFont"),
				InPainter.SectionClippingRect,
				DrawEffects,
				FColor(200, 200, 200)
			);
		}
		else
		{
			// if we are primed for recording, display where we will create the recording
			FString Path = SectionObject.GetTargetPathToRecordTo() / SectionObject.GetTargetSequenceName();

			FSlateDrawElement::MakeText(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToOffsetPaintGeometry(FVector2D(11.0f, 32.0f)),
				FText::Format(LOCTEXT("RecordingDestination", "Target: \"{0}\""), FText::FromString(Path)),
				FEditorStyle::GetFontStyle("NormalFont"),
				InPainter.SectionClippingRect,
				DrawEffects,
				FColor(200, 200, 200)
			);
		}

		return LayerId;
	}

	virtual FReply OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent) override
	{
		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			if (SectionObject.GetSequence())
			{
				Sequencer.Pin()->FocusSequenceInstance(SectionObject);
			}
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
		LOCTEXT("AddSubTrack", "Subscenes Track"),
		LOCTEXT("AddSubTooltip", "Adds a new track that can contain other sequences."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Sub"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FSubTrackEditor::HandleAddSubTrackMenuEntryExecute)
		)
	);
}

TSharedPtr<SWidget> FSubTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	// Create a container edit box
	return SNew(SHorizontalBox)

	// Add the sub sequence combo box
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("SubText", "Sequence"), FOnGetContent::CreateSP(this, &FSubTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent, Track), Params.NodeIsHovered)
	];
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

	//@todo If there's already a cinematic shot track, allow that track to handle this asset
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene != nullptr && FocusedMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>() != nullptr)
	{
		return false;
	}

	if (CanAddSubSequence(*Sequence))
	{
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSubTrackEditor::HandleSequenceAdded, Sequence));

		return true;
	}

	return false;
}


bool FSubTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	// We support sub movie scenes
	return Type == UMovieSceneSubTrack::StaticClass();
}

const FSlateBrush* FSubTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.Sub");
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

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}

/** Helper function - get the first PIE world (or first PIE client world if there is more than one) */
static UWorld* GetFirstPIEWorld()
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World()->IsPlayInEditor())
		{
			if(Context.World()->GetNetMode() == ENetMode::NM_Standalone ||
				(Context.World()->GetNetMode() == ENetMode::NM_Client && Context.PIEInstance == 2))
			{
				return Context.World();
			}
		}
	}

	return nullptr;
}

TSharedRef<SWidget> FSubTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent(UMovieSceneTrack* InTrack)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(TEXT("RecordSequence"), LOCTEXT("RecordSequence", "Record Sequence"));
	{
		AActor* ActorToRecord = nullptr;
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RecordNewSequence", "Record New Sequence"), 
			LOCTEXT("RecordNewSequence_ToolTip", "Record a new level sequence into this sub-track from gameplay/simulation etc.\nThis only primes the track for recording. Click the record button to begin recording into this track once primed.\nOnly one sequence can be recorded at a time."), 
			FSlateIcon(), 
			FUIAction(
				FExecuteAction::CreateSP(this, &FSubTrackEditor::HandleRecordNewSequence, ActorToRecord, InTrack),
				FCanExecuteAction::CreateSP(this, &FSubTrackEditor::CanRecordNewSequence)));

		if(UWorld* PIEWorld = GetFirstPIEWorld())
		{
			APlayerController* Controller = GEngine->GetFirstLocalPlayerController(PIEWorld);
			if(Controller && Controller->GetPawn())
			{
				ActorToRecord = Controller->GetPawn();
				MenuBuilder.AddMenuEntry(
					LOCTEXT("RecordNewSequenceFromPlayer", "Record New Sequence From Current Player"), 
					LOCTEXT("RecordNewSequenceFromPlayer_ToolTip", "Record a new level sequence into this sub track using the current player's pawn.\nThis only primes the track for recording. Click the record button to begin recording into this track once primed.\nOnly one sequence can be recorded at a time."), 
					FSlateIcon(), 
					FUIAction(
						FExecuteAction::CreateSP(this, &FSubTrackEditor::HandleRecordNewSequence, ActorToRecord, InTrack),
						FCanExecuteAction::CreateSP(this, &FSubTrackEditor::CanRecordNewSequence)));
			}
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("ChooseSequence"), LOCTEXT("ChooseSequence", "Choose Sequence"));
	{
		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw( this, &FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryExecute, InTrack);
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;
			AssetPickerConfig.Filter.ClassNames.Add(TEXT("LevelSequence"));
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TSharedPtr<SBox> MenuEntry = SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(300.f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			];

		MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryExecute(const FAssetData& AssetData, UMovieSceneTrack* InTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UMovieSceneSequence::StaticClass()))
	{
		UMovieSceneSequence* MovieSceneSequence = CastChecked<UMovieSceneSequence>(AssetData.GetAsset());

		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FSubTrackEditor::AddKeyInternal, MovieSceneSequence, InTrack) );
	}
}

bool FSubTrackEditor::AddKeyInternal(float KeyTime, UMovieSceneSequence* InMovieSceneSequence, UMovieSceneTrack* InTrack)
{	
	if (CanAddSubSequence(*InMovieSceneSequence))
	{
		UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(InTrack);
		float Duration = InMovieSceneSequence->GetMovieScene()->GetPlaybackRange().Size<float>();
		SubTrack->AddSequence(InMovieSceneSequence, KeyTime, Duration);

		return true;
	}

	return false;
}

bool FSubTrackEditor::HandleSequenceAdded(float KeyTime, UMovieSceneSequence* Sequence)
{
	auto SubTrack = FindOrCreateMasterTrack<UMovieSceneSubTrack>().Track;
	float Duration = Sequence->GetMovieScene()->GetPlaybackRange().Size<float>();
	SubTrack->AddSequence(Sequence, KeyTime, Duration);

	return true;
}

bool FSubTrackEditor::CanRecordNewSequence() const
{
	return !UMovieSceneSubSection::IsSetAsRecording();
}

void FSubTrackEditor::HandleRecordNewSequence(AActor* InActorToRecord, UMovieSceneTrack* InTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FSubTrackEditor::HandleRecordNewSequenceInternal, InActorToRecord, InTrack) );
}

bool FSubTrackEditor::HandleRecordNewSequenceInternal(float KeyTime, AActor* InActorToRecord, UMovieSceneTrack* InTrack)
{
	UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(InTrack);
	UMovieSceneSubSection* Section = SubTrack->AddSequenceToRecord();

	// @todo: we could default to the same directory as a parent sequence, or the last sequence recorded. Lots of options!
	Section->SetTargetSequenceName(GetDefault<USequenceRecorderSettings>()->SequenceName);
	Section->SetTargetPathToRecordTo(GetDefault<USequenceRecorderSettings>()->SequenceRecordingBasePath.Path);
	Section->SetActorToRecord(InActorToRecord);

	return true;
}

#undef LOCTEXT_NAMESPACE
