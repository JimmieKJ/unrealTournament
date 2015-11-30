// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "Sequencer.h"
#include "SequencerEdMode.h"
#include "SequencerDetailKeyframeHandler.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/SequencerWidgets/Public/ITimeSlider.h"
#include "Editor/EditorWidgets/Public/ITransportControl.h"
#include "Editor/EditorWidgets/Public/EditorWidgetsModule.h"
#include "Editor/LevelEditor/Public/ILevelEditor.h"
#include "Editor/LevelEditor/Public/ILevelViewport.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SSequencer.h"
#include "SSequencerTreeView.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditorModule.h"
#include "MovieSceneTrack.h"
#include "MovieSceneShotTrack.h"
#include "MovieSceneAudioTrack.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneTrackEditor.h"
#include "MovieSceneToolHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetSelection.h"
#include "ScopedTransaction.h"
#include "MovieSceneShotSection.h"
#include "MovieScene3DTransformSection.h"
#include "MovieSceneSubTrack.h"
#include "ISequencerSection.h"
#include "MovieSceneSequenceInstance.h"
#include "IKeyArea.h"
#include "IDetailsView.h"
#include "SnappingUtils.h"
#include "GenericCommands.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Selection.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "LevelEditor.h"
#include "IMenu.h"
#include "NotificationManager.h"
#include "SNotificationList.h"
#include "MovieSceneClipboard.h"
#include "SequencerClipboardReconciler.h"
#include "STextEntryPopup.h"
#include "SequencerHotspots.h"
#include "MovieSceneCaptureDialogModule.h"
#include "AutomatedLevelSequenceCapture.h"
#include "MovieSceneCommonHelpers.h"

#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"

#define LOCTEXT_NAMESPACE "Sequencer"

DEFINE_LOG_CATEGORY(LogSequencer);


bool FSequencer::IsSequencerEnabled()
{
	return true;
}

namespace
{
	TSharedPtr<FSequencer> ActiveSequencer;
}

void FSequencer::InitSequencer(const FSequencerInitParams& InitParams, const TSharedRef<ISequencerObjectChangeListener>& InObjectChangeListener, const TSharedRef<IDetailKeyframeHandler>& InDetailKeyframeHandler, const TArray<FOnCreateTrackEditor>& TrackEditorDelegates)
{
	if( IsSequencerEnabled() )
	{
		bIsEditingWithinLevelEditor = InitParams.bEditWithinLevelEditor;
		
		if (InitParams.SpawnRegister.IsValid())
		{
			SpawnRegister = InitParams.SpawnRegister;
		}
		else
		{
			// Spawnables not supported
			SpawnRegister = MakeShareable(new FNullMovieSceneSpawnRegister);
		}

		// If this sequencer edits the level, close out the existing active sequencer and mark this as the active sequencer.
		if (bIsEditingWithinLevelEditor)
		{
			Settings = USequencerSettingsContainer::GetOrCreate<ULevelEditorSequencerSettings>(*InitParams.ViewParams.UniqueName);

			if (ActiveSequencer.IsValid())
			{
				ActiveSequencer->Close();
			}
			ActiveSequencer = SharedThis(this);

			// Register for saving the level so that the state of the scene can be restored before saving and updated after saving.
			FEditorDelegates::PreSaveWorld.AddSP(this, &FSequencer::OnPreSaveWorld);
			FEditorDelegates::PostSaveWorld.AddSP(this, &FSequencer::OnPostSaveWorld);
			FEditorDelegates::PreBeginPIE.AddSP(this, &FSequencer::OnPreBeginPIE);
			FEditorDelegates::EndPIE.AddSP(this, &FSequencer::OnEndPIE);

			FEditorDelegates::NewCurrentLevel.AddSP(this, &FSequencer::OnNewCurrentLevel);
			FEditorDelegates::OnMapOpened.AddSP(this, &FSequencer::OnMapOpened);
		}
		else
		{
			Settings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(*InitParams.ViewParams.UniqueName);
		}

		ToolkitHost = InitParams.ToolkitHost;

		ScrubPosition = InitParams.ViewParams.InitialScrubPosition;
		ObjectChangeListener = InObjectChangeListener;
		DetailKeyframeHandler = InDetailKeyframeHandler;

		check( ObjectChangeListener.IsValid() );
		
		UMovieSceneSequence& RootSequence = *InitParams.RootSequence;
		
		// Focusing the initial movie scene needs to be done before the first time GetFocusedMovieSceneInstane or GetRootMovieSceneInstance is used
		RootMovieSceneSequenceInstance = MakeShareable(new FMovieSceneSequenceInstance(RootSequence));
		SequenceInstanceStack.Add(RootMovieSceneSequenceInstance.ToSharedRef());

		// Make internal widgets
		SequencerWidget = SNew( SSequencer, SharedThis( this ) )
			.ViewRange( this, &FSequencer::GetViewRange )
			.ClampRange( this, &FSequencer::GetClampRange )
			.PlaybackRange( this, &FSequencer::GetPlaybackRange )
			.OnPlaybackRangeChanged( this, &FSequencer::SetPlaybackRange )
			.OnBeginPlaybackRangeDrag( this, &FSequencer::OnBeginPlaybackRangeDrag )
			.OnEndPlaybackRangeDrag( this, &FSequencer::OnEndPlaybackRangeDrag )
			.ScrubPosition( this, &FSequencer::OnGetScrubPosition )
			.OnBeginScrubbing( this, &FSequencer::OnBeginScrubbing )
			.OnEndScrubbing( this, &FSequencer::OnEndScrubbing )
			.OnScrubPositionChanged( this, &FSequencer::OnScrubPositionChanged )
			.OnViewRangeChanged( this, &FSequencer::SetViewRange )
			.OnClampRangeChanged( this, &FSequencer::OnClampRangeChanged )
			.OnGetAddMenuContent(InitParams.ViewParams.OnGetAddMenuContent)
			.AddMenuExtender(InitParams.ViewParams.AddMenuExtender);

		// When undo occurs, get a notification so we can make sure our view is up to date
		GEditor->RegisterForUndo(this);

		if( bIsEditingWithinLevelEditor )
		{
			// @todo remove when world-centric mode is added
			// Hook into the editor's mechanism for checking whether we need live capture of PIE/SIE actor state
			GEditor->GetActorRecordingState().AddSP(this, &FSequencer::GetActorRecordingState);

			ActivateDetailKeyframeHandler();
				
			FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			EditModule.OnPropertyEditorOpened().AddSP(this, &FSequencer::OnPropertyEditorOpened);
		}

		// Create tools and bind them to this sequencer
		for( int32 DelegateIndex = 0; DelegateIndex < TrackEditorDelegates.Num(); ++DelegateIndex )
		{
			check( TrackEditorDelegates[DelegateIndex].IsBound() );
			// Tools may exist in other modules, call a delegate that will create one for us 
			TSharedRef<ISequencerTrackEditor> TrackEditor = TrackEditorDelegates[DelegateIndex].Execute( SharedThis( this ) );
			TrackEditors.Add( TrackEditor );
		}

		ZoomAnimation = FCurveSequence();
		ZoomCurve = ZoomAnimation.AddCurve(0.f, 0.2f, ECurveEaseFunction::QuadIn);
		OverlayAnimation = FCurveSequence();
		OverlayCurve = OverlayAnimation.AddCurve(0.f, 0.2f, ECurveEaseFunction::QuadIn);

		// Update initial movie scene data
		NotifyMovieSceneDataChanged();

		UpdateTimeBoundsToFocusedMovieScene();

		// NOTE: Could fill in asset editor commands here!

		BindSequencerCommands();

		ActivateSequencerEditorMode();
	}
}


FSequencer::FSequencer()
	: SequencerCommandBindings( new FUICommandList )
	, TargetViewRange(0.f, 5.f)
	, LastViewRange(0.f, 5.f)
	, ViewRangeBeforeZoom(TRange<float>::Empty())
	, PlaybackState( EMovieScenePlayerStatus::Stopped )
	, ScrubPosition( 0.0f )
	, bPerspectiveViewportPossessionEnabled( true )
	, bPerspectiveViewportCameraCutEnabled( false )
	, bIsEditingWithinLevelEditor( false )
	, bNeedTreeRefresh( false )
{
	Selection.GetOnOutlinerNodeSelectionChanged().AddRaw(this, &FSequencer::OnSelectedOutlinerNodesChanged);
}

FSequencer::~FSequencer()
{
	GEditor->GetActorRecordingState().RemoveAll( this );
	GEditor->UnregisterForUndo( this );

	TrackEditors.Empty();
	SequencerWidget.Reset();
}

void FSequencer::Close()
{
	if (ActiveSequencer.IsValid() && ActiveSequencer.Get() == this)
	{
		RootMovieSceneSequenceInstance->RestoreState(*this);

		FEditorDelegates::PreSaveWorld.RemoveAll(this);
		FEditorDelegates::PostSaveWorld.RemoveAll(this);
		FEditorDelegates::PreBeginPIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);

		FEditorDelegates::NewCurrentLevel.RemoveAll(this);
		FEditorDelegates::OnMapOpened.RemoveAll(this);
		
		for (auto TrackEditor : TrackEditors)
		{
			TrackEditor->OnRelease();
		}
		ActiveSequencer.Reset();

		if( GLevelEditorModeTools().IsModeActive( FSequencerEdMode::EM_SequencerMode ) )
		{
			GLevelEditorModeTools().DeactivateMode( FSequencerEdMode::EM_SequencerMode );
		}
	}

	if( bIsEditingWithinLevelEditor )
	{
		DeactivateDetailKeyframeHandler();
			
		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");			
		EditModule.OnPropertyEditorOpened().RemoveAll(this);
	}

	SequencerWidget.Reset();
}

void FSequencer::Tick(float InDeltaTime)
{
	if (bNeedTreeRefresh)
	{
		// @todo - Sequencer Will be called too often
		UpdateRuntimeInstances();

		SequencerWidget->UpdateLayoutTree();
		bNeedTreeRefresh = false;
	}
	
	static const float AutoScrollFactor = 0.1f;

	// Animate the autoscroll offset if it's set
	if (AutoscrollOffset.IsSet())
	{
		float Offset = AutoscrollOffset.GetValue() * AutoScrollFactor;
		SetViewRange(TRange<float>(TargetViewRange.GetLowerBoundValue() + Offset, TargetViewRange.GetUpperBoundValue() + Offset), EViewRangeInterpolation::Immediate);
	}

	// Animate the autoscrub offset if it's set
	if (AutoscrubOffset.IsSet())
	{
		float Offset = AutoscrubOffset.GetValue() * AutoScrollFactor;
		SetGlobalTimeDirectly(GetGlobalTime() + Offset);
	}

	float NewTime = GetGlobalTime() + InDeltaTime * GWorld->GetWorldSettings()->MatineeTimeDilation;

	if (PlaybackState == EMovieScenePlayerStatus::Playing ||
		PlaybackState == EMovieScenePlayerStatus::Recording)
	{
		SetGlobalTimeLooped(NewTime);
	}

	// Tick all the tools we own as well
	for (int32 EditorIndex = 0; EditorIndex < TrackEditors.Num(); ++EditorIndex)
	{
		TrackEditors[EditorIndex]->Tick(InDeltaTime);
	}
}


UMovieSceneSequence* FSequencer::GetRootMovieSceneSequence() const
{
	if (SequenceInstanceStack.Num())
	{
		return SequenceInstanceStack[0]->GetSequence();
	}

	return nullptr;
}

UMovieSceneSequence* FSequencer::GetFocusedMovieSceneSequence() const
{
	// the last item is the focused movie scene
	if (SequenceInstanceStack.Num())
	{
		return SequenceInstanceStack.Top()->GetSequence();
	}

	return nullptr;
}

