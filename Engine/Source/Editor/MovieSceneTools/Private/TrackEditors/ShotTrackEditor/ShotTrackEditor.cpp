// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneShotTrack.h"
#include "MovieSceneShotSection.h"
#include "ScopedTransaction.h"
#include "ISequencerObjectChangeListener.h"
#include "IKeyArea.h"
#include "MovieSceneTrackEditor.h"
#include "ShotTrackEditor.h"
#include "CommonMovieSceneTools.h"
#include "Camera/CameraActor.h"
#include "AssetToolsModule.h"
#include "ShotSequencerSection.h"

#include "ActorEditorUtils.h"
#include "Editor/SceneOutliner/Public/SceneOutliner.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "SceneOutlinerPublicTypes.h"

#define LOCTEXT_NAMESPACE "FShotTrackEditor"


/* FShotTrackEditor structors
 *****************************************************************************/

FShotTrackEditor::FShotTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer) 
{
	ThumbnailPool = MakeShareable(new FShotTrackThumbnailPool(InSequencer));
}


TSharedRef<ISequencerTrackEditor> FShotTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FShotTrackEditor(InSequencer));
}


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FShotTrackEditor::AddKey(const FGuid& ObjectGuid, UObject* AdditionalAsset)
{/*
	TArray<UObject*> OutObjects;

	// @todo sequencer: the director track may be able to get cameras from sub-movie scenes
	GetSequencer()->GetRuntimeObjects(GetSequencer()->GetRootMovieSceneSequenceInstance(), ObjectGuid, OutObjects);
	bool bValidKey = OutObjects.Num() == 1 && OutObjects[0]->IsA<ACameraActor>();
	
	if (bValidKey)*/
	{
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FShotTrackEditor::AddKeyInternal, ObjectGuid));
	}
}


void FShotTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	UMovieSceneSequence* RootMovieSceneSequence = GetSequencer()->GetRootMovieSceneSequence();

	if ((RootMovieSceneSequence == nullptr) || (RootMovieSceneSequence->GetClass()->GetName() != TEXT("LevelSequence")))
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddShotTrack", "Shot Track"),
		LOCTEXT("AddShotTooltip", "Adds a shot track, as well as a new shot at the current scrubber location if a camera is selected."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Shot"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FShotTrackEditor::HandleAddShotTrackMenuEntryExecute),
			FCanExecuteAction::CreateRaw(this, &FShotTrackEditor::HandleAddShotTrackMenuEntryCanExecute)
		)
	);
}

TSharedPtr<SWidget> FShotTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track)
{
	// Create a container edit box
	TSharedPtr<class SHorizontalBox> EditBox;
	SAssignNew(EditBox, SHorizontalBox);	

	// Add the camera combo box
	EditBox.Get()->AddSlot()
	[
		SNew(SComboButton)
		.ButtonStyle(FEditorStyle::Get(), "FlatButton.Light")
		.OnGetMenuContent(this, &FShotTrackEditor::HandleAddShotComboButtonGetMenuContent)
		.ContentPadding(FMargin(2, 0))
		.ButtonContent()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.8"))
						.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4, 0, 0, 0)
				[
					SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
						.Text(LOCTEXT("AddShotButtonText", "Shot"))
				]
		]
	];

	EditBox.Get()->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.AutoWidth()
		.Padding(4, 0, 0, 0)
		[
			SNew(SCheckBox)
				.IsFocusable(false)
				.IsChecked(this, &FShotTrackEditor::IsCameraLocked)
				.OnCheckStateChanged(this, &FShotTrackEditor::OnLockCameraClicked)
				.ToolTipText(this, &FShotTrackEditor::GetLockCameraToolTip)
				.ForegroundColor(FLinearColor::White)
				.CheckedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
				.CheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
				.CheckedPressedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
				.UncheckedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
				.UncheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
				.UncheckedPressedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
		];

	return EditBox.ToSharedRef();
}


