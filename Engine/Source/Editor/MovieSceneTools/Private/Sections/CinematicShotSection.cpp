// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "CinematicShotSection.h"
#include "ISectionLayoutBuilder.h"
#include "Runtime/MovieSceneTracks/Public/Sections/MovieSceneCinematicShotSection.h"
#include "CinematicShotTrackEditor.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "SInlineEditableTextBlock.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneToolsUserSettings.h"


#define LOCTEXT_NAMESPACE "FCinematicShotSection"


/* FCinematicShotSection structors
 *****************************************************************************/

FCinematicShotSection::FCinematicSectionCache::FCinematicSectionCache(UMovieSceneCinematicShotSection* Section)
	: ActualStartTime(0.f), TimeScale(0.f)
{
	if (Section)
	{
		ActualStartTime = Section->GetStartTime() - Section->StartOffset;
		TimeScale = Section->TimeScale;
	}
}


FCinematicShotSection::FCinematicShotSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection, TSharedPtr<FCinematicShotTrackEditor> InCinematicShotTrackEditor)
	: FThumbnailSection(InSequencer, InThumbnailPool, InSection)
	, SectionObject(*CastChecked<UMovieSceneCinematicShotSection>(&InSection))
	, Sequencer(InSequencer)
	, CinematicShotTrackEditor(InCinematicShotTrackEditor)
	, InitialStartOffsetDuringResize(0.f)
	, InitialStartTimeDuringResize(0.f)
	, ThumbnailCacheData(&SectionObject)
{
	if (InSequencer->HasSequenceInstanceForSection(InSection))
	{
		SequenceInstance = InSequencer->GetSequenceInstanceForSection(InSection);
	}
}


FCinematicShotSection::~FCinematicShotSection()
{
}

float FCinematicShotSection::GetSectionHeight() const
{
	return FThumbnailSection::GetSectionHeight() + 2*9.f;
}

FMargin FCinematicShotSection::GetContentPadding() const
{
	return FMargin(8.f, 15.f);
}

void FCinematicShotSection::SetSingleTime(float GlobalTime)
{
	SectionObject.SetThumbnailReferenceOffset(GlobalTime - SectionObject.GetStartTime());
}

void FCinematicShotSection::BeginResizeSection()
{
	InitialStartOffsetDuringResize = SectionObject.StartOffset;
	InitialStartTimeDuringResize = SectionObject.GetStartTime();
}

void FCinematicShotSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, float ResizeTime)
{
	// Adjust the start offset when resizing from the beginning
	if (ResizeMode == SSRM_LeadingEdge)
	{
		float StartOffset = (ResizeTime - InitialStartTimeDuringResize) / SectionObject.TimeScale;
		StartOffset += InitialStartOffsetDuringResize;

		// Ensure start offset is not less than 0
		StartOffset = FMath::Max(StartOffset, 0.f);

		SectionObject.StartOffset = StartOffset;
	}

	FThumbnailSection::ResizeSection(ResizeMode, ResizeTime);
}

void FCinematicShotSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Set cached data
	FCinematicSectionCache NewCacheData(&SectionObject);
	if (NewCacheData != ThumbnailCacheData)
	{
		ThumbnailCache.ForceRedraw();
	}
	ThumbnailCacheData = NewCacheData;

	// Update single reference frame settings
	if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails)
	{
		ThumbnailCache.SetSingleReferenceFrame(SectionObject.GetStartTime() + SectionObject.GetThumbnailReferenceOffset());
	}
	else
	{
		ThumbnailCache.SetSingleReferenceFrame(TOptional<float>());
	}

	FThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
}