void FSequencer::ResetToNewRootSequence(UMovieSceneSequence& NewSequence)
{
	if (RootMovieSceneSequenceInstance.IsValid())
	{
		RootMovieSceneSequenceInstance->RestoreState(*this);
	}

	//@todo Sequencer - Encapsulate this better
	SequenceInstanceStack.Empty();
	Selection.Empty();
	SequenceInstanceBySection.Empty();

	// Focusing the initial movie scene needs to be done before the first time NewSequence or GetRootMovieSceneInstance is used
	RootMovieSceneSequenceInstance = MakeShareable(new FMovieSceneSequenceInstance(NewSequence));
	SequenceInstanceStack.Add(RootMovieSceneSequenceInstance.ToSharedRef());

	SequencerWidget->ResetBreadcrumbs();

	NotifyMovieSceneDataChanged();
	UpdateTimeBoundsToFocusedMovieScene();
}


TSharedRef<FMovieSceneSequenceInstance> FSequencer::GetRootMovieSceneSequenceInstance() const
{
	return SequenceInstanceStack[0];
}


TSharedRef<FMovieSceneSequenceInstance> FSequencer::GetFocusedMovieSceneSequenceInstance() const
{
	// the last item is the focused movie scene
	return SequenceInstanceStack.Top();
}


void FSequencer::FocusSequenceInstance(TSharedRef<FMovieSceneSequenceInstance> SequenceInstance)
{
	// Check for infinite recursion
	check(SequenceInstance != SequenceInstanceStack.Top());

	// Focus the movie scene
	SequenceInstanceStack.Push(SequenceInstance);

	// Reset data that is only used for the previous movie scene
	ResetPerMovieSceneData();

	// Update internal data for the new movie scene
	NotifyMovieSceneDataChanged();
	UpdateTimeBoundsToFocusedMovieScene();

	SequencerWidget->UpdateBreadcrumbs();
}

FGuid FSequencer::CreateBinding(UObject& InObject, const FString& InName)
{
	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();
		
	const FGuid PossessableGuid = OwnerMovieScene->AddPossessable(InName, InObject.GetClass());

	UObject* BindingContext = OwnerSequence->GetParentObject(&InObject);
	if (BindingContext)
	{
		// Ensure we have possessed the outer object, if necessary
		FGuid ParentGuid = GetHandleToObject(BindingContext);
		
		// Set up parent/child guids for possessables within spawnables
		if (ParentGuid.IsValid())
		{
			FMovieScenePossessable* ChildPossessable = OwnerMovieScene->FindPossessable(PossessableGuid);
			if (ensure(ChildPossessable))
			{
				ChildPossessable->SetParent(ParentGuid);
			}

			FMovieSceneSpawnable* ParentSpawnable = OwnerMovieScene->FindSpawnable(ParentGuid);
			if (ParentSpawnable)
			{
				ParentSpawnable->AddChildPossessable(PossessableGuid);
			}
		}
	}
	else
	{
		BindingContext = GetPlaybackContext();
	}

	OwnerSequence->BindPossessableObject(PossessableGuid, InObject, BindingContext);

	return PossessableGuid;
}

UObject* FSequencer::GetPlaybackContext() const
{
	if (bIsEditingWithinLevelEditor)
	{
		return GWorld;
	}
	return nullptr;
}

void FSequencer::GetAllKeyedProperties(UObject& Object, TSet<UProperty*>& OutProperties)
{
	GetAllKeyedPropertiesForInstance(Object, *GetRootMovieSceneSequenceInstance(), OutProperties);
}

void FSequencer::GetAllKeyedPropertiesForInstance(UObject& Object, FMovieSceneSequenceInstance& Instance, TSet<UProperty*>& OutProperties)
{
	GetKeyedProperties(Object, Instance, OutProperties);

	UMovieSceneSubTrack* SubTrack = Instance.GetSequence()->GetMovieScene()->FindMasterTrack<UMovieSceneSubTrack>();
	if (SubTrack)
	{
		// Iterate sub movie scenes
		for (UMovieSceneSection* SubSection : SubTrack->GetAllSections())
		{
			// Find the instance for the section
			TSharedRef<FMovieSceneSequenceInstance>* SubInstance = SequenceInstanceBySection.Find(SubSection);
			GetAllKeyedPropertiesForInstance(Object, SubInstance->Get(), OutProperties);
		}
	}
}

void FSequencer::GetKeyedProperties(UObject& Object, FMovieSceneSequenceInstance& Instance, TSet<UProperty*>& OutProperties)
{
	FGuid BindingId = Instance.FindObjectId(Object);

	const FMovieSceneBinding* Binding = BindingId.IsValid() ? Instance.GetSequence()->GetMovieScene()->GetBindings().FindByPredicate([&](const FMovieSceneBinding& InBinding){
		return BindingId == InBinding.GetObjectGuid();
	}) : nullptr;

	if (!Binding)
	{
		return;
	}

	// Add any properties for this object
	for (UMovieSceneTrack* Track : Binding->GetTracks())
	{
		if (UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Track))
		{
			// Add transform properties manually. This is technically not correct, since it's the scene component that's being keyed, not the actor,
			// But since the transform track is always stored on an actor binding, we have to add them this way
			AActor* Actor = Cast<AActor>(&Object);
			if (Actor)
			{
				UClass* Class = USceneComponent::StaticClass();
				OutProperties.Add(Class->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation)));
				OutProperties.Add(Class->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation)));
				OutProperties.Add(Class->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D)));
			}
		}
		else if (UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track))
		{
			FTrackInstancePropertyBindings InstanceBinding(PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath());
			UProperty* Property = InstanceBinding.GetProperty(&Object);
			if (Property)
			{
				OutProperties.Add(Property);
			}
		}
	}
}

TSharedRef<FMovieSceneSequenceInstance> FSequencer::GetSequenceInstanceForSection(UMovieSceneSection& Section) const
{
	return SequenceInstanceBySection.FindChecked(&Section);
}


void FSequencer::PopToSequenceInstance(TSharedRef<FMovieSceneSequenceInstance> SequenceInstance)
{
	if( SequenceInstanceStack.Num() > 1 )
	{
		// Pop until we find the movie scene to focus
		while( SequenceInstance != SequenceInstanceStack.Last() )
		{
			SequenceInstanceStack.Pop();
		}
	
		check( SequenceInstanceStack.Num() > 0 );

		ResetPerMovieSceneData();
		NotifyMovieSceneDataChanged();
		UpdateTimeBoundsToFocusedMovieScene();
	}
}


void FSequencer::DeleteSections(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections)
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	bool bAnythingRemoved = false;

	FScopedTransaction DeleteSectionTransaction( NSLOCTEXT("Sequencer", "DeleteSection_Transaction", "Delete Section") );

	for (const auto Section : Sections)
	{
		if (!Section.IsValid() || Section->IsLocked())
		{
			continue;
		}

		// if this check fails then the section is outered to a type that doesnt know about the section
		UMovieSceneTrack* Track = CastChecked<UMovieSceneTrack>(Section->GetOuter());
		{
			Track->SetFlags(RF_Transactional);
			Track->Modify();
			Track->RemoveSection(*Section);
		}

		bAnythingRemoved = true;
		Selection.RemoveFromSelection(Section.Get());
	}

	if (bAnythingRemoved)
	{
		// Full refresh required just in case the last section was removed from any track.
		NotifyMovieSceneDataChanged();
	}
}


void FSequencer::DeleteSelectedKeys()
{
	FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteSelectedKeys_Transaction", "Delete Selected Keys") );
	bool bAnythingRemoved = false;
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				Key.KeyArea->DeleteKey(Key.KeyHandle.GetValue());
				bAnythingRemoved = true;
			}
		}
	}

	if (bAnythingRemoved)
	{
		UpdateRuntimeInstances();
	}

	Selection.EmptySelectedKeys();
}


void FSequencer::SetInterpTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode)
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	if (SelectedKeysArray.Num() == 0)
	{
		return;
	}

	FScopedTransaction SetInterpTangentModeTransaction(NSLOCTEXT("Sequencer", "SetInterpTangentMode_Transaction", "Set Interpolation and Tangent Mode"));
	bool bAnythingChanged = false;

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				Key.KeyArea->SetKeyInterpMode(Key.KeyHandle.GetValue(), InterpMode);
				Key.KeyArea->SetKeyTangentMode(Key.KeyHandle.GetValue(), TangentMode);
				bAnythingChanged = true;
			}
		}
	}

	if (bAnythingChanged)
	{
		UpdateRuntimeInstances();
	}
}


bool FSequencer::IsInterpTangentModeSelected(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	bool bAllSelected = false;
	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			bAllSelected = true;
			if (Key.KeyArea->GetKeyInterpMode(Key.KeyHandle.GetValue()) != InterpMode || 
				Key.KeyArea->GetKeyTangentMode(Key.KeyHandle.GetValue()) != TangentMode)
			{
				bAllSelected = false;
				break;
			}
		}
	}
	return bAllSelected;
}


void FSequencer::SnapToFrame()
{
	FScopedTransaction SnapToFrameTransaction(NSLOCTEXT("Sequencer", "SnapToFrame_Transaction", "Snap Selected Keys to Frame"));
	bool bAnythingChanged = false;
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				float NewKeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());

				// Convert to frame
				float FrameRate = 1.0f / Settings->GetTimeSnapInterval();
				int32 NewFrame = SequencerHelpers::TimeToFrame(NewKeyTime, FrameRate);

				// Convert back to time
				NewKeyTime = SequencerHelpers::FrameToTime(NewFrame, FrameRate);

				Key.KeyArea->SetKeyTime(Key.KeyHandle.GetValue(), NewKeyTime);
				bAnythingChanged = true;
			}
		}
	}

	if (bAnythingChanged)
	{
		UpdateRuntimeInstances();
	}
}


bool FSequencer::CanSnapToFrame() const
{
	const bool bKeysSelected = Selection.GetSelectedKeys().Num() > 0;

	return bKeysSelected && CanShowFrameNumbers();
}

void FSequencer::NotifyMapChanged( class UWorld* NewWorld, EMapChangeType MapChangeType )
{
	// @todo sequencer: We should only wipe/respawn puppets that are affected by the world that is being changed! (multi-UWorld support)
	if( ( MapChangeType == EMapChangeType::LoadMap || MapChangeType == EMapChangeType::NewMap ) )
	{
		NotifyMovieSceneDataChanged();
	}
}

void FSequencer::OnActorsDropped( const TArray<TWeakObjectPtr<AActor> >& Actors )
{
	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction(LOCTEXT("UndoPossessingObject", "Possess Object in Sequencer"));
	GetFocusedMovieSceneSequence()->Modify();

	bool bPossessableAdded = false;
	for (TWeakObjectPtr<AActor> WeakActor : Actors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			FGuid ExistingGuid = SequenceInstanceStack.Top()->FindObjectId(*Actor);
			if (!ExistingGuid.IsValid())
			{
				CreateBinding(*Actor, Actor->GetActorLabel());
			}
			bPossessableAdded = true;
		}
	}
	
	if (bPossessableAdded)
	{
		NotifyMovieSceneDataChanged();
	}
}

void FSequencer::NotifyMovieSceneDataChanged()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	bNeedTreeRefresh = true;
}


FAnimatedRange FSequencer::GetViewRange() const
{
	FAnimatedRange AnimatedRange(FMath::Lerp(LastViewRange.GetLowerBoundValue(), TargetViewRange.GetLowerBoundValue(), ZoomCurve.GetLerp()),
		FMath::Lerp(LastViewRange.GetUpperBoundValue(), TargetViewRange.GetUpperBoundValue(), ZoomCurve.GetLerp()));

	if (ZoomAnimation.IsPlaying())
	{
		AnimatedRange.AnimationTarget = TargetViewRange;
	}

	return AnimatedRange;
}

FAnimatedRange FSequencer::GetClampRange() const
{
	UMovieScene* FocussedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	return FocussedMovieScene->GetEditorData().WorkingRange;
}

TRange<float> FSequencer::GetPlaybackRange() const
{
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
}