TSharedRef<ISequencerSection> FShotTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));

	return MakeShareable(new FShotSequencerSection(GetSequencer(), ThumbnailPool, SectionObject));
}


bool FShotTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneShotTrack::StaticClass());
}


void FShotTrackEditor::Tick(float DeltaTime)
{
	if (FSlateThrottleManager::Get().IsAllowingExpensiveTasks())
	{
		ThumbnailPool->DrawThumbnails();
	}
}


/* FShotTrackEditor implementation
 *****************************************************************************/

bool FShotTrackEditor::AddKeyInternal( float KeyTime, const FGuid ObjectGuid )
{
	UMovieSceneShotTrack* ShotTrack = FindOrCreateShotTrack();
	const TArray<UMovieSceneSection*>& AllSections = ShotTrack->GetAllSections();

	int32 SectionIndex = FindIndexForNewShot(AllSections, KeyTime);
	const int32 ShotNumber = GenerateShotNumber(*ShotTrack, SectionIndex);
	FString ShotName = FString::Printf(TEXT("Shot_%04d"), ShotNumber);

	ShotTrack->AddNewShot(ObjectGuid, KeyTime, FText::FromString(ShotName), ShotNumber);

	return true;
}


UMovieSceneShotTrack* FShotTrackEditor::FindOrCreateShotTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	UMovieSceneTrack* ShotTrack = FocusedMovieScene->GetShotTrack();

	if (ShotTrack == nullptr)
	{
		ShotTrack = FocusedMovieScene->AddShotTrack(UMovieSceneShotTrack::StaticClass());
	}

	return CastChecked<UMovieSceneShotTrack>(ShotTrack);
}


int32 FShotTrackEditor::FindIndexForNewShot(const TArray<UMovieSceneSection*>& ShotSections, float NewShotTime) const
{
	// @todo Sequencer: This could be a binary search. All shots should be sorted 
	int32 SectionIndex = 0;
	for (SectionIndex = 0; SectionIndex < ShotSections.Num() && ShotSections[SectionIndex]->GetStartTime() < NewShotTime; ++SectionIndex);

	return SectionIndex;
}


int32 FShotTrackEditor::GenerateShotNumber(UMovieSceneShotTrack& ShotTrack, int32 SectionIndex) const
{
	const TArray<UMovieSceneSection*>& AllSections = ShotTrack.GetAllSections();

	const int32 Interval = 10;
	int32 ShotNum = Interval;
	int32 LastKeyIndex = AllSections.Num() - 1;

	int32 PrevShotNum = 0;
	//get the preceding shot number if any
	if (SectionIndex > 0)
	{
		PrevShotNum = CastChecked<UMovieSceneShotSection>( AllSections[SectionIndex - 1] )->GetShotNumber();
	}

	if (SectionIndex < LastKeyIndex)
	{
		//we're inserting before something before the first frame
		int32 NextShotNum = CastChecked<UMovieSceneShotSection>( AllSections[SectionIndex + 1] )->GetShotNumber();
		if (NextShotNum == 0)
		{
			NextShotNum = PrevShotNum + (Interval * 2);
		}

		if (NextShotNum > PrevShotNum)
		{
			//find a midpoint if we're in order

			//try to stick to the nearest interval if possible
			int32 NearestInterval = PrevShotNum - (PrevShotNum % Interval) + Interval;
			if (NearestInterval > PrevShotNum && NearestInterval < NextShotNum)
			{
				ShotNum = NearestInterval;
			}
			//else find the exact mid point
			else
			{
				ShotNum = ((NextShotNum - PrevShotNum) / 2) + PrevShotNum;
			}
		}
		else
		{
			//Just use the previous shot number + 1 with we're out of order
			ShotNum = PrevShotNum + 1;
		}
	}
	else
	{
		//we're adding to the end of the track
		ShotNum = PrevShotNum + Interval;
	}

	return ShotNum;
}


