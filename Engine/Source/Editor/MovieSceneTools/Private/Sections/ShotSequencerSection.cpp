// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ISectionLayoutBuilder.h"
#include "Runtime/MovieSceneTracks/Public/Sections/MovieSceneShotSection.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "FShotSequencerSection"


namespace ShotSequencerSectionConstants
{
	const uint32 ThumbnailHeight = 90;
	const uint32 TrackHeight = ThumbnailHeight+10; // some extra padding
	const uint32 TrackWidth = 90;
	const float SectionGripSize = 4.0f;
	const FName ShotTrackGripBrushName = FName("Sequencer.ShotTrack.SectionHandle");
}


/* FShotSequencerSection structors
 *****************************************************************************/

FShotSequencerSection::FShotSequencerSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FShotTrackThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection)
	: Section(CastChecked<UMovieSceneShotSection>(&InSection))
	, SequencerPtr(InSequencer)
	, ThumbnailPool(InThumbnailPool)
	, Thumbnails()
	, ThumbnailWidth(0)
	, StoredSize(ForceInit)
	, StoredStartTime(0.f)
	, VisibleTimeRange(TRange<float>::Empty())
	, InternalViewportScene(nullptr)
	, InternalViewportClient(nullptr)
{
	Camera = UpdateCameraObject();

	if (Camera.IsValid())
	{
		// @todo Sequencer optimize This code shouldn't even be called if this is offscreen
		// the following code could be generated on demand when the section is scrolled onscreen
		InternalViewportClient = MakeShareable(new FLevelEditorViewportClient(nullptr));
		{
			InternalViewportClient->ViewportType = LVT_Perspective;
			InternalViewportClient->bDisableInput = true;
			InternalViewportClient->bDrawAxes = false;
			InternalViewportClient->EngineShowFlags = FEngineShowFlags(ESFIM_Game);
			InternalViewportClient->EngineShowFlags.DisableAdvancedFeatures();
			InternalViewportClient->SetActorLock(Camera.Get());
			InternalViewportClient->SetAllowCinematicPreview( false );
			InternalViewportClient->SetRealtime( false );
		}

		InternalViewportScene = MakeShareable(new FSceneViewport(InternalViewportClient.Get(), nullptr));
		InternalViewportClient->Viewport = InternalViewportScene.Get();
	}

	WhiteBrush = FEditorStyle::GetBrush("WhiteBrush");
}


FShotSequencerSection::~FShotSequencerSection()
{
	if (ThumbnailPool.IsValid())
	{
		ThumbnailPool.Pin()->RemoveThumbnailsNeedingRedraw(Thumbnails);
	}
	
	if (InternalViewportClient.IsValid())
	{
		InternalViewportClient->Viewport = nullptr;
	}
}


/* FShotSequencerSection interface
 *****************************************************************************/

void FShotSequencerSection::DrawViewportThumbnail(TSharedPtr<FShotTrackThumbnail> ShotThumbnail)
{
	SequencerPtr.Pin()->SetGlobalTime(ShotThumbnail->GetTime());
	InternalViewportClient->UpdateViewForLockedActor();
	GWorld->SendAllEndOfFrameUpdates();
	InternalViewportScene->Draw(false);
	ShotThumbnail->CopyTextureIn((FSlateRenderTargetRHI*)InternalViewportScene->GetViewportRenderTargetTexture());
}


uint32 FShotSequencerSection::GetThumbnailWidth() const
{
	return ThumbnailWidth;
}