void FSequencer::SetPlaybackRange(TRange<float> InNewPlaybackRange)
{
	if (ensure(InNewPlaybackRange.HasLowerBound() && InNewPlaybackRange.HasUpperBound() && !InNewPlaybackRange.IsDegenerate()))
	{
		const FScopedTransaction Transaction(LOCTEXT("SetPlaybackRange_Transaction", "Set playback range"));

		UMovieScene* FocussedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
		FocussedMovieScene->SetPlaybackRange(InNewPlaybackRange.GetLowerBoundValue(), InNewPlaybackRange.GetUpperBoundValue());
	}
}

void FSequencer::SetStartPlaybackRange()
{
	SetPlaybackRange(TRange<float>(GetGlobalTime(), GetPlaybackRange().GetUpperBoundValue()));
}

void FSequencer::SetEndPlaybackRange()
{
	SetPlaybackRange(TRange<float>( GetPlaybackRange().GetLowerBoundValue(), GetGlobalTime()));
}

bool FSequencer::GetAutoKeyEnabled() const 
{
	return Settings->GetAutoKeyEnabled();
}

void FSequencer::SetAutoKeyEnabled(bool bAutoKeyEnabled)
{
	Settings->SetAutoKeyEnabled(bAutoKeyEnabled);
}

bool FSequencer::GetKeyAllEnabled() const 
{
	return Settings->GetKeyAllEnabled();
}

void FSequencer::SetKeyAllEnabled(bool bKeyAllEnabled) 
{
	Settings->SetKeyAllEnabled(bKeyAllEnabled);
}

bool FSequencer::GetKeyInterpPropertiesOnly() const 
{
	return Settings->GetKeyInterpPropertiesOnly();
}

void FSequencer::SetKeyInterpPropertiesOnly(bool bKeyInterpPropertiesOnly) 
{
	Settings->SetKeyInterpPropertiesOnly(bKeyInterpPropertiesOnly);
}

EMovieSceneKeyInterpolation FSequencer::GetKeyInterpolation() const
{
	return Settings->GetKeyInterpolation();
}

void FSequencer::SetKeyInterpolation(EMovieSceneKeyInterpolation InKeyInterpolation)
{
	Settings->SetKeyInterpolation(InKeyInterpolation);
}

bool FSequencer::IsRecordingLive() const 
{
	return PlaybackState == EMovieScenePlayerStatus::Recording && GIsPlayInEditorWorld;
}

float FSequencer::GetCurrentLocalTime( UMovieSceneSequence& InMovieSceneSequence )
{
	//@todo Sequencer - Nested movie scenes:  Figure out the parent of the passed in movie scene and 
	// calculate local time
	return ScrubPosition;
}

float FSequencer::GetGlobalTime()
{
	return ScrubPosition;
}

void FSequencer::SetGlobalTime( float NewTime )
{
	if (IsAutoScrollEnabled())
	{
		float RangeOffset = CalculateAutoscrollEncroachment(NewTime).Get(0.f);
			
		// When not scrubbing, we auto scroll the view range immediately
		if (RangeOffset != 0.f)
		{
			TRange<float> WorkingRange = GetClampRange();
			if (TargetViewRange.GetLowerBoundValue() + RangeOffset > WorkingRange.GetLowerBoundValue() &&
				TargetViewRange.GetUpperBoundValue() + RangeOffset < WorkingRange.GetUpperBoundValue())
			{
				SetViewRange(TRange<float>(TargetViewRange.GetLowerBoundValue() + RangeOffset, TargetViewRange.GetUpperBoundValue() + RangeOffset), EViewRangeInterpolation::Immediate);
			}
		}
	}

	SetGlobalTimeDirectly(NewTime);
}

void FSequencer::SetGlobalTimeDirectly( float NewTime )
{
	float LastTime = ScrubPosition;

	// Update the position
	ScrubPosition = NewTime;

	SequenceInstanceStack.Top()->Update(NewTime, LastTime, *this);

	// Ensure any relevant spawned objects are cleaned up if we're playing back the master sequence
	if (SequenceInstanceStack.Num() == 1)
	{
		if (!GetPlaybackRange().Contains(NewTime))
		{
			SpawnRegister->DestroyAllOwnedObjects(*this);
		}
	}

	// If realtime is off, this needs to be called to update the pivot location when scrubbing.
	GUnrealEd->UpdatePivotLocationForSelection();
	GEditor->RedrawLevelEditingViewports();

	OnGlobalTimeChangedDelegate.Broadcast();
}

void FSequencer::UpdateAutoScroll(float NewTime)
{
	float ThresholdPercentage = 0.025f;
	AutoscrollOffset = CalculateAutoscrollEncroachment(NewTime, ThresholdPercentage);

	if (!AutoscrollOffset.IsSet())
	{
		AutoscrubOffset.Reset();
		return;
	}

	TRange<float> ViewRange = GetViewRange();
	const float Threshold = (ViewRange.GetUpperBoundValue() - ViewRange.GetLowerBoundValue()) * ThresholdPercentage;

	// If we have no autoscrub offset yet, we move the scrub position to the boundary of the autoscroll threasdhold, then autoscrub from there
	if (!AutoscrubOffset.IsSet())
	{
		if (AutoscrollOffset.GetValue() < 0 && ScrubPosition > ViewRange.GetLowerBoundValue() + Threshold)
		{
			SetGlobalTimeDirectly( ViewRange.GetLowerBoundValue() + Threshold );
		}
		else if (AutoscrollOffset.GetValue() > 0 && ScrubPosition < ViewRange.GetUpperBoundValue() - Threshold)
		{
			SetGlobalTimeDirectly( ViewRange.GetUpperBoundValue() - Threshold );
		}
	}

	// Don't autoscrub if we're at the extremes of the movie scene range
	const TRange<float>& WorkingRange = GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange;
	if (NewTime < WorkingRange.GetLowerBoundValue() + Threshold ||
		NewTime > WorkingRange.GetUpperBoundValue() - Threshold
		)
	{
		AutoscrubOffset.Reset();
		return;
	}

	// Scrub at the same rate we scroll
	AutoscrubOffset = AutoscrollOffset;
}

TOptional<float> FSequencer::CalculateAutoscrollEncroachment(float NewTime, float ThresholdPercentage) const
{
	enum class EDirection { Positive, Negative };
	const EDirection Movement = NewTime - ScrubPosition >= 0 ? EDirection::Positive : EDirection::Negative;

	const TRange<float> CurrentRange = GetViewRange();
	const float RangeMin = CurrentRange.GetLowerBoundValue(), RangeMax = CurrentRange.GetUpperBoundValue();
	const float AutoScrollThreshold = (RangeMax - RangeMin) * ThresholdPercentage;

	if (Movement == EDirection::Negative && NewTime < RangeMin + AutoScrollThreshold)
	{
		// Scrolling backwards in time, and have hit the threshold
		return NewTime - (RangeMin + AutoScrollThreshold);
	}
	else if (Movement == EDirection::Positive && NewTime > RangeMax - AutoScrollThreshold)
	{
		// Scrolling forwards in time, and have hit the threshold
		return NewTime - (RangeMax - AutoScrollThreshold);
	}
	else
	{
		return TOptional<float>();
	}
}

void FSequencer::SetPerspectiveViewportPossessionEnabled(bool bEnabled)
{
	bPerspectiveViewportPossessionEnabled = bEnabled;
}

void FSequencer::SetPerspectiveViewportCameraCutEnabled(bool bEnabled)
{
	bPerspectiveViewportCameraCutEnabled = bEnabled;
}

FGuid FSequencer::GetHandleToObject( UObject* Object, bool bCreateHandleIfMissing )
{
	if (Object == nullptr)
	{
		return FGuid();
	}

	TSharedRef<FMovieSceneSequenceInstance> FocusedMovieSceneSequenceInstance = GetFocusedMovieSceneSequenceInstance();
	UMovieSceneSequence* FocusedMovieSceneSequence = FocusedMovieSceneSequenceInstance->GetSequence();

	UMovieScene* FocusedMovieScene = FocusedMovieSceneSequence->GetMovieScene();
	
	// Attempt to resolve the object through the movie scene instance first, 
	FGuid ObjectGuid = FocusedMovieSceneSequenceInstance->FindObjectId(*Object);

	// Check here for spawnable otherwise spawnables get recreated as possessables, which doesn't make sense
	FMovieSceneSpawnable* Spawnable = FocusedMovieScene->FindSpawnable(ObjectGuid);
	if (Spawnable)
	{
		return ObjectGuid;
	}

	// Make sure that the possessable is still valid, if it's not remove the binding so new one 
	// can be created.  This can happen due to undo.
	FMovieScenePossessable* Possessable = FocusedMovieScene->FindPossessable(ObjectGuid);
	if(Possessable == nullptr)
	{
		FocusedMovieSceneSequence->UnbindPossessableObjects(ObjectGuid);
		ObjectGuid.Invalidate();
	}

	bool bPossessableAdded = false;

	// If the object guid was not found attempt to add it
	// Note: Only possessed actors can be added like this
	if (!ObjectGuid.IsValid() && FocusedMovieSceneSequence->CanPossessObject(*Object) && bCreateHandleIfMissing)
	{
		AActor* PossessedActor = Cast<AActor>(Object);

		ObjectGuid = CreateBinding(*Object, PossessedActor != nullptr ? PossessedActor->GetActorLabel() : Object->GetName());
		bPossessableAdded = true;
	}

	if (bPossessableAdded)
	{
		NotifyMovieSceneDataChanged();
	}
	
	return ObjectGuid;
}

ISequencerObjectChangeListener& FSequencer::GetObjectChangeListener()
{ 
	return *ObjectChangeListener;
}

void FSequencer::GetRuntimeObjects( TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray< UObject*>& OutObjects ) const
{
	UObject* FoundObject = MovieSceneInstance->FindObject(ObjectHandle, *this);
	if (FoundObject)
	{
		OutObjects.Add(FoundObject);
	}
}

void FSequencer::UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject) const
{
	if (!IsPerspectiveViewportCameraCutEnabled())
	{
		return;
	}

	ACameraActor* UnlockIfCameraActor = Cast<ACameraActor>(UnlockIfCameraObject);

	for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if ((LevelVC == nullptr) || !LevelVC->IsPerspective() || !LevelVC->AllowsCinematicPreview())
		{
			continue;
		}

		if ((CameraObject != nullptr) || LevelVC->IsLockedToActor(UnlockIfCameraActor))
		{
			UpdatePreviewLevelViewportClientFromCameraCut(*LevelVC, CameraObject);
		}			
	}
}

void FSequencer::SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap)
{
	if(!IsPerspectiveViewportPossessionEnabled())
	{
		return;
	}

	for(FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if(LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview())
		{
			if (ViewportParamsMap.Contains(LevelVC))
			{
				const EMovieSceneViewportParams* ViewportParams = ViewportParamsMap.Find(LevelVC);
				if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_FadeAmount)
				{
					LevelVC->FadeAmount = ViewportParams->FadeAmount;
					LevelVC->bEnableFading = true;
				}
				if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_FadeColor)
				{
					LevelVC->FadeColor = ViewportParams->FadeColor.ToRGBE();
					LevelVC->bEnableFading = true;
				}
				if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_ColorScaling)
				{
					LevelVC->bEnableColorScaling = ViewportParams->bEnableColorScaling;
					LevelVC->ColorScale = ViewportParams->ColorScale;
				}
			}
		}
	}
}

void FSequencer::GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const
{
	for(FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if(LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview())
		{
			EMovieSceneViewportParams ViewportParams;
			ViewportParams.FadeAmount = LevelVC->FadeAmount;
			ViewportParams.FadeColor = FLinearColor(LevelVC->FadeColor);
			ViewportParams.ColorScale = LevelVC->ColorScale;

			ViewportParamsMap.Add(LevelVC, ViewportParams);
		}
	}
}

EMovieScenePlayerStatus::Type FSequencer::GetPlaybackStatus() const
{
	return PlaybackState;
}

