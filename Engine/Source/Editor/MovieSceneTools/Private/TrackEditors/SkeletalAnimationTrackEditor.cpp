// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "ScopedTransaction.h"
#include "ISequencerObjectChangeListener.h"
#include "ISectionLayoutBuilder.h"
#include "IKeyArea.h"
#include "MovieSceneTrackEditor.h"
#include "SkeletalAnimationTrackEditor.h"
#include "MovieSceneSkeletalAnimationSection.h"
#include "CommonMovieSceneTools.h"
#include "AssetRegistryModule.h"
#include "Animation/SkeletalMeshActor.h"
#include "ContentBrowserModule.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "SequencerUtilities.h"


namespace SkeletalAnimationEditorConstants
{
	// @todo Sequencer Allow this to be customizable
	const uint32 AnimationTrackHeight = 20;
}


#define LOCTEXT_NAMESPACE "FSkeletalAnimationTrackEditor"


FSkeletalAnimationSection::FSkeletalAnimationSection( UMovieSceneSection& InSection )
	: Section( InSection )
{ }


UMovieSceneSection* FSkeletalAnimationSection::GetSectionObject()
{ 
	return &Section;
}


FText FSkeletalAnimationSection::GetDisplayName() const
{
	return LOCTEXT("AnimationSection", "Animation");
}


FText FSkeletalAnimationSection::GetSectionTitle() const
{
	UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(&Section);
	if (AnimSection != nullptr && AnimSection->GetAnimSequence() != nullptr)
	{
		return FText::FromString( AnimSection->GetAnimSequence()->GetName() );
	}
	return LOCTEXT("NoAnimationSection", "No Animation");
}


float FSkeletalAnimationSection::GetSectionHeight() const
{
	return (float)SkeletalAnimationEditorConstants::AnimationTrackHeight;
}


int32 FSkeletalAnimationSection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	const ESlateDrawEffect::Type DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(&Section);
	
	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	int32 LayerId = Painter.PaintSectionBackground();

	// Add lines where the animation starts and ends/loops
	float CurrentTime = AnimSection->GetStartTime();
	float AnimPlayRate = FMath::IsNearlyZero(AnimSection->GetPlayRate()) ? 1.0f : AnimSection->GetPlayRate();
	float SeqLength = (AnimSection->GetSequenceLength() - (AnimSection->GetStartOffset() + AnimSection->GetEndOffset())) / AnimPlayRate;
	while (CurrentTime < AnimSection->GetEndTime() && !FMath::IsNearlyZero(AnimSection->GetDuration()) && SeqLength > 0)
	{
		if (CurrentTime > AnimSection->GetStartTime())
		{
			float CurrentPixels = TimeToPixelConverter.TimeToPixel(CurrentTime);

			TArray<FVector2D> Points;
			Points.Add(FVector2D(CurrentPixels, 0));
			Points.Add(FVector2D(CurrentPixels, Painter.SectionGeometry.Size.Y));

			FSlateDrawElement::MakeLines(
				Painter.DrawElements,
				++LayerId,
				Painter.SectionGeometry.ToPaintGeometry(),
				Points,
				Painter.SectionClippingRect,
				DrawEffects
			);
		}
		CurrentTime += SeqLength;
	}

	return LayerId;
}


FSkeletalAnimationTrackEditor::FSkeletalAnimationTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{ }


TSharedRef<ISequencerTrackEditor> FSkeletalAnimationTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FSkeletalAnimationTrackEditor( InSequencer ) );
}


bool FSkeletalAnimationTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneSkeletalAnimationTrack::StaticClass();
}


TSharedRef<ISequencerSection> FSkeletalAnimationTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	
	return MakeShareable( new FSkeletalAnimationSection(SectionObject) );
}


void FSkeletalAnimationTrackEditor::AddKey(const FGuid& ObjectGuid)
{
	USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectGuid);

	if (Skeleton)
	{
		// Load the asset registry module
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Collect a full list of assets with the specified class
		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssetsByClass(UAnimSequence::StaticClass()->GetFName(), AssetDataList);

		if (AssetDataList.Num())
		{
			TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow(); 
			if (Parent.IsValid())
			{
				FSlateApplication::Get().PushMenu(
					Parent.ToSharedRef(),
					FWidgetPath(),
					BuildAnimationSubMenu(ObjectGuid, Skeleton),
					FSlateApplication::Get().GetCursorPos(),
					FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
					);
			}
		}
	}
}


bool FSkeletalAnimationTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	if (Asset->IsA<UAnimSequence>())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(Asset);
		
		if (TargetObjectGuid.IsValid())
		{
			USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(TargetObjectGuid);

			if (Skeleton && Skeleton == AnimSequence->GetSkeleton())
			{
				TArray<TWeakObjectPtr<UObject>> OutObjects;

				GetSequencer()->GetRuntimeObjects(GetSequencer()->GetFocusedMovieSceneSequenceInstance(), TargetObjectGuid, OutObjects);
				AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSkeletalAnimationTrackEditor::AddKeyInternal, OutObjects, AnimSequence));

				return true;
			}
		}
	}
	return false;
}


void FSkeletalAnimationTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBinding);

		if (Skeleton)
		{
			// Load the asset registry module
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			// Collect a full list of assets with the specified class
			TArray<FAssetData> AssetDataList;
			AssetRegistryModule.Get().GetAssetsByClass(UAnimSequence::StaticClass()->GetFName(), AssetDataList);

			if (AssetDataList.Num())
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("AddAnimation", "Animation"), NSLOCTEXT("Sequencer", "AddAnimationTooltip", "Adds an animation track."),
					FNewMenuDelegate::CreateRaw(this, &FSkeletalAnimationTrackEditor::AddAnimationSubMenu, ObjectBinding, Skeleton)
				);
			}
		}
	}
}

TSharedRef<SWidget> FSkeletalAnimationTrackEditor::BuildAnimationSubMenu(FGuid ObjectBinding, USkeleton* Skeleton)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	AddAnimationSubMenu(MenuBuilder, ObjectBinding, Skeleton);

	return MenuBuilder.MakeWidget();
}

void FSkeletalAnimationTrackEditor::AddAnimationSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, USkeleton* Skeleton)
{
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw( this, &FSkeletalAnimationTrackEditor::OnAnimationAssetSelected, ObjectBinding);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
		AssetPickerConfig.Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(Skeleton).GetExportTextName());
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


void FSkeletalAnimationTrackEditor::OnAnimationAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UAnimSequence::StaticClass()))
	{
		UAnimSequence* AnimSequence = CastChecked<UAnimSequence>(AssetData.GetAsset());

		TArray<TWeakObjectPtr<UObject>> OutObjects;

		GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectBinding, OutObjects);
		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FSkeletalAnimationTrackEditor::AddKeyInternal, OutObjects, AnimSequence) );
	}
}


bool FSkeletalAnimationTrackEditor::AddKeyInternal( float KeyTime, const TArray<TWeakObjectPtr<UObject>> Objects, class UAnimSequence* AnimSequence )
{
	bool bHandleCreated = false;
	bool bTrackCreated = false;
	bool bTrackModified = false;

	for( int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex )
	{
		UObject* Object = Objects[ObjectIndex].Get();

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object );
		FGuid ObjectHandle = HandleResult.Handle;
		bHandleCreated |= HandleResult.bWasCreated;
		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieSceneSkeletalAnimationTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				Cast<UMovieSceneSkeletalAnimationTrack>(Track)->AddNewAnimation( KeyTime, AnimSequence );
				bTrackModified = true;
			}
		}
	}

	return bHandleCreated || bTrackCreated || bTrackModified;
}


USkeleton* FSkeletalAnimationTrackEditor::AcquireSkeletonFromObjectGuid(const FGuid& Guid)
{
	TArray<TWeakObjectPtr<UObject>> OutObjects;
	GetSequencer()->GetRuntimeObjects(GetSequencer()->GetFocusedMovieSceneSequenceInstance(), Guid, OutObjects);

	USkeleton* Skeleton = NULL;
	for (int32 i = 0; i < OutObjects.Num(); ++i)
	{
		UObject* Object = OutObjects[i].Get();

		if (AActor* Actor = Cast<AActor>(Object))
		{
			TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshComponents;
			Actor->GetComponents(SkeletalMeshComponents);

			for (int32 j = 0; j <SkeletalMeshComponents.Num(); ++j)
			{
				USkeletalMeshComponent* SkeletalMeshComp = SkeletalMeshComponents[j];
				if (SkeletalMeshComp->SkeletalMesh && SkeletalMeshComp->SkeletalMesh->Skeleton)
				{
					// @todo Multiple actors, multiple components
					check(!Skeleton);
					Skeleton = SkeletalMeshComp->SkeletalMesh->Skeleton;
				}
			}
		}
		else if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Object))
		{
			if (SkeletalMeshComponent->SkeletalMesh && SkeletalMeshComponent->SkeletalMesh->Skeleton)
			{
				check(!Skeleton);
				Skeleton = SkeletalMeshComponent->SkeletalMesh->Skeleton;
			}
		}
	}

	return Skeleton;
}


void FSkeletalAnimationTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackAnimControl* MatineeAnimControlTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		MatineeAnimControlTrack = Cast<UInterpTrackAnimControl>( CopyPasteObject );
		if ( MatineeAnimControlTrack != nullptr )
		{
			break;
		}
	}
	UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack = Cast<UMovieSceneSkeletalAnimationTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeAnimControlTrack", "Paste Matinee SkeletalAnimation Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeAnimControlTrackTooltip", "Pastes keys from a Matinee float track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &FMatineeImportTools::CopyInterpAnimControlTrack, GetSequencer().ToSharedRef(), MatineeAnimControlTrack, SkeletalAnimationTrack ),
			FCanExecuteAction::CreateLambda( [=]()->bool { return MatineeAnimControlTrack != nullptr && MatineeAnimControlTrack->AnimSeqs.Num() > 0 && SkeletalAnimationTrack != nullptr; } ) ) );
}

TSharedPtr<SWidget> FSkeletalAnimationTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBinding);

	if (Skeleton)
	{
		// Create a container edit box
		return SNew(SHorizontalBox)

		// Add the animation combo box
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			FSequencerUtilities::MakeAddButton(LOCTEXT("AnimationText", "Animation"), FOnGetContent::CreateSP(this, &FSkeletalAnimationTrackEditor::BuildAnimationSubMenu, ObjectBinding, Skeleton), Params.NodeIsHovered)
		];
	}

	else
	{
		return TSharedPtr<SWidget>();
	}
}

#undef LOCTEXT_NAMESPACE