void FShotSequencerSection::RegenerateViewportThumbnails(const FIntPoint& Size)
{
	StoredSize = Size;
	StoredStartTime = Section->GetStartTime();
	
	if (Camera.IsValid())
	{
		ThumbnailPool.Pin()->RemoveThumbnailsNeedingRedraw(Thumbnails);
		Thumbnails.Empty();

		if (Size.X == 0 || Size.Y == 0)
		{
			return;
		}

		float AspectRatio = Camera->CameraComponent->AspectRatio;

		int32 NewThumbnailWidth = FMath::TruncToInt(ShotSequencerSectionConstants::TrackWidth * AspectRatio);
		int32 ThumbnailCount = FMath::DivideAndRoundUp(StoredSize.X, NewThumbnailWidth);

		if (NewThumbnailWidth != ThumbnailWidth)
		{
			ThumbnailWidth = NewThumbnailWidth;
			InternalViewportScene->UpdateViewportRHI(false, ThumbnailWidth, ShotSequencerSectionConstants::ThumbnailHeight, EWindowMode::Windowed);
		}

		float StartTime = Section->GetStartTime();
		float EndTime = Section->GetEndTime();
		float DeltaTime = EndTime - StartTime;

		int32 TotalX = StoredSize.X;

		// @todo sequencer optimize this to use a single viewport and a single thumbnail
		// instead paste the textures into the thumbnail at specific offsets
		for (int32 ThumbnailIndex = 0; ThumbnailIndex < ThumbnailCount; ++ThumbnailIndex)
		{
			int32 StartX = ThumbnailIndex * ThumbnailWidth;
			float FractionThrough = (float)StartX / (float)TotalX;
			float Time = StartTime + DeltaTime * FractionThrough;

			int32 NextStartX = (ThumbnailIndex+1) * ThumbnailWidth;
			float NextFractionThrough = (float)NextStartX / (float)TotalX;
			float NextTime = FMath::Min(StartTime + DeltaTime * NextFractionThrough, EndTime);

			FIntPoint ThumbnailSize(NextStartX-StartX, ShotSequencerSectionConstants::ThumbnailHeight);

			check(FractionThrough >= 0.f && FractionThrough <= 1.f && NextFractionThrough >= 0.f);
			TSharedPtr<FShotTrackThumbnail> NewThumbnail = MakeShareable(new FShotTrackThumbnail(SharedThis(this), ThumbnailSize, TRange<float>(Time, NextTime)));

			Thumbnails.Add(NewThumbnail);
		}

		// @todo Sequencer Optimize Only say a thumbnail needs redraw if it is onscreen
		ThumbnailPool.Pin()->AddThumbnailsNeedingRedraw(Thumbnails);
	}
}


/* ISequencerSection interface
 *****************************************************************************/

bool FShotSequencerSection::AreSectionsConnected() const
{
	return true;
}


void FShotSequencerSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();

	if (World == nullptr)
	{
		return;
	}

	// get list of available cameras
	TArray<AActor*> AllCameras;
	TArray<AActor*> SelectedCameras;

	for (FActorIterator ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;

		if ((Actor != Camera) && Actor->IsListedInSceneOutliner() && Actor->IsA<ACameraActor>())
		{
			AllCameras.Add(Actor);
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
				FText::Format(LOCTEXT("SetCameraMenuEntryTooltipFormat", "Assign camera {0} to this shot"), ActorLabel),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FShotSequencerSection::HandleSetCameraMenuEntryExecute, EachCamera))
			);
		}
	}
	MenuBuilder.EndSection();
}


TSharedRef<SWidget> FShotSequencerSection::GenerateSectionWidget()
{
	return SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(15.0f, 7.0f, 0.0f, 0.0f))
		[
			SNew(SInlineEditableTextBlock)
				.ToolTipText(LOCTEXT("RenameShot", "The name of this shot.  Click or hit F2 to rename"))
				.Text(this, &FShotSequencerSection::HandleShotNameTextBlockText)
				.ShadowOffset(FVector2D(1,1))
				.OnTextCommitted(this, &FShotSequencerSection::HandleShotNameTextBlockTextCommitted)
		];
}


FText FShotSequencerSection::GetDisplayName() const
{
	return NSLOCTEXT("FShotSection", "", "Shots");
}


FName FShotSequencerSection::GetSectionGripLeftBrushName() const
{
	return ShotSequencerSectionConstants::ShotTrackGripBrushName;
}


FName FShotSequencerSection::GetSectionGripRightBrushName() const
{
	return ShotSequencerSectionConstants::ShotTrackGripBrushName;
}


float FShotSequencerSection::GetSectionGripSize() const
{
	return ShotSequencerSectionConstants::SectionGripSize;
}


float FShotSequencerSection::GetSectionHeight() const
{
	return ShotSequencerSectionConstants::TrackHeight;
}


UMovieSceneSection* FShotSequencerSection::GetSectionObject() 
{
	return Section;
}


FText FShotSequencerSection::GetSectionTitle() const
{
	return FText::GetEmpty();
}