void FSequencer::AddOrUpdateMovieSceneInstance( UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToAdd )
{
	if( !SequenceInstanceBySection.Contains( &MovieSceneSection ) )
	{
		SequenceInstanceBySection.Add( &MovieSceneSection, InstanceToAdd );
	}
}

void FSequencer::RemoveMovieSceneInstance( UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToRemove )
{
	UMovieSceneSequence* Sequence = InstanceToRemove->GetSequence();

	SpawnRegister->DestroyObjectsSpawnedByInstance(*InstanceToRemove, *this);

	SequenceInstanceBySection.Remove( &MovieSceneSection );
}

void FSequencer::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( Settings );

	for( int32 MovieSceneIndex = 0; MovieSceneIndex < SequenceInstanceStack.Num(); ++MovieSceneIndex )
	{
		UMovieSceneSequence* Sequence = SequenceInstanceStack[MovieSceneIndex]->GetSequence();
		Collector.AddReferencedObject( Sequence );
	}
}

void FSequencer::ResetPerMovieSceneData()
{
	//@todo Sequencer - We may want to preserve selections when moving between movie scenes
	Selection.Empty();

	// @todo run through all tracks for new movie scene changes
	//  needed for audio track decompression
}

void FSequencer::UpdateRuntimeInstances()
{
	// Refresh the current root instance
	SequenceInstanceStack.Top()->RefreshInstance( *this );
	SequenceInstanceStack.Top()->Update(ScrubPosition, ScrubPosition, *this);

	// Ensure any relevant spawned objects are cleaned up if we're playing back the master sequence
	if (SequenceInstanceStack.Num() == 1)
	{
		if (!GetPlaybackRange().Contains(ScrubPosition))
		{
			SpawnRegister->DestroyAllOwnedObjects(*this);
		}
	}

	GEditor->RedrawLevelEditingViewports();
}

FReply FSequencer::OnPlay(bool bTogglePlay)
{
	if( (PlaybackState == EMovieScenePlayerStatus::Playing ||
		 PlaybackState == EMovieScenePlayerStatus::Recording) && bTogglePlay )
	{
		PlaybackState = EMovieScenePlayerStatus::Stopped;
		// Update on stop (cleans up things like sounds that are playing)
		SequenceInstanceStack.Top()->Update( ScrubPosition, ScrubPosition, *this );
	}
	else
	{
		PlaybackState = EMovieScenePlayerStatus::Playing;
		
		// Make sure Slate ticks during playback
		SequencerWidget->RegisterActiveTimerForPlayback();
	}

	return FReply::Handled();
}

FReply FSequencer::OnRecord()
{
	if (PlaybackState != EMovieScenePlayerStatus::Recording)
	{
		PlaybackState = EMovieScenePlayerStatus::Recording;
		
		// Make sure Slate ticks during playback
		SequencerWidget->RegisterActiveTimerForPlayback();

		// @todo sequencer livecapture: Ideally we would support fixed timestep capture from simulation
		//			Basically we need to run the PIE world at a fixed time step, capturing key frames every frame. 
		//			The editor world would still be ticked at the normal throttle-real-time rate
	}
	else
	{
		PlaybackState = EMovieScenePlayerStatus::Stopped;
	}

	return FReply::Handled();
}

FReply FSequencer::OnStepForward()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	float NewPosition = ScrubPosition + Settings->GetTimeSnapInterval();
	if (Settings->GetIsSnapEnabled())
	{
		NewPosition = Settings->SnapTimeToInterval(NewPosition);
	}
	SetGlobalTime(NewPosition);
	return FReply::Handled();
}

FReply FSequencer::OnStepBackward()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	float NewPosition = ScrubPosition - Settings->GetTimeSnapInterval();
	if (Settings->GetIsSnapEnabled())
	{
		NewPosition = Settings->SnapTimeToInterval(NewPosition);
	}
	SetGlobalTime(NewPosition);
	return FReply::Handled();
}

FReply FSequencer::OnStepToEnd()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	SetGlobalTime(GetPlaybackRange().GetUpperBoundValue());
	return FReply::Handled();
}

FReply FSequencer::OnStepToBeginning()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	SetGlobalTime(GetPlaybackRange().GetLowerBoundValue());
	return FReply::Handled();
}

FReply FSequencer::OnToggleLooping()
{
	Settings->SetLooping(!Settings->IsLooping());
	return FReply::Handled();
}

bool FSequencer::IsLooping() const
{
	return Settings->IsLooping();
}

void FSequencer::SetGlobalTimeLooped(float InTime)
{
	if (Settings->IsLooping())
	{
		const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
		if (FocusedSequence)
		{
			if (InTime > FocusedSequence->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue())
			{
				InTime -= FocusedSequence->GetMovieScene()->GetPlaybackRange().Size<float>();
			}
		}
	}
	else
	{
		TRange<float> TimeBounds = GetTimeBounds();
		TRange<float> WorkingRange = GetClampRange();

		const bool bReachedEnd = GetGlobalTime() < TimeBounds.GetUpperBoundValue() && InTime >= TimeBounds.GetUpperBoundValue();

		// Stop if we hit the playback range end
		if (bReachedEnd)
		{
			InTime = TimeBounds.GetUpperBoundValue();
			PlaybackState = EMovieScenePlayerStatus::Stopped;
		}
		// Constrain to the play range if necessary
		else if (Settings->ShouldKeepCursorInPlayRange())
		{
			// Clamp to lower bound
			InTime = FMath::Max(InTime, TimeBounds.GetLowerBoundValue());

			// Jump back if necessary
			if (InTime >= TimeBounds.GetUpperBoundValue())
			{
				InTime = TimeBounds.GetLowerBoundValue();
			}
		}
		// Ensure the time is within the working range
		else if (!WorkingRange.Contains(InTime))
		{
			InTime = FMath::Clamp(InTime, WorkingRange.GetLowerBoundValue(), WorkingRange.GetUpperBoundValue());
			PlaybackState = EMovieScenePlayerStatus::Stopped;
		}
	}

	SetGlobalTime(InTime);
}

bool FSequencer::CanShowFrameNumbers() const
{
	return SequencerSnapValues::IsTimeSnapIntervalFrameRate(Settings->GetTimeSnapInterval());
}

EPlaybackMode::Type FSequencer::GetPlaybackMode() const
{
	return PlaybackState == EMovieScenePlayerStatus::Playing ? EPlaybackMode::PlayingForward :
		PlaybackState == EMovieScenePlayerStatus::Recording ? EPlaybackMode::Recording :
		EPlaybackMode::Stopped;
}

void FSequencer::UpdateTimeBoundsToFocusedMovieScene()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// Set the view range to:
	// 1. The moviescene view range
	// 2. The moviescene playback range
	// 3. Some sensible default
	TRange<float> NewRange = FocusedMovieScene->GetEditorData().ViewRange;

	if (NewRange.IsEmpty() || NewRange.IsDegenerate())
	{
		NewRange = FocusedMovieScene->GetPlaybackRange();
	}
	if (NewRange.IsEmpty() || NewRange.IsDegenerate())
	{
		NewRange = TRange<float>(0.f, 5.f);
	}

	// Set the view range to the new range
	SetViewRange(NewRange, EViewRangeInterpolation::Immediate);

	// Make sure the current time is within the bounds
	if (!TargetViewRange.Contains(ScrubPosition))
	{
		ScrubPosition = LastViewRange.GetLowerBoundValue();
		OnGlobalTimeChangedDelegate.Broadcast();
	}
}

TRange<float> FSequencer::GetTimeBounds() const
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();

	// When recording, we never want to constrain the time bound range.  You might not even have any sections or keys yet
	// but we need to be able to move the time cursor during playback so you can capture data in real-time
	if( PlaybackState == EMovieScenePlayerStatus::Recording || !FocusedSequence)
	{
		return TRange<float>( -100000.0f, 100000.0f );
	}
	
	return FocusedSequence->GetMovieScene()->GetPlaybackRange();
}

void FSequencer::SetViewRange(TRange<float> NewViewRange, EViewRangeInterpolation Interpolation)
{
	if (!ensure(NewViewRange.HasUpperBound() && NewViewRange.HasLowerBound() && !NewViewRange.IsDegenerate()))
	{
		return;
	}

	const float AnimationLengthSeconds = Interpolation == EViewRangeInterpolation::Immediate ? 0.f : 0.1f;
	if (AnimationLengthSeconds != 0.f)
	{
		if (ZoomAnimation.GetCurve(0).DurationSeconds != AnimationLengthSeconds)
		{
			ZoomAnimation = FCurveSequence();
			ZoomCurve = ZoomAnimation.AddCurve(0.f, AnimationLengthSeconds, ECurveEaseFunction::QuadIn);
		}

		if (!ZoomAnimation.IsPlaying())
		{
			LastViewRange = TargetViewRange;
			ZoomAnimation.Play( SequencerWidget.ToSharedRef() );
		}
		TargetViewRange = NewViewRange;
	}
	else
	{
		TargetViewRange = LastViewRange = NewViewRange;
		ZoomAnimation.JumpToEnd();
	}


	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			FocusedMovieScene->GetEditorData().ViewRange = TargetViewRange;

			// Always ensure the working range is big enough to fit the view range
			TRange<float>& WorkingRange = FocusedMovieScene->GetEditorData().WorkingRange;

			WorkingRange = TRange<float>(
				FMath::Min(TargetViewRange.GetLowerBoundValue(), WorkingRange.GetLowerBoundValue()),
				FMath::Max(TargetViewRange.GetUpperBoundValue(), WorkingRange.GetUpperBoundValue())
				);
		}
	}
}

void FSequencer::OnClampRangeChanged( TRange<float> NewClampRange )
{
	if (!NewClampRange.IsEmpty())
	{
		GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange = NewClampRange;
	}
}

void FSequencer::OnScrubPositionChanged( float NewScrubPosition, bool bScrubbing )
{
	bool bClampToViewRange = true;

	if (PlaybackState == EMovieScenePlayerStatus::Scrubbing)
	{
		if (!bScrubbing)
		{
			OnEndScrubbing();
		}
		else if (IsAutoScrollEnabled())
		{
			// Clamp to the view range when not auto-scrolling
			bClampToViewRange = false;
	
			UpdateAutoScroll(NewScrubPosition);
			
			// When scrubbing, we animate auto-scrolled scrub position in Tick()
			if (AutoscrubOffset.IsSet())
			{
				return;
			}
		}
	}

	if (bClampToViewRange)
	{
		NewScrubPosition = FMath::Clamp(NewScrubPosition, TargetViewRange.GetLowerBoundValue(), TargetViewRange.GetUpperBoundValue());
	}

	SetGlobalTimeDirectly( NewScrubPosition );
}

void FSequencer::OnBeginScrubbing()
{
	PlaybackState = EMovieScenePlayerStatus::Scrubbing;
	SequencerWidget->RegisterActiveTimerForPlayback();
}

void FSequencer::OnEndScrubbing()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	AutoscrubOffset.Reset();
	StopAutoscroll();
}

void FSequencer::OnBeginPlaybackRangeDrag()
{
	GEditor->BeginTransaction(LOCTEXT("SetPlaybackRange_Transaction", "Set playback range"));
}

void FSequencer::OnEndPlaybackRangeDrag()
{
	GEditor->EndTransaction();
}

void FSequencer::StartAutoscroll(float UnitsPerS)
{
	AutoscrollOffset = UnitsPerS;
}

void FSequencer::StopAutoscroll()
{
	AutoscrollOffset.Reset();
}

void FSequencer::OnToggleAutoScroll()
{
	Settings->SetAutoScrollEnabled(!Settings->GetAutoScrollEnabled());
}

bool FSequencer::IsAutoScrollEnabled() const
{
	return Settings->GetAutoScrollEnabled();
}

void FSequencer::FindInContentBrowser()
{
	if (GetFocusedMovieSceneSequence())
	{
		TArray<UObject*> ObjectsToFocus;
		ObjectsToFocus.Add(GetCurrentAsset());

		GEditor->SyncBrowserToObjects(ObjectsToFocus);
	}
}