UFactory* FShotTrackEditor::GetAssetFactoryForNewShot( UClass* SequenceClass )
{
	static TWeakObjectPtr<UFactory> ShotFactory;

	if( !ShotFactory.IsValid() || ShotFactory->SupportedClass != SequenceClass )
	{
		TArray<UFactory*> Factories;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class->IsChildOf(UFactory::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
			{
				UFactory* Factory = Class->GetDefaultObject<UFactory>();
				if (Factory->CanCreateNew())
				{
					UClass* SupportedClass = Factory->GetSupportedClass();
					if ( SupportedClass == SequenceClass )
					{
						ShotFactory = Factory;
					}
				}
			}
		}
	}

	return ShotFactory.Get();
}


/* FShotTrackEditor callbacks
 *****************************************************************************/

bool FShotTrackEditor::HandleAddShotTrackMenuEntryCanExecute() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	return ((FocusedMovieScene != nullptr) && (FocusedMovieScene->GetShotTrack() == nullptr));
}


void FShotTrackEditor::HandleAddShotTrackMenuEntryExecute()
{
	FindOrCreateShotTrack();
	GetSequencer()->NotifyMovieSceneDataChanged();
}

bool FShotTrackEditor::IsCameraPickable(const AActor* const PickableActor)
{
	if (PickableActor->IsListedInSceneOutliner() &&
		!FActorEditorUtils::IsABuilderBrush(PickableActor) &&
		!PickableActor->IsA( AWorldSettings::StaticClass() ) &&
		!PickableActor->IsPendingKill() &&
		PickableActor->IsA( ACameraActor::StaticClass()) )
	{			
		return true;
	}
	return false;
}

TSharedRef<SWidget> FShotTrackEditor::HandleAddShotComboButtonGetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	using namespace SceneOutliner;

	SceneOutliner::FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;			
		InitOptions.bShowHeaderRow = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;
		InitOptions.bShowTransient = true;
		InitOptions.bShowCreateNewFolder = false;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display Actors that we can attach too
		InitOptions.Filters->AddFilterPredicate( SceneOutliner::FActorFilterPredicate::CreateSP(this, &FShotTrackEditor::IsCameraPickable) );
	}		

	// Actor selector to allow the user to choose a parent actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>( "SceneOutliner" );

	TSharedRef< SWidget > MenuWidget = 
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.MaxHeight(400.0f)
			[
				SceneOutlinerModule.CreateSceneOutliner(
					InitOptions,
					FOnActorPicked::CreateSP(this, &FShotTrackEditor::HandleAddShotComboButtonMenuEntryExecute )
					)
			]
		];

	MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), false);
	return MenuBuilder.MakeWidget();
}


void FShotTrackEditor::HandleAddShotComboButtonMenuEntryExecute(AActor* Camera)
{
	FGuid ObjectGuid = FindOrCreateHandleToObject(Camera).Handle;

	if (ObjectGuid.IsValid())
	{
		AddKey(ObjectGuid);
	}
}

ECheckBoxState FShotTrackEditor::IsCameraLocked() const
{
	if (GetSequencer()->IsPerspectiveViewportCameraCutEnabled())
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}


void FShotTrackEditor::OnLockCameraClicked(ECheckBoxState CheckBoxState)
{
	if (CheckBoxState == ECheckBoxState::Checked)
	{
		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
			if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() && LevelVC->GetViewMode() != VMI_Unknown)
			{
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}
		GetSequencer()->SetPerspectiveViewportCameraCutEnabled(true);
	}
	else
	{
		GetSequencer()->UpdateCameraCut(nullptr, nullptr);
		GetSequencer()->SetPerspectiveViewportCameraCutEnabled(false);
	}

	GetSequencer()->UpdateRuntimeInstances();
}


FText FShotTrackEditor::GetLockCameraToolTip() const
{
	return IsCameraLocked() == ECheckBoxState::Checked ?
		LOCTEXT("UnlockCamera", "Unlock Viewport from Shot Camera") :
		LOCTEXT("LockCamera", "Lock Viewport to Shot Camera");
}

#undef LOCTEXT_NAMESPACE