int32 FShotSequencerSection::OnPaintSection(const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled) const
{
	const ESlateDrawEffect::Type DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	if (Camera.IsValid())
	{
		FGeometry ThumbnailAreaGeometry = AllottedGeometry.MakeChild( FVector2D(GetSectionGripSize(), 0.0f), AllottedGeometry.GetDrawSize() - FVector2D( GetSectionGripSize()*2, 0.0f ) );

		FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				ThumbnailAreaGeometry.ToPaintGeometry(),
				FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
				SectionClippingRect,
				DrawEffects
			);

		// @todo Sequencer: Need a way to visualize the key here

		int32 ThumbnailIndex = 0;
		for( const TSharedPtr<FShotTrackThumbnail>& Thumbnail : Thumbnails )
		{
			FGeometry TruncatedGeometry = ThumbnailAreaGeometry.MakeChild(
				Thumbnail->GetSize(),
				FSlateLayoutTransform(ThumbnailAreaGeometry.Scale, FVector2D( ThumbnailIndex * ThumbnailWidth, 5.f))
				);
			
			FSlateDrawElement::MakeViewport(
				OutDrawElements,
				LayerId,
				TruncatedGeometry.ToPaintGeometry(),
				Thumbnail,
				SectionClippingRect,
				false,
				false,
				DrawEffects,
				FLinearColor::White
			);

			if(Thumbnail->GetFadeInCurve() > 0.0f )
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId+1,
					TruncatedGeometry.ToPaintGeometry(),
					WhiteBrush,
					SectionClippingRect,
					DrawEffects,
					FLinearColor(1.0f, 1.0f, 1.0f, Thumbnail->GetFadeInCurve())
					);
			}

			++ThumbnailIndex;
		}
	}
	else
	{
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect,
			DrawEffects
		); 
	}

	return LayerId + 2;
}


void FShotSequencerSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!Camera.IsValid())
	{
		// @todo sequencer: This is called too often and is expensive
		Camera = UpdateCameraObject();
	}

	FTimeToPixel TimeToPixelConverter(AllottedGeometry, TRange<float>(Section->GetStartTime(), Section->GetEndTime()));

	// get the visible time range
	VisibleTimeRange = TRange<float>(
		TimeToPixelConverter.PixelToTime(-AllottedGeometry.Position.X),
		TimeToPixelConverter.PixelToTime(-AllottedGeometry.Position.X + ParentGeometry.Size.X)
	);

	FIntPoint AllocatedSize = AllottedGeometry.MakeChild( FVector2D( GetSectionGripSize(), 0.0f ), FVector2D( AllottedGeometry.Size.X - (GetSectionGripSize()*2), AllottedGeometry.Size.Y) ).Size.IntPoint();
	AllocatedSize.X = FMath::Max(AllocatedSize.X, 1);

	float StartTime = Section->GetStartTime();

	if (!FSlateApplication::Get().HasAnyMouseCaptor() && ( AllocatedSize.X != StoredSize.X || !FMath::IsNearlyEqual(StartTime, StoredStartTime) ) )
	{
		RegenerateViewportThumbnails(AllocatedSize);
	}
}


/* FShotSequencerSection implementation
 *****************************************************************************/

ACameraActor* FShotSequencerSection::UpdateCameraObject() const
{
	TArray<UObject*> OutObjects;
	// @todo sequencer: the director track may be able to get cameras from sub-movie scenes
	SequencerPtr.Pin()->GetRuntimeObjects(SequencerPtr.Pin()->GetRootMovieSceneSequenceInstance(),  Section->GetCameraGuid(), OutObjects);

	ACameraActor* ReturnCam = nullptr;

	if (OutObjects.Num() > 0)
	{
		ReturnCam = Cast<ACameraActor>(OutObjects[0]);
	}

	return ReturnCam;
}


/* FShotSequencerSection callbacks
 *****************************************************************************/

void FShotSequencerSection::HandleSetCameraMenuEntryExecute(AActor* InCamera)
{
	auto Sequencer = SequencerPtr.Pin();

	if (Sequencer.IsValid())
	{
		FGuid ObjectGuid = Sequencer->GetHandleToObject(InCamera, true);
		Section->SetCameraGuid(ObjectGuid);
	}
}


FText FShotSequencerSection::HandleShotNameTextBlockText() const
{
	return Section->GetShotDisplayName();
}


void FShotSequencerSection::HandleShotNameTextBlockTextCommitted(const FText& NewShotName, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter && !HandleShotNameTextBlockText().EqualTo(NewShotName))
	{
		Section->SetShotNameAndNumber(NewShotName, Section->GetShotNumber());
	}
}


#undef LOCTEXT_NAMESPACE