UObject* FSequencer::GetCurrentAsset() const
{
	// For now we find the asset by looking at the root movie scene's outer.
	// @todo: this may need refining if/when we support editing movie scene instances
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetOuter();
}

void FSequencer::VerticalScroll(float ScrollAmountUnits)
{
	SequencerWidget->GetTreeView()->ScrollByDelta(ScrollAmountUnits);
}

FGuid FSequencer::AddSpawnableForAssetOrClass( UObject* Object )
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	if (!Sequence->AllowsSpawnableObjects())
	{
		return FGuid();
	}

	// Grab the MovieScene that is currently focused.  We'll add our Blueprint as an inner of the
	// MovieScene asset.
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );

	// Use the class as the spawnable's name if this is an actor class, otherwise just use the object name (asset)
	const bool bIsActorClass = Object->IsA( AActor::StaticClass() ) && !Object->HasAnyFlags( RF_ArchetypeObject );
	const FName AssetName = bIsActorClass ? Object->GetClass()->GetFName() : Object->GetFName();

	// Inner objects don't need a name (it will be auto-generated by the UObject system), but we want one in this case
	// because the class of any actors that are created from this Blueprint may end up being user-facing.
	const FName BlueprintName = MakeUniqueObjectName( OwnerMovieScene, UBlueprint::StaticClass(), AssetName );

	// Use the asset name as the initial spawnable name
	const FString NewSpawnableName = AssetName.ToString();		// @todo sequencer: Need UI to allow user to rename these slots

	FText ErrorText;

	// Create our new blueprint!
	UBlueprint* NewBlueprint = nullptr;

	// If we're dropping a blueprint, create a new blueprint out of this one, so we can propagate changes from the world to it without affecting the dragged BP.
	if (UBlueprint* SourceBlueprint = Cast<UBlueprint>(Object))
	{
		UClass* SourceParentClass = SourceBlueprint->GeneratedClass;

		if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(SourceParentClass))
		{
			ErrorText = FText::Format(LOCTEXT("UnableToAddSpawnableBlueprint", "Unable to add spawnable for class of type '{0}' since it is not a valid blueprint parent class."),
				FText::FromString(SourceParentClass->GetName())
				);
		}
		else
		{
			NewBlueprint = FKismetEditorUtilities::CreateBlueprint(SourceParentClass, OwnerMovieScene, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		}
	}
	else
	{
		// @todo sequencer: Add support for forcing specific factories for an asset
		UActorFactory* FactoryToUse = nullptr;
		if( bIsActorClass )
		{
			// Placing an actor class directly::
			FactoryToUse = GEditor->FindActorFactoryForActorClass( Object->GetClass() );
		}
		else
		{
			// Placing an asset
			FactoryToUse = FActorFactoryAssetProxy::GetFactoryForAssetObject( Object );
		}

		if( FactoryToUse != nullptr )
		{
			// Create the blueprint
			NewBlueprint = FactoryToUse->CreateBlueprint( Object, OwnerMovieScene, BlueprintName );
		}
		else if( bIsActorClass )
		{
			// We don't have a factory, but we can still try to create a blueprint for this actor class
			NewBlueprint = FKismetEditorUtilities::CreateBlueprint( Object->GetClass(), OwnerMovieScene, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass() );
		}
	}

	if (!NewBlueprint)
	{
		if (ErrorText.IsEmpty())
		{
			ErrorText = FText::Format(LOCTEXT("UnableToAddSpawnable", "Unable to add spawnable for '{0}'."),
				FText::FromString(Object->GetName())
				);
		}

		FNotificationInfo Info(ErrorText);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FGuid();
	}

	FTransformData NewTransform;

	if ((NewBlueprint->GeneratedClass != nullptr) && FBlueprintEditorUtils::IsActorBased(NewBlueprint))
	{
		AActor* ActorCDO = CastChecked< AActor >( NewBlueprint->GeneratedClass->ClassDefaultObject );
		// Place the new spawnable in front of the camera (unless we were automatically created from a PIE actor)
		if (Settings->GetSpawnPosition() == SSP_PlaceInFrontOfCamera)
		{
			PlaceActorInFrontOfCamera( ActorCDO );
		}
		NewTransform.Translation = ActorCDO->GetActorLocation();
		NewTransform.Rotation = ActorCDO->GetActorRotation();
		NewTransform.Scale = FVector(1.0f, 1.0f, 1.0f);
		NewTransform.bValid = true;
	}

	FGuid NewSpawnableGuid = OwnerMovieScene->AddSpawnable(NewSpawnableName, NewBlueprint);
	if (NewSpawnableGuid.IsValid())
	{
		UMovieSceneSpawnTrack* SpawnTrack = Cast<UMovieSceneSpawnTrack>(OwnerMovieScene->AddTrack(UMovieSceneSpawnTrack::StaticClass(), NewSpawnableGuid));
		if (SpawnTrack)
		{
			SpawnTrack->AddSection(*SpawnTrack->CreateNewSection());
			SpawnTrack->SetObjectId(NewSpawnableGuid);
		}
						
		if (NewTransform.bValid)
		{
			UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(OwnerMovieScene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), NewSpawnableGuid));
			if (TransformTrack)
			{
				static FName Transform("Transform");
				TransformTrack->SetPropertyNameAndPath(Transform, Transform.ToString());
				const bool bUnwindRotation = false;

				float Time = GetCurrentLocalTime( *Sequence );
				EMovieSceneKeyInterpolation Interpolation = GetKeyInterpolation();

				UMovieScene3DTransformSection* NewSection = CastChecked<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());

				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Translation, EKey3DTransformChannel::Default, EAxis::X, NewTransform.Translation.X, bUnwindRotation ) );
				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Translation, EKey3DTransformChannel::Default, EAxis::Y, NewTransform.Translation.Y, bUnwindRotation ) );
				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Translation, EKey3DTransformChannel::Default, EAxis::Z, NewTransform.Translation.Z, bUnwindRotation ) );

				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Rotation, EKey3DTransformChannel::Default, EAxis::X, NewTransform.Rotation.Euler().X, bUnwindRotation ) );
				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Rotation, EKey3DTransformChannel::Default, EAxis::Y, NewTransform.Rotation.Euler().Y, bUnwindRotation ) );
				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Rotation, EKey3DTransformChannel::Default, EAxis::Z, NewTransform.Rotation.Euler().Z, bUnwindRotation ) );

				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Scale, EKey3DTransformChannel::Default, EAxis::X, NewTransform.Scale.X, bUnwindRotation ) );
				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Scale, EKey3DTransformChannel::Default, EAxis::Y, NewTransform.Scale.Y, bUnwindRotation ) );
				NewSection->SetDefault( FTransformKey( EKey3DTransformChannel::Scale, EKey3DTransformChannel::Default, EAxis::Z, NewTransform.Scale.Z, bUnwindRotation ) );

				TransformTrack->AddSection(*NewSection);
			}
		}
	}

	return NewSpawnableGuid;
}

void FSequencer::AddSubSequence(UMovieSceneSequence* Sequence)
{
	// @todo Sequencer - sub-moviescenes This should be moved to the sub-moviescene editor

	// Grab the MovieScene that is currently focused.  THis is the movie scene that will contain the sub-moviescene
	UMovieScene* OwnerMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );

	OwnerMovieScene->Modify();

	UMovieSceneSubTrack* SubTrack = OwnerMovieScene->AddMasterTrack<UMovieSceneSubTrack>();
	SubTrack->AddSequence(*Sequence, ScrubPosition);
}

bool FSequencer::OnHandleAssetDropped(UObject* DroppedAsset, const FGuid& TargetObjectGuid)
{
	bool bWasConsumed = false;
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		bool bWasHandled = TrackEditors[i]->HandleAssetAdded(DroppedAsset, TargetObjectGuid);
		if (bWasHandled)
		{
			// @todo Sequencer - This will crash if multiple editors try to handle a single asset
			// Should we allow this? How should it consume then?
			// gmp 10/7/2015: the user should be presented with a dialog asking what kind of track they want to create
			check(!bWasConsumed);
			bWasConsumed = true;
		}
	}
	return bWasConsumed;
}

// Takes a display node and traverses it's parents to find the nearest track node if any.  Also collects the names of the nodes which make
// up the path from the track node to the display node being checked.  The name path includes the name of the node being checked, but not
// the name of the track node.
void GetParentTrackNodeAndNamePath(TSharedRef<const FSequencerDisplayNode> DisplayNode, TSharedPtr<FSequencerTrackNode>& OutParentTrack, TArray<FName>& OutNamePath )
{
	TArray<FName> PathToTrack;
	PathToTrack.Add( DisplayNode->GetNodeName() );
	TSharedPtr<FSequencerDisplayNode> CurrentParent = DisplayNode->GetParent();
	while ( CurrentParent.IsValid() && CurrentParent->GetType() != ESequencerNode::Track )
	{
		PathToTrack.Add( CurrentParent->GetNodeName() );
		CurrentParent = CurrentParent->GetParent();
	}
	if ( CurrentParent.IsValid() )
	{
		OutParentTrack = StaticCastSharedPtr<FSequencerTrackNode>( CurrentParent );
		for ( int32 i = PathToTrack.Num() - 1; i >= 0; i-- )
		{
			OutNamePath.Add( PathToTrack[i] );
		}
	}
}

void FSequencer::OnRequestNodeDeleted( TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted )
{
	bool bAnythingRemoved = false;
	
	TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance = GetFocusedMovieSceneSequenceInstance();
	UMovieSceneSequence* Sequence = MovieSceneInstance->GetSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	// Only object nodes or section areas can be deleted
	if (NodeToBeDeleted->GetType() == ESequencerNode::Object)
	{
		// Delete any child object bindings
		for (const TSharedRef<FSequencerDisplayNode>& ChildNode : NodeToBeDeleted->GetChildNodes())
		{
			if (ChildNode->GetType() == ESequencerNode::Object)
			{
				OnRequestNodeDeleted(ChildNode);
			}
		}

		const FGuid& BindingToRemove = StaticCastSharedRef<const FSequencerObjectBindingNode>( NodeToBeDeleted )->GetObjectBinding();
		
		// Try to remove as a spawnable first
		if (OwnerMovieScene->RemoveSpawnable(BindingToRemove))
		{
			SpawnRegister->DestroySpawnedObject(BindingToRemove, *MovieSceneInstance, *this);
		}
		// The guid should be associated with a possessable if it wasnt a spawnable
		else if (OwnerMovieScene->RemovePossessable(BindingToRemove))
		{
			Sequence->Modify();
			Sequence->UnbindPossessableObjects( BindingToRemove );
		}
		else
		{
			// If this check fails there was an invalid guid being stored on a node
			checkf(false, TEXT("Guid was not associated with a spawnable or possessable"));
		}

		bAnythingRemoved = true;
	}
	else if( NodeToBeDeleted->GetType() == ESequencerNode::Track  )
	{
		TSharedRef<const FSequencerTrackNode> SectionAreaNode = StaticCastSharedRef<const FSequencerTrackNode>( NodeToBeDeleted );
		UMovieSceneTrack* Track = SectionAreaNode->GetTrack();

		if (Track != nullptr)
		{
			if (OwnerMovieScene->IsAMasterTrack(*Track))
			{
				OwnerMovieScene->RemoveMasterTrack(*Track);
			}
			else if (OwnerMovieScene->GetShotTrack() == Track)
			{
				OwnerMovieScene->RemoveShotTrack();
			}
			else
			{
				OwnerMovieScene->RemoveTrack(*Track);
			}
		
			bAnythingRemoved = true;
		}
	}
	else if ( NodeToBeDeleted->GetType() == ESequencerNode::Category )
	{
		TSharedPtr<FSequencerTrackNode> ParentTrackNode;
		TArray<FName> PathFromTrack;
		GetParentTrackNodeAndNamePath(NodeToBeDeleted, ParentTrackNode, PathFromTrack);
		if ( ParentTrackNode.IsValid() )
		{
			for ( TSharedRef<ISequencerSection> Section : ParentTrackNode->GetSections() )
			{
				bAnythingRemoved |= Section->RequestDeleteCategory( PathFromTrack );
			}
		}
	}
	else if ( NodeToBeDeleted->GetType() == ESequencerNode::KeyArea )
	{
		TSharedPtr<FSequencerTrackNode> ParentTrackNode;
		TArray<FName> PathFromTrack;
		GetParentTrackNodeAndNamePath( NodeToBeDeleted, ParentTrackNode, PathFromTrack );
		if ( ParentTrackNode.IsValid() )
		{
			for ( TSharedRef<ISequencerSection> Section : ParentTrackNode->GetSections() )
			{
				bAnythingRemoved |= Section->RequestDeleteKeyArea( PathFromTrack );
			}
		}
	}

	if( bAnythingRemoved )
	{
		NotifyMovieSceneDataChanged();
	}
}