int32 FCinematicShotSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	static const FSlateBrush* FilmBorder = FEditorStyle::GetBrush("Sequencer.Section.FilmBorder");

	InPainter.LayerId = InPainter.PaintSectionBackground();

	FVector2D SectionSize = InPainter.SectionGeometry.GetLocalSize();

	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(SectionSize.X-2.f, 7.f), FSlateLayoutTransform(FVector2D(1.f, 4.f))),
		FilmBorder,
		InPainter.SectionClippingRect.InsetBy(FMargin(1.f)),
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(SectionSize.X-2.f, 7.f), FSlateLayoutTransform(FVector2D(1.f, SectionSize.Y - 11.f))),
		FilmBorder,
		InPainter.SectionClippingRect.InsetBy(FMargin(1.f)),
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	if (SequenceInstance.IsValid())
	{
		return FThumbnailSection::OnPaintSection(InPainter);
	}

	return InPainter.LayerId;
}

void FCinematicShotSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding)
{
	FThumbnailSection::BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ShotMenuText", "Shot"));
	{
		if (SequenceInstance.IsValid())
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("TakesMenu", "Takes"),
				LOCTEXT("TakesMenuTooltip", "Shot takes"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& InMenuBuilder){ AddTakesMenu(InMenuBuilder); }));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("NewTake", "New Take"),
				FText::Format(LOCTEXT("NewTakeTooltip", "Create a new take for {0}"), SectionObject.GetShotDisplayName()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::NewTake, &SectionObject))
			);
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("InsertNewShot", "Insert Shot"),
			FText::Format(LOCTEXT("InsertNewShotTooltip", "Insert a new shot after {0}"), SectionObject.GetShotDisplayName()),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::InsertShot, &SectionObject))
		);

		if (SequenceInstance.IsValid())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DuplicateShot", "Duplicate Shot"),
				FText::Format(LOCTEXT("DuplicateShotTooltip", "Duplicate {0} to create a new shot"), SectionObject.GetShotDisplayName()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::DuplicateShot, &SectionObject))
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RenderShot", "Render Shot"),
				FText::Format(LOCTEXT("RenderShotTooltip", "Render shot movie"), SectionObject.GetShotDisplayName()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::RenderShot, &SectionObject))
			);

			/*
			//@todo
			MenuBuilder.AddMenuEntry(
				LOCTEXT("RenameShot", "Rename Shot"),
				FText::Format(LOCTEXT("RenameShotTooltip", "Rename {0}"), SectionObject.GetShotDisplayName()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::RenameShot, &SectionObject))
			);
			*/
		}
	}
	MenuBuilder.EndSection();
}

void FCinematicShotSection::AddTakesMenu(FMenuBuilder& MenuBuilder)
{
	TArray<uint32> TakeNumbers;
	uint32 CurrentTakeNumber = INDEX_NONE;
	MovieSceneToolHelpers::GatherTakes(&SectionObject, TakeNumbers, CurrentTakeNumber);

	for (auto TakeNumber : TakeNumbers)
	{
		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("TakeNumber", "Take {0}"), FText::AsNumber(TakeNumber)),
			FText::Format(LOCTEXT("TakeNumberTooltip", "Switch to take {0}"), FText::AsNumber(TakeNumber)),
			TakeNumber == CurrentTakeNumber ? FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Star") : FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Empty"),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::SwitchTake, &SectionObject, TakeNumber))
		);
	}
}

FText FCinematicShotSection::GetDisplayName() const
{
	return NSLOCTEXT("FCinematicShotSection", "Shot", "Shot");
}

/* FCinematicShotSection callbacks
 *****************************************************************************/

FText FCinematicShotSection::HandleThumbnailTextBlockText() const
{
	return SectionObject.GetShotDisplayName();
}


void FCinematicShotSection::HandleThumbnailTextBlockTextCommitted(const FText& NewShotName, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter && !HandleThumbnailTextBlockText().EqualTo(NewShotName))
	{
		SectionObject.Modify();

		const FScopedTransaction Transaction(LOCTEXT("SetShotName", "Set Shot Name"));
	
		SectionObject.SetShotDisplayName(NewShotName);
	}
}

FReply FCinematicShotSection::OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent)
{
	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		if (SequenceInstance.IsValid())
		{
			if (SectionObject.GetSequence())
			{
				Sequencer.Pin()->FocusSequenceInstance(SectionObject);
			}
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
