// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "CameraCutSection.h"
#include "ISectionLayoutBuilder.h"
#include "Runtime/MovieSceneTracks/Public/Sections/MovieSceneCameraCutSection.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "SInlineEditableTextBlock.h"
#include "MovieSceneToolsUserSettings.h"


#define LOCTEXT_NAMESPACE "FCameraCutSection"


/* FCameraCutSection structors
 *****************************************************************************/

FCameraCutSection::FCameraCutSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection) : FThumbnailSection(InSequencer, InThumbnailPool, InSection)
{
}

FCameraCutSection::~FCameraCutSection()
{
}


/* ISequencerSection interface
 *****************************************************************************/

void FCameraCutSection::SetSingleTime(float GlobalTime)
{
	UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
	if (CameraCutSection)
	{
		CameraCutSection->SetThumbnailReferenceOffset(GlobalTime - CameraCutSection->GetStartTime());
	}
}

void FCameraCutSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
	if (CameraCutSection)
	{
		if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails)
		{
			ThumbnailCache.SetSingleReferenceFrame(CameraCutSection->GetThumbnailReferenceOffset() + CameraCutSection->GetStartTime());
		}
		else
		{
			ThumbnailCache.SetSingleReferenceFrame(TOptional<float>());
		}
	}

	FThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
}

void FCameraCutSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding)
{
	FThumbnailSection::BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	UWorld* World = GEditor->GetEditorWorldContext().World();

	if (World == nullptr)
	{
		return;
	}

	const AActor* CameraActor = GetCameraForFrame(Section->GetStartTime());

	// get list of available cameras
	TArray<AActor*> AllCameras;

	for (FActorIterator ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;

		if ((Actor != CameraActor) && Actor->IsListedInSceneOutliner())
		{
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromActor(Actor);
			if (CameraComponent)
			{
				AllCameras.Add(Actor);
			}
		}
	}

	if (AllCameras.Num() == 0)
	{
		return;
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ChangeCameraMenuText", "Change Camera"));
	{
		for (auto EachCamera : AllCameras)
		{
			FText ActorLabel = FText::FromString(EachCamera->GetActorLabel());

			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("SetCameraMenuEntryTextFormat", "{0}"), ActorLabel),
				FText::Format(LOCTEXT("SetCameraMenuEntryTooltipFormat", "Assign {0} to this camera cut"), ActorLabel),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FCameraCutSection::HandleSetCameraMenuEntryExecute, EachCamera))
			);
		}
	}
	MenuBuilder.EndSection();
}


FText FCameraCutSection::GetDisplayName() const
{
	return NSLOCTEXT("FCameraCutSection", "CameraCuts", "CameraCuts");
}


/* FThumbnailSection interface
 *****************************************************************************/

const AActor* FCameraCutSection::GetCameraForFrame(float Time) const
{
	UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

	if (CameraCutSection && Sequencer.IsValid())
	{
		TSharedRef<FMovieSceneSequenceInstance> SequenceInstance = Sequencer->GetFocusedMovieSceneSequenceInstance();

		AActor* Actor = Cast<AActor>(SequenceInstance->FindObject(CameraCutSection->GetCameraGuid(), *Sequencer));
		if (Actor)
		{
			return Actor;
		}
		else
		{
			FMovieSceneSpawnable* Spawnable = SequenceInstance->GetSequence()->GetMovieScene()->FindSpawnable(CameraCutSection->GetCameraGuid());
			if (Spawnable)
			{
				return Cast<AActor>(Spawnable->GetObjectTemplate());
			}
		}
	}

	return nullptr;
}

float FCameraCutSection::GetSectionHeight() const
{
	return FThumbnailSection::GetSectionHeight() + 10.f;
}

FMargin FCameraCutSection::GetContentPadding() const
{
	return FMargin(6.f, 10.f);
}

int32 FCameraCutSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	static const FSlateBrush* FilmBorder = FEditorStyle::GetBrush("Sequencer.Section.FilmBorder");

	InPainter.LayerId = InPainter.PaintSectionBackground();
	return FThumbnailSection::OnPaintSection(InPainter);
}

FText FCameraCutSection::HandleThumbnailTextBlockText() const
{
	const AActor* CameraActor = GetCameraForFrame(Section->GetStartTime());
	if (CameraActor)
	{
		return FText::FromString(CameraActor->GetActorLabel());
	}

	return FText::GetEmpty();
}


/* FCameraCutSection callbacks
 *****************************************************************************/

void FCameraCutSection::HandleSetCameraMenuEntryExecute(AActor* InCamera)
{
	auto Sequencer = SequencerPtr.Pin();

	if (Sequencer.IsValid())
	{
		FGuid ObjectGuid = Sequencer->GetHandleToObject(InCamera, true);

		UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);

		CameraCutSection->SetFlags(RF_Transactional);

		const FScopedTransaction Transaction(LOCTEXT("SetCameraCut", "Set Camera Cut"));

		CameraCutSection->Modify();
	
		CameraCutSection->SetCameraGuid(ObjectGuid);
	
		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}


#undef LOCTEXT_NAMESPACE