void FSequencer::PlaceActorInFrontOfCamera( AActor* ActorCDO )
{
	// Place the actor in front of the active perspective camera if we have one
	if ((GCurrentLevelEditingViewportClient != nullptr) && GCurrentLevelEditingViewportClient->IsPerspective())
	{
		// Don't allow this when the active viewport is showing a simulation/PIE level
		const bool bIsViewportShowingPIEWorld = GCurrentLevelEditingViewportClient->GetWorld()->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
		if( !bIsViewportShowingPIEWorld )
		{
			// @todo sequencer actors: Ideally we could use the actor's collision to figure out how far to push out
			// the object (like when placing in viewports), but we can't really do that because we're only dealing with a CDO
			const float DistanceFromCamera = 50.0f;

			// Find a place to put the object
			// @todo sequencer cleanup: This code should be reconciled with the GEditor->MoveActorInFrontOfCamera() stuff
			const FVector& CameraLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
			FRotator CameraRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
			const FVector CameraDirection = CameraRotation.Vector();

			FVector NewLocation = CameraLocation + CameraDirection * ( DistanceFromCamera + GetDefault<ULevelEditorViewportSettings>()->BackgroundDropDistance );
			FSnappingUtils::SnapPointToGrid( NewLocation, FVector::ZeroVector );

			CameraRotation.Roll = 0.f;
			CameraRotation.Pitch =0.f;

			ActorCDO->SetActorRelativeLocation( NewLocation );
			ActorCDO->SetActorRelativeRotation( CameraRotation );
		}
	}
}


void FSequencer::CopyActorProperties( AActor* PuppetActor, AActor* TargetActor ) const
{
	// @todo sequencer: How do we make this undoable with the original action that caused the change in the first place? 
	//       -> Ideally we are still within the transaction scope while performing this change

	// @todo sequencer: If we decide to propagate changes BACK to an actor after changing the CDO, we may need to
	// do additional work to make sure transient state is refreshed (e.g. call PostEditMove() is transform is changed)

	// @todo sequencer: Pass option here to call PostEditChange, if needed
	const int32 CopiedPropertyCount = EditorUtilities::CopyActorProperties( PuppetActor, TargetActor );
}


void FSequencer::GetActorRecordingState( bool& bIsRecording /* In+Out */ ) const
{
	if( IsRecordingLive() )
	{
		bIsRecording = true;
	}
}

void FSequencer::PostUndo(bool bSuccess)
{
	NotifyMovieSceneDataChanged();
}

void FSequencer::OnPreSaveWorld(uint32 SaveFlags, class UWorld* World)
{
	// Restore the saved state so that the level save can save that instead of the animated state.
	RootMovieSceneSequenceInstance->RestoreState(*this);
}

void FSequencer::OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess)
{
	// Reset the time after saving so that an update will be triggered to put objects back to their animated state.
	RootMovieSceneSequenceInstance->RefreshInstance(*this);
	SetGlobalTime(GetGlobalTime());
}

void FSequencer::OnNewCurrentLevel()
{
	ActivateSequencerEditorMode();
}

void FSequencer::OnMapOpened(const FString& Filename, bool bLoadAsTemplate)
{
	ActivateSequencerEditorMode();
}


void FSequencer::ActivateSequencerEditorMode()
{
	if( GLevelEditorModeTools().IsModeActive( FSequencerEdMode::EM_SequencerMode ) )
	{
		GLevelEditorModeTools().DeactivateMode( FSequencerEdMode::EM_SequencerMode );
	}

	GLevelEditorModeTools().ActivateMode( FSequencerEdMode::EM_SequencerMode );

	FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(GLevelEditorModeTools().GetActiveMode(FSequencerEdMode::EM_SequencerMode));
	SequencerEdMode->SetSequencer(ActiveSequencer.Get());
}

void FSequencer::OnPreBeginPIE(bool bIsSimulating)
{
	RootMovieSceneSequenceInstance->RestoreState(*this);
}

void FSequencer::OnEndPIE(bool bIsSimulating)
{
	UpdateRuntimeInstances();
}

void FSequencer::ActivateDetailKeyframeHandler()
{			
	// Add sequencer detail keyframe handler
	static const FName DetailsTabIdentifiers[] = { "LevelEditorSelectionDetails", "LevelEditorSelectionDetails2", "LevelEditorSelectionDetails3", "LevelEditorSelectionDetails4" };
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for(const FName& DetailsTabIdentifier : DetailsTabIdentifiers)
	{
		TSharedPtr<IDetailsView> DetailsView = EditModule.FindDetailView(DetailsTabIdentifier);

		if(DetailsView.IsValid())
		{
			DetailsView->SetKeyframeHandler(DetailKeyframeHandler);
		}
	}
}

void FSequencer::DeactivateDetailKeyframeHandler()
{
	static const FName DetailsTabIdentifiers[] = { "LevelEditorSelectionDetails", "LevelEditorSelectionDetails2", "LevelEditorSelectionDetails3", "LevelEditorSelectionDetails4" };
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for(const FName& DetailsTabIdentifier : DetailsTabIdentifiers)
	{
		TSharedPtr<IDetailsView> DetailsView = EditModule.FindDetailView(DetailsTabIdentifier);

		if(DetailsView.IsValid())
		{
			if (DetailsView->GetKeyframeHandler() == DetailKeyframeHandler)
			{
				DetailsView->SetKeyframeHandler(0);
			}
		}
	}
}

void FSequencer::OnPropertyEditorOpened()
{
	ActivateDetailKeyframeHandler();
}

void FSequencer::UpdatePreviewLevelViewportClientFromCameraCut(FLevelEditorViewportClient& InViewportClient, UObject* InCameraObject) const
{
	ACameraActor* CameraActor = Cast<ACameraActor>(InCameraObject);

	if (CameraActor)
	{
		InViewportClient.SetViewLocation(CameraActor->GetActorLocation());
		InViewportClient.SetViewRotation(CameraActor->GetActorRotation());
		InViewportClient.bEditorCameraCut = !InViewportClient.IsLockedToActor(CameraActor);
	}
	else
	{
		InViewportClient.ViewFOV = InViewportClient.FOVAngle;
		InViewportClient.bEditorCameraCut = false;
	}

	// Set the actor lock.
	InViewportClient.SetMatineeActorLock(CameraActor);
	InViewportClient.bLockedCameraView = CameraActor != nullptr;
	InViewportClient.RemoveCameraRoll();

	// If viewing through a camera - enforce aspect ratio.
	if (CameraActor)
	{
		if (CameraActor->GetCameraComponent()->AspectRatio == 0)
		{
			InViewportClient.AspectRatio = 1.7f;
		}
		else
		{
			InViewportClient.AspectRatio = CameraActor->GetCameraComponent()->AspectRatio;
		}

		//don't stop the camera from zooming when not playing back
		InViewportClient.ViewFOV = CameraActor->GetCameraComponent()->FieldOfView;

		// If there are selected actors, invalidate the viewports hit proxies, otherwise they won't be selectable afterwards
		if (InViewportClient.Viewport && GEditor->GetSelectedActorCount() > 0)
		{
			InViewportClient.Viewport->InvalidateHitProxy();
		}
	}

	// Update ControllingActorViewInfo, so it is in sync with the updated viewport
	InViewportClient.UpdateViewForLockedActor();
}

void FSequencer::SaveCurrentMovieScene()
{
	UPackage* MovieScenePackage = GetCurrentAsset()->GetOutermost();

	TArray<UPackage*> PackagesToSave;

	PackagesToSave.Add( MovieScenePackage );

	FEditorFileUtils::PromptForCheckoutAndSave( PackagesToSave, false, false );
}

void FSequencer::AddSelectedObjects()
{
	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	TArray<AActor*> SelectedActors;

	USelection* CurrentSelection = GEditor->GetSelectedActors();
	TArray<UObject*> SelectedObjects;
	CurrentSelection->GetSelectedObjects( AActor::StaticClass(), SelectedObjects );
	for (TArray<UObject*>::TIterator It(SelectedObjects); It; ++It)
	{
		AActor* Actor = CastChecked<AActor>(*It);

		if (Actor != nullptr)
		{
			SelectedActors.Add(Actor);
		}
	}

	bool bPossessableAdded = false;
	if (SelectedActors.Num() != 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoPossessingObject", "Possess Object in Sequencer"));
		OwnerSequence->Modify();

		for (auto Actor : SelectedActors)
		{
			FGuid ExistingGuid = SequenceInstanceStack.Top()->FindObjectId(*Actor);
			if (!ExistingGuid.IsValid())
			{
				CreateBinding(*Actor, Actor->GetActorLabel());
				bPossessableAdded = true;
			}
		}
	}

	if (bPossessableAdded)
	{
		NotifyMovieSceneDataChanged();
	}
}

void FSequencer::OnSectionSelectionChanged()
{
}

void FSequencer::OnSelectedOutlinerNodesChanged()
{
	if (IsLevelEditorSequencer())
	{
		// Clear the selection if no sequencer display nodes were clicked on and there are selected actors
		if (Selection.GetSelectedOutlinerNodes().Num() == 0 && GEditor->GetSelectedActors()->Num() != 0 && SequencerWidget->UserIsSelecting())
		{
			const bool bNotifySelectionChanged = true;
			const bool bDeselectBSP = true;
			const bool bWarnAboutTooManyActors = false;

			const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ClickingOnActors", "Clicking on Actors"));
			GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
		}
	}
}

void FSequencer::ZoomToSelectedSections()
{
	TArray< TRange<float> > Bounds;
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : Selection.GetSelectedSections())
	{
		Bounds.Add(SelectedSection->GetRange());
	}
	TRange<float> BoundsHull = TRange<float>::Hull(Bounds);

	if (BoundsHull.IsEmpty())
	{
		BoundsHull = GetTimeBounds();
	}

	if (!BoundsHull.IsEmpty() && !BoundsHull.IsDegenerate())
	{
		// Zoom back to last view range if already expanded
		if (!ViewRangeBeforeZoom.IsEmpty() &&
			FMath::IsNearlyEqual(BoundsHull.GetLowerBoundValue(), GetViewRange().GetLowerBoundValue(), KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(BoundsHull.GetUpperBoundValue(), GetViewRange().GetUpperBoundValue(), KINDA_SMALL_NUMBER))
		{
			SetViewRange(ViewRangeBeforeZoom, EViewRangeInterpolation::Animated);
		}
		else
		{
			ViewRangeBeforeZoom = GetViewRange();

			SetViewRange(BoundsHull, EViewRangeInterpolation::Animated);
		}
	}
}

bool FSequencer::CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const
{
	return ObjectChangeListener->CanKeyProperty(CanKeyPropertyParams);
} 

void FSequencer::KeyProperty(FKeyPropertyParams KeyPropertyParams) 
{
	ObjectChangeListener->KeyProperty(KeyPropertyParams);
}

FSequencerSelection& FSequencer::GetSelection()
{
	return Selection;
}

FSequencerSelectionPreview& FSequencer::GetSelectionPreview()
{
	return SelectionPreview;
}

float FSequencer::GetOverlayFadeCurve() const
{
	return OverlayCurve.GetLerp();
}

void FSequencer::DeleteSelectedItems()
{
	if (Selection.GetActiveSelection() == FSequencerSelection::EActiveSelection::KeyAndSection)
	{
		FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteKeysAndSections_Transaction", "Delete Keys and Sections") );
		
		DeleteSelectedKeys();

		DeleteSections(Selection.GetSelectedSections());
	}
	else if (Selection.GetActiveSelection() == FSequencerSelection::EActiveSelection::OutlinerNode)
	{
		DeleteSelectedNodes();
	}
}

void FSequencer::AssignActor(FMenuBuilder& MenuBuilder, FGuid InObjectBinding)
{
	UObject* RuntimeObject = SequenceInstanceStack.Top()->FindObject(InObjectBinding, *this);

	auto IsActorValidForAssignment = [=](const AActor* InActor, UObject* CurrentObject){
		return CurrentObject != InActor;
	};

	using namespace SceneOutliner;

	// Set up a menu entry to assign an actor to the object binding node
	FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

		// We hide the header row to keep the UI compact.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display actors that are not possessed already
		InitOptions.Filters->AddFilterPredicate( FActorFilterPredicate::CreateLambda( IsActorValidForAssignment, RuntimeObject ) );
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400.0f)
		[
			SceneOutlinerModule.CreateSceneOutliner(
				InitOptions,
				FOnActorPicked::CreateLambda([=](AActor* Actor){
					// Create a new binding for this actor
					FSlateApplication::Get().DismissAllMenus();
					DoAssignActor(&Actor, 1, InObjectBinding);
				})
			)
		];
	MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	MenuBuilder.EndSection();
}

void FSequencer::DoAssignActor(AActor*const* InActors, int32 NumActors, FGuid InObjectBinding)
{
	if (NumActors > 0)
	{
		AActor* Actor = InActors[0];
		if (Actor != nullptr)
		{
			FScopedTransaction AssignActor( NSLOCTEXT("Sequencer", "AssignActor", "Assign Actor") );

			RootMovieSceneSequenceInstance->SaveState(*this);

			UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
			UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

			Actor->Modify();
			OwnerSequence->Modify();
			OwnerMovieScene->Modify();

			UObject* RuntimeObject = SequenceInstanceStack.Top()->FindObject(InObjectBinding, *this);

			// Replace the object itself
			FGuid ParentGuid;
			{
				// Get the object guid to assign, remove the binding if it already exists
				ParentGuid = SequenceInstanceStack.Top()->FindObjectId(*Actor);
				FString NewActorLabel = Actor->GetActorLabel();
				if (ParentGuid.IsValid())
				{
					OwnerMovieScene->RemovePossessable(ParentGuid);
					OwnerSequence->UnbindPossessableObjects(ParentGuid);
				}

				// Add this object
				FMovieScenePossessable NewPossessable( NewActorLabel, Actor->GetClass());
				ParentGuid = NewPossessable.GetGuid();
				OwnerSequence->BindPossessableObject(ParentGuid, *Actor, GetPlaybackContext());

				// Replace
				OwnerMovieScene->ReplacePossessable(InObjectBinding, ParentGuid, NewActorLabel);
			}

			// Handle components
			if (AActor* ActorToReplace = Cast<AActor>(RuntimeObject))
			{
				for (UActorComponent* ComponentToReplace : ActorToReplace->GetComponents())
				{
					if (ComponentToReplace != nullptr)
					{
						FGuid ComponentGuid = SequenceInstanceStack.Top()->FindObjectId(*ComponentToReplace);
						if (ComponentGuid.IsValid())
						{
							UActorComponent* NewComponent = Actor->GetComponentByClass(ComponentToReplace->GetClass());
							if (NewComponent)
							{
								// Get the object guid to assign, remove the binding if it already exists
								FGuid NewComponentGuid = SequenceInstanceStack.Top()->FindObjectId(*NewComponent);
								FString NewComponentLabel = NewComponent->GetName();
								if (NewComponentGuid.IsValid())
								{
									OwnerMovieScene->RemovePossessable(NewComponentGuid);
									OwnerSequence->UnbindPossessableObjects(NewComponentGuid);
								}

								// Add this object
								FMovieScenePossessable NewPossessable( NewComponentLabel, NewComponent->GetClass());
								NewComponentGuid = NewPossessable.GetGuid();
								OwnerSequence->BindPossessableObject(NewComponentGuid, *NewComponent, Actor);

								// Replace
								OwnerMovieScene->ReplacePossessable(ComponentGuid, NewComponentGuid, NewComponentLabel);

								FMovieScenePossessable* ThisPossessable = OwnerMovieScene->FindPossessable(NewComponentGuid);
								if (ensure(ThisPossessable))
								{
									ThisPossessable->SetParent(ParentGuid);
								}
							}
						}
					}
				}
			}
			
			RootMovieSceneSequenceInstance->RestoreState(*this);
			NotifyMovieSceneDataChanged();
		}
	}
}

void FSequencer::DeleteNode(TSharedRef<FSequencerDisplayNode> NodeToBeDeleted)
{
	// If this node is selected, delete all selected nodes
	if (GetSelection().IsSelected(NodeToBeDeleted))
	{
		DeleteSelectedNodes();
	}
	else
	{
		const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "UndoDeletingObject", "Delete Node") );

		OnRequestNodeDeleted(NodeToBeDeleted);
	}
}

void FSequencer::DeleteSelectedNodes()
{
	TSet< TSharedRef<FSequencerDisplayNode> > SelectedNodesCopy = GetSelection().GetSelectedOutlinerNodes();

	if( SelectedNodesCopy.Num() > 0 )
	{
		const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "UndoDeletingObject", "Delete Node") );

		for( const TSharedRef<FSequencerDisplayNode>& SelectedNode : SelectedNodesCopy )
		{
			if( !SelectedNode->IsHidden() )
			{
				// Delete everything in the entire node
				TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted = StaticCastSharedRef<const FSequencerDisplayNode>(SelectedNode);
				OnRequestNodeDeleted( NodeToBeDeleted );
			}
		}
	}
}

void FSequencer::ToggleNodeActive()
{
	bool bIsActive = !IsNodeActive();

	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "ToggleNodeActive", "Toggle Node Active") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			Section->SetIsActive(bIsActive);
		}
	}
}

bool FSequencer::IsNodeActive() const
{
	// Active only if all are active
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			if (!Section->IsActive())
			{
				return false;
			}
		}
	}
	return true;
}

void FSequencer::ToggleNodeLocked()
{
	bool bIsLocked = !IsNodeLocked();

	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "ToggleNodeLocked", "Toggle Node Locked") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			Section->SetIsLocked(bIsLocked);
		}
	}
}

bool FSequencer::IsNodeLocked() const
{
	// Locked only if all are locked
	int NumSections = 0;
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			if (!Section->IsLocked())
			{
				return false;
			}
			++NumSections;
		}
	}
	return NumSections > 0;
}

void FSequencer::TogglePlay()
{
	OnPlay();
}

void FSequencer::PlayForward()
{
	OnPlay(false);
}

void FSequencer::Rewind()
{
	OnStepToBeginning();
}

void FSequencer::StepForward()
{
	OnStepForward();
}

void FSequencer::StepBackward()
{
	OnStepBackward();
}

void FSequencer::StepToNextKey()
{
	SequencerWidget->StepToNextKey();
}

void FSequencer::StepToPreviousKey()
{
	SequencerWidget->StepToPreviousKey();
}

void FSequencer::StepToNextCameraKey()
{
	SequencerWidget->StepToNextCameraKey();
}

void FSequencer::StepToPreviousCameraKey()
{
	SequencerWidget->StepToPreviousCameraKey();
}

void FSequencer::ExpandAllNodesAndDescendants()
{
	const bool bExpandAll = true;
	SequencerWidget->GetTreeView()->ExpandNodes(ETreeRecursion::Recursive, bExpandAll);
}

void FSequencer::CollapseAllNodesAndDescendants()
{
	const bool bExpandAll = true;
	SequencerWidget->GetTreeView()->CollapseNodes(ETreeRecursion::Recursive, bExpandAll);
}

void FSequencer::ToggleExpandCollapseNodes()
{
	SequencerWidget->GetTreeView()->ToggleExpandCollapseNodes(ETreeRecursion::NonRecursive);
}

void FSequencer::ToggleExpandCollapseNodesAndDescendants()
{
	SequencerWidget->GetTreeView()->ToggleExpandCollapseNodes(ETreeRecursion::Recursive);
}

void FSequencer::SetKey()
{
	TSet<TSharedPtr<IKeyArea> > KeyAreas;
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		SequencerHelpers::GetAllKeyAreas(OutlinerNode, KeyAreas);
	}

	if (KeyAreas.Num() > 0)
	{
		FScopedTransaction SetKeyTransaction( NSLOCTEXT("Sequencer", "SetKey_Transaction", "Set Key") );
	
		for (auto KeyArea : KeyAreas)
		{
			if (KeyArea->GetOwningSection()->TryModify())
			{
				KeyArea->AddKeyUnique(GetGlobalTime(), GetKeyInterpolation());
			}
		}
	}
}

bool FSequencer::CanSetKeyTime() const
{
	return Selection.GetSelectedKeys().Num() > 0;
}

void FSequencer::SetKeyTime(const bool bUseFrames)
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	float KeyTime = 0.f;
	for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
	{
		if (Key.IsValid())
		{
			KeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());
			break;
		}
	}

	float FrameRate = 1.0f / Settings->GetTimeSnapInterval();
	
	GenericTextEntryModeless(
		bUseFrames ? NSLOCTEXT("Sequencer.Popups", "SetKeyFramePopup", "New Frame") : NSLOCTEXT("Sequencer.Popups", "SetKeyTimePopup", "New Time"),
		bUseFrames ? FText::AsNumber( SequencerHelpers::TimeToFrame( KeyTime, FrameRate )) : FText::AsNumber( KeyTime ),
		FOnTextCommitted::CreateSP(this, &FSequencer::OnSetKeyTimeTextCommitted, bUseFrames)
		);
}

void FSequencer::OnSetKeyTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, const bool bUseFrames)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		float FrameRate = 1.0f / Settings->GetTimeSnapInterval();
		double dNewTime = bUseFrames ? SequencerHelpers::FrameToTime(FCString::Atod(*InText.ToString()), FrameRate) : FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric(); 
		if(!bIsNumber)
			return;

		const float NewKeyTime = (float)dNewTime;

		FScopedTransaction SetKeyTimeTransaction(NSLOCTEXT("Sequencer", "SetKeyTime_Transaction", "Set Key Time"));

		TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	
		for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
		{
			if (Key.IsValid())
			{
				if (Key.Section->TryModify())
				{
					Key.KeyArea->SetKeyTime(Key.KeyHandle.GetValue(), NewKeyTime);
				}
			}
		}
	}
}

TArray<TSharedPtr<FMovieSceneClipboard>> GClipboardStack;

void FSequencer::CopySelectedKeys()
{
	if (Selection.GetSelectedKeys().Num() == 0)
	{
		return;
	}

	TOptional<float> CopyRelativeTo;
	
	// Copy relative to the current key hotspot, if applicable
	TSharedPtr<ISequencerHotspot> Hotspot = SequencerWidget->GetEditTool().GetHotspot();
	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		FKeyHotspot* KeyHotspot = StaticCastSharedPtr<FKeyHotspot>(Hotspot).Get();
		if (KeyHotspot->Key.KeyArea.IsValid() && KeyHotspot->Key.KeyHandle.IsSet())
		{
			CopyRelativeTo = KeyHotspot->Key.KeyArea->GetKeyTime(KeyHotspot->Key.KeyHandle.GetValue());
		}
	}

	FMovieSceneClipboardBuilder Builder;

	// Map selected keys to their key areas
	TMap<const IKeyArea*, TArray<FKeyHandle>> KeyAreaMap;
	for (const FSequencerSelectedKey& Key : Selection.GetSelectedKeys())
	{
		if (Key.KeyHandle.IsSet())
		{
			KeyAreaMap.FindOrAdd(Key.KeyArea.Get()).Add(Key.KeyHandle.GetValue());
		}
	}

	// Serialize each key area to the clipboard
	for (auto& Pair : KeyAreaMap)
	{
		Pair.Key->CopyKeys(Builder, [&](FKeyHandle Handle, const IKeyArea&){
			return Pair.Value.Contains(Handle);
		});
	}


	TSharedRef<FMovieSceneClipboard> Clipboard = MakeShareable( new FMovieSceneClipboard(Builder.Commit(CopyRelativeTo)) );
	
	if (Clipboard->GetKeyTrackGroups().Num())
	{
		GClipboardStack.Push(Clipboard);

		if (GClipboardStack.Num() > 10)
		{
			GClipboardStack.RemoveAt(0, 1);
		}
	}
}

void FSequencer::CutSelectedKeys()
{
	FScopedTransaction CutKeysTransaction( LOCTEXT("CutSelectedKeys_Transaction", "Cut Selected Keys") );
	CopySelectedKeys();
	DeleteSelectedKeys();
}

const TArray<TSharedPtr<FMovieSceneClipboard>>& FSequencer::GetClipboardStack() const
{
	return GClipboardStack;
}

void FSequencer::OnClipboardUsed(TSharedPtr<FMovieSceneClipboard> Clipboard)
{
	Clipboard->GetEnvironment().DateTime = FDateTime::UtcNow();

	// Last entry in the stack should be the most up-to-date
	GClipboardStack.Sort([](const TSharedPtr<FMovieSceneClipboard>& A, const TSharedPtr<FMovieSceneClipboard>& B){
		return A->GetEnvironment().DateTime < B->GetEnvironment().DateTime;
	});
}

void FSequencer::GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted)
{
	TSharedRef<STextEntryPopup> TextEntryPopup = 
		SNew(STextEntryPopup)
		.Label(DialogText)
		.DefaultText(DefaultText)
		.OnTextCommitted(OnTextComitted)
		.ClearKeyboardFocusOnCommit(false)
		.SelectAllTextWhenFocused(true)
		.MaxWidth(1024.0f);

	EntryPopupMenu = FSlateApplication::Get().PushMenu(
		ToolkitHost.Pin()->GetParentWidget(),
		FWidgetPath(),
		TextEntryPopup,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
		);
}

void FSequencer::CloseEntryPopupMenu()
{
	if (EntryPopupMenu.IsValid())
	{
		EntryPopupMenu.Pin()->Dismiss();
	}
}

void FSequencer::TrimSection(bool bTrimLeft)
{
	FScopedTransaction TrimSectionTransaction( NSLOCTEXT("Sequencer", "TrimSection_Transaction", "Trim Section") );

	MovieSceneToolHelpers::TrimSection(Selection.GetSelectedSections(), GetGlobalTime(), bTrimLeft);

	NotifyMovieSceneDataChanged();
}

void FSequencer::SplitSection()
{
	FScopedTransaction SplitSectionTransaction( NSLOCTEXT("Sequencer", "SplitSection_Transaction", "Split Section") );

	MovieSceneToolHelpers::SplitSection(Selection.GetSelectedSections(), GetGlobalTime());

	NotifyMovieSceneDataChanged();
}

ISequencerEditTool& FSequencer::GetEditTool()
{
	return SequencerWidget->GetEditTool();
}

void FSequencer::BindSequencerCommands()
{
	const FSequencerCommands& Commands = FSequencerCommands::Get();

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP( this, &FSequencer::DeleteSelectedItems ),
		FCanExecuteAction::CreateSP(this, &FSequencer::IsSequencerWidgetFocused ));

	SequencerCommandBindings->MapAction(
		Commands.TogglePlay,
		FExecuteAction::CreateSP( this, &FSequencer::TogglePlay ),
		FCanExecuteAction::CreateSP(this, &FSequencer::IsSequencerWidgetFocused ));

	SequencerCommandBindings->MapAction(
		Commands.PlayForward,
		FExecuteAction::CreateSP( this, &FSequencer::PlayForward ),
		FCanExecuteAction::CreateSP(this, &FSequencer::IsSequencerWidgetFocused ) );

	SequencerCommandBindings->MapAction(
		Commands.Rewind,
		FExecuteAction::CreateSP( this, &FSequencer::Rewind ),
		FCanExecuteAction::CreateSP(this, &FSequencer::IsSequencerWidgetFocused ) );

	SequencerCommandBindings->MapAction(
		Commands.StepForward,
		FExecuteAction::CreateSP( this, &FSequencer::StepForward ),
		FCanExecuteAction::CreateSP(this, &FSequencer::IsSequencerWidgetFocused ),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.StepBackward,
		FExecuteAction::CreateSP( this, &FSequencer::StepBackward ),
		FCanExecuteAction::CreateSP(this, &FSequencer::IsSequencerWidgetFocused ),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.StepToNextKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToNextCameraKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextCameraKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousCameraKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousCameraKey ) );

	SequencerCommandBindings->MapAction(
		Commands.SetStartPlaybackRange,
		FExecuteAction::CreateSP( this, &FSequencer::SetStartPlaybackRange ) );

	SequencerCommandBindings->MapAction(
		Commands.SetEndPlaybackRange,
		FExecuteAction::CreateSP( this, &FSequencer::SetEndPlaybackRange ) );

	SequencerCommandBindings->MapAction(
		Commands.ExpandAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::ExpandAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.CollapseAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::CollapseAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.ToggleExpandCollapseNodes,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleExpandCollapseNodes));

	SequencerCommandBindings->MapAction(
		Commands.ToggleExpandCollapseNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleExpandCollapseNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.SetKey,
		FExecuteAction::CreateSP( this, &FSequencer::SetKey ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicAuto,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_Auto ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicUser,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_User ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicBreak,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_Break ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationLinear,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Linear, ERichCurveTangentMode::RCTM_Auto ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationConstant,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Constant, ERichCurveTangentMode::RCTM_Auto ) );

	SequencerCommandBindings->MapAction(
		Commands.TrimSectionLeft,
		FExecuteAction::CreateSP( this, &FSequencer::TrimSection, true ) );

	SequencerCommandBindings->MapAction(
		Commands.TrimSectionRight,
		FExecuteAction::CreateSP( this, &FSequencer::TrimSection, false ) );

	SequencerCommandBindings->MapAction(
		Commands.SplitSection,
		FExecuteAction::CreateSP( this, &FSequencer::SplitSection ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleAutoKeyEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoKeyEnabled( !Settings->GetAutoKeyEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoKeyEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeyAllEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeyAllEnabled( !Settings->GetKeyAllEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetKeyAllEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleAutoScroll,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoScrollEnabled( !Settings->GetAutoScrollEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoScrollEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.FindInContentBrowser,
		FExecuteAction::CreateSP( this, &FSequencer::FindInContentBrowser ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleDetailsView,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetDetailsViewVisible( !Settings->GetDetailsViewVisible() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetDetailsViewVisible(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleLabelBrowser,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetLabelBrowserVisible( !Settings->GetLabelBrowserVisible() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetLabelBrowserVisible(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowFrameNumbers,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowFrameNumbers( !Settings->GetShowFrameNumbers() ); } ),
		FCanExecuteAction::CreateSP(this, &FSequencer::CanShowFrameNumbers ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowFrameNumbers(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowRangeSlider,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowRangeSlider( !Settings->GetShowRangeSlider() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowRangeSlider(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleIsSnapEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetIsSnapEnabled( !Settings->GetIsSnapEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetIsSnapEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapKeyTimesToInterval( !Settings->GetSnapKeyTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapKeyTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToKeys,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapKeyTimesToKeys( !Settings->GetSnapKeyTimesToKeys() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapKeyTimesToKeys(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapSectionTimesToInterval( !Settings->GetSnapSectionTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapSectionTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToSections,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapSectionTimesToSections( !Settings->GetSnapSectionTimesToSections() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapSectionTimesToSections(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToInterval( !Settings->GetSnapPlayTimeToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToDraggedKey,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToDraggedKey( !Settings->GetSnapPlayTimeToDraggedKey() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToDraggedKey(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapCurveValueToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapCurveValueToInterval( !Settings->GetSnapCurveValueToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapCurveValueToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowCurveEditor,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowCurveEditor(!Settings->GetShowCurveEditor()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowCurveEditor(); } ) );

	auto CanCutOrCopy = [this]{
		if (!IsSequencerWidgetFocused())
		{
			return false;
		}
		
		UMovieSceneTrack* Track = nullptr;
		for (FSequencerSelectedKey Key : Selection.GetSelectedKeys())
		{
			if (!Track)
			{
				Track = Key.Section->GetTypedOuter<UMovieSceneTrack>();
			}
			if (!Track || Track != Key.Section->GetTypedOuter<UMovieSceneTrack>())
			{
				return false;
			}
		}
		return true;
	};

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &FSequencer::CutSelectedKeys),
		FCanExecuteAction::CreateLambda(CanCutOrCopy)
	);

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FSequencer::CopySelectedKeys),
		FCanExecuteAction::CreateLambda(CanCutOrCopy)
	);

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepCursorInPlaybackRange,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepCursorInPlayRange( !Settings->ShouldKeepCursorInPlayRange() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepCursorInPlayRange(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.RenderMovie,
		FExecuteAction::CreateLambda([this]{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

			// Create a new movie scene capture object for an automated level sequence, and open the tab
			UAutomatedLevelSequenceCapture* MovieSceneCapture = NewObject<UAutomatedLevelSequenceCapture>(GetTransientPackage(), UAutomatedLevelSequenceCapture::StaticClass(), NAME_None, RF_Transient);
			MovieSceneCapture->LoadConfig();

			// Attempt to find a level sequence actor in the world that references this asset
			for (auto It = TActorIterator<ALevelSequenceActor>(GWorld); It; ++It)
			{
				if (It->LevelSequence == GetCurrentAsset())
				{
					MovieSceneCapture->SetLevelSequenceActor(*It);
					break;
				}
			}

			MovieSceneCapture->SetLevelSequenceAsset(GetCurrentAsset()->GetPathName());

			if (CanShowFrameNumbers())
			{
				MovieSceneCapture->Settings.FrameRate = FMath::RoundToInt(1.f / Settings->GetTimeSnapInterval());
				MovieSceneCapture->Settings.bUseRelativeFrameNumbers = false;

				const int32 SequenceStartFrame = FMath::RoundToInt( GetPlaybackRange().GetLowerBoundValue() * MovieSceneCapture->Settings.FrameRate );
				const int32 SequenceEndFrame = FMath::Max( SequenceStartFrame, FMath::RoundToInt( GetPlaybackRange().GetUpperBoundValue() * MovieSceneCapture->Settings.FrameRate ) );

				MovieSceneCapture->StartFrame = SequenceStartFrame;
				MovieSceneCapture->EndFrame = SequenceEndFrame;
			}

			IMovieSceneCaptureDialogModule::Get().OpenDialog(LevelEditorModule.GetLevelEditorTabManager().ToSharedRef(), MovieSceneCapture);
		})
	);

	SequencerWidget->BindCommands(SequencerCommandBindings);

	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BindCommands(SequencerCommandBindings);
	}
}

void FSequencer::BuildAddTrackMenu(class FMenuBuilder& MenuBuilder)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildAddTrackMenu(MenuBuilder);
	}
}

void FSequencer::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingTrackMenu(MenuBuilder, ObjectBinding, ObjectClass);
	}
}

void FSequencer::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingEditButtons(EditBox, ObjectBinding, ObjectClass);
	}
}

bool FSequencer::IsSequencerWidgetFocused() const
{
	// Only perform certain operations if the key was pressed in the sequencer widget and not in, for example, the viewport.
	return SequencerWidget->HasFocusedDescendants() || SequencerWidget->HasKeyboardFocus();
}

#undef LOCTEXT_NAMESPACE
